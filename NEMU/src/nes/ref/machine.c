#include "nes/ref/nes_ref.h"

#include <stdlib.h>
#include <string.h>

int nes_ref_machine_init(nes_ref_machine *m, nes_ref_region region) {
  if (m == NULL) return NES_REF_ERR_INVALID;
  memset(m, 0, sizeof(*m));
  nes_ref_clock_init(&m->state.clock, region);
  return NES_REF_OK;
}

int nes_ref_machine_init_flat(nes_ref_machine *m, nes_ref_region region) {
  int ret = nes_ref_machine_init(m, region);
  if (ret != NES_REF_OK) return ret;
  m->flat_ram = calloc(1, 65536);
  if (m->flat_ram == NULL) return NES_REF_ERR_NOMEM;
  m->flat_mode = true;
  return NES_REF_OK;
}

int nes_ref_machine_load_flat(nes_ref_machine *m, const uint8_t *data, size_t size, uint16_t entry) {
  if (m == NULL || data == NULL || !m->flat_mode || size > 65536) return NES_REF_ERR_INVALID;
  memcpy(m->flat_ram, data, size);
  m->state.cpu.a = 0;
  m->state.cpu.x = 0;
  m->state.cpu.y = 0;
  m->state.cpu.sp = 0xFD;
  m->state.cpu.p = 0x24;
  m->state.cpu.pc = entry;
  m->state.cpu.cycles = 0;
  m->state.cpu.nmi_pending = false;
  m->state.cpu.irq_pending = false;
  m->state.cpu.jammed = false;
  m->state.cpu.is_65c02 = false;
  return NES_REF_OK;
}

int nes_ref_machine_load_rom(nes_ref_machine *m, const uint8_t *data, size_t size) {
  if (m == NULL) return NES_REF_ERR_INVALID;
  nes_ref_rom rom;
  int ret = nes_ref_rom_load_mem(&rom, data, size);
  if (ret != NES_REF_OK) return ret;
  nes_ref_rom_free(&m->rom);
  m->rom = rom;
  m->loaded = true;
  nes_ref_machine_reset(m);
  return NES_REF_OK;
}

int nes_ref_machine_load_rom_file(nes_ref_machine *m, const char *path) {
  if (m == NULL || path == NULL) return NES_REF_ERR_INVALID;
  nes_ref_rom rom;
  int ret = nes_ref_rom_load_file(&rom, path);
  if (ret != NES_REF_OK) return ret;
  nes_ref_rom_free(&m->rom);
  m->rom = rom;
  m->loaded = true;
  nes_ref_machine_reset(m);
  return NES_REF_OK;
}

static uint8_t nrom_read(const nes_ref_machine *m, uint16_t addr) {
  if (!m->loaded || m->rom.mapper != 0 || m->rom.prg_size == 0) return 0;
  size_t off = (size_t)(addr - 0x8000u);
  if (m->rom.prg_size == 16384) off &= 0x3FFFu;
  else off &= 0x7FFFu;
  if (off >= m->rom.prg_size) off %= m->rom.prg_size;
  return m->rom.prg[off];
}

void nes_ref_machine_reset(nes_ref_machine *m) {
  if (m == NULL) return;
  nes_ref_region region = m->state.clock.region;
  bool flat_mode = m->flat_mode;
  uint8_t *flat_ram = m->flat_ram;
  memset(&m->state, 0, sizeof(m->state));
  m->flat_mode = flat_mode;
  m->flat_ram = flat_ram;
  nes_ref_clock_init(&m->state.clock, region);
  memset(m->prg_ram, 0, sizeof(m->prg_ram));
  m->open_bus = 0;
  m->state.cpu.sp = 0xFD;
  m->state.cpu.p = 0x24;
  if (m->loaded && !m->flat_mode) {
    m->state.cpu.pc = (uint16_t)(nrom_read(m, 0xFFFC) | (nrom_read(m, 0xFFFD) << 8));
  }
}

void nes_ref_machine_free(nes_ref_machine *m) {
  if (m == NULL) return;
  nes_ref_rom_free(&m->rom);
  free(m->flat_ram);
  memset(m, 0, sizeof(*m));
}

uint8_t nes_ref_machine_read(nes_ref_machine *m, uint16_t addr) {
  uint8_t v;
  if (m->flat_mode) return m->flat_ram[addr];
  if (addr < 0x2000) v = m->state.ram[addr & 0x7FF];
  else if (addr < 0x4000) v = 0;
  else if (addr < 0x6000) v = m->open_bus;
  else if (addr < 0x8000) v = m->prg_ram[(addr - 0x6000) & 0x1FFF];
  else v = nrom_read(m, addr);
  m->open_bus = v;
  return v;
}

void nes_ref_machine_write(nes_ref_machine *m, uint16_t addr, uint8_t value) {
  if (m->flat_mode) {
    m->flat_ram[addr] = value;
    return;
  }
  if (addr < 0x2000) m->state.ram[addr & 0x7FF] = value;
  else if (addr < 0x6000) { /* PPU/APU/IO: not modeled in M0 */ }
  else if (addr < 0x8000) m->prg_ram[(addr - 0x6000) & 0x1FFF] = value;
  else { /* NROM: ROM is read-only */ }
  m->open_bus = value;
}

uint64_t nes_ref_machine_hash(const nes_ref_machine *m) {
  uint64_t h = nes_ref_state_hash(&m->state);
  h = nes_ref_hash_bytes(h, m->prg_ram, sizeof(m->prg_ram));
  if (m->flat_mode && m->flat_ram != NULL) {
    h = nes_ref_hash_bytes(h, m->flat_ram, 65536);
  }
  h = nes_ref_hash_bytes(h, &m->open_bus, sizeof(m->open_bus));
  return h;
}
