#include "frontend.h"
#include "gem6502.h"
#include "render.h"
#include <stdio.h>
#include <string.h>

#ifndef GEM_HAVE_SDL
int gem_play(const char *rom) {
  (void)rom;
  fprintf(stderr, "gem6502: play requires SDL2. Install SDL2, then rebuild with make -B.\n");
  return 2;
}
#else
#include <SDL.h>

/* Small built-in font keeps the frontend independent of system font files. */
static const unsigned char font[36][5] = {
  {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
  {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
  {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
  {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
  {62,65,81,33,94},{127,9,25,41,70},{70,73,73,73,49},{1,1,127,1,1},
  {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
  {7,8,112,8,7},{97,81,73,69,67},
  {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},
  {24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},
  {54,73,73,73,54},{6,73,73,41,30}
};

static void text(SDL_Renderer *r, int x, int y, const char *s, int scale) {
  for (; *s; s++, x += 6 * scale) {
    int idx = *s >= 'A' && *s <= 'Z' ? *s - 'A' :
              *s >= '0' && *s <= '9' ? *s - '0' + 26 : -1;
    if (idx < 0) continue;
    for (int col = 0; col < 5; col++) for (int row = 0; row < 7; row++)
      if (font[idx][col] & (1 << row)) {
        SDL_Rect pixel = {x + col * scale, y + row * scale, scale, scale};
        SDL_RenderFillRect(r, &pixel);
      }
  }
}

typedef struct {
  Gem *g;
  const char *rom;
  bool keys[SDL_NUM_SCANCODES];
  bool paused, help, quit, mouse_start, muted;
  SDL_AudioDeviceID audio;
  int error;
} Player;

static void clear_input(Player *p) {
  memset(p->keys, 0, sizeof(p->keys));
  p->mouse_start = false;
  p->g->apu.joy1 = p->g->apu.joy2 = 0;
}

static void action(Player *p, int button) {
  switch (button) {
    case 0: p->mouse_start = true; break;
    case 1:
      if (p->help) { p->help = false; p->paused = false; }
      else p->paused = !p->paused;
      break;
    case 2:
      clear_input(p);
      if (p->audio) SDL_ClearQueuedAudio(p->audio);
      p->error = gem_load_game(p->g, p->rom);
      p->paused = false;
      if (p->error) p->quit = true;
      break;
    case 3: p->help = !p->help; break;
    case 4:
      p->muted = !p->muted;
      if (p->audio) SDL_ClearQueuedAudio(p->audio);
      break;
    case 5: p->quit = true; break;
  }
}

static void handle_event(Player *p, SDL_Renderer *r, const SDL_Event *e) {
  if (e->type == SDL_QUIT) p->quit = true;
  if (e->type == SDL_WINDOWEVENT && e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
    clear_input(p);
    p->paused = true;
  }
  if (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) {
    SDL_Scancode key = e->key.keysym.scancode;
    if ((unsigned)key < SDL_NUM_SCANCODES) p->keys[key] = e->type == SDL_KEYDOWN;
    if (e->type == SDL_KEYDOWN && !e->key.repeat) {
      if (key == SDL_SCANCODE_ESCAPE) p->quit = true;
      if (key == SDL_SCANCODE_P) action(p, 1);
      if (key == SDL_SCANCODE_R) action(p, 2);
      if (key == SDL_SCANCODE_F1) action(p, 3);
      if (key == SDL_SCANCODE_M) action(p, 4);
    }
  }
  if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT)
    p->mouse_start = false;
  if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
    float x, y;
    SDL_RenderWindowToLogical(r, e->button.x, e->button.y, &x, &y);
    if (y >= 484 && y < 512 && x >= 6 && x < 504)
      action(p, ((int)x - 6) / 83);
  }
}

static void update_input(Player *p) {
  const bool *k = p->keys;
  p->g->apu.joy1 =
      ((k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_SPACE]) ? 1 : 0) | (k[SDL_SCANCODE_X] ? 2 : 0) |
      (k[SDL_SCANCODE_BACKSPACE] ? 4 : 0) |
      ((k[SDL_SCANCODE_RETURN] || p->mouse_start) ? 8 : 0) |
      ((k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W]) ? 16 : 0) |
      ((k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S]) ? 32 : 0) |
      ((k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A]) ? 64 : 0) |
      ((k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) ? 128 : 0);
}

static void play_audio(Player *p) {
    int16_t samples[2048];
    int count = gem_sound_take_samples(&p->g->apu.sound, samples, 2048);
    if (p->audio) {
      if (p->paused || p->help || p->muted || p->g->cpu.halted) SDL_ClearQueuedAudio(p->audio);
      else if (count > 0) {
        /* Bound latency after device stalls instead of replaying old sound. */
        if (SDL_GetQueuedAudioSize(p->audio) > GEM_AUDIO_HZ * sizeof(int16_t) / 8)
          SDL_ClearQueuedAudio(p->audio);
        if (SDL_QueueAudio(p->audio, samples, (Uint32)(count * sizeof(int16_t)))) {
          fprintf(stderr, "gem6502: audio queue: %s\n", SDL_GetError());
          SDL_CloseAudioDevice(p->audio);
          p->audio = 0;
        }
      }
    }
}

