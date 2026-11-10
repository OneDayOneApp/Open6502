#ifndef NES_REF_NES_REF_H
#define NES_REF_NES_REF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NES_REF_OK 0
#define NES_REF_ERR_INVALID (-1)
#define NES_REF_ERR_NOMEM (-2)
#define NES_REF_ERR_FORMAT (-3)
#define NES_REF_ERR_UNSUPPORTED (-4)
#define NES_REF_HASH_SEED UINT64_C(1469598103934665603)
#define NES_REF_STATE_VERSION 1u

typedef enum {
  NES_REF_NTSC = 0,
  NES_REF_PAL = 1
} nes_ref_region;

typedef struct {
  nes_ref_region region;
  uint64_t master_tick;
  uint64_t cpu_cycle;
  uint64_t ppu_dot;
  uint32_t ppu_cycle;
  uint32_t ppu_scanline;
  uint64_t frame;
  uint32_t cpu_phase;
  uint32_t ppu_phase;
} nes_ref_clock;

typedef struct {
  uint8_t a, x, y, sp, p;
  uint16_t pc;
  uint64_t cycles;
  bool nmi_pending;
  bool irq_pending;
  bool jammed;
  bool is_65c02;
} nes_ref_cpu;

typedef struct {
  nes_ref_clock clock;
  nes_ref_cpu cpu;
  uint8_t ram[2048];
  uint8_t framebuffer_hash[32];
  uint8_t audio_hash[32];
} nes_ref_state;

typedef struct {
  int mapper;
  size_t prg_size;
  size_t chr_size;
  uint8_t *prg;
  uint8_t *chr;
  bool chr_ram;
  bool battery;
  bool four_screen;
  int mirroring;
} nes_ref_rom;

void nes_ref_clock_init(nes_ref_clock *c, nes_ref_region region);
void nes_ref_clock_tick(nes_ref_clock *c, uint64_t ticks);

int nes_ref_rom_load_mem(nes_ref_rom *r, const uint8_t *data, size_t size);
int nes_ref_rom_load_file(nes_ref_rom *r, const char *path);
void nes_ref_rom_free(nes_ref_rom *r);

uint64_t nes_ref_hash_bytes(uint64_t seed, const void *data, size_t size);
uint64_t nes_ref_state_hash(const nes_ref_state *state);

typedef struct {
  nes_ref_state state;
  nes_ref_rom rom;
  uint8_t prg_ram[8192];
  uint8_t open_bus;
  bool loaded;
  bool flat_mode;
  uint8_t *flat_ram;
} nes_ref_machine;

int nes_ref_machine_init(nes_ref_machine *m, nes_ref_region region);
int nes_ref_machine_init_flat(nes_ref_machine *m, nes_ref_region region);
int nes_ref_machine_load_flat(nes_ref_machine *m, const uint8_t *data, size_t size, uint16_t entry);
int nes_ref_machine_load_rom(nes_ref_machine *m, const uint8_t *data, size_t size);
int nes_ref_machine_load_rom_file(nes_ref_machine *m, const char *path);
void nes_ref_machine_reset(nes_ref_machine *m);
void nes_ref_machine_free(nes_ref_machine *m);
uint8_t nes_ref_machine_read(nes_ref_machine *m, uint16_t addr);
void nes_ref_machine_write(nes_ref_machine *m, uint16_t addr, uint8_t value);
uint64_t nes_ref_machine_hash(const nes_ref_machine *m);
int nes_ref_cpu_step(nes_ref_machine *m);

#endif
