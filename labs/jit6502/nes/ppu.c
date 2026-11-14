#include "nes.h"
#include <string.h>

/* A compact NTSC 2C02 palette.  Emphasis bits are intentionally ignored. */
static const uint32_t ntsc_palette[64] = {
  0x666666, 0x002A88, 0x1412A7, 0x3B00A4, 0x5C007E, 0x6E0040, 0x6C0600, 0x561D00,
  0x333500, 0x0B4800, 0x005200, 0x004F08, 0x00404D, 0x000000, 0x000000, 0x000000,
  0xADADAD, 0x155FD9, 0x4240FF, 0x7527FE, 0xA01ACC, 0xB71E7B, 0xB53120, 0x994E00,
  0x6B6D00, 0x388700, 0x0C9300, 0x008F32, 0x007C8D, 0x000000, 0x000000, 0x000000,
  0xFFFEFF, 0x64B0FF, 0x9290FF, 0xC676FF, 0xF36AFF, 0xFE6ECC, 0xFE8170, 0xEA9E22,
  0xBCBE00, 0x88D800, 0x5CE430, 0x45E082, 0x48CDDE, 0x4F4F4F, 0x000000, 0x000000,
  0xFFFEFF, 0xC4E0FF, 0xD3D2FF, 0xE8C8FF, 0xFBC2FF, 0xFEC4EA, 0xFECCC5, 0xF7D8A5,
  0xE4E594, 0xCFEF96, 0xBDF4AB, 0xB3F3CC, 0xB5EBF2, 0xB8B8B8, 0x000000, 0x000000,
};

uint32_t ppu_palette_rgb(uint8_t idx) {
  return 0xFF000000u | ntsc_palette[idx & 0x3F];
}

static uint8_t ppu_chr(NES_PPU *p, uint16_t addr) {
  if (p->chr_flat != NULL) return p->chr_flat[addr & p->chr_mask];
  return p->chr_read ? p->chr_read(p->bus, addr) : 0;
}

static uint16_t palette_addr(uint16_t addr) {
  addr &= 0x1F;
  if ((addr & 0x13) == 0x10) addr &= (uint16_t)~0x10;
  return addr;
}

static uint8_t ppu_ciram_read(NES_PPU *p, uint16_t addr) {
  int nt = p->nt_index ? p->nt_index(p->bus, addr) : 0;
  return p->ciram[nt & 3][addr & 0x3FF];
}

static void ppu_ciram_write(NES_PPU *p, uint16_t addr, uint8_t value) {
  int nt = p->nt_index ? p->nt_index(p->bus, addr) : 0;
  p->ciram[nt & 3][addr & 0x3FF] = value;
}

static uint8_t ppu_vram_read(NES_PPU *p, uint16_t addr) {
  addr &= 0x3FFF;
  if (addr < 0x2000) return ppu_chr(p, addr);
  if (addr < 0x3F00) return ppu_ciram_read(p, addr);
  return p->palette[palette_addr(addr)];
}

static void ppu_vram_write(NES_PPU *p, uint16_t addr, uint8_t value) {
  addr &= 0x3FFF;
  if (addr < 0x2000) {
    if (p->chr_write) p->chr_write(p->bus, addr, value);
  } else if (addr < 0x3F00) {
    ppu_ciram_write(p, addr, value);
  } else {
    p->palette[palette_addr(addr)] = value & 0x3F;
  }
}

static void inc_v(NES_PPU *p) {
  if ((p->v & 0x7000) != 0x7000) {
    p->v += 0x1000;
  } else {
    p->v &= (uint16_t)~0x7000;
    int y = (p->v >> 5) & 0x1F;
    if (y == 29) {
      y = 0;
      p->v ^= 0x0800;
    } else if (y == 31) {
      y = 0;
    } else {
      y++;
    }
    p->v = (uint16_t)((p->v & ~0x03E0) | (y << 5));
  }
}

static uint32_t backdrop_rgb(NES_PPU *p) {
  return ppu_palette_rgb(p->palette[0] & 0x3F);
}

/* One-entry cache for the background tile fetch.  Within a visible scanline
 * the same tile is read for 8 consecutive dots, and the PPU is stepped in
 * short chunks, so without this cache the tile/attribute/pattern reads happen
 * 8x more often than the hardware performs them. */
