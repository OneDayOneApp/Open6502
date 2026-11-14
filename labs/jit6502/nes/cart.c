#include "nes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t prg_16k_banks(const NES_Cart *c) { return c->prg_size / 0x4000; }
static size_t chr_1k_banks(const NES_Cart *c) { return c->chr_size / 0x0400; }

static uint8_t prg_bank_read(const NES_Cart *c, size_t bank, uint16_t offset) {
  if (c->prg_size == 0) return 0;
  size_t banks = prg_16k_banks(c);
  if (banks == 0) return 0;
  bank %= banks;
  size_t pos = bank * 0x4000 + (offset & 0x3FFF);
  return pos < c->prg_size ? c->prg[pos] : 0;
}

static uint8_t prg_8k_read(const NES_Cart *c, size_t bank, uint16_t offset) {
  size_t banks = c->prg_size / 0x2000;
  if (banks == 0) return 0;
  bank %= banks;
  size_t pos = bank * 0x2000 + (offset & 0x1FFF);
  return pos < c->prg_size ? c->prg[pos] : 0;
}

static uint8_t prg_32k_read(const NES_Cart *c, size_t bank, uint16_t offset) {
  if (c->prg_size == 0) return 0;
  size_t banks = c->prg_size / 0x8000;
  if (banks == 0) return prg_bank_read(c, 0, offset);
  bank %= banks;
  size_t pos = bank * 0x8000 + (offset & 0x7FFF);
  return pos < c->prg_size ? c->prg[pos] : 0;
}

static uint8_t chr_read_index(const NES_Cart *c, size_t bank, uint16_t offset) {
  if (c->chr_size == 0) return 0;
  bank %= chr_1k_banks(c);
  return c->chr[(bank * 0x400 + (offset & 0x3FF)) % c->chr_size];
}

static void chr_write_index(NES_Cart *c, size_t bank, uint16_t offset, uint8_t value) {
  if (!c->chr_ram || c->chr_size == 0) return;
  bank %= chr_1k_banks(c);
  c->chr[(bank * 0x400 + (offset & 0x3FF)) % c->chr_size] = value;
}

bool cart_load(const char *path, NES_Cart *c) {
  memset(c, 0, sizeof(*c));
  c->mirror = 0;
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) return false;

  uint8_t hdr[16];
  if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr) ||
      hdr[0] != 'N' || hdr[1] != 'E' || hdr[2] != 'S' || hdr[3] != 0x1A) {
    fclose(fp);
    return false;
  }

  size_t prg = (size_t)hdr[4] * 0x4000;
  size_t chr = (size_t)hdr[5] * 0x2000;
  c->mapper = (hdr[7] & 0xF0) | (hdr[6] >> 4);
  c->four_screen = (hdr[6] & 0x08) != 0;
  c->mirror = c->four_screen ? 2 : (hdr[6] & 1);
  c->battery = (hdr[6] & 0x02) != 0;
  c->chr_ram = chr == 0;
  if (c->four_screen) c->mirror = 2;

  if (hdr[6] & 0x04) {
    uint8_t trainer[512];
    if (fread(trainer, 1, sizeof(trainer), fp) != sizeof(trainer)) {
      fclose(fp);
      return false;
    }
  }

  if (prg == 0) {
    fclose(fp);
    return false;
  }
  c->prg = malloc(prg);
  c->chr = malloc(chr ? chr : 0x2000);
  if (c->prg == NULL || c->chr == NULL) {
    fclose(fp);
    cart_free(c);
    return false;
  }
  c->prg_size = prg;
  c->chr_size = chr ? chr : 0x2000;
  if (fread(c->prg, 1, prg, fp) != prg) {
    fclose(fp);
    cart_free(c);
    return false;
  }
  if (chr != 0) {
    if (fread(c->chr, 1, chr, fp) != chr) {
      fclose(fp);
      cart_free(c);
      return false;
    }
  } else {
    memset(c->chr, 0, c->chr_size);
  }
  fclose(fp);

  c->shift = 0x10;
  c->mmc1_control = 0x0C;
  c->prg_ram_enable = true;
  c->irq_counter = 0;
  c->irq_latch = 0;
  return true;
}

void cart_free(NES_Cart *c) {
  free(c->prg);
  free(c->chr);
  c->prg = NULL;
  c->chr = NULL;
  c->prg_size = c->chr_size = 0;
}

