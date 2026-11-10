#include "nes.h"
#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <isa.h>
#include <utils.h>
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

bool nes_active = false;
NES_PPU ppu;
NES_APU apu;
NES_Cart cart;

static uint8_t cpu_ram[2048];
static uint8_t controller_state[2];
static uint8_t controller_shift[2];
static uint8_t controller_strobe;
static bool g_quit;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static SDL_AudioDeviceID audio_dev;
static bool audio_enabled;
static bool headless;
static bool fast_mode;
static int scale = 3;
static int max_frames;
static uint8_t forced_buttons;
static const char *screenshot_path;
static const char *wav_path;
static int16_t *wav_samples;
static size_t wav_len;
static size_t wav_cap;

static bool has_token(const char *list, const char *token) {
  size_t n = strlen(token);
  const char *p = list;
  while (*p) {
    while (*p == ',' || *p == ' ') p++;
    const char *start = p;
    while (*p && *p != ',' && *p != ' ') p++;
    if ((size_t)(p - start) == n && strncasecmp(start, token, n) == 0) return true;
  }
  return false;
}

static bool ends_with_nes(const char *s) {
  size_t n = strlen(s);
  return n >= 4 && strcasecmp(s + n - 4, ".nes") == 0;
}

bool nes_is_requested(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--nes") == 0) return true;
    if (argv[i][0] != '-' && ends_with_nes(argv[i])) return true;
  }
  return false;
}

static uint8_t read_controller(int port) {
  if (controller_strobe) return controller_state[port] & 1;
  uint8_t v = controller_shift[port] & 1;
  controller_shift[port] >>= 1;
  return v;
}

uint8_t nes_cpu_read(uint16_t addr) {
  if (addr < 0x2000) return cpu_ram[addr & 0x7FF];
  if (addr < 0x4000) return ppu_read_reg(&ppu, (uint16_t)(0x2000 | (addr & 7)));
  if (addr == 0x4015) return apu_read(&apu, addr);
  if (addr == 0x4016) return read_controller(0);
  if (addr == 0x4017) return read_controller(1);
  if (addr < 0x4020) return 0;
  if (addr < 0x6000) return 0;
  if (addr < 0x8000) return cart_cpu_read(&cart, addr);
  return cart_cpu_read(&cart, addr);
}

void nes_cpu_write(uint16_t addr, uint8_t value) {
  if (addr < 0x2000) {
    cpu_ram[addr & 0x7FF] = value;
    return;
  }
  if (addr < 0x4000) {
    ppu_write_reg(&ppu, (uint16_t)(0x2000 | (addr & 7)), value);
    return;
  }
  if (addr == 0x4014) {
    uint8_t page[256];
    for (int i = 0; i < 256; i++) page[i] = nes_cpu_read((uint16_t)((value << 8) | i));
    ppu_oam_dma(&ppu, page);
    cpu.cycles += 513;
    return;
  }
  if (addr == 0x4016) {
    controller_strobe = value & 1;
    if (controller_strobe) {
      controller_shift[0] = controller_state[0];
      controller_shift[1] = controller_state[1];
    }
    return;
  }
  if (addr >= 0x4000 && addr <= 0x4013) {
    apu_write(&apu, addr, value);
    return;
  }
  if (addr == 0x4015 || addr == 0x4017) {
    apu_write(&apu, addr, value);
    return;
  }
  if (addr < 0x4020) return;
  if (addr < 0x6000) return;
  if (addr < 0x8000) {
    cart_cpu_write(&cart, addr, value);
    return;
  }
  cart_cpu_write(&cart, addr, value);
}

