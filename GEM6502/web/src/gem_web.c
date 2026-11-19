/*
 * GEM6502 WebAssembly bridge.
 *
 * This is the whole C side of the browser player: a tiny, stable ABI over
 * the functional core.  The JavaScript frontend owns rendering, input,
 * audio scheduling and pacing; nothing here knows about a DOM, SDL or a
 * filesystem.
 */
#include "gem6502.h"
#include "ines.h"
#include "render.h"

#include <string.h>

#define GEM_WEB_ROM_MAX (1024u * 1024u)
#define GEM_WEB_AUDIO_MAX 8192
#define GEM_WEB_FRAME_GUARD 1000000

static Gem *gem;
static uint8_t rom_stage[GEM_WEB_ROM_MAX];
static uint8_t framebuffer[GEM_RENDER_WIDTH * GEM_RENDER_HEIGHT * 3];
static uint8_t framebuffer_rgba[GEM_RENDER_WIDTH * GEM_RENDER_HEIGHT * 4];
static int16_t audio_buffer[GEM_WEB_AUDIO_MAX];
static char last_error[128];

static void set_error(const char *message) {
  size_t n;

  if (message == NULL) message = "";
  n = strlen(message);
  if (n >= sizeof(last_error)) n = sizeof(last_error) - 1;
  memcpy(last_error, message, n);
  last_error[n] = '\0';
}

int gem_web_init(void) {
  if (gem == NULL) {
    gem = gem_create();
    if (gem == NULL) {
      set_error("out of memory creating GEM6502");
      return 0;
    }
  }
  return 1;
}

void gem_web_destroy(void) {
  if (gem != NULL) {
    gem_destroy(gem);
    gem = NULL;
  }
}

void gem_web_reset(void) {
  if (gem != NULL) gem_reset(gem);
}

uint8_t *gem_web_rom_buffer(void) { return rom_stage; }

int gem_web_rom_capacity(void) { return (int)GEM_WEB_ROM_MAX; }

int gem_web_load_rom(const uint8_t *data, unsigned size) {
  GemInes ines;
  int rc;

  last_error[0] = '\0';
  if (!gem_web_init()) return -1;
  if (data == NULL || size == 0) {
    set_error("empty ROM image");
    return -1;
  }
  if (size > GEM_WEB_ROM_MAX) {
    set_error("ROM image is larger than 1 MiB");
    return -1;
  }

  /* Parse once up front so the host gets the loader's diagnostic text. */
  memset(&ines, 0, sizeof(ines));
  if (gem_ines_parse(data, size, &ines) != 0) {
    set_error(ines.error);
    gem_ines_free(&ines);
    return -1;
  }
  gem_ines_free(&ines);

  rc = gem_load_game_memory(gem, data, size);
  if (rc != 0) {
    if (last_error[0] == '\0') set_error("cartridge rejected");
    return -1;
  }
  return 0;
}

void gem_web_set_buttons(unsigned joy1, unsigned joy2) {
  if (gem == NULL) return;
  gem->apu.joy1 = (uint8_t)joy1;
  gem->apu.joy2 = (uint8_t)joy2;
}

/* Run until the PPU completes a frame.  Returns 1 when a frame was produced. */
int gem_web_run_frame(void) {
  uint64_t frame;
  int guard;

  if (gem == NULL) return 0;
  frame = gem->stats.ppu_frames;
  for (guard = 0; guard < GEM_WEB_FRAME_GUARD; guard++) {
    if (gem->cpu.halted || gem->stats.ppu_frames != frame) break;
    gem_step(gem);
  }
  return gem->stats.ppu_frames != frame;
}

/* Render the current frame into the RGB24 buffer and return its address. */
const uint8_t *gem_web_framebuffer(void) {
  if (gem == NULL) return framebuffer;
  gem_render_frame(gem, framebuffer, GEM_RENDER_WIDTH, GEM_RENDER_HEIGHT);
  return framebuffer;
}

/*
 * Same frame, packed as RGBA for Canvas ImageData.  Browsers want four bytes
 * per pixel, so the conversion happens once in C instead of in a JS loop.
 */
