/*
 * Synthetic renderer test. No copyrighted ROM data is used.
 *
 * Build and run from GEM6502/:
 *   cc -std=c11 -O2 -Wall -Wextra -Iinclude src/render.c src/gem6502.c \
 *      tests/test_render.c -o /tmp/test_render
 *   /tmp/test_render
 */
#include "render.h"

#include "gem6502.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void expect(int cond, const char *what) {
  if (cond) {
    printf("PASS: %s\n", what);
  } else {
    printf("FAIL: %s\n", what);
    failures++;
  }
}

static int pixel_eq(const uint8_t *rgb, int w, int x, int y,
                    uint8_t r, uint8_t g, uint8_t b) {
  const uint8_t *p = rgb + ((size_t)y * (size_t)w + (size_t)x) * 3u;
  return p[0] == r && p[1] == g && p[2] == b;
}

static int pixel_black(const uint8_t *rgb, int w, int x, int y) {
  return pixel_eq(rgb, w, x, y, 0, 0, 0);
}

static void clear_gem(Gem *g) {
  memset(g, 0, sizeof(*g));
  g->ppu.pal[0] = 0x0F; /* Black universal background color. */
}

/* Fill one 8x8 tile with color 1, 2 or 3 in a 2bpp pattern table. */
static void fill_tile(Gem *g, int pattern_base, int tile, int color) {
  const int addr = pattern_base + tile * 16;
  for (int row = 0; row < 8; ++row) {
    g->ppu.chr[addr + row] = (color & 1) ? 0xFF : 0x00;
    g->ppu.chr[addr + 8 + row] = (color & 2) ? 0xFF : 0x00;
  }
}

static void render(const Gem *g, uint8_t *rgb) {
  memset(rgb, 0xAA, (size_t)GEM_RENDER_WIDTH * GEM_RENDER_HEIGHT * 3u);
  gem_render_frame(g, rgb, GEM_RENDER_WIDTH, GEM_RENDER_HEIGHT);
}

