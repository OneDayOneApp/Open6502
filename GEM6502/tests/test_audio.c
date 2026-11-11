#include "gem6502.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void measure(Gem *g, int cycles, int *samples, int *changes, int *rising) {
  int16_t pcm[2048], last = 0;
  *samples = *changes = *rising = 0;
  while (cycles > 0) {
    int step = cycles > 10000 ? 10000 : cycles;
    gem_apu_advance(g, step);
    int n = gem_sound_take_samples(&g->apu.sound, pcm, 2048);
    for (int i = 0; i < n; i++) {
      if (pcm[i] != last) (*changes)++;
      if (pcm[i] > 0 && last <= 0) (*rising)++;
      last = pcm[i];
    }
    *samples += n;
    cycles -= step;
  }
}

int main(void) {
  Gem *g = gem_create();
  assert(g);
  int count, changes, rising;
  measure(g, GEM_CPU_HZ, &count, &changes, &rising);
  assert(count == GEM_AUDIO_HZ && changes == 0);
  gem_bus_write(g, 0x4015, 1);
  gem_bus_write(g, 0x4000, 0xBF); /* 50 percent duty, constant volume, length halt */
  gem_bus_write(g, 0x4002, 253); /* approximately 440 Hz */
  gem_bus_write(g, 0x4003, 0);
  measure(g, GEM_CPU_HZ, &count, &changes, &rising);
  assert(count == GEM_AUDIO_HZ && changes > 1000);
  assert(rising >= 438 && rising <= 443);
  gem_bus_write(g, 0x4015, 0);
  measure(g, GEM_CPU_HZ, &count, &changes, &rising); /* filter settles */
  measure(g, GEM_CPU_HZ, &count, &changes, &rising);
  assert(changes == 0);
  gem_reset(g);
  gem_bus_write(g, 0x4015, 0x0C);
  gem_bus_write(g, 0x4008, 0xFF);
  gem_bus_write(g, 0x400A, 126);
  gem_bus_write(g, 0x400B, 0);
  gem_bus_write(g, 0x400C, 0x3F);
  gem_bus_write(g, 0x400E, 4);
  gem_bus_write(g, 0x400F, 0);
  measure(g, GEM_CPU_HZ, &count, &changes, &rising);
  assert(changes > 1000);
  puts("PASS: audio silence, 44100 Hz sample rate, 440 Hz pulse, triangle/noise and disable");
  for (int game = 0; game < 2; game++) {
    assert(gem_load_game(g, game ? "../roms/stallar.nes" : "../roms/file-fixers.nes") == 0);
    int varying = 0;
    int16_t pcm[2048];
    for (int frame = 0; frame < 240; frame++) {
      uint64_t next = g->stats.ppu_frames + 1;
      g->apu.joy1 = frame >= 120 && frame < 180 ? 8 : 0;
      while (!g->cpu.halted && g->stats.ppu_frames < next) gem_step(g);
      assert(!g->cpu.halted);
      int n = gem_sound_take_samples(&g->apu.sound, pcm, 2048);
      for (int i = 1; i < n; i++) if (abs(pcm[i] - pcm[i-1]) > 10) varying++;
    }
    assert(varying > 1000);
    printf("PASS: %s gameplay produces varying PCM (%d transitions)\n", game ? "Stallar" : "File Fixers", varying);
  }
  gem_destroy(g);
}
