#include "gem6502.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put(Gem *g, uint16_t addr, const uint8_t *bytes, size_t n) {
  for (size_t i = 0; i < n; i++) g->bus.prg[addr - 0x8000 + i] = bytes[i];
}

static void test_event_order(void) {
  GemEventQueue q;
  gem_event_init(&q);
  GemEvent a = { .when = 30, .type = 1 };
  GemEvent b = { .when = 10, .type = 2 };
  GemEvent c = { .when = 20, .type = 3 };
  assert(gem_event_push(&q, a) == 0);
  assert(gem_event_push(&q, b) == 0);
  assert(gem_event_push(&q, c) == 0);
  GemEvent out;
  assert(gem_event_pop(&q, &out) == 0 && out.when == 10);
  assert(gem_event_pop(&q, &out) == 0 && out.when == 20);
  assert(gem_event_pop(&q, &out) == 0 && out.when == 30);
  assert(gem_event_pop(&q, &out) != 0);
  gem_event_destroy(&q);
}

static void test_alu_program(void) {
  Gem *g = gem_create();
  assert(g);
  /* LDA #$10; CLC; ADC #$05; STA $0200; LDX #3; loop: DEX; BNE loop; JMP $800F */
  const uint8_t code[] = {
    0xA9, 0x10, 0x18, 0x69, 0x05, 0x8D, 0x00, 0x02,
    0xA2, 0x03, 0xCA, 0xD0, 0xFD, 0x4C, 0x0D, 0x80
  };
  put(g, 0x8000, code, sizeof(code));
  g->bus.prg[0x7FFC] = 0x00;
  g->bus.prg[0x7FFD] = 0x80;
  gem_reset(g);
  assert(g->cpu.pc == 0x8000);
  while (!g->cpu.halted && g->cpu.pc != 0x800D) gem_step(g);
  assert(g->cpu.a == 0x15);
  assert(g->cpu.x == 0x00);
  assert(g->bus.ram[0x0200] == 0x15);
  assert(g->cpu.cycles == 26);
  puts("PASS: GEM6502 ALU/branch/cycle count");
  gem_destroy(g);
}

static void test_stack_and_jsr(void) {
  Gem *g = gem_create();
  assert(g);
  /* JSR $8010; LDA #$42; JMP self; ... $8010: PHA; LDA #$55; PLA; RTS */
  const uint8_t code[] = {
    0x20, 0x10, 0x80, 0xA9, 0x42, 0x4C, 0x05, 0x80,
    0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA,
    0x48, 0xA9, 0x55, 0x68, 0x60
  };
  put(g, 0x8000, code, sizeof(code));
  g->bus.prg[0x7FFC] = 0x00;
  g->bus.prg[0x7FFD] = 0x80;
  gem_reset(g);
  gem_run(g, 200);
  assert(g->cpu.a == 0x42);
  assert(g->cpu.sp == 0xFD);
  puts("PASS: GEM6502 JSR/RTS stack");
  gem_destroy(g);
}

static void test_irq_rti(void) {
  Gem *g = gem_create();
  assert(g);
  /* NOP; JMP $8000; IRQ vector -> $8020: PHA; LDA $4015; LDA #$55; PLA; RTI */
  const uint8_t code[] = { 0xEA, 0x4C, 0x00, 0x80 };
  put(g, 0x8000, code, sizeof(code));
  const uint8_t handler[] = { 0xA9, 0x55, 0x48, 0xAD, 0x15, 0x40, 0x68, 0x40 };
  put(g, 0x8020, handler, sizeof(handler));
  g->bus.prg[0x7FFC] = 0x00; g->bus.prg[0x7FFD] = 0x80;
  g->bus.prg[0x7FFE] = 0x20; g->bus.prg[0x7FFF] = 0x80;
  gem_reset(g);
  g->cpu.p &= (uint8_t)~0x04; /* clear I */
  g->apu.irq = true;
  gem_step(g);                /* take IRQ */
  assert(g->cpu.pc == 0x8020);
  for (int i = 0; i < 8 && g->cpu.pc != 0x8000; i++) gem_step(g);
  assert(g->cpu.pc == 0x8000);
  assert(g->cpu.a == 0x55);
  assert(g->cpu.sp == 0xFD);
  assert(!g->apu.irq);
  puts("PASS: GEM6502 IRQ/RTI");
  gem_destroy(g);
}

static void test_ppu_vblank(void) {
  Gem *g = gem_create();
  assert(g);
  g->ppu.ctrl = 0x80;
  gem_ppu_advance(g, 27600);   /* enough to cross scanline 241 */
  assert((g->ppu.status & 0x80) != 0);
  assert(g->ppu.nmi_pending);
  assert(g->stats.ppu_frames >= 1);
  puts("PASS: GEM6502 PPU vblank/NMI");
  gem_destroy(g);
}

int main(void) {
  test_event_order();
  test_alu_program();
  test_stack_and_jsr();
  test_irq_rti();
  test_ppu_vblank();
  puts("ALL GEM6502 TESTS PASS");
  return 0;
}
