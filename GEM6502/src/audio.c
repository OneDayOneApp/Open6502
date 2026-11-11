/* Audio synthesis adapted from NEMU/src/nes/apu.c.
 * IRQ delivery stays in GEM6502; this model supplies PCM only. */
#include "audio.h"
#include <string.h>

static const uint8_t length_table[32] = {
  10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
  12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};

static const uint16_t dmc_rate_table[16] = {
  428, 380, 340, 320, 286, 254, 226, 214,
  190, 160, 142, 128, 106, 84, 72, 54,
};

static const uint8_t duty_table[4][8] = {
  {0, 1, 0, 0, 0, 0, 0, 0},
  {0, 1, 1, 0, 0, 0, 0, 0},
  {0, 1, 1, 1, 1, 0, 0, 0},
  {1, 0, 0, 1, 1, 1, 1, 1},
};

static const uint16_t noise_period_table[16] = {
  4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
};

static void pulse_write(GemPulse *p, uint8_t reg, uint8_t value) {
  switch (reg & 3) {
    case 0:
      p->duty = value >> 6;
      p->halt = (value & 0x20) != 0;
      p->constant_volume = (value & 0x10) != 0;
      p->volume = value & 0x0F;
      break;
    case 1:
      p->sweep_enable = (value & 0x80) != 0;
      p->sweep_period = (value >> 4) & 7;
      p->sweep_negate = (value & 0x08) != 0;
      p->sweep_shift = value & 7;
      break;
    case 2:
      p->period = (uint16_t)((p->period & 0x0700) | value);
      break;
    case 3:
      p->period = (uint16_t)((p->period & 0x00FF) | ((value & 7) << 8));
      p->length = length_table[value >> 3];
      p->env_start = true;
      break;
  }
}

static void triangle_write(GemTriangle *t, uint8_t reg, uint8_t value) {
  switch (reg & 3) {
    case 0:
      t->control = (value & 0x80) != 0;
      t->linear_reload = value & 0x7F;
      break;
    case 2:
      t->period = (uint16_t)((t->period & 0x0700) | value);
      break;
    case 3:
      t->period = (uint16_t)((t->period & 0x00FF) | ((value & 7) << 8));
      t->length = length_table[value >> 3];
      t->linear_reload_flag = true;
      break;
  }
}

static void noise_write(GemNoise *n, uint8_t reg, uint8_t value) {
  switch (reg & 3) {
    case 0:
      n->halt = (value & 0x20) != 0;
      n->constant_volume = (value & 0x10) != 0;
      n->volume = value & 0x0F;
      break;
    case 2:
      n->period = noise_period_table[value & 0x0F];
      n->mode = (value & 0x80) != 0;
      break;
    case 3:
      n->length = length_table[value >> 3];
      n->env_start = true;
      break;
  }
}

static void pulse_clock(GemPulse *p) {
  if (p->timer == 0) {
    p->timer = p->period;
    p->duty_pos = (p->duty_pos + 1) & 7;
  } else {
    p->timer--;
  }
}

static int pulse_output(GemPulse *p) {
  if (!p->enabled || p->length == 0 || p->period < 8) return 0;
  if (!duty_table[p->duty & 3][p->duty_pos & 7]) return 0;
  return p->constant_volume ? p->volume : p->envelope;
}

