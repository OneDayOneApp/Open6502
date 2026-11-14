/*
 * JIT lab driver.
 *
 *   jit6502 klaus   image.bin            run Klaus to the success trap
 *   jit6502 run     image.bin [options]  run an image on one engine
 *   jit6502 bench   image.bin [options]  time an image on one engine
 *   jit6502 nes     game.nes  [options]  run a NES ROM and render frames
 *
 * Options:
 *   --engine=interp|jit   execution engine (default: interp)
 *   --load=ADDR           load address, default 0 (Klaus images are 64K @ 0)
 *   --entry=ADDR          start PC, overriding the RESET vector
 *   --max-inst=N          instruction budget, default 100,000,000
 *   --stop-pc=ADDR        stop when PC reaches ADDR
 *   --trace=FILE          write "pc a x y sp p" per instruction
  --mem=ADDR:LEN        dump LEN bytes at ADDR after the run
  --code=LO-HI          code window for self-modifying-code tracking
  --chain / --no-chain  force block chaining on or off
  --stats               print JIT cache statistics
  --frames=N            nes: run N frames (default 300)
  --run-inst=N          nes: run N instructions (debug/compare mode)
  --trace-state=FILE    nes: per-instruction state trace (DiffTest against GEM6502)
  --wav=FILE            nes: write rendered audio as a mono 16-bit WAV
  --window              nes: open an SDL window and play
  --scale=N             nes window scale (default 3)
  --screenshot=FILE     nes: write the last frame as a PPM
 *   --repeat=N            bench only: repeat the run N times
 */
#include "cpu.h"
#include "jit.h"
#include "frontend.h"
#include "nes_system.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int cpu_step(Cpu *c);
int selftest_difftest(int iterations);

/* Klaus 6502 functional test: success = the CPU parks itself on this JMP. */
#define KLAUS_PASS_PC 0x3469
#define KLAUS_DEFAULT_MAX_INST 200000000ull

static double now_sec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static uint64_t parse_u64(const char *s) {
  char *end = NULL;
  uint64_t v = strtoull(s, &end, 0);
  if (end == s || *end != '\0') {
    fprintf(stderr, "error: bad number '%s'\n", s);
    exit(2);
  }
  return v;
}

static void usage(const char *argv0) {
  printf("usage: %s <klaus|run|bench|selfcheck> [IMAGE|COUNT] [options]\n", argv0);
  printf("  --engine=interp|jit   --chain/--no-chain   --stats\n");
  printf("  --load=ADDR  --entry=ADDR  --max-inst=N  --stop-pc=ADDR\n");
  printf("  --code=LO-HI  --trace=FILE  --mem=ADDR:LEN  --repeat=N  --quiet\n");
}

typedef struct Options {
  const char *image;
  const char *engine;
  const char *trace_path;
  int has_mem;
  uint16_t mem_addr;
  int mem_len;
  int has_code;
  uint16_t code_lo, code_hi;
  int chain; /* -1 = per-command default */
  int stats;
  uint64_t frames;
  uint64_t run_inst;
  const char *screenshot;
  const char *trace_state;
  int window;
  int scale;
  const char *wav_path;
  uint16_t load;
  uint64_t max_inst;
  uint64_t repeat;
  int has_stop;
  uint16_t stop_pc;
  int has_entry;
  uint16_t entry;
  bool quiet;
} Options;

static const char *engine_name(const Options *o) {
  return strcmp(o->engine, "jit") == 0 ? "jit" : "interp";
}

static int run_once(const Options *o, Cpu *c, FILE *trace, uint64_t *insts) {
  if (strcmp(o->engine, "jit") == 0) {
    return jit_run(c, o->max_inst, o->has_stop, o->stop_pc, trace, insts);
  }
  for (uint64_t n = 0; n < o->max_inst; n++) {
    if (o->has_stop && c->pc == o->stop_pc) {
      *insts = n;
      return RUN_STOP_PC;
    }
    if (trace != NULL) {
      fprintf(trace, "%04X %02X %02X %02X %02X %02X\n",
              (unsigned)c->pc, c->a, c->x, c->y, c->sp, c->p);
    }
    int rc = cpu_step(c);
    if (rc != RUN_OK) {
      *insts = n;
      return rc;
    }
  }
  *insts = o->max_inst;
  return RUN_MAX_INST;
}

