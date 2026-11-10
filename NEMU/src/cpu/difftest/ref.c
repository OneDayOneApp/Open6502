/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <difftest-def.h>
#include <memory/paddr.h>
#include <string.h>

__EXPORT void difftest_memcpy(paddr_t addr, void *buf, size_t n, bool direction) {
#ifdef CONFIG_ISA_m6502
  if (direction == DIFFTEST_TO_REF) memcpy(guest_to_host(addr), buf, n);
  else memcpy(buf, guest_to_host(addr), n);
#else
  assert(0);
#endif
}

__EXPORT void difftest_regcpy(void *dut, bool direction) {
#ifdef CONFIG_ISA_m6502
  if (direction == DIFFTEST_TO_REF) memcpy(&cpu, dut, sizeof(cpu));
  else memcpy(dut, &cpu, sizeof(cpu));
#else
  assert(0);
#endif
}

__EXPORT void difftest_exec(uint64_t n) {
#ifdef CONFIG_ISA_m6502
  for (; n > 0; n--) {
    Decode s;
    memset(&s, 0, sizeof(s));
    s.pc = cpu.pc;
    s.snpc = cpu.pc;
    s.dnpc = cpu.pc;
    isa_exec_once(&s);
    cpu.pc = s.dnpc;
  }
#else
  assert(0);
#endif
}

__EXPORT void difftest_raise_intr(word_t NO) {
#ifdef CONFIG_ISA_m6502
  (void)NO;
#else
  assert(0);
#endif
}

__EXPORT void difftest_init(int port) {
  void init_mem();
  init_mem();
  init_isa();
#ifdef CONFIG_ISA_m6502
  cpu.is_65c02 = false;
#endif
}