static void update_input(void) {
  const Uint8 *keys = SDL_GetKeyboardState(NULL);

  /* Player 1: WASD for the D-pad. */
  uint8_t p1 = 0;
  if (keys[SDL_SCANCODE_W]) p1 |= 0x10;
  if (keys[SDL_SCANCODE_S]) p1 |= 0x20;
  if (keys[SDL_SCANCODE_A]) p1 |= 0x40;
  if (keys[SDL_SCANCODE_D]) p1 |= 0x80;

  /* Player 2: arrow keys for the D-pad. */
  uint8_t p2 = 0;
  if (keys[SDL_SCANCODE_UP]) p2 |= 0x10;
  if (keys[SDL_SCANCODE_DOWN]) p2 |= 0x20;
  if (keys[SDL_SCANCODE_LEFT]) p2 |= 0x40;
  if (keys[SDL_SCANCODE_RIGHT]) p2 |= 0x80;

  /* Player 1 buttons: Z/X = A/B, C/V = Select/Start.
   * Right Shift and Return remain aliases for Select/Start. */
  if (keys[SDL_SCANCODE_Z]) p1 |= 0x01;
  if (keys[SDL_SCANCODE_X]) p1 |= 0x02;
  if (keys[SDL_SCANCODE_C] || keys[SDL_SCANCODE_RSHIFT]) p1 |= 0x04;
  if (keys[SDL_SCANCODE_V] || keys[SDL_SCANCODE_RETURN]) p1 |= 0x08;

  /* Player 2 buttons: comma/period = A/B, slash/apostrophe = Select/Start. */
  if (keys[SDL_SCANCODE_COMMA]) p2 |= 0x01;
  if (keys[SDL_SCANCODE_PERIOD]) p2 |= 0x02;
  if (keys[SDL_SCANCODE_SLASH]) p2 |= 0x04;
  if (keys[SDL_SCANCODE_APOSTROPHE]) p2 |= 0x08;

  controller_state[0] = p1 | forced_buttons;
  controller_state[1] = p2;
}

static void poll_events(void) {
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    if (ev.type == SDL_QUIT) g_quit = true;
    if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE) g_quit = true;
  }
  update_input();
}

static void video_init(void) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL video init failed: %s\n", SDL_GetError());
    exit(1);
  }
  window = SDL_CreateWindow("6502EMU - NES", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      NES_SCREEN_W * scale, NES_SCREEN_H * scale, 0);
  if (window == NULL) {
    fprintf(stderr, "SDL window failed: %s\n", SDL_GetError());
    exit(1);
  }
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer == NULL) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (renderer == NULL) {
    fprintf(stderr, "SDL renderer failed: %s\n", SDL_GetError());
    exit(1);
  }
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
      NES_SCREEN_W, NES_SCREEN_H);
  if (texture == NULL) {
    fprintf(stderr, "SDL texture failed: %s\n", SDL_GetError());
    exit(1);
  }
  SDL_RenderSetLogicalSize(renderer, NES_SCREEN_W, NES_SCREEN_H);
}

static void audio_init(void) {
  if (!audio_enabled) return;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "SDL audio init failed: %s (continuing without sound)\n", SDL_GetError());
    audio_enabled = false;
    return;
  }
  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = NES_AUDIO_HZ;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 1024;
  audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_dev == 0) {
    fprintf(stderr, "SDL audio device failed: %s (continuing without sound)\n", SDL_GetError());
    audio_enabled = false;
    return;
  }
  SDL_PauseAudioDevice(audio_dev, 0);
}

