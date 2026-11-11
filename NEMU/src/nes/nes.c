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
static const char *save_state_path;
static const char *load_state_path;
static const char *record_input_path;
static const char *replay_input_path;
static FILE *record_input_fp;
static uint8_t *replay_input_data;
static size_t replay_input_size;
static size_t replay_input_pos;
static int16_t *wav_samples;
static size_t wav_len;
static size_t wav_cap;
static bool print_hash;
static uint64_t frame_hash_state;
static uint64_t audio_hash_state;

#define HASH_SEED UINT64_C(1469598103934665603)

static uint64_t hash_bytes(uint64_t h, const void *data, size_t n) {
  const uint8_t *p = data;
  for (size_t i = 0; i < n; i++) {
    h ^= p[i];
    h *= UINT64_C(1099511628211);
  }
  return h;
}

static uint8_t ppu_bus_chr_read(void *bus, uint16_t addr) {
  return cart_chr_read((NES_Cart *)bus, addr);
}
static void ppu_bus_chr_write(void *bus, uint16_t addr, uint8_t value) {
  cart_chr_write((NES_Cart *)bus, addr, value);
}
static int ppu_bus_nt_index(void *bus, uint16_t addr) {
  return cart_nt_index((NES_Cart *)bus, addr);
}
static void ppu_bus_scanline(void *bus) {
  cart_scanline((NES_Cart *)bus);
}

static uint8_t apu_dmc_read(void *bus, uint16_t addr) {
  (void)bus;
  return nes_cpu_read(addr);
}

static void apu_attach_bus(void) {
  apu.dmc_read = apu_dmc_read;
  apu.dmc_bus = NULL;
}

static void ppu_attach_cart(void) {
  ppu_set_bus(&ppu, &cart, ppu_bus_chr_read, ppu_bus_chr_write,
      ppu_bus_nt_index, ppu_bus_scanline);
}

#define NES_STATE_MAGIC "NESST001"
#define NES_STATE_VERSION 1u

typedef struct {
  CPU_state cpu;
  uint8_t cpu_ram[2048];
  NES_PPU ppu;
  NES_APU apu;
  NES_Cart cart;
  uint8_t controller_state[2];
  uint8_t controller_shift[2];
  uint8_t controller_strobe;
  uint64_t audio_hash_state;
} NESState;

typedef struct {
  char magic[8];
  uint32_t version;
  uint32_t reserved;
  uint64_t rom_hash;
} NESStateHeader;

static uint64_t cart_rom_hash(void) {
  uint64_t h = HASH_SEED;
  h = hash_bytes(h, cart.prg, cart.prg_size);
  if (!cart.chr_ram) h = hash_bytes(h, cart.chr, cart.chr_size);
  return h;
}

static bool save_state(const char *path) {
  if (path == NULL) return false;
  NESState s;
  memset(&s, 0, sizeof(s));
  s.cpu = cpu;
  memcpy(s.cpu_ram, cpu_ram, sizeof(s.cpu_ram));
  s.ppu = ppu;
  s.ppu.cart = NULL;
  s.ppu.bus = NULL;
  s.ppu.chr_read = NULL;
  s.ppu.chr_write = NULL;
  s.ppu.nt_index = NULL;
  s.ppu.on_scanline = NULL;
  s.apu = apu;
  s.cart = cart;
  s.cart.prg = NULL;
  s.cart.chr = NULL;
  memcpy(s.controller_state, controller_state, sizeof(controller_state));
  memcpy(s.controller_shift, controller_shift, sizeof(controller_shift));
  s.controller_strobe = controller_strobe;
  s.audio_hash_state = audio_hash_state;

  NESStateHeader h;
  memset(&h, 0, sizeof(h));
  memcpy(h.magic, NES_STATE_MAGIC, 8);
  h.version = NES_STATE_VERSION;
  h.rom_hash = cart_rom_hash();

  FILE *fp = fopen(path, "wb");
  if (fp == NULL) return false;
  bool ok = fwrite(&h, sizeof(h), 1, fp) == 1 &&
            fwrite(&s, sizeof(s), 1, fp) == 1;
  if (ok && s.cart.chr_ram) {
    ok = fwrite(cart.chr, 1, cart.chr_size, fp) == cart.chr_size;
  }
  fclose(fp);
  return ok;
}

