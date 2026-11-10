#include "nes/ref/nes_ref.h"

static void clock_params(nes_ref_region region, uint32_t *cpu_div, uint32_t *ppu_div) {
  if (region == NES_REF_PAL) {
    *cpu_div = 16;
    *ppu_div = 5;
  } else {
    *cpu_div = 12;
    *ppu_div = 4;
  }
}

void nes_ref_clock_init(nes_ref_clock *c, nes_ref_region region) {
  *c = (nes_ref_clock){0};
  c->region = region;
}

void nes_ref_clock_tick(nes_ref_clock *c, uint64_t ticks) {
  uint32_t cpu_div, ppu_div;
  clock_params(c->region, &cpu_div, &ppu_div);
  for (uint64_t i = 0; i < ticks; i++) {
    c->master_tick++;
    if (++c->cpu_phase >= cpu_div) {
      c->cpu_phase = 0;
      c->cpu_cycle++;
    }
    if (++c->ppu_phase >= ppu_div) {
      c->ppu_phase = 0;
      c->ppu_dot++;
      if (++c->ppu_cycle >= 341) {
        c->ppu_cycle = 0;
        if (++c->ppu_scanline >= (c->region == NES_REF_PAL ? 312u : 262u)) {
          c->ppu_scanline = 0;
          c->frame++;
        }
      }
    }
  }
}
