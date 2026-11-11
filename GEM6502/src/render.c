#include "render.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * Compact NTSC 2C02 palette, 64 entries, in the same order as the NES
 * palette RAM. Entries 0x0D..0x0F and 0x1D..0x1F are the unused black
 * entries. Emphasis and greyscale are intentionally not applied.
 */
static const uint8_t k_nes_palette[64][3] = {
  {0x66, 0x66, 0x66}, {0x00, 0x2A, 0x88}, {0x14, 0x12, 0xA7}, {0x3B, 0x00, 0xA4},
  {0x5C, 0x00, 0x7E}, {0x6E, 0x00, 0x40}, {0x6C, 0x06, 0x00}, {0x56, 0x1D, 0x00},
  {0x33, 0x35, 0x00}, {0x0B, 0x48, 0x00}, {0x00, 0x52, 0x00}, {0x00, 0x4F, 0x08},
  {0x00, 0x40, 0x4D}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},

  {0xAD, 0xAD, 0xAD}, {0x15, 0x5F, 0xD9}, {0x42, 0x40, 0xFF}, {0x75, 0x27, 0xFE},
  {0xA0, 0x1A, 0xCC}, {0xB7, 0x1E, 0x7B}, {0xB5, 0x31, 0x20}, {0x99, 0x4E, 0x00},
  {0x6B, 0x6D, 0x00}, {0x38, 0x87, 0x00}, {0x0C, 0x93, 0x00}, {0x00, 0x8F, 0x32},
  {0x00, 0x7C, 0x8D}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},

  {0xFF, 0xFE, 0xFF}, {0x64, 0xB0, 0xFF}, {0x92, 0x90, 0xFF}, {0xC6, 0x76, 0xFF},
  {0xF3, 0x6A, 0xFF}, {0xFE, 0x6E, 0xCC}, {0xFE, 0x81, 0x70}, {0xEA, 0x9E, 0x22},
  {0xBC, 0xBE, 0x00}, {0x88, 0xD8, 0x00}, {0x5C, 0xE4, 0x30}, {0x45, 0xE0, 0x82},
  {0x48, 0xCD, 0xDE}, {0x4F, 0x4F, 0x4F}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},

  {0xFF, 0xFE, 0xFF}, {0xC4, 0xE0, 0xFF}, {0xD3, 0xD2, 0xFF}, {0xE8, 0xC8, 0xFF},
  {0xFB, 0xC2, 0xFF}, {0xFE, 0xC4, 0xEA}, {0xFE, 0xCC, 0xC5}, {0xF7, 0xD8, 0xA5},
  {0xE4, 0xE5, 0x94}, {0xCF, 0xEF, 0x96}, {0xBD, 0xF4, 0xAB}, {0xB3, 0xF3, 0xCC},
  {0xB5, 0xEB, 0xF2}, {0xB8, 0xB8, 0xB8}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}
};

/* One scanline's worth of a visible sprite, with its pattern already fetched. */
typedef struct GemSpriteLine {
  int x;
  uint8_t attr;
  uint8_t lo;
  uint8_t hi;
} GemSpriteLine;

/*
 * NES palette RAM mirrors $3F10/$3F14/$3F18/$3F1C to $3F00/$3F04/$3F08/$3F0C.
 * The same rule also covers the $3F20-$3FFF read mirrors because callers mask
 * the address to 5 bits before calling this helper.
 */
static inline uint8_t gem_palette_addr(uint8_t idx) {
  idx &= 0x1Fu;
  if ((idx & 0x13u) == 0x10u) idx = (uint8_t)(idx & (uint8_t)~0x10u);
  return idx;
}

static inline uint8_t gem_palette_color(const GemPpu *ppu, uint8_t idx) {
  return (uint8_t)(ppu->pal[gem_palette_addr(idx)] & 0x3Fu);
}

static inline void gem_emit_rgb(uint8_t *dst, uint8_t color_idx) {
  const uint8_t *c = k_nes_palette[color_idx & 0x3Fu];
  dst[0] = c[0];
  dst[1] = c[1];
  dst[2] = c[2];
}

static inline void gem_emit_black(uint8_t *dst) {
  dst[0] = 0;
  dst[1] = 0;
  dst[2] = 0;
}

