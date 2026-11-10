#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nes/ref/nes_ref.h"

static void test_ntsc_clock(void) {
  nes_ref_clock c;
  nes_ref_clock_init(&c, NES_REF_NTSC);
  assert(c.master_tick == 0);
  assert(c.cpu_cycle == 0);
  assert(c.ppu_dot == 0);

  nes_ref_clock_tick(&c, 1000000);
  assert(c.master_tick == 1000000);
  assert(c.cpu_cycle == 1000000 / 12);
  assert(c.ppu_dot == 1000000 / 4);
  assert(c.ppu_cycle == (1000000 / 4) % 341);
  assert(c.ppu_scanline == (1000000 / 4 / 341) % 262);
}

static void test_pal_clock(void) {
  nes_ref_clock c;
  nes_ref_clock_init(&c, NES_REF_PAL);
  nes_ref_clock_tick(&c, 1000000);
  assert(c.cpu_cycle == 1000000 / 16);
  assert(c.ppu_dot == 1000000 / 5);
}

static void test_clock_is_deterministic(void) {
  nes_ref_clock a, b;
  nes_ref_clock_init(&a, NES_REF_NTSC);
  nes_ref_clock_init(&b, NES_REF_NTSC);
  nes_ref_clock_tick(&a, 1234567);
  for (int i = 0; i < 1234567; i++) nes_ref_clock_tick(&b, 1);
  assert(a.master_tick == b.master_tick);
  assert(a.cpu_cycle == b.cpu_cycle);
  assert(a.ppu_dot == b.ppu_dot);
  assert(a.ppu_scanline == b.ppu_scanline);
  assert(a.frame == b.frame);
}

static void test_clock_frame(void) {
  nes_ref_clock c;
  nes_ref_clock_init(&c, NES_REF_NTSC);
  /* 341 * 262 PPU dots = one NTSC frame; master = dots * 4. */
  nes_ref_clock_tick(&c, 341u * 262u * 4u);
  assert(c.frame == 1);
  assert(c.ppu_scanline == 0);
  assert(c.ppu_cycle == 0);
}

static void test_rom_parse(void) {
  uint8_t rom[16 + 16384 + 8192];
  memset(rom, 0, sizeof(rom));
  memcpy(rom, "NES\x1A", 4);
  rom[4] = 1; /* 16K PRG */
  rom[5] = 1; /* 8K CHR */
  rom[16] = 0xEA;
  rom[16 + 16384] = 0x55;

  nes_ref_rom r;
  assert(nes_ref_rom_load_mem(&r, rom, sizeof(rom)) == NES_REF_OK);
  assert(r.mapper == 0);
  assert(r.prg_size == 16384);
  assert(r.chr_size == 8192);
  assert(r.prg[0] == 0xEA);
  assert(r.chr[0] == 0x55);
  assert(!r.chr_ram);
  nes_ref_rom_free(&r);
}

static void test_rom_rejects_bad_header(void) {
  uint8_t bad[16] = {0};
  nes_ref_rom r;
  assert(nes_ref_rom_load_mem(&r, bad, sizeof(bad)) != NES_REF_OK);
}

static void test_rom_chr_ram(void) {
  uint8_t rom[16 + 16384];
  memset(rom, 0, sizeof(rom));
  memcpy(rom, "NES\x1A", 4);
  rom[4] = 1;
  rom[5] = 0;
  nes_ref_rom r;
  assert(nes_ref_rom_load_mem(&r, rom, sizeof(rom)) == NES_REF_OK);
  assert(r.chr_ram);
  assert(r.chr_size == 8192);
  nes_ref_rom_free(&r);
}

static void test_hash_deterministic(void) {
  uint8_t data[64];
  for (int i = 0; i < 64; i++) data[i] = (uint8_t)(i * 7);
  uint64_t a = nes_ref_hash_bytes(NES_REF_HASH_SEED, data, sizeof(data));
  uint64_t b = nes_ref_hash_bytes(NES_REF_HASH_SEED, data, sizeof(data));
  assert(a == b);
  data[0] ^= 1;
  uint64_t c = nes_ref_hash_bytes(NES_REF_HASH_SEED, data, sizeof(data));
  assert(a != c);
}

static void test_state_hash(void) {
  nes_ref_state s1, s2;
  memset(&s1, 0, sizeof(s1));
  memset(&s2, 0, sizeof(s2));
  s1.clock.master_tick = s2.clock.master_tick = 42;
  s1.cpu.a = 1;
  s2.cpu.a = 1;
  assert(nes_ref_state_hash(&s1) == nes_ref_state_hash(&s2));
  s2.cpu.a = 2;
  assert(nes_ref_state_hash(&s1) != nes_ref_state_hash(&s2));
}

static void make_nrom(uint8_t *rom, size_t size, uint8_t fill) {
  memset(rom, 0, size);
  memcpy(rom, "NES\x1A", 4);
  rom[4] = 1;
  rom[5] = 0;
  memset(rom + 16, fill, 16384);
  /* NROM reset vector at PRG offset 0x3FFC/0x3FFD (CPU $FFFC/FFFD). */
  rom[16 + 0x3FFC] = 0x00;
  rom[16 + 0x3FFD] = 0x80;
}