static int prepare(const Options *o, Cpu *c, uint8_t **mem_out) {
  uint8_t *mem = malloc(MEM_SIZE);
  if (mem == NULL) {
    fprintf(stderr, "error: out of memory\n");
    return 0;
  }
  cpu_init(c, mem);
  if (!cpu_load(c, o->image, o->load)) {
    fprintf(stderr, "error: cannot load image '%s'\n", o->image);
    free(mem);
    return 0;
  }
  cpu_reset(c);
  if (o->has_entry) c->pc = o->entry;
  *mem_out = mem;
  return 1;
}

static const char *status_name(int rc) {
  switch (rc) {
    case RUN_STOP_PC: return "stop-pc";
    case RUN_MAX_INST: return "max-inst";
    case RUN_ILLEGAL: return "illegal-opcode";
    default: return "ok";
  }
}

static int cmd_run(const Options *o) {
  Cpu c;
  uint8_t *mem = NULL;
  if (!prepare(o, &c, &mem)) return 1;

  FILE *trace = NULL;
  if (o->trace_path != NULL) {
    trace = fopen(o->trace_path, "w");
    if (trace == NULL) {
      fprintf(stderr, "error: cannot open trace '%s'\n", o->trace_path);
      free(mem);
      return 1;
    }
  }

  uint64_t insts = 0;
  double t0 = now_sec();
  int rc = run_once(o, &c, trace, &insts);
  double elapsed = now_sec() - t0;

  if (trace != NULL) fclose(trace);

  if (o->has_mem) {
    printf("mem %04X:", (unsigned)o->mem_addr);
    for (int i = 0; i < o->mem_len; i++) {
      printf(" %02X", c.mem[(uint16_t)(o->mem_addr + i)]);
    }
    printf("\n");
  }
  printf("state pc=%04X a=%02X x=%02X y=%02X sp=%02X p=%02X cycles=%" PRIu64 "\n",
         (unsigned)c.pc, c.a, c.x, c.y, c.sp, c.p, c.cycles);
  if (!o->quiet) {
    printf("image     = %s (load $%04X)\n", o->image, (unsigned)o->load);
    printf("engine    = %s\n", engine_name(o));
    printf("status    = %s\n", status_name(rc));
    printf("pc        = $%04X\n", (unsigned)c.pc);
    printf("a x y sp p= %02X %02X %02X %02X %02X\n",
           c.a, c.x, c.y, c.sp, c.p);
    printf("inst      = %" PRIu64 "\n", insts);
    printf("cycles    = %" PRIu64 "\n", c.cycles);
    printf("elapsed   = %.6f s\n", elapsed);
    if (elapsed > 0) {
      printf("speed     = %.2f MIPS\n", (double)insts / elapsed / 1e6);
    }
  }
  if (o->stats && strcmp(o->engine, "jit") == 0) {
    printf("jit       = %llu blocks compiled, %llu full flushes, %llu page flushes\n",
           (unsigned long long)jit_compile_count(),
           (unsigned long long)jit_flush_count(),
           (unsigned long long)jit_page_flush_count());
  }
  free(mem);
  return rc == RUN_ILLEGAL ? 1 : 0;
}

