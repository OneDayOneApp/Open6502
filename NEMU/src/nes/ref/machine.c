#include "nes/ref/nes_ref.h"

#include <stdlib.h>
#include <string.h>

static size_t prg_16k_banks(const nes_ref_machine *m) {
  return m->rom.prg_size / 0x4000;
}

static size_t chr_1k_banks(const nes_ref_machine *m) {
  return m->rom.chr_size / 0x400;
}

static uint8_t prg_16k_read(const nes_ref_machine *m, size_t bank, uint16_t off) {
  size_t banks = prg_16k_banks(m);
  if (banks == 0) return 0;
  bank %= banks;
  size_t pos = bank * 0x4000 + (off & 0x3FFF);
  return pos < m->rom.prg_size ? m->rom.prg[pos] : 0;
}

static uint8_t prg_8k_read(const nes_ref_machine *m, size_t bank, uint16_t off) {
  size_t banks = m->rom.prg_size / 0x2000;
  if (banks == 0) return 0;
  bank %= banks;
  size_t pos = bank * 0x2000 + (off & 0x1FFF);
  return pos < m->rom.prg_size ? m->rom.prg[pos] : 0;
}

static uint8_t prg_32k_read(const nes_ref_machine *m, size_t bank, uint16_t off) {
  size_t banks = m->rom.prg_size / 0x8000;
  if (banks == 0) return prg_16k_read(m, 0, off);
  bank %= banks;
  size_t pos = bank * 0x8000 + (off & 0x7FFF);
  return pos < m->rom.prg_size ? m->rom.prg[pos] : 0;
}

static uint8_t chr_1k_read(const nes_ref_machine *m, size_t bank, uint16_t off) {
  size_t banks = chr_1k_banks(m);
  if (banks == 0) return 0;
  bank %= banks;
  return m->rom.chr[bank * 0x400 + (off & 0x3FF)];
}

static void chr_1k_write(nes_ref_machine *m, size_t bank, uint16_t off, uint8_t value) {
  if (!m->rom.chr_ram) return;
  size_t banks = chr_1k_banks(m);
  if (banks == 0) return;
  bank %= banks;
  m->rom.chr[bank * 0x400 + (off & 0x3FF)] = value;
}

static uint8_t mapper_prg_read(const nes_ref_machine *m, uint16_t addr) {
  if (!m->loaded || addr < 0x8000) return 0;
  uint16_t off = (uint16_t)(addr - 0x8000);
  switch (m->rom.mapper) {
    case 0:
      return prg_32k_read(m, 0, off);
    case 1: {
      size_t mode = (m->mapper.mmc1_control >> 2) & 3;
      size_t banks = prg_16k_banks(m);
      if (banks == 0) return 0;
      if (mode <= 1) return prg_32k_read(m, m->mapper.mmc1_prg >> 1, off);
      if (mode == 2) {
        if (addr < 0xC000) return prg_16k_read(m, 0, off);
        return prg_16k_read(m, m->mapper.mmc1_prg, off);
      }
      if (addr < 0xC000) return prg_16k_read(m, m->mapper.mmc1_prg, off);
      return prg_16k_read(m, banks - 1, off);
    }
    case 2:
      if (addr < 0xC000) return prg_16k_read(m, m->mapper.prg_bank, off);
      return prg_16k_read(m, prg_16k_banks(m) ? prg_16k_banks(m) - 1 : 0, off);
    case 3:
      return prg_32k_read(m, 0, off);
    case 4: {
      size_t banks = m->rom.prg_size / 0x2000;
      if (banks == 0) return 0;
      size_t r6 = m->mapper.bank_regs[6] % banks;
      size_t r7 = m->mapper.bank_regs[7] % banks;
      if (m->mapper.prg_mode == 0) {
        if (addr < 0xA000) return prg_8k_read(m, r6, off);
        if (addr < 0xC000) return prg_8k_read(m, r7, off);
        if (addr < 0xE000) return prg_8k_read(m, banks - 2, off);
        return prg_8k_read(m, banks - 1, off);
      }
      if (addr < 0xA000) return prg_8k_read(m, banks - 2, off);
      if (addr < 0xC000) return prg_8k_read(m, r7, off);
      if (addr < 0xE000) return prg_8k_read(m, r6, off);
      return prg_8k_read(m, banks - 1, off);
    }
    case 7:
      return prg_32k_read(m, m->mapper.prg_bank, off);
    default:
      return 0;
  }
}

