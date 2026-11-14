/*
 * Decimal-mode ADC/SBC helpers.
 *
 * These follow the NMOS flag rules (N/Z/C from the decimal result, V from the
 * binary sum).  Keeping them out of the emitted code avoids duplicating the
 * BCD adjust sequence in machine code for a path real games rarely take.
 */
#include <stdint.h>

enum { F_C = 0x01, F_Z = 0x02, F_V = 0x40, F_N = 0x80 };

static uint16_t pack(uint8_t a, uint8_t p) {
  return (uint16_t)(((uint16_t)p << 8) | a);
}

static uint8_t set_nz(uint8_t p, uint8_t v) {
  return (uint8_t)((p & ~(F_N | F_Z)) | (v & F_N) | (v == 0 ? F_Z : 0));
}

uint16_t jit_adc_decimal(uint8_t a, uint8_t m, uint8_t p) {
  unsigned carry = (p & F_C) ? 1u : 0u;
  unsigned al = (unsigned)(a & 0x0F) + (m & 0x0F) + carry;
  unsigned ah = (unsigned)(a >> 4) + (m >> 4);
  unsigned bin = (unsigned)a + m + carry;

  if (!((a ^ m) & 0x80) && ((a ^ bin) & 0x80)) p |= F_V;
  else p = (uint8_t)(p & ~F_V);

  if (al > 9) al += 6;
  if (al > 0x0F) ah++;
  if (ah > 9) ah += 6;
  if (ah > 0x0F) p |= F_C; else p = (uint8_t)(p & ~F_C);

  uint8_t res = (uint8_t)(((ah & 0x0F) << 4) | (al & 0x0F));
  return pack(res, set_nz(p, res));
}

uint16_t jit_sbc_decimal(uint8_t a, uint8_t m, uint8_t p) {
  unsigned borrow = (p & F_C) ? 1u : 0u;
  unsigned diff = (unsigned)a - m - (1u - borrow);
  unsigned al = (unsigned)(a & 0x0F) - (m & 0x0F) - (1u - borrow);
  unsigned ah = (unsigned)(a >> 4) - (m >> 4);

  if (((a ^ m) & 0x80) && ((a ^ diff) & 0x80)) p |= F_V;
  else p = (uint8_t)(p & ~F_V);

  if (al & 0x10u) {
    al -= 6;
    ah--;
  }
  if (ah & 0x10u) ah -= 6;
  if (diff < 0x100u) p |= F_C; else p = (uint8_t)(p & ~F_C);

  uint8_t res = (uint8_t)(((ah & 0x0F) << 4) | (al & 0x0F));
  return pack(res, set_nz(p, res));
}