int main(void) {
  static uint8_t rgb[(size_t)GEM_RENDER_WIDTH * GEM_RENDER_HEIGHT * 3u];
  Gem g;

  /* 1. Required synthetic case: one tile, one nametable entry, one palette
   *    entry; the tile must produce a non-black pixel. */
  clear_gem(&g);
  g.ppu.mask = 0x08; /* Background enable. */
  g.ppu.ctrl = 0x00; /* Background pattern table $0000. */
  g.ppu.vram[0] = 0; /* Nametable 0 top-left entry -> tile 0. */
  g.ppu.pal[1] = 0x30;
  fill_tile(&g, 0x0000, 0, 1);
  render(&g, rgb);
  expect(pixel_eq(rgb, GEM_RENDER_WIDTH, 16, 16, 0xFF, 0xFE, 0xFF),
         "background tile renders non-black palette $30 pixel");

  /* 2. Attribute-table palette selection: top-left 32x32 block -> palette 1. */
  clear_gem(&g);
  g.ppu.mask = 0x08;
  g.ppu.vram[0] = 0;
  g.ppu.vram[0x3C0] = 0x01;
  g.ppu.pal[5] = 0x16;
  fill_tile(&g, 0x0000, 0, 1);
  render(&g, rgb);
  /* Attribute byte 0 covers tiles 0-3; palette 1 is selected in its
   * top-left 2x2 tile quadrant, i.e. pixels 0-15. */
  expect(pixel_eq(rgb, GEM_RENDER_WIDTH, 0, 0, 0xB5, 0x31, 0x20) &&
             pixel_eq(rgb, GEM_RENDER_WIDTH, 8, 8, 0xB5, 0x31, 0x20),
         "attribute bits select background palette 1");

  /* 3. ctrl bit 4 selects the $1000 background pattern table. */
  clear_gem(&g);
  g.ppu.mask = 0x08;
  g.ppu.ctrl = 0x10;
  g.ppu.vram[0] = 0;
  g.ppu.pal[1] = 0x30;
  fill_tile(&g, 0x1000, 0, 1);
  render(&g, rgb);
  expect(pixel_eq(rgb, GEM_RENDER_WIDTH, 16, 16, 0xFF, 0xFE, 0xFF),
         "ctrl bit 4 selects the $1000 background pattern table");

  /* 4. Disabled background must not draw, even with a visible backdrop. */
  clear_gem(&g);
  g.ppu.mask = 0x00;
  g.ppu.pal[0] = 0x30;
  g.ppu.vram[0] = 0;
  fill_tile(&g, 0x0000, 0, 1);
  render(&g, rgb);
  expect(pixel_black(rgb, GEM_RENDER_WIDTH, 16, 16),
         "mask bit 3 off suppresses background output");

  /* 5. Scroll model: px = x + scroll_x. */
  clear_gem(&g);
  g.ppu.mask = 0x08;
  g.ppu.scroll_x = 8;
  g.ppu.vram[0] = 0;
  g.ppu.vram[1] = 1;
  g.ppu.vram[2] = 2; /* Empty tile, so x=8 shows the black backdrop. */
  g.ppu.pal[1] = 0x30;
  g.ppu.pal[2] = 0x16;
  fill_tile(&g, 0x0000, 0, 1); /* White. */
  fill_tile(&g, 0x0000, 1, 2); /* Red. */
  render(&g, rgb);
  expect(pixel_eq(rgb, GEM_RENDER_WIDTH, 0, 0, 0xB5, 0x31, 0x20) &&
             pixel_black(rgb, GEM_RENDER_WIDTH, 8, 0),
         "horizontal scroll shifts by scroll_x pixels");

  /* 6. Sprite 0: OAM Y is top-1 and sprite palette color is used. */
  clear_gem(&g);
  g.ppu.mask = 0x10;
  g.ppu.oam[0] = 15; /* Top row is y=16. */
  g.ppu.oam[1] = 1;
  g.ppu.oam[2] = 0x00;
  g.ppu.oam[3] = 20;
  g.ppu.pal[0x11] = 0x16;
  fill_tile(&g, 0x0000, 1, 1);
  render(&g, rgb);
  expect(pixel_eq(rgb, GEM_RENDER_WIDTH, 20, 16, 0xB5, 0x31, 0x20) &&
             pixel_black(rgb, GEM_RENDER_WIDTH, 20, 15) &&
             pixel_black(rgb, GEM_RENDER_WIDTH, 0, 0),
         "sprite renders at OAM Y+1 with sprite palette");

  /* 7. Sprite priority over/under opaque background. */
  clear_gem(&g);
  g.ppu.mask = 0x18; /* Background + sprites. */
  g.ppu.vram[0] = 0;
  g.ppu.oam[0] = 15;
  g.ppu.oam[1] = 1;
  g.ppu.oam[2] = 0x00; /* In front of background. */
  g.ppu.oam[3] = 20;
  g.ppu.pal[1] = 0x30;
  g.ppu.pal[0x11] = 0x16;
  fill_tile(&g, 0x0000, 0, 1); /* White background. */
  fill_tile(&g, 0x0000, 1, 1); /* Red sprite. */
  render(&g, rgb);
  expect(pixel_eq(rgb, GEM_RENDER_WIDTH, 20, 16, 0xB5, 0x31, 0x20),
         "front-priority sprite draws over opaque background");

  g.ppu.oam[2] = 0x20; /* Behind background. */
  render(&g, rgb);
  expect(pixel_eq(rgb, GEM_RENDER_WIDTH, 20, 16, 0xFF, 0xFE, 0xFF),
         "behind-priority sprite is hidden by opaque background");

  /* 8. 8x16 sprites: tile bit 0 picks the table, tile pair forms 16 rows. */
  clear_gem(&g);
  g.ppu.mask = 0x10;
  g.ppu.ctrl = 0x20;
  g.ppu.oam[0] = 15;
  g.ppu.oam[1] = 1; /* Table $1000, tiles 0 (top) and 1 (bottom). */
  g.ppu.oam[2] = 0x00;
  g.ppu.oam[3] = 20;
  g.ppu.pal[0x11] = 0x30;
  g.ppu.pal[0x12] = 0x16;
  fill_tile(&g, 0x1000, 0, 1);
  fill_tile(&g, 0x1000, 1, 2);
  render(&g, rgb);
  expect(pixel_eq(rgb, GEM_RENDER_WIDTH, 20, 16, 0xFF, 0xFE, 0xFF) &&
             pixel_eq(rgb, GEM_RENDER_WIDTH, 20, 24, 0xB5, 0x31, 0x20),
         "8x16 sprite draws both tiles from the selected table");

  /* 9. Horizontal flip: a left-column pixel moves to the right edge. */
  clear_gem(&g);
  g.ppu.mask = 0x10;
  g.ppu.oam[0] = 15;
  g.ppu.oam[1] = 1;
  g.ppu.oam[2] = 0x40;
  g.ppu.oam[3] = 20;
  g.ppu.pal[0x11] = 0x30;
  for (int row = 0; row < 8; ++row) g.ppu.chr[16 + row] = 0x80;
  render(&g, rgb);
  expect(pixel_black(rgb, GEM_RENDER_WIDTH, 20, 16) &&
             pixel_eq(rgb, GEM_RENDER_WIDTH, 27, 16, 0xFF, 0xFE, 0xFF),
         "horizontal sprite flip mirrors tile pixels");

  /* 10. Vertical flip: a top-row pixel moves to the bottom edge. */
  clear_gem(&g);
  g.ppu.mask = 0x10;
  g.ppu.oam[0] = 15;
  g.ppu.oam[1] = 1;
  g.ppu.oam[2] = 0x80;
  g.ppu.oam[3] = 20;
  g.ppu.pal[0x11] = 0x30;
  g.ppu.chr[16] = 0xFF;
  render(&g, rgb);
  expect(pixel_black(rgb, GEM_RENDER_WIDTH, 20, 16) &&
             pixel_eq(rgb, GEM_RENDER_WIDTH, 20, 23, 0xFF, 0xFE, 0xFF),
         "vertical sprite flip mirrors tile rows");

  /* Scroll across a horizontal nametable boundary. */
  clear_gem(&g);
  g.bus.vertical_mirroring = true;
  g.ppu.mask = 0x0A;
  g.ppu.scroll_x = 252;
  g.ppu.pal[1] = 0x30; g.ppu.pal[2] = 0x16;
  fill_tile(&g, 0, 1, 1); fill_tile(&g, 0, 2, 2);
  memset(g.ppu.vram, 1, 960); memset(g.ppu.vram + 1024, 2, 960);
  render(&g, rgb);
  expect(pixel_eq(rgb, 256, 0, 0, 255, 254, 255) &&
         pixel_eq(rgb, 256, 4, 0, 181, 49, 32),
         "horizontal scrolling switches nametables at the screen edge");

  /* A bank or palette write after a pixel is scanned must not repaint it. */
  clear_gem(&g);
  g.ppu.mask = 0x0A;
  g.ppu.pal[1] = 0x30;
  fill_tile(&g, 0, 0, 1);
  g.ppu.scanline = 0; g.ppu.cycle = 1;
  gem_ppu_advance(&g, 1); /* x=0..2 white */
  gem_bus_write(&g, 0x2006, 0x3F); gem_bus_write(&g, 0x2006, 1);
  gem_bus_write(&g, 0x2007, 0x16);
  g.ppu.vaddr = 0;
  gem_ppu_advance(&g, 1); /* x=3..5 red */
  g.ppu.framebuffer_valid = true;
  render(&g, rgb);
  expect(pixel_eq(rgb, 256, 0, 0, 255, 254, 255) &&
         pixel_eq(rgb, 256, 3, 0, 181, 49, 32),
         "raster output preserves palette changes within a frame");

  clear_gem(&g);
  g.ppu.mask = 8;
  g.ppu.vaddr = 0x7000 | (29 << 5);
  g.ppu.scanline = 0; g.ppu.cycle = 256;
  gem_ppu_advance(&g, 1);
  expect((g.ppu.vaddr & 0x7BE0) == 0x0800,
         "fine Y overflow toggles vertical nametable at coarse row 29");
  gem_bus_write(&g, 0x2005, 0xFD); gem_bus_write(&g, 0x2005, 0xEE);
  expect(g.ppu.fine_x == 5 && (g.ppu.taddr & 31) == 31 &&
         ((g.ppu.taddr >> 12) & 7) == 6 && ((g.ppu.taddr >> 5) & 31) == 29,
         "scroll writes latch coarse and fine coordinates separately");

  if (failures == 0) {
    printf("ALL GEM6502 RENDER TESTS PASS\n");
    return 0;
  }
  printf("%d GEM6502 RENDER TEST(S) FAILED\n", failures);
  return 1;
}