typedef struct BgTileCache {
  int tx;
  uint8_t lo, hi;
  int pal;
} BgTileCache;

static void background_pixel(NES_PPU *p, int x, int y, BgTileCache *tc,
                             uint32_t *out, bool *opaque) {
  (void)y;
  *out = backdrop_rgb(p);
  *opaque = false;
  if (!(p->mask & 0x08)) return;
  if (x < 8 && !(p->mask & 0x02)) return;

  int coarse_y = (p->v >> 5) & 0x1F;
  if (coarse_y >= 30) coarse_y %= 30;
  int coarse_x = p->v & 0x1F;

  int scrolled = x + p->fine_x;
  int tx = coarse_x + (scrolled >> 3);
  if (tx != tc->tx) {
    int fine_y = (p->v >> 12) & 7;
    int nt_base = (p->v >> 10) & 3;
    int nt = (nt_base ^ ((tx >> 5) & 1)) & 3;
    int tile_x = tx & 31;
    uint16_t nt_addr = (uint16_t)(0x2000 | (nt << 10) | (coarse_y << 5) | tile_x);
    uint8_t tile = ppu_ciram_read(p, nt_addr);
    uint16_t attr_addr = (uint16_t)(0x23C0 | (nt << 10) | ((coarse_y >> 2) << 3) | (tile_x >> 2));
    uint8_t attr = ppu_ciram_read(p, attr_addr);
    int shift = ((coarse_y & 2) << 1) | (tile_x & 2);
    int pattern = (p->ctrl & 0x10) ? 0x1000 : 0x0000;
    uint16_t pat = (uint16_t)(pattern | (tile << 4) | fine_y);
    tc->lo = ppu_chr(p, pat);
    tc->hi = ppu_chr(p, (uint16_t)(pat + 8));
    tc->pal = (attr >> shift) & 3;
    tc->tx = tx;
  }
  int bit = 7 - (scrolled & 7);
  int color = ((tc->lo >> bit) & 1) | (((tc->hi >> bit) & 1) << 1);
  if (color != 0) {
    *out = ppu_palette_rgb(p->palette[tc->pal * 4 + color] & 0x3F);
    *opaque = true;
  }
}

static void render_sprites(NES_PPU *p, int y, const uint8_t *bg_opaque, uint32_t *line) {
  if (!(p->mask & 0x10)) return;
  int height = (p->ctrl & 0x20) ? 16 : 8;
  int sprite_table = (p->ctrl & 0x08) ? 0x1000 : 0x0000;
  int sprite_count = 0;

  /* Lower OAM indices have priority, so iterate backwards while drawing. */
  for (int i = 63; i >= 0; i--) {
    int sy = p->oam[i * 4 + 0] + 1;
    int tile = p->oam[i * 4 + 1];
    int attr = p->oam[i * 4 + 2];
    int sx = p->oam[i * 4 + 3];
    if (y < sy || y >= sy + height) continue;
    sprite_count++;
    int row = y - sy;
    if (attr & 0x80) row = height - 1 - row;
    int table = sprite_table;
    if (height == 16) {
      table = (tile & 1) ? 0x1000 : 0x0000;
      tile &= 0xFE;
      if (row >= 8) { row -= 8; tile++; }
    }
    uint16_t pat = (uint16_t)(table | (tile << 4) | (row & 7));
    uint8_t lo = ppu_chr(p, pat);
    uint8_t hi = ppu_chr(p, (uint16_t)(pat + 8));
    for (int col = 0; col < 8; col++) {
      int px = sx + col;
      if (px < 0 || px >= NES_SCREEN_W) continue;
      if (px < 8 && !(p->mask & 0x04)) continue;
      int bit = (attr & 0x40) ? col : 7 - col;
      int color = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
      if (color == 0) continue;
      if ((attr & 0x20) && bg_opaque[px]) continue;
      uint8_t idx = p->palette[16 + (attr & 3) * 4 + color] & 0x3F;
      line[px] = ppu_palette_rgb(idx);
    }
  }
  if (sprite_count > 8) p->status |= 0x20;
}