uint8_t cart_cpu_read(NES_Cart *c, uint16_t addr) {
  if (addr >= 0x6000 && addr <= 0x7FFF) return c->prg_ram[addr & 0x1FFF];
  if (addr < 0x8000) return 0;

  switch (c->mapper) {
    case 0:
      return prg_32k_read(c, 0, addr - 0x8000);
    case 1: {
      size_t mode = (c->mmc1_control >> 2) & 3;
      size_t banks = prg_16k_banks(c);
      if (banks == 0) return 0;
      if (mode <= 1) return prg_32k_read(c, c->mmc1_prg >> 1, addr - 0x8000);
      if (mode == 2) {
        if (addr < 0xC000) return prg_bank_read(c, 0, addr - 0x8000);
        return prg_bank_read(c, c->mmc1_prg, addr - 0xC000);
      }
      if (addr < 0xC000) return prg_bank_read(c, c->mmc1_prg, addr - 0x8000);
      return prg_bank_read(c, banks - 1, addr - 0xC000);
    }
    case 2:
      if (addr < 0xC000) return prg_bank_read(c, c->prg_bank, addr - 0x8000);
      return prg_bank_read(c, prg_16k_banks(c) - 1, addr - 0xC000);
    case 3:
      return prg_32k_read(c, 0, addr - 0x8000);
    case 4: {
      size_t banks = c->prg_size / 0x2000;
      if (banks == 0) return 0;
      size_t r6 = c->bank_regs[6] % banks;
      size_t r7 = c->bank_regs[7] % banks;
      if (c->prg_mode == 0) {
        if (addr < 0xA000) return prg_8k_read(c, r6, addr - 0x8000);
        if (addr < 0xC000) return prg_8k_read(c, r7, addr - 0xA000);
        if (addr < 0xE000) return prg_8k_read(c, banks - 2, addr - 0xC000);
        return prg_8k_read(c, banks - 1, addr - 0xE000);
      }
      if (addr < 0xA000) return prg_8k_read(c, banks - 2, addr - 0x8000);
      if (addr < 0xC000) return prg_8k_read(c, r7, addr - 0xA000);
      if (addr < 0xE000) return prg_8k_read(c, r6, addr - 0xC000);
      return prg_8k_read(c, banks - 1, addr - 0xE000);
    }
    case 7:
      return prg_32k_read(c, c->prg_bank, addr - 0x8000);
    default:
      return prg_32k_read(c, 0, addr - 0x8000);
  }
}

void cart_cpu_write(NES_Cart *c, uint16_t addr, uint8_t value) {
  if (addr >= 0x6000 && addr <= 0x7FFF) {
    c->prg_ram[addr & 0x1FFF] = value;
    return;
  }
  if (addr < 0x8000) return;

  switch (c->mapper) {
    case 0:
      break;
    case 1: {
      if (value & 0x80) {
        c->shift = 0x10;
        c->mmc1_control |= 0x0C;
        return;
      }
      bool complete = (c->shift & 1) != 0;
      c->shift = (uint8_t)((c->shift >> 1) | ((value & 1) << 4));
      if (complete) {
        uint8_t out = c->shift & 0x1F;
        c->shift = 0x10;
        switch ((addr >> 13) & 3) {
          case 0: c->mmc1_control = out; break;
          case 1: c->mmc1_chr0 = out; break;
          case 2: c->mmc1_chr1 = out; break;
          case 3: c->mmc1_prg = out; break;
        }
      }
      break;
    }
    case 2:
      c->prg_bank = value;
      break;
    case 3:
      c->chr_bank = value;
      break;
    case 4:
      if (addr < 0xA000) {
        if ((addr & 1) == 0) {
          c->bank_select = value;
          c->prg_mode = (value >> 6) & 1;
          c->chr_mode = (value >> 7) & 1;
        } else {
          c->bank_regs[c->bank_select & 7] = value;
        }
      } else if (addr < 0xC000) {
        if ((addr & 1) == 0) c->mirror = value & 1;
        else c->prg_ram_enable = (value & 0x80) != 0;
      } else if (addr < 0xE000) {
        if ((addr & 1) == 0) c->irq_latch = value;
        else c->irq_reload = true;
      } else {
        if ((addr & 1) == 0) {
          c->irq_enable = false;
          c->irq = false;
          c->irq_counter = 0;
        } else {
          c->irq_enable = true;
        }
      }
      break;
    case 7:
      c->prg_bank = value & 0x0F;
      c->mirror = (value & 0x10) ? 1 : 0;
      break;
    default:
      break;
  }
}

