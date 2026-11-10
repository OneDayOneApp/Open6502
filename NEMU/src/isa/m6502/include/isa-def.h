#ifndef __ISA_M6502_DEF_H__
#define __ISA_M6502_DEF_H__

#include <common.h>

/*
 * 6502 architectural state.  Only the low 16 bits of pc are used; NEMU's
 * generic word_t remains 32-bit so the rest of the framework is unchanged.
 */
typedef struct {
  uint8_t a;
  uint8_t x;
  uint8_t y;
  uint8_t sp;
  uint8_t p;
  vaddr_t pc;
  uint64_t cycles;
  bool irq_pending;
  bool nmi_pending;
  bool is_65c02;
} m6502_CPU_state;

typedef struct {
  uint8_t inst[4];
  uint8_t len;
} m6502_ISADecodeInfo;

#define isa_mmu_check(vaddr, len, type) (MMU_DIRECT)

#endif
