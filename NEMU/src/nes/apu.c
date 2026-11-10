#include "nes.h"
#include <string.h>

static const uint8_t length_table[32] = {
  10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
  12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
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

static void pulse_write(NES_Pulse *p, uint8_t reg, uint8_t value) {
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

static void triangle_write(NES_Triangle *t, uint8_t reg, uint8_t value) {
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

static void noise_write(NES_Noise *n, uint8_t reg, uint8_t value) {
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

static void pulse_clock(NES_Pulse *p) {
  if (p->timer == 0) {
    p->timer = p->period;
    p->duty_pos = (p->duty_pos + 1) & 7;
  } else {
    p->timer--;
  }
}

static int pulse_output(NES_Pulse *p) {
  if (!p->enabled || p->length == 0 || p->period < 8) return 0;
  if (!duty_table[p->duty & 3][p->duty_pos & 7]) return 0;
  return p->constant_volume ? p->volume : p->envelope;
}

static int triangle_output(NES_Triangle *t) {
  static const uint8_t seq[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
  };
  if (!t->enabled || t->length == 0 || t->linear == 0 || t->period < 2) return 0;
  return seq[t->pos & 31];
}

static int noise_output(NES_Noise *n) {
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

static void half_frame(NES_APU *a) {
  for (int i = 0; i < 2; i++) {
    NES_Pulse *p = &a->pulse[i];
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

static void quarter_frame(NES_APU *a) {
  for (int i = 0; i < 2; i++) {
    NES_Pulse *p = &a->pulse[i];
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

static void emit_sample(NES_APU *a) {
  int p1 = pulse_output(&a->pulse[0]);
  int p2 = pulse_output(&a->pulse[1]);
  int tri = triangle_output(&a->triangle);
  int noi = noise_output(&a->noise);
  int mix = (p1 + p2) * 3 + tri * 2 + noi;
  int sample = mix == 0 ? 0 : mix * 180;
  if (sample > 32767) sample = 32767;
  if (sample < -32768) sample = -32768;
  if (a->sample_count < (int)(sizeof(a->samples) / sizeof(a->samples[0]))) {
    a->samples[a->sample_count++] = (int16_t)sample;
  }
}

void apu_reset(NES_APU *a) {
  memset(a, 0, sizeof(*a));
  for (int i = 0; i < 2; i++) {
    a->pulse[i].period = 0;
    a->pulse[i].timer = 0;
    a->pulse[i].duty_pos = 0;
  }
  a->noise.shift = 1;
  a->triangle.period = 0;
}

void apu_write(NES_APU *a, uint16_t addr, uint8_t value) {
  if (addr >= 0x4000 && addr <= 0x4003) {
    pulse_write(&a->pulse[0], (uint8_t)(addr - 0x4000), value);
  } else if (addr >= 0x4004 && addr <= 0x4007) {
    pulse_write(&a->pulse[1], (uint8_t)(addr - 0x4004), value);
  } else if (addr >= 0x4008 && addr <= 0x400B) {
    triangle_write(&a->triangle, (uint8_t)(addr - 0x4008), value);
  } else if (addr >= 0x400C && addr <= 0x400F) {
    noise_write(&a->noise, (uint8_t)(addr - 0x400C), value);
  } else if (addr == 0x4015) {
    a->pulse[0].enabled = (value & 0x01) != 0;
    a->pulse[1].enabled = (value & 0x02) != 0;
    a->triangle.enabled = (value & 0x04) != 0;
    a->noise.enabled = (value & 0x08) != 0;
    if (!a->pulse[0].enabled) a->pulse[0].length = 0;
    if (!a->pulse[1].enabled) a->pulse[1].length = 0;
    if (!a->triangle.enabled) a->triangle.length = 0;
    if (!a->noise.enabled) a->noise.length = 0;
  } else if (addr == 0x4017) {
    a->frame_mode = (value & 0x80) ? 1 : 0;
    a->frame_irq_inhibit = (value & 0x40) != 0;
    a->frame_cycle = 0;
    if (a->frame_irq_inhibit) a->frame_irq = false;
  }
}

uint8_t apu_read(NES_APU *a, uint16_t addr) {
  if (addr != 0x4015) return 0;
  uint8_t v = 0;
  if (a->pulse[0].length > 0) v |= 0x01;
  if (a->pulse[1].length > 0) v |= 0x02;
  if (a->triangle.length > 0) v |= 0x04;
  if (a->noise.length > 0) v |= 0x08;
  if (a->frame_irq) v |= 0x40;
  a->frame_irq = false;
  return v;
}

void apu_step(NES_APU *a, int cpu_cycles, bool irq_enabled) {
  (void)irq_enabled;
  for (int i = 0; i < cpu_cycles; i++) {
    /* channel timers */
    for (int c = 0; c < 2; c++) {
      NES_Pulse *p = &a->pulse[c];
      if (p->period >= 8) pulse_clock(p);
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

    a->sample_accum += NES_AUDIO_HZ;
    if (a->sample_accum >= NES_CPU_HZ) {
      a->sample_accum -= NES_CPU_HZ;
      emit_sample(a);
    }
  }
  a->cycles += (uint32_t)cpu_cycles;
}

int apu_take_samples(NES_APU *a, int16_t *out, int max) {
  int n = a->sample_count < max ? a->sample_count : max;
  if (n > 0) memcpy(out, a->samples, (size_t)n * sizeof(int16_t));
  a->sample_count = 0;
  return n;
}
