#include "nes/ref/nes_ref.h"
#include "nes/ref/m6502_ref.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  uint16_t pc;
  uint16_t snpc;
  uint16_t dnpc;
  struct {
    uint8_t len;
    uint8_t inst[8];
  } isa;
} Decode;

static nes_ref_machine *g_machine;

static inline uint32_t vaddr_ifetch(uint32_t addr, int len) {
  (void)len;
  return nes_ref_machine_read(g_machine, (uint16_t)addr);
}
static inline uint32_t vaddr_read(uint32_t addr, int len) {
  (void)len;
  return nes_ref_machine_read(g_machine, (uint16_t)addr);
}
static inline void vaddr_write(uint32_t addr, int len, uint32_t value) {
  (void)len;
  nes_ref_machine_write(g_machine, (uint16_t)addr, (uint8_t)value);
}

#define cpu (g_machine->state.cpu)
#define NEMUTRAP(pc, ret) do { (void)(pc); (void)(ret); cpu.jammed = true; } while (0)
#define INV(pc) do { (void)(pc); cpu.jammed = true; } while (0)

#define ADDR_MASK 0xFFFFu

typedef struct {
  uint16_t addr;
  uint16_t target;
  bool page_cross;
} Operand;

static inline uint8_t fetch8(Decode *s) {
  uint8_t b = (uint8_t)vaddr_ifetch(s->snpc & ADDR_MASK, 1);
  s->snpc = (s->snpc + 1) & ADDR_MASK;
  if (s->isa.len < sizeof(s->isa.inst)) {
    s->isa.inst[s->isa.len++] = b;
  }
  return b;
}

static inline uint8_t read8(uint16_t addr) {
  return (uint8_t)vaddr_read(addr, 1);
}

static inline void write8(uint16_t addr, uint8_t value) {
  vaddr_write(addr, 1, value);
}

static inline void set_flag(uint8_t flag, bool on) {
  if (on) cpu.p |= flag;
  else cpu.p &= ~flag;
}

static inline bool get_flag(uint8_t flag) {
  return (cpu.p & flag) != 0;
}

static inline void set_nz(uint8_t value) {
  cpu.p = (cpu.p & ~(M6502_N | M6502_Z)) |
          (value == 0 ? M6502_Z : 0) |
          (value & M6502_N);
}

static inline void push8(uint8_t value) {
  write8(0x0100 | cpu.sp, value);
  cpu.sp--;
}

static inline uint8_t pull8(void) {
  cpu.sp++;
  return read8(0x0100 | cpu.sp);
}

/* Push an interrupt frame.  B is set for BRK and clear for hardware IRQ/NMI. */
static void interrupt_frame(uint16_t return_pc, bool is_brk) {
  push8((uint8_t)(return_pc >> 8));
  push8((uint8_t)(return_pc & 0xFF));
  push8(cpu.p | M6502_U | (is_brk ? M6502_B : 0));
  cpu.p |= M6502_I | M6502_U;
}

static uint16_t read_vector(uint16_t addr) {
  return read8(addr) | ((uint16_t)read8((uint16_t)(addr + 1)) << 8);
}

static bool take_interrupt(Decode *s) {
  uint16_t vector;
  if (cpu.nmi_pending) {
    cpu.nmi_pending = false;
    vector = 0xFFFA;
  } else if (cpu.irq_pending && !get_flag(M6502_I)) {
    cpu.irq_pending = false;
    vector = 0xFFFE;
  } else {
    return false;
  }

  interrupt_frame((uint16_t)s->pc, false);
  s->snpc = s->pc;
  s->dnpc = read_vector(vector);
  s->isa.len = 0;
  cpu.cycles += 7;
  return true;
}