const uint8_t *gem_web_framebuffer_rgba(void) {
  const uint8_t *src;
  uint8_t *dst;
  int i;

  src = gem_web_framebuffer();
  dst = framebuffer_rgba;
  for (i = 0; i < GEM_RENDER_WIDTH * GEM_RENDER_HEIGHT; i++) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = 0xFF;
    src += 3;
    dst += 4;
  }
  return framebuffer_rgba;
}

int gem_web_take_audio(int max_samples) {
  if (gem == NULL || max_samples <= 0) return 0;
  if (max_samples > GEM_WEB_AUDIO_MAX) max_samples = GEM_WEB_AUDIO_MAX;
  return gem_sound_take_samples(&gem->apu.sound, audio_buffer, max_samples);
}

const int16_t *gem_web_audio_buffer(void) { return audio_buffer; }

const char *gem_web_error(void) { return last_error; }

unsigned long long gem_web_frames(void) {
  return gem == NULL ? 0 : (unsigned long long)gem->stats.ppu_frames;
}

unsigned long long gem_web_cycles(void) {
  return gem == NULL ? 0 : (unsigned long long)gem->cpu.cycles;
}

int gem_web_halted(void) { return gem == NULL ? 1 : gem->cpu.halted ? 1 : 0; }

/* --------------------------------------------------------------- states */
/*
 * Save states for the browser player.
 *
 * The record covers everything that changes while a game runs: CPU, PPU
 * (including VRAM/OAM/palette/CHR), APU + sound channels, the active PRG
 * window, mapper registers and the tick counters.  Pointer fields
 * (event-queue heap, DMC callback, trace stream) are deliberately left out:
 * they belong to the live instance and stay valid across a load.  The
 * cartridge image (cart_prg/cart_chr) is also omitted; it is rebuilt by
 * gem_web_load_rom() before a state is ever restored.
 *
 * Layout: magic, version and payload length, then little-endian fields in a
 * fixed order.  The JS side treats the bytes as opaque and only persists them.
 */
#define GEM_WEB_STATE_MAGIC 0x47454D31u /* "GEM1" */
#define GEM_WEB_STATE_VERSION 1u
#define GEM_WEB_STATE_MAX (96u * 1024u)

static uint8_t state_buffer[GEM_WEB_STATE_MAX];
static unsigned state_len;
static unsigned state_pos;

static void st_put(const void *src, unsigned n) {
  if (state_pos + n > GEM_WEB_STATE_MAX) {
    state_pos = GEM_WEB_STATE_MAX + 1u;
    return;
  }
  memcpy(state_buffer + state_pos, src, n);
  state_pos += n;
}

static void put_u8(uint8_t v) { st_put(&v, 1u); }
static void put_u16(uint16_t v) {
  uint8_t b[2];
  b[0] = (uint8_t)(v & 0xFFu);
  b[1] = (uint8_t)((v >> 8) & 0xFFu);
  st_put(b, 2u);
}
static void put_u32(uint32_t v) {
  uint8_t b[4];
  b[0] = (uint8_t)(v & 0xFFu);
  b[1] = (uint8_t)((v >> 8) & 0xFFu);
  b[2] = (uint8_t)((v >> 16) & 0xFFu);
  b[3] = (uint8_t)((v >> 24) & 0xFFu);
  st_put(b, 4u);
}
static void put_u64(uint64_t v) {
  uint8_t b[8];
  int i;
  for (i = 0; i < 8; i++) b[i] = (uint8_t)((v >> (8 * i)) & 0xFFu);
  st_put(b, 8u);
}
static void put_i32(int32_t v) { put_u32((uint32_t)v); }
static void put_bool(bool v) { put_u8(v ? 1u : 0u); }
static void put_f32(float v) {
  uint32_t bits;
  memcpy(&bits, &v, sizeof(bits));
  put_u32(bits);
}
static void put_arr(const void *p, unsigned n) { st_put(p, n); }

