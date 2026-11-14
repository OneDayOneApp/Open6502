/*
 * Interface between the translator (jit_compile.c) and the runtime
 * (jit_runtime.c).  A block is a straight run of 6502 instructions that ends
 * on any control-flow change; the runtime caches one compiled function per
 * block start address.
 */
#ifndef J6502_JIT_H
#define J6502_JIT_H

#include <stdio.h>

#include "arm64.h"
#include "cpu.h"

/* Per-PC resume entries for block chaining (NULL when not compiled), plus
 * the NES bus hooks.  x29 points at this struct inside translated code, so
 * the first members must stay pointer-sized and small-offset. */
typedef struct JitRuntime {
  uint8_t (*bus_read)(uint16_t);
  void (*bus_write)(uint16_t, uint8_t);
  volatile uint8_t pending; /* set when a frame/interrupt boundary is reached */
  int (*resume[65536])(Cpu *);
} JitRuntime;

extern JitRuntime g_jit_rt;

typedef struct JitBlock {
  int (*fn)(Cpu *);
  int (*resume)(Cpu *); /* entry point after the prologue, for chaining */
  struct JitBlock *next_page;
  uint16_t pc;
  uint8_t insts;
} JitBlock;

/* Translate the block at `start_pc`.  Returns the number of guest
 * instructions covered (0 = not translatable), and sets *out_fn. */
int jit_compile(Emitter *e, const uint8_t *mem, uint8_t (*code_read)(uint16_t),
                uint16_t start_pc, int has_stop, uint16_t stop_pc, int max_insts,
                uint16_t code_lo, uint16_t code_hi, int chaining, int bus,
                int (**out_fn)(Cpu *), int (**out_resume)(Cpu *));

/* Block size used by the runtime (JIT_MAX_BLOCK env var, default 48). */
void jit_set_max_block(int n);
/* Guest address window that translated stores treat as potentially-code. */
void jit_set_code_range(uint16_t lo, uint16_t hi);
/* Block chaining trades exact --max-inst accounting for throughput. */
void jit_set_chaining(int on);
/* Route translated loads/stores through the NES bus instead of flat memory. */
void jit_set_bus_mode(int on);
void jit_set_sync_hook(void (*fn)(Cpu *));
/* Called between blocks; returning non-zero stops the run (frame boundary). */
void jit_set_stop_check(int (*fn)(void));

/* Runtime services. */
int jit_available(void);
void jit_set_max_block(int n);
/* Guest address window that translated stores treat as potentially-code. */
void jit_set_code_range(uint16_t lo, uint16_t hi);
/* Block chaining trades exact --max-inst accounting for throughput. */
void jit_set_chaining(int on);
/* Route translated loads/stores through the NES bus instead of flat memory. */
void jit_set_bus_mode(int on);
void jit_set_sync_hook(void (*fn)(Cpu *));
/* Called between blocks; returning non-zero stops the run (frame boundary). */
void jit_set_stop_check(int (*fn)(void));
int jit_run(Cpu *c, uint64_t max_inst, int has_stop, uint16_t stop_pc,
            FILE *trace, uint64_t *out_inst);
void jit_flush(void);
uint64_t jit_flush_count(void);
uint64_t jit_compile_count(void);
uint64_t jit_page_flush_count(void);
uint64_t jit_fallback_steps(void);
uint64_t jit_blocks_run(void);

/* C helpers for decimal-mode ADC/SBC; they return (p << 8) | a. */
uint16_t jit_adc_decimal(uint8_t a, uint8_t m, uint8_t p);
uint16_t jit_sbc_decimal(uint8_t a, uint8_t m, uint8_t p);

#endif /* J6502_JIT_H */