void gem_render_frame(const Gem *gem, uint8_t *rgb, int width, int height) {
  if (gem == NULL || rgb == NULL || width <= 0 || height <= 0) return;

  const GemPpu *ppu = &gem->ppu;
  const int draw_w = (width < GEM_RENDER_WIDTH) ? width : GEM_RENDER_WIDTH;
  const int draw_h = (height < GEM_RENDER_HEIGHT) ? height : GEM_RENDER_HEIGHT;
  const size_t stride = (size_t)width * 3u;

  if (ppu->framebuffer_valid) {
    for (int y = 0; y < height; y++) for (int x = 0; x < width; x++) {
      uint32_t color = x < draw_w && y < draw_h ? ppu->framebuffer[y * 256 + x] : 0;
      uint8_t *out = rgb + (size_t)y * stride + (size_t)x * 3;
      out[0] = color >> 16; out[1] = color >> 8; out[2] = color;
    }
    return;
  }

  const bool bg_enabled = (ppu->mask & 0x08u) != 0;
  const bool sprites_enabled = (ppu->mask & 0x10u) != 0;
  const int bg_pattern = (ppu->ctrl & 0x10u) ? 0x1000 : 0x0000;
  const int sprite_pattern = (ppu->ctrl & 0x08u) ? 0x1000 : 0x0000;
  const int sprite_height = (ppu->ctrl & 0x20u) ? 16 : 8;
  const uint8_t backdrop = gem_palette_color(ppu, 0);

  for (int y = 0; y < draw_h; ++y) {
    uint8_t *row = rgb + (size_t)y * stride;

    /* Vertical source position (this model: py = y + scroll_y). */
    const int py = y + (int)ppu->scroll_y;
    const int tile_y = (py >> 3) % 30;
    const int fine_y = py & 7;
    const int attr_row = 0x3C0 + ((tile_y >> 2) << 3);
    const int shift_y = (tile_y & 2) << 1;

    /* Cache the current background tile while walking this scanline. */
    int cached_tx = -1;
    uint8_t tile_lo = 0;
    uint8_t tile_hi = 0;
    int tile_pal = 0;

    /* Gather visible sprites once per scanline, in OAM priority order. The
     * first opaque sprite wins, so OAM entry 0 (sprite 0) has top priority.
     * The API takes a const Gem, so the sprite-0 hit status bit is not set. */
    GemSpriteLine sprites[64];
    int sprite_count = 0;
    if (sprites_enabled) {
      for (int i = 0; i < 64; ++i) {
        const uint8_t *s = &ppu->oam[i * 4];
        int sy = (int)s[0] + 1; /* OAM Y is one pixel above the top row. */
        int srow = y - sy;
        if (srow < 0 || srow >= sprite_height) continue;

        const int attr = s[2];
        const int tile = s[1];
        int addr;
        if (sprite_height == 16) {
          /* Bit 0 of the tile selects the pattern table; pairs of tiles. */
          const int table = (tile & 1) ? 0x1000 : 0x0000;
          if (attr & 0x80) srow = sprite_height - 1 - srow;
          const int tile_idx = (tile & 0xFE) + (srow >> 3);
          addr = table + (tile_idx << 4) + (srow & 7);
        } else {
          if (attr & 0x80) srow = sprite_height - 1 - srow;
          addr = sprite_pattern + (tile << 4) + srow;
        }

        GemSpriteLine *dst = &sprites[sprite_count++];
        dst->x = s[3];
        dst->attr = (uint8_t)attr;
        dst->lo = gem_chr_read(gem, addr);
        dst->hi = gem_chr_read(gem, addr + 8);
      }
    }

    for (int x = 0; x < draw_w; ++x) {
      uint8_t color = 0;
      bool has_color = false;
      bool bg_opaque = false;

      if (bg_enabled) {
        /* Horizontal source position (this model: px = x + scroll_x). */
        const int px = x + (int)ppu->scroll_x;
        const int tx = (px >> 3) & 31;
        const int fine_x = px & 7;

        const int nt = (ppu->ctrl & 3) ^ ((px / 256) & 1) ^ (((py / 240) & 1) << 1);
        if (tx != cached_tx) {
          const uint8_t tile = ppu->vram[gem_nt_offset(gem, 0x2000 | (nt << 10) | (tile_y << 5) | tx)];
          const uint8_t attr = ppu->vram[gem_nt_offset(gem, 0x2000 | (nt << 10) | (attr_row + (tx >> 2)))];
          const int shift = shift_y | (tx & 2);
          const int pat_addr = bg_pattern + (tile << 4) + fine_y;
          tile_pal = (attr >> shift) & 3;
          tile_lo = gem_chr_read(gem, pat_addr);
          tile_hi = gem_chr_read(gem, pat_addr + 8);
          cached_tx = tx;
        }

        const int bit = 7 - fine_x;
        const int pix = ((tile_lo >> bit) & 1) | (((tile_hi >> bit) & 1) << 1);
        if (pix == 0) {
          color = backdrop; /* Universal background color. */
        } else {
          color = gem_palette_color(ppu, (uint8_t)((tile_pal << 2) + pix));
          bg_opaque = true;
        }
        has_color = true;
      }

      if (sprite_count > 0) {
        int sprite_color = -1;
        bool in_front = false;

        for (int i = 0; i < sprite_count; ++i) {
          const int col = x - sprites[i].x;
          if (col < 0 || col >= 8) continue;

          const int bit = (sprites[i].attr & 0x40) ? col : (7 - col);
          const int pix = ((sprites[i].lo >> bit) & 1) |
                          (((sprites[i].hi >> bit) & 1) << 1);
          if (pix == 0) continue; /* Color 0 is transparent. */

          sprite_color = (int)gem_palette_color(
              ppu, (uint8_t)(0x10 + ((sprites[i].attr & 3) << 2) + pix));
          in_front = (sprites[i].attr & 0x20) == 0;
          break; /* First opaque sprite in OAM order has priority. */
        }

        if (sprite_color >= 0 && (in_front || !bg_opaque)) {
          color = (uint8_t)sprite_color;
          has_color = true;
        }
      }

      if (has_color) {
        gem_emit_rgb(row + (size_t)x * 3u, color);
      } else {
        gem_emit_black(row + (size_t)x * 3u);
      }
    }

    if (draw_w < width) {
      memset(row + (size_t)draw_w * 3u, 0, (size_t)(width - draw_w) * 3u);
    }
  }

  /* Rows below the NES image (if the caller asked for a taller buffer). */
  for (int y = draw_h; y < height; ++y) {
    memset(rgb + (size_t)y * stride, 0, stride);
  }
}

