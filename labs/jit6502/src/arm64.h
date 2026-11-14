/*
 * Tiny AArch64 encoder + emitter.
 *
 * Only the encodings the 6502 translator actually emits are implemented; each
 * one is commented with the instruction it produces so the generated code can
 * be checked against `otool -tV` when debugging.
 *
 * Emitter holds a growable byte buffer plus a fixup list, so translation can
 * branch forward to labels that are only known later (block epilogue, taken /
 * not-taken paths).
 */
#ifndef J6502_ARM64_H
#define J6502_ARM64_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
  A64_EQ = 0, A64_NE = 1, A64_CS = 2, A64_CC = 3,
  A64_MI = 4, A64_PL = 5, A64_VS = 6, A64_VC = 7,
  A64_HI = 8, A64_LS = 9, A64_GE = 10, A64_LT = 11,
  A64_GT = 12, A64_LE = 13, A64_AL = 14
};

/* General purpose registers, for readability at the call sites. */
enum {
  R0 = 0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15,
  R16, R17, R18, R19, R20, R21, R22, R23, R24, R25, R26, R27, R28, R29, R30,
  R31 = 31 /* XZR / SP depending on context */
};

/* ---- instruction encoders ---------------------------------------------- */

/* movz {w|x}Rd, #imm, lsl #(hw*16) */
static inline uint32_t a64_movz(bool sf, uint32_t rd, uint16_t imm, int hw) {
  return (sf ? 0xD2800000u : 0x52800000u) | ((uint32_t)hw << 21) |
         ((uint32_t)imm << 5) | rd;
}

/* movk {w|x}Rd, #imm, lsl #(hw*16) */
static inline uint32_t a64_movk(bool sf, uint32_t rd, uint16_t imm, int hw) {
  return (sf ? 0xF2800000u : 0x72800000u) | ((uint32_t)hw << 21) |
         ((uint32_t)imm << 5) | rd;
}

/* add/sub {w|x}Rd, {w|x}Rn, #imm12 */
static inline uint32_t a64_addsub_imm(bool sf, uint32_t rd, uint32_t rn,
                                      uint32_t imm12, bool sub) {
  uint32_t base = sf ? (sub ? 0xD1000000u : 0x91000000u)
                     : (sub ? 0x51000000u : 0x11000000u);
  return base | ((imm12 & 0xFFFu) << 10) | (rn << 5) | rd;
}

/* subs {w|x}Rn, {w|x}Rm, #imm12 -> cmp Rn, #imm (Rd = XZR) */
static inline uint32_t a64_cmp_imm(bool sf, uint32_t rn, uint32_t imm12) {
  return (sf ? 0xF1000000u : 0x71000000u) | ((imm12 & 0xFFFu) << 10) |
         (rn << 5) | 31u;
}

/* add/sub {w|x}Rd, {w|x}Rn, {w|x}Rm, {lsl|lsr|asr} #shift */
static inline uint32_t a64_addsub_reg(bool sf, uint32_t rd, uint32_t rn,
                                      uint32_t rm, int shift, int shamt,
                                      bool sub) {
  uint32_t base = sf ? (sub ? 0xCB000000u : 0x8B000000u)
                     : (sub ? 0x4B000000u : 0x0B000000u);
  return base | ((uint32_t)shift << 22) | ((uint32_t)shamt << 10) |
         (rm << 16) | (rn << 5) | rd;
}

/* subs {w|x}Rd, {w|x}Rn, {w|x}Rm, shift #shamt */
static inline uint32_t a64_subs_reg(bool sf, uint32_t rd, uint32_t rn,
                                    uint32_t rm, int shift, int shamt) {
  uint32_t base = sf ? 0xEB000000u : 0x6B000000u;
  return base | ((uint32_t)shift << 22) | ((uint32_t)shamt << 10) |
         (rm << 16) | (rn << 5) | rd;
}

/* logical {w|x}Rd, {w|x}Rn, {w|x}Rm, shift #shamt */
enum { A64_LSL = 0, A64_LSR = 1, A64_ASR = 2 };
static inline uint32_t a64_logic_reg(bool sf, uint32_t op, uint32_t rd,
                                     uint32_t rn, uint32_t rm, int shift,
                                     int shamt) {
  uint32_t base = sf ? (op == 0 ? 0x8A000000u : op == 1 ? 0xAA000000u : 0xCA000000u)
                     : (op == 0 ? 0x0A000000u : op == 1 ? 0x2A000000u : 0x4A000000u);
  return base | ((uint32_t)shift << 22) | ((uint32_t)shamt << 10) |
         (rm << 16) | (rn << 5) | rd;
}

