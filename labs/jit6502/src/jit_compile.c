/*
 * 6502 -> AArch64 basic-block translator.
 *
 * Contract with the runtime (jit_runtime.c):
 *   - jit_compile() appends one block to an Emitter whose buffer lives inside
 *     the executable arena, and returns how many guest instructions it covers.
 *   - Returning 0 means "the first instruction is not translatable"; the
 *     dispatcher then runs the interpreter for one instruction.
 *
 * Register plan inside a block (all callee-saved, saved by the prologue):
 *   x19 = Cpu*            x20 = mem              w21 = A
 *   w22 = X               w23 = Y                w24 = SP
 *   w25 = P               x26 = mem + pc (host pointer)
 *   x27 = cycles          w28 = 0x7D (clears N|Z)
 *
 * Keeping the PC as a host pointer makes every operand fetch a single
 * `ldrb wN, [x26, #disp]`, which is where most of the win over the
 * interpreter's decode path comes from.
 */
#include "jit.h"

#include "arm64.h"
#include "opcodes_gen.h"

#include <stddef.h>
#include <stdint.h>

/* Stores inside [code_lo, code_hi) may have rewritten translated code. */
#ifndef CODE_LO_DEFAULT
#define CODE_LO_DEFAULT 0x0400u
#endif
#ifndef CODE_HI_DEFAULT
#define CODE_HI_DEFAULT 0x8000u
#endif
#define MAX_BLOCK_INST 48

enum {
  X_CTX = 19,
  X_MEM = 20,
  W_A = 21,
  W_X = 22,
  W_Y = 23,
  W_SP = 24,
  W_P = 25,
  X_PC = 26,
  X_CYC = 27,
  W_NZMASK = 28
};

typedef struct Compiler {
  Emitter *e;
  const uint8_t *mem;
  uint8_t (*code_read)(uint16_t); /* non-NULL: fetch instruction bytes via bus */
  uint16_t start_pc;
  uint16_t stop_pc;
  uint16_t code_lo;
  uint16_t code_hi;
  int chaining;
  int bus;
  int cur_cycles; /* base cycle count of the instruction being translated */
  int count;
  int next_label;
} Compiler;

/* Byte of guest code at `addr`: cartridge ROM in NES mode, flat image else. */
static inline uint8_t cbyte(const Compiler *c, uint16_t addr) {
  return c->code_read != NULL ? c->code_read(addr) : c->mem[addr];
}

/* ---- small emission helpers -------------------------------------------- */

static void emit_add_cycles(Compiler *c, int n) {
  if (n > 0) emit_u32(c->e, a64_addsub_imm(true, X_CYC, X_CYC, (uint32_t)n, false));
}

static void emit_set_nz(Compiler *c, int wreg) {
  Emitter *e = c->e;
  /* Scratch: w12/w13.  Callers may hold the result in w10 or w11, so those
   * two must stay untouched here. */
  emit_u32(e, a64_logic_imm(0, 12, (uint32_t)wreg, 0x80)); /* and w12, Rw, #0x80 */
  emit_u32(e, a64_cmp_imm(false, (uint32_t)wreg, 0));      /* cmp Rw, #0 */
  emit_u32(e, a64_cset(13, A64_EQ));                       /* cset w13, eq */
  emit_u32(e, a64_logic_reg(false, 1, 12, 12, 13, A64_LSL, 1)); /* orr w12,w12,w13,lsl#1 */
  emit_u32(e, a64_logic_reg(false, 0, W_P, W_P, W_NZMASK, A64_LSL, 0));
  emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 12, A64_LSL, 0));
}

/* ---- memory access (flat image or NES bus) ------------------------------ */

static void emit_bus_read(Compiler *c, int wdst) {
  Emitter *e = c->e;
  /* w1 = address.  w0 carries the running instruction count, which the C
   * helper clobbers, so stash it in w14. */
  emit_u32(e, a64_str_off(0, X_CTX, (uint32_t)offsetof(Cpu, jit_count)));
  /* Publish the cycle counter as of the *start* of this instruction (cycles
   * are added up front in the block), so PPU state matches the interpreter. */
  emit_u32(e, a64_addsub_imm(true, 2, X_CYC, (uint32_t)c->cur_cycles, true));
  emit_u32(e, a64_str_off(2, X_CTX, (uint32_t)offsetof(Cpu, cycles)));
  emit_u32(e, a64_mov_reg(false, 0, 1));
  emit_u32(e, a64_ldr_off(16, 29, (uint32_t)offsetof(JitRuntime, bus_read)));
  emit_u32(e, a64_blr(16));
  emit_u32(e, a64_mov_reg(false, (uint32_t)wdst, 0));
  emit_u32(e, a64_ldr_off(0, X_CTX, (uint32_t)offsetof(Cpu, jit_count)));
}

static void emit_bus_write(Compiler *c, int wsrc) {
  Emitter *e = c->e;
  emit_u32(e, a64_str_off(0, X_CTX, (uint32_t)offsetof(Cpu, jit_count)));
  emit_u32(e, a64_addsub_imm(true, 2, X_CYC, (uint32_t)c->cur_cycles, true));
  emit_u32(e, a64_str_off(2, X_CTX, (uint32_t)offsetof(Cpu, cycles)));
  emit_u32(e, a64_mov_reg(false, 0, 1)); /* address */
  emit_u32(e, a64_mov_reg(false, 1, (uint32_t)wsrc));
  emit_u32(e, a64_ldr_off(16, 29, (uint32_t)offsetof(JitRuntime, bus_write)));
  emit_u32(e, a64_blr(16));
  emit_u32(e, a64_ldr_off(0, X_CTX, (uint32_t)offsetof(Cpu, jit_count)));
}

