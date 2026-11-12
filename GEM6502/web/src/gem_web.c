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
