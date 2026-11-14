/*
 * In-process differential tester.
 *
 * For every opcode and a stream of pseudo-random machine states, execute the
 * instruction once through the interpreter and once through the JIT, then
 * compare the full architectural state *and* the 64K memory image.  This is
 * far more effective at finding codegen bugs than staring at game traces.
 */
#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jit.h"
#include "opcodes_gen.h"

static const char *kOpcodesName(uint8_t op) { return kOpcodes[op].name; }

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;

static uint32_t rnd(void) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 7;
  rng_state ^= rng_state << 17;
  return (uint32_t)(rng_state >> 16);
}

static void fill_random(uint8_t *mem) {
  for (size_t i = 0; i < MEM_SIZE; i++) mem[i] = (uint8_t)rnd();
}

typedef struct Snapshot {
  uint8_t a, x, y, sp, p;
  uint16_t pc;
  uint64_t cycles;
} Snapshot;

static Snapshot snap(const Cpu *c) {
  Snapshot s = {c->a, c->x, c->y, c->sp, c->p, c->pc, c->cycles};
  return s;
}

static bool same(const Snapshot *a, const Snapshot *b) {
  return a->a == b->a && a->x == b->x && a->y == b->y && a->sp == b->sp &&
         a->p == b->p && a->pc == b->pc && a->cycles == b->cycles;
}

/* One instruction, identical memory image on both sides. */
static int check_instruction(uint8_t opcode, uint8_t b1, uint8_t b2,
                             Cpu *ci, Cpu *cj, uint8_t *mi, uint8_t *mj) {
  memcpy(mj, mi, MEM_SIZE);
  mi[0x0400] = opcode;
  mi[0x0401] = b1;
  mi[0x0402] = b2;
  memcpy(mj, mi, MEM_SIZE);

  Cpu a = *ci;
  Cpu b = *cj;
  a.mem = mi;
  b.mem = mj;
  a.pc = 0x0400;
  b.pc = 0x0400;

  int r1 = cpu_step(&a);
  uint64_t n = 0;
  int r2 = jit_run(&b, 1, 0, 0, NULL, &n);
  if (r2 == RUN_MAX_INST && n == 1) r2 = RUN_OK;

  if (r1 != r2) {
    printf("status mismatch op=%02X interp=%d jit=%d\n", opcode, r1, r2);
    return 0;
  }
  if (r1 == RUN_ILLEGAL) return 1; /* both refused, fine */

  Snapshot s1 = snap(&a), s2 = snap(&b);
  if (!same(&s1, &s2)) {
    uint16_t ea = 0;
    switch (kOpcodes[opcode].mode) {
      case AM_IMM: ea = 0x0401; break;
      case AM_ZP: ea = b1; break;
      case AM_ZPX: ea = (uint8_t)(b1 + ci->x); break;
      case AM_ZPY: ea = (uint8_t)(b1 + ci->y); break;
      case AM_ABS: ea = (uint16_t)(b1 | (b2 << 8)); break;
      case AM_ABSX: ea = (uint16_t)((b1 | (b2 << 8)) + ci->x); break;
      case AM_ABSY: ea = (uint16_t)((b1 | (b2 << 8)) + ci->y); break;
      case AM_INDX: {
        uint8_t zp = (uint8_t)(b1 + ci->x);
        ea = (uint16_t)(mi[zp] | (mi[(uint8_t)(zp + 1)] << 8));
        break;
      }
      case AM_INDY: ea = (uint16_t)((mi[b1] | (mi[(uint8_t)(b1 + 1)] << 8)) + ci->y); break;
      default: break;
    }
    printf("  ea=$%04X bytes=%02X %02X  mem[ea]=%02X mode=%d\n", ea, mi[ea],
           mi[(uint16_t)(ea + 1)], mi[ea], kOpcodes[opcode].mode);
    printf("state mismatch op=%02X %s\n  interp pc=%04X a=%02X x=%02X y=%02X sp=%02X p=%02X cyc=%llu\n"
           "  jit    pc=%04X a=%02X x=%02X y=%02X sp=%02X p=%02X cyc=%llu\n",
           opcode, kOpcodesName(opcode), s1.pc, s1.a, s1.x, s1.y, s1.sp, s1.p,
           (unsigned long long)s1.cycles, s2.pc, s2.a, s2.x, s2.y, s2.sp, s2.p,
           (unsigned long long)s2.cycles);
    return 0;
  }
  if (memcmp(mi, mj, MEM_SIZE) != 0) {
    size_t at = 0;
    while (at < MEM_SIZE && mi[at] == mj[at]) at++;
    printf("memory mismatch op=%02X at %04zX interp=%02X jit=%02X\n", opcode, at,
           mi[at], mj[at]);
    return 0;
  }
  return 1;
}

int selftest_difftest(int iterations) {
  uint8_t *mi = aligned_alloc(65536, MEM_SIZE);
  uint8_t *mj = aligned_alloc(65536, MEM_SIZE);
  if (mi == NULL || mj == NULL) return 1;

  Cpu ci, cj;
  int failures = 0;

  for (int it = 0; it < iterations && failures < 12; it++) {
    fill_random(mi);
    memset(&ci, 0, sizeof(ci));
    ci.a = (uint8_t)rnd();
    ci.x = (uint8_t)rnd();
    ci.y = (uint8_t)rnd();
    ci.sp = (uint8_t)rnd();
    ci.p = (uint8_t)rnd();
    ci.pc = 0x0400;
    ci.cycles = 0;
    ci.mem = mi;
    cj = ci;
    cj.mem = mj;

    /* Bias the operand bytes toward zero page / stack / small addresses so
     * zero-page and indexed modes get useful coverage. */
    uint8_t opcode = (uint8_t)rnd();
    uint8_t b1 = (rnd() & 1) ? (uint8_t)rnd() : (uint8_t)(rnd() & 0x0F);
    uint8_t b2 = (uint8_t)rnd();

    if (!check_instruction(opcode, b1, b2, &ci, &cj, mi, mj)) {
      printf("  state: a=%02X x=%02X y=%02X sp=%02X p=%02X b1=%02X b2=%02X\n",
             ci.a, ci.x, ci.y, ci.sp, ci.p, b1, b2);
      failures++;
    }
    jit_flush();
  }

  free(mi);
  free(mj);
  if (failures == 0) {
    printf("difftest OK (%d random instructions)\n", iterations);
    return 0;
  }
  printf("difftest FAILED (%d mismatches)\n", failures);
  return 1;
}
