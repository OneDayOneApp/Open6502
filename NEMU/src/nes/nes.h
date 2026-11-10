#ifndef __NES_H__
#define __NES_H__

#include <common.h>
#include <stdint.h>

#define NES_SCREEN_W 256
#define NES_SCREEN_H 240
#define NES_CPU_HZ 1789773
#define NES_FPS 60
#define NES_AUDIO_HZ 44100

typedef struct NES_Cart {
  uint8_t *prg;
  size_t prg_size;
  uint8_t *chr;
  size_t chr_size;
  bool chr_ram;
  uint8_t prg_ram[8192];
  int mapper;
  int mirror;          /* 0 horizontal, 1 vertical, 2 four-screen, 3 single */
  bool battery;
  bool four_screen;

  /* mapper state (kept in one struct to keep the implementation compact) */
  uint8_t shift;
  uint8_t mmc1_control;
  uint8_t mmc1_chr0;
  uint8_t mmc1_chr1;
  uint8_t mmc1_prg;
  uint8_t prg_bank;
  uint8_t chr_bank;
  uint8_t bank_select;
  uint8_t bank_regs[8];
  bool prg_ram_enable;
  bool irq_enable;
  bool irq_reload;
  bool irq;
  int irq_counter;
  int irq_latch;
  uint8_t prg_mode;
  uint8_t chr_mode;
} NES_Cart;

typedef struct NES_PPU {
  NES_Cart *cart;
  uint8_t ciram[4][1024];
  uint8_t palette[32];
  uint8_t oam[256];
  uint8_t oam_addr;
  uint8_t ctrl;
  uint8_t mask;
  uint8_t status;
  uint16_t v;
  uint16_t t;
  uint8_t fine_x;
  uint8_t w;
  uint16_t read_buffer;
  int scanline;
  int cycle;
  bool nmi_line;
  bool nmi_trigger;
  bool frame_ready;
  uint32_t framebuffer[NES_SCREEN_W * NES_SCREEN_H];
} NES_PPU;

typedef struct NES_Pulse {
  bool enabled;
  uint8_t duty;
  uint8_t duty_pos;
  uint16_t timer;
  uint16_t period;
  uint8_t length;
  bool halt;
  bool constant_volume;
  uint8_t volume;
  uint8_t envelope;
  uint8_t env_divider;
  bool env_start;
  bool sweep_enable;
  uint8_t sweep_period;
  bool sweep_negate;
  uint8_t sweep_shift;
  uint16_t sweep_divider;
} NES_Pulse;

typedef struct NES_Triangle {
  bool enabled;
  uint16_t timer;
  uint16_t period;
  uint8_t length;
  bool control;
  uint8_t linear;
  uint8_t linear_reload;
  bool linear_reload_flag;
  uint8_t pos;
} NES_Triangle;

typedef struct NES_Noise {
  bool enabled;
  uint16_t timer;
  uint16_t period;
  uint8_t length;
  bool halt;
  bool constant_volume;
  uint8_t volume;
  uint8_t envelope;
  uint8_t env_divider;
  bool env_start;
  uint16_t shift;
  bool mode;
} NES_Noise;

typedef struct NES_APU {
  NES_Pulse pulse[2];
  NES_Triangle triangle;
  NES_Noise noise;
  uint8_t regs[0x18];
  uint8_t status;
  uint8_t frame_mode;
  bool frame_irq_inhibit;
  bool frame_irq;
  uint32_t frame_cycle;
  uint32_t sample_accum;
  int16_t samples[2048];
  int sample_count;
  uint32_t cycles;
} NES_APU;

/* src/nes/nes.c */
extern bool nes_active;
extern NES_PPU ppu;
extern NES_APU apu;
extern NES_Cart cart;

bool nes_is_requested(int argc, char **argv);
int nes_main(int argc, char **argv);
uint8_t nes_cpu_read(uint16_t addr);
void nes_cpu_write(uint16_t addr, uint8_t value);

/* src/nes/cart.c */
bool cart_load(const char *path, NES_Cart *c);
void cart_free(NES_Cart *c);
uint8_t cart_cpu_read(NES_Cart *c, uint16_t addr);
void cart_cpu_write(NES_Cart *c, uint16_t addr, uint8_t value);
uint8_t cart_chr_read(NES_Cart *c, uint16_t addr);
void cart_chr_write(NES_Cart *c, uint16_t addr, uint8_t value);
int cart_nt_index(NES_Cart *c, uint16_t addr);
void cart_scanline(NES_Cart *c);

/* src/nes/ppu.c */
void ppu_reset(NES_PPU *p, NES_Cart *c);
void ppu_step(NES_PPU *p, int ppu_cycles);
uint8_t ppu_read_reg(NES_PPU *p, uint16_t addr);
void ppu_write_reg(NES_PPU *p, uint16_t addr, uint8_t value);
void ppu_oam_dma(NES_PPU *p, const uint8_t *src);
uint8_t ppu_read_oam(NES_PPU *p);
void ppu_write_oam(NES_PPU *p, uint8_t value);
uint32_t ppu_palette_rgb(uint8_t idx);

/* src/nes/apu.c */
void apu_reset(NES_APU *a);
void apu_write(NES_APU *a, uint16_t addr, uint8_t value);
uint8_t apu_read(NES_APU *a, uint16_t addr);
void apu_step(NES_APU *a, int cpu_cycles, bool irq_enabled);
int apu_take_samples(NES_APU *a, int16_t *out, int max);

#endif
