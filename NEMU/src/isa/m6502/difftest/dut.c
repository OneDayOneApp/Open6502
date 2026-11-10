#include <isa.h>
#include <cpu/difftest.h>
#include "../local-include/m6502.h"

bool isa_difftest_checkregs(CPU_state *ref_r, vaddr_t pc) {
  bool ok = true;
  ok &= difftest_check_reg("pc", pc, ref_r->pc & 0xFFFF, cpu.pc & 0xFFFF);
  ok &= difftest_check_reg("a", pc, ref_r->a, cpu.a);
  ok &= difftest_check_reg("x", pc, ref_r->x, cpu.x);
  ok &= difftest_check_reg("y", pc, ref_r->y, cpu.y);
  ok &= difftest_check_reg("sp", pc, ref_r->sp, cpu.sp);
  ok &= difftest_check_reg("p", pc, ref_r->p, cpu.p);
  ok &= difftest_check_reg("cycles", pc, ref_r->cycles, cpu.cycles);
  return ok;
}

void isa_difftest_attach() {
}
