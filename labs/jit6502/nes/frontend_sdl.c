/*
 * Minimal SDL2 frontend: 256x240 texture, keyboard = NES controller 1.
 *
 *   W/A/S/D or arrows : D-pad          Space/Z : A      X : B
 *   C / Right Shift   : Select         V / Enter : Start
 *   Esc               : quit
 */
#include "frontend.h"

#include <stdio.h>

#include "nes_system.h"

#ifdef HAVE_SDL2

#include <SDL.h>

static uint8_t poll_buttons(const uint8_t *keys) {
  uint8_t b = 0;
  if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) b |= 0x10;
  if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) b |= 0x20;
  if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) b |= 0x40;
  if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) b |= 0x80;
  if (keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_Z]) b |= 0x01;
  if (keys[SDL_SCANCODE_X]) b |= 0x02;
  if (keys[SDL_SCANCODE_C] || keys[SDL_SCANCODE_RSHIFT]) b |= 0x04;
  if (keys[SDL_SCANCODE_V] || keys[SDL_SCANCODE_RETURN]) b |= 0x08;
  return b;
}

int nes_window_available(void) { return 1; }

int nes_run_window(Cpu *c, int scale, NesFrameStep step) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "sdl: %s\n", SDL_GetError());
    return 0;
  }

  /* Mono 44.1 kHz, 16-bit, queue-based: pull samples once per emulated frame
   * and let SDL drain them. */
  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = NES_AUDIO_HZ;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 1024;
  SDL_AudioDeviceID audio_dev =
      SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_dev != 0) SDL_PauseAudioDevice(audio_dev, 0);
  if (scale < 1) scale = 1;

  SDL_Window *win = SDL_CreateWindow(
      "Open6502 JIT - NES", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      NES_SCREEN_W * scale, NES_SCREEN_H * scale,
      SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
  if (win == NULL) {
    fprintf(stderr, "sdl: %s\n", SDL_GetError());
    SDL_Quit();
    return 0;
  }
  SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
  if (ren == NULL) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       NES_SCREEN_W, NES_SCREEN_H);
  if (ren == NULL || tex == NULL) {
    fprintf(stderr, "sdl: %s\n", SDL_GetError());
    if (tex != NULL) SDL_DestroyTexture(tex);
    if (ren != NULL) SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
  }

  const uint8_t *keys = SDL_GetKeyboardState(NULL);
  int running = 1;
  uint64_t next = SDL_GetTicks64();
  const double frame_ms = 1000.0 / 60.0988;

  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) running = 0;
      if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
        running = 0;
      }
    }
    if (!running) break;

    nes_set_buttons(poll_buttons(keys), 0);
    if (!step(c)) {
      fprintf(stderr, "nes: emulation error\n");
      break;
    }

    if (audio_dev != 0 &&
        SDL_GetQueuedAudioSize(audio_dev) < (Uint32)(NES_AUDIO_HZ / 4 * 2)) {
      int16_t samples[2048];
      int n = nes_audio_pull(samples, (int)(sizeof(samples) / sizeof(samples[0])));
      if (n > 0) {
        SDL_QueueAudio(audio_dev, samples, (Uint32)(n * sizeof(int16_t)));
      }
    }

    SDL_UpdateTexture(tex, NULL, g_ppu.framebuffer, NES_SCREEN_W * 4);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);

    /* Pace to NTSC frame rate. */
    next += (uint64_t)frame_ms;
    uint64_t now = SDL_GetTicks64();
    if (next > now) {
      SDL_Delay((uint32_t)(next - now));
    } else if (now - next > 100) {
      next = now; /* resync after a stall */
    }
  }

  if (audio_dev != 0) SDL_CloseAudioDevice(audio_dev);
  SDL_DestroyTexture(tex);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 1;
}

#else /* !HAVE_SDL2 */

int nes_window_available(void) { return 0; }

int nes_run_window(Cpu *c, int scale, NesFrameStep step) {
  (void)c;
  (void)scale;
  (void)step;
  fprintf(stderr, "nes: this build has no SDL2 support (install sdl2)\n");
  return 0;
}

#endif