/* ubfm: lsl/lsr immediate and uxtb/uxtb-style masks. */
static inline uint32_t a64_ubfm(bool sf, uint32_t rd, uint32_t rn,
                                uint32_t immr, uint32_t imms) {
  return (sf ? 0xD3400000u : 0x53000000u) | (immr << 16) | (imms << 10) |
         (rn << 5) | rd;
}

static inline uint32_t a64_lsl_imm(bool sf, uint32_t rd, uint32_t rn, int sh) {
  int bits = sf ? 64 : 32;
  return a64_ubfm(sf, rd, rn, (uint32_t)((bits - sh) & (bits - 1)),
                  (uint32_t)(bits - 1 - sh));
}

static inline uint32_t a64_lsr_imm(bool sf, uint32_t rd, uint32_t rn, int sh) {
  int bits = sf ? 64 : 32;
  return a64_ubfm(sf, rd, rn, (uint32_t)sh, (uint32_t)(bits - 1));
}

static inline uint32_t a64_uxtb(uint32_t rd, uint32_t rn) {
  return a64_ubfm(false, rd, rn, 0, 7);
}

/* mov {w|x}Rd, {w|x}Rm  (orr Rd, wzr, Rm) */
static inline uint32_t a64_mov_reg(bool sf, uint32_t rd, uint32_t rm) {
  return (sf ? 0xAA0003E0u : 0x2A0003E0u) | (rm << 16) | rd;
}

/* ldrb/strb with 12-bit unsigned offset (0..4095) */
static inline uint32_t a64_ldrb_off(uint32_t rt, uint32_t rn, uint32_t off) {
  return 0x39400000u | ((off & 0xFFFu) << 10) | (rn << 5) | rt;
}
static inline uint32_t a64_strb_off(uint32_t rt, uint32_t rn, uint32_t off) {
  return 0x39000000u | ((off & 0xFFFu) << 10) | (rn << 5) | rt;
}
/* ldrb/strb, [Xn, Xm] (64-bit zero-extended register offset) */
static inline uint32_t a64_ldrb_reg(uint32_t rt, uint32_t rn, uint32_t rm) {
  return 0x38606800u | (rm << 16) | (rn << 5) | rt;
}
static inline uint32_t a64_strb_reg(uint32_t rt, uint32_t rn, uint32_t rm) {
  return 0x38206800u | (rm << 16) | (rn << 5) | rt;
}

/* halfword/word accesses scale the 12-bit offset by the access size */
static inline uint32_t a64_ldrh_off(uint32_t rt, uint32_t rn, uint32_t off) {
  return 0x79400000u | (((off / 2u) & 0xFFFu) << 10) | (rn << 5) | rt;
}
static inline uint32_t a64_strh_off(uint32_t rt, uint32_t rn, uint32_t off) {
  return 0x79000000u | (((off / 2u) & 0xFFFu) << 10) | (rn << 5) | rt;
}
static inline uint32_t a64_ldr_off(uint32_t rt, uint32_t rn, uint32_t off) {
  return 0xF9400000u | (((off / 8u) & 0xFFFu) << 10) | (rn << 5) | rt;
}
static inline uint32_t a64_str_off(uint32_t rt, uint32_t rn, uint32_t off) {
  return 0xF9000000u | (((off / 8u) & 0xFFFu) << 10) | (rn << 5) | rt;
}

/* stp/ldp 64-bit, pre/post-indexed by +/-16 (our only frame pattern). */
static inline uint32_t a64_stp_pre(uint32_t rt, uint32_t rt2, uint32_t rn,
                                   int off) {
  return 0xA9800000u | (((uint32_t)((off / 8) & 0x7F)) << 15) |
         (rt2 << 10) | (rn << 5) | rt;
}
static inline uint32_t a64_ldp_post(uint32_t rt, uint32_t rt2, uint32_t rn,
                                    int off) {
  return 0xA8C00000u | (((uint32_t)((off / 8) & 0x7F)) << 15) |
         (rt2 << 10) | (rn << 5) | rt;
}

/* Logical immediate encoding (ARM ARM "DecodeBitMasks" in reverse).  Needed
 * for masks like 0x7D, 0x7F and 0xFFFFFFFE that the 6502 flag code uses. */