static void draw_ui(SDL_Renderer *r, SDL_Texture *texture, const Player *p) {
  SDL_SetRenderDrawColor(r, 16, 21, 32, 255);
  SDL_RenderClear(r);
  SDL_Rect game = {0, 0, 512, 480};
  SDL_RenderCopy(r, texture, NULL, &game);
  const char *buttons[] = {"START", (p->paused || p->help) ? "RESUME" : "PAUSE", "RESET", "HELP", p->muted ? "UNMUTE" : "MUTE", "QUIT"};
  for (int i = 0; i < 6; i++) {
    SDL_Rect b = {6 + i * 83, 484, 79, 28};
    SDL_SetRenderDrawColor(r, 39, 56, 79, 255);
    SDL_RenderFillRect(r, &b);
    SDL_SetRenderDrawColor(r, 225, 236, 245, 255);
    text(r, b.x + 10, b.y + 10, buttons[i], 1);
  }
  SDL_SetRenderDrawColor(r, 175, 195, 215, 255);
  text(r, 10, 521, "ARROWS MOVE   SPACE JUMP   ENTER START   F1 HELP", 1);
  if (p->paused || p->help || p->g->cpu.halted) {
    SDL_Rect box = {32, 155, 448, 164};
    SDL_SetRenderDrawColor(r, 16, 21, 32, 255);
    SDL_RenderFillRect(r, &box);
    SDL_SetRenderDrawColor(r, 225, 236, 245, 255);
    const char *status = p->g->cpu.halted ? "CPU STOPPED" : p->help ? "CONTROLS" : "PAUSED";
    text(r, 58, 175, status, 2);
    text(r, 58, 208, "ARROWS OR WASD   MOVE", 1);
    text(r, 58, 226, "SPACE OR Z   JUMP    X   B BUTTON", 1);
    text(r, 58, 244, "ENTER   START   BACKSPACE   SELECT", 1);
    text(r, 58, 262, "P   PAUSE    R   RESTART", 1);
    text(r, 58, 280, "F1   HELP    M   MUTE    ESC   QUIT", 1);
    text(r, 58, 298, p->g->cpu.halted ? "PRESS R TO RESTART" : "CLICK RESUME OR PRESS P TO CONTINUE", 1);
  }
  SDL_RenderPresent(r);
}

int gem_play(const char *rom) {
  Player p = {.rom = rom};
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;
  SDL_Texture *texture = NULL;
  int rc = 2;
  p.g = gem_create();
  if (!p.g || gem_load_game(p.g, rom)) goto done;
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) goto sdl_error;
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  window = SDL_CreateWindow("GEM6502", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            768, 810, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) goto sdl_error;
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer || SDL_RenderSetLogicalSize(renderer, 512, 540)) goto sdl_error;
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24,
                              SDL_TEXTUREACCESS_STREAMING, 256, 240);
  if (!texture) goto sdl_error;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {
    SDL_AudioSpec wanted = {0};
    wanted.freq = GEM_AUDIO_HZ;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 1;
    wanted.samples = 1024;
    p.audio = SDL_OpenAudioDevice(NULL, 0, &wanted, NULL, 0);
  }
  if (!p.audio) fprintf(stderr, "gem6502: audio unavailable: %s\n", SDL_GetError());
  else {
    fprintf(stderr, "gem6502: audio ready, %d Hz mono (%s)\n", GEM_AUDIO_HZ, SDL_GetCurrentAudioDriver());
    SDL_PauseAudioDevice(p.audio, 0);
  }
  uint8_t rgb[256 * 240 * 3];
  uint64_t deadline = SDL_GetPerformanceCounter();
  const uint64_t frequency = SDL_GetPerformanceFrequency();
  const uint64_t frame_ticks = (uint64_t)(frequency / 60.0988);
  while (!p.quit) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(&p, renderer, &e);
    if (p.quit) break;
    update_input(&p);
    if (!p.paused && !p.help && !p.g->cpu.halted) {
      uint64_t frame = p.g->stats.ppu_frames;
      while (!p.g->cpu.halted && p.g->stats.ppu_frames == frame) gem_step(p.g);
    }
    play_audio(&p);
    gem_render_frame(p.g, rgb, 256, 240);
    if (SDL_UpdateTexture(texture, NULL, rgb, 256 * 3)) goto sdl_error;
    draw_ui(renderer, texture, &p);
    char title[512];
    snprintf(title, sizeof(title), "GEM6502 | %s | %s | Frame %llu | %s", rom,
             p.g->cpu.halted ? "CPU stopped - R to restart" :
             p.paused ? "Paused" : p.help ? "Help" : "Playing",
             (unsigned long long)p.g->stats.ppu_frames,
             !p.audio ? "Audio unavailable" : p.muted ? "Muted" : "Sound on");
    SDL_SetWindowTitle(window, title);
    deadline += frame_ticks;
    uint64_t now = SDL_GetPerformanceCounter();
    if (deadline > now) SDL_Delay((Uint32)((deadline - now) * 1000 / frequency));
    else deadline = now; /* Never fast-forward input after a slow frame. */
  }
  rc = p.error;
  goto done;
sdl_error:
  fprintf(stderr, "gem6502: SDL: %s\n", SDL_GetError());
done:
  if (p.audio) SDL_CloseAudioDevice(p.audio);
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  SDL_Quit();
  gem_destroy(p.g);
  return rc;
}
#endif
