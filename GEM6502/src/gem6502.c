/*
 * GEM6502 - a small GEM5-inspired fast cycle model for the Open6502 SoC.
 *
 * The model keeps the parts that matter for a 6502/NES SoC:
 *   - an event queue,
 *   - explicit clock/cycle accounting,
 *   - a bus with RAM/PPU/APU/cartridge decode,
 *   - a CPU with 6502 cycle counts,
 *   - PPU vblank/NMI and a minimal APU/controller register file.
 *
 * It is deliberately not a gate-level model and not a full NES PPU renderer.
 */
#include "gem6502.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  M_IMP, M_ACC, M_IMM, M_ZP, M_ZPX, M_ZPY, M_ABS, M_ABSX, M_ABSY,
  M_IND, M_INDX, M_INDY, M_REL
};

enum {
  O_ILL, O_LDA, O_LDX, O_LDY, O_STA, O_STX, O_STY,
  O_ADC, O_SBC, O_AND, O_ORA, O_EOR, O_CMP, O_CPX, O_CPY, O_BIT,
  O_INC, O_DEC, O_ASL, O_LSR, O_ROL, O_ROR,
  O_JMP, O_JSR, O_RTS, O_BRK, O_RTI,
  O_PHA, O_PLA, O_PHP, O_PLP,
  O_BPL, O_BMI, O_BVC, O_BVS, O_BCC, O_BCS, O_BNE, O_BEQ,
  O_INX, O_DEX, O_INY, O_DEY,
  O_TAX, O_TXA, O_TAY, O_TYA, O_TSX, O_TXS,
  O_CLC, O_SEC, O_CLI, O_SEI, O_CLD, O_SED, O_CLV, O_NOP
};

typedef struct {
  uint8_t mode;
  uint8_t cycles;
  uint8_t op;
} OpInfo;

#define OP(code, m, c, o) [code] = { (m), (c), (o) }

