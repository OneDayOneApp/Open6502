#ifndef GEM6502_H
#define GEM6502_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "audio.h"

#define GEM6502_VERSION "0.1.0"
#define GEM_RAM_SIZE   0x0800u
#define GEM_PRG_SIZE   0x8000u
#define GEM_CHR_SIZE   0x2000u
#define GEM_VRAM_SIZE  0x0800u
#define GEM_OAM_SIZE   0x0100u
#define GEM_PAL_SIZE   0x0020u
#define GEM_CART_PRG_MAX 0x40000u
#define GEM_CART_CHR_MAX 0x20000u

typedef struct Gem Gem;

/* GEM5-inspired event queue: one min-heap ordered by absolute tick. */
typedef struct GemEvent {
  uint64_t when;
  int type;
  void (*callback)(Gem *gem, void *arg);
  void *arg;
} GemEvent;

typedef struct GemEventQueue {
  GemEvent *heap;
  size_t size;
  size_t capacity;
} GemEventQueue;

typedef struct GemCpu {
  uint8_t a, x, y, sp, p;
  uint16_t pc;
  uint64_t cycles;
  uint64_t instructions;
  int dma_stall;
  bool halted;
  bool irq_pending;
  bool trace;
  const char *stop_reason;
} GemCpu;

typedef struct GemPpu {
  uint8_t ctrl, mask, status, oam_addr;
  uint8_t vram[GEM_VRAM_SIZE];
  uint8_t oam[GEM_OAM_SIZE];
  uint8_t pal[GEM_PAL_SIZE];
  uint8_t chr[GEM_CHR_SIZE];
  uint8_t read_buf;
  uint16_t vaddr, taddr;
  uint8_t fine_x;
  bool nmi_line;
  bool framebuffer_valid;
  uint32_t framebuffer[256 * 240];
  uint8_t bg_opaque[256];
  uint8_t scroll_x, scroll_y;
  bool w_toggle;
  int scanline;       /* current NEMU scanline */
  int cycle;          /* current NEMU cycle within scanline (0..340) */
  bool odd_frame;     /* next completed frame is odd -> skip pre-render dot 340 */
  bool frame_ready;   /* set at scanline 261 cycle 1, cleared per rendered frame */
  bool nmi_pending;
} GemPpu;

typedef struct GemApu {
  GemSound sound;
  uint8_t regs[0x18];
  uint64_t cycles;
  uint8_t joy1, joy2;
  bool joy_strobe;
  uint8_t joy1_shift, joy2_shift;
  bool irq;
  uint64_t frame_div;
  uint8_t frame_step;
  bool frame_mode;
  bool frame_inhibit;
} GemApu;

typedef struct GemBus {
  uint8_t ram[GEM_RAM_SIZE];
  uint8_t prg[GEM_PRG_SIZE];
  uint8_t prg_ram[0x2000];
  uint8_t chr_rom[GEM_CHR_SIZE];
  uint8_t cart_prg[GEM_CART_PRG_MAX];
  size_t cart_prg_size;
  uint8_t cart_chr[GEM_CART_CHR_MAX];
  size_t cart_chr_size;
  uint8_t mapper;
  bool vertical_mirroring;
  uint8_t m1_shift, m1_count, m1_ctrl, m1_chr0, m1_chr1, m1_prg;
  uint8_t m2_bank, m3_bank, m7_bank;
  uint8_t m4_select, m4_regs[8], m4_prg_mode, m4_chr_mode;
  uint8_t m4_irq_latch, m4_irq_counter, m4_irq_reload;
  bool m4_irq_enable, m4_irq_pending;
  bool cart_irq;
  uint64_t reads;
  uint64_t writes;
} GemBus;

typedef struct GemStats {
  uint64_t cpu_cycles;
  uint64_t instructions;
  uint64_t bus_reads;
  uint64_t bus_writes;
  uint64_t ppu_frames;
  uint64_t nmi_count;
  uint64_t illegal_opcodes;
} GemStats;

struct Gem {
  GemEventQueue events;
  GemCpu cpu;
  GemPpu ppu;
  GemApu apu;
  GemBus bus;
  GemStats stats;
  uint64_t now;         /* global event tick */
  uint64_t max_cycles;
  FILE *trace_state;
};

Gem *gem_create(void);
void gem_destroy(Gem *gem);
void gem_reset(Gem *gem);
int gem_load_game(Gem *gem, const char *path);
int gem_load_prg(Gem *gem, const char *path);
int gem_run(Gem *gem, uint64_t max_cycles);
int gem_step(Gem *gem);
/* side_effect=false peeks without changing device state or read counters. */
uint8_t gem_bus_read(Gem *gem, uint16_t addr, bool side_effect);
void gem_bus_write(Gem *gem, uint16_t addr, uint8_t value);
void gem_ppu_advance(Gem *gem, uint32_t cpu_cycles);
uint8_t gem_chr_read(const Gem *gem, uint16_t addr);
unsigned gem_nt_offset(const Gem *gem, uint16_t addr);
void gem_cart_a12(Gem *gem, int level);
void gem_apu_advance(Gem *gem, uint32_t cpu_cycles);
void gem_print_stats(const Gem *gem);

/* Event queue API */
void gem_event_init(GemEventQueue *q);
void gem_event_destroy(GemEventQueue *q);
int gem_event_push(GemEventQueue *q, GemEvent ev);
int gem_event_pop(GemEventQueue *q, GemEvent *out);
int gem_event_peek(const GemEventQueue *q, GemEvent *out);
int gem_run_events(Gem *gem, uint64_t until);

#endif
