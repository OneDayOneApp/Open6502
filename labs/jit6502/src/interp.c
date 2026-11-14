/*
 * Reference interpreter for the lab.
 *
 * This is the "before" picture in the JIT story: every guest instruction goes
 * through fetch -> table lookup -> mode decode -> opcode switch.  The JIT will
 * be measured against exactly this loop, and used as the correctness oracle
 * (bit-identical register file after every step).
 */
#include "cpu.h"
#include "opcodes_gen.h"

#include <stdio.h>

static inline uint8_t fetch8(Cpu *c) {
  uint16_t pc = c->pc++;
  return cpu_rd(c, pc);
}

static inline uint16_t fetch16(Cpu *c) {
  uint8_t lo = fetch8(c);
  uint8_t hi = fetch8(c);
  return (uint16_t)(lo | ((uint16_t)hi << 8));
}

static inline void set_flag(Cpu *c, uint8_t flag, bool on) {
  if (on) {
    c->p |= flag;
  } else {
    c->p = (uint8_t)(c->p & ~flag);
  }
}

/* Clears N/Z and sets them from `v`.  All 8-bit results go through here. */
static inline void set_nz(Cpu *c, uint8_t v) {
  c->p = (uint8_t)((c->p & ~(F_N | F_Z)) | (v & F_N) | (v == 0 ? F_Z : 0u));
}

static inline bool flag(const Cpu *c, uint8_t f) { return (c->p & f) != 0; }

static inline void push(Cpu *c, uint8_t v) {
  cpu_wr(c, (uint16_t)(0x0100u | c->sp), v);
  c->sp = (uint8_t)(c->sp - 1);
}

static inline uint8_t pull(Cpu *c) {
  c->sp = (uint8_t)(c->sp + 1);
  return cpu_rd(c, (uint16_t)(0x0100u | c->sp));
}

static inline uint16_t read16(const Cpu *c, uint16_t addr) {
  return (uint16_t)(cpu_rd(c, addr) |
                    ((uint16_t)cpu_rd(c, (uint16_t)(addr + 1)) << 8));
}

/* Zero-page pointer reads wrap inside page zero, like the real chip. */
static inline uint16_t read16_zp(const Cpu *c, uint8_t addr) {
  return (uint16_t)(cpu_rd(c, addr) |
                    ((uint16_t)cpu_rd(c, (uint8_t)(addr + 1)) << 8));
}

typedef struct Operand {
  uint16_t addr;
  bool cross; /* ABSX/ABSY/INDY crossed a page boundary */
} Operand;

