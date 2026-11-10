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

#include <isa.h>
#include <memory/paddr.h>
#ifdef CONFIG_NES
#include <nes.h>
#endif

word_t vaddr_ifetch(vaddr_t addr, int len) {
#ifdef CONFIG_NES
  if (nes_active) {
    if (len == 1) return nes_cpu_read((uint16_t)addr);
    word_t ret = 0;
    for (int i = 0; i < len; i++) ret |= (word_t)nes_cpu_read((uint16_t)(addr + i)) << (8 * i);
    return ret;
  }
#endif
  return paddr_read(addr, len);
}

word_t vaddr_read(vaddr_t addr, int len) {
#ifdef CONFIG_NES
  if (nes_active) {
    if (len == 1) return nes_cpu_read((uint16_t)addr);
    word_t ret = 0;
    for (int i = 0; i < len; i++) ret |= (word_t)nes_cpu_read((uint16_t)(addr + i)) << (8 * i);
    return ret;
  }
#endif
  return paddr_read(addr, len);
}

void vaddr_write(vaddr_t addr, int len, word_t data) {
#ifdef CONFIG_NES
  if (nes_active) {
    for (int i = 0; i < len; i++) nes_cpu_write((uint16_t)(addr + i), (uint8_t)(data >> (8 * i)));
    return;
  }
#endif
  paddr_write(addr, len, data);
}
