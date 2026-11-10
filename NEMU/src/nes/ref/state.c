#include "nes/ref/nes_ref.h"

#include <string.h>

static uint64_t hash_u8(uint64_t h, uint8_t v) {
  return nes_ref_hash_bytes(h, &v, 1);
}

static uint64_t hash_u32(uint64_t h, uint32_t v) {
  uint8_t b[4] = {
    (uint8_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF),
    (uint8_t)((v >> 16) & 0xFF), (uint8_t)((v >> 24) & 0xFF)
  };
  return nes_ref_hash_bytes(h, b, sizeof(b));
}

static uint64_t hash_u64(uint64_t h, uint64_t v) {
  uint8_t b[8];
  for (int i = 0; i < 8; i++) b[i] = (uint8_t)((v >> (i * 8)) & 0xFF);
  return nes_ref_hash_bytes(h, b, sizeof(b));
}

uint64_t nes_ref_hash_bytes(uint64_t seed, const void *data, size_t size) {
  const uint8_t *p = data;
  uint64_t h = seed;
  for (size_t i = 0; i < size; i++) {
    h ^= p[i];
    h *= UINT64_C(1099511628211);
  }
  return h;
}

uint64_t nes_ref_state_hash(const nes_ref_state *s) {
  uint64_t h = NES_REF_HASH_SEED;
  if (s == NULL) return h;
  h = hash_u32(h, s->clock.region);
  h = hash_u64(h, s->clock.master_tick);
  h = hash_u64(h, s->clock.cpu_cycle);
  h = hash_u64(h, s->clock.ppu_dot);
  h = hash_u32(h, s->clock.ppu_cycle);
  h = hash_u32(h, s->clock.ppu_scanline);
  h = hash_u64(h, s->clock.frame);
  h = hash_u32(h, s->clock.cpu_phase);
  h = hash_u32(h, s->clock.ppu_phase);
  h = hash_u8(h, s->cpu.a);
  h = hash_u8(h, s->cpu.x);
  h = hash_u8(h, s->cpu.y);
  h = hash_u8(h, s->cpu.sp);
  h = hash_u8(h, s->cpu.p);
  h = hash_u32(h, s->cpu.pc);
  h = hash_u64(h, s->cpu.cycles);
  h = hash_u8(h, (uint8_t)s->cpu.nmi_pending);
  h = hash_u8(h, (uint8_t)s->cpu.irq_pending);
  h = hash_u8(h, (uint8_t)s->cpu.jammed);
  h = nes_ref_hash_bytes(h, s->ram, sizeof(s->ram));
  h = nes_ref_hash_bytes(h, s->framebuffer_hash, sizeof(s->framebuffer_hash));
  h = nes_ref_hash_bytes(h, s->audio_hash, sizeof(s->audio_hash));
  return h;
}