static Operand fetch_operand(Cpu *c, uint8_t mode) {
  Operand o = {0, false};
  switch (mode) {
    case AM_IMM:
      o.addr = c->pc++;
      break;
    case AM_ZP:
      o.addr = fetch8(c);
      break;
    case AM_ZPX:
      o.addr = (uint8_t)(fetch8(c) + c->x);
      break;
    case AM_ZPY:
      o.addr = (uint8_t)(fetch8(c) + c->y);
      break;
    case AM_ABS:
      o.addr = fetch16(c);
      break;
    case AM_ABSX: {
      uint16_t base = fetch16(c);
      o.addr = (uint16_t)(base + c->x);
      o.cross = (base & 0xFF00u) != (o.addr & 0xFF00u);
      break;
    }
    case AM_ABSY: {
      uint16_t base = fetch16(c);
      o.addr = (uint16_t)(base + c->y);
      o.cross = (base & 0xFF00u) != (o.addr & 0xFF00u);
      break;
    }
    case AM_INDX: {
      uint8_t zp = (uint8_t)(fetch8(c) + c->x);
      o.addr = read16_zp(c, zp);
      break;
    }
    case AM_INDY: {
      uint8_t zp = fetch8(c);
      uint16_t base = read16_zp(c, zp);
      o.addr = (uint16_t)(base + c->y);
      o.cross = (base & 0xFF00u) != (o.addr & 0xFF00u);
      break;
    }
    case AM_REL: {
      int8_t off = (int8_t)fetch8(c);
      o.addr = (uint16_t)(c->pc + (int16_t)off);
      break;
    }
    case AM_IND: {
      /* The NMOS indirect JMP has the famous page-wrap bug: the high byte is
       * fetched from the same page as the low byte. */
      uint16_t ptr = fetch16(c);
      uint16_t hi_addr = (uint16_t)((ptr & 0xFF00u) | ((ptr + 1) & 0x00FFu));
      o.addr = (uint16_t)(cpu_rd(c, ptr) | ((uint16_t)cpu_rd(c, hi_addr) << 8));
      break;
    }
    default:
      break;
  }
  return o;
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

static uint8_t adc(Cpu *c, uint8_t m) {
  unsigned addend = m;
  unsigned carry = flag(c, F_C) ? 1u : 0u;
  if (flag(c, F_D)) {
    unsigned al = (unsigned)(c->a & 0x0F) + (addend & 0x0F) + carry;
    unsigned ah = (unsigned)(c->a >> 4) + (addend >> 4);
    unsigned bin = (unsigned)c->a + addend + carry;
    if (!((c->a ^ addend) & 0x80) && ((c->a ^ bin) & 0x80)) {
      c->p |= F_V;
    } else {
      c->p = (uint8_t)(c->p & ~F_V);
    }
    if (al > 9) al += 6;
    if (al > 0x0F) ah++;
    if (ah > 9) ah += 6;
    if (ah > 0x0F) c->p |= F_C; else c->p &= (uint8_t)~F_C;
    uint8_t res = (uint8_t)(((ah & 0x0F) << 4) | (al & 0x0F));
    set_nz(c, res);
    return res;
  }
  unsigned sum = (unsigned)c->a + addend + carry;
  if (!((c->a ^ addend) & 0x80) && ((c->a ^ sum) & 0x80)) {
    c->p |= F_V;
  } else {
    c->p = (uint8_t)(c->p & ~F_V);
  }
  set_flag(c, F_C, sum > 0xFF);
  uint8_t res = (uint8_t)sum;
  set_nz(c, res);
  return res;
}

static uint8_t sbc(Cpu *c, uint8_t m) {
  unsigned borrow = flag(c, F_C) ? 1u : 0u;
  if (flag(c, F_D)) {
    unsigned diff = (unsigned)c->a - m - (1u - borrow);
    unsigned al = (unsigned)(c->a & 0x0F) - (m & 0x0F) - (1u - borrow);
    unsigned ah = (unsigned)(c->a >> 4) - (m >> 4);
    if (((c->a ^ m) & 0x80) && ((c->a ^ diff) & 0x80)) {
      c->p |= F_V;
    } else {
      c->p = (uint8_t)(c->p & ~F_V);
    }
    if (al & 0x10u) {
      al -= 6;
      ah--;
    }
    if (ah & 0x10u) ah -= 6;
    set_flag(c, F_C, diff < 0x100u);
    uint8_t res = (uint8_t)(((ah & 0x0F) << 4) | (al & 0x0F));
    set_nz(c, res);
    return res;
  }
  unsigned diff = (unsigned)c->a - m - (1u - borrow);
  if (((c->a ^ m) & 0x80) && ((c->a ^ diff) & 0x80)) {
    c->p |= F_V;
  } else {
    c->p = (uint8_t)(c->p & ~F_V);
  }
  set_flag(c, F_C, diff < 0x100u);
  uint8_t res = (uint8_t)diff;
  set_nz(c, res);
  return res;
}

/* Takes a pending NMI/IRQ at the next instruction boundary. */
static bool take_interrupt(Cpu *c) {
  uint16_t vector;
  if (c->nmi_pending) {
    c->nmi_pending = 0;
    vector = 0xFFFA;
  } else if (c->irq_pending && !flag(c, F_I)) {
    c->irq_pending = 0;
    vector = 0xFFFE;
  } else {
    return false;
  }
  push(c, (uint8_t)(c->pc >> 8));
  push(c, (uint8_t)(c->pc & 0xFF));
  push(c, (uint8_t)((c->p | F_U) & (uint8_t)~F_B));
  c->p |= F_I;
  c->pc = read16(c, vector);
  c->cycles += 7;
  return true;
}

/* One instruction.  Returns RUN_OK, or RUN_ILLEGAL for an undocumented
 * opcode (the lab only claims NMOS 6502 coverage). */
int cpu_step(Cpu *c) {
  if (take_interrupt(c)) return RUN_OK;
  uint8_t opcode = fetch8(c);
  const OpcodeInfo info = kOpcodes[opcode];
  if (info.op == OP_ILL) {
    return RUN_ILLEGAL;
  }

  Operand o = {0, false};
  if (info.mode != AM_IMP && info.mode != AM_ACC && info.mode != AM_REL) {
    o = fetch_operand(c, info.mode);
  } else if (info.mode == AM_REL) {
    o = fetch_operand(c, AM_REL);
  }

  unsigned extra = 0;
  switch (info.op) {
    case OP_LDA: c->a = cpu_rd(c, o.addr); set_nz(c, c->a); break;
    case OP_LDX: c->x = cpu_rd(c, o.addr); set_nz(c, c->x); break;
    case OP_LDY: c->y = cpu_rd(c, o.addr); set_nz(c, c->y); break;
    case OP_STA: cpu_wr(c, o.addr, c->a); break;
    case OP_STX: cpu_wr(c, o.addr, c->x); break;
    case OP_STY: cpu_wr(c, o.addr, c->y); break;

    case OP_TAX: c->x = c->a; set_nz(c, c->x); break;
    case OP_TAY: c->y = c->a; set_nz(c, c->y); break;
    case OP_TXA: c->a = c->x; set_nz(c, c->a); break;
    case OP_TYA: c->a = c->y; set_nz(c, c->a); break;
    case OP_TSX: c->x = c->sp; set_nz(c, c->x); break;
    case OP_TXS: c->sp = c->x; break;

    case OP_ADC: c->a = adc(c, cpu_rd(c, o.addr)); break;
    case OP_SBC: c->a = sbc(c, cpu_rd(c, o.addr)); break;
    case OP_AND: c->a &= cpu_rd(c, o.addr); set_nz(c, c->a); break;
    case OP_ORA: c->a |= cpu_rd(c, o.addr); set_nz(c, c->a); break;
    case OP_EOR: c->a ^= cpu_rd(c, o.addr); set_nz(c, c->a); break;

    case OP_CMP: {
      uint8_t v = cpu_rd(c, o.addr);
      uint8_t r = (uint8_t)(c->a - v);
      set_flag(c, F_C, c->a >= v);
      set_nz(c, r);
      break;
    }
    case OP_CPX: {
      uint8_t v = cpu_rd(c, o.addr);
      uint8_t r = (uint8_t)(c->x - v);
      set_flag(c, F_C, c->x >= v);
      set_nz(c, r);
      break;
    }
    case OP_CPY: {
      uint8_t v = cpu_rd(c, o.addr);
      uint8_t r = (uint8_t)(c->y - v);
      set_flag(c, F_C, c->y >= v);
      set_nz(c, r);
      break;
    }
    case OP_BIT: {
      uint8_t v = cpu_rd(c, o.addr);
      c->p = (uint8_t)((c->p & ~(F_N | F_V | F_Z)) |
                       (v & (F_N | F_V)) |
                       ((c->a & v) == 0 ? F_Z : 0u));
      break;
    }

    case OP_INC: {
      uint8_t v = (uint8_t)(cpu_rd(c, o.addr) + 1);
      cpu_wr(c, o.addr, v);
      set_nz(c, v);
      break;
    }
    case OP_DEC: {
      uint8_t v = (uint8_t)(cpu_rd(c, o.addr) - 1);
      cpu_wr(c, o.addr, v);
      set_nz(c, v);
      break;
    }
    case OP_INX: c->x++; set_nz(c, c->x); break;
    case OP_INY: c->y++; set_nz(c, c->y); break;
    case OP_DEX: c->x--; set_nz(c, c->x); break;
    case OP_DEY: c->y--; set_nz(c, c->y); break;

    case OP_ASL: {
      uint8_t v;
      if (info.mode == AM_ACC) {
        v = c->a;
        set_flag(c, F_C, (v & 0x80) != 0);
        c->a = (uint8_t)(v << 1);
        set_nz(c, c->a);
      } else {
        v = cpu_rd(c, o.addr);
        set_flag(c, F_C, (v & 0x80) != 0);
        v = (uint8_t)(v << 1);
        cpu_wr(c, o.addr, v);
        set_nz(c, v);
      }
      break;
    }
    case OP_LSR: {
      uint8_t v;
      if (info.mode == AM_ACC) {
        v = c->a;
        set_flag(c, F_C, (v & 0x01) != 0);
        c->a = (uint8_t)(v >> 1);
        set_nz(c, c->a);
      } else {
        v = cpu_rd(c, o.addr);
        set_flag(c, F_C, (v & 0x01) != 0);
        v = (uint8_t)(v >> 1);
        cpu_wr(c, o.addr, v);
        set_nz(c, v);
      }
      break;
    }
    case OP_ROL: {
      uint8_t v;
      bool carry_in = flag(c, F_C);
      if (info.mode == AM_ACC) {
        v = c->a;
        set_flag(c, F_C, (v & 0x80) != 0);
        c->a = (uint8_t)((v << 1) | (carry_in ? 1 : 0));
        set_nz(c, c->a);
      } else {
        v = cpu_rd(c, o.addr);
        set_flag(c, F_C, (v & 0x80) != 0);
        v = (uint8_t)((v << 1) | (carry_in ? 1 : 0));
        cpu_wr(c, o.addr, v);
        set_nz(c, v);
      }
      break;
    }
    case OP_ROR: {
      uint8_t v;
      bool carry_in = flag(c, F_C);
      if (info.mode == AM_ACC) {
        v = c->a;
        set_flag(c, F_C, (v & 0x01) != 0);
        c->a = (uint8_t)((v >> 1) | (carry_in ? 0x80 : 0));
        set_nz(c, c->a);
      } else {
        v = cpu_rd(c, o.addr);
        set_flag(c, F_C, (v & 0x01) != 0);
        v = (uint8_t)((v >> 1) | (carry_in ? 0x80 : 0));
        cpu_wr(c, o.addr, v);
        set_nz(c, v);
      }
      break;
    }

    case OP_JMP: c->pc = o.addr; break;
    case OP_JSR: {
      uint16_t ret = (uint16_t)(c->pc - 1);
      push(c, (uint8_t)(ret >> 8));
      push(c, (uint8_t)(ret & 0xFF));
      c->pc = o.addr;
      break;
    }
    case OP_RTS: {
      uint8_t lo = pull(c);
      uint8_t hi = pull(c);
      c->pc = (uint16_t)(((uint16_t)(hi << 8) | lo) + 1);
      break;
    }
    case OP_RTI: {
      c->p = (uint8_t)(pull(c) | F_U);
      uint8_t lo = pull(c);
      uint8_t hi = pull(c);
      c->pc = (uint16_t)(lo | ((uint16_t)hi << 8));
      break;
    }
    case OP_BRK: {
      uint16_t ret = (uint16_t)(c->pc + 1); /* skip the padding byte */
      push(c, (uint8_t)(ret >> 8));
      push(c, (uint8_t)(ret & 0xFF));
      push(c, (uint8_t)(c->p | F_B | F_U));
      c->p |= F_I;
      c->pc = read16(c, 0xFFFE);
      break;
    }

    case OP_PHA: push(c, c->a); break;
    case OP_PLA: c->a = pull(c); set_nz(c, c->a); break;
    case OP_PHP: push(c, (uint8_t)(c->p | F_B | F_U)); break;
    case OP_PLP: c->p = (uint8_t)(pull(c) | F_U); break;

    case OP_BCC: case OP_BCS: case OP_BEQ: case OP_BNE:
    case OP_BMI: case OP_BPL: case OP_BVS: case OP_BVC: {
      bool take = false;
      switch (info.op) {
        case OP_BCC: take = !flag(c, F_C); break;
        case OP_BCS: take = flag(c, F_C); break;
        case OP_BEQ: take = flag(c, F_Z); break;
        case OP_BNE: take = !flag(c, F_Z); break;
        case OP_BMI: take = flag(c, F_N); break;
        case OP_BPL: take = !flag(c, F_N); break;
        case OP_BVS: take = flag(c, F_V); break;
        case OP_BVC: take = !flag(c, F_V); break;
        default: break;
      }
      if (take) {
        /* fetch_operand already resolved the target; a taken branch costs one
         * extra cycle, and one more when it crosses a page. */
        extra += 1;
        if ((o.addr & 0xFF00u) != (c->pc & 0xFF00u)) extra += 1;
        c->pc = o.addr;
      }
      break;
    }

    case OP_CLC: set_flag(c, F_C, false); break;
    case OP_SEC: set_flag(c, F_C, true); break;
    case OP_CLI: set_flag(c, F_I, false); break;
    case OP_SEI: set_flag(c, F_I, true); break;
    case OP_CLV: set_flag(c, F_V, false); break;
    case OP_CLD: set_flag(c, F_D, false); break;
    case OP_SED: set_flag(c, F_D, true); break;

    case OP_NOP: break;

    default:
      return RUN_ILLEGAL;
  }

  /* Read instructions pay one extra cycle when ABSX/ABSY/INDY crosses a page. */
  if (o.cross && op_reads_memory(info.op) &&
      (info.mode == AM_ABSX || info.mode == AM_ABSY || info.mode == AM_INDY)) {
    extra += 1;
  }

  c->cycles += info.cycles + extra;
  return RUN_OK;
}
