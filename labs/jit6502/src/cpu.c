#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cpu_init(Cpu *c, uint8_t *mem) {
  memset(c, 0, sizeof(*c));
  memset(mem, 0, MEM_SIZE);
  c->mem = mem;
}

bool cpu_load(Cpu *c, const char *path, uint16_t load_addr) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) return false;

  size_t nread = 0;
  if (load_addr != 0) {
    /* For images assembled for $8000-style layouts: zero-fill the hole. */
    memset(c->mem, 0, MEM_SIZE);
  }
  nread = fread(c->mem + load_addr, 1, MEM_SIZE - load_addr, fp);
  fclose(fp);
  return nread > 0;
}

void cpu_reset(Cpu *c) {
  c->a = 0;
  c->x = 0;
  c->y = 0;
  c->sp = 0xFD;
  c->p = F_U | F_I;
  c->cycles = 0;
  c->pc = (uint16_t)(c->mem[0xFFFC] | ((uint16_t)c->mem[0xFFFD] << 8));
}