/* Load one byte into wdst from a constant address or from w1. */
static void emit_load_addr(Compiler *c, bool is_const, uint16_t addr, int wdst) {
  Emitter *e = c->e;
  if (!c->bus) {
    if (is_const) {
      emit_u32(e, a64_movz(false, 16, addr, 0));
      emit_u32(e, a64_ldrb_reg((uint32_t)wdst, X_MEM, 16));
    } else {
      emit_u32(e, a64_ldrb_reg((uint32_t)wdst, X_MEM, 1));
    }
    return;
  }
  if (is_const) {
    emit_u32(e, a64_movz(false, 1, addr, 0));
    if (addr < 0x2000) {
      emit_u32(e, a64_logic_imm(0, 2, 1, 0x7FF));
      emit_u32(e, a64_ldrb_reg((uint32_t)wdst, X_MEM, 2));
    } else {
      emit_bus_read(c, wdst);
    }
    return;
  }
  int slow = c->next_label++, done = c->next_label++;
  emit_u32(e, a64_lsr_imm(false, 2, 1, 13)); /* w2 = addr >> 13 */
  emit_cbz(e, 2, slow, true);
  emit_u32(e, a64_logic_imm(0, 2, 1, 0x7FF));
  emit_u32(e, a64_ldrb_reg((uint32_t)wdst, X_MEM, 2));
  emit_b(e, done);
  emit_label(e, slow);
  emit_bus_read(c, wdst);
  emit_label(e, done);
}

/* Store one byte from wsrc to a constant address or to w1. */
static void emit_store_addr(Compiler *c, bool is_const, uint16_t addr, int wsrc) {
  Emitter *e = c->e;
  if (!c->bus) {
    if (is_const) {
      emit_u32(e, a64_movz(false, 16, addr, 0));
      emit_u32(e, a64_strb_reg((uint32_t)wsrc, X_MEM, 16));
    } else {
      emit_u32(e, a64_strb_reg((uint32_t)wsrc, X_MEM, 1));
    }
    return;
  }
  if (is_const) {
    emit_u32(e, a64_movz(false, 1, addr, 0));
    if (addr < 0x2000) {
      emit_u32(e, a64_logic_imm(0, 2, 1, 0x7FF));
      emit_u32(e, a64_strb_reg((uint32_t)wsrc, X_MEM, 2));
    } else {
      emit_bus_write(c, wsrc);
    }
    return;
  }
  int slow = c->next_label++, done = c->next_label++;
  emit_u32(e, a64_lsr_imm(false, 2, 1, 13));
  emit_cbz(e, 2, slow, true);
  emit_u32(e, a64_logic_imm(0, 2, 1, 0x7FF));
  emit_u32(e, a64_strb_reg((uint32_t)wsrc, X_MEM, 2));
  emit_b(e, done);
  emit_label(e, slow);
  emit_bus_write(c, wsrc);
  emit_label(e, done);
}

static void emit_mov_imm64(Compiler *c, int rd, uint64_t v) {
  Emitter *e = c->e;
  emit_u32(e, a64_movz(true, (uint32_t)rd, (uint16_t)(v & 0xFFFF), 0));
  emit_u32(e, a64_movk(true, (uint32_t)rd, (uint16_t)((v >> 16) & 0xFFFF), 1));
  emit_u32(e, a64_movk(true, (uint32_t)rd, (uint16_t)((v >> 32) & 0xFFFF), 2));
  emit_u32(e, a64_movk(true, (uint32_t)rd, (uint16_t)((v >> 48) & 0xFFFF), 3));
}

/* After a store that might have hit translated code: mark the cache dirty and
 * leave the block so the dispatcher recompiles. */
/* Adds the instructions this block executed to the running count in w0. */
static void emit_count(Compiler *c, int executed) {
  if (executed > 0) {
    emit_u32(c->e, a64_addsub_imm(false, 0, 0, (uint32_t)executed, false));
  }
}

/* Leave the block.  With chaining enabled, try to jump straight into the
 * target block's resume point; otherwise fall back to the dispatcher. */
static void emit_exit(Compiler *c, int epilogue, int executed, int target_reg,
                      uint16_t target, bool set_pc) {
  Emitter *e = c->e;
  emit_count(c, executed);
  if (target_reg < 0) {
    /* x16 always ends up as the target address (chain index below). */
    emit_u32(e, a64_movz(false, 16, target, 0));
    if (set_pc) {
      emit_u32(e, a64_addsub_reg(true, X_PC, X_MEM, 16, A64_LSL, 0, false));
    }
  }
  if (!c->chaining) {
    emit_b(e, epilogue);
    return;
  }
  emit_u32(e, a64_addsub_imm(true, 17, 29,
                             (uint32_t)offsetof(JitRuntime, resume), false));
  if (target_reg < 0) {
    emit_u32(e, a64_addsub_reg(true, 17, 17, 16, A64_LSL, 3, false));
  } else {
    emit_u32(e, a64_addsub_reg(true, 17, 17, (uint32_t)target_reg, A64_LSL, 3, false));
  }
  int skip = c->next_label++;
  emit_u32(e, a64_ldrb_off(3, 29, (uint32_t)offsetof(JitRuntime, pending)));
  emit_cbz(e, 3, skip, true); /* pending != 0 -> return to dispatcher */
  /* Bound the chain so the dispatcher keeps control (budgets, stop points). */
  emit_u32(e, a64_ldrh_off(2, X_CTX, (uint32_t)offsetof(Cpu, chain_budget)));
  emit_u32(e, a64_addsub_imm(false, 2, 2, 1, true));
  emit_u32(e, a64_strh_off(2, X_CTX, (uint32_t)offsetof(Cpu, chain_budget)));
  emit_cbz(e, 2, skip, false);
  emit_u32(e, a64_ldr_off(17, 17, 0));
  emit_u32(e, a64_cmp_imm(true, 17, 0)); /* cmp x17, #0 */
  emit_bcond(e, A64_EQ, skip);
  emit_u32(e, 0xD61F0000u | (17u << 5)); /* br x17 */
  emit_label(e, skip);
  emit_b(e, epilogue);
}