static int cmd_bench(const Options *o) {
  Cpu c;
  uint8_t *mem = NULL;
  if (!prepare(o, &c, &mem)) return 1;

  uint64_t total_inst = 0, total_cycles = 0;
  uint64_t repeat = o->repeat > 0 ? o->repeat : 1;

  /* Warm-up pass: page faults, code-cache population, branch predictors. */
  uint64_t sink = 0;
  run_once(o, &c, NULL, &sink);

  double best = 1e30, t_total = 0;
  for (uint64_t r = 0; r < repeat; r++) {
    cpu_reset(&c);
    jit_flush();
    uint64_t insts = 0;
    double t0 = now_sec();
    int rc = run_once(o, &c, NULL, &insts);
    double dt = now_sec() - t0;
    if (rc == RUN_ILLEGAL) {
      fprintf(stderr, "error: illegal opcode at $%04X\n", (unsigned)c.pc);
      free(mem);
      return 1;
    }
    total_inst += insts;
    total_cycles += c.cycles;
    t_total += dt;
    if (dt < best) best = dt;
  }

  double avg = t_total / (double)repeat;
  double inst_per_s = (double)(total_inst / repeat) / (avg > 0 ? avg : 1e-9);
  double cycles_per_s = (double)(total_cycles / repeat) / (avg > 0 ? avg : 1e-9);
  const double nes_hz = 1789773.0; /* NTSC 6502 rate, for scale reference */

  printf("image      = %s (load $%04X)\n", o->image, (unsigned)o->load);
  printf("engine     = %s\n", engine_name(o));
  printf("inst/run   = %" PRIu64 "\n", total_inst / repeat);
  printf("cycles/run = %" PRIu64 "\n", total_cycles / repeat);
  printf("elapsed    = %.6f s (avg of %" PRIu64 ", best %.6f)\n", avg, repeat, best);
  printf("speed      = %.1f MIPS\n", inst_per_s / 1e6);
  printf("cycle rate = %.1f MHz\n", cycles_per_s / 1e6);
  printf("realtime   = %.1fx NES NTSC\n", cycles_per_s / nes_hz);
  if (o->stats && strcmp(o->engine, "jit") == 0) {
    printf("jit blocks = %" PRIu64 " compiled, %" PRIu64 " full flushes, %" PRIu64 " page flushes\n",
           jit_compile_count(), jit_flush_count(), jit_page_flush_count());
  }

  free(mem);
  return 0;
}

typedef struct AudioBuf {
  int16_t *data;
  size_t len, cap;
} AudioBuf;

static void audio_append(AudioBuf *b, const int16_t *src, int n) {
  if (n <= 0) return;
  if (b->len + (size_t)n > b->cap) {
    size_t cap = b->cap ? b->cap : 65536;
    while (cap < b->len + (size_t)n) cap *= 2;
    int16_t *p = realloc(b->data, cap * sizeof(int16_t));
    if (p == NULL) return;
    b->data = p;
    b->cap = cap;
  }
  memcpy(b->data + b->len, src, (size_t)n * sizeof(int16_t));
  b->len += (size_t)n;
}

static bool write_wav(const char *path, const AudioBuf *b) {
  FILE *fp = fopen(path, "wb");
  if (fp == NULL) return false;
  uint32_t data_size = (uint32_t)(b->len * sizeof(int16_t));
  uint32_t riff_size = 36 + data_size;
  uint32_t rate = 44100;
  uint32_t byte_rate = rate * 2;
  uint16_t block_align = 2;
  uint16_t bits = 16;

  fwrite("RIFF", 1, 4, fp);
  fwrite(&riff_size, 4, 1, fp);
  fwrite("WAVEfmt ", 1, 8, fp);
  uint32_t fmt_size = 16;
  uint16_t format = 1, channels = 1;
  fwrite(&fmt_size, 4, 1, fp);
  fwrite(&format, 2, 1, fp);
  fwrite(&channels, 2, 1, fp);
  fwrite(&rate, 4, 1, fp);
  fwrite(&byte_rate, 4, 1, fp);
  fwrite(&block_align, 2, 1, fp);
  fwrite(&bits, 2, 1, fp);
  fwrite("data", 1, 4, fp);
  fwrite(&data_size, 4, 1, fp);
  if (b->len > 0) fwrite(b->data, sizeof(int16_t), b->len, fp);
  fclose(fp);
  return true;
}

static bool write_ppm(const char *path, const uint32_t *fb) {
  FILE *fp = fopen(path, "wb");
  if (fp == NULL) return false;
  fprintf(fp, "P6\n%d %d\n255\n", NES_SCREEN_W, NES_SCREEN_H);
  for (int i = 0; i < NES_SCREEN_W * NES_SCREEN_H; i++) {
    uint8_t rgb[3] = {(uint8_t)(fb[i] >> 16), (uint8_t)(fb[i] >> 8),
                      (uint8_t)fb[i]};
    fwrite(rgb, 1, sizeof(rgb), fp);
  }
  fclose(fp);
  return true;
}

