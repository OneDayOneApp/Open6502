/*
 * NES machine glue: bus map, controllers, PPU/APU/CPU synchronisation.
 *
 * The PPU, APU and cartridge implementations are the project's own (copied
 * from NEMU), so frames and audio produced here can be compared against the
 * emulator's golden output.
 */
#include "nes_system.h"

#include "jit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

NES_PPU g_ppu;
NES_APU g_apu;
NES_Cart g_cart;
uint8_t g_wram[2048];

static uint8_t controller_state[2];
static uint8_t controller_shift[2];
static uint8_t controller_strobe;
static uint64_t last_cycles;
static Cpu *current_cpu;

/* ---- PPU <-> cartridge wiring ------------------------------------------ */

static uint8_t ppu_bus_chr_read(void *bus, uint16_t addr) {
  return cart_chr_read((NES_Cart *)bus, addr);
}
static void ppu_bus_chr_write(void *bus, uint16_t addr, uint8_t value) {
  cart_chr_write((NES_Cart *)bus, addr, value);
}
static int ppu_bus_nt_index(void *bus, uint16_t addr) {
  return cart_nt_index((NES_Cart *)bus, addr);
}
static void ppu_bus_scanline(void *bus) { cart_scanline((NES_Cart *)bus); }

static void attach_ppu(void) {
  ppu_set_bus(&g_ppu, &g_cart, ppu_bus_chr_read, ppu_bus_chr_write,
              ppu_bus_nt_index, ppu_bus_scanline);
  /* Mapper 0 never banks CHR, so pattern fetches can index the cartridge
   * buffer directly instead of going through an indirect call per fetch. */
  g_ppu.chr_flat = NULL;
  if (g_cart.mapper == 0 && g_cart.chr_size > 0 &&
      (g_cart.chr_size & (g_cart.chr_size - 1)) == 0) {
    g_ppu.chr_flat = g_cart.chr;
    g_ppu.chr_mask = (uint16_t)(g_cart.chr_size - 1);
  }
}

/* ---- APU --------------------------------------------------------------- */

static uint8_t apu_dmc_read(void *bus, uint16_t addr) {
  (void)bus;
  return nes_bus_read(addr);
}

static void attach_apu(void) {
  g_apu.dmc_read = apu_dmc_read;
  g_apu.dmc_bus = NULL;
}

/* ---- controllers -------------------------------------------------------- */

void nes_set_buttons(uint8_t p1, uint8_t p2) {
  controller_state[0] = p1;
  controller_state[1] = p2;
}

static uint8_t read_controller(int port) {
  if (controller_strobe) return controller_state[port] & 1;
  uint8_t v = controller_shift[port] & 1;
  controller_shift[port] >>= 1;
  return v;
}

/* ---- bus ---------------------------------------------------------------- */

uint8_t nes_bus_read(uint16_t addr) {
  if (addr < 0x2000) return g_wram[addr & 0x7FF];
  if (current_cpu != NULL) nes_sync(current_cpu);
  if (addr < 0x4000) {
    return ppu_read_reg(&g_ppu, (uint16_t)(0x2000 | (addr & 7)));
  }
  if (addr == 0x4015) return apu_read(&g_apu, addr);
  if (addr == 0x4016) return read_controller(0);
  if (addr == 0x4017) return read_controller(1);
  if (addr < 0x4020) return 0;
  if (addr < 0x6000) return 0;
  return cart_cpu_read(&g_cart, addr);
}

uint8_t nes_peek(uint16_t addr) {
  if (addr < 0x2000) return g_wram[addr & 0x7FF];
  if (addr < 0x4020) return 0; /* registers are never guest code */
  if (addr < 0x6000) return 0;
  return cart_cpu_read(&g_cart, addr);
}

void nes_bus_write(uint16_t addr, uint8_t value) {
  if (addr < 0x2000) {
    g_wram[addr & 0x7FF] = value;
    return;
  }
  if (current_cpu != NULL) nes_sync(current_cpu);
  if (addr < 0x4000) {
    ppu_write_reg(&g_ppu, (uint16_t)(0x2000 | (addr & 7)), value);
    return;
  }
  if (addr == 0x4014) {
    uint8_t page[256];
    for (int i = 0; i < 256; i++) {
      page[i] = nes_bus_read((uint16_t)((value << 8) | i));
    }
    ppu_oam_dma(&g_ppu, page);
    if (current_cpu != NULL) current_cpu->extra_cycles += 513;
    return;
  }
  if (addr == 0x4016) {
    controller_strobe = value & 1;
    if (controller_strobe) {
      controller_shift[0] = controller_state[0];
      controller_shift[1] = controller_state[1];
    }
    return;
  }
  if (addr >= 0x4000 && addr <= 0x4017) {
    apu_write(&g_apu, addr, value);
    return;
  }
  if (addr < 0x6000) return;
  cart_cpu_write(&g_cart, addr, value);
}

/* ---- lifecycle ---------------------------------------------------------- */

void nes_system_reset(Cpu *c) {
  memset(g_wram, 0, sizeof(g_wram));
  ppu_reset(&g_ppu, &g_cart);
  attach_ppu();
  apu_reset(&g_apu);
  attach_apu();
  memset(controller_state, 0, sizeof(controller_state));
  memset(controller_shift, 0, sizeof(controller_shift));
  controller_strobe = 0;
  last_cycles = 0;
  current_cpu = c;
  c->bus_mode = 1;
  c->nmi_pending = 0;
  c->irq_pending = 0;
  c->cycles = 0;
  c->extra_cycles = 0;
  g_cart.irq = false;
}

void nes_sync(Cpu *c) {
  current_cpu = c;
  if (c->extra_cycles != 0) {
    c->cycles += c->extra_cycles;
    c->extra_cycles = 0;
  }
  uint64_t delta = c->cycles - last_cycles;
  if (delta == 0) return;
  last_cycles = c->cycles;
  ppu_step(&g_ppu, (int)(delta * 3));
  apu_step(&g_apu, (int)delta, true);
  if (g_apu.frame_irq || g_apu.dmc.irq) c->irq_pending = 1;
  if (g_ppu.nmi_trigger) {
    g_ppu.nmi_trigger = false;
    c->nmi_pending = 1;
  }
  if (g_cart.irq) c->irq_pending = 1;
  if (g_ppu.frame_ready || c->nmi_pending || c->irq_pending) {
    g_jit_rt.pending = 1;
  }
}

int nes_audio_pull(int16_t *out, int max) {
  return apu_take_samples(&g_apu, out, max);
}

bool nes_system_load(const char *rom_path) {
  if (!cart_load(rom_path, &g_cart)) return false;
  g_ppu.cart = &g_cart;
  attach_ppu();
  return true;
}

void cpu_reset_nes(Cpu *c) {
  c->a = 0;
  c->x = 0;
  c->y = 0;
  c->sp = 0xFD;
  c->p = F_U | F_I;
  c->cycles = 0;
  c->pc = (uint16_t)(nes_bus_read(0xFFFC) | ((uint16_t)nes_bus_read(0xFFFD) << 8));
}