/* Self-modifying store: publish the written page, set the dirty flag and let
 * the dispatcher invalidate only that page's blocks.  Never chains. */
static void emit_mark_dirty_and_exit(Compiler *c, int epilogue, int executed,
                                     int page_reg, uint8_t page) {
  Emitter *e = c->e;
  if (page_reg < 0) {
    emit_u32(e, a64_movz(false, 2, page, 0));
  } else {
    emit_u32(e, a64_lsr_imm(false, 2, (uint32_t)page_reg, 8));
    emit_u32(e, a64_logic_imm(0, 2, 2, 0xFF));
  }
  emit_u32(e, a64_strb_off(2, X_CTX, (uint32_t)offsetof(Cpu, dirty_page)));
  emit_u32(e, a64_movz(false, 2, 1, 0));
  emit_u32(e, a64_strb_off(2, X_CTX, (uint32_t)offsetof(Cpu, code_dirty)));
  emit_count(c, executed);
  emit_b(e, epilogue);
}

/* Store through an effective address that is either a translation-time
 * constant (w1 unused) or already computed into w1. */
static void emit_store_checked(Compiler *c, int wsrc, bool is_const,
                               uint16_t addr, int epilogue, int executed) {
  if (c->bus) {
    /* NES code lives in cartridge ROM; translated blocks never go stale from
     * a store, so no dirty-page bookkeeping is needed. */
    emit_store_addr(c, is_const, addr, wsrc);
    return;
  }
  if (is_const) {
    emit_store_addr(c, true, addr, wsrc);
    if (addr >= c->code_lo && addr < c->code_hi) {
      emit_mark_dirty_and_exit(c, epilogue, executed, -1, (uint8_t)(addr >> 8));
    }
    return;
  }
  emit_store_addr(c, false, 0, wsrc);
  int skip = c->next_label++;
  emit_u32(c->e, a64_cmp_imm(false, 1, c->code_lo));
  emit_bcond(c->e, A64_CC, skip); /* addr < lo: plain data */
  emit_u32(c->e, a64_cmp_imm(false, 1, c->code_hi));
  emit_bcond(c->e, A64_CS, skip); /* addr >= hi: plain data */
  emit_mark_dirty_and_exit(c, epilogue, executed, 1, 0);
  emit_label(c->e, skip);
}

/* ---- effective addresses ----------------------------------------------- */

typedef struct Ea {
  bool is_const;
  uint16_t addr;
} Ea;

static int inst_len(uint8_t mode) {
  switch (mode) {
    case AM_IMP:
    case AM_ACC:
      return 1;
    case AM_IMM:
    case AM_ZP:
    case AM_ZPX:
    case AM_ZPY:
    case AM_INDX:
    case AM_INDY:
    case AM_REL:
      return 2;
    default:
      return 3;
  }
}

/* Leaves the effective address in w1 when it depends on a register; returns
 * the constant address otherwise.  `charge_cross` emits the page-cross cycle
 * accounting for read instructions. */
static Ea emit_ea(Compiler *c, uint16_t pc, uint8_t mode, bool charge_cross) {
  Emitter *e = c->e;
  Ea r = {false, 0};
  switch (mode) {
    case AM_IMM:
      r.is_const = true;
      r.addr = (uint16_t)(pc + 1);
      break;
    case AM_ZP:
      r.is_const = true;
      r.addr = cbyte(c, (uint16_t)(pc + 1));
      break;
    case AM_ZPX:
      emit_u32(e, a64_addsub_imm(false, 1, W_X, cbyte(c, (uint16_t)(pc + 1)), false));
      emit_u32(e, a64_logic_imm(0, 1, 1, 0xFF));
      break;
    case AM_ZPY:
      emit_u32(e, a64_addsub_imm(false, 1, W_Y, cbyte(c, (uint16_t)(pc + 1)), false));
      emit_u32(e, a64_logic_imm(0, 1, 1, 0xFF));
      break;
    case AM_ABS:
      r.is_const = true;
      r.addr = (uint16_t)(cbyte(c, (uint16_t)(pc + 1)) |
                          (cbyte(c, (uint16_t)(pc + 2)) << 8));
      break;
    case AM_ABSX:
    case AM_ABSY: {
      int idx = mode == AM_ABSX ? W_X : W_Y;
      uint16_t base = (uint16_t)(cbyte(c, (uint16_t)(pc + 1)) |
                                    (cbyte(c, (uint16_t)(pc + 2)) << 8));
      if (charge_cross) {
        emit_u32(e, a64_addsub_imm(false, 2, (uint32_t)idx, base & 0xFF, false));
        emit_u32(e, a64_lsr_imm(false, 2, 2, 8));
        emit_u32(e, a64_addsub_reg(true, X_CYC, X_CYC, 2, A64_LSL, 0, false));
      }
      emit_u32(e, a64_movz(false, 1, base, 0));
      emit_u32(e, a64_addsub_reg(false, 1, 1, (uint32_t)idx, A64_LSL, 0, false));
      emit_u32(e, a64_ubfm(false, 1, 1, 0, 15)); /* uxth w1, w1 */
      break;
    }
    case AM_INDX: {
      uint8_t zp = cbyte(c, (uint16_t)(pc + 1));
      emit_u32(e, a64_addsub_imm(false, 1, W_X, zp, false));
      emit_u32(e, a64_logic_imm(0, 1, 1, 0xFF));
      emit_u32(e, a64_ldrb_reg(2, X_MEM, 1));
      emit_u32(e, a64_addsub_imm(false, 3, 1, 1, false));
      emit_u32(e, a64_logic_imm(0, 3, 3, 0xFF));
      emit_u32(e, a64_ldrb_reg(3, X_MEM, 3));
      /* low byte | high byte << 8 */
      emit_u32(e, a64_logic_reg(false, 1, 1, 2, 3, A64_LSL, 8));
      break;
    }
    case AM_INDY: {
      uint8_t zp = cbyte(c, (uint16_t)(pc + 1));
      emit_u32(e, a64_movz(false, 1, zp, 0));
      emit_u32(e, a64_ldrb_reg(2, X_MEM, 1));
      emit_u32(e, a64_addsub_imm(false, 3, 1, 1, false));
      emit_u32(e, a64_logic_imm(0, 3, 3, 0xFF));
      emit_u32(e, a64_ldrb_reg(3, X_MEM, 3));
      emit_u32(e, a64_logic_reg(false, 1, 2, 2, 3, A64_LSL, 8)); /* base16 */
      if (charge_cross) {
        emit_u32(e, a64_logic_imm(0, 3, 2, 0xFF));
        emit_u32(e, a64_addsub_reg(false, 3, 3, W_Y, A64_LSL, 0, false));
        emit_u32(e, a64_lsr_imm(false, 3, 3, 8));
        emit_u32(e, a64_addsub_reg(true, X_CYC, X_CYC, 3, A64_LSL, 0, false));
      }
      emit_u32(e, a64_addsub_reg(false, 1, 2, W_Y, A64_LSL, 0, false));
      emit_u32(e, a64_ubfm(false, 1, 1, 0, 15));
      break;
    }
    default:
      break;
  }
  return r;
}