static bool load_state(const char *path) {
  if (path == NULL) return false;
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) return false;
  NESStateHeader h;
  NESState s;
  bool ok = fread(&h, sizeof(h), 1, fp) == 1 &&
            fread(&s, sizeof(s), 1, fp) == 1;
  if (!ok || memcmp(h.magic, NES_STATE_MAGIC, 8) != 0 ||
      h.version != NES_STATE_VERSION || h.rom_hash != cart_rom_hash()) {
    fclose(fp);
    return false;
  }
  if (s.cart.prg_size != cart.prg_size || s.cart.chr_size != cart.chr_size ||
      s.cart.mapper != cart.mapper) {
    fclose(fp);
    return false;
  }

  uint8_t *prg = cart.prg;
  uint8_t *chr = cart.chr;
  cart = s.cart;
  cart.prg = prg;
  cart.chr = chr;
  if (cart.chr_ram) {
    ok = fread(cart.chr, 1, cart.chr_size, fp) == cart.chr_size;
  }
  fclose(fp);
  if (!ok) return false;

  cpu = s.cpu;
  memcpy(cpu_ram, s.cpu_ram, sizeof(cpu_ram));
  ppu = s.ppu;
  ppu.cart = &cart;
  ppu_attach_cart();
  apu = s.apu;
  apu_attach_bus();
  memcpy(controller_state, s.controller_state, sizeof(controller_state));
  memcpy(controller_shift, s.controller_shift, sizeof(controller_shift));
  controller_strobe = s.controller_strobe;
  audio_hash_state = s.audio_hash_state;
  nemu_state.state = NEMU_RUNNING;
  nemu_state.halt_pc = 0;
  nemu_state.halt_ret = 0;
  g_quit = false;
  return true;
}

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

#define NES_INPUT_MAGIC "NESINP01"

static bool open_record_input(void) {
  if (record_input_path == NULL) return true;
  record_input_fp = fopen(record_input_path, "wb");
  if (record_input_fp == NULL) return false;
  return fwrite(NES_INPUT_MAGIC, 1, 8, record_input_fp) == 8;
}

static bool load_replay_input(void) {
  if (replay_input_path == NULL) return true;
  FILE *fp = fopen(replay_input_path, "rb");
  if (fp == NULL) return false;
  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  rewind(fp);
  if (len < 10) { fclose(fp); return false; }
  uint8_t *buf = malloc((size_t)len);
  if (buf == NULL) { fclose(fp); return false; }
  if (fread(buf, 1, (size_t)len, fp) != (size_t)len ||
      memcmp(buf, NES_INPUT_MAGIC, 8) != 0) {
    free(buf); fclose(fp); return false;
  }
  replay_input_data = buf;
  replay_input_size = (size_t)len;
  replay_input_pos = 8;
  return true;
}

static void apply_replay_input(void) {
  uint8_t p1 = 0, p2 = 0;
  if (replay_input_data != NULL && replay_input_pos + 1 < replay_input_size) {
    p1 = replay_input_data[replay_input_pos++];
    p2 = replay_input_data[replay_input_pos++];
  }
  controller_state[0] = p1 | forced_buttons;
  controller_state[1] = p2;
}

static void record_input_frame(void) {
  if (record_input_fp == NULL) return;
  fputc(controller_state[0], record_input_fp);
  fputc(controller_state[1], record_input_fp);
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

  /* Player 1 buttons: Space/Z = A (jump), X = B, C/V = Select/Start.
   * Right Shift and Return remain aliases for Select/Start. */
  if (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_Z]) p1 |= 0x01;
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
  if (n > 0) audio_hash_state = hash_bytes(audio_hash_state, buf, (size_t)n * sizeof(int16_t));
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
  if (getenv("NEMU_DEBUG_FBA0") && s.pc == 0xFBA0) {
    fprintf(stderr, "NEMU FBA0 sp=%02x sel=%02x regs=%02x %02x %02x %02x %02x %02x %02x %02x\n",
            cpu.sp, cart.bank_select, cart.bank_regs[0], cart.bank_regs[1],
            cart.bank_regs[2], cart.bank_regs[3], cart.bank_regs[4],
            cart.bank_regs[5], cart.bank_regs[6], cart.bank_regs[7]);
    fprintf(stderr, "NEMU stack:");
    for (int i = 1; i <= 16; i++) fprintf(stderr, " %02x", nes_cpu_read((uint16_t)(0x100 | (uint8_t)(cpu.sp + i))));
    fprintf(stderr, "\n");
  }
  s.snpc = s.pc;
  s.dnpc = s.pc;
  uint64_t before = cpu.cycles;
  isa_exec_once(&s);
  cpu.pc = s.dnpc & 0xFFFF;
  {
    static FILE *trace_fp = NULL;
    static int trace_init = 0;
    if (!trace_init) {
      const char *path = getenv("NEMU_NES_TRACE");
      if (path) trace_fp = fopen(path, "w");
      trace_init = 1;
    }
    if (trace_fp) fprintf(trace_fp, "%04x %02x %02x %02x %02x %02x %llu %d %d %d\n",
        (unsigned)(cpu.pc & 0xFFFF), cpu.a, cpu.x, cpu.y, cpu.sp, cpu.p,
        (unsigned long long)cpu.cycles, ppu.scanline, ppu.cycle, ppu.nmi_trigger);
  }
  int delta = (int)(cpu.cycles - before);
  if (delta <= 0) delta = 1;

  ppu_step(&ppu, delta * 3);
  apu_step(&apu, delta, true);

  if (ppu.nmi_trigger) {
    ppu.nmi_trigger = false;
    cpu.nmi_pending = true;
  }
  if (cart.irq) cpu.irq_pending = true;
  if (apu.frame_irq || apu.dmc.irq) cpu.irq_pending = true;
}

