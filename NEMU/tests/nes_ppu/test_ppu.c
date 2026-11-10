#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "nes.h"

static int step_until_frame(NES_PPU *p, int max_dots) {
  for (int i = 0; i < max_dots; i++) {
    ppu_step(p, 1);
    if (p->frame_ready) return i + 1;
  }
  return -1;
}

static void test_reset(void) {
  NES_PPU p;
  NES_Cart c;
  memset(&c, 0, sizeof(c));
  ppu_reset(&p, &c);
  assert(p.scanline == 0);
  assert(p.cycle == 0);
  assert(p.status == 0);
  assert(p.frame_ready == false);
  assert(p.odd_frame == false);
}

static void test_vblank_nmi(void) {
  NES_PPU p;
  NES_Cart c;
  memset(&c, 0, sizeof(c));
  ppu_reset(&p, &c);
  p.ctrl = 0x80;

  int i;
  for (i = 0; i < 241 * 341 + 4 && !(p.status & 0x80); i++) {
    ppu_step(&p, 1);
  }
  assert(i < 241 * 341 + 4);
  assert(p.status & 0x80);
  assert(p.nmi_line);
  assert(p.nmi_trigger);

  /* Reading status clears vblank and the write latch. */
  p.w = 1;
  uint8_t status = ppu_read_reg(&p, 0x2002);
  assert(status & 0x80);
  assert(!(p.status & 0x80));
  assert(p.w == 0);
  assert(p.nmi_line);
}

static void test_frame_boundary(void) {
  NES_PPU p;
  NES_Cart c;
  memset(&c, 0, sizeof(c));
  ppu_reset(&p, &c);

  int dots = step_until_frame(&p, 341 * 262 + 4);
  assert(dots == 341 * 261 + 2);
  assert(p.frame_ready);
  assert(p.scanline == 261);
  assert(p.cycle == 2);
  assert(!(p.status & 0x80));

  p.frame_ready = false;
  int dots2 = step_until_frame(&p, 341 * 262 + 4);
  assert(dots2 == 341 * 262);
  assert(p.frame_ready);
}

static void test_odd_frame_skip(void) {
  NES_PPU p;
  NES_Cart c;
  memset(&c, 0, sizeof(c));
  ppu_reset(&p, &c);
  p.mask = 0x18; /* rendering enabled */

  int warm = step_until_frame(&p, 341 * 262 + 4);
  assert(warm == 341 * 261 + 2);

  p.frame_ready = false;
  int a = step_until_frame(&p, 341 * 262 + 4);
  p.frame_ready = false;
  int b = step_until_frame(&p, 341 * 262 + 4);
  assert(a != b); /* one of the two intervals is an odd frame */
  assert((a < b ? a : b) == 341 * 262 - 1);
  assert((a > b ? a : b) == 341 * 262);

  /* Rendering disabled: no odd-frame skip. */
  ppu_reset(&p, &c);
  p.mask = 0;
  int warm2 = step_until_frame(&p, 341 * 262 + 4);
  assert(warm2 == 341 * 261 + 2);
  p.frame_ready = false;
  int x = step_until_frame(&p, 341 * 262 + 4);
  p.frame_ready = false;
  int y = step_until_frame(&p, 341 * 262 + 4);
  assert(x == y);
  assert(x == 341 * 262);
}

int main(void) {
  test_reset();
  test_vblank_nmi();
  test_frame_boundary();
  test_odd_frame_skip();
  puts("PASS: PPU timing");
  return 0;
}