static bool op_reads_memory(uint8_t op) {
  switch (op) {
    case OP_LDA: case OP_LDX: case OP_LDY:
    case OP_AND: case OP_ORA: case OP_EOR:
    case OP_ADC: case OP_SBC: case OP_CMP: case OP_CPX: case OP_CPY:
    case OP_BIT:
      return true;
    default:
      return false;
  }
}

/* ---- stack helpers ------------------------------------------------------ */

static void emit_push(Compiler *c, int wsrc) {
  Emitter *e = c->e;
  emit_u32(e, a64_addsub_imm(false, 3, W_SP, 0x100, false));
  emit_u32(e, a64_strb_reg((uint32_t)wsrc, X_MEM, 3));
  emit_u32(e, a64_addsub_imm(false, W_SP, W_SP, 1, true));
  emit_u32(e, a64_uxtb(W_SP, W_SP));
}

static void emit_pull(Compiler *c, int wdst) {
  Emitter *e = c->e;
  emit_u32(e, a64_addsub_imm(false, W_SP, W_SP, 1, false));
  emit_u32(e, a64_uxtb(W_SP, W_SP));
  emit_u32(e, a64_addsub_imm(false, 2, W_SP, 0x100, false));
  emit_u32(e, a64_ldrb_reg((uint32_t)wdst, X_MEM, 2));
}

/* ---- ADC / SBC ---------------------------------------------------------- */

static void emit_adc_binary(Compiler *c) {
  Emitter *e = c->e;
  emit_u32(e, a64_logic_imm(0, 2, W_P, 1));            /* w2 = carry */
  emit_u32(e, a64_mov_reg(false, 10, 9));              /* w10 = m */
  emit_u32(e, a64_addsub_reg(false, 3, W_A, 9, A64_LSL, 0, false));
  emit_u32(e, a64_addsub_reg(false, 3, 3, 2, A64_LSL, 0, false)); /* w3 = sum */
  emit_u32(e, a64_logic_reg(false, 2, 4, W_A, 10, A64_LSL, 0));   /* eor w4 */
  emit_u32(e, a64_mvn(4, 4));
  emit_u32(e, a64_logic_reg(false, 2, 5, W_A, 3, A64_LSL, 0));    /* eor w5 */
  emit_u32(e, a64_logic_reg(false, 0, 4, 4, 5, A64_LSL, 0));
  emit_u32(e, a64_logic_imm(0, 4, 4, 0x80));                      /* V candidate */
  emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFFEu));           /* clear C */
  emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFBFu));           /* clear V */
  emit_u32(e, a64_lsr_imm(false, 6, 3, 8));
  emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 6, A64_LSL, 0));  /* set C */
  emit_u32(e, a64_lsr_imm(false, 4, 4, 1));
  emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 4, A64_LSL, 0));  /* set V */
  emit_u32(e, a64_uxtb(W_A, 3));
  emit_set_nz(c, W_A);
}

static void emit_sbc_binary(Compiler *c) {
  Emitter *e = c->e;
  emit_u32(e, a64_logic_imm(0, 2, W_P, 1));            /* carry */
  emit_u32(e, a64_mov_reg(false, 10, 9));              /* w10 = m */
  emit_u32(e, a64_mvn(9, 9));
  emit_u32(e, a64_uxtb(9, 9));
  emit_u32(e, a64_addsub_reg(false, 3, W_A, 9, A64_LSL, 0, false));
  emit_u32(e, a64_addsub_reg(false, 3, 3, 2, A64_LSL, 0, false)); /* sum */
  emit_u32(e, a64_logic_reg(false, 2, 4, W_A, 10, A64_LSL, 0));
  emit_u32(e, a64_logic_reg(false, 2, 5, W_A, 3, A64_LSL, 0));
  emit_u32(e, a64_logic_reg(false, 0, 4, 4, 5, A64_LSL, 0));
  emit_u32(e, a64_logic_imm(0, 4, 4, 0x80));
  emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFFEu));
  emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFBFu));
  emit_u32(e, a64_lsr_imm(false, 6, 3, 8));
  emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 6, A64_LSL, 0));
  emit_u32(e, a64_lsr_imm(false, 4, 4, 1));
  emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 4, A64_LSL, 0));
  emit_u32(e, a64_uxtb(W_A, 3));
  emit_set_nz(c, W_A);
}

/* w9 holds the operand.  Decimal mode goes through a C helper because it is
 * rare and its flag rules are fiddly; the branch keeps the fast path clean. */