static bool sprite0_opaque(NES_PPU *p, int x, int y) {
  if (!(p->mask & 0x10)) return false;
  int height = (p->ctrl & 0x20) ? 16 : 8;
  int sx = p->oam[3];
  int sy = p->oam[0] + 1;
  if (x < sx || x >= sx + 8 || y < sy || y >= sy + height) return false;
  if (x < 8 && !(p->mask & 0x04)) return false;
  int tile = p->oam[1];
  int attr = p->oam[2];
  int row = y - sy;
  if (attr & 0x80) row = height - 1 - row;
  int table = (p->ctrl & 0x08) ? 0x1000 : 0x0000;
  if (height == 16) {
    table = (tile & 1) ? 0x1000 : 0x0000;
    tile &= 0xFE;
    if (row >= 8) { row -= 8; tile++; }
  }
  uint16_t pat = (uint16_t)(table | (tile << 4) | (row & 7));
  uint8_t lo = ppu_chr(p, pat);
  uint8_t hi = ppu_chr(p, (uint16_t)(pat + 8));
  int col = x - sx;
  int bit = (attr & 0x40) ? col : 7 - col;
  return (((lo >> bit) & 1) | (((hi >> bit) & 1) << 1)) != 0;
}

void ppu_set_bus(NES_PPU *p, void *bus,
    uint8_t (*chr_read)(void *, uint16_t),
    void (*chr_write)(void *, uint16_t, uint8_t),
    int (*nt_index)(void *, uint16_t),
    void (*on_scanline)(void *)) {
  p->bus = bus;
  p->chr_read = chr_read;
  p->chr_write = chr_write;
  p->nt_index = nt_index;
  p->on_scanline = on_scanline;
}

void ppu_reset(NES_PPU *p, NES_Cart *c) {
  memset(p, 0, sizeof(*p));
  p->cart = c;
  p->scanline = 0;
  p->cycle = 0;
  p->palette[0] = 0x0F;
  p->status = 0;
  p->frame_ready = false;
}

/* Draw dots [x0, x1) of one visible scanline: background pixel plus the
 * sprite-0 hit flag. */
static void render_bg_run(NES_PPU *p, int s, int x0, int x1, BgTileCache *tc) {
  uint32_t *line = &p->framebuffer[s * NES_SCREEN_W];
  const bool hit_check = (p->mask & 0x18) == 0x18;
  for (int x = x0; x < x1; x++) {
    uint32_t color;
    bool opaque;
    background_pixel(p, x, s, tc, &color, &opaque);
    line[x] = color;
    p->bg_opaque[x] = opaque;
    if (opaque && hit_check && sprite0_opaque(p, x, s)) p->status |= 0x40;
  }
}

void ppu_step(NES_PPU *p, int ppu_cycles) {
  /* 240 of the 262 scanlines are visible and contribute 61k pixels per frame,
   * so they are drawn in runs; every event dot (0/1/256/257/339) is handled
   * explicitly so a run never steps over it. */
  BgTileCache tc = {-1, 0, 0, 0};
  int n = ppu_cycles;

  while (n > 0) {
    const int s = p->scanline;

    if (s < 240) {
      const int c = p->cycle;
      if (c == 0) {
        if (p->on_scanline) p->on_scanline(p->bus);
        tc.tx = -1; /* new scanline: row/pattern base changed */
        p->cycle = 1;
        n--;
        continue;
      }
      if (c <= 255) {
        int batch = 256 - c; /* dots c..255 */
        if (batch > n) batch = n;
        render_bg_run(p, s, c - 1, c - 1 + batch, &tc);
        p->cycle = c + batch;
        n -= batch;
        continue;
      }
      if (c == 256) {
        render_bg_run(p, s, 255, 256, &tc);
        render_sprites(p, s, p->bg_opaque, &p->framebuffer[s * NES_SCREEN_W]);
        if (p->mask & 0x18) {
          inc_v(p);
          tc.tx = -1; /* v moved: the cached tile belongs to the old row */
        }
        p->cycle = 257;
        n--;
        continue;
      }
      if (c == 257) {
        if (p->mask & 0x18) {
          p->v = (uint16_t)((p->v & ~0x041F) | (p->t & 0x041F));
          tc.tx = -1;
        }
        p->cycle = 258;
        n--;
        continue;
      }
      int batch = 341 - c; /* dots 258..340 are idle */
      if (batch > n) batch = n;
      p->cycle = c + batch;
      n -= batch;
      if (p->cycle >= 341) {
        p->cycle = 0;
        if (++p->scanline >= 262) p->scanline = 0;
      }
      continue;
    }

    /* Vblank and pre-render lines: three event dots, everything else idle. */
    const int c = p->cycle;
    if (s == 241 || s == 261) {
      if (c == 0) {
        p->cycle = 1;
        n--;
        continue;
      }
      if (c == 1) {
        if (s == 241) {
          p->status |= 0x80;
          if (p->ctrl & 0x80) {
            p->nmi_line = true;
            p->nmi_trigger = true;
          }
        } else {
          p->status &= (uint8_t)~0xE0;
          p->nmi_line = false;
          if (p->mask & 0x18) p->v = p->t;
          p->frame_ready = true;
          p->odd_frame = !p->odd_frame;
          tc.tx = -1;
        }
        p->cycle = 2;
        n--;
        continue;
      }
      if (s == 261 && c == 339) {
        /* Odd frames skip PPU dot 339 of the pre-render scanline when
         * rendering is enabled. */
        if (p->odd_frame && (p->mask & 0x18)) {
          p->cycle = 0;
          p->scanline = 0;
        } else {
          p->cycle = 340;
        }
        n--;
        continue;
      }
    }

    /* Never step over the pre-render odd-frame skip at dot 339. */
    int stop = (s == 261 && p->cycle < 339) ? 339 : 341;
    int batch = stop - p->cycle;
    if (batch > n) batch = n;
    p->cycle += batch;
    n -= batch;
    if (p->cycle >= 341) {
      p->cycle = 0;
      if (++p->scanline >= 262) p->scanline = 0;
    }
  }
}

