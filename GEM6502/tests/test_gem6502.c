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

static void test_mmc3_banks_irq(void) {
  Gem *g = gem_create();
  assert(g);
  g->bus.mapper = 4;
  g->bus.cart_prg_size = 0x10000;
  for (size_t i = 0; i < g->bus.cart_prg_size; i++) g->bus.cart_prg[i] = (uint8_t)(i >> 13);
  /* bank select 6, bank 3 -> $8000; $C000 second-last; $E000 last */
  gem_bus_write(g, 0x8000, 6);
  gem_bus_write(g, 0x8001, 3);
  assert(gem_bus_read(g, 0x8000, false) == 3);
  assert(gem_bus_read(g, 0xC000, false) == 6);
  assert(gem_bus_read(g, 0xE000, false) == 7);
  /* A12 IRQ: latch=1, reload, enable, one rising edge */
  gem_bus_write(g, 0xC000, 1);
  gem_bus_write(g, 0xC001, 0);
  gem_bus_write(g, 0xE001, 0);
  gem_cart_a12(g, 0);
  gem_cart_a12(g, 1);
  gem_cart_a12(g, 0);
  gem_cart_a12(g, 1);
  assert(g->bus.cart_irq);
  puts("PASS: GEM6502 MMC3 banks/A12 IRQ");
  gem_destroy(g);
}

static void test_ppu_vblank(void) {
  Gem *g = gem_create();
  assert(g);
  g->ppu.ctrl = 0x80;
  /* Advance until NEMU's scanline-241 cycle-1 transition is observed. */
  for (int i = 0; i < 30000 && !(g->ppu.status & 0x80); i++)
    gem_ppu_advance(g, 1);
  assert((g->ppu.status & 0x80) != 0);
  assert(g->ppu.nmi_pending);
  puts("PASS: GEM6502 PPU vblank/NMI at scanline 241 cycle 1");
  gem_destroy(g);
}

static void test_ppu_status_read_clear(void) {
  Gem *g = gem_create();
  assert(g);
  g->ppu.status = 0xE0;
  assert(gem_bus_read(g, 0x2002, true) == 0xE0);
  assert(g->ppu.status == 0x60); /* NEMU clears vblank only on read. */
  assert(!g->ppu.w_toggle);
  puts("PASS: GEM6502 $2002 clears vblank only");
  gem_destroy(g);
}

static void test_ppu_sprite0_hit_once_latched(void) {
  Gem *g = gem_create();
  assert(g);
  /* NEMU approximation samples background/sprite overlap at each visible dot. */
  g->ppu.mask = 0x1E;
  g->ppu.oam[0] = 0; /* y=1 */
  g->ppu.oam[1] = 0;
  g->ppu.oam[2] = 0;
  g->ppu.oam[3] = 8;
  memset(g->ppu.chr, 0xFF, sizeof(g->ppu.chr));
  memset(g->ppu.vram, 0, sizeof(g->ppu.vram));
  g->ppu.scanline = 1;
  g->ppu.cycle = 9;
  gem_ppu_advance(g, 1);
  assert((g->ppu.status & 0x40) != 0);
  (void)gem_bus_read(g, 0x2002, true);
  assert((g->ppu.status & 0x40) != 0);
  puts("PASS: GEM6502 sprite-0 hit latches until pre-render");
  gem_destroy(g);
}