/* Rasterize with the register and mapper state at the current PPU dot.
 * Raster helpers follow NEMU/src/nes/ppu.c at instruction-level timing. */
static uint32_t raster_rgb(uint8_t index) {
  const uint8_t *c = k_nes_palette[index & 63];
  return ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | c[2];
}
static void background_pixel(Gem *g, int x, int y, uint32_t *out, bool *opaque) {
  GemPpu *p = &g->ppu;
  (void)y;
  *out = raster_rgb(p->pal[0]);
  *opaque = false;
  if (!(p->mask & 0x08)) return;
  if (x < 8 && !(p->mask & 0x02)) return;

  int fine_y = (p->vaddr >> 12) & 7;
  int coarse_y = (p->vaddr >> 5) & 0x1F;
  int nt_base = (p->vaddr >> 10) & 3;
  int coarse_x = p->vaddr & 0x1F;
  if (coarse_y >= 30) coarse_y %= 30;
  int pattern = (p->ctrl & 0x10) ? 0x1000 : 0x0000;

  int scrolled = x + p->fine_x;
  int tx = coarse_x + (scrolled >> 3);
  int nt = (nt_base ^ ((tx >> 5) & 1)) & 3;
  int tile_x = tx & 31;
  uint16_t nt_addr = (uint16_t)(0x2000 | (nt << 10) | (coarse_y << 5) | tile_x);
  uint8_t tile = g->ppu.vram[gem_nt_offset(g, nt_addr)];
  uint16_t attr_addr = (uint16_t)(0x23C0 | (nt << 10) | ((coarse_y >> 2) << 3) | (tile_x >> 2));
  uint8_t attr = g->ppu.vram[gem_nt_offset(g, attr_addr)];
  int shift = ((coarse_y & 2) << 1) | (tile_x & 2);
  int pal = (attr >> shift) & 3;
  uint16_t pat = (uint16_t)(pattern | (tile << 4) | fine_y);
  uint8_t lo = gem_chr_read(g, pat);
  uint8_t hi = gem_chr_read(g, (uint16_t)(pat + 8));
  int bit = 7 - (scrolled & 7);
  int color = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
  if (color != 0) {
    *out = raster_rgb(p->pal[pal * 4 + color] & 0x3F);
    *opaque = true;
  }
}

static void render_sprites(Gem *g, int y, const uint8_t *bg_opaque, uint32_t *line) {
  GemPpu *p = &g->ppu;
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
    uint8_t lo = gem_chr_read(g, pat);
    uint8_t hi = gem_chr_read(g, (uint16_t)(pat + 8));
    for (int col = 0; col < 8; col++) {
      int px = sx + col;
      if (px < 0 || px >= 256) continue;
      if (px < 8 && !(p->mask & 0x04)) continue;
      int bit = (attr & 0x40) ? col : 7 - col;
      int color = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
      if (color == 0) continue;
      if ((attr & 0x20) && bg_opaque[px]) continue;
      uint8_t idx = p->pal[16 + (attr & 3) * 4 + color] & 0x3F;
      line[px] = raster_rgb(idx);
    }
  }
  if (sprite_count > 8) p->status |= 0x20;
}

static bool sprite0_opaque(Gem *g, int x, int y) {
  GemPpu *p = &g->ppu;
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
  uint8_t lo = gem_chr_read(g, pat);
  uint8_t hi = gem_chr_read(g, (uint16_t)(pat + 8));
  int col = x - sx;
  int bit = (attr & 0x40) ? col : 7 - col;
  return (((lo >> bit) & 1) | (((hi >> bit) & 1) << 1)) != 0;
}


void gem_ppu_draw_dot(Gem *g) {
  GemPpu *p = &g->ppu;
  int x = p->cycle - 1, y = p->scanline;
  bool opaque;
  uint32_t color;
  background_pixel(g, x, y, &color, &opaque);
  p->framebuffer[y * 256 + x] = color;
  p->bg_opaque[x] = opaque;
  if (opaque && (p->mask & 0x18) == 0x18 && x != 255 && sprite0_opaque(g, x, y))
    p->status |= 0x40;
  if (x == 255) render_sprites(g, y, p->bg_opaque, &p->framebuffer[y * 256]);
}