static void mapper_write(nes_ref_machine *m, uint16_t addr, uint8_t value) {
  if (!m->loaded || addr < 0x8000) return;
  switch (m->rom.mapper) {
    case 0:
      break;
    case 1:
      if (value & 0x80) {
        m->mapper.shift = 0x10;
        m->mapper.mmc1_control |= 0x0C;
        return;
      }
      {
        bool complete = (m->mapper.shift & 1) != 0;
        m->mapper.shift = (uint8_t)((m->mapper.shift >> 1) | ((value & 1) << 4));
        if (complete) {
          uint8_t out = m->mapper.shift & 0x1F;
          m->mapper.shift = 0x10;
          switch ((addr >> 13) & 3) {
            case 0: m->mapper.mmc1_control = out; break;
            case 1: m->mapper.mmc1_chr0 = out; break;
            case 2: m->mapper.mmc1_chr1 = out; break;
            case 3: m->mapper.mmc1_prg = out; break;
          }
        }
      }
      break;
    case 2:
      m->mapper.prg_bank = value;
      break;
    case 3:
      m->mapper.chr_bank = value;
      break;
    case 4:
      if (addr < 0xA000) {
        if ((addr & 1) == 0) {
          m->mapper.bank_select = value;
          m->mapper.prg_mode = (value >> 6) & 1;
          m->mapper.chr_mode = (value >> 7) & 1;
        } else {
          m->mapper.bank_regs[m->mapper.bank_select & 7] = value;
        }
      } else if (addr < 0xC000) {
        if ((addr & 1) == 0) m->mapper.mirroring = value & 1;
        else m->mapper.prg_ram_enable = (value & 0x80) != 0;
      } else if (addr < 0xE000) {
        if ((addr & 1) == 0) m->mapper.irq_latch = value;
        else m->mapper.irq_reload = true;
      } else {
        if ((addr & 1) == 0) {
          m->mapper.irq_enable = false;
          m->mapper.irq = false;
          m->mapper.irq_counter = 0;
        } else {
          m->mapper.irq_enable = true;
        }
      }
      break;
    case 7:
      m->mapper.prg_bank = value & 0x0F;
      m->mapper.mirroring = (value & 0x10) ? 1 : 0;
      break;
    default:
      break;
  }
}

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
  m->state.cpu = (nes_ref_cpu){0};
  m->state.cpu.sp = 0xFD;
  m->state.cpu.p = 0x24;
  m->state.cpu.pc = entry;
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