static int triangle_output(GemTriangle *t) {
  static const uint8_t seq[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  if (!t->enabled || t->length == 0 || t->linear == 0 || t->period < 2) return 0;
  return seq[t->pos & 31];
}

static int noise_output(GemNoise *n) {
  if (!n->enabled || n->length == 0 || (n->shift & 1)) return 0;
  return n->constant_volume ? n->volume : n->envelope;
}

static void clock_envelope(uint8_t *env, uint8_t *divider, bool *start, uint8_t volume) {
  if (*start) {
    *start = false;
    *env = 15;
    *divider = volume;
  } else if (*divider == 0) {
    if (*env > 0) (*env)--;
    *divider = volume;
  } else {
    (*divider)--;
  }
}

static void half_frame(GemSound *a) {
  for (int i = 0; i < 2; i++) {
    GemPulse *p = &a->pulse[i];
    if (!p->halt && p->length > 0) p->length--;
    if (p->sweep_enable && p->sweep_shift > 0 && p->period >= 8) {
      int delta = p->period >> p->sweep_shift;
      int target = p->sweep_negate ? (int)p->period - delta : (int)p->period + delta;
      if (i == 1 && p->sweep_negate) target -= 1;
      if (target >= 0 && target <= 0x7FF) p->period = (uint16_t)target;
    }
  }
  if (!a->triangle.control && a->triangle.length > 0) a->triangle.length--;
  if (!a->noise.halt && a->noise.length > 0) a->noise.length--;
}

static void quarter_frame(GemSound *a) {
  for (int i = 0; i < 2; i++) {
    GemPulse *p = &a->pulse[i];
    clock_envelope(&p->envelope, &p->env_divider, &p->env_start, p->volume);
  }
  clock_envelope(&a->noise.envelope, &a->noise.env_divider, &a->noise.env_start, a->noise.volume);
  if (a->triangle.linear_reload_flag) {
    a->triangle.linear = a->triangle.linear_reload;
  } else if (a->triangle.linear > 0) {
    a->triangle.linear--;
  }
  if (!a->triangle.control) a->triangle.linear_reload_flag = false;
}

static void dmc_restart(GemSound *a) {
  GemDmc *d = &a->dmc;
  d->current_addr = d->sample_addr;
  d->bytes_remaining = d->sample_len;
  d->buffer_full = false;
  d->bits = 0;
}

static void dmc_fetch(GemSound *a) {
  GemDmc *d = &a->dmc;
  if (!d->enabled || d->buffer_full || d->bytes_remaining == 0) return;
  d->sample_buffer = a->dmc_read ? a->dmc_read(a->dmc_bus, d->current_addr) : 0;
  d->current_addr++;
  d->bytes_remaining--;
  d->buffer_full = true;
  if (d->bytes_remaining == 0) {
    if (d->loop) dmc_restart(a);
    else if (d->irq_enable) d->irq = true;
  }
}

static void dmc_clock(GemSound *a) {
  GemDmc *d = &a->dmc;
  if (!d->enabled) return;
  if (d->timer > 0) {
    d->timer--;
    return;
  }
  d->timer = d->period;
  dmc_fetch(a);
  if (d->bits == 0) {
    if (!d->buffer_full) return;
    d->shift = d->sample_buffer;
    d->buffer_full = false;
    d->bits = 8;
  }
  if (d->shift & 1) {
    if (d->output <= 125) d->output += 2;
  } else {
    if (d->output >= 2) d->output -= 2;
  }
  d->shift >>= 1;
  d->bits--;
}

static void emit_sample(GemSound *a) {
  int p1 = pulse_output(&a->pulse[0]);
  int p2 = pulse_output(&a->pulse[1]);
  int tri = triangle_output(&a->triangle);
  int noi = noise_output(&a->noise);
  int mix = (p1 + p2) * 3 + tri * 2 + noi + (int)a->dmc.output - 64;
  float input = (float)(mix * 180);
  a->high_pass = input - a->previous_mix + 0.995f * a->high_pass;
  a->previous_mix = input;
  int sample = (int)a->high_pass;
  if (sample > 32767) sample = 32767;
  if (sample < -32768) sample = -32768;
  if (a->sample_count < (int)(sizeof(a->samples) / sizeof(a->samples[0]))) {
    a->samples[a->sample_count++] = (int16_t)sample;
  }
}

void gem_sound_reset(GemSound *a) {
  memset(a, 0, sizeof(*a));
  for (int i = 0; i < 2; i++) {
    a->pulse[i].period = 0;
    a->pulse[i].timer = 0;
    a->pulse[i].duty_pos = 0;
  }
  a->noise.shift = 1;
  a->triangle.period = 0;
  a->dmc.period = dmc_rate_table[0];
  a->dmc.output = 64;
}

void gem_sound_write(GemSound *a, uint16_t addr, uint8_t value) {
  if (addr >= 0x4000 && addr <= 0x4003) {
    pulse_write(&a->pulse[0], (uint8_t)(addr - 0x4000), value);
  } else if (addr >= 0x4004 && addr <= 0x4007) {
    pulse_write(&a->pulse[1], (uint8_t)(addr - 0x4004), value);
  } else if (addr >= 0x4008 && addr <= 0x400B) {
    triangle_write(&a->triangle, (uint8_t)(addr - 0x4008), value);
  } else if (addr >= 0x400C && addr <= 0x400F) {
    noise_write(&a->noise, (uint8_t)(addr - 0x400C), value);
  } else if (addr >= 0x4010 && addr <= 0x4013) {
    GemDmc *d = &a->dmc;
    switch (addr & 3) {
      case 0:
        d->irq_enable = (value & 0x80) != 0;
        d->loop = (value & 0x40) != 0;
        d->rate_index = value & 0x0F;
        d->period = dmc_rate_table[d->rate_index];
        if (!d->irq_enable) d->irq = false;
        break;
      case 1:
        d->output = value & 0x7F;
        break;
      case 2:
        d->sample_addr = (uint16_t)(0xC000 | ((uint16_t)value << 6));
        break;
      case 3:
        d->sample_len = (uint16_t)(((uint16_t)value << 4) + 1);
        break;
    }
  } else if (addr == 0x4015) {
    a->pulse[0].enabled = (value & 0x01) != 0;
    a->pulse[1].enabled = (value & 0x02) != 0;
    a->triangle.enabled = (value & 0x04) != 0;
    a->noise.enabled = (value & 0x08) != 0;
    if (!a->pulse[0].enabled) a->pulse[0].length = 0;
    if (!a->pulse[1].enabled) a->pulse[1].length = 0;
    if (!a->triangle.enabled) a->triangle.length = 0;
    if (!a->noise.enabled) a->noise.length = 0;
    a->dmc.enabled = (value & 0x10) != 0;
    if (a->dmc.enabled) {
      if (a->dmc.bytes_remaining == 0) dmc_restart(a);
    } else {
      a->dmc.bytes_remaining = 0;
      a->dmc.buffer_full = false;
    }
    a->dmc.irq = false;
  } else if (addr == 0x4017) {
    a->frame_mode = (value & 0x80) ? 1 : 0;
    a->frame_irq_inhibit = (value & 0x40) != 0;
    a->frame_cycle = 0;
    if (a->frame_irq_inhibit) a->frame_irq = false;
  }
}

void gem_sound_step(GemSound *a, int cpu_cycles, bool irq_enabled) {
  (void)irq_enabled;
  for (int i = 0; i < cpu_cycles; i++) {
    /* channel timers */
    for (int c = 0; c < 2; c++) {
      GemPulse *p = &a->pulse[c];
      if (((a->cycles + (uint32_t)i) & 1) == 0 && p->period >= 8) pulse_clock(p);
    }
    if (a->triangle.period >= 2) {
      if (a->triangle.timer == 0) {
        a->triangle.timer = a->triangle.period;
        a->triangle.pos = (a->triangle.pos + 1) & 31;
      } else {
        a->triangle.timer--;
      }
    }
    if (a->noise.period > 0) {
      if (a->noise.timer == 0) {
        a->noise.timer = a->noise.period;
        uint16_t feedback = (uint16_t)((a->noise.shift & 1) ^ ((a->noise.shift >> 1) & 1));
        a->noise.shift >>= 1;
        a->noise.shift |= (uint16_t)(feedback << 14);
        if (a->noise.mode) {
          a->noise.shift &= (uint16_t)~0x40;
          a->noise.shift |= (uint16_t)(feedback << 6);
        }
      } else {
        a->noise.timer--;
      }
    }

    dmc_clock(a);
    a->frame_cycle++;
    if (a->frame_cycle == 7457 || a->frame_cycle == 22371) {
      quarter_frame(a);
    }
    if (a->frame_cycle == 14913 || a->frame_cycle == 29829) {
      half_frame(a);
      if (a->frame_mode == 0 && a->frame_cycle == 29829 && !a->frame_irq_inhibit) {
        a->frame_irq = true;
      }
      if (a->frame_cycle == 29829) {
        a->frame_cycle = 0;
        if (a->frame_mode == 1) {
          a->frame_cycle = 0;
        }
      }
    }

    a->sample_accum += GEM_AUDIO_HZ;
    if (a->sample_accum >= GEM_CPU_HZ) {
      a->sample_accum -= GEM_CPU_HZ;
      emit_sample(a);
    }
  }
  a->cycles += (uint32_t)cpu_cycles;
}

int gem_sound_take_samples(GemSound *a, int16_t *out, int max) {
  if (max <= 0) return 0;
  int n = a->sample_count < max ? a->sample_count : max;
  if (n > 0) memcpy(out, a->samples, (size_t)n * sizeof(int16_t));
  if (n > 0) {
    a->sample_count -= n;
    memmove(a->samples, a->samples + n, (size_t)a->sample_count * sizeof(int16_t));
  }
  return n;
}