uint8_t ppu_read_reg(NES_PPU *p, uint16_t addr) {
  switch (addr & 7) {
    case 2: {
      uint8_t ret = p->status;
      p->status &= (uint8_t)~0x80;
      p->w = 0;
      return ret;
    }
    case 4:
      return p->oam[p->oam_addr];
    case 7: {
      uint16_t a = p->v & 0x3FFF;
      uint8_t ret;
      if (a >= 0x3F00) {
        ret = p->palette[palette_addr(a)];
      } else {
        ret = (uint8_t)p->read_buffer;
        p->read_buffer = ppu_vram_read(p, a);
      }
      p->v = (uint16_t)(p->v + ((p->ctrl & 0x04) ? 32 : 1));
      return ret;
    }
    default:
      return 0;
  }
}

void ppu_write_reg(NES_PPU *p, uint16_t addr, uint8_t value) {
  switch (addr & 7) {
    case 0:
      p->ctrl = value;
      p->t = (uint16_t)((p->t & ~0x0C00) | ((value & 3) << 10));
      if ((value & 0x80) && (p->status & 0x80) && !p->nmi_line) {
        p->nmi_line = true;
        p->nmi_trigger = true;
      }
      break;
    case 1:
      p->mask = value;
      break;
    case 3:
      p->oam_addr = value;
      break;
    case 4:
      p->oam[p->oam_addr] = value;
      p->oam_addr++;
      break;
    case 5:
      if (!p->w) {
        p->t = (uint16_t)((p->t & ~0x001F) | (value >> 3));
        p->fine_x = value & 7;
        p->w = 1;
      } else {
        p->t = (uint16_t)((p->t & ~0x73E0) | ((value & 0xF8) << 2) | ((value & 7) << 12));
        p->w = 0;
      }
      break;
    case 6:
      if (!p->w) {
        p->t = (uint16_t)((p->t & 0x00FF) | ((value & 0x3F) << 8));
        p->w = 1;
      } else {
        p->t = (uint16_t)((p->t & 0x7F00) | value);
        p->v = p->t;
        p->w = 0;
      }
      break;
    case 7:
      ppu_vram_write(p, p->v, value);
      p->v = (uint16_t)(p->v + ((p->ctrl & 0x04) ? 32 : 1));
      break;
    default:
      break;
  }
}

void ppu_oam_dma(NES_PPU *p, const uint8_t *src) {
  for (int i = 0; i < 256; i++) p->oam[(p->oam_addr + i) & 0xFF] = src[i];
}

uint8_t ppu_read_oam(NES_PPU *p) {
  return p->oam[p->oam_addr];
}

void ppu_write_oam(NES_PPU *p, uint8_t value) {
  p->oam[p->oam_addr++] = value;
}