static void emit_adc(Compiler *c, void (*binary)(Compiler *),
                     uint16_t (*helper)(uint8_t, uint8_t, uint8_t)) {
  Emitter *e = c->e;
  int dec = c->next_label++, done = c->next_label++;
  emit_tbz(e, W_P, 3, dec, true); /* if D set -> decimal */
  binary(c);
  emit_b(e, done);
  emit_label(e, dec);
  /* w0 holds the running instruction count; the C helper takes arguments in
   * w0-w2 and returns in w0, so stash the counter in w14 around the call. */
  emit_u32(e, a64_str_off(0, X_CTX, (uint32_t)offsetof(Cpu, jit_count)));
  emit_u32(e, a64_mov_reg(false, 0, W_A));
  emit_u32(e, a64_mov_reg(false, 1, 9));
  emit_u32(e, a64_mov_reg(false, 2, W_P));
  emit_mov_imm64(c, 16, (uint64_t)(uintptr_t)helper);
  emit_u32(e, a64_blr(16));
  emit_u32(e, a64_uxtb(W_A, 0));
  emit_u32(e, a64_ubfm(false, W_P, 0, 8, 15)); /* ubfx w25,w0,#8,#8 */
  emit_u32(e, a64_ldr_off(0, X_CTX, (uint32_t)offsetof(Cpu, jit_count)));
  emit_label(e, done);
}

/* ---- one instruction ---------------------------------------------------- */

static bool emit_instruction(Compiler *c, uint16_t pc, const OpcodeInfo *info,
                             int epilogue, int index);

static void emit_branch(Compiler *c, uint16_t pc, uint8_t opcode,
                        const OpcodeInfo *info, int epilogue, int executed) {
  Emitter *e = c->e;
  int bit = 0;
  bool taken_if_set = true;
  switch (opcode) {
    case 0x10: bit = 7; taken_if_set = false; break; /* BPL */
    case 0x30: bit = 7; taken_if_set = true; break;  /* BMI */
    case 0x50: bit = 6; taken_if_set = false; break; /* BVC */
    case 0x70: bit = 6; taken_if_set = true; break;  /* BVS */
    case 0x90: bit = 0; taken_if_set = false; break; /* BCC */
    case 0xB0: bit = 0; taken_if_set = true; break;  /* BCS */
    case 0xD0: bit = 1; taken_if_set = false; break; /* BNE */
    case 0xF0: bit = 1; taken_if_set = true; break;  /* BEQ */
    default: return;
  }
  int8_t rel = (int8_t)cbyte(c, (uint16_t)(pc + 1));
  uint16_t target = (uint16_t)(pc + 2 + rel);
  int skip = c->next_label++;
  /* fall through when the flag is in the "not taken" state */
  emit_tbz(e, W_P, bit, skip, !taken_if_set);
  /* taken: PC = target and leave the block (chain if possible) */
  emit_add_cycles(c, 1 + (((target ^ (pc + 2)) & 0xFF00u) ? 1 : 0));
  emit_exit(c, epilogue, executed, -1, target, true);
  emit_label(e, skip);
  (void)info;
}