static const OpInfo optab[256] = {
  OP(0x00, M_IMP, 7, O_BRK), OP(0x01, M_INDX, 6, O_ORA),
  OP(0x05, M_ZP, 3, O_ORA), OP(0x06, M_ZP, 5, O_ASL),
  OP(0x08, M_IMP, 3, O_PHP), OP(0x09, M_IMM, 2, O_ORA),
  OP(0x0A, M_ACC, 2, O_ASL), OP(0x0D, M_ABS, 4, O_ORA),
  OP(0x0E, M_ABS, 6, O_ASL),
  OP(0x10, M_REL, 2, O_BPL), OP(0x11, M_INDY, 5, O_ORA),
  OP(0x15, M_ZPX, 4, O_ORA), OP(0x16, M_ZPX, 6, O_ASL),
  OP(0x18, M_IMP, 2, O_CLC), OP(0x19, M_ABSY, 4, O_ORA),
  OP(0x1D, M_ABSX, 4, O_ORA), OP(0x1E, M_ABSX, 7, O_ASL),
  OP(0x20, M_ABS, 6, O_JSR), OP(0x21, M_INDX, 6, O_AND),
  OP(0x24, M_ZP, 3, O_BIT), OP(0x25, M_ZP, 3, O_AND),
  OP(0x26, M_ZP, 5, O_ROL), OP(0x28, M_IMP, 4, O_PLP),
  OP(0x29, M_IMM, 2, O_AND), OP(0x2A, M_ACC, 2, O_ROL),
  OP(0x2C, M_ABS, 4, O_BIT), OP(0x2D, M_ABS, 4, O_AND),
  OP(0x2E, M_ABS, 6, O_ROL),
  OP(0x30, M_REL, 2, O_BMI), OP(0x31, M_INDY, 5, O_AND),
  OP(0x35, M_ZPX, 4, O_AND), OP(0x36, M_ZPX, 6, O_ROL),
  OP(0x38, M_IMP, 2, O_SEC), OP(0x39, M_ABSY, 4, O_AND),
  OP(0x3D, M_ABSX, 4, O_AND), OP(0x3E, M_ABSX, 7, O_ROL),
  OP(0x40, M_IMP, 6, O_RTI), OP(0x41, M_INDX, 6, O_EOR),
  OP(0x45, M_ZP, 3, O_EOR), OP(0x46, M_ZP, 5, O_LSR),
  OP(0x48, M_IMP, 3, O_PHA), OP(0x49, M_IMM, 2, O_EOR),
  OP(0x4A, M_ACC, 2, O_LSR), OP(0x4C, M_ABS, 3, O_JMP),
  OP(0x4D, M_ABS, 4, O_EOR), OP(0x4E, M_ABS, 6, O_LSR),
  OP(0x50, M_REL, 2, O_BVC), OP(0x51, M_INDY, 5, O_EOR),
  OP(0x55, M_ZPX, 4, O_EOR), OP(0x56, M_ZPX, 6, O_LSR),
  OP(0x58, M_IMP, 2, O_CLI), OP(0x59, M_ABSY, 4, O_EOR),
  OP(0x5D, M_ABSX, 4, O_EOR), OP(0x5E, M_ABSX, 7, O_LSR),
  OP(0x60, M_IMP, 6, O_RTS), OP(0x61, M_INDX, 6, O_ADC),
  OP(0x65, M_ZP, 3, O_ADC), OP(0x66, M_ZP, 5, O_ROR),
  OP(0x68, M_IMP, 4, O_PLA), OP(0x69, M_IMM, 2, O_ADC),
  OP(0x6A, M_ACC, 2, O_ROR), OP(0x6C, M_IND, 5, O_JMP),
  OP(0x6D, M_ABS, 4, O_ADC), OP(0x6E, M_ABS, 6, O_ROR),
  OP(0x70, M_REL, 2, O_BVS), OP(0x71, M_INDY, 5, O_ADC),
  OP(0x75, M_ZPX, 4, O_ADC), OP(0x76, M_ZPX, 6, O_ROR),
  OP(0x78, M_IMP, 2, O_SEI), OP(0x79, M_ABSY, 4, O_ADC),
  OP(0x7D, M_ABSX, 4, O_ADC), OP(0x7E, M_ABSX, 7, O_ROR),
  OP(0x81, M_INDX, 6, O_STA), OP(0x84, M_ZP, 3, O_STY),
  OP(0x85, M_ZP, 3, O_STA), OP(0x86, M_ZP, 3, O_STX),
  OP(0x88, M_IMP, 2, O_DEY), OP(0x8A, M_IMP, 2, O_TXA),
  OP(0x8C, M_ABS, 4, O_STY), OP(0x8D, M_ABS, 4, O_STA),
  OP(0x8E, M_ABS, 4, O_STX),
  OP(0x90, M_REL, 2, O_BCC), OP(0x91, M_INDY, 6, O_STA),
  OP(0x94, M_ZPX, 4, O_STY), OP(0x95, M_ZPX, 4, O_STA),
  OP(0x96, M_ZPY, 4, O_STX), OP(0x98, M_IMP, 2, O_TYA),
  OP(0x99, M_ABSY, 5, O_STA), OP(0x9A, M_IMP, 2, O_TXS),
  OP(0x9D, M_ABSX, 5, O_STA),
  OP(0xA0, M_IMM, 2, O_LDY), OP(0xA1, M_INDX, 6, O_LDA),
  OP(0xA2, M_IMM, 2, O_LDX), OP(0xA4, M_ZP, 3, O_LDY),
  OP(0xA5, M_ZP, 3, O_LDA), OP(0xA6, M_ZP, 3, O_LDX),
  OP(0xA8, M_IMP, 2, O_TAY), OP(0xA9, M_IMM, 2, O_LDA),
  OP(0xAA, M_IMP, 2, O_TAX), OP(0xAC, M_ABS, 4, O_LDY),
  OP(0xAD, M_ABS, 4, O_LDA), OP(0xAE, M_ABS, 4, O_LDX),
  OP(0xB0, M_REL, 2, O_BCS), OP(0xB1, M_INDY, 5, O_LDA),
  OP(0xB4, M_ZPX, 4, O_LDY), OP(0xB5, M_ZPX, 4, O_LDA),
  OP(0xB6, M_ZPY, 4, O_LDX), OP(0xB8, M_IMP, 2, O_CLV),
  OP(0xB9, M_ABSY, 4, O_LDA), OP(0xBA, M_IMP, 2, O_TSX),
  OP(0xBC, M_ABSX, 4, O_LDY), OP(0xBD, M_ABSX, 4, O_LDA),
  OP(0xBE, M_ABSY, 4, O_LDX),
  OP(0xC0, M_IMM, 2, O_CPY), OP(0xC1, M_INDX, 6, O_CMP),
  OP(0xC4, M_ZP, 3, O_CPY), OP(0xC5, M_ZP, 3, O_CMP),
  OP(0xC6, M_ZP, 5, O_DEC), OP(0xC8, M_IMP, 2, O_INY),
  OP(0xC9, M_IMM, 2, O_CMP), OP(0xCA, M_IMP, 2, O_DEX),
  OP(0xCC, M_ABS, 4, O_CPY), OP(0xCD, M_ABS, 4, O_CMP),
  OP(0xCE, M_ABS, 6, O_DEC),
  OP(0xD0, M_REL, 2, O_BNE), OP(0xD1, M_INDY, 5, O_CMP),
  OP(0xD5, M_ZPX, 4, O_CMP), OP(0xD6, M_ZPX, 6, O_DEC),
  OP(0xD8, M_IMP, 2, O_CLD), OP(0xD9, M_ABSY, 4, O_CMP),
  OP(0xDD, M_ABSX, 4, O_CMP), OP(0xDE, M_ABSX, 7, O_DEC),
  OP(0xE0, M_IMM, 2, O_CPX), OP(0xE1, M_INDX, 6, O_SBC),
  OP(0xE4, M_ZP, 3, O_CPX), OP(0xE5, M_ZP, 3, O_SBC),
  OP(0xE6, M_ZP, 5, O_INC), OP(0xE8, M_IMP, 2, O_INX),
  OP(0xE9, M_IMM, 2, O_SBC), OP(0xEA, M_IMP, 2, O_NOP),
  OP(0xEC, M_ABS, 4, O_CPX), OP(0xED, M_ABS, 4, O_SBC),
  OP(0xEE, M_ABS, 6, O_INC),
  OP(0xF0, M_REL, 2, O_BEQ), OP(0xF1, M_INDY, 5, O_SBC),
  OP(0xF5, M_ZPX, 4, O_SBC), OP(0xF6, M_ZPX, 6, O_INC),
  OP(0xF8, M_IMP, 2, O_SED), OP(0xF9, M_ABSY, 4, O_SBC),
  OP(0xFD, M_ABSX, 4, O_SBC), OP(0xFE, M_ABSX, 7, O_INC)
};

#define F_C 0x01
#define F_Z 0x02
#define F_I 0x04
#define F_D 0x08
#define F_B 0x10
#define F_U 0x20
#define F_V 0x40
#define F_N 0x80

