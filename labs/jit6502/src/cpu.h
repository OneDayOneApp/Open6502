/*
 * Minimal flat-memory 6502 core shared by the interpreter and the JIT.
 *
 * The lab deliberately keeps the machine small: 64 KiB of RAM at address
 * zero, no PPU/APU/mapper.  That is enough to run the Klaus functional test
 * (which self-tests the CPU and loads at $0000) and to measure translation
 * overhead without dragging in the NES bus model.
 */
#ifndef J6502_CPU_H
#define J6502_CPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEM_SIZE 65536u

typedef struct Cpu {
  uint8_t a;
  uint8_t x;
  uint8_t y;
  uint8_t sp;
  uint8_t p;
  uint16_t pc;
  uint64_t cycles;
  uint8_t *mem; /* MEM_SIZE bytes, owned by the caller */
  /* Set by translated stores that may have overwritten code; the dispatcher
   * flushes the block cache before the next block. */
  volatile uint8_t code_dirty;
  volatile uint8_t dirty_page;
  /* Decremented by chained blocks; when it hits zero control returns to the
   * dispatcher so budgets/stop points stay responsive. */
  uint16_t chain_budget;
  /* NES mode: memory accesses go through the bus instead of the flat image. */
  uint8_t bus_mode;
  uint8_t nmi_pending;
  uint8_t irq_pending;
  /* Translated code spills the running instruction count here around C
   * helper calls (all registers are caller-saved or already in use). */
  uint32_t jit_count;
  /* Cycles owed by bus side effects (OAM DMA) that the block epilogue would
   * otherwise overwrite; absorbed by nes_sync. */
  uint16_t extra_cycles;
} Cpu;

/* Processor status bits.  B (0x10) and U (0x20) are not real state: they are
 * set by pushes and ignored on pulls, but keeping U set matches the NMOS
 * power-on value and the RTL model. */
enum {
  F_C = 0x01,
  F_Z = 0x02,
  F_I = 0x04,
  F_D = 0x08,
  F_B = 0x10,
  F_U = 0x20,
  F_V = 0x40,
  F_N = 0x80
};

/* Return codes for the step loop. */
enum {
  RUN_OK = 0,
  RUN_STOP_PC = 1,
  RUN_MAX_INST = 2,
  RUN_ILLEGAL = 3
};

void cpu_init(Cpu *c, uint8_t *mem);

/* NES bus (implemented in nes/nes_system.c). */
uint8_t nes_bus_read(uint16_t addr);
void nes_bus_write(uint16_t addr, uint8_t value);
uint8_t nes_peek(uint16_t addr);

static inline uint8_t cpu_rd(const Cpu *c, uint16_t addr) {
  return c->bus_mode ? nes_bus_read(addr) : c->mem[addr];
}
static inline void cpu_wr(Cpu *c, uint16_t addr, uint8_t value) {
  if (c->bus_mode) {
    nes_bus_write(addr, value);
  } else {
    c->mem[addr] = value;
  }
}
/* Execute one instruction (interpreter). Returns RUN_OK / RUN_ILLEGAL. */
int cpu_step(Cpu *c);
/* Load an image at `load_addr`; returns false on I/O error. */
bool cpu_load(Cpu *c, const char *path, uint16_t load_addr);
/* pc = RESET vector, sp = $FD, p = U|I, a/x/y = 0. */
void cpu_reset(Cpu *c);
static inline uint8_t cpu_read(const Cpu *c, uint16_t addr) {
  return c->mem[addr];
}
static inline void cpu_write(Cpu *c, uint16_t addr, uint8_t value) {
  c->mem[addr] = value;
}

#endif /* J6502_CPU_H */
