#include "gem6502.h"
#include "ines.h"
#include "render.h"
#include "frontend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0) {
  fprintf(stderr,
    "GEM6502 %s - fast 6502/NES SoC cycle model\n"
    "usage:\n"
    "  %s run <prg.bin> [max_cycles] [--trace]\n"
    "  %s stats <prg.bin>\n"
    "  %s game <rom.nes> [frames] [out.ppm] [--start] [--replay-input=F]\n"
    "  %s play <rom.nes>\n"
    "\n"
    "run: raw PRG at $8000, reset vector from $FFFC/$FFFD.\n"
    "game: iNES mapper 0/1/2/3/4/7 ROM, renders a 256x240 PPM frame.\n",
    GEM6502_VERSION, argv0, argv0, argv0, argv0);
}

static int write_ppm(const char *path, const uint8_t *rgb) {
  FILE *f = fopen(path, "wb");
  if (!f) { perror(path); return -1; }
  fprintf(f, "P6\n256 240\n255\n");
  fwrite(rgb, 1, 256u * 240u * 3u, f);
  fclose(f);
  return 0;
}

static int run_game(Gem *g, const char *rom, int frames, const char *out, bool press_start, const char *replay) {
  if (gem_load_game(g, rom) != 0) return 2;

  uint8_t *rgb = (uint8_t *)malloc(256u * 240u * 3u);
  if (!rgb) return 2;
  FILE *input = NULL;
  if (replay) {
    input = fopen(replay, "rb");
    char magic[8];
    if (!input || fread(magic, 1, 8, input) != 8 || memcmp(magic, "NESINP01", 8)) {
      fprintf(stderr, "gem6502: invalid input replay\n");
      if (input) fclose(input);
      free(rgb); return 2;
    }
  }
  uint64_t input_frame = UINT64_MAX;
  uint64_t start_cycles = g->cpu.cycles;
  int target = frames > 0 ? frames : 60;
  while (!g->cpu.halted && g->stats.ppu_frames < (uint64_t)target) {
    if (input && input_frame != g->stats.ppu_frames) {
      int p1 = fgetc(input), p2 = fgetc(input);
      g->apu.joy1 = p1 == EOF ? 0 : (uint8_t)p1;
      g->apu.joy2 = p2 == EOF ? 0 : (uint8_t)p2;
      input_frame = g->stats.ppu_frames;
    }
    if (press_start && g->stats.ppu_frames == 5) g->apu.joy1 = 0x08;
    gem_step(g);
  }
  if (input) fclose(input);
  gem_render_frame(g, rgb, 256, 240);
  if (out) {
    if (write_ppm(out, rgb) != 0) {
      free(rgb); return 2;
    }
  }
  gem_print_stats(g);
  printf("ppu: ctrl=%02x mask=%02x pal0=%02x pal1=%02x chr0=%02x vram0=%02x\n",
         g->ppu.ctrl, g->ppu.mask, g->ppu.pal[0], g->ppu.pal[1],
         g->ppu.chr[0], g->ppu.vram[0]);
  printf("game: frames=%llu cycles=%llu render=%s\n",
         (unsigned long long)g->stats.ppu_frames,
         (unsigned long long)(g->cpu.cycles - start_cycles), out ? out : "(none)");
  free(rgb);
  return g->cpu.halted ? 1 : 0;
}

int main(int argc, char **argv) {
  if (argc < 3) { usage(argv[0]); return 2; }
  const char *cmd = argv[1];
  const char *path = argv[2];
  if (strcmp(cmd, "play") == 0) return gem_play(path);
  uint64_t max_cycles = 1000000;
  bool trace = false;
  const char *trace_state = NULL;
  bool press_start = false;
  int frames = 60;
  const char *out = NULL;
  const char *replay = NULL;

  if (strcmp(cmd, "game") == 0) {
    for (int i = 3; i < argc; i++) {
      if (strncmp(argv[i], "--replay-input=", 15) == 0) replay = argv[i] + 15;
      else if (strcmp(argv[i], "--start") == 0) press_start = true;
      else if (strncmp(argv[i], "--trace-state=", 14) == 0) trace_state = argv[i] + 14;
      else if (argv[i][0] == '-' && argv[i][1] == '-') { usage(argv[0]); return 2; }
      else if (frames == 60 && atoi(argv[i]) > 0) frames = atoi(argv[i]);
      else if (!out) out = argv[i];
    }
    Gem *g = gem_create();
    if (!g) return 2;
    if (trace_state) g->trace_state = fopen(trace_state, "w");
    int rc = run_game(g, path, frames, out, press_start, replay);
    if (g->trace_state) fclose(g->trace_state);
    gem_destroy(g);
    return rc;
  }

  for (int i = 3; i < argc; i++) {
    if (strcmp(argv[i], "--trace") == 0) trace = true;
    else if (strncmp(argv[i], "--trace-state=", 14) == 0) trace_state = argv[i] + 14;
    else max_cycles = strtoull(argv[i], NULL, 0);
  }
  Gem *g = gem_create();
  if (!g) return 2;
  if (gem_load_prg(g, path) != 0) { gem_destroy(g); return 2; }
  g->cpu.trace = trace;
  if (trace_state) {
    g->trace_state = fopen(trace_state, "w");
    if (!g->trace_state) { perror(trace_state); gem_destroy(g); return 2; }
  }
  if (strcmp(cmd, "stats") == 0) {
    gem_print_stats(g);
    if (g->trace_state) fclose(g->trace_state);
    gem_destroy(g);
    return 0;
  }
  if (strcmp(cmd, "run") != 0) { usage(argv[0]); gem_destroy(g); return 2; }
  gem_run(g, max_cycles);
  gem_print_stats(g);
  if (g->trace_state) fclose(g->trace_state);
  gem_destroy(g);
  return 0;
}