static uint8_t read8(Gem *g, uint16_t a) { return gem_bus_read(g, a, true); }
static void write8(Gem *g, uint16_t a, uint8_t v) { gem_bus_write(g, a, v); }
static uint8_t fetch8(Gem *g) { uint8_t v = read8(g, g->cpu.pc); g->cpu.pc++; return v; }
static uint16_t fetch16(Gem *g) { uint16_t lo = fetch8(g); return (uint16_t)(lo | (fetch8(g) << 8)); }
static uint8_t getf(Gem *g, uint8_t f) { return (g->cpu.p & f) != 0; }
static void setf(Gem *g, uint8_t f, bool on) { if (on) g->cpu.p |= f; else g->cpu.p &= (uint8_t)~f; }
static void set_nz(Gem *g, uint8_t v) {
  setf(g, F_N, (v & 0x80) != 0); setf(g, F_Z, v == 0);
}
static void push8(Gem *g, uint8_t v) { write8(g, (uint16_t)(0x0100 | g->cpu.sp), v); g->cpu.sp--; }
static uint8_t pull8(Gem *g) { g->cpu.sp++; return read8(g, (uint16_t)(0x0100 | g->cpu.sp)); }
static uint16_t read16zp(Gem *g, uint8_t zp) {
  uint8_t lo = read8(g, zp); uint8_t hi = read8(g, (uint8_t)(zp + 1));
  return (uint16_t)(lo | (hi << 8));
}
static uint16_t read16(Gem *g, uint16_t a) {
  uint8_t lo = read8(g, a); uint8_t hi = read8(g, (uint16_t)(a + 1));
  return (uint16_t)(lo | (hi << 8));
}

static void adc(Gem *g, uint8_t v) {
  uint8_t olda = g->cpu.a;
  unsigned c = getf(g, F_C) ? 1 : 0;
  unsigned sum = olda + v + c;
  if (getf(g, F_D)) {
    unsigned lo = (g->cpu.a & 0x0F) + (v & 0x0F) + c;
    unsigned hi = (g->cpu.a >> 4) + (v >> 4);
    if (lo > 9) { lo += 6; hi++; }
    if (hi > 9) hi += 6;
    g->cpu.a = (uint8_t)((hi << 4) | (lo & 0x0F));
    setf(g, F_C, hi > 0x0F);
  } else {
    g->cpu.a = (uint8_t)sum;
    setf(g, F_C, sum > 0xFF);
  }
  setf(g, F_V, (~(olda ^ v) & (olda ^ (uint8_t)sum) & 0x80) != 0);
  set_nz(g, g->cpu.a);
}

static void sbc(Gem *g, uint8_t v) {
  uint8_t olda = g->cpu.a;
  unsigned borrow = getf(g, F_C) ? 0 : 1;
  int diff = (int)olda - v - (int)borrow;
  if (getf(g, F_D)) {
    int lo = (g->cpu.a & 0x0F) - (v & 0x0F) - (int)borrow;
    int hi = (g->cpu.a >> 4) - (v >> 4);
    if (lo < 0) { lo -= 6; hi--; }
    if (hi < 0) hi -= 6;
    g->cpu.a = (uint8_t)((hi << 4) | (lo & 0x0F));
    setf(g, F_C, hi >= 0);
  } else {
    g->cpu.a = (uint8_t)diff;
    setf(g, F_C, diff >= 0);
  }
  setf(g, F_V, ((olda ^ v) & (olda ^ g->cpu.a) & 0x80) != 0);
  set_nz(g, g->cpu.a);
}

static void cmp(Gem *g, uint8_t reg, uint8_t v) {
  unsigned d = (unsigned)reg - v;
  setf(g, F_C, reg >= v); set_nz(g, (uint8_t)d);
}

/* Compute effective address.  Returns base cycles and sets page_cross. */
static uint16_t addr_mode(Gem *g, uint8_t mode, bool *page_cross) {
  uint16_t base;
  *page_cross = false;
  switch (mode) {
    case M_IMM: return g->cpu.pc++;
    case M_ZP: return fetch8(g);
    case M_ZPX: return (uint8_t)(fetch8(g) + g->cpu.x);
    case M_ZPY: return (uint8_t)(fetch8(g) + g->cpu.y);
    case M_ABS: return fetch16(g);
    case M_ABSX:
      base = fetch16(g);
      { uint16_t a = (uint16_t)(base + g->cpu.x); *page_cross = (base & 0xFF00) != (a & 0xFF00); return a; }
    case M_ABSY:
      base = fetch16(g);
      { uint16_t a = (uint16_t)(base + g->cpu.y); *page_cross = (base & 0xFF00) != (a & 0xFF00); return a; }
    case M_INDX: {
      uint8_t zp = (uint8_t)(fetch8(g) + g->cpu.x);
      return read16zp(g, zp);
    }
    case M_INDY: {
      uint8_t zp = fetch8(g);
      base = read16zp(g, zp);
      { uint16_t a = (uint16_t)(base + g->cpu.y); *page_cross = (base & 0xFF00) != (a & 0xFF00); return a; }
    }
    case M_IND: {
      uint16_t p = fetch16(g);
      uint8_t lo = read8(g, p);
      uint8_t hi = read8(g, (uint16_t)((p & 0xFF00) | ((p + 1) & 0x00FF)));
      return (uint16_t)(lo | (hi << 8));
    }
    case M_REL: return fetch8(g);
    default: return 0;
  }
}

