/*
 * Self-contained tests for the GEM6502 iNES loader.
 *
 * Build and run from the GEM6502 directory:
 *   cc -std=c11 -O2 -Wall -Wextra -Iinclude src/ines.c tests/test_ines.c \
 *      -o /tmp/test_ines
 *   /tmp/test_ines
 *
 * An optional ROM path may be passed as argv[1]; otherwise the test tries
 * GEM_INES_ROM and a few paths relative to the usual build directories.
 */
#include "ines.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned tmp_seq;

static void tmp_path(char *buf, size_t cap, const char *tag) {
  snprintf(buf, cap, "/tmp/gem_ines_selftest_%s_%u.nes", tag, tmp_seq++);
}

static int write_bytes(const char *path, const uint8_t *data, size_t size) {
  FILE *fp = fopen(path, "wb");
  size_t wrote;
  int ok;

  if (fp == NULL) return -1;
  wrote = fwrite(data, 1, size, fp);
  ok = (wrote == size);
  if (fclose(fp) != 0) ok = 0;
  return ok ? 0 : -1;
}

static void make_header(uint8_t header[GEM_INES_HEADER_SIZE], int prg_banks,
                        int chr_banks, uint8_t flags6, uint8_t flags7) {
  memset(header, 0, GEM_INES_HEADER_SIZE);
  header[0] = 'N';
  header[1] = 'E';
  header[2] = 'S';
  header[3] = 0x1A;
  header[4] = (uint8_t)prg_banks;
  header[5] = (uint8_t)chr_banks;
  header[6] = flags6;
  header[7] = flags7;
}

static int write_image(const char *path,
                       const uint8_t header[GEM_INES_HEADER_SIZE],
                       size_t prg_size, size_t chr_size, int with_trainer) {
  size_t total = GEM_INES_HEADER_SIZE + prg_size + chr_size +
                 (with_trainer ? GEM_INES_TRAINER_SIZE : 0u);
  uint8_t *image = malloc(total);
  int rc;

  if (image == NULL) return -1;
  memset(image, 0xEA, total);
  memcpy(image, header, GEM_INES_HEADER_SIZE);
  if (with_trainer) {
    memset(image + GEM_INES_HEADER_SIZE, 0x00, GEM_INES_TRAINER_SIZE);
  }
  rc = write_bytes(path, image, total);
  free(image);
  return rc;
}

static void expect_rejected(const char *path, const char *needle) {
  GemInes ines;

  memset(&ines, 0xA5, sizeof(ines)); /* loader must initialize the struct */
  assert(gem_ines_load(path, &ines) == -1);
  assert(ines.error[0] != '\0');
  assert(strstr(ines.error, needle) != NULL);
  assert(ines.prg == NULL && ines.chr == NULL);
  assert(ines.prg_size == 0 && ines.chr_size == 0);
  gem_ines_free(&ines);
}

static void test_valid_plain_ines(void) {
  uint8_t header[GEM_INES_HEADER_SIZE];
  char path[256];
  GemInes ines;

  make_header(header, 1, 2, 0x01, 0x00); /* mapper 0, vertical mirroring */
  tmp_path(path, sizeof(path), "valid");
  assert(write_image(path, header, 0x4000, 0x4000, 0) == 0);

  assert(gem_ines_load(path, &ines) == 0);
  assert(ines.error[0] == '\0');
  assert(ines.mapper == 0);
  assert(ines.mirroring == GEM_INES_MIRROR_VERTICAL);
  assert(ines.prg_banks == 1 && ines.chr_banks == 2);
  assert(ines.prg_size == 0x4000 && ines.chr_size == 0x4000);
  assert(ines.prg != NULL && ines.chr != NULL);
  assert(ines.prg[0] == 0xEA && ines.chr[0] == 0xEA);

  gem_ines_free(&ines);
  assert(ines.prg == NULL && ines.chr == NULL);
  assert(ines.prg_size == 0 && ines.chr_size == 0);
  remove(path);
  puts("PASS: valid mapper-0 image (vertical mirroring)");
}

static void test_chr_ram(void) {
  uint8_t header[GEM_INES_HEADER_SIZE];
  char path[256];
  GemInes ines;

  make_header(header, 1, 0, 0x00, 0x00); /* no CHR ROM: CHR RAM */
  tmp_path(path, sizeof(path), "chr_ram");
  assert(write_image(path, header, 0x4000, 0, 0) == 0);

  assert(gem_ines_load(path, &ines) == 0);
  assert(ines.mapper == 0);
  assert(ines.mirroring == GEM_INES_MIRROR_HORIZONTAL);
  assert(ines.prg_size == 0x4000 && ines.chr_size == 0);
  assert(ines.prg != NULL && ines.chr == NULL);
  gem_ines_free(&ines);
  remove(path);
  puts("PASS: CHR RAM image yields chr == NULL, chr_size == 0");
}

static void test_four_screen(void) {
  uint8_t header[GEM_INES_HEADER_SIZE];
  char path[256];
  GemInes ines;

  make_header(header, 1, 1, 0x08, 0x00); /* four-screen */
  tmp_path(path, sizeof(path), "four_screen");
  assert(write_image(path, header, 0x4000, 0x2000, 0) == 0);

  assert(gem_ines_load(path, &ines) == 0);
  assert(ines.mirroring == GEM_INES_MIRROR_FOUR_SCREEN);
  gem_ines_free(&ines);
  remove(path);
  puts("PASS: four-screen mirroring decoded");
}

