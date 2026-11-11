/*
 * GEM6502 - iNES cartridge image loader.
 *
 * Parses the 16-byte iNES header, validates that the image is one this
 * model can execute (NROM / mapper 0), and copies the PRG and CHR payloads
 * into malloc'ed buffers.  NES 2.0 headers are understood for the size and
 * mapper fields, but the same mapper-0 restriction applies.  Trainer images
 * are rejected.
 */
#include "ines.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INES_PRG_BANK_SIZE 0x4000u
#define INES_CHR_BANK_SIZE 0x2000u

static void ines_set_error(GemInes *ines, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(ines->error, sizeof(ines->error), fmt, ap);
  va_end(ap);
}

/*
 * Compute bank_size * banks * 2^exponent with overflow checking.
 * NES 2.0 uses the exponent; plain iNES leaves it at zero.
 */
static int ines_rom_size(size_t bank_size, int banks, int exponent,
                         size_t *size_out) {
  size_t size;

  if (banks < 0 || (size_t)banks > SIZE_MAX / bank_size) return -1;
  size = (size_t)banks * bank_size;
  while (exponent-- > 0) {
    if (size > SIZE_MAX / 2u) return -1;
    size *= 2u;
  }
  *size_out = size;
  return 0;
}

int gem_ines_load(const char *path, GemInes *out) {
  uint8_t header[GEM_INES_HEADER_SIZE];
  FILE *fp = NULL;
  uint8_t *prg = NULL;
  uint8_t *chr = NULL;
  size_t prg_size = 0;
  size_t chr_size = 0;
  size_t got;
  int mapper, prg_banks, chr_banks;
  int prg_exponent = 0, chr_exponent = 0;
  int nes2;
  uint8_t flags6, flags7;

  if (out == NULL) return -1;
  memset(out, 0, sizeof(*out));

  if (path == NULL || path[0] == '\0') {
    ines_set_error(out, "no ROM path given");
    return -1;
  }

  fp = fopen(path, "rb");
  if (fp == NULL) {
    ines_set_error(out, "cannot open '%s': %s", path, strerror(errno));
    return -1;
  }

  got = fread(header, 1, sizeof(header), fp);
  if (got != sizeof(header)) {
    if (ferror(fp)) {
      ines_set_error(out, "read error on '%s' header: %s", path,
                     strerror(errno));
    } else {
      ines_set_error(out, "truncated iNES header: got %zu of %u bytes",
                     got, (unsigned)sizeof(header));
    }
    fclose(fp);
    return -1;
  }

  if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' ||
      header[3] != 0x1A) {
    ines_set_error(out, "not an iNES image: bad magic (expected NES\\x1A)");
    fclose(fp);
    return -1;
  }

  flags6 = header[6];
  flags7 = header[7];

  if ((flags6 & 0x04u) != 0) {
    ines_set_error(out,
                   "trainer is not supported (iNES header trainer bit set)");
    fclose(fp);
    return -1;
  }

  /*
   * NES 2.0 is identified by bits 2-3 of byte 7 being 10b.  Its extended
   * size fields live in bytes 9-10 and its mapper high nibble in byte 8;
   * the basic 16-byte iNES layout is otherwise unchanged.
   */
  nes2 = ((flags7 >> 2) & 0x03u) == 0x02u;

  mapper = (int)((flags6 >> 4) | (flags7 & 0xF0u));
  if (nes2) mapper |= (int)(header[8] & 0x0Fu) << 8;

  prg_banks = header[4];
  chr_banks = header[5];
  if (nes2) {
    prg_banks |= (int)(header[9] & 0x0Fu) << 8;
    chr_banks |= (int)(header[9] >> 4) << 8;
    prg_exponent = header[10] & 0x0Fu;
    chr_exponent = header[10] >> 4;
  }

  out->mapper = mapper;
  out->prg_banks = prg_banks;
  out->chr_banks = chr_banks;
  out->mirroring = (flags6 & 0x08u)
                       ? GEM_INES_MIRROR_FOUR_SCREEN
                       : ((flags6 & 0x01u) ? GEM_INES_MIRROR_VERTICAL
                                           : GEM_INES_MIRROR_HORIZONTAL);

  if (prg_banks == 0) {
    ines_set_error(out, "invalid iNES header: PRG bank count is 0");
    fclose(fp);
    return -1;
  }

  if (ines_rom_size(INES_PRG_BANK_SIZE, prg_banks, prg_exponent, &prg_size) !=
          0 ||
      ines_rom_size(INES_CHR_BANK_SIZE, chr_banks, chr_exponent, &chr_size) !=
          0) {
    ines_set_error(out, "iNES header declares an impossible ROM size");
    fclose(fp);
    return -1;
  }

  prg = malloc(prg_size);
  if (prg == NULL) {
    ines_set_error(out, "out of memory allocating %zu-byte PRG ROM", prg_size);
    fclose(fp);
    return -1;
  }

  got = fread(prg, 1, prg_size, fp);
  if (got != prg_size) {
    if (ferror(fp)) {
      ines_set_error(out, "read error on PRG ROM: %s", strerror(errno));
    } else {
      ines_set_error(out, "truncated PRG ROM: got %zu of %zu bytes", got,
                     prg_size);
    }
    free(prg);
    fclose(fp);
    return -1;
  }

  if (chr_size != 0) {
    chr = malloc(chr_size);
    if (chr == NULL) {
      ines_set_error(out, "out of memory allocating %zu-byte CHR ROM",
                     chr_size);
      free(prg);
      fclose(fp);
      return -1;
    }

    got = fread(chr, 1, chr_size, fp);
    if (got != chr_size) {
      if (ferror(fp)) {
        ines_set_error(out, "read error on CHR ROM: %s", strerror(errno));
      } else {
        ines_set_error(out, "truncated CHR ROM: got %zu of %zu bytes", got,
                       chr_size);
      }
      free(chr);
      free(prg);
      fclose(fp);
      return -1;
    }
  }
  /* chr_size == 0 means CHR RAM: leave chr NULL by design. */

  if (fclose(fp) != 0) {
    ines_set_error(out, "error closing '%s': %s", path, strerror(errno));
    free(chr);
    free(prg);
    return -1;
  }

  out->prg = prg;
  out->prg_size = prg_size;
  out->chr = chr;
  out->chr_size = chr_size;
  out->error[0] = '\0';
  return 0;
}

void gem_ines_free(GemInes *ines) {
  if (ines == NULL) return;
  free(ines->prg);
  free(ines->chr);
  memset(ines, 0, sizeof(*ines));
}