/* Same layout as GEM6502's --trace-state so the two traces can be diffed:
 * pc a x y sp p ram00 ram0A ram37 ram38 ram39 ram47 cycles scanline dot nmi */
static void trace_nes_line(FILE *f, const Cpu *c) {
  static const uint16_t addr[6] = {0x00, 0x0A, 0x37, 0x38, 0x39, 0x47};
  fprintf(f, "%04x %02x %02x %02x %02x %02x", (unsigned)c->pc, c->a, c->x, c->y,
          c->sp, c->p);
  for (int i = 0; i < 6; i++) fprintf(f, " %02x", g_wram[addr[i] & 0x7FF]);
  fprintf(f, " %llu %d %llu %d\n", (unsigned long long)c->cycles, g_ppu.scanline,
          (unsigned long long)g_ppu.cycle, c->nmi_pending);
}

static int nes_frame_ready(void) { return g_ppu.frame_ready ? 1 : 0; }

static int step_frame_interp(Cpu *c) {
  g_ppu.frame_ready = false;
  while (!g_ppu.frame_ready) {
    if (cpu_step(c) != RUN_OK) return 0;
    nes_sync(c);
  }
  return 1;
}

static int step_frame_jit(Cpu *c) {
  g_ppu.frame_ready = false;
  while (!g_ppu.frame_ready) {
    uint64_t ran = 0;
    if (jit_run(c, 1000000000ull, 0, 0, NULL, &ran) != RUN_STOP_PC) return 0;
  }
  return 1;
}