static bool emit_instruction(Compiler *c, uint16_t pc, const OpcodeInfo *info,
                             int epilogue, int index) {
  Emitter *e = c->e;
  bool read = op_reads_memory(info->op);
  bool ends = false;

  emit_add_cycles(c, info->cycles);
  c->cur_cycles = info->cycles;
  /* Advance the host PC pointer up front: operand addressing uses absolute
   * constants, and every exit path (fallthrough or a dirty-store exit) then
   * already points at the next instruction.  Control-flow instructions
   * overwrite x26 with their target before leaving the block. */
  emit_u32(e, a64_addsub_imm(true, X_PC, X_PC,
                             (uint32_t)inst_len(info->mode), false));

  switch (info->op) {
    case OP_LDA: case OP_LDX: case OP_LDY: {
      int dst = info->op == OP_LDA ? W_A : (info->op == OP_LDX ? W_X : W_Y);
      Ea ea = emit_ea(c, pc, info->mode, true);
      emit_load_addr(c, ea.is_const, ea.addr, 9);
      emit_u32(e, a64_mov_reg(false, (uint32_t)dst, 9));
      emit_set_nz(c, dst);
      (void)read;
      break;
    }
    case OP_STA: case OP_STX: case OP_STY: {
      int src = info->op == OP_STA ? W_A : (info->op == OP_STX ? W_X : W_Y);
      Ea ea = emit_ea(c, pc, info->mode, false);
      emit_store_checked(c, src, ea.is_const, ea.addr, epilogue, index + 1);
      break;
    }
    case OP_TAX: emit_u32(e, a64_mov_reg(false, W_X, W_A)); emit_set_nz(c, W_X); break;
    case OP_TAY: emit_u32(e, a64_mov_reg(false, W_Y, W_A)); emit_set_nz(c, W_Y); break;
    case OP_TXA: emit_u32(e, a64_mov_reg(false, W_A, W_X)); emit_set_nz(c, W_A); break;
    case OP_TYA: emit_u32(e, a64_mov_reg(false, W_A, W_Y)); emit_set_nz(c, W_A); break;
    case OP_TSX: emit_u32(e, a64_mov_reg(false, W_X, W_SP)); emit_set_nz(c, W_X); break;
    case OP_TXS: emit_u32(e, a64_mov_reg(false, W_SP, W_X)); break;

    case OP_ADC: case OP_SBC: {
      Ea ea = emit_ea(c, pc, info->mode, true);
      emit_load_addr(c, ea.is_const, ea.addr, 9);
      if (info->op == OP_ADC) emit_adc(c, emit_adc_binary, jit_adc_decimal);
      else emit_adc(c, emit_sbc_binary, jit_sbc_decimal);
      break;
    }
    case OP_AND: case OP_ORA: case OP_EOR: {
      Ea ea = emit_ea(c, pc, info->mode, true);
      emit_load_addr(c, ea.is_const, ea.addr, 9);
      uint32_t op = info->op == OP_AND ? 0 : (info->op == OP_ORA ? 1 : 2);
      emit_u32(e, a64_logic_reg(false, op, W_A, W_A, 9, A64_LSL, 0));
      emit_set_nz(c, W_A);
      break;
    }
    case OP_BIT: {
      Ea ea = emit_ea(c, pc, info->mode, false);
      emit_load_addr(c, ea.is_const, ea.addr, 9);
      emit_u32(e, a64_logic_reg(false, 0, 2, W_A, 9, A64_LSL, 0)); /* A & M */
      emit_u32(e, a64_cmp_imm(false, 2, 0));
      emit_u32(e, a64_cset(3, A64_EQ));
      emit_u32(e, a64_logic_imm(0, 12, 9, 0xC0));               /* N|V from M */
      emit_u32(e, a64_logic_reg(false, 0, W_P, W_P, W_NZMASK, A64_LSL, 0));
      emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFBFu));      /* clear V */
      emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 12, A64_LSL, 0));
      emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 3, A64_LSL, 1));
      break;
    }
    case OP_CMP: case OP_CPX: case OP_CPY: {
      int reg = info->op == OP_CMP ? W_A : (info->op == OP_CPX ? W_X : W_Y);
      Ea ea = emit_ea(c, pc, info->mode, true);
      emit_load_addr(c, ea.is_const, ea.addr, 9);
      /* subs wzr, R, w9 */
      emit_u32(e, a64_subs_reg(false, 31, (uint32_t)reg, 9, A64_LSL, 0));
      emit_u32(e, a64_cset(11, A64_CS));
      emit_u32(e, a64_addsub_reg(false, 10, (uint32_t)reg, 9, A64_LSL, 0, true));
      emit_u32(e, a64_uxtb(10, 10));
      emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFFEu)); /* clear C */
      emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 11, A64_LSL, 0));
      emit_set_nz(c, 10);
      break;
    }
    case OP_INC: case OP_DEC: {
      Ea ea = emit_ea(c, pc, info->mode, false);
      emit_load_addr(c, ea.is_const, ea.addr, 9);
      if (info->op == OP_INC)
        emit_u32(e, a64_addsub_imm(false, 9, 9, 1, false));
      else
        emit_u32(e, a64_addsub_imm(false, 9, 9, 1, true));
      emit_u32(e, a64_uxtb(9, 9));
      /* Flags first: the store may end the block on a self-modifying write. */
      emit_set_nz(c, 9);
      emit_store_checked(c, 9, ea.is_const, ea.addr, epilogue, index + 1);
      break;
    }
    case OP_INX: case OP_DEX: case OP_INY: case OP_DEY: {
      int reg = (info->op == OP_INX || info->op == OP_DEX) ? W_X : W_Y;
      bool dec = (info->op == OP_DEX || info->op == OP_DEY);
      emit_u32(e, a64_addsub_imm(false, (uint32_t)reg, (uint32_t)reg, 1, dec));
      emit_u32(e, a64_uxtb((uint32_t)reg, (uint32_t)reg));
      emit_set_nz(c, reg);
      break;
    }
    case OP_ASL: case OP_LSR: case OP_ROL: case OP_ROR: {
      bool acc = info->mode == AM_ACC;
      Ea ea = {true, 0};
      int reg = acc ? W_A : 9;
      if (!acc) {
        ea = emit_ea(c, pc, info->mode, false);
        emit_load_addr(c, ea.is_const, ea.addr, 9);
      }
      if (info->op == OP_ASL) {
        emit_u32(e, a64_lsr_imm(false, 2, reg, 7));
        emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFFEu));
        emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 2, A64_LSL, 0));
        emit_u32(e, a64_lsl_imm(false, reg, reg, 1));
        emit_u32(e, a64_uxtb((uint32_t)reg, (uint32_t)reg));
      } else if (info->op == OP_LSR) {
        emit_u32(e, a64_logic_imm(0, 2, reg, 1));
        emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFFEu));
        emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 2, A64_LSL, 0));
        emit_u32(e, a64_lsr_imm(false, reg, reg, 1));
      } else if (info->op == OP_ROL) {
        emit_u32(e, a64_logic_imm(0, 3, W_P, 1));       /* carry in */
        emit_u32(e, a64_lsr_imm(false, 2, reg, 7));     /* carry out */
        emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFFEu));
        emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 2, A64_LSL, 0));
        emit_u32(e, a64_lsl_imm(false, reg, reg, 1));
        emit_u32(e, a64_uxtb((uint32_t)reg, (uint32_t)reg));
        emit_u32(e, a64_logic_reg(false, 1, (uint32_t)reg, (uint32_t)reg, 3, A64_LSL, 0));
      } else {
        emit_u32(e, a64_logic_imm(0, 3, W_P, 1));       /* carry in */
        emit_u32(e, a64_logic_imm(0, 2, reg, 1));       /* carry out */
        emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFFEu));
        emit_u32(e, a64_logic_reg(false, 1, W_P, W_P, 2, A64_LSL, 0));
        emit_u32(e, a64_lsr_imm(false, reg, reg, 1));
        emit_u32(e, a64_logic_reg(false, 1, (uint32_t)reg, (uint32_t)reg, 3, A64_LSL, 7));
      }
      emit_set_nz(c, reg);
      if (!acc) emit_store_checked(c, 9, ea.is_const, ea.addr, epilogue, index + 1);
      break;
    }
    case OP_JMP:
      if (info->mode == AM_IND) return false; /* handled by the caller */
      {
        uint16_t target = (uint16_t)(cbyte(c, (uint16_t)(pc + 1)) |
                                       (cbyte(c, (uint16_t)(pc + 2)) << 8));
        emit_exit(c, epilogue, index + 1, -1, target, true);
        ends = true;
      }
      break;
    case OP_JSR: {
      uint16_t target = (uint16_t)(cbyte(c, (uint16_t)(pc + 1)) |
                                       (cbyte(c, (uint16_t)(pc + 2)) << 8));
      uint16_t ret = (uint16_t)(pc + 2);
      /* Push high byte at $0100+SP, then low byte at $0100+((SP-1)&0xFF);
       * the low address must wrap inside page one, not spill into page zero. */
      emit_u32(e, a64_addsub_imm(false, 2, W_SP, 0x100, false));
      emit_u32(e, a64_movz(false, 3, (uint16_t)(ret >> 8), 0));
      emit_u32(e, a64_strb_reg(3, X_MEM, 2));
      emit_u32(e, a64_addsub_imm(false, 4, W_SP, 1, true));
      emit_u32(e, a64_uxtb(4, 4));
      emit_u32(e, a64_addsub_imm(false, 2, 4, 0x100, false));
      emit_u32(e, a64_movz(false, 3, (uint16_t)(ret & 0xFF), 0));
      emit_u32(e, a64_strb_reg(3, X_MEM, 2));
      /* JSR pushes two bytes; the address scratch used sp-1, so subtract 2. */
      emit_u32(e, a64_addsub_imm(false, W_SP, W_SP, 2, true));
      emit_u32(e, a64_uxtb(W_SP, W_SP));
      emit_exit(c, epilogue, index + 1, -1, target, true);
      ends = true;
      break;
    }
    case OP_RTS: {
      emit_u32(e, a64_addsub_imm(false, W_SP, W_SP, 1, false));
      emit_u32(e, a64_uxtb(W_SP, W_SP));
      emit_u32(e, a64_addsub_imm(false, 2, W_SP, 0x100, false));
      emit_u32(e, a64_ldrb_reg(3, X_MEM, 2));
      emit_u32(e, a64_addsub_imm(false, W_SP, W_SP, 1, false));
      emit_u32(e, a64_uxtb(W_SP, W_SP));
      emit_u32(e, a64_addsub_imm(false, 2, W_SP, 0x100, false));
      emit_u32(e, a64_ldrb_reg(4, X_MEM, 2));
      emit_u32(e, a64_logic_reg(false, 1, 3, 3, 4, A64_LSL, 8));
      emit_u32(e, a64_addsub_imm(false, 3, 3, 1, false));
      emit_u32(e, a64_ubfm(false, 3, 3, 0, 15));
      emit_u32(e, a64_addsub_reg(true, X_PC, X_MEM, 3, A64_LSL, 0, false));
      emit_exit(c, epilogue, index + 1, 3, 0, false);
      ends = true;
      break;
    }
    case OP_PHA: emit_push(c, W_A); break;
    case OP_PHP:
      emit_u32(e, a64_logic_imm(1, 2, W_P, 0x30)); /* p | B | U */
      emit_push(c, 2);
      break;
    case OP_PLA:
      emit_pull(c, W_A);
      emit_set_nz(c, W_A);
      break;
    case OP_PLP:
      emit_pull(c, 25);
      emit_u32(e, a64_logic_imm(1, W_P, W_P, 0x20)); /* U stays set */
      break;
    case OP_CLC: emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFFEu)); break;
    case OP_SEC: emit_u32(e, a64_logic_imm(1, W_P, W_P, 0x01)); break;
    case OP_CLI: emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFFBu)); break;
    case OP_SEI: emit_u32(e, a64_logic_imm(1, W_P, W_P, 0x04)); break;
    case OP_CLV: emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFBFu)); break;
    case OP_CLD: emit_u32(e, a64_logic_imm(0, W_P, W_P, 0xFFFFFFF7u)); break;
    case OP_SED: emit_u32(e, a64_logic_imm(1, W_P, W_P, 0x08)); break;
    case OP_NOP: break;
    case OP_BCC: case OP_BCS: case OP_BEQ: case OP_BNE:
    case OP_BMI: case OP_BPL: case OP_BVS: case OP_BVC:
      /* Both directions leave the block: the taken path inside emit_branch,
       * the fallthrough here.  Ending at the branch keeps the block's
       * instruction count exact, which the dispatcher's budget relies on. */
      /* The taken path exits inside emit_branch (and reports its own
       * instruction count); the fallthrough keeps translating. */
      emit_branch(c, pc, cbyte(c, pc), info, epilogue, index + 1);
      break;
    default:
      return false;
  }
  (void)read;
  return ends;
}

