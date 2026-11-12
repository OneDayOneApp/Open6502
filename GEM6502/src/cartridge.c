#include "gem6502.h"
#include "ines.h"
#include <string.h>

/* Install a parsed cartridge image into the SoC and reset it. */
static int install_ines(Gem *g, const GemInes *ines) {
  if (ines->mapper != 0 && ines->mapper != 1 && ines->mapper != 2 &&
      ines->mapper != 3 && ines->mapper != 4 && ines->mapper != 7) {
    fprintf(stderr, "gem6502: mapper %d not implemented yet\n", ines->mapper);
    return 2;
  }
  if (ines->prg_size > GEM_CART_PRG_MAX || ines->chr_size > GEM_CART_CHR_MAX) {
    fprintf(stderr, "gem6502: cartridge too large\n");
    return 2;
  }
  memset(&g->bus, 0, sizeof(g->bus));
  memset(g->bus.cart_prg, 0xEA, sizeof(g->bus.cart_prg));
  memcpy(g->bus.cart_prg, ines->prg, ines->prg_size);
  g->bus.cart_prg_size = ines->prg_size;
  g->bus.mapper = (uint8_t)ines->mapper;
  g->bus.vertical_mirroring = ines->mirroring == 1;
  g->bus.cart_chr_size = ines->chr_size;
  if (ines->chr) memcpy(g->bus.cart_chr, ines->chr, ines->chr_size);
  memset(g->bus.prg, 0xEA, sizeof(g->bus.prg));
  memcpy(g->bus.prg, ines->prg,
         ines->prg_size < GEM_PRG_SIZE ? ines->prg_size : GEM_PRG_SIZE);
  gem_reset(g);
  g->apu.frame_inhibit = true;
  /* gem_reset clears PPU state, so load CHR afterwards. */
  memset(g->ppu.chr, 0, sizeof(g->ppu.chr));
  if (ines->chr && ines->chr_size <= GEM_CHR_SIZE) {
    memcpy(g->ppu.chr, ines->chr, ines->chr_size);
  }
  return 0;
}

int gem_load_game(Gem *g, const char *rom) {
  GemInes ines;
  int rc;

  if (g == NULL) return 2;
  memset(&ines, 0, sizeof(ines));
  if (gem_ines_load(rom, &ines) != 0) {
    fprintf(stderr, "gem6502: %s\n", ines.error);
    gem_ines_free(&ines);
    return 2;
  }
  rc = install_ines(g, &ines);
  gem_ines_free(&ines);
  return rc;
}

int gem_load_game_memory(Gem *g, const uint8_t *data, size_t size) {
  GemInes ines;
  int rc;

  if (g == NULL) return 2;
  memset(&ines, 0, sizeof(ines));
  if (gem_ines_parse(data, size, &ines) != 0) {
    fprintf(stderr, "gem6502: %s\n", ines.error);
    gem_ines_free(&ines);
    return 2;
  }
  rc = install_ines(g, &ines);
  gem_ines_free(&ines);
  return rc;
}