static void decode_operand(Decode *s, m6502_addrmode mode, Operand *op) {
  uint16_t base = (uint16_t)s->snpc;
  uint16_t ptr;
  uint8_t zp;
  op->addr = 0;
  op->target = 0;
  op->page_cross = false;

  switch (mode) {
    case AM_IMP:
    case AM_ACC:
      break;
    case AM_IMM:
      op->addr = (uint16_t)s->snpc;
      fetch8(s);
      break;
    case AM_ZP:
      op->addr = fetch8(s);
      break;
    case AM_ZPX:
      op->addr = (uint16_t)((fetch8(s) + cpu.x) & 0xFF);
      break;
    case AM_ZPY:
      op->addr = (uint16_t)((fetch8(s) + cpu.y) & 0xFF);
      break;
    case AM_ABS: {
      uint8_t lo = fetch8(s);
      uint8_t hi = fetch8(s);
      op->addr = (uint16_t)(lo | (hi << 8));
      break;
    }
    case AM_ABSX: {
      uint8_t lo = fetch8(s);
      uint8_t hi = fetch8(s);
      uint16_t raw = (uint16_t)(lo | (hi << 8));
      op->addr = (uint16_t)(raw + cpu.x);
      op->page_cross = (raw & 0xFF00) != (op->addr & 0xFF00);
      break;
    }
    case AM_ABSY: {
      uint8_t lo = fetch8(s);
      uint8_t hi = fetch8(s);
      uint16_t raw = (uint16_t)(lo | (hi << 8));
      op->addr = (uint16_t)(raw + cpu.y);
      op->page_cross = (raw & 0xFF00) != (op->addr & 0xFF00);
      break;
    }
    case AM_INDX:
      zp = fetch8(s);
      ptr = (uint16_t)((zp + cpu.x) & 0xFF);
      op->addr = (uint16_t)(read8(ptr) | (read8((uint16_t)((ptr + 1) & 0xFF)) << 8));
      break;
    case AM_INDY:
      zp = fetch8(s);
      base = (uint16_t)(read8(zp) | (read8((uint16_t)((zp + 1) & 0xFF)) << 8));
      op->addr = (uint16_t)(base + cpu.y);
      op->page_cross = (base & 0xFF00) != (op->addr & 0xFF00);
      break;
    case AM_IND: {
      uint8_t lo = fetch8(s);
      uint8_t hi = fetch8(s);
      ptr = (uint16_t)(lo | (hi << 8));
      uint16_t hi_addr = (uint16_t)(ptr + 1);
      if (!cpu.is_65c02) {
        /* The NMOS 6502 wraps the high byte access within the pointer's page. */
        hi_addr = (uint16_t)((ptr & 0xFF00) | ((ptr + 1) & 0xFF));
      }
      op->addr = (uint16_t)(read8(ptr) | (read8(hi_addr) << 8));
      break;
    }
    case AM_ZPIND:
      zp = fetch8(s);
      op->addr = (uint16_t)(read8(zp) | (read8((uint16_t)((zp + 1) & 0xFF)) << 8));
      break;
    case AM_ABSXIND: {
      uint8_t lo = fetch8(s);
      uint8_t hi = fetch8(s);
      ptr = (uint16_t)((lo | (hi << 8)) + cpu.x);
      op->addr = (uint16_t)(read8(ptr) | (read8((uint16_t)(ptr + 1)) << 8));
      break;
    }
    case AM_ZPREL: {
      op->addr = fetch8(s);
      int8_t offset = (int8_t)fetch8(s);
      op->target = (uint16_t)(s->snpc + offset);
      op->page_cross = ((uint16_t)s->snpc & 0xFF00) != (op->target & 0xFF00);
      break;
    }
    case AM_REL: {
      int8_t offset = (int8_t)fetch8(s);
      op->addr = (uint16_t)(s->snpc + offset);
      op->page_cross = ((uint16_t)s->snpc & 0xFF00) != (op->addr & 0xFF00);
      break;
    }
    case AM_ILL:
    default:
      break;
  }
  (void)base;
}

static void do_adc(uint8_t value) {
  uint8_t a = cpu.a;
  uint8_t carry = get_flag(M6502_C) ? 1 : 0;
  uint16_t binary_sum = (uint16_t)a + value + carry;
  uint8_t result;

  if (get_flag(M6502_D)) {
    uint16_t lo = (a & 0x0F) + (value & 0x0F) + carry;
    uint16_t hi = (a >> 4) + (value >> 4);
    if (lo > 9) { lo += 6; hi++; }
    if (hi > 9) hi += 6;
    result = (uint8_t)((hi << 4) | (lo & 0x0F));
    set_flag(M6502_C, hi > 0x0F);
  } else {
    result = (uint8_t)binary_sum;
    set_flag(M6502_C, binary_sum > 0xFF);
  }

  set_flag(M6502_V, (~(a ^ value) & (a ^ (uint8_t)binary_sum) & 0x80) != 0);
  cpu.a = result;
  set_nz(result);
}

