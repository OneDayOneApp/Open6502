/*
 * NES machine around the lab CPU: 2 KiB work RAM, PPU registers, controllers,
 * cartridge/mapper.  Audio is not wired up yet (silence, no frame IRQ).
 */
#ifndef J6502_NES_SYSTEM_H
#define J6502_NES_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>

#include "cpu.h"
#include "nes.h"

extern NES_PPU g_ppu;
extern NES_APU g_apu;
extern NES_Cart g_cart;
extern uint8_t g_wram[2048];

/* Load a .nes image, initialise PPU/controller state and reset the CPU.
 * The CPU itself is reset by the caller (cpu_reset_nes). */
bool nes_system_load(const char *rom_path);

/* Put the machine into power-on state (keeps the loaded cartridge). */
void nes_system_reset(Cpu *c);

/* Bus accessed by both the interpreter and translated code. */
uint8_t nes_bus_read(uint16_t addr);
/* Side-effect-free read used by the translator to fetch guest code bytes. */
uint8_t nes_peek(uint16_t addr);
void nes_bus_write(uint16_t addr, uint8_t value);

/* Advance PPU (3 dots per CPU cycle) up to the CPU's current cycle count and
 * turn PPU/mapper events into pending CPU interrupts. */
void nes_sync(Cpu *c);

/* Controller state, bit0=A, bit1=B, bit2=Select, bit3=Start,
 * bit4=Up, bit5=Down, bit6=Left, bit7=Right. */
void nes_set_buttons(uint8_t p1, uint8_t p2);

/* Pull up to `max` mono 16-bit samples produced since the last call. */
int nes_audio_pull(int16_t *out, int max);

/* CPU reset that starts at the cartridge RESET vector with the NES stack. */
void cpu_reset_nes(Cpu *c);

#endif /* J6502_NES_SYSTEM_H */