static bool is_read_op(uint8_t op) {
  switch (op) {
    case O_LDA: case O_LDX: case O_LDY: case O_ADC: case O_SBC:
    case O_AND: case O_ORA: case O_EOR: case O_CMP: case O_CPX: case O_CPY:
    case O_BIT: return true;
    default: return false;
  }
}

typedef struct { uint16_t jsr_pc, target, ret; uint8_t sp; } JsrHist;
static JsrHist g_jsr_hist[16];
static int g_jsr_idx;

static int cpu_step(Gem *g) {
  GemCpu *c = &g->cpu;
  if (c->halted) return 0;
  if (c->dma_stall > 0) {
    int n = c->dma_stall > 64 ? 64 : c->dma_stall;
    c->dma_stall -= n;
    return n;
  }
  if (g->ppu.nmi_pending) {
    g->ppu.nmi_pending = false;
    push8(g, (uint8_t)(c->pc >> 8)); push8(g, (uint8_t)c->pc);
    push8(g, (uint8_t)(c->p | F_U));
    setf(g, F_I, true);
    c->pc = read16(g, 0xFFFA);
    g->stats.nmi_count++;
    c->cycles += 7;
    return 7;
  }
  if ((g->apu.irq || g->bus.cart_irq) && !getf(g, F_I)) {
    if (getenv("GEM_DEBUG_IRQ"))
      fprintf(stderr, "IRQ entry pc=%04x sp=%02x p=%02x cart=%d apu=%d\n",
              c->pc, c->sp, c->p, g->bus.cart_irq, g->apu.irq);
    push8(g, (uint8_t)(c->pc >> 8)); push8(g, (uint8_t)c->pc);
    push8(g, (uint8_t)(c->p | F_U));
    setf(g, F_I, true);
    c->pc = read16(g, 0xFFFE);
    c->cycles += 7;
    return 7;
  }
  uint16_t op_pc = c->pc;
  if (getenv("GEM_DEBUG_IRQ") && op_pc == 0xFBA0 &&
      read8(g, (uint16_t)(0x100 | (uint8_t)(c->sp + 1))) == 0 &&
      read8(g, (uint16_t)(0x100 | (uint8_t)(c->sp + 2))) == 0) {
    fprintf(stderr, "FBA0 sp=%02x m4 sel=%02x regs=%02x %02x %02x %02x %02x %02x %02x %02x\n",
            c->sp, g->bus.m4_select, g->bus.m4_regs[0], g->bus.m4_regs[1],
            g->bus.m4_regs[2], g->bus.m4_regs[3], g->bus.m4_regs[4],
            g->bus.m4_regs[5], g->bus.m4_regs[6], g->bus.m4_regs[7]);
    fprintf(stderr, "FBA0 stack:");
    for (int i = 1; i <= 16; i++) fprintf(stderr, " %02x", read8(g, (uint16_t)(0x100 | (uint8_t)(c->sp + i))));
    fprintf(stderr, "\n");
    for (int i = 0; i < 16; i++) {
      JsrHist *h = &g_jsr_hist[(g_jsr_idx + i) & 15];
      if (h->jsr_pc) fprintf(stderr, "  JSR pc=%04x target=%04x ret=%04x sp=%02x\n", h->jsr_pc, h->target, h->ret, h->sp);
    }
  }
  if (getenv("GEM_DEBUG_IRQ") && (op_pc == 0xF55B || op_pc == 0xF56B)) {
    fprintf(stderr, "PC=%04x sp=%02x p=%02x stack=", op_pc, c->sp, c->p);
    for (int i = 1; i <= 6; i++) fprintf(stderr, "%02x ", read8(g, (uint16_t)(0x100 | (uint8_t)(c->sp + i))));
    fprintf(stderr, "\n");
  }
  uint8_t opcode = fetch8(g);
  const OpInfo *info = &optab[opcode];
  if (info->op == O_ILL) {
    if (getenv("GEM_DEBUG_ILL")) fprintf(stderr, "ILL opcode=%02x pc=%04x\n", opcode, op_pc);
    g->stats.illegal_opcodes++;
    c->halted = true;
    c->stop_reason = "illegal opcode";
    return 2;
  }
  bool page_cross = false;
  uint16_t ea = 0;
  if (info->mode != M_IMP && info->mode != M_ACC)
    ea = addr_mode(g, info->mode, &page_cross);
  int cycles = info->cycles;
  uint8_t v = 0;
  bool read_op = is_read_op(info->op);

  if (read_op && page_cross) cycles++;

  switch (info->op) {
    case O_LDA: c->a = read8(g, ea); set_nz(g, c->a); break;
    case O_LDX: c->x = read8(g, ea); set_nz(g, c->x); break;
    case O_LDY: c->y = read8(g, ea); set_nz(g, c->y); break;
    case O_STA: write8(g, ea, c->a); break;
    case O_STX: write8(g, ea, c->x); break;
    case O_STY: write8(g, ea, c->y); break;
    case O_ADC: adc(g, read8(g, ea)); break;
    case O_SBC: sbc(g, read8(g, ea)); break;
    case O_AND: c->a &= read8(g, ea); set_nz(g, c->a); break;
    case O_ORA: c->a |= read8(g, ea); set_nz(g, c->a); break;
    case O_EOR: c->a ^= read8(g, ea); set_nz(g, c->a); break;
    case O_CMP: cmp(g, c->a, read8(g, ea)); break;
    case O_CPX: cmp(g, c->x, read8(g, ea)); break;
    case O_CPY: cmp(g, c->y, read8(g, ea)); break;
    case O_BIT:
      v = read8(g, ea);
      setf(g, F_N, (v & 0x80) != 0);
      setf(g, F_V, (v & 0x40) != 0);
      setf(g, F_Z, (c->a & v) == 0);
      break;
    case O_INC: v = (uint8_t)(read8(g, ea) + 1); write8(g, ea, v); set_nz(g, v); break;
    case O_DEC: v = (uint8_t)(read8(g, ea) - 1); write8(g, ea, v); set_nz(g, v); break;
    case O_ASL: {
      v = (info->mode == M_ACC) ? c->a : read8(g, ea);
      setf(g, F_C, (v & 0x80) != 0); v <<= 1; set_nz(g, v);
      if (info->mode == M_ACC) c->a = v; else write8(g, ea, v);
      break;
    }
    case O_LSR: {
      v = (info->mode == M_ACC) ? c->a : read8(g, ea);
      setf(g, F_C, (v & 1) != 0); v >>= 1; set_nz(g, v);
      if (info->mode == M_ACC) c->a = v; else write8(g, ea, v);
      break;
    }
    case O_ROL: {
      uint8_t carry = getf(g, F_C);
      v = (info->mode == M_ACC) ? c->a : read8(g, ea);
      setf(g, F_C, (v & 0x80) != 0); v = (uint8_t)((v << 1) | carry); set_nz(g, v);
      if (info->mode == M_ACC) c->a = v; else write8(g, ea, v);
      break;
    }
    case O_ROR: {
      uint8_t carry = getf(g, F_C);
      v = (info->mode == M_ACC) ? c->a : read8(g, ea);
      setf(g, F_C, (v & 1) != 0); v = (uint8_t)((v >> 1) | (carry << 7)); set_nz(g, v);
      if (info->mode == M_ACC) c->a = v; else write8(g, ea, v);
      break;
    }
    case O_JMP: c->pc = ea; break;
    case O_JSR: {
      uint16_t target = ea;
      uint16_t ret = (uint16_t)(c->pc - 1);
      push8(g, (uint8_t)(ret >> 8)); push8(g, (uint8_t)ret);
      c->pc = target;
      g_jsr_hist[g_jsr_idx++ & 15] = (JsrHist){op_pc, target, ret, c->sp};
      break;
    }
    case O_RTS: {
      uint8_t lo = pull8(g), hi = pull8(g);
      c->pc = (uint16_t)(((hi << 8) | lo) + 1);
      break;
    }
    case O_BRK: {
      uint16_t ret = (uint16_t)(op_pc + 2);
      push8(g, (uint8_t)(ret >> 8)); push8(g, (uint8_t)ret);
      push8(g, (uint8_t)(c->p | F_B | F_U));
      setf(g, F_I, true);
      c->pc = read16(g, 0xFFFE);
      break;
    }
    case O_RTI: {
      c->p = pull8(g);
      uint8_t lo = pull8(g), hi = pull8(g);
      c->pc = (uint16_t)((hi << 8) | lo);
      break;
    }
    case O_PHA: push8(g, c->a); break;
    case O_PLA: c->a = pull8(g); set_nz(g, c->a); break;
    case O_PHP: push8(g, (uint8_t)(c->p | F_B | F_U)); break;
    case O_PLP: c->p = (uint8_t)(pull8(g) | F_U); break;
    case O_BPL: case O_BMI: case O_BVC: case O_BVS:
    case O_BCC: case O_BCS: case O_BNE: case O_BEQ: {
      bool take = false;
      switch (info->op) {
        case O_BPL: take = !getf(g, F_N); break;
        case O_BMI: take = getf(g, F_N); break;
        case O_BVC: take = !getf(g, F_V); break;
        case O_BVS: take = getf(g, F_V); break;
        case O_BCC: take = !getf(g, F_C); break;
        case O_BCS: take = getf(g, F_C); break;
        case O_BNE: take = !getf(g, F_Z); break;
        case O_BEQ: take = getf(g, F_Z); break;
        default: break;
      }
      if (take) {
        int8_t off = (int8_t)ea;
        uint16_t old = c->pc;
        c->pc = (uint16_t)(c->pc + off);
        cycles++;
        if ((old & 0xFF00) != (c->pc & 0xFF00)) cycles++;
      }
      break;
    }
    case O_INX: c->x++; set_nz(g, c->x); break;
    case O_DEX: c->x--; set_nz(g, c->x); break;
    case O_INY: c->y++; set_nz(g, c->y); break;
    case O_DEY: c->y--; set_nz(g, c->y); break;
    case O_TAX: c->x = c->a; set_nz(g, c->x); break;
    case O_TXA: c->a = c->x; set_nz(g, c->a); break;
    case O_TAY: c->y = c->a; set_nz(g, c->y); break;
    case O_TYA: c->a = c->y; set_nz(g, c->a); break;
    case O_TSX: c->x = c->sp; set_nz(g, c->x); break;
    case O_TXS: c->sp = c->x; break;
    case O_CLC: setf(g, F_C, false); break;
    case O_SEC: setf(g, F_C, true); break;
    case O_CLI: setf(g, F_I, false); break;
    case O_SEI: setf(g, F_I, true); break;
    case O_CLD: setf(g, F_D, false); break;
    case O_SED: setf(g, F_D, true); break;
    case O_CLV: setf(g, F_V, false); break;
    case O_NOP: break;
    default:
      c->halted = true;
      c->stop_reason = "unimplemented opcode";
      break;
  }
  c->cycles += (uint64_t)cycles;
  c->instructions++;
  g->stats.cpu_cycles = c->cycles;
  g->stats.instructions = c->instructions;
  return cycles;
}

