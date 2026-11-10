#include "nes.h"

uint8_t cart_chr_read(NES_Cart *c, uint16_t addr) { (void)c; (void)addr; return 0; }
void cart_chr_write(NES_Cart *c, uint16_t addr, uint8_t value) { (void)c; (void)addr; (void)value; }
int cart_nt_index(NES_Cart *c, uint16_t addr) { (void)c; (void)addr; return 0; }
void cart_scanline(NES_Cart *c) { (void)c; }