static inline bool a64_encode_bitmask(uint32_t imm, uint32_t *out) {
  if (imm == 0 || imm == 0xFFFFFFFFu) return false;
  /* Prefer the largest element size, matching what assemblers emit. */
  for (int esize = 32; esize >= 2; esize >>= 1) {
    uint32_t mask = (esize == 32) ? 0xFFFFFFFFu : ((1u << esize) - 1u);
    uint32_t elem = imm & mask;
    if (elem == 0) continue;
    bool replicated = true;
    for (int b = 0; b < 32; b += esize) {
      if (((imm >> b) & mask) != elem) {
        replicated = false;
        break;
      }
    }
    if (!replicated) continue;
    uint32_t ones = (uint32_t)__builtin_popcount(elem);
    uint32_t target = (ones == 32) ? 0xFFFFFFFFu : ((1u << ones) - 1u);
    for (int r = 0; r < esize; r++) {
      uint32_t rot = (r == 0) ? elem
                              : ((elem >> r) | (elem << (esize - r)));
      rot &= mask;
      if (rot == target) {
        int len = 0;
        for (int e = esize; e > 1; e >>= 1) len++;
        uint32_t imms = ((~((1u << (len + 1)) - 1u)) & 0x3Fu) | (ones - 1u);
        /* decode does ROR(welem, immr), so our right-rotation r satisfies
         * r == esize - immr. */
        uint32_t immr = (uint32_t)((esize - r) & (esize - 1));
        *out = (immr << 16) | (imms << 10);
        return true;
      }
    }
  }
  return false;
}

/* and/orr/eor/ands Rd, Rn, #imm32  (op: 0=and 1=orr 2=eor 3=ands)
 * Returns 0 if `imm` is not a representable logical immediate; callers only
 * use masks that are known to encode. */
static inline uint32_t a64_logic_imm(uint32_t op, uint32_t rd, uint32_t rn,
                                     uint32_t imm) {
  uint32_t field = 0;
  if (!a64_encode_bitmask(imm, &field)) return 0;
  uint32_t base = (op == 0) ? 0x12000000u : (op == 1) ? 0x32000000u
                 : (op == 2) ? 0x52000000u : 0x72000000u;
  return base | field | (rn << 5) | rd;
}

/* mvn wRd, wRm  (orn Rd, wzr, Rm) */
static inline uint32_t a64_mvn(uint32_t rd, uint32_t rm) {
  return 0x2A2003E0u | (rm << 16) | rd;
}

/* bic wRd, wRn, wRm */
static inline uint32_t a64_bic_reg(uint32_t rd, uint32_t rn, uint32_t rm) {
  return 0x0A200000u | (rm << 16) | (rn << 5) | rd;
}

static inline uint32_t a64_cset(uint32_t rd, int cond) {
  return 0x1A9F07E0u | ((uint32_t)(cond ^ 1) << 12) | rd;
}

static inline uint32_t a64_ret(void) { return 0xD65F03C0u; }
static inline uint32_t a64_blr(uint32_t rn) { return 0xD63F0000u | (rn << 5); }

static inline uint32_t a64_b(int32_t off_bytes) {
  return 0x14000000u | (((uint32_t)(off_bytes >> 2)) & 0x3FFFFFFu);
}
static inline uint32_t a64_bl(int32_t off_bytes) {
  return 0x94000000u | (((uint32_t)(off_bytes >> 2)) & 0x3FFFFFFu);
}
static inline uint32_t a64_bcond(int cond, int32_t off_bytes) {
  return 0x54000000u | ((((uint32_t)(off_bytes >> 2)) & 0x7FFFFu) << 5) |
         (uint32_t)cond;
}
static inline uint32_t a64_cbz(uint32_t rt, int32_t off_bytes, bool nz) {
  return (nz ? 0x35000000u : 0x34000000u) |
         ((((uint32_t)(off_bytes >> 2)) & 0x7FFFFu) << 5) | rt;
}
static inline uint32_t a64_tbz(uint32_t rt, int bit, int32_t off_bytes,
                               bool nz) {
  uint32_t b5 = ((uint32_t)bit >> 5) & 1u;
  uint32_t b40 = (uint32_t)bit & 0x1Fu;
  return (nz ? 0x37000000u : 0x36000000u) | (b5 << 31) | (b40 << 19) |
         ((((uint32_t)(off_bytes >> 2)) & 0x3FFFu) << 5) | rt;
}

/* ---- emitter ----------------------------------------------------------- */

enum { FIX_B, FIX_BCOND, FIX_CBZ, FIX_CBNZ, FIX_TBZ, FIX_TBNZ };

typedef struct Fixup {
  uint32_t pos; /* byte offset of the instruction */
  int16_t label;
  uint8_t kind;
  uint8_t bit; /* for TBZ/TBNZ */
} Fixup;

