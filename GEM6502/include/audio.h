/* Audio channel model adapted from NEMU/src/nes/apu.c and nes.h. */
#ifndef GEM_AUDIO_H
#define GEM_AUDIO_H
#include <stdint.h>
#include <stdbool.h>
#define GEM_CPU_HZ 1789773
#define GEM_AUDIO_HZ 44100
typedef struct GemPulse {
  bool enabled;
  uint8_t duty;
  uint8_t duty_pos;
  uint16_t timer;
  uint16_t period;
  uint8_t length;
  bool halt;
  bool constant_volume;
  uint8_t volume;
  uint8_t envelope;
  uint8_t env_divider;
  bool env_start;
  bool sweep_enable;
  uint8_t sweep_period;
  bool sweep_negate;
  uint8_t sweep_shift;
  uint16_t sweep_divider;
} GemPulse;

typedef struct GemTriangle {
  bool enabled;
  uint16_t timer;
  uint16_t period;
  uint8_t length;
  bool control;
  uint8_t linear;
  uint8_t linear_reload;
  bool linear_reload_flag;
  uint8_t pos;
} GemTriangle;

typedef struct GemNoise {
  bool enabled;
  uint16_t timer;
  uint16_t period;
  uint8_t length;
  bool halt;
  bool constant_volume;
  uint8_t volume;
  uint8_t envelope;
  uint8_t env_divider;
  bool env_start;
  uint16_t shift;
  bool mode;
} GemNoise;

typedef struct GemDmc {
  bool enabled;
  bool irq_enable;
  bool loop;
  bool irq;
  uint8_t rate_index;
  uint16_t timer;
  uint16_t period;
  uint16_t sample_addr;
  uint16_t sample_len;
  uint16_t current_addr;
  uint16_t bytes_remaining;
  uint8_t sample_buffer;
  bool buffer_full;
  uint8_t shift;
  uint8_t bits;
  uint8_t output;
} GemDmc;

typedef struct GemSound {
  GemPulse pulse[2];
  GemTriangle triangle;
  GemNoise noise;
  GemDmc dmc;
  uint8_t (*dmc_read)(void *bus, uint16_t addr);
  void *dmc_bus;
  uint8_t regs[0x18];
  uint8_t status;
  uint8_t frame_mode;
  bool frame_irq_inhibit;
  bool frame_irq;
  uint32_t frame_cycle;
  uint32_t sample_accum;
  int16_t samples[2048];
  int sample_count;
  uint32_t cycles;
  float previous_mix, high_pass;
} GemSound;
void gem_sound_reset(GemSound *a);
void gem_sound_write(GemSound *a, uint16_t addr, uint8_t value);
void gem_sound_step(GemSound *a, int cycles, bool irq_enabled);
int gem_sound_take_samples(GemSound *a, int16_t *out, int max);
#endif