static void test_machine_nrom_bus(void) {
  uint8_t rom[16 + 16384];
  make_nrom(rom, sizeof(rom), 0xA5);
  nes_ref_machine m;
  assert(nes_ref_machine_init(&m, NES_REF_NTSC) == NES_REF_OK);
  assert(nes_ref_machine_load_rom(&m, rom, sizeof(rom)) == NES_REF_OK);
  assert(m.state.cpu.pc == 0x8000);
  assert(nes_ref_machine_read(&m, 0x8000) == 0xA5);
  assert(nes_ref_machine_read(&m, 0xC000) == 0xA5); /* 16K NROM mirror */
  nes_ref_machine_write(&m, 0x0000, 0x12);
  assert(nes_ref_machine_read(&m, 0x0800) == 0x12);
  nes_ref_machine_write(&m, 0x6000, 0x34);
  assert(nes_ref_machine_read(&m, 0x6000) == 0x34);
  uint64_t h1 = nes_ref_machine_hash(&m);
  nes_ref_machine_write(&m, 0x0001, 0x99);
  assert(nes_ref_machine_hash(&m) != h1);
  nes_ref_machine_free(&m);
}

static void test_cpu_basic_program(void) {
  uint8_t rom[16 + 16384];
  make_nrom(rom, sizeof(rom), 0xEA);
  uint8_t *prg = rom + 16;
  /* $8000: LDA #$42; STA $0200; LDX #$05; INX; CPX #$06;
   * BNE fail; LDY #$7F; STY $0201; success: JMP success */
  const uint8_t code[] = {
    0xA9, 0x42,       /* LDA #$42 */
    0x8D, 0x00, 0x02, /* STA $0200 */
    0xA2, 0x05,       /* LDX #$05 */
    0xE8,             /* INX */
    0xE0, 0x06,       /* CPX #$06 */
    0xD0, 0xFE,       /* BNE fail (to itself) */
    0xA0, 0x7F,       /* LDY #$7F */
    0x8C, 0x01, 0x02, /* STY $0201 */
    0x4C, 0x11, 0x80  /* JMP $8011 */
  };
  memcpy(prg, code, sizeof(code));

  nes_ref_machine m;
  assert(nes_ref_machine_init(&m, NES_REF_NTSC) == NES_REF_OK);
  assert(nes_ref_machine_load_rom(&m, rom, sizeof(rom)) == NES_REF_OK);
  assert(m.state.cpu.pc == 0x8000);

  for (int i = 0; i < 100 && m.state.cpu.pc != 0x8011 && !m.state.cpu.jammed; i++) {
    assert(nes_ref_cpu_step(&m) == NES_REF_OK);
  }
  assert(m.state.cpu.pc == 0x8011);
  assert(m.state.cpu.a == 0x42);
  assert(m.state.cpu.x == 0x06);
  assert(m.state.cpu.y == 0x7F);
  assert(m.state.ram[0x0200] == 0x42);
  assert(m.state.ram[0x0201] == 0x7F);
  assert(m.state.cpu.cycles > 0);
  nes_ref_machine_free(&m);
}

static int run_klaus(const char *path) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) return -1;
  if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return -1; }
  long len = ftell(fp);
  rewind(fp);
  if (len <= 0 || len > 65536) { fclose(fp); return -1; }
  uint8_t *buf = malloc((size_t)len);
  if (buf == NULL) { fclose(fp); return -1; }
  if (fread(buf, 1, (size_t)len, fp) != (size_t)len) {
    free(buf); fclose(fp); return -1;
  }
  fclose(fp);

  nes_ref_machine m;
  if (nes_ref_machine_init_flat(&m, NES_REF_NTSC) != NES_REF_OK) {
    free(buf);
    return -1;
  }
  if (nes_ref_machine_load_flat(&m, buf, (size_t)len, 0x0400) != NES_REF_OK) {
    free(buf); nes_ref_machine_free(&m); return -1;
  }
  free(buf);

  for (int i = 0; i < 60000000 && m.state.cpu.pc != 0x3469 && !m.state.cpu.jammed; i++) {
    if (nes_ref_cpu_step(&m) != NES_REF_OK) break;
  }
  int ok = m.state.cpu.pc == 0x3469;
  printf("PASS: REF CPU Klaus at PC=%04X (cycles=%llu)\n",
         m.state.cpu.pc, (unsigned long long)m.state.cpu.cycles);
  nes_ref_machine_free(&m);
  return ok ? 0 : -1;
}

int main(void) {
  test_ntsc_clock();
  test_pal_clock();
  test_clock_is_deterministic();
  test_clock_frame();
  test_rom_parse();
  test_rom_rejects_bad_header();
  test_rom_chr_ram();
  test_hash_deterministic();
  test_state_hash();
  test_machine_nrom_bus();
  test_cpu_basic_program();
  const char *klaus = getenv("NESREF_KLAUS");
  if (klaus != NULL && *klaus != 0) {
    assert(run_klaus(klaus) == 0);
  }
  puts("PASS: nes_ref clock/rom/hash");
  return 0;
}