/* ---- prologue / epilogue ------------------------------------------------ */

static void emit_prologue(Compiler *c) {
  Emitter *e = c->e;
  emit_u32(e, a64_stp_pre(29, 30, 31, -16));
  emit_u32(e, a64_stp_pre(19, 20, 31, -16));
  emit_u32(e, a64_stp_pre(21, 22, 31, -16));
  emit_u32(e, a64_stp_pre(23, 24, 31, -16));
  emit_u32(e, a64_stp_pre(25, 26, 31, -16));
  emit_u32(e, a64_stp_pre(27, 28, 31, -16));
  emit_u32(e, a64_mov_reg(true, X_CTX, R0));
  emit_u32(e, a64_ldr_off(X_MEM, X_CTX, (uint32_t)offsetof(Cpu, mem)));
  emit_u32(e, a64_ldrb_off(W_A, X_CTX, (uint32_t)offsetof(Cpu, a)));
  emit_u32(e, a64_ldrb_off(W_X, X_CTX, (uint32_t)offsetof(Cpu, x)));
  emit_u32(e, a64_ldrb_off(W_Y, X_CTX, (uint32_t)offsetof(Cpu, y)));
  emit_u32(e, a64_ldrb_off(W_SP, X_CTX, (uint32_t)offsetof(Cpu, sp)));
  emit_u32(e, a64_ldrb_off(W_P, X_CTX, (uint32_t)offsetof(Cpu, p)));
  emit_u32(e, a64_ldrh_off(0, X_CTX, (uint32_t)offsetof(Cpu, pc)));
  emit_u32(e, a64_addsub_reg(true, X_PC, X_MEM, 0, A64_LSL, 0, false));
  emit_u32(e, a64_ldr_off(X_CYC, X_CTX, (uint32_t)offsetof(Cpu, cycles)));
  emit_u32(e, a64_movz(false, W_NZMASK, 0x7D, 0));
  emit_mov_imm64(c, 29, (uint64_t)(uintptr_t)&g_jit_rt); /* bus hooks + resume table */
  emit_u32(e, a64_movz(false, 0, 0, 0)); /* w0 = executed instruction count */
}