/* ---------- Event queue ---------- */
static int ev_less(const GemEvent *a, const GemEvent *b) { return a->when < b->when; }
void gem_event_init(GemEventQueue *q) { memset(q, 0, sizeof(*q)); }
void gem_event_destroy(GemEventQueue *q) { free(q->heap); memset(q, 0, sizeof(*q)); }
int gem_event_push(GemEventQueue *q, GemEvent ev) {
  if (q->size == q->capacity) {
    size_t nc = q->capacity ? q->capacity * 2 : 16;
    GemEvent *nh = (GemEvent *)realloc(q->heap, nc * sizeof(GemEvent));
    if (!nh) return -1;
    q->heap = nh; q->capacity = nc;
  }
  size_t i = q->size++;
  q->heap[i] = ev;
  while (i > 0) {
    size_t p = (i - 1) / 2;
    if (!ev_less(&q->heap[i], &q->heap[p])) break;
    GemEvent t = q->heap[i]; q->heap[i] = q->heap[p]; q->heap[p] = t; i = p;
  }
  return 0;
}
int gem_event_pop(GemEventQueue *q, GemEvent *out) {
  if (q->size == 0) return -1;
  *out = q->heap[0];
  q->size--;
  if (q->size) {
    q->heap[0] = q->heap[q->size];
    size_t i = 0;
    for (;;) {
      size_t l = i * 2 + 1, r = l + 1, m = i;
      if (l < q->size && ev_less(&q->heap[l], &q->heap[m])) m = l;
      if (r < q->size && ev_less(&q->heap[r], &q->heap[m])) m = r;
      if (m == i) break;
      GemEvent t = q->heap[i]; q->heap[i] = q->heap[m]; q->heap[m] = t; i = m;
    }
  }
  return 0;
}
int gem_event_peek(const GemEventQueue *q, GemEvent *out) {
  if (!q->size) return -1;
  *out = q->heap[0]; return 0;
}
int gem_run_events(Gem *g, uint64_t until) {
  GemEvent ev;
  while (gem_event_peek(&g->events, &ev) == 0 && ev.when <= until) {
    gem_event_pop(&g->events, &ev);
    if (ev.callback) ev.callback(g, ev.arg);
    g->now = ev.when;
  }
  return 0;
}