static bool run_frame(void) {
  ppu.frame_ready = false;
  if (!headless) poll_events();
  if (replay_input_data != NULL) apply_replay_input();
  record_input_frame();
  while (!ppu.frame_ready && !g_quit) {
    cpu_step();
    if (nemu_state.state != NEMU_RUNNING) break;
  }
  if (!headless) present_frame();
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
  printf("  --hash           print deterministic frame/audio hashes\n");
  printf("  --save-state=F   save NES state after the run\n");
  printf("  --load-state=F   load NES state before the run\n");
  printf("  --record-input=F record per-frame controller state\n");
  printf("  --replay-input=F replay per-frame controller state\n");
  printf("  --help           show this help\n\n");
  printf("Controls: 1P WASD + Space/Z/X/C/V; 2P arrows + comma/period/slash/apostrophe\n");
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
    if (strcmp(argv[i], "--hash") == 0) { print_hash = true; continue; }
    if (strncmp(argv[i], "--save-state=", 13) == 0) { save_state_path = argv[i] + 13; continue; }
    if (strncmp(argv[i], "--load-state=", 13) == 0) { load_state_path = argv[i] + 13; continue; }
    if (strncmp(argv[i], "--record-input=", 15) == 0) { record_input_path = argv[i] + 15; continue; }
    if (strncmp(argv[i], "--replay-input=", 15) == 0) { replay_input_path = argv[i] + 15; continue; }
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
  ppu_attach_cart();
  apu_reset(&apu);
  apu_attach_bus();
  memset(controller_state, 0, sizeof(controller_state));
  controller_state[0] = forced_buttons;
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
  frame_hash_state = HASH_SEED;
  audio_hash_state = HASH_SEED;
  if (load_state_path != NULL && !load_state(load_state_path)) {
    fprintf(stderr, "failed to load NES state: %s\n", load_state_path);
    cart_free(&cart);
    return 1;
  }
  if (!open_record_input() || !load_replay_input()) {
    fprintf(stderr, "failed to initialize input record/replay\n");
    if (record_input_fp != NULL) fclose(record_input_fp);
    free(replay_input_data);
    cart_free(&cart);
    return 1;
  }
  audio_enabled = !no_audio && !headless;
  if (!headless) {
    video_init();
    audio_init();
  }

  uint32_t next_tick = headless ? 0 : SDL_GetTicks();
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
  if (save_state_path != NULL && !save_state(save_state_path)) {
    fprintf(stderr, "failed to save NES state: %s\n", save_state_path);
  }
  frame_hash_state = hash_bytes(frame_hash_state, ppu.framebuffer,
      sizeof(ppu.framebuffer));
  if (print_hash) {
    printf("6502EMU NES hash: frame=%016" PRIx64 " audio=%016" PRIx64
        " frames=%d\n", frame_hash_state, audio_hash_state, frames);
  }
  printf("6502EMU NES: %d frames, PC=%04X, cycles=%" PRIu64 "\n",
      frames, (unsigned)(cpu.pc & 0xFFFF), cpu.cycles);

  if (!headless) {
    if (audio_dev != 0) SDL_CloseAudioDevice(audio_dev);
    if (texture != NULL) SDL_DestroyTexture(texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    SDL_Quit();
  }
  if (record_input_fp != NULL) fclose(record_input_fp);
  free(replay_input_data);
  replay_input_data = NULL;
  free(wav_samples);
  wav_samples = NULL;
  cart_free(&cart);
  nes_active = false;
  return (nemu_state.state == NEMU_ABORT) ? 1 : 0;
}