static void do_sbc(uint8_t value) {
  uint8_t a = cpu.a;
  uint8_t borrow = get_flag(M6502_C) ? 0 : 1;
  int binary_diff = (int)a - value - borrow;
  uint8_t result;

  if (get_flag(M6502_D)) {
    int lo = (a & 0x0F) - (value & 0x0F) - borrow;
    int hi = (a >> 4) - (value >> 4);
    if (lo < 0) {
      lo -= 6;
      hi--;
    }
    if (hi < 0) hi -= 6;
    result = (uint8_t)((hi << 4) | (lo & 0x0F));
    set_flag(M6502_C, hi >= 0);
  } else {
    result = (uint8_t)binary_diff;
    set_flag(M6502_C, binary_diff >= 0);
  }

  set_flag(M6502_V, ((a ^ value) & (a ^ result) & 0x80) != 0);
  cpu.a = result;
  set_nz(result);
}

static void compare(uint8_t reg, uint8_t value) {
  uint16_t diff = (uint16_t)reg - value;
  uint8_t result = (uint8_t)diff;
  set_flag(M6502_C, reg >= value);
  set_nz(result);
}

static bool is_store(int op) {
  return op == OP_STA || op == OP_STX || op == OP_STY || op == OP_STZ;
}

static bool is_branch(int op) {
  switch (op) {
    case OP_BCC: case OP_BCS: case OP_BEQ: case OP_BNE:
    case OP_BMI: case OP_BPL: case OP_BVS: case OP_BVC:
    case OP_BRA: case OP_BBR: case OP_BBS:
      return true;
    default:
      return false;
  }
}