/* ---------- Bus / devices ---------- */
static void ppu_register_write(Gem *g, uint8_t reg, uint8_t v) {
  GemPpu *p = &g->ppu;
  switch (reg) {
    case 0: p->ctrl = v; break;
    case 1: p->mask = v; break;
    case 3: p->oam_addr = v; break;
    case 4: p->oam[p->oam_addr++] = v; break;
    case 5:
      if (!p->w_toggle) p->scroll_x = v; else p->scroll_y = v;
      p->w_toggle = !p->w_toggle; break;
    case 6:
      if (!p->w_toggle) p->vaddr = (uint16_t)((p->vaddr & 0x00FF) | (v << 8));
      else p->vaddr = (uint16_t)((p->vaddr & 0xFF00) | v);
      p->w_toggle = !p->w_toggle; break;
    case 7:
      if (p->vaddr < 0x2000) p->chr[p->vaddr & 0x1FFF] = v;
      else if (p->vaddr < 0x3F00) p->vram[(p->vaddr - 0x2000) & 0x07FF] = v;
      else p->pal[p->vaddr & 0x1F] = v;
      p->vaddr = (uint16_t)(p->vaddr + ((p->ctrl & 0x04) ? 32 : 1));
      break;
    default: break;
  }
}
static uint8_t ppu_register_read(Gem *g, uint8_t reg) {
  GemPpu *p = &g->ppu;
  switch (reg) {
    case 2: {
      uint8_t v = p->status;
      p->status &= (uint8_t)~0x80;
      p->w_toggle = false;
      return v;
    }
    case 4: return p->oam[p->oam_addr];
    case 7: {
      uint8_t v = p->read_buf;
      if (p->vaddr < 0x2000) p->read_buf = p->chr[p->vaddr & 0x1FFF];
      else if (p->vaddr < 0x3F00) p->read_buf = p->vram[(p->vaddr - 0x2000) & 0x07FF];
      else return p->pal[p->vaddr & 0x1F];
      p->vaddr = (uint16_t)(p->vaddr + ((p->ctrl & 0x04) ? 32 : 1));
      return v;
    }
    default: return 0;
  }
}


static size_t cart_prg_offset(Gem *g, uint16_t addr) {
  size_t size = g->bus.cart_prg_size;
  if (size == 0) return (size_t)(addr - 0x8000);
  switch (g->bus.mapper) {
    case 0:
      return (size_t)(addr - 0x8000) & (size - 1u);
    case 1: {
      size_t bank;
      if (g->bus.m1_ctrl & 0x08u) {
        bank = (addr < 0xC000) ? (g->bus.m1_prg & 0x0Fu)
                               : (size / 0x4000u - 1u);
      } else {
        bank = (g->bus.m1_prg & 0x0Eu) >> 1;
      }
      return (bank * 0x4000u + (addr & 0x3FFFu)) % size;
    }
    case 2: {
      size_t bank = (addr < 0xC000) ? (g->bus.m2_bank & 0x0Fu)
                                    : (size / 0x4000u - 1u);
      return (bank * 0x4000u + (addr & 0x3FFFu)) % size;
    }
    case 3:
      return (size_t)(addr - 0x8000) & (size - 1u);
    case 7: {
      size_t bank = g->bus.m7_bank & 0x07u;
      return (bank * 0x8000u + (addr & 0x7FFFu)) % size;
    }
    case 4: {
      size_t bank;
      int slot = (addr >> 13) & 3;
      if (slot == 0) bank = g->bus.m4_prg_mode ? (size / 0x2000u - 2u) : (g->bus.m4_regs[6] & 0x3Fu);
      else if (slot == 1) bank = g->bus.m4_regs[7] & 0x3Fu;
      else if (slot == 2) bank = g->bus.m4_prg_mode ? (g->bus.m4_regs[6] & 0x3Fu) : (size / 0x2000u - 2u);
      else bank = size / 0x2000u - 1u;
      return (bank * 0x2000u + (addr & 0x1FFFu)) % size;
    }
    default:
      return (size_t)(addr - 0x8000) & (size - 1u);
  }
}