static void present_frame(void) {
  SDL_UpdateTexture(texture, NULL, ppu.framebuffer, NES_SCREEN_W * sizeof(uint32_t));
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

static void record_samples(const int16_t *buf, int n) {
  if (wav_path == NULL || n <= 0) return;
  if (wav_len + (size_t)n > wav_cap) {
    size_t cap = wav_cap ? wav_cap * 2 : 8192;
    while (cap < wav_len + (size_t)n) cap *= 2;
    int16_t *p = realloc(wav_samples, cap * sizeof(int16_t));
    if (p == NULL) return;
    wav_samples = p;
    wav_cap = cap;
  }
  memcpy(wav_samples + wav_len, buf, (size_t)n * sizeof(int16_t));
  wav_len += (size_t)n;
}

static void queue_audio(void) {
  int16_t buf[2048];
  int n = apu_take_samples(&apu, buf, (int)(sizeof(buf) / sizeof(buf[0])));
  record_samples(buf, n);
  if (!audio_enabled || n <= 0) return;
  if (SDL_GetQueuedAudioSize(audio_dev) < (Uint32)(NES_AUDIO_HZ / 4 * 2)) {
    SDL_QueueAudio(audio_dev, buf, (Uint32)(n * sizeof(int16_t)));
  }
}

static void write_wav(const char *path) {
  if (path == NULL || wav_len == 0) return;
  FILE *fp = fopen(path, "wb");
  if (fp == NULL) return;
  uint32_t data_size = (uint32_t)(wav_len * sizeof(int16_t));
  uint32_t riff_size = 36 + data_size;
  uint32_t rate = NES_AUDIO_HZ;
  uint32_t byte_rate = rate * 2;
  uint16_t block_align = 2;
  uint16_t bits = 16;
  fwrite("RIFF", 1, 4, fp); fwrite(&riff_size, 4, 1, fp);
  fwrite("WAVEfmt ", 1, 8, fp);
  uint32_t fmt_size = 16; uint16_t fmt = 1; uint16_t channels = 1;
  fwrite(&fmt_size, 4, 1, fp); fwrite(&fmt, 2, 1, fp); fwrite(&channels, 2, 1, fp);
  fwrite(&rate, 4, 1, fp); fwrite(&byte_rate, 4, 1, fp);
  fwrite(&block_align, 2, 1, fp); fwrite(&bits, 2, 1, fp);
  fwrite("data", 1, 4, fp); fwrite(&data_size, 4, 1, fp);
  fwrite(wav_samples, sizeof(int16_t), wav_len, fp);
  fclose(fp);
  printf("audio written to %s (%zu samples)\n", path, wav_len);
}

static void cpu_step(void) {
  Decode s;
  memset(&s, 0, sizeof(s));
  s.pc = cpu.pc & 0xFFFF;
  s.snpc = s.pc;
  s.dnpc = s.pc;
  uint64_t before = cpu.cycles;
  isa_exec_once(&s);
  cpu.pc = s.dnpc & 0xFFFF;
  int delta = (int)(cpu.cycles - before);
  if (delta <= 0) delta = 1;

  ppu_step(&ppu, delta * 3);
  apu_step(&apu, delta, true);

  if (ppu.nmi_trigger) {
    ppu.nmi_trigger = false;
    cpu.nmi_pending = true;
  }
  if (cart.irq) cpu.irq_pending = true;
  if (apu.frame_irq) cpu.irq_pending = true;
}

static bool run_frame(void) {
  ppu.frame_ready = false;
  poll_events();
  while (!ppu.frame_ready && !g_quit) {
    cpu_step();
    if (nemu_state.state != NEMU_RUNNING) break;
  }
  present_frame();
  queue_audio();
  return !g_quit && nemu_state.state == NEMU_RUNNING;
}

static void save_screenshot(const char *path) {
  FILE *fp = fopen(path, "wb");
  if (fp == NULL) return;
  fprintf(fp, "P6\n%d %d\n255\n", NES_SCREEN_W, NES_SCREEN_H);
  for (int i = 0; i < NES_SCREEN_W * NES_SCREEN_H; i++) {
    uint32_t c = ppu.framebuffer[i];
    uint8_t rgb[3] = { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c };
    fwrite(rgb, 1, 3, fp);
  }
  fclose(fp);
}

static void usage(const char *prog) {
  printf("Usage: %s [OPTIONS] GAME.nes\n\n", prog);
  printf("  --frames=N       stop after N frames (default: run until window closes)\n");
  printf("  --scale=N        window scale, default 3\n");
  printf("  --no-audio       disable APU audio output\n");
  printf("  --headless       use SDL dummy video/audio drivers\n");
  printf("  --fast           do not wait for 60 Hz frame pacing\n");
  printf("  --screenshot=F   write the last frame as a PPM image\n");
  printf("  --wav=F          write APU output as a mono 16-bit WAV file\n");
  printf("  --hold=BUTTONS   hold comma-separated buttons (testing aid)\n");
  printf("  --help           show this help\n\n");
  printf("Controls: 1P WASD + Z/X/C/V; 2P arrows + comma/period/slash/apostrophe\n");
}

static int parse_int(const char *s, int fallback) {
  char *end = NULL;
  long v = strtol(s, &end, 0);
  if (end == s || *end != '\0' || v < 0) return fallback;
  return (int)v;
}

int nes_main(int argc, char **argv) {
  const char *rom_path = NULL;
  bool no_audio = false;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0) { usage(argv[0]); return 0; }
    if (strcmp(argv[i], "--nes") == 0) continue;
    if (strncmp(argv[i], "--frames=", 9) == 0) { max_frames = parse_int(argv[i] + 9, 0); continue; }
    if (strncmp(argv[i], "--scale=", 8) == 0) { scale = parse_int(argv[i] + 8, 3); if (scale < 1) scale = 1; continue; }
    if (strncmp(argv[i], "--screenshot=", 13) == 0) { screenshot_path = argv[i] + 13; continue; }
    if (strncmp(argv[i], "--wav=", 6) == 0) { wav_path = argv[i] + 6; continue; }
    if (strncmp(argv[i], "--hold=", 7) == 0) {
      const char *h = argv[i] + 7;
      if (has_token(h, "a")) forced_buttons |= 0x01;
      if (has_token(h, "b")) forced_buttons |= 0x02;
      if (has_token(h, "select")) forced_buttons |= 0x04;
      if (has_token(h, "start")) forced_buttons |= 0x08;
      if (has_token(h, "up")) forced_buttons |= 0x10;
      if (has_token(h, "down")) forced_buttons |= 0x20;
      if (has_token(h, "left")) forced_buttons |= 0x40;
      if (has_token(h, "right")) forced_buttons |= 0x80;
      continue;
    }
    if (strcmp(argv[i], "--no-audio") == 0) { no_audio = true; continue; }
    if (strcmp(argv[i], "--headless") == 0) { headless = true; continue; }
    if (strcmp(argv[i], "--fast") == 0) { fast_mode = true; continue; }
    if (strncmp(argv[i], "--log=", 6) == 0) continue;
    if (ends_with_nes(argv[i])) { rom_path = argv[i]; continue; }
  }
  if (rom_path == NULL) {
    usage(argv[0]);
    return 1;
  }
  if (headless) {
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_AUDIODRIVER", "dummy", 1);
  }

  if (!cart_load(rom_path, &cart)) {
    fprintf(stderr, "failed to load iNES ROM: %s\n", rom_path);
    return 1;
  }
  printf("6502EMU NES: %s, mapper %d, PRG %zu bytes, CHR %zu bytes\n",
      rom_path, cart.mapper, cart.prg_size, cart.chr_size);

  memset(cpu_ram, 0, sizeof(cpu_ram));
  ppu_reset(&ppu, &cart);
  apu_reset(&apu);
  memset(controller_state, 0, sizeof(controller_state));
  controller_shift[0] = controller_shift[1] = 0;
  controller_strobe = 0;
  g_quit = false;
  nemu_state.state = NEMU_RUNNING;
  nemu_state.halt_pc = 0;
  nemu_state.halt_ret = 0;

  cpu.a = 0;
  cpu.x = 0;
  cpu.y = 0;
  cpu.sp = 0xFD;
  cpu.p = 0x24;
  cpu.cycles = 0;
  cpu.irq_pending = false;
  cpu.nmi_pending = false;
  cpu.is_65c02 = false;
  cpu.pc = (vaddr_t)(nes_cpu_read(0xFFFC) | (nes_cpu_read(0xFFFD) << 8));

  nes_active = true;
  audio_enabled = !no_audio;
  video_init();
  audio_init();

  uint32_t next_tick = SDL_GetTicks();
  int frames = 0;
  while (!g_quit) {
    if (!run_frame()) break;
    frames++;
    if (max_frames > 0 && frames >= max_frames) break;
    if (!headless && !fast_mode) {
      next_tick += 1000 / NES_FPS;
      uint32_t now = SDL_GetTicks();
      if ((int32_t)(next_tick - now) > 0) SDL_Delay(next_tick - now);
      else next_tick = now;
    }
  }

  if (screenshot_path != NULL) {
    save_screenshot(screenshot_path);
    printf("screenshot written to %s\n", screenshot_path);
  }
  write_wav(wav_path);
  printf("6502EMU NES: %d frames, PC=%04X, cycles=%" PRIu64 "\n",
      frames, (unsigned)(cpu.pc & 0xFFFF), cpu.cycles);

  if (audio_dev != 0) SDL_CloseAudioDevice(audio_dev);
  if (texture != NULL) SDL_DestroyTexture(texture);
  if (renderer != NULL) SDL_DestroyRenderer(renderer);
  if (window != NULL) SDL_DestroyWindow(window);
  SDL_Quit();
  free(wav_samples);
  wav_samples = NULL;
  cart_free(&cart);
  nes_active = false;
  return (nemu_state.state == NEMU_ABORT) ? 1 : 0;
}