void nes_ref_machine_reset(nes_ref_machine *m) {
  if (m == NULL) return;
  nes_ref_region region = m->state.clock.region;
  bool flat_mode = m->flat_mode;
  uint8_t *flat_ram = m->flat_ram;
  memset(&m->state, 0, sizeof(m->state));
  m->flat_mode = flat_mode;
  m->flat_ram = flat_ram;
  nes_ref_clock_init(&m->state.clock, region);
  memset(&m->mapper, 0, sizeof(m->mapper));
  m->mapper.shift = 0x10;
  m->mapper.mmc1_control = 0x0C;
  m->mapper.prg_ram_enable = true;
  m->mapper.mirroring = m->rom.mirroring;
  memset(m->prg_ram, 0, sizeof(m->prg_ram));
  m->open_bus = 0;
  m->state.cpu.sp = 0xFD;
  m->state.cpu.p = 0x24;
  if (m->loaded && !m->flat_mode) {
    m->state.cpu.pc = (uint16_t)(mapper_prg_read(m, 0xFFFC) |
        (mapper_prg_read(m, 0xFFFD) << 8));
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
  else v = mapper_prg_read(m, addr);
  m->open_bus = v;
  return v;
}

void nes_ref_machine_write(nes_ref_machine *m, uint16_t addr, uint8_t value) {
  if (m->flat_mode) {
    m->flat_ram[addr] = value;
    return;
  }
  if (addr < 0x2000) m->state.ram[addr & 0x7FF] = value;
  else if (addr < 0x6000) { /* PPU/APU/IO: not modeled yet */ }
  else if (addr < 0x8000) m->prg_ram[(addr - 0x6000) & 0x1FFF] = value;
  else mapper_write(m, addr, value);
  m->open_bus = value;
}

uint8_t nes_ref_machine_chr_read(nes_ref_machine *m, uint16_t addr) {
  addr &= 0x1FFF;
  switch (m->rom.mapper) {
    case 1:
      if (m->mapper.mmc1_control & 0x10) {
        if (addr < 0x1000) return chr_1k_read(m, m->mapper.mmc1_chr0, addr);
        return chr_1k_read(m, m->mapper.mmc1_chr1, (uint16_t)(addr - 0x1000));
      }
      return chr_1k_read(m, m->mapper.mmc1_chr0 >> 1, addr);
    case 3:
      return chr_1k_read(m, (size_t)m->mapper.chr_bank * 8, addr);
    case 4:
      if (m->mapper.chr_mode == 0) {
        if (addr < 0x0800) return chr_1k_read(m, m->mapper.bank_regs[0] & 0xFE, addr);
        if (addr < 0x1000) return chr_1k_read(m, m->mapper.bank_regs[1] & 0xFE, addr);
        if (addr < 0x1400) return chr_1k_read(m, m->mapper.bank_regs[2], (uint16_t)(addr - 0x1000));
        if (addr < 0x1800) return chr_1k_read(m, m->mapper.bank_regs[3], (uint16_t)(addr - 0x1400));
        if (addr < 0x1C00) return chr_1k_read(m, m->mapper.bank_regs[4], (uint16_t)(addr - 0x1800));
        return chr_1k_read(m, m->mapper.bank_regs[5], (uint16_t)(addr - 0x1C00));
      }
      if (addr < 0x0400) return chr_1k_read(m, m->mapper.bank_regs[2], addr);
      if (addr < 0x0800) return chr_1k_read(m, m->mapper.bank_regs[3], (uint16_t)(addr - 0x0400));
      if (addr < 0x0C00) return chr_1k_read(m, m->mapper.bank_regs[4], (uint16_t)(addr - 0x0800));
      if (addr < 0x1000) return chr_1k_read(m, m->mapper.bank_regs[5], (uint16_t)(addr - 0x0C00));
      if (addr < 0x1800) return chr_1k_read(m, m->mapper.bank_regs[0] & 0xFE, (uint16_t)(addr - 0x1000));
      return chr_1k_read(m, m->mapper.bank_regs[1] & 0xFE, (uint16_t)(addr - 0x1800));
    default:
      return m->rom.chr_size ? m->rom.chr[addr % m->rom.chr_size] : 0;
  }
}

void nes_ref_machine_chr_write(nes_ref_machine *m, uint16_t addr, uint8_t value) {
  addr &= 0x1FFF;
  if (!m->rom.chr_ram) return;
  switch (m->rom.mapper) {
    case 1:
      if (m->mapper.mmc1_control & 0x10) {
        if (addr < 0x1000) chr_1k_write(m, m->mapper.mmc1_chr0, addr, value);
        else chr_1k_write(m, m->mapper.mmc1_chr1, (uint16_t)(addr - 0x1000), value);
      } else {
        chr_1k_write(m, m->mapper.mmc1_chr0 >> 1, addr, value);
      }
      break;
    case 3:
      chr_1k_write(m, (size_t)m->mapper.chr_bank * 8, addr, value);
      break;
    case 4:
      if (m->mapper.chr_mode == 0) {
        if (addr < 0x0800) chr_1k_write(m, m->mapper.bank_regs[0] & 0xFE, addr, value);
        else if (addr < 0x1000) chr_1k_write(m, m->mapper.bank_regs[1] & 0xFE, (uint16_t)(addr - 0x0800), value);
        else if (addr < 0x1400) chr_1k_write(m, m->mapper.bank_regs[2], (uint16_t)(addr - 0x1000), value);
        else if (addr < 0x1800) chr_1k_write(m, m->mapper.bank_regs[3], (uint16_t)(addr - 0x1400), value);
        else if (addr < 0x1C00) chr_1k_write(m, m->mapper.bank_regs[4], (uint16_t)(addr - 0x1800), value);
        else chr_1k_write(m, m->mapper.bank_regs[5], (uint16_t)(addr - 0x1C00), value);
      } else {
        if (addr < 0x0400) chr_1k_write(m, m->mapper.bank_regs[2], addr, value);
        else if (addr < 0x0800) chr_1k_write(m, m->mapper.bank_regs[3], (uint16_t)(addr - 0x0400), value);
        else if (addr < 0x0C00) chr_1k_write(m, m->mapper.bank_regs[4], (uint16_t)(addr - 0x0800), value);
        else if (addr < 0x1000) chr_1k_write(m, m->mapper.bank_regs[5], (uint16_t)(addr - 0x0C00), value);
        else if (addr < 0x1800) chr_1k_write(m, m->mapper.bank_regs[0] & 0xFE, (uint16_t)(addr - 0x1000), value);
        else chr_1k_write(m, m->mapper.bank_regs[1] & 0xFE, (uint16_t)(addr - 0x1800), value);
      }
      break;
    default:
      m->rom.chr[addr % m->rom.chr_size] = value;
      break;
  }
}

int nes_ref_machine_nt_index(const nes_ref_machine *m, uint16_t addr) {
  int nt = (addr >> 10) & 3;
  if (m->rom.four_screen) return nt;
  if (m->rom.mapper == 7) return m->mapper.mirroring & 1;
  if (m->rom.mapper == 1) {
    switch (m->mapper.mmc1_control & 3) {
      case 0: return 0;
      case 1: return 1;
      case 2: return nt & 1;
      case 3: return nt >> 1;
    }
  }
  if (m->rom.mapper == 4) {
    return m->mapper.mirroring ? (nt >> 1) : (nt & 1);
  }
  if (m->mapper.mirroring == 0) return nt >> 1;
  if (m->mapper.mirroring == 1) return nt & 1;
  return nt;
}

uint64_t nes_ref_machine_hash(const nes_ref_machine *m) {
  uint64_t h = nes_ref_state_hash(&m->state);
  h = nes_ref_hash_bytes(h, &m->mapper, sizeof(m->mapper));
  h = nes_ref_hash_bytes(h, m->prg_ram, sizeof(m->prg_ram));
  if (m->flat_mode && m->flat_ram != NULL) {
    h = nes_ref_hash_bytes(h, m->flat_ram, 65536);
  }
  h = nes_ref_hash_bytes(h, &m->open_bus, sizeof(m->open_bus));
  return h;
}