static void cart_write(Gem *g, uint16_t addr, uint8_t value) {
  switch (g->bus.mapper) {
    case 1:
      if (value & 0x80u) {
        g->bus.m1_shift = 0; g->bus.m1_count = 0;
      } else {
        g->bus.m1_shift |= (uint8_t)((value & 1u) << g->bus.m1_count);
        if (++g->bus.m1_count == 5) {
          uint8_t v = g->bus.m1_shift & 0x1Fu;
          if (addr < 0xA000) g->bus.m1_ctrl = v;
          else if (addr < 0xC000) g->bus.m1_chr0 = v;
          else if (addr < 0xE000) g->bus.m1_chr1 = v;
          else g->bus.m1_prg = v;
          g->bus.m1_shift = 0; g->bus.m1_count = 0;
        }
      }
      break;
    case 2: g->bus.m2_bank = value & 0x0Fu; break;
    case 3: g->bus.m3_bank = value & 0x03u; break;
    case 7: g->bus.m7_bank = value & 0x07u; break;
    case 4:
      if ((addr & 1u) == 0) {
        if (addr < 0xA000) {
          g->bus.m4_select = value & 0x07u;
          g->bus.m4_prg_mode = (value >> 6) & 1u;
          g->bus.m4_chr_mode = (value >> 7) & 1u;
        } else if (addr < 0xC000) {
          /* mirroring: not modelled in this fast model */
        } else if (addr < 0xE000) {
          g->bus.m4_irq_latch = value;
        } else {
          g->bus.m4_irq_enable = false;
          g->bus.cart_irq = false;
        }
      } else {
        if (addr < 0xA000) {
          g->bus.m4_regs[g->bus.m4_select & 7u] = value;
        } else if (addr < 0xC000) {
          /* PRG RAM protect */
        } else if (addr < 0xE000) {
          g->bus.m4_irq_counter = 0;
          g->bus.m4_irq_reload = 1;
        } else {
          g->bus.m4_irq_enable = true;
        }
      }
      break;
    default: break;
  }
}

uint8_t gem_bus_read(Gem *g, uint16_t addr, bool side_effect) {
  (void)side_effect;
  g->stats.bus_reads++;
  if (addr < 0x2000) return g->bus.ram[addr & 0x07FF];
  if (addr < 0x4000) return ppu_register_read(g, (uint8_t)(addr & 7));
  if (addr < 0x4020) {
    switch (addr) {
      case 0x4015: {
        uint8_t v = (uint8_t)(g->apu.regs[0x15] & 0x1F);
        if (g->apu.irq) { v |= 0x40; g->apu.irq = false; }
        return v;
      }
      case 0x4016: {
        uint8_t v = g->apu.joy1_shift & 1;
        g->apu.joy1_shift >>= 1;
        return v;
      }
      case 0x4017: {
        uint8_t v = g->apu.joy2_shift & 1;
        g->apu.joy2_shift >>= 1;
        return v;
      }
      default: return 0;
    }
  }
  if (addr >= 0x6000 && addr < 0x8000) return g->bus.ram[addr & 0x07FF];
  if (addr >= 0x8000) {
    if (g->bus.cart_prg_size) return g->bus.cart_prg[cart_prg_offset(g, addr)];
    return g->bus.prg[(addr - 0x8000) & 0x7FFF];
  }
  return 0;
}

void gem_bus_write(Gem *g, uint16_t addr, uint8_t value) {
  g->stats.bus_writes++;
  if (getenv("GEM_DEBUG_STACK") && addr >= 0x01F8 && addr <= 0x01FB)
    fprintf(stderr, "WR stack %04x=%02x pc=%04x sp=%02x\n", addr, value, g->cpu.pc, g->cpu.sp);
  if (addr < 0x2000) { g->bus.ram[addr & 0x07FF] = value; return; }
  if (addr < 0x4000) { ppu_register_write(g, (uint8_t)(addr & 7), value); return; }
  if (addr < 0x4020) {
    if (addr == 0x4014) {
      uint16_t page = (uint16_t)(value << 8);
      for (int i = 0; i < 256; i++)
        g->ppu.oam[i] = gem_bus_read(g, (uint16_t)(page + i), false);
      g->cpu.dma_stall = 513;
      return;
    }
    if (addr == 0x4016) {
      g->apu.joy1_shift = g->apu.joy1;
      g->apu.joy2_shift = g->apu.joy2;
      return;
    }
    if (addr == 0x4017) {
      g->apu.frame_mode = (value & 0x80) != 0;
      g->apu.frame_inhibit = (value & 0x40) != 0;
      g->apu.frame_step = 0;
      g->apu.frame_div = 0;
      if (g->apu.frame_inhibit) g->apu.irq = false;
    }
    g->apu.regs[addr & 0x1F] = value;
    return;
  }
  if (addr >= 0x6000 && addr < 0x8000) { g->bus.ram[addr & 0x07FF] = value; return; }
  if (addr >= 0x8000) { cart_write(g, addr, value); return; }
}