static void test_bus_peek(void) {
  Gem *g = gem_create();
  assert(g);
  g->ppu.status = 0xE0;
  g->ppu.w_toggle = true;
  g->ppu.vaddr = 0x2000;
  g->ppu.read_buf = 0x12;
  g->ppu.vram[0] = 0x34;
  g->apu.irq = true;
  g->apu.joy1_shift = 0xA5;
  g->apu.joy2_shift = 0x5A;
  uint64_t reads = g->stats.bus_reads;
  for (int i = 0; i < 2; i++) {
    assert(gem_bus_read(g, 0x3FFA, false) == 0xE0); /* $2002 mirror */
    assert(gem_bus_read(g, 0x2007, false) == 0x12);
    assert(gem_bus_read(g, 0x4015, false) == 0x40);
    assert(gem_bus_read(g, 0x4016, false) == 1);
    assert(gem_bus_read(g, 0x4017, false) == 0);
  }
  assert(g->ppu.status == 0xE0 && g->ppu.w_toggle);
  assert(g->ppu.vaddr == 0x2000 && g->ppu.read_buf == 0x12);
  assert(g->apu.irq && g->apu.joy1_shift == 0xA5 && g->apu.joy2_shift == 0x5A);
  assert(g->stats.bus_reads == reads);
  assert(gem_bus_read(g, 0x2007, true) == 0x12);
  assert(g->ppu.vaddr == 0x2001 && g->ppu.read_buf == 0x34);
  assert(gem_bus_read(g, 0x4015, true) == 0x40 && !g->apu.irq);
  assert(gem_bus_read(g, 0x4016, true) == 1 && g->apu.joy1_shift == 0x52);
  assert(gem_bus_read(g, 0x4017, true) == 0 && g->apu.joy2_shift == 0x2D);
  assert(g->stats.bus_reads == reads + 4);
  gem_destroy(g);
  puts("PASS: GEM6502 bus peeks preserve device state and counters");
}

static void test_ppudata_palette(void) {
  Gem *g = gem_create();
  assert(g);
  g->bus.vertical_mirroring = true;
  g->ppu.vaddr = 0x3F03;
  g->ppu.pal[3] = 0x21;
  g->ppu.vram[0x703] = 0x56; /* $2F03 beneath the palette */
  g->ppu.read_buf = 0x78;
  assert(gem_bus_read(g, 0x2007, false) == 0x21);
  assert(g->ppu.read_buf == 0x78 && g->ppu.vaddr == 0x3F03);
  assert(gem_bus_read(g, 0x2007, true) == 0x21);
  assert(g->ppu.read_buf == 0x56 && g->ppu.vaddr == 0x3F04);
  g->ppu.ctrl = 0x04;
  g->ppu.vaddr = 0x3FFF;
  (void)gem_bus_read(g, 0x2007, true);
  assert(g->ppu.vaddr == 0x401F);
  g->ppu.chr[0x1F] = 0xAB; /* PPU memory bus wraps at $4000 */
  assert(gem_bus_read(g, 0x2007, true) == 0);
  assert(g->ppu.read_buf == 0xAB && g->ppu.vaddr == 0x403F);
  gem_destroy(g);
  puts("PASS: GEM6502 PPUDATA palette buffering and address increment");
}

static void test_dma_reads(void) {
  Gem *g = gem_create();
  assert(g);
  for (int i = 0; i < 256; i++) g->bus.ram[0x200 + i] = (uint8_t)i;
  uint64_t reads = g->stats.bus_reads;
  g->ppu.oam_addr = 0xF0;
  gem_bus_write(g, 0x4014, 2);
  assert(g->stats.bus_reads == reads + 256);
  for (int i = 0; i < 256; i++) assert(g->ppu.oam[(0xF0 + i) & 0xFF] == (uint8_t)i);
  assert(g->cpu.dma_stall == 513);
  gem_destroy(g);
  puts("PASS: GEM6502 DMA performs counted bus reads");
}

static void test_mmc3_ram_isolation(void) {
  Gem *g = gem_create();
  assert(g);
  g->bus.mapper = 4;
  gem_bus_write(g, 0x003B, 0x20);
  gem_bus_write(g, 0x013B, 0x80);
  gem_bus_write(g, 0x603B, 0xAA);
  gem_bus_write(g, 0x683B, 0xBB);
  gem_bus_write(g, 0x7FFF, 0xCC);
  assert(gem_bus_read(g, 0x003B, false) == 0x20);
  assert(gem_bus_read(g, 0x013B, false) == 0x80);
  assert(gem_bus_read(g, 0x603B, false) == 0xAA);
  assert(gem_bus_read(g, 0x683B, false) == 0xBB);
  assert(gem_bus_read(g, 0x7FFF, false) == 0xCC);
  gem_destroy(g);
  puts("PASS: GEM6502 cartridge RAM does not alias zero page or stack");
}

