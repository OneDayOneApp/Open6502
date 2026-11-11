/* Exercise the same SDL event handler and draw path used by the player. */
#include "../src/frontend.c"
#include <assert.h>

static void key(Player *p, SDL_Renderer *r, SDL_Scancode code, bool down) {
  SDL_Event e = {0};
  e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
  e.key.keysym.scancode = code;
  handle_event(p, r, &e);
  update_input(p);
}

int main(void) {
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0);
  SDL_Window *w = SDL_CreateWindow("test", 0, 0, 512, 540, SDL_WINDOW_HIDDEN);
  assert(w);
  SDL_Renderer *r = SDL_CreateRenderer(w, -1, SDL_RENDERER_SOFTWARE);
  assert(r && SDL_RenderSetLogicalSize(r, 512, 540) == 0);
  SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 256, 240);
  assert(t);
  Player p = {.g = gem_create(), .rom = "../roms/file-fixers.nes"};
  assert(p.g && gem_load_game(p.g, p.rom) == 0);
  SDL_AudioSpec spec = {0};
  spec.freq = GEM_AUDIO_HZ; spec.format = AUDIO_S16SYS; spec.channels = 1; spec.samples = 1024;
  p.audio = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
  assert(p.audio);
  gem_apu_advance(p.g, 10000);
  play_audio(&p);
  assert(SDL_GetQueuedAudioSize(p.audio) > 0);
  key(&p, r, SDL_SCANCODE_M, true);
  assert(p.muted && SDL_GetQueuedAudioSize(p.audio) == 0);
  key(&p, r, SDL_SCANCODE_M, false);
  key(&p, r, SDL_SCANCODE_M, true);
  assert(!p.muted);

  key(&p, r, SDL_SCANCODE_SPACE, true);
  assert(!p.paused);
  key(&p, r, SDL_SCANCODE_RIGHT, true);
  assert(p.g->apu.joy1 == 0x81);
  gem_bus_write(p.g, 0x4016, 1);
  assert(gem_bus_read(p.g, 0x4016, true) == 1);
  assert(gem_bus_read(p.g, 0x4016, true) == 1); /* high strobe does not shift */
  gem_bus_write(p.g, 0x4016, 0);
  key(&p, r, SDL_SCANCODE_SPACE, false);
  key(&p, r, SDL_SCANCODE_RIGHT, false);
  for (int bit = 0; bit < 8; bit++)
    assert(gem_bus_read(p.g, 0x4016, true) == ((0x81 >> bit) & 1));
  assert(p.g->apu.joy1 == 0);
  key(&p, r, SDL_SCANCODE_P, true);
  assert(p.paused);
  key(&p, r, SDL_SCANCODE_F1, true);
  assert(p.help);
  key(&p, r, SDL_SCANCODE_P, true);
  assert(!p.paused && !p.help);
  SDL_Event e = {0};
  e.type = SDL_WINDOWEVENT;
  e.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
  handle_event(&p, r, &e);
  assert(p.paused && !p.g->apu.joy1);
  e.type = SDL_MOUSEBUTTONDOWN;
  e.button.button = SDL_BUTTON_LEFT;
  e.button.x = 120; e.button.y = 495;
  handle_event(&p, r, &e);
  assert(!p.paused);
  e.button.x = 20;
  handle_event(&p, r, &e);
  update_input(&p);
  assert(p.g->apu.joy1 == 8);
  e.type = SDL_MOUSEBUTTONUP;
  handle_event(&p, r, &e);
  update_input(&p);
  assert(p.g->apu.joy1 == 0);
  for (int game = 0; game < 2; game++) {
    p.rom = game ? "../roms/stallar.nes" : "../roms/file-fixers.nes";
    action(&p, 2);
    assert(!p.error && p.g->stats.ppu_frames == 0);
    while (p.g->stats.ppu_frames < 120 && !p.g->cpu.halted) gem_step(p.g);
    key(&p, r, SDL_SCANCODE_RETURN, true);
    while (p.g->stats.ppu_frames < 180 && !p.g->cpu.halted) gem_step(p.g);
    key(&p, r, SDL_SCANCODE_RETURN, false);
    while (p.g->stats.ppu_frames < 240 && !p.g->cpu.halted) gem_step(p.g);
    assert(p.g->stats.ppu_frames == 240 && !p.g->cpu.halted);
    uint8_t rgb[256 * 240 * 3];
    gem_render_frame(p.g, rgb, 256, 240);
    assert(SDL_UpdateTexture(t, NULL, rgb, 256 * 3) == 0);
    draw_ui(r, t, &p);
    p.paused = p.help = true;
    draw_ui(r, t, &p);
    puts(game ? "PASS: Stallar SDL input/render gameplay" : "PASS: File Fixers SDL input/render gameplay");
  }
  key(&p, r, SDL_SCANCODE_ESCAPE, true);
  assert(p.quit);
  SDL_CloseAudioDevice(p.audio);
  gem_destroy(p.g);
  SDL_DestroyTexture(t);
  SDL_DestroyRenderer(r);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: SDL controls, pause, help, focus loss, reset and quit");
}
