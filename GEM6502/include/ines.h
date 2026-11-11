#ifndef GEM6502_INES_H
#define GEM6502_INES_H

#include <stddef.h>
#include <stdint.h>

#define GEM_INES_HEADER_SIZE  16u
#define GEM_INES_TRAINER_SIZE 512u
#define GEM_INES_PRG_BANK_SIZE 0x4000u
#define GEM_INES_CHR_BANK_SIZE 0x2000u

/* Values stored in GemInes.mirroring. */
enum {
  GEM_INES_MIRROR_HORIZONTAL  = 0,
  GEM_INES_MIRROR_VERTICAL    = 1,
  GEM_INES_MIRROR_FOUR_SCREEN = 2,
  GEM_INES_MIRROR_ONE_SCREEN  = 3
};

/*
 * A cartridge image parsed from a 16-byte iNES header.
 *
 * prg and chr are malloc'ed and owned by this object; release them with
 * gem_ines_free().  A CHR RAM cartridge (zero CHR banks in the header)
 * yields chr == NULL and chr_size == 0.
 */
typedef struct GemInes {
  uint8_t *prg; size_t prg_size;
  uint8_t *chr; size_t chr_size;
  int mapper;
  int mirroring;      /* 0 horizontal, 1 vertical, 2 four-screen, 3 one-screen */
  int prg_banks, chr_banks;
  char error[128];
} GemInes;

/*
 * Load an iNES image from path.
 *
 * Returns 0 on success and -1 on failure.  On failure out->error holds a
 * human-readable reason (and no buffers are left allocated).  Trainer-bearing
 * images are rejected.  Mapper support is implemented by the SoC/bus layer.
 */
int gem_ines_load(const char *path, GemInes *out);

/* Free PRG/CHR buffers and zero the descriptor.  Safe on NULL and on a
 * descriptor whose load failed. */
void gem_ines_free(GemInes *ines);

#endif
