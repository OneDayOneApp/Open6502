/*
 * JIT runtime: executable arena, block cache, dispatcher.
 *
 * Design notes that matter for the experiment:
 *   - One compiled function per block start (64K-entry direct map).
 *   - Blocks never span a 256-byte page, so a block belongs to exactly one
 *     page and self-modifying writes invalidate just that page.
 *   - Blocks return to the C dispatcher at control-flow boundaries.  Chained
 *     mode links blocks directly (resume entry points) but is bounded so the
 *     dispatcher keeps control for budgets and stop points.
 *   - NES mode (jit_set_bus_mode) routes $2000+ accesses through helper calls
 *     and steps the PPU between blocks via the sync hook.
 */
#include "jit.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <pthread.h>
#include <sys/mman.h>
#else
#include <sys/mman.h>
#endif

#define CODE_CAP (8u << 20)
#define MAX_BLOCKS 131072u
#define BLOCK_MARGIN 16384u

/* Referenced by translated code through x29.  Keep the two bus pointers first:
 * the chain lookup adds offsetof(JitRuntime, resume) to x29. */
JitRuntime g_jit_rt;

static uint8_t *g_code;
static size_t g_code_len;
static JitBlock *g_blocks;
static size_t g_nblocks;
static JitBlock **g_map;
static JitBlock *g_page_head[256];
static bool g_ready;
static uint64_t g_flush_count;
static uint64_t g_page_flush_count;
static uint64_t g_compile_count;
static int g_max_block = 48;
/* Read once from JIT_CHAIN_BUDGET: getenv() walks the whole environment and
 * used to run on every dispatched block (millions of calls per second). */
static uint16_t g_chain_budget = 4096;
static int g_chaining = 1;
static int g_bus_mode;
static uint16_t g_code_lo = 0x0400;
static uint16_t g_code_hi = 0x8000;
static void (*g_sync_hook)(Cpu *);
static int (*g_stop_check)(void);
static uint64_t g_fallback_steps;
static uint64_t g_blocks_run;

void jit_set_max_block(int n) { g_max_block = n < 1 ? 1 : n; }
void jit_set_chaining(int on) { g_chaining = on ? 1 : 0; }
void jit_set_code_range(uint16_t lo, uint16_t hi) {
  g_code_lo = lo;
  g_code_hi = hi;
}
void jit_set_sync_hook(void (*fn)(Cpu *)) { g_sync_hook = fn; }
void jit_set_stop_check(int (*fn)(void)) { g_stop_check = fn; }

void jit_set_bus_mode(int on) {
  g_bus_mode = on ? 1 : 0;
  if (g_bus_mode) {
    g_jit_rt.bus_read = nes_bus_read;
    g_jit_rt.bus_write = nes_bus_write;
  }
}

uint64_t jit_flush_count(void) { return g_flush_count; }
uint64_t jit_page_flush_count(void) { return g_page_flush_count; }
uint64_t jit_compile_count(void) { return g_compile_count; }
uint64_t jit_fallback_steps(void) { return g_fallback_steps; }
uint64_t jit_blocks_run(void) { return g_blocks_run; }