uint8_t cart_chr_read(NES_Cart *c, uint16_t addr) {
  addr &= 0x1FFF;
  switch (c->mapper) {
    case 1:
      if (c->mmc1_control & 0x10) {
        if (addr < 0x1000) return chr_read_index(c, c->mmc1_chr0, addr);
        return chr_read_index(c, c->mmc1_chr1, addr);
      } else {
        return chr_read_index(c, c->mmc1_chr0 >> 1, addr);
      }
    case 3:
      return chr_read_index(c, (size_t)c->chr_bank * 8, addr);
    case 4:
      if (c->chr_mode == 0) {
        if (addr < 0x0800) return chr_read_index(c, c->bank_regs[0] & 0xFE, addr);
        if (addr < 0x1000) return chr_read_index(c, c->bank_regs[1] & 0xFE, addr);
        if (addr < 0x1400) return chr_read_index(c, c->bank_regs[2], addr);
        if (addr < 0x1800) return chr_read_index(c, c->bank_regs[3], addr);
        if (addr < 0x1C00) return chr_read_index(c, c->bank_regs[4], addr);
        return chr_read_index(c, c->bank_regs[5], addr);
      } else {
        if (addr < 0x0400) return chr_read_index(c, c->bank_regs[2], addr);
        if (addr < 0x0800) return chr_read_index(c, c->bank_regs[3], addr);
        if (addr < 0x0C00) return chr_read_index(c, c->bank_regs[4], addr);
        if (addr < 0x1000) return chr_read_index(c, c->bank_regs[5], addr);
        if (addr < 0x1800) return chr_read_index(c, c->bank_regs[0] & 0xFE, addr);
        return chr_read_index(c, c->bank_regs[1] & 0xFE, addr);
      }
    default:
      return c->chr_size ? c->chr[addr % c->chr_size] : 0;
  }
}

void cart_chr_write(NES_Cart *c, uint16_t addr, uint8_t value) {
  addr &= 0x1FFF;
  if (!c->chr_ram) return;
  switch (c->mapper) {
    case 1:
      if (c->mmc1_control & 0x10) {
        if (addr < 0x1000) chr_write_index(c, c->mmc1_chr0, addr, value);
        else chr_write_index(c, c->mmc1_chr1, addr, value);
      } else {
        chr_write_index(c, c->mmc1_chr0 >> 1, addr, value);
      }
      break;
    case 3:
      chr_write_index(c, (size_t)c->chr_bank * 8, addr, value);
      break;
    case 4:
      if (c->chr_mode == 0) {
        if (addr < 0x0800) chr_write_index(c, c->bank_regs[0] & 0xFE, addr, value);
        else if (addr < 0x1000) chr_write_index(c, c->bank_regs[1] & 0xFE, addr, value);
        else if (addr < 0x1400) chr_write_index(c, c->bank_regs[2], addr, value);
        else if (addr < 0x1800) chr_write_index(c, c->bank_regs[3], addr, value);
        else if (addr < 0x1C00) chr_write_index(c, c->bank_regs[4], addr, value);
        else chr_write_index(c, c->bank_regs[5], addr, value);
      } else {
        if (addr < 0x0400) chr_write_index(c, c->bank_regs[2], addr, value);
        else if (addr < 0x0800) chr_write_index(c, c->bank_regs[3], addr, value);
        else if (addr < 0x0C00) chr_write_index(c, c->bank_regs[4], addr, value);
        else if (addr < 0x1000) chr_write_index(c, c->bank_regs[5], addr, value);
        else if (addr < 0x1800) chr_write_index(c, c->bank_regs[0] & 0xFE, addr, value);
        else chr_write_index(c, c->bank_regs[1] & 0xFE, addr, value);
      }
      break;
    default:
      c->chr[addr % c->chr_size] = value;
      break;
  }
}

int cart_nt_index(NES_Cart *c, uint16_t addr) {
  int nt = (addr >> 10) & 3;
  if (c->four_screen) return nt;
  if (c->mapper == 7) return c->mirror & 1;
  if (c->mapper == 1) {
    switch (c->mmc1_control & 3) {
      case 0: return 0;
      case 1: return 1;
      case 2: return nt & 1;       /* vertical */
      case 3: return nt >> 1;      /* horizontal */
    }
  }
  if (c->mapper == 4) {
    return c->mirror ? (nt >> 1) : (nt & 1);
  }
  if (c->mirror == 0) return nt >> 1;     /* horizontal */
  if (c->mirror == 1) return nt & 1;      /* vertical */
  return nt;
}

void cart_scanline(NES_Cart *c) {
  if (c->mapper != 4 || !c->irq_enable) return;
  if (c->irq_counter <= 0 || c->irq_reload) {
    c->irq_counter = c->irq_latch;
    c->irq_reload = false;
  } else {
    c->irq_counter--;
  }
  if (c->irq_counter == 0) {
    c->irq = true;
  }
}