static void test_dma_timing(void) {
  Gem *g = gem_create();
  assert(g);
  const uint8_t code[] = { 0xA9, 2, 0x8D, 0x14, 0x40, 0xEA };
  put(g, 0x8000, code, sizeof(code));
  g->cpu.pc = 0x8000;
  assert(gem_step(g) == 2);
  assert(gem_step(g) == 517);
  assert(g->cpu.cycles == 519 && g->now == 519);
  assert(g->stats.cpu_cycles == 519 && g->cpu.dma_stall == 0);
  assert(g->ppu.scanline == 4 && g->ppu.cycle == 193);
  assert(g->apu.cycles == 519 && g->cpu.instructions == 2);
  gem_destroy(g);
  puts("PASS: GEM6502 DMA keeps CPU/PPU/APU/global clocks aligned");
}

static void test_ppu_odd_frame(void) {
  Gem *g = gem_create();
  assert(g);
  for (int rendering = 0; rendering < 2; rendering++) {
    for (int odd = 0; odd < 2; odd++) {
      g->ppu.scanline = 261;
      g->ppu.cycle = 339;
      g->ppu.odd_frame = odd;
      g->ppu.mask = rendering ? 0x08 : 0;
      gem_ppu_advance(g, 1);
      assert(g->ppu.scanline == 0);
      assert(g->ppu.cycle == (rendering && odd ? 2 : 1));
    }
  }
  gem_destroy(g);
  puts("PASS: GEM6502 odd-frame skip requires rendering");
}

static void test_mmc3_chr(void) {
  Gem *g = gem_create();
  assert(g);
  g->bus.mapper = 4;
  g->bus.cart_chr_size = 0x4000;
  for (unsigned i = 0; i < 0x4000; i++) g->bus.cart_chr[i] = i >> 10;
  for (int r = 0; r < 6; r++) {
    gem_bus_write(g, 0x8000, r);
    gem_bus_write(g, 0x8001, r + 3);
  }
  const uint8_t banks[] = { 2, 3, 4, 5, 5, 6, 7, 8 };
  for (int mode = 0; mode < 2; mode++) {
    gem_bus_write(g, 0x8000, mode ? 0x80 : 0);
    for (unsigned slot = 0; slot < 8; slot++)
      assert(gem_chr_read(g, slot * 1024) == banks[slot ^ (mode ? 4 : 0)]);
  }
  gem_bus_write(g, 0xA000, 0);
  assert(gem_nt_offset(g, 0x2800) == 0);
  gem_bus_write(g, 0xA000, 1);
  assert(gem_nt_offset(g, 0x2400) == 0);
  assert(gem_nt_offset(g, 0x2800) == 0x400);
  gem_destroy(g);
  puts("PASS: GEM6502 MMC3 CHR inversion and nametable mirroring");
}

static void test_irq_pending_ack(void) {
  Gem *g = gem_create();
  assert(g);
  g->bus.mapper = 4;
  g->bus.prg[0] = 0xEA;
  g->bus.prg[0x7FFE] = 0x20; g->bus.prg[0x7FFF] = 0x80;
  g->cpu.pc = 0x8000;
  g->bus.cart_irq = true;
  gem_step(g); /* I is set: latch the pending IRQ at the boundary. */
  assert(g->cpu.irq_pending);
  gem_bus_write(g, 0xE000, 0);
  assert(!g->bus.cart_irq && g->cpu.irq_pending);
  g->cpu.p &= (uint8_t)~4;
  assert(gem_step(g) == 7);
  assert(g->cpu.pc == 0x8020 && !g->cpu.irq_pending);
  gem_destroy(g);
  puts("PASS: GEM6502 pending MMC3 IRQ survives mapper acknowledgement");
}

int main(void) {
  test_irq_pending_ack();
  test_mmc3_ram_isolation();
  test_dma_timing();
  test_ppu_odd_frame();
  test_mmc3_chr();
  test_bus_peek();
  test_ppudata_palette();
  test_dma_reads();
  test_event_order();
  test_alu_program();
  test_stack_and_jsr();
  test_irq_rti();
  test_mmc3_banks_irq();
  test_ppu_vblank();
  test_ppu_status_read_clear();
  test_ppu_sprite0_hit_once_latched();
  puts("ALL GEM6502 TESTS PASS");
  return 0;
}