static void emit_epilogue(Compiler *c) {
  Emitter *e = c->e;
  emit_u32(e, a64_addsub_reg(true, 1, X_PC, X_MEM, A64_LSL, 0, true));
  emit_u32(e, a64_ubfm(false, 1, 1, 0, 15)); /* uxth w1, w1 */
  emit_u32(e, a64_strh_off(1, X_CTX, (uint32_t)offsetof(Cpu, pc)));
  emit_u32(e, a64_strb_off(W_A, X_CTX, (uint32_t)offsetof(Cpu, a)));
  emit_u32(e, a64_strb_off(W_X, X_CTX, (uint32_t)offsetof(Cpu, x)));
  emit_u32(e, a64_strb_off(W_Y, X_CTX, (uint32_t)offsetof(Cpu, y)));
  emit_u32(e, a64_strb_off(W_SP, X_CTX, (uint32_t)offsetof(Cpu, sp)));
  emit_u32(e, a64_strb_off(W_P, X_CTX, (uint32_t)offsetof(Cpu, p)));
  emit_u32(e, a64_str_off(X_CYC, X_CTX, (uint32_t)offsetof(Cpu, cycles)));
  emit_u32(e, a64_ldp_post(27, 28, 31, 16));
  emit_u32(e, a64_ldp_post(25, 26, 31, 16));
  emit_u32(e, a64_ldp_post(23, 24, 31, 16));
  emit_u32(e, a64_ldp_post(21, 22, 31, 16));
  emit_u32(e, a64_ldp_post(19, 20, 31, 16));
  emit_u32(e, a64_ldp_post(29, 30, 31, 16));
  emit_u32(e, a64_ret()); /* w0 already holds the executed instruction count */
}

/* True when this instruction may touch $2000+ (PPU/APU/mapper), in which case
 * the block must end so the dispatcher can step the PPU before continuing. */
static bool touches_io(const Compiler *c, uint16_t start, uint16_t next_pc,
                       const OpcodeInfo *info) {
  if (info->op == OP_JMP || info->op == OP_JSR || info->op == OP_RTS ||
      info->op == OP_BRK || info->op == OP_RTI) {
    return false; /* only stack access, which is WRAM */
  }
  switch (info->mode) {
    case AM_IMM:
    case AM_IMP:
    case AM_ACC:
    case AM_REL:
    case AM_ZP:
    case AM_ZPX:
    case AM_ZPY:
      return false;
    case AM_ABS: {
      uint16_t pc = (uint16_t)(next_pc - 3);
      uint16_t a = (uint16_t)(cbyte(c, (uint16_t)(pc + 1)) |
                              (cbyte(c, (uint16_t)(pc + 2)) << 8));
      /* Only OAM DMA needs the block boundary: it adds CPU cycles. */
      return a == 0x4014;
    }
    default:
      /* Indexed/indirect: almost always WRAM or PRG.  Ending every block here
       * would destroy block density, so the PPU catches up at block exit
       * instead (documented approximation). */
      return false;
  }
  (void)start;
}

int jit_compile(Emitter *e, const uint8_t *mem, uint8_t (*code_read)(uint16_t),
                uint16_t start_pc, int has_stop, uint16_t stop_pc,
                int max_insts, uint16_t code_lo, uint16_t code_hi, int chaining,
                int bus, int (**out_fn)(Cpu *), int (**out_resume)(Cpu *)) {
  Compiler c = {
      .e = e,
      .mem = mem,
      .code_read = code_read,
      .start_pc = start_pc,
      .stop_pc = stop_pc,
      .code_lo = code_lo,
      .code_hi = code_hi,
      .chaining = chaining,
      .bus = bus,
      .count = 0,
      .next_label = 2,
  };
  const int epilogue = 0;

  emit_prologue(&c);
  emit_label(e, 1); /* label 1 = chaining resume point */
  c.next_label = 2;

  uint16_t pc = start_pc;
  if (max_insts < 1) max_insts = 1;
  if (max_insts > MAX_BLOCK_INST) max_insts = MAX_BLOCK_INST;
  for (int i = 0; i < max_insts; i++) {
    if (has_stop && pc == stop_pc) break;
    if ((pc & 0xFF00u) != (start_pc & 0xFF00u)) break;
    uint8_t opcode = cbyte(&c, pc);
    const OpcodeInfo info = kOpcodes[opcode];
    if (info.op == OP_ILL) break;
    if (info.op == OP_BRK || info.op == OP_RTI) break;
    if (info.op == OP_JMP && info.mode == AM_IND) break;

    bool ends = emit_instruction(&c, pc, &info, epilogue, c.count);
    c.count++;
    pc = (uint16_t)(pc + inst_len(info.mode));
    if (ends) break;
    if (c.bus && touches_io(&c, start_pc, pc, &info)) break;
    if (e->oom) break;
  }

  /* Normal fallthrough exit: chain to the next instruction's block. */
  emit_exit(&c, epilogue, c.count, -1, pc, false);
  emit_label(e, epilogue);
  emit_epilogue(&c);
  if (!emit_finish(e)) return 0;
  if (c.count == 0) return 0;
  *out_fn = (int (*)(Cpu *))(void *)e->buf;
  *out_resume = (int (*)(Cpu *))(void *)(e->buf + e->label_pos[1]);
  return c.count;
}