static int st_get(void *dst, unsigned n) {
  if (state_pos + n > state_len) return -1;
  memcpy(dst, state_buffer + state_pos, n);
  state_pos += n;
  return 0;
}
static int get_u8(uint8_t *v) { return st_get(v, 1u); }
static int get_u16(uint16_t *v) {
  uint8_t b[2];
  if (st_get(b, 2u) != 0) return -1;
  *v = (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
  return 0;
}
static int get_u32(uint32_t *v) {
  uint8_t b[4];
  if (st_get(b, 4u) != 0) return -1;
  *v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
  return 0;
}
static int get_u64(uint64_t *v) {
  uint8_t b[8];
  int i;
  if (st_get(b, 8u) != 0) return -1;
  *v = 0;
  for (i = 7; i >= 0; i--) *v = (*v << 8) | b[i];
  return 0;
}
static int get_i32(int32_t *v) {
  uint32_t u;
  if (get_u32(&u) != 0) return -1;
  *v = (int32_t)u;
  return 0;
}
static int get_bool(bool *v) {
  uint8_t b;
  if (get_u8(&b) != 0) return -1;
  *v = b != 0;
  return 0;
}
static int get_f32(float *v) {
  uint32_t bits;
  if (get_u32(&bits) != 0) return -1;
  memcpy(v, &bits, sizeof(bits));
  return 0;
}
static int get_arr(void *dst, unsigned n) { return st_get(dst, n); }

uint8_t *gem_web_state_buffer(void) { return state_buffer; }

int gem_web_state_size(void) { return (int)GEM_WEB_STATE_MAX; }

int gem_web_save_state(void) {
  Gem *g = gem;
  int i;
  uint32_t payload;

  if (g == NULL) {
    set_error("no machine to save");
    return 0;
  }
  state_pos = 0;
  put_u32(GEM_WEB_STATE_MAGIC);
  put_u32(GEM_WEB_STATE_VERSION);
  put_u32(0u); /* patched below once the payload length is known */

  /* CPU */
  put_u8(g->cpu.a);
  put_u8(g->cpu.x);
  put_u8(g->cpu.y);
  put_u8(g->cpu.sp);
  put_u8(g->cpu.p);
  put_u16(g->cpu.pc);
  put_u64(g->cpu.cycles);
  put_u64(g->cpu.instructions);
  put_i32(g->cpu.dma_stall);
  put_bool(g->cpu.halted);
  put_bool(g->cpu.irq_pending);

  /* PPU: registers and memories; the pixel framebuffer is rebuilt by the
     next rendered frame, so it is not worth 240 KB in the record. */
  put_u8(g->ppu.ctrl);
  put_u8(g->ppu.mask);
  put_u8(g->ppu.status);
  put_u8(g->ppu.oam_addr);
  put_u8(g->ppu.read_buf);
  put_u16(g->ppu.vaddr);
  put_u16(g->ppu.taddr);
  put_u8(g->ppu.fine_x);
  put_bool(g->ppu.nmi_line);
  put_bool(g->ppu.framebuffer_valid);
  put_u8(g->ppu.scroll_x);
  put_u8(g->ppu.scroll_y);
  put_bool(g->ppu.w_toggle);
  put_i32(g->ppu.scanline);
  put_i32(g->ppu.cycle);
  put_bool(g->ppu.odd_frame);
  put_bool(g->ppu.frame_ready);
  put_bool(g->ppu.nmi_pending);
  put_arr(g->ppu.vram, (unsigned)sizeof(g->ppu.vram));
  put_arr(g->ppu.oam, (unsigned)sizeof(g->ppu.oam));
  put_arr(g->ppu.pal, (unsigned)sizeof(g->ppu.pal));
  put_arr(g->ppu.chr, (unsigned)sizeof(g->ppu.chr));

  /* APU + sound channels */
  put_arr(g->apu.regs, (unsigned)sizeof(g->apu.regs));
  put_u64(g->apu.cycles);
  put_u8(g->apu.joy1);
  put_u8(g->apu.joy2);
  put_bool(g->apu.joy_strobe);
  put_u8(g->apu.joy1_shift);
  put_u8(g->apu.joy2_shift);
  put_bool(g->apu.irq);
  put_u64(g->apu.frame_div);
  put_u8(g->apu.frame_step);
  put_bool(g->apu.frame_mode);
  put_bool(g->apu.frame_inhibit);

  for (i = 0; i < 2; i++) {
    const GemPulse *p = &g->apu.sound.pulse[i];
    put_bool(p->enabled);
    put_u8(p->duty);
    put_u8(p->duty_pos);
    put_u16(p->timer);
    put_u16(p->period);
    put_u8(p->length);
    put_bool(p->halt);
    put_bool(p->constant_volume);
    put_u8(p->volume);
    put_u8(p->envelope);
    put_u8(p->env_divider);
    put_bool(p->env_start);
    put_bool(p->sweep_enable);
    put_u8(p->sweep_period);
    put_bool(p->sweep_negate);
    put_u8(p->sweep_shift);
    put_u16(p->sweep_divider);
  }
  {
    const GemTriangle *t = &g->apu.sound.triangle;
    put_bool(t->enabled);
    put_u16(t->timer);
    put_u16(t->period);
    put_u8(t->length);
    put_bool(t->control);
    put_u8(t->linear);
    put_u8(t->linear_reload);
    put_bool(t->linear_reload_flag);
    put_u8(t->pos);
  }
  {
    const GemNoise *n = &g->apu.sound.noise;
    put_bool(n->enabled);
    put_u16(n->timer);
    put_u16(n->period);
    put_u8(n->length);
    put_bool(n->halt);
    put_bool(n->constant_volume);
    put_u8(n->volume);
    put_u8(n->envelope);
    put_u8(n->env_divider);
    put_bool(n->env_start);
    put_u16(n->shift);
    put_bool(n->mode);
  }
  {
    const GemDmc *d = &g->apu.sound.dmc;
    put_bool(d->enabled);
    put_bool(d->irq_enable);
    put_bool(d->loop);
    put_bool(d->irq);
    put_u8(d->rate_index);
    put_u16(d->timer);
    put_u16(d->period);
    put_u16(d->sample_addr);
    put_u16(d->sample_len);
    put_u16(d->current_addr);
    put_u16(d->bytes_remaining);
    put_u8(d->sample_buffer);
    put_bool(d->buffer_full);
    put_u8(d->shift);
    put_u8(d->bits);
    put_u8(d->output);
  }
  put_arr(g->apu.sound.regs, (unsigned)sizeof(g->apu.sound.regs));
  put_u8(g->apu.sound.status);
  put_u8(g->apu.sound.frame_mode);
  put_bool(g->apu.sound.frame_irq_inhibit);
  put_bool(g->apu.sound.frame_irq);
  put_u32(g->apu.sound.frame_cycle);
  put_u32(g->apu.sound.sample_accum);
  put_arr(g->apu.sound.samples, (unsigned)sizeof(g->apu.sound.samples));
  put_i32(g->apu.sound.sample_count);
  put_u32(g->apu.sound.cycles);
  put_f32(g->apu.sound.previous_mix);
  put_f32(g->apu.sound.high_pass);

  /* Bus: RAM, the active PRG window and the CHR window.  The full cartridge
     image stays out of the record (it is reloaded from the ROM first). */
  put_arr(g->bus.ram, (unsigned)sizeof(g->bus.ram));
  put_arr(g->bus.prg, (unsigned)sizeof(g->bus.prg));
  put_arr(g->bus.prg_ram, (unsigned)sizeof(g->bus.prg_ram));
  put_arr(g->bus.chr_rom, (unsigned)sizeof(g->bus.chr_rom));
  put_u8(g->bus.mapper);
  put_bool(g->bus.vertical_mirroring);
  put_u8(g->bus.m1_shift);
  put_u8(g->bus.m1_count);
  put_u8(g->bus.m1_ctrl);
  put_u8(g->bus.m1_chr0);
  put_u8(g->bus.m1_chr1);
  put_u8(g->bus.m1_prg);
  put_u8(g->bus.m2_bank);
  put_u8(g->bus.m3_bank);
  put_u8(g->bus.m7_bank);
  put_u8(g->bus.m4_select);
  put_arr(g->bus.m4_regs, (unsigned)sizeof(g->bus.m4_regs));
  put_u8(g->bus.m4_prg_mode);
  put_u8(g->bus.m4_chr_mode);
  put_u8(g->bus.m4_irq_latch);
  put_u8(g->bus.m4_irq_counter);
  put_u8(g->bus.m4_irq_reload);
  put_bool(g->bus.m4_irq_enable);
  put_bool(g->bus.m4_irq_pending);
  put_bool(g->bus.cart_irq);
  put_bool(g->bus.host_halt);
  put_u8(g->bus.host_exit);
  put_u64(g->bus.reads);
  put_u64(g->bus.writes);

  /* Counters */
  put_u64(g->stats.cpu_cycles);
  put_u64(g->stats.instructions);
  put_u64(g->stats.bus_reads);
  put_u64(g->stats.bus_writes);
  put_u64(g->stats.ppu_frames);
  put_u64(g->stats.nmi_count);
  put_u64(g->stats.illegal_opcodes);
  put_u64(g->now);
  put_u64(g->max_cycles);

  if (state_pos > GEM_WEB_STATE_MAX) {
    set_error("state buffer overflow");
    return 0;
  }
  state_len = state_pos;
  payload = (uint32_t)(state_len - 12u);
  state_buffer[8] = (uint8_t)(payload & 0xFFu);
  state_buffer[9] = (uint8_t)((payload >> 8) & 0xFFu);
  state_buffer[10] = (uint8_t)((payload >> 16) & 0xFFu);
  state_buffer[11] = (uint8_t)((payload >> 24) & 0xFFu);
  return (int)state_len;
}

int gem_web_load_state(unsigned size) {
  Gem *g = gem;
  uint32_t magic = 0, version = 0, payload = 0;
  int i;

  if (g == NULL) {
    set_error("no machine to load");
    return -1;
  }
  if (size < 12u || size > GEM_WEB_STATE_MAX) {
    set_error("state is truncated");
    return -1;
  }
  state_len = size;
  state_pos = 0;
  if (get_u32(&magic) != 0 || get_u32(&version) != 0 || get_u32(&payload) != 0 ||
      magic != GEM_WEB_STATE_MAGIC || version != GEM_WEB_STATE_VERSION ||
      payload != size - 12u) {
    set_error("state header mismatch");
    return -1;
  }

  /* CPU */
  if (get_u8(&g->cpu.a) != 0 || get_u8(&g->cpu.x) != 0 || get_u8(&g->cpu.y) != 0 ||
      get_u8(&g->cpu.sp) != 0 || get_u8(&g->cpu.p) != 0 || get_u16(&g->cpu.pc) != 0 ||
      get_u64(&g->cpu.cycles) != 0 || get_u64(&g->cpu.instructions) != 0 ||
      get_i32(&g->cpu.dma_stall) != 0 || get_bool(&g->cpu.halted) != 0 ||
      get_bool(&g->cpu.irq_pending) != 0)
    return -1;

  /* PPU */
  if (get_u8(&g->ppu.ctrl) != 0 || get_u8(&g->ppu.mask) != 0 ||
      get_u8(&g->ppu.status) != 0 || get_u8(&g->ppu.oam_addr) != 0 ||
      get_u8(&g->ppu.read_buf) != 0 || get_u16(&g->ppu.vaddr) != 0 ||
      get_u16(&g->ppu.taddr) != 0 || get_u8(&g->ppu.fine_x) != 0 ||
      get_bool(&g->ppu.nmi_line) != 0 || get_bool(&g->ppu.framebuffer_valid) != 0 ||
      get_u8(&g->ppu.scroll_x) != 0 || get_u8(&g->ppu.scroll_y) != 0 ||
      get_bool(&g->ppu.w_toggle) != 0 || get_i32(&g->ppu.scanline) != 0 ||
      get_i32(&g->ppu.cycle) != 0 || get_bool(&g->ppu.odd_frame) != 0 ||
      get_bool(&g->ppu.frame_ready) != 0 || get_bool(&g->ppu.nmi_pending) != 0 ||
      get_arr(g->ppu.vram, (unsigned)sizeof(g->ppu.vram)) != 0 ||
      get_arr(g->ppu.oam, (unsigned)sizeof(g->ppu.oam)) != 0 ||
      get_arr(g->ppu.pal, (unsigned)sizeof(g->ppu.pal)) != 0 ||
      get_arr(g->ppu.chr, (unsigned)sizeof(g->ppu.chr)) != 0)
    return -1;

  /* APU + sound channels */
  if (get_arr(g->apu.regs, (unsigned)sizeof(g->apu.regs)) != 0 ||
      get_u64(&g->apu.cycles) != 0 || get_u8(&g->apu.joy1) != 0 ||
      get_u8(&g->apu.joy2) != 0 || get_bool(&g->apu.joy_strobe) != 0 ||
      get_u8(&g->apu.joy1_shift) != 0 || get_u8(&g->apu.joy2_shift) != 0 ||
      get_bool(&g->apu.irq) != 0 || get_u64(&g->apu.frame_div) != 0 ||
      get_u8(&g->apu.frame_step) != 0 || get_bool(&g->apu.frame_mode) != 0 ||
      get_bool(&g->apu.frame_inhibit) != 0)
    return -1;

  for (i = 0; i < 2; i++) {
    GemPulse *p = &g->apu.sound.pulse[i];
    if (get_bool(&p->enabled) != 0 || get_u8(&p->duty) != 0 ||
        get_u8(&p->duty_pos) != 0 || get_u16(&p->timer) != 0 ||
        get_u16(&p->period) != 0 || get_u8(&p->length) != 0 ||
        get_bool(&p->halt) != 0 || get_bool(&p->constant_volume) != 0 ||
        get_u8(&p->volume) != 0 || get_u8(&p->envelope) != 0 ||
        get_u8(&p->env_divider) != 0 || get_bool(&p->env_start) != 0 ||
        get_bool(&p->sweep_enable) != 0 || get_u8(&p->sweep_period) != 0 ||
        get_bool(&p->sweep_negate) != 0 || get_u8(&p->sweep_shift) != 0 ||
        get_u16(&p->sweep_divider) != 0)
      return -1;
  }
  {
    GemTriangle *t = &g->apu.sound.triangle;
    if (get_bool(&t->enabled) != 0 || get_u16(&t->timer) != 0 ||
        get_u16(&t->period) != 0 || get_u8(&t->length) != 0 ||
        get_bool(&t->control) != 0 || get_u8(&t->linear) != 0 ||
        get_u8(&t->linear_reload) != 0 || get_bool(&t->linear_reload_flag) != 0 ||
        get_u8(&t->pos) != 0)
      return -1;
  }
  {
    GemNoise *n = &g->apu.sound.noise;
    if (get_bool(&n->enabled) != 0 || get_u16(&n->timer) != 0 ||
        get_u16(&n->period) != 0 || get_u8(&n->length) != 0 ||
        get_bool(&n->halt) != 0 || get_bool(&n->constant_volume) != 0 ||
        get_u8(&n->volume) != 0 || get_u8(&n->envelope) != 0 ||
        get_u8(&n->env_divider) != 0 || get_bool(&n->env_start) != 0 ||
        get_u16(&n->shift) != 0 || get_bool(&n->mode) != 0)
      return -1;
  }
  {
    GemDmc *d = &g->apu.sound.dmc;
    if (get_bool(&d->enabled) != 0 || get_bool(&d->irq_enable) != 0 ||
        get_bool(&d->loop) != 0 || get_bool(&d->irq) != 0 ||
        get_u8(&d->rate_index) != 0 || get_u16(&d->timer) != 0 ||
        get_u16(&d->period) != 0 || get_u16(&d->sample_addr) != 0 ||
        get_u16(&d->sample_len) != 0 || get_u16(&d->current_addr) != 0 ||
        get_u16(&d->bytes_remaining) != 0 || get_u8(&d->sample_buffer) != 0 ||
        get_bool(&d->buffer_full) != 0 || get_u8(&d->shift) != 0 ||
        get_u8(&d->bits) != 0 || get_u8(&d->output) != 0)
      return -1;
  }
  if (get_arr(g->apu.sound.regs, (unsigned)sizeof(g->apu.sound.regs)) != 0 ||
      get_u8(&g->apu.sound.status) != 0 || get_u8(&g->apu.sound.frame_mode) != 0 ||
      get_bool(&g->apu.sound.frame_irq_inhibit) != 0 ||
      get_bool(&g->apu.sound.frame_irq) != 0 ||
      get_u32(&g->apu.sound.frame_cycle) != 0 ||
      get_u32(&g->apu.sound.sample_accum) != 0 ||
      get_arr(g->apu.sound.samples, (unsigned)sizeof(g->apu.sound.samples)) != 0 ||
      get_i32(&g->apu.sound.sample_count) != 0 ||
      get_u32(&g->apu.sound.cycles) != 0 ||
      get_f32(&g->apu.sound.previous_mix) != 0 ||
      get_f32(&g->apu.sound.high_pass) != 0)
    return -1;

  /* Bus */
  if (get_arr(g->bus.ram, (unsigned)sizeof(g->bus.ram)) != 0 ||
      get_arr(g->bus.prg, (unsigned)sizeof(g->bus.prg)) != 0 ||
      get_arr(g->bus.prg_ram, (unsigned)sizeof(g->bus.prg_ram)) != 0 ||
      get_arr(g->bus.chr_rom, (unsigned)sizeof(g->bus.chr_rom)) != 0 ||
      get_u8(&g->bus.mapper) != 0 || get_bool(&g->bus.vertical_mirroring) != 0 ||
      get_u8(&g->bus.m1_shift) != 0 || get_u8(&g->bus.m1_count) != 0 ||
      get_u8(&g->bus.m1_ctrl) != 0 || get_u8(&g->bus.m1_chr0) != 0 ||
      get_u8(&g->bus.m1_chr1) != 0 || get_u8(&g->bus.m1_prg) != 0 ||
      get_u8(&g->bus.m2_bank) != 0 || get_u8(&g->bus.m3_bank) != 0 ||
      get_u8(&g->bus.m7_bank) != 0 || get_u8(&g->bus.m4_select) != 0 ||
      get_arr(g->bus.m4_regs, (unsigned)sizeof(g->bus.m4_regs)) != 0 ||
      get_u8(&g->bus.m4_prg_mode) != 0 || get_u8(&g->bus.m4_chr_mode) != 0 ||
      get_u8(&g->bus.m4_irq_latch) != 0 || get_u8(&g->bus.m4_irq_counter) != 0 ||
      get_u8(&g->bus.m4_irq_reload) != 0 || get_bool(&g->bus.m4_irq_enable) != 0 ||
      get_bool(&g->bus.m4_irq_pending) != 0 || get_bool(&g->bus.cart_irq) != 0 ||
      get_bool(&g->bus.host_halt) != 0 || get_u8(&g->bus.host_exit) != 0 ||
      get_u64(&g->bus.reads) != 0 || get_u64(&g->bus.writes) != 0)
    return -1;

  /* Counters */
  if (get_u64(&g->stats.cpu_cycles) != 0 || get_u64(&g->stats.instructions) != 0 ||
      get_u64(&g->stats.bus_reads) != 0 || get_u64(&g->stats.bus_writes) != 0 ||
      get_u64(&g->stats.ppu_frames) != 0 || get_u64(&g->stats.nmi_count) != 0 ||
      get_u64(&g->stats.illegal_opcodes) != 0 || get_u64(&g->now) != 0 ||
      get_u64(&g->max_cycles) != 0)
    return -1;

  if (state_pos != state_len) {
    set_error("state length mismatch");
    return -1;
  }

  /* The pixel buffer belongs to the live instance, not the record: force the
     next rendered frame to refill it instead of showing stale pixels. */
  g->ppu.framebuffer_valid = false;
  g->ppu.frame_ready = false;
  set_error("");
  return 0;
}