#define MAX_LABELS 192
#define MAX_FIXUPS 1024

typedef struct Emitter {
  uint8_t *buf;
  size_t len;
  size_t cap;
  int32_t label_pos[MAX_LABELS];
  Fixup fixups[MAX_FIXUPS];
  int nfix;
  bool oom;
} Emitter;

static inline void emit_init(Emitter *e, uint8_t *buf, size_t cap) {
  memset(e, 0, sizeof(*e));
  e->buf = buf;
  e->cap = cap;
  for (int i = 0; i < MAX_LABELS; i++) e->label_pos[i] = -1;
}

static inline void emit_u32(Emitter *e, uint32_t insn) {
  if (e->oom || e->len + 4 > e->cap) {
    e->oom = true;
    return;
  }
  memcpy(e->buf + e->len, &insn, 4);
  e->len += 4;
}

static inline int32_t emit_here(const Emitter *e) { return (int32_t)e->len; }

static inline void emit_label(Emitter *e, int label) {
  if (label < 0 || label >= MAX_LABELS) return;
  e->label_pos[label] = (int32_t)e->len;
}

static inline void emit_fixup(Emitter *e, int label, uint8_t kind, uint8_t bit) {
  if (e->oom || e->nfix >= MAX_FIXUPS) {
    e->oom = true;
    return;
  }
  e->fixups[e->nfix++] = (Fixup){(uint32_t)e->len, (int16_t)label, kind, bit};
}

static inline void emit_b(Emitter *e, int label) {
  emit_fixup(e, label, FIX_B, 0);
  emit_u32(e, a64_b(0));
}
static inline void emit_bcond(Emitter *e, int cond, int label) {
  emit_fixup(e, label, FIX_BCOND, (uint8_t)cond);
  emit_u32(e, a64_bcond(cond, 0));
}
static inline void emit_cbz(Emitter *e, uint32_t rt, int label, bool nz) {
  emit_fixup(e, label, nz ? FIX_CBNZ : FIX_CBZ, 0);
  emit_u32(e, a64_cbz(rt, 0, nz));
}
static inline void emit_tbz(Emitter *e, uint32_t rt, int bit, int label,
                            bool nz) {
  emit_fixup(e, label, nz ? FIX_TBNZ : FIX_TBZ, (uint8_t)bit);
  emit_u32(e, a64_tbz(rt, bit, 0, nz));
}

/* Resolve every fixup; returns false if a label was never defined or a branch
 * displacement does not fit its immediate field. */
static inline bool emit_finish(Emitter *e) {
  if (e->oom) return false;
  for (int i = 0; i < e->nfix; i++) {
    Fixup *f = &e->fixups[i];
    if (f->label < 0 || f->label >= MAX_LABELS ||
        e->label_pos[f->label] < 0) {
      return false;
    }
    int32_t delta = e->label_pos[f->label] - (int32_t)f->pos;
    if ((delta & 3) != 0) return false;
    uint32_t imm = (uint32_t)(delta >> 2);
    uint32_t insn;
    memcpy(&insn, e->buf + f->pos, 4);
    uint32_t rt = insn & 0x1Fu;

    switch (f->kind) {
      case FIX_B:
        if (delta < -(1 << 27) || delta >= (1 << 27)) return false;
        insn = 0x14000000u | (imm & 0x3FFFFFFu);
        break;
      case FIX_BCOND:
        if (delta < -(1 << 20) || delta >= (1 << 20)) return false;
        insn = 0x54000000u | ((imm & 0x7FFFFu) << 5) | (uint32_t)f->bit;
        break;
      case FIX_CBZ:
      case FIX_CBNZ:
        if (delta < -(1 << 20) || delta >= (1 << 20)) return false;
        insn = (f->kind == FIX_CBNZ ? 0x35000000u : 0x34000000u) |
               ((imm & 0x7FFFFu) << 5) | rt;
        break;
      case FIX_TBZ:
      case FIX_TBNZ:
        if (delta < -(1 << 14) || delta >= (1 << 14)) return false;
        insn = (f->kind == FIX_TBNZ ? 0x37000000u : 0x36000000u) |
               ((((uint32_t)f->bit >> 5) & 1u) << 31) |
               (((uint32_t)f->bit & 0x1Fu) << 19) |
               ((imm & 0x3FFFu) << 5) | rt;
        break;
      default:
        return false;
    }
    memcpy(e->buf + f->pos, &insn, 4);
  }
  return true;
}

#endif /* J6502_ARM64_H */
