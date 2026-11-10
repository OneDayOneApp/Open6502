/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <memory/host.h>
#include <memory/paddr.h>
#include <device/mmio.h>
#include <isa.h>
#include <cpu/cpu.h>

#if   defined(CONFIG_PMEM_MALLOC)
static uint8_t *pmem = NULL;
#else // CONFIG_PMEM_GARRAY
static uint8_t pmem[CONFIG_MSIZE] PG_ALIGN = {};
#endif

uint8_t* guest_to_host(paddr_t paddr) { return pmem + paddr - CONFIG_MBASE; }
paddr_t host_to_guest(uint8_t *haddr) { return haddr - pmem + CONFIG_MBASE; }

static word_t pmem_read(paddr_t addr, int len) {
  word_t ret = host_read(guest_to_host(addr), len);
  return ret;
}

static void pmem_write(paddr_t addr, int len, word_t data) {
  host_write(guest_to_host(addr), len, data);
}

static void out_of_bound(paddr_t addr) {
  panic("address = " FMT_PADDR " is out of bound of pmem [" FMT_PADDR ", " FMT_PADDR "] at pc = " FMT_WORD,
      addr, PMEM_LEFT, PMEM_RIGHT, cpu.pc);
}

void init_mem() {
#if   defined(CONFIG_PMEM_MALLOC)
  pmem = malloc(CONFIG_MSIZE);
  assert(pmem);
#endif
  IFDEF(CONFIG_MEM_RANDOM, memset(pmem, rand(), CONFIG_MSIZE));
  Log("physical memory area [" FMT_PADDR ", " FMT_PADDR "]", PMEM_LEFT, PMEM_RIGHT);
}

#ifdef CONFIG_ISA_m6502
/* A tiny host I/O window.  $F001 writes a byte to stdout; $F002 terminates
 * the simulation with the written byte as the exit status.  These addresses
 * are only intercepted for 6502 configurations. */
static bool m6502_io_write(paddr_t addr, int len, word_t data) {
  if (len != 1) return false;
#ifdef CONFIG_TARGET_SHARE
  /* A REF shared object must not consume input or produce host output. */
  if (addr == 0xF001 || addr == 0xF002) return true;
  return false;
#else
  switch (addr) {
    case 0xF001:
      putchar((int)(data & 0xFF));
      fflush(stdout);
      return true;
    case 0xF002:
      set_nemu_state(NEMU_END, cpu.pc, (int)(data & 0xFF));
      return true;
    default:
      return false;
  }
#endif
}

static bool m6502_io_read(paddr_t addr, int len, word_t *data) {
  if (len != 1) return false;
  switch (addr) {
    case 0xF001:
      *data = 0;
      return true;
    case 0xF003: {
#ifdef CONFIG_TARGET_SHARE
      *data = 0;
      return true;
#endif
      int ch = getchar();
      if (ch == EOF) {
        /* The guest is waiting for console input, but the host stream ended.
         * End the simulation cleanly instead of spinning forever. */
        set_nemu_state(NEMU_END, cpu.pc, 0);
        *data = 0;
      } else {
        /* 6502 BASIC expects CR, while Unix terminals/pipes usually deliver LF. */
        if (ch == '\n' || ch == '\r') ch = '\r';
        *data = (word_t)(ch & 0xFF);
      }
      return true;
    }
    default:
      return false;
  }
}
#endif

word_t paddr_read(paddr_t addr, int len) {
#ifdef CONFIG_ISA_m6502
  word_t io_data;
  if (m6502_io_read(addr, len, &io_data)) return io_data;
#endif
  if (likely(in_pmem(addr))) return pmem_read(addr, len);
  IFDEF(CONFIG_DEVICE, return mmio_read(addr, len));
  out_of_bound(addr);
  return 0;
}

void paddr_write(paddr_t addr, int len, word_t data) {
#ifdef CONFIG_ISA_m6502
  if (m6502_io_write(addr, len, data)) return;
#endif
  if (likely(in_pmem(addr))) { pmem_write(addr, len, data); return; }
  IFDEF(CONFIG_DEVICE, mmio_write(addr, len, data); return);
  out_of_bound(addr);
}