void gem_cart_a12(Gem *g, int level) {
  static int prev = 0;
  if (g->bus.mapper != 4) return;
  if (level && !prev) {
    if (g->bus.m4_irq_counter == 0 || g->bus.m4_irq_reload) {
      g->bus.m4_irq_counter = g->bus.m4_irq_latch;
      g->bus.m4_irq_reload = 0;
    } else {
      g->bus.m4_irq_counter--;
    }
    if (g->bus.m4_irq_counter == 0 && g->bus.m4_irq_enable) {
      g->bus.cart_irq = true;
    }
  }
  prev = level;
}

void gem_ppu_advance(Gem *g, uint32_t cpu_cycles) {
  GemPpu *p = &g->ppu;
  uint64_t old_total = p->dot / 341u;
  p->dot += (uint64_t)cpu_cycles * 3u;
  uint64_t new_total = p->dot / 341u;
  p->scanline = (int)(new_total % 262u);
  p->frame = (int)(new_total / 262u);
  for (uint64_t t = old_total + 1; t <= new_total; t++) {
    int line = (int)(t % 262u);
    if (line == 241 && p->dot > t * 341u) {
      p->status |= 0x80;
      g->stats.ppu_frames++;
      if (p->ctrl & 0x80) p->nmi_pending = true;
    } else if (line == 261) {
      p->status &= (uint8_t)~0x80;
    }
  }
}
void gem_apu_advance(Gem *g, uint32_t cpu_cycles) {
  GemApu *a = &g->apu;
  a->cycles += cpu_cycles;
  a->frame_div += cpu_cycles;
  /* 240 Hz frame sequencer approximation: 7457 CPU cycles per step. */
  while (a->frame_div >= 7457u) {
    a->frame_div -= 7457u;
    a->frame_step = (uint8_t)((a->frame_step + 1) & 3);
    if (a->frame_step == 3 && !a->frame_inhibit) a->irq = true;
  }
}

/* ---------- Lifecycle ---------- */
Gem *gem_create(void) {
  Gem *g = (Gem *)calloc(1, sizeof(Gem));
  if (!g) return NULL;
  gem_event_init(&g->events);
  gem_reset(g);
  return g;
}
void gem_destroy(Gem *g) {
  if (!g) return;
  gem_event_destroy(&g->events);
  free(g);
}
void gem_reset(Gem *g) {
  memset(&g->cpu, 0, sizeof(g->cpu));
  memset(&g->ppu, 0, sizeof(g->ppu));
  memset(&g->apu, 0, sizeof(g->apu));
  memset(&g->stats, 0, sizeof(g->stats));
  g->cpu.sp = 0xFD;
  g->cpu.p = F_U | F_I;
  g->now = 0;
  g->ppu.scanline = 0;
  g->ppu.dot = 0;
  g->cpu.pc = read16(g, 0xFFFC);
  g->cpu.stop_reason = "running";
}
int gem_load_prg(Gem *g, const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "gem6502: %s: %s\n", path, strerror(errno)); return -1; }
  size_t n = fread(g->bus.prg, 1, GEM_PRG_SIZE, f);
  fclose(f);
  if (n == 0) { fprintf(stderr, "gem6502: empty PRG\n"); return -1; }
  gem_reset(g);
  return 0;
}
int gem_step(Gem *g) {
  int c = cpu_step(g);
  if (c > 0) {
    g->now += (uint64_t)c;
    gem_ppu_advance(g, (uint32_t)c);
    gem_apu_advance(g, (uint32_t)c);
    if (g->trace_state) {
      fprintf(g->trace_state, "%04x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %llu\n",
              g->cpu.pc, g->cpu.a, g->cpu.x, g->cpu.y, g->cpu.sp, g->cpu.p,
              g->bus.ram[0x00], g->bus.ram[0x0A], g->bus.ram[0x37],
              g->bus.ram[0x38], g->bus.ram[0x39], g->bus.ram[0x47],
              (unsigned long long)g->cpu.cycles);
    }
  }
  return c;
}
int gem_run(Gem *g, uint64_t max_cycles) {
  uint64_t start = g->cpu.cycles;
  while (!g->cpu.halted && g->cpu.cycles - start < max_cycles) {
    gem_step(g);
  }
  if (!g->cpu.halted) g->cpu.stop_reason = "cycle limit";
  return g->cpu.halted ? 1 : 0;
}
void gem_print_stats(const Gem *g) {
  printf("GEM6502 %s\n", GEM6502_VERSION);
  printf("pc=%04X a=%02X x=%02X y=%02X sp=%02X p=%02X\n",
         g->cpu.pc, g->cpu.a, g->cpu.x, g->cpu.y, g->cpu.sp, g->cpu.p);
  printf("cycles=%llu inst=%llu reads=%llu writes=%llu frames=%llu nmi=%llu illegal=%llu\n",
         (unsigned long long)g->stats.cpu_cycles,
         (unsigned long long)g->stats.instructions,
         (unsigned long long)g->stats.bus_reads,
         (unsigned long long)g->stats.bus_writes,
         (unsigned long long)g->stats.ppu_frames,
         (unsigned long long)g->stats.nmi_count,
         (unsigned long long)g->stats.illegal_opcodes);
  printf("stop=%s\n", g->cpu.stop_reason ? g->cpu.stop_reason : "?");
}
