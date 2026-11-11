#include "gem6502.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0) {
  fprintf(stderr,
    "GEM6502 %s - fast 6502/NES SoC cycle model\n"
    "usage:\n"
    "  %s run <prg.bin> [max_cycles] [--trace]\n"
    "  %s stats <prg.bin>\n"
    "\n"
    "The PRG image is loaded at $8000. The reset vector is read from\n"
    "$FFFC/$FFFD, so a 32 KiB NROM PRG image can be used directly.\n",
    GEM6502_VERSION, argv0, argv0);
}

int main(int argc, char **argv) {
  if (argc < 3) { usage(argv[0]); return 2; }
  const char *cmd = argv[1];
  const char *path = argv[2];
  uint64_t max_cycles = 1000000;
  bool trace = false;
  for (int i = 3; i < argc; i++) {
    if (strcmp(argv[i], "--trace") == 0) trace = true;
    else max_cycles = strtoull(argv[i], NULL, 0);
  }
  Gem *g = gem_create();
  if (!g) { fprintf(stderr, "out of memory\n"); return 2; }
  if (gem_load_prg(g, path) != 0) { gem_destroy(g); return 2; }
  g->cpu.trace = trace;
  if (strcmp(cmd, "stats") == 0) {
    gem_print_stats(g);
    gem_destroy(g);
    return 0;
  }
  if (strcmp(cmd, "run") != 0) { usage(argv[0]); gem_destroy(g); return 2; }
  gem_run(g, max_cycles);
  gem_print_stats(g);
  gem_destroy(g);
  return 0;
}