static int cmd_nes(const Options *o) {
  int use_jit = strcmp(o->engine, "jit") == 0;
  if (use_jit && !jit_available()) {
    fprintf(stderr, "error: jit engine is not built into this binary\n");
    return 2;
  }

  uint8_t *mem = malloc(MEM_SIZE);
  if (mem == NULL) return 1;
  Cpu c;
  cpu_init(&c, mem);

  if (!nes_system_load(o->image)) {
    fprintf(stderr, "error: cannot load NES ROM '%s'\n", o->image);
    free(mem);
    return 1;
  }
  nes_system_reset(&c);
  cpu_reset_nes(&c);

  if (use_jit) {
    /* Translated code sees the 2 KiB WRAM as its fast path and calls the bus
     * helpers for everything at $2000 and above. */
    c.mem = g_wram;
    jit_set_bus_mode(1);
    jit_set_sync_hook(nes_sync);
    jit_set_stop_check(nes_frame_ready);
    /* NES mode stays unchained by default: chains only step the PPU at I/O
     * accesses, so a long RAM-only stretch delays the vblank NMI and makes
     * mid-frame register writes land late (visible as a torn scroll line).
     * --chain remains available for throughput experiments. */
    if (o->chain < 0) jit_set_chaining(0);
  }

  if (o->run_inst > 0 && o->trace_state == NULL) {
    uint64_t want = o->run_inst;
    uint64_t ran = 0;
    if (use_jit) {
      jit_set_stop_check(NULL);
      FILE *tr = o->trace_path != NULL ? fopen(o->trace_path, "w") : NULL;
      int rc = jit_run(&c, want, 0, 0, tr, &ran);
      if (tr != NULL) fclose(tr);
      if (rc != RUN_MAX_INST && rc != RUN_STOP_PC) {
        fprintf(stderr, "error: jit stopped with %d at $%04X\n", rc,
                (unsigned)c.pc);
        free(mem);
        return 1;
      }
    } else {
      while (ran < want) {
        int rc = cpu_step(&c);
        if (rc != RUN_OK) {
          fprintf(stderr, "error: illegal opcode at $%04X\n", (unsigned)c.pc);
          free(mem);
          return 1;
        }
        nes_sync(&c);
        ran++;
      }
    }
    printf("run-inst  = %llu (executed %llu)\n", (unsigned long long)want,
           (unsigned long long)ran);
    printf("state pc=%04X a=%02X x=%02X y=%02X sp=%02X p=%02X cycles=%llu\n",
           (unsigned)c.pc, c.a, c.x, c.y, c.sp, c.p,
           (unsigned long long)c.cycles);
    free(mem);
    return 0;
  }

  if (o->trace_state != NULL) {
    FILE *ts = fopen(o->trace_state, "w");
    if (ts == NULL) {
      fprintf(stderr, "error: cannot write '%s'\n", o->trace_state);
      free(mem);
      return 1;
    }
    uint64_t want = o->run_inst > 0 ? o->run_inst : 1000000;
    if (use_jit) {
      jit_set_max_block(1);   /* one instruction per block -> exact trace */
      jit_set_chaining(0);    /* chains would run past the 1-instruction budget */
      jit_set_stop_check(NULL); /* frame boundaries must not stop the trace */
    }
    uint64_t n = 0;
    while (n < want) {
      if (use_jit) {
        uint64_t ran = 0;
        int rc = jit_run(&c, 1, 0, 0, NULL, &ran);
        if (rc != RUN_MAX_INST) {
          fprintf(stderr, "error: jit stopped with %d at $%04X\n", rc,
                  (unsigned)c.pc);
          fclose(ts);
          free(mem);
          return 1;
        }
      } else {
        if (cpu_step(&c) != RUN_OK) {
          fprintf(stderr, "error: illegal opcode at $%04X\n", (unsigned)c.pc);
          fclose(ts);
          free(mem);
          return 1;
        }
        nes_sync(&c);
      }
      trace_nes_line(ts, &c);
      n++;
    }
    fclose(ts);
    printf("trace     = %s (%llu instructions)\n", o->trace_state,
           (unsigned long long)n);
    free(mem);
    return 0;
  }

  if (o->window) {
    int ok = nes_run_window(&c, o->scale,
                            use_jit ? step_frame_jit : step_frame_interp);
    free(mem);
    return ok ? 0 : 1;
  }

  uint64_t frames = o->frames > 0 ? o->frames : 1;
  uint64_t done = 0;
  AudioBuf wav = {0};
  double t0 = now_sec();
  while (done < frames) {
    g_ppu.frame_ready = false;
    while (!g_ppu.frame_ready) {
      if (use_jit) {
        uint64_t insts = 0;
        int rc = jit_run(&c, 1000000000ull, 0, 0, NULL, &insts);
        if (rc != RUN_STOP_PC) {
          fprintf(stderr, "error: jit stopped with %d at $%04X\n", rc,
                  (unsigned)c.pc);
          free(mem);
          return 1;
        }
      } else {
        int rc = cpu_step(&c);
        if (rc != RUN_OK) {
          fprintf(stderr, "error: illegal opcode at $%04X\n", (unsigned)c.pc);
          free(mem);
          return 1;
        }
        nes_sync(&c);
      }
    }
    done++;
    if (o->wav_path != NULL) {
      int16_t buf[4096];
      int n = nes_audio_pull(buf, (int)(sizeof(buf) / sizeof(buf[0])));
      audio_append(&wav, buf, n);
    }
  }

  printf("nes       = %s (mapper %d, PRG %zu KiB, CHR %zu KiB)\n", o->image,
         g_cart.mapper, g_cart.prg_size / 1024, g_cart.chr_size / 1024);
  printf("frames    = %" PRIu64 "\n", done);
  double elapsed = now_sec() - t0;
  if (elapsed > 0) {
    printf("elapsed   = %.6f s (%.1f fps)\n", elapsed, (double)done / elapsed);
  }
  printf("pc        = $%04X\n", (unsigned)c.pc);
  printf("cycles    = %" PRIu64 "\n", c.cycles);
  printf("state pc=%04X a=%02X x=%02X y=%02X sp=%02X p=%02X cycles=%" PRIu64 "\n",
         (unsigned)c.pc, c.a, c.x, c.y, c.sp, c.p, c.cycles);
  if (use_jit) {
    printf("jit       = %" PRIu64 " blocks run, %" PRIu64 " compiled, %" PRIu64
           " page flushes, %" PRIu64 " interpreter fallbacks\n",
           jit_blocks_run(), jit_compile_count(), jit_page_flush_count(),
           jit_fallback_steps());
  }

  if (o->wav_path != NULL) {
    if (!write_wav(o->wav_path, &wav)) {
      fprintf(stderr, "error: cannot write '%s'\n", o->wav_path);
      free(wav.data);
      free(mem);
      return 1;
    }
    printf("wav       = %s (%zu samples)\n", o->wav_path, wav.len);
    free(wav.data);
  }

  if (o->screenshot != NULL) {
    if (!write_ppm(o->screenshot, g_ppu.framebuffer)) {
      fprintf(stderr, "error: cannot write '%s'\n", o->screenshot);
      free(mem);
      return 1;
    }
    printf("screenshot= %s\n", o->screenshot);
  }
  free(mem);
  return 0;
}