static int decode_exec(Decode *s) {
  const m6502_opcode_info *info;
  Operand op;
  uint8_t opcode;
  uint8_t value;
  uint8_t result;
  int cycles;

  s->isa.len = 0;
  opcode = fetch8(s);
  info = &nes_ref_m6502_opcode_table[opcode];
  decode_operand(s, (m6502_addrmode)info->mode, &op);
  s->dnpc = s->snpc;
  if (!cpu.is_65c02 && nes_ref_m6502_is_65c02(opcode)) {
    if (nes_ref_m6502_is_legacy_nop(opcode)) {
      cpu.cycles += info->cycles;
      return 0;
    }
    INV(s->pc);
    return 0;
  }
  if (info->op == OP_ILL) {
    INV(s->pc);
    return 0;
  }
  if (info->op == OP_TRAP) {
    NEMUTRAP(s->pc, cpu.a);
    return 0;
  }

  cycles = info->cycles;
  if (op.page_cross && !is_store(info->op) && !is_branch(info->op)) cycles++;

  switch (info->op) {
    case OP_LDA: cpu.a = read8(op.addr); set_nz(cpu.a); break;
    case OP_LDX: cpu.x = read8(op.addr); set_nz(cpu.x); break;
    case OP_LDY: cpu.y = read8(op.addr); set_nz(cpu.y); break;
    case OP_STA: write8(op.addr, cpu.a); break;
    case OP_STX: write8(op.addr, cpu.x); break;
    case OP_STY: write8(op.addr, cpu.y); break;

    case OP_TAX: cpu.x = cpu.a; set_nz(cpu.x); break;
    case OP_TAY: cpu.y = cpu.a; set_nz(cpu.y); break;
    case OP_TXA: cpu.a = cpu.x; set_nz(cpu.a); break;
    case OP_TYA: cpu.a = cpu.y; set_nz(cpu.a); break;
    case OP_TSX: cpu.x = cpu.sp; set_nz(cpu.x); break;
    case OP_TXS: cpu.sp = cpu.x; break;

    case OP_PHA: push8(cpu.a); break;
    case OP_PHP: push8(cpu.p | M6502_B | M6502_U); break;
    case OP_PLA: cpu.a = pull8(); set_nz(cpu.a); break;
    case OP_PLP: cpu.p = pull8() | M6502_U; break;

    case OP_AND: cpu.a &= read8(op.addr); set_nz(cpu.a); break;
    case OP_ORA: cpu.a |= read8(op.addr); set_nz(cpu.a); break;
    case OP_EOR: cpu.a ^= read8(op.addr); set_nz(cpu.a); break;
    case OP_BIT:
      value = read8(op.addr);
      set_flag(M6502_Z, (cpu.a & value) == 0);
      set_flag(M6502_N, (value & 0x80) != 0);
      set_flag(M6502_V, (value & 0x40) != 0);
      break;

    case OP_ADC: do_adc(read8(op.addr)); break;
    case OP_SBC: do_sbc(read8(op.addr)); break;

    case OP_CMP: compare(cpu.a, read8(op.addr)); break;
    case OP_CPX: compare(cpu.x, read8(op.addr)); break;
    case OP_CPY: compare(cpu.y, read8(op.addr)); break;

    case OP_ASL:
      value = (info->mode == AM_ACC) ? cpu.a : read8(op.addr);
      set_flag(M6502_C, (value & 0x80) != 0);
      result = (uint8_t)(value << 1);
      if (info->mode == AM_ACC) cpu.a = result; else write8(op.addr, result);
      set_nz(result);
      break;
    case OP_LSR:
      value = (info->mode == AM_ACC) ? cpu.a : read8(op.addr);
      set_flag(M6502_C, (value & 0x01) != 0);
      result = (uint8_t)(value >> 1);
      if (info->mode == AM_ACC) cpu.a = result; else write8(op.addr, result);
      set_nz(result);
      break;
    case OP_ROL:
      value = (info->mode == AM_ACC) ? cpu.a : read8(op.addr);
      result = (uint8_t)((value << 1) | (get_flag(M6502_C) ? 1 : 0));
      set_flag(M6502_C, (value & 0x80) != 0);
      if (info->mode == AM_ACC) cpu.a = result; else write8(op.addr, result);
      set_nz(result);
      break;
    case OP_ROR:
      value = (info->mode == AM_ACC) ? cpu.a : read8(op.addr);
      result = (uint8_t)((value >> 1) | (get_flag(M6502_C) ? 0x80 : 0));
      set_flag(M6502_C, (value & 0x01) != 0);
      if (info->mode == AM_ACC) cpu.a = result; else write8(op.addr, result);
      set_nz(result);
      break;

    case OP_INC:
      if (info->mode == AM_ACC) {
        cpu.a++;
        set_nz(cpu.a);
      } else {
        result = (uint8_t)(read8(op.addr) + 1);
        write8(op.addr, result);
        set_nz(result);
      }
      break;
    case OP_DEC:
      if (info->mode == AM_ACC) {
        cpu.a--;
        set_nz(cpu.a);
      } else {
        result = (uint8_t)(read8(op.addr) - 1);
        write8(op.addr, result);
        set_nz(result);
      }
      break;
    case OP_INX: cpu.x++; set_nz(cpu.x); break;
    case OP_INY: cpu.y++; set_nz(cpu.y); break;
    case OP_DEX: cpu.x--; set_nz(cpu.x); break;
    case OP_DEY: cpu.y--; set_nz(cpu.y); break;

    case OP_JMP: s->dnpc = op.addr; break;
    case OP_JSR: {
      uint16_t ret = (uint16_t)(s->snpc - 1);
      push8((uint8_t)(ret >> 8));
      push8((uint8_t)(ret & 0xFF));
      s->dnpc = op.addr;
      break;
    }
    case OP_RTS: {
      uint8_t lo = pull8();
      uint8_t hi = pull8();
      s->dnpc = (uint16_t)((lo | (hi << 8)) + 1);
      break;
    }
    case OP_RTI: {
      cpu.p = pull8() | M6502_U;
      uint8_t lo = pull8();
      uint8_t hi = pull8();
      s->dnpc = (uint16_t)(lo | (hi << 8));
      break;
    }

    case OP_BCC: case OP_BCS: case OP_BEQ: case OP_BNE:
    case OP_BMI: case OP_BPL: case OP_BVS: case OP_BVC:
    case OP_BRA: {
      bool take = false;
      switch (info->op) {
        case OP_BCC: take = !get_flag(M6502_C); break;
        case OP_BCS: take = get_flag(M6502_C); break;
        case OP_BEQ: take = get_flag(M6502_Z); break;
        case OP_BNE: take = !get_flag(M6502_Z); break;
        case OP_BMI: take = get_flag(M6502_N); break;
        case OP_BPL: take = !get_flag(M6502_N); break;
        case OP_BVS: take = get_flag(M6502_V); break;
        case OP_BVC: take = !get_flag(M6502_V); break;
        case OP_BRA: take = true; break;
        default: break;
      }
      if (take) {
        s->dnpc = op.addr;
        cycles += 1 + (op.page_cross ? 1 : 0);
      }
      break;
    }

    case OP_CLC: set_flag(M6502_C, false); break;
    case OP_SEC: set_flag(M6502_C, true); break;
    case OP_CLI: set_flag(M6502_I, false); break;
    case OP_SEI: set_flag(M6502_I, true); break;
    case OP_CLV: set_flag(M6502_V, false); break;
    case OP_CLD: set_flag(M6502_D, false); break;
    case OP_SED: set_flag(M6502_D, true); break;
    case OP_NOP: break;

    case OP_PHX: push8(cpu.x); break;
    case OP_PHY: push8(cpu.y); break;
    case OP_PLX: cpu.x = pull8(); set_nz(cpu.x); break;
    case OP_PLY: cpu.y = pull8(); set_nz(cpu.y); break;
    case OP_STZ: write8(op.addr, 0); break;
    case OP_TSB:
      value = read8(op.addr);
      set_flag(M6502_Z, (cpu.a & value) == 0);
      write8(op.addr, value | cpu.a);
      break;
    case OP_TRB:
      value = read8(op.addr);
      set_flag(M6502_Z, (cpu.a & value) == 0);
      write8(op.addr, value & (uint8_t)~cpu.a);
      break;
    case OP_RMB:
      value = read8(op.addr);
      value &= (uint8_t)~(1u << ((opcode >> 4) & 7));
      write8(op.addr, value);
      break;
    case OP_SMB:
      value = read8(op.addr);
      value |= (uint8_t)(1u << ((opcode >> 4) & 7));
      write8(op.addr, value);
      break;
    case OP_BBR:
    case OP_BBS: {
      bool bit_set = (read8(op.addr) & (1u << ((opcode >> 4) & 7))) != 0;
      bool take = (info->op == OP_BBS) ? bit_set : !bit_set;
      if (take) {
        s->dnpc = op.target;
        cycles += 1 + (op.page_cross ? 1 : 0);
      }
      break;
    }
    case OP_WAI:
    case OP_STP:
      NEMUTRAP(s->pc, 0);
      break;

    case OP_BRK:
      /* The NMOS 6502 skips the byte following BRK when forming the return PC. */
      interrupt_frame((uint16_t)(s->snpc + 1), true);
      s->dnpc = read_vector(0xFFFE);
      break;

    default:
      INV(s->pc);
      return 0;
  }

  cpu.cycles += cycles;
  return 0;
}

static int ref_isa_exec_once(Decode *s) {
  s->snpc = s->pc & ADDR_MASK;
  s->dnpc = s->snpc;
  s->isa.len = 0;
  if (take_interrupt(s)) return 0;
  return decode_exec(s);
}


#undef cpu

int nes_ref_cpu_step(nes_ref_machine *m) {
  if (m == NULL) return NES_REF_ERR_INVALID;
  if (m->state.cpu.jammed) return NES_REF_ERR_INVALID;
  m->state.cpu.irq_pending = m->mapper.irq;
  g_machine = m;
  Decode s;
  memset(&s, 0, sizeof(s));
  s.pc = m->state.cpu.pc;
  ref_isa_exec_once(&s);
  m->state.cpu.pc = s.dnpc;
  return NES_REF_OK;
}