static int jit_init(void) {
  if (g_ready) return 1;
  g_map = (JitBlock **)calloc(65536, sizeof(*g_map));
  g_blocks = (JitBlock *)calloc(MAX_BLOCKS, sizeof(*g_blocks));
  if (g_map == NULL || g_blocks == NULL) return 0;
#if defined(__APPLE__)
  g_code = (uint8_t *)mmap(NULL, CODE_CAP, PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
#else
  g_code = (uint8_t *)mmap(NULL, CODE_CAP, PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
  if (g_code == MAP_FAILED) {
    g_code = NULL;
    return 0;
  }
  const char *mb = getenv("JIT_MAX_BLOCK");
  if (mb != NULL && *mb != '\0') jit_set_max_block(atoi(mb));
  const char *cb = getenv("JIT_CHAIN_BUDGET");
  if (cb != NULL && *cb != '\0') g_chain_budget = (uint16_t)atoi(cb);
  g_ready = true;
  return 1;
}

int jit_available(void) {
#if defined(__aarch64__)
  return jit_init();
#else
  return 0;
#endif
}

/* Drop every block that starts in `page`. */
static void flush_page(uint8_t page) {
  JitBlock *b = g_page_head[page];
  while (b != NULL) {
    JitBlock *next = b->next_page;
    g_map[b->pc] = NULL;
    g_jit_rt.resume[b->pc] = NULL;
    b->fn = NULL;
    b->resume = NULL;
    b = next;
  }
  g_page_head[page] = NULL;
  g_page_flush_count++;
}

/* Drop everything and reuse the arena from offset zero. */
void jit_flush(void) {
  if (!g_ready) return;
  for (size_t i = 0; i < g_nblocks; i++) {
    g_map[g_blocks[i].pc] = NULL;
    g_jit_rt.resume[g_blocks[i].pc] = NULL;
  }
  memset(g_page_head, 0, sizeof(g_page_head));
  g_nblocks = 0;
  g_code_len = 0;
  g_flush_count++;
}

static JitBlock *compile_block(Cpu *c, uint16_t pc, int has_stop,
                               uint16_t stop_pc) {
  if (g_nblocks >= MAX_BLOCKS || g_code_len + BLOCK_MARGIN > CODE_CAP) {
    jit_flush();
  }
  uint8_t *base = g_code + g_code_len;
  Emitter e;
  emit_init(&e, base, CODE_CAP - g_code_len);

#if defined(__APPLE__)
  pthread_jit_write_protect_np(0);
#endif
  int (*fn)(Cpu *) = NULL;
  int (*resume)(Cpu *) = NULL;
  uint8_t (*code_read)(uint16_t) = g_bus_mode ? nes_peek : NULL;
  int n = jit_compile(&e, c->mem, code_read, pc, has_stop, stop_pc, g_max_block,
                      g_code_lo, g_code_hi, g_chaining, g_bus_mode, &fn,
                      &resume);
  if (n > 0) __builtin___clear_cache((char *)base, (char *)base + e.len);
#if defined(__APPLE__)
  pthread_jit_write_protect_np(1);
#endif
  if (n <= 0) {
    if (getenv("JIT_DEBUG_COMPILE") != NULL) {
      fprintf(stderr, "compile failed pc=%04X n=%d oom=%d\n", (unsigned)pc, n,
              (int)e.oom);
    }
    return NULL;
  }

  {
    const char *dbg = getenv("JIT_DEBUG_PC");
    if (dbg != NULL && (unsigned)strtoul(dbg, NULL, 16) == pc) {
      fprintf(stderr, "block pc=%04X insts=%d len=%zu bytes:", pc, n, e.len);
      for (size_t i = 0; i < e.len && i < 400; i += 4) {
        uint32_t w;
        memcpy(&w, base + i, 4);
        fprintf(stderr, " %08x", w);
      }
      fprintf(stderr, "\n");
    }
  }

  JitBlock *b = &g_blocks[g_nblocks++];
  b->fn = fn;
  b->resume = resume;
  b->pc = pc;
  b->insts = (uint8_t)n;
  uint8_t page = (uint8_t)(pc >> 8);
  b->next_page = g_page_head[page];
  g_page_head[page] = b;
  g_map[pc] = b;
  g_jit_rt.resume[pc] = resume;
  g_code_len += e.len;
  g_compile_count++;
  return b;
}

static void trace_step(FILE *trace, const Cpu *c) {
  fprintf(trace, "%04X %02X %02X %02X %02X %02X\n", (unsigned)c->pc, c->a, c->x,
          c->y, c->sp, c->p);
}

int jit_run(Cpu *c, uint64_t max_inst, int has_stop, uint16_t stop_pc,
            FILE *trace, uint64_t *out_inst) {
  *out_inst = 0;
  if (!jit_init()) return RUN_ILLEGAL;

  uint64_t done = 0;
  for (;;) {
    if (g_stop_check != NULL && g_stop_check()) {
      *out_inst = done;
      return RUN_STOP_PC;
    }
    if (has_stop && c->pc == stop_pc) {
      *out_inst = done;
      return RUN_STOP_PC;
    }
    if (done >= max_inst) {
      *out_inst = done;
      return RUN_MAX_INST;
    }
    if (g_bus_mode && (c->nmi_pending || c->irq_pending)) {
      /* Interrupts are taken at instruction boundaries, like the CPU. */
      int rc = cpu_step(c);
      done++;
      if (g_sync_hook != NULL) g_sync_hook(c);
      if (rc != RUN_OK) {
        *out_inst = done;
        return rc;
      }
      continue;
    }
    if (c->code_dirty) {
      c->code_dirty = 0;
      if (getenv("JIT_FULL_FLUSH") != NULL) {
        jit_flush();
      } else {
        flush_page(c->dirty_page);
      }
    }

    JitBlock *b = g_map[c->pc];
    if (b == NULL) b = compile_block(c, c->pc, has_stop, stop_pc);

    if (b != NULL && done + b->insts <= max_inst) {
      if (trace != NULL) {
        trace_step(trace, c); /* block entry, not per insn */
        fprintf(trace, "-- block pc=%04X insts=%d done=%llu\n",
                (unsigned)b->pc, (int)b->insts, (unsigned long long)done);
      }
      c->chain_budget = g_chain_budget;
      g_jit_rt.pending = 0;
      int executed = b->fn(c);
      if (executed <= 0) {
        fprintf(stderr, "jit: block pc=%04X reported %d instructions\n",
                (unsigned)b->pc, executed);
        *out_inst = done;
        return RUN_ILLEGAL;
      }
      done += (uint64_t)executed;
      g_blocks_run++;
      if (g_sync_hook != NULL) g_sync_hook(c);
      continue;
    }

    /* Not translatable, or the remaining budget is smaller than this block. */
    if (trace != NULL) trace_step(trace, c);
    int rc = cpu_step(c);
    done++;
    g_fallback_steps++;
    if (g_sync_hook != NULL) g_sync_hook(c);
    if (rc != RUN_OK) {
      *out_inst = done;
      return rc;
    }
  }
}