static int cmd_klaus(const Options *o) {
  Options k = *o;
  k.has_stop = 1;
  k.stop_pc = KLAUS_PASS_PC;
  /* Klaus is started at its `start` label, not through the reset vector:
   * the vectors in the image point at trap loops. */
  k.has_entry = 1;
  k.entry = 0x0400;
  k.max_inst = KLAUS_DEFAULT_MAX_INST;

  Cpu c;
  uint8_t *mem = NULL;
  if (!prepare(&k, &c, &mem)) return 1;

  uint64_t insts = 0;
  double t0 = now_sec();
  int rc = run_once(&k, &c, NULL, &insts);
  double elapsed = now_sec() - t0;

  int pass = (rc == RUN_STOP_PC && c.pc == KLAUS_PASS_PC);
  printf("klaus engine = %s\n", engine_name(&k));
  if (pass) {
    printf("klaus result = PASS at PC=$%04X\n", (unsigned)c.pc);
  } else {
    printf("klaus result = FAIL (status %s, PC=$%04X, a=%02X x=%02X y=%02X sp=%02X p=%02X)\n",
           status_name(rc), (unsigned)c.pc, c.a, c.x, c.y, c.sp, c.p);
  }
  printf("klaus inst   = %" PRIu64 " (%.3f s, %.1f MIPS)\n", insts, elapsed,
         elapsed > 0 ? (double)insts / elapsed / 1e6 : 0.0);
  if (o->stats && strcmp(o->engine, "jit") == 0) {
    printf("jit blocks   = %" PRIu64 " compiled, %" PRIu64 " full flushes, %" PRIu64 " page flushes\n",
           jit_compile_count(), jit_flush_count(), jit_page_flush_count());
  }
  free(mem);
  return pass ? 0 : 1;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
    return 2;
  }

  const char *cmd = argv[1];
  Options o = {
      .image = NULL,
      .engine = "interp",
      .trace_path = NULL,
      .has_mem = 0,
      .mem_addr = 0,
      .mem_len = 0,
      .has_code = 0,
      .code_lo = 0x0400,
      .code_hi = 0x8000,
      .chain = -1,
      .stats = 0,
      .frames = 300,
      .run_inst = 0,
      .screenshot = NULL,
      .trace_state = NULL,
      .window = 0,
      .scale = 3,
      .wav_path = NULL,
      .load = 0,
      .max_inst = 100000000ull,
      .repeat = 1,
      .has_stop = 0,
      .stop_pc = 0,
      .has_entry = 0,
      .entry = 0,
      .quiet = false,
  };

  for (int i = 2; i < argc; i++) {
    const char *a = argv[i];
    if (strncmp(a, "--engine=", 9) == 0) {
      o.engine = a + 9;
      if (strcmp(o.engine, "interp") != 0 && strcmp(o.engine, "jit") != 0) {
        fprintf(stderr, "error: unknown engine '%s'\n", o.engine);
        return 2;
      }
    } else if (strncmp(a, "--load=", 7) == 0) {
      o.load = (uint16_t)parse_u64(a + 7);
    } else if (strncmp(a, "--max-inst=", 11) == 0) {
      o.max_inst = parse_u64(a + 11);
    } else if (strncmp(a, "--stop-pc=", 10) == 0) {
      o.stop_pc = (uint16_t)parse_u64(a + 10);
      o.has_stop = 1;
    } else if (strncmp(a, "--entry=", 8) == 0) {
      o.entry = (uint16_t)parse_u64(a + 8);
      o.has_entry = 1;
    } else if (strncmp(a, "--trace=", 8) == 0) {
      o.trace_path = a + 8;
    } else if (strncmp(a, "--repeat=", 9) == 0) {
      o.repeat = parse_u64(a + 9);
      if (o.repeat == 0) o.repeat = 1;
    } else if (strncmp(a, "--code=", 7) == 0) {
      unsigned lo = 0, hi = 0;
      if (sscanf(a + 7, "%x-%x", &lo, &hi) == 2) {
        o.code_lo = (uint16_t)lo;
        o.code_hi = (uint16_t)hi;
        o.has_code = 1;
      }
    } else if (strncmp(a, "--mem=", 6) == 0) {
      unsigned addr = 0, len = 0;
      if (sscanf(a + 6, "%x:%d", &addr, &len) == 2) {
        o.mem_addr = (uint16_t)addr;
        o.mem_len = len > 256 ? 256 : (int)len;
        o.has_mem = 1;
      }
    } else if (strncmp(a, "--trace-state=", 14) == 0) {
      o.trace_state = a + 14;
    } else if (strncmp(a, "--wav=", 6) == 0) {
      o.wav_path = a + 6;
    } else if (strcmp(a, "--window") == 0) {
      o.window = 1;
    } else if (strncmp(a, "--scale=", 8) == 0) {
      o.scale = (int)parse_u64(a + 8);
    } else if (strncmp(a, "--run-inst=", 11) == 0) {
      o.run_inst = parse_u64(a + 11);
    } else if (strncmp(a, "--frames=", 9) == 0) {
      o.frames = parse_u64(a + 9);
    } else if (strncmp(a, "--screenshot=", 13) == 0) {
      o.screenshot = a + 13;
    } else if (strcmp(a, "--stats") == 0) {
      o.stats = 1;
    } else if (strcmp(a, "--chain") == 0) {
      o.chain = 1;
    } else if (strcmp(a, "--no-chain") == 0) {
      o.chain = 0;
    } else if (strcmp(a, "--quiet") == 0) {
      o.quiet = true;
    } else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
      usage(argv[0]);
      return 0;
    } else if (a[0] == '-') {
      fprintf(stderr, "error: unknown option '%s'\n", a);
      return 2;
    } else if (o.image == NULL) {
      o.image = a;
    } else {
      fprintf(stderr, "error: extra argument '%s'\n", a);
      return 2;
    }
  }

  if (o.image == NULL && strcmp(cmd, "selfcheck") != 0) {
    usage(argv[0]);
    return 2;
  }
  if (o.has_code) jit_set_code_range(o.code_lo, o.code_hi);
  /* Exact instruction accounting (run/selfcheck) needs unchained blocks;
   * throughput commands (bench/klaus) use chaining. */
  int chaining = (strcmp(cmd, "bench") == 0 || strcmp(cmd, "klaus") == 0);
  if (o.chain >= 0) chaining = o.chain;
  jit_set_chaining(chaining);
  if (strcmp(o.engine, "jit") == 0 && !jit_available()) {
    fprintf(stderr, "error: jit engine is not built into this binary\n");
    return 2;
  }

  if (strcmp(cmd, "selfcheck") == 0) {
    /* `selfcheck [iterations]`: the positional argument is a count, not a ROM. */
    int iterations = 100000;
    if (o.image != NULL) {
      iterations = (int)strtoul(o.image, NULL, 0);
      if (iterations <= 0) iterations = 100000;
    }
    jit_set_max_block(1);
    return selftest_difftest(iterations);
  }
  if (strcmp(cmd, "nes") == 0) return cmd_nes(&o);
  if (strcmp(cmd, "klaus") == 0) return cmd_klaus(&o);
  if (strcmp(cmd, "run") == 0) return cmd_run(&o);
  if (strcmp(cmd, "bench") == 0) return cmd_bench(&o);

  fprintf(stderr, "error: unknown command '%s'\n", cmd);
  usage(argv[0]);
  return 2;
}