static void test_nes2_header(void) {
  uint8_t header[GEM_INES_HEADER_SIZE];
  char path[256];
  GemInes ines;

  make_header(header, 1, 1, 0x00, 0x08); /* NES 2.0 identifier, mapper 0 */
  tmp_path(path, sizeof(path), "nes2");
  assert(write_image(path, header, 0x4000, 0x2000, 0) == 0);

  assert(gem_ines_load(path, &ines) == 0);
  assert(ines.mapper == 0);
  assert(ines.prg_size == 0x4000 && ines.chr_size == 0x2000);
  gem_ines_free(&ines);
  remove(path);
  puts("PASS: NES 2.0 mapper-0 header with base sizes");
}

static void test_bad_magic(void) {
  uint8_t header[GEM_INES_HEADER_SIZE];
  char path[256];

  make_header(header, 1, 1, 0x00, 0x00);
  header[0] = 'X';
  tmp_path(path, sizeof(path), "bad_magic");
  assert(write_image(path, header, 0, 0, 0) == 0);
  expect_rejected(path, "magic");
  remove(path);
  puts("PASS: bad magic rejected");
}

static void test_trainer(void) {
  uint8_t header[GEM_INES_HEADER_SIZE];
  char path[256];

  make_header(header, 1, 1, 0x04, 0x00); /* trainer bit */
  tmp_path(path, sizeof(path), "trainer");
  assert(write_image(path, header, 0x4000, 0x2000, 1) == 0);
  expect_rejected(path, "trainer");
  remove(path);
  puts("PASS: trainer image rejected");
}

static void test_mapper_header(void) {
  uint8_t header[GEM_INES_HEADER_SIZE];
  char path[256];
  GemInes ines;
  make_header(header, 1, 1, 0x20, 0x00); /* mapper 2 */
  tmp_path(path, sizeof(path), "mapper");
  assert(write_image(path, header, 0x4000, 0x2000, 0) == 0);
  assert(gem_ines_load(path, &ines) == 0);
  assert(ines.mapper == 2);
  gem_ines_free(&ines);
  remove(path);
  puts("PASS: mapper header parsed");
}

static void test_truncated_files(void) {
  uint8_t header[GEM_INES_HEADER_SIZE];
  char path[256];
  const uint8_t short_header[4] = { 'N', 'E', 'S', 0x1A };

  tmp_path(path, sizeof(path), "short_header");
  assert(write_bytes(path, short_header, sizeof(short_header)) == 0);
  expect_rejected(path, "truncated");
  remove(path);

  make_header(header, 2, 1, 0x00, 0x00); /* 32K PRG + 8K CHR expected */
  tmp_path(path, sizeof(path), "short_prg");
  assert(write_image(path, header, 0x4000, 0x2000, 0) == 0);
  expect_rejected(path, "truncated");
  remove(path);

  tmp_path(path, sizeof(path), "short_chr");
  assert(write_image(path, header, 0x8000, 0x1000, 0) == 0);
  expect_rejected(path, "truncated");
  remove(path);

  puts("PASS: truncated header/PRG/CHR rejected without crashing");
}

static void test_missing_and_null(void) {
  GemInes ines;
  char path[256];

  tmp_path(path, sizeof(path), "missing");
  remove(path);
  memset(&ines, 0, sizeof(ines));
  assert(gem_ines_load(path, &ines) == -1);
  assert(strstr(ines.error, "cannot open") != NULL);

  assert(gem_ines_load(NULL, &ines) == -1);
  assert(strstr(ines.error, "path") != NULL);
  assert(gem_ines_load(path, NULL) == -1);
  gem_ines_free(NULL);
  puts("PASS: missing file and NULL arguments handled");
}

static const char *find_falling_rom(int argc, char **argv) {
  static const char *candidates[] = {
    "../roms/falling.nes",
    "roms/falling.nes",
    "../../roms/falling.nes",
    "/Users/tongxiaojun/github/Open6502/roms/falling.nes"
  };
  const char *env = getenv("GEM_INES_ROM");
  size_t i;

  if (argc > 1) return argv[1];
  if (env != NULL && env[0] != '\0') return env;
  for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    FILE *fp = fopen(candidates[i], "rb");
    if (fp != NULL) {
      fclose(fp);
      return candidates[i];
    }
  }
  return NULL;
}

static void test_falling(const char *path) {
  GemInes ines;

  assert(gem_ines_load(path, &ines) == 0);
  assert(ines.error[0] == '\0');
  assert(ines.mapper == 0);
  assert(ines.mirroring == GEM_INES_MIRROR_HORIZONTAL);
  assert(ines.prg_banks == 2 && ines.chr_banks == 1);
  assert(ines.prg_size == 32768);
  assert(ines.chr_size == 8192);
  assert(ines.prg != NULL && ines.chr != NULL);

  printf("PASS: %s mapper=%d PRG=%zu CHR=%zu mirroring=horizontal\n", path,
         ines.mapper, ines.prg_size, ines.chr_size);
  gem_ines_free(&ines);
}

int main(int argc, char **argv) {
  const char *rom;

  test_valid_plain_ines();
  test_chr_ram();
  test_four_screen();
  test_nes2_header();
  test_bad_magic();
  test_trainer();
  test_mapper_header();
  test_truncated_files();
  test_missing_and_null();

  rom = find_falling_rom(argc, argv);
  if (rom != NULL) {
    test_falling(rom);
  } else {
    puts("SKIP: falling.nes not found (pass its path as argv[1])");
  }

  puts("PASS: all iNES tests");
  return 0;
}
