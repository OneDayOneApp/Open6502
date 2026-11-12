/*
 * GEM6502 browser player.
 *
 * The WebAssembly module is the unmodified functional core plus a small C
 * bridge (src/gem_web.c).  Everything here is presentation: canvas blitting,
 * keyboard/gamepad input, WebAudio scheduling and 60.0988 Hz pacing.
 */
"use strict";

const WIDTH = 256;
const HEIGHT = 240;
const RGBA_BYTES = WIDTH * HEIGHT * 4;
const FRAME_MS = 1000 / 60.0988;
const MAX_CATCHUP = 2;

const GAMES = [
  { file: "falling.nes", title: "Falling", note: "MIT · NROM" },
  { file: "galaxypatrol.nes", title: "Galaxy Patrol", note: "MIT · NROM" },
  { file: "hanoi.nes", title: "Hanoi Tower", note: "MIT · NROM" },
  { file: "table_tennis.nes", title: "8bit Table Tennis", note: "MIT · NROM" },
  { file: "file-fixers.nes", title: "File Fixers", note: "MIT · MMC3" },
  { file: "stallar.nes", title: "Stallar", note: "MIT · MMC3" },
];

/* Site embedding hook: prefix for gem6502.wasm and roms/. */
const BASE = window.GEM_PLAYER_BASE || "";

/* Defaults are Chinese; pages can override any key before loading this file. */
const TEXT = Object.assign(
  {
    loading: "加载中",
    running: "运行中",
    paused: "已暂停",
    loadFailed: "加载失败",
    initFailed: "初始化失败",
    halted: "CPU 已停止",
    clickStart: "点击开始",
    clickStartSub: "CLICK TO START · 音频需要一次用户操作",
    pausedSub: "按 P 或点击继续",
    haltedSub: "按 R 或点击重新载入",
    loadingSub: "正在实例化 WebAssembly 模块",
    pause: "暂停",
    resume: "继续",
    soundOn: "声音：开",
    soundOff: "声音：关",
    soundUnavailable: "声音：不可用",
    soundPending: "待启用（需点击）",
    muted: "静音",
    audioOn: "开",
    romTooLarge: (size, cap) => `ROM 太大：${size} 字节（上限 ${cap}）`,
    wasmHttp: (status) => `无法读取 gem6502.wasm（HTTP ${status}）`,
    romHttp: (file, status) => `无法读取 ${file}（HTTP ${status}）`,
  },
  window.GEM_PLAYER_I18N || {}
);

const JOY = {
  A: 0x01,
  B: 0x02,
  SELECT: 0x04,
  START: 0x08,
  UP: 0x10,
  DOWN: 0x20,
  LEFT: 0x40,
  RIGHT: 0x80,
};

const KEYMAP = {
  ArrowUp: JOY.UP, KeyW: JOY.UP,
  ArrowDown: JOY.DOWN, KeyS: JOY.DOWN,
  ArrowLeft: JOY.LEFT, KeyA: JOY.LEFT,
  ArrowRight: JOY.RIGHT, KeyD: JOY.RIGHT,
  KeyZ: JOY.A, Space: JOY.A,
  KeyX: JOY.B,
  Backspace: JOY.SELECT, ShiftLeft: JOY.SELECT, ShiftRight: JOY.SELECT,
  Enter: JOY.START,
};

const $ = (id) => document.getElementById(id);

const dom = {
  canvas: $("screen"),
  overlay: $("overlay"),
  overlayTitle: $("overlay-title"),
  overlaySub: $("overlay-sub"),
  toast: $("toast"),
  select: $("game-select"),
  buttonStart: $("btn-start"),
  buttonPause: $("btn-pause"),
  buttonReset: $("btn-reset"),
  buttonSound: $("btn-sound"),
  file: $("file-input"),
  screenWrap: $("screen-wrap"),
  statState: $("stat-state"),
  statGame: $("stat-game"),
  statFps: $("stat-fps"),
  statFrames: $("stat-frames"),
  statCycles: $("stat-cycles"),
  statAudio: $("stat-audio"),
};

const root = document.getElementById("emulator");

function emulatorFocused() {
  return !root || root.contains(document.activeElement);
}

function focusEmulator() {
  if (!root) return;
  try {
    root.focus({ preventScroll: true });
  } catch {
    root.focus();
  }
}

/* Play links target #emulator; centre the card when it fits the viewport. */
function scrollToEmulator() {
  if (!root) return;
  const rect = root.getBoundingClientRect();
  const nav = document.querySelector("header.site-nav, .topbar");
  const navHeight = nav ? nav.getBoundingClientRect().height : 0;
  const fits = rect.height + navHeight + 24 <= window.innerHeight;
  const target = fits
    ? Math.max(navHeight + 12, (window.innerHeight - rect.height) / 2)
    : navHeight + 12;
  const smooth = !window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  window.scrollTo({
    top: Math.max(0, window.scrollY + rect.top - target),
    behavior: smooth ? "smooth" : "auto",
  });
}

const ctx2d = dom.canvas.getContext("2d", { alpha: false });
const imageData = ctx2d.createImageData(WIDTH, HEIGHT);

const state = {
  wasm: null,
  loaded: false,
  started: false,
  paused: false,
  halted: false,
  keys: new Set(),
  padBits: 0,
  touchBits: 0,
  pulseMask: 0,
  pulseFrames: 0,
  gameTitle: "",
  lastRom: null,
  frameAcc: 0,
  lastTs: 0,
  rafId: 0,
  fpsFrames: 0,
  fpsSince: 0,
  fps: 0,
  sound: true,
  toastTimer: 0,
  audio: {
    ctx: null,
    gain: null,
    nextTime: 0,
    sources: new Set(),
  },
};

/* --------------------------------------------------------------- helpers */

function readCString(ptr) {
  const mem = new Uint8Array(state.wasm.memory.buffer);
  let out = "";
  for (let i = 0; mem[ptr + i]; i++) out += String.fromCharCode(mem[ptr + i]);
  return out;
}

function toast(message) {
  dom.toast.textContent = message;
  dom.toast.hidden = false;
  clearTimeout(state.toastTimer);
  state.toastTimer = setTimeout(() => { dom.toast.hidden = true; }, 4200);
}

function showOverlay(title, sub) {
  dom.overlayTitle.textContent = title;
  dom.overlaySub.textContent = sub;
  dom.overlay.hidden = false;
}

function hideOverlay() {
  dom.overlay.hidden = true;
}

function setState(text) {
  dom.statState.textContent = text;
}

/* ------------------------------------------------------------------ wasm */

async function instantiate() {
  const response = await fetch(`${BASE}gem6502.wasm`);
  if (!response.ok) throw new Error(TEXT.wasmHttp(response.status));
  if (WebAssembly.instantiateStreaming) {
    try {
      return (await WebAssembly.instantiateStreaming(response.clone(), {})).instance;
    } catch {
      /* fall through: server may use the wrong MIME type */
    }
  }
  const bytes = await response.arrayBuffer();
  return (await WebAssembly.instantiate(bytes, {})).instance;
}

function applyRom(bytes, title) {
  const wasm = state.wasm;
  const capacity = wasm.gem_web_rom_capacity();
  if (bytes.length > capacity) {
    throw new Error(TEXT.romTooLarge(bytes.length, capacity));
  }

  let mem = new Uint8Array(wasm.memory.buffer);
  mem.set(bytes, wasm.gem_web_rom_buffer());
  const rc = wasm.gem_web_load_rom(wasm.gem_web_rom_buffer(), bytes.length);
  if (rc !== 0) throw new Error(readCString(wasm.gem_web_error()));

  state.lastRom = bytes.slice();
  state.gameTitle = title;
  state.loaded = true;
  state.halted = false;
  state.paused = false;
  state.frameAcc = 0;
  state.lastTs = 0;
  state.pulseFrames = 0;
  stopAudio();
  hideOverlay();
  setState(TEXT.running);
  dom.buttonPause.textContent = TEXT.pause;
  dom.statGame.textContent = title;
  dom.statFrames.textContent = "0";
  dom.statCycles.textContent = "0";
  dom.statFps.textContent = "—";

  if (!state.started) {
    showOverlay(TEXT.clickStart, TEXT.clickStartSub);
  }
}

async function loadGame(entry, { quiet = false } = {}) {
  setState(TEXT.loading);
  try {
    const response = await fetch(`${BASE}roms/${entry.file}`);
    if (!response.ok) throw new Error(TEXT.romHttp(entry.file, response.status));
    const bytes = new Uint8Array(await response.arrayBuffer());
    applyRom(bytes, entry.title);
    if (!quiet) dom.select.value = entry.file;
  } catch (err) {
    setState(TEXT.loadFailed);
    showOverlay(TEXT.loadFailed, String(err.message || err));
    toast(String(err.message || err));
  }
}

/* ----------------------------------------------------------------- audio */

function ensureAudio() {
  if (!state.audio.ctx) {
    const Ctor = window.AudioContext || window.webkitAudioContext;
    if (!Ctor) {
      state.sound = false;
      dom.buttonSound.textContent = TEXT.soundUnavailable;
      return;
    }
    const audioCtx = new Ctor();
    const gain = audioCtx.createGain();
    gain.gain.value = 0.22;
    gain.connect(audioCtx.destination);
    state.audio.ctx = audioCtx;
    state.audio.gain = gain;
    state.audio.nextTime = 0;
  }
  if (state.audio.ctx.state === "suspended") state.audio.ctx.resume();
}

function pushSamples(samples) {
  const audio = state.audio;
  if (!audio.ctx || !state.sound || samples.length === 0) return;

  const buffer = audio.ctx.createBuffer(1, samples.length, 44100);
  const channel = buffer.getChannelData(0);
  for (let i = 0; i < samples.length; i++) channel[i] = samples[i] / 32768;

  const source = audio.ctx.createBufferSource();
  source.buffer = buffer;
  source.connect(audio.gain);
  source.onended = () => audio.sources.delete(source);
  audio.sources.add(source);

  const now = audio.ctx.currentTime;
  let start = Math.max(audio.nextTime, now + 0.04);
  if (start - now > 0.35) start = now + 0.04; /* drift / background tab */
  source.start(start);
  audio.nextTime = start + buffer.duration;
}

function stopAudio() {
  for (const source of state.audio.sources) {
    try { source.stop(); } catch { /* already stopped */ }
  }
  state.audio.sources.clear();
  state.audio.nextTime = 0;
}

function setSound(on) {
  state.sound = on;
  dom.buttonSound.textContent = on ? TEXT.soundOn : TEXT.soundOff;
  dom.buttonSound.setAttribute("aria-pressed", String(!on));
  dom.statAudio.textContent = on ? (state.audio.ctx ? TEXT.audioOn : TEXT.soundPending) : TEXT.muted;
  if (!on) stopAudio();
}

/* ----------------------------------------------------------------- input */

function currentButtons() {
  let bits = state.padBits | state.touchBits;
  for (const code of state.keys) bits |= KEYMAP[code] || 0;
  if (state.pulseFrames > 0) bits |= state.pulseMask;
  return bits & 0xff;
}

function pollGamepad() {
  if (!navigator.getGamepads) return;
  let bits = 0;
  for (const pad of navigator.getGamepads()) {
    if (!pad || !pad.connected) continue;
    const pressed = (i) => pad.buttons[i] && pad.buttons[i].pressed;
    if (pressed(0)) bits |= JOY.A;
    if (pressed(1)) bits |= JOY.B;
    if (pressed(8)) bits |= JOY.SELECT;
    if (pressed(9)) bits |= JOY.START;
    if (pressed(12) || pad.axes[1] < -0.45) bits |= JOY.UP;
    if (pressed(13) || pad.axes[1] > 0.45) bits |= JOY.DOWN;
    if (pressed(14) || pad.axes[0] < -0.45) bits |= JOY.LEFT;
    if (pressed(15) || pad.axes[0] > 0.45) bits |= JOY.RIGHT;
    break;
  }
  state.padBits = bits;
}

function beginPlay() {
  state.started = true;
  ensureAudio();
  setSound(state.sound);
  if (state.paused && !state.halted) togglePause();
  hideOverlay();
  state.pulseMask = JOY.START;
  state.pulseFrames = 8;
  focusEmulator();
}

function togglePause() {
  if (!state.loaded) return;
  if (state.halted) {
    reloadRom();
    return;
  }
  state.paused = !state.paused;
  dom.buttonPause.textContent = state.paused ? TEXT.resume : TEXT.pause;
  if (state.paused) {
    stopAudio();
    showOverlay(TEXT.paused, TEXT.pausedSub);
    setState(TEXT.paused);
  } else {
    hideOverlay();
    setState(TEXT.running);
    state.lastTs = 0;
  }
}

function reloadRom() {
  if (!state.lastRom) return;
  try {
    applyRom(state.lastRom, state.gameTitle);
    ensureAudio();
    state.started = true;
    hideOverlay();
  } catch (err) {
    toast(String(err.message || err));
  }
}

function handleKey(event, down) {
  const code = event.code;
  const tag = event.target && event.target.tagName;
  if (tag === "SELECT" || tag === "INPUT" || tag === "TEXTAREA") return;
  if (!emulatorFocused()) return;
  if (code === "KeyP" && down) { togglePause(); event.preventDefault(); return; }
  if (code === "KeyR" && down) { reloadRom(); event.preventDefault(); return; }
  if (code === "KeyM" && down) { setSound(!state.sound); event.preventDefault(); return; }
  if (!(code in KEYMAP)) return;

  event.preventDefault();
  if (!state.started && down) beginPlay();
  if (down) state.keys.add(code); else state.keys.delete(code);
}

/* ------------------------------------------------------------- main loop */

function stepFrame() {
  const wasm = state.wasm;
  wasm.gem_web_set_buttons(currentButtons(), 0);
  if (state.pulseFrames > 0) state.pulseFrames--;

  const produced = wasm.gem_web_run_frame();
  state.fpsFrames++;

  const count = wasm.gem_web_take_audio(8192);
  if (count > 0 && state.sound) {
    const mem = new Uint8Array(wasm.memory.buffer);
    const pcm = new Int16Array(mem.buffer, wasm.gem_web_audio_buffer(), count);
    pushSamples(pcm);
  }

  if (!produced && wasm.gem_web_halted()) {
    state.halted = true;
    state.paused = true;
    stopAudio();
    setState(TEXT.halted);
    showOverlay(TEXT.halted, TEXT.haltedSub);
  }
}

function drawFrame() {
  const wasm = state.wasm;
  const ptr = wasm.gem_web_framebuffer_rgba();
  const mem = new Uint8Array(wasm.memory.buffer);
  imageData.data.set(new Uint8ClampedArray(mem.buffer, ptr, RGBA_BYTES));
  ctx2d.putImageData(imageData, 0, 0);
}

function updateStats(ts) {
  if (ts - state.fpsSince >= 500) {
    const elapsed = ts - state.fpsSince;
    state.fps = state.fpsSince ? (state.fpsFrames * 1000) / elapsed : 0;
    state.fpsFrames = 0;
    state.fpsSince = ts;
    dom.statFps.textContent = state.fps.toFixed(1);
    dom.statFrames.textContent = String(state.wasm.gem_web_frames());
    dom.statCycles.textContent = Number(state.wasm.gem_web_cycles()).toLocaleString("en-US");
  }
}

function loop(ts) {
  state.rafId = requestAnimationFrame(loop);
  if (!state.loaded) return;

  if (!state.lastTs) {
    state.lastTs = ts;
    state.fpsSince = ts;
    return;
  }

  const delta = Math.min(ts - state.lastTs, 250);
  state.lastTs = ts;

  if (!state.paused) {
    state.frameAcc += delta;
    let steps = 0;
    pollGamepad();
    while (state.frameAcc >= FRAME_MS && steps < MAX_CATCHUP && !state.halted) {
      stepFrame();
      state.frameAcc -= FRAME_MS;
      steps++;
    }
    if (state.frameAcc > FRAME_MS * 4) state.frameAcc = 0;
    if (steps > 0) drawFrame();
  }

  updateStats(ts);
}

/* ------------------------------------------------------------------ boot */

function bindUi() {
  dom.select.addEventListener("change", () => {
    const entry = GAMES.find((game) => game.file === dom.select.value);
    if (entry) loadGame(entry);
    focusEmulator();
  });

  dom.buttonStart.addEventListener("click", beginPlay);
  dom.buttonPause.addEventListener("click", () => {
    togglePause();
    focusEmulator();
  });
  dom.buttonReset.addEventListener("click", () => { reloadRom(); beginPlay(); });
  dom.buttonSound.addEventListener("click", () => {
    ensureAudio();
    setSound(!state.sound);
    focusEmulator();
  });

  dom.overlay.addEventListener("click", beginPlay);

  document.addEventListener("click", (event) => {
    const link = event.target.closest('a[href="#emulator"]');
    if (!link) return;
    event.preventDefault();
    history.pushState(null, "", "#emulator");
    scrollToEmulator();
  });

  if (root) {
    root.addEventListener(
      "pointerdown",
      (event) => {
        if (event.target.closest("#screen-wrap")) focusEmulator();
      },
      true
    );
    root.addEventListener("focusout", () => state.keys.clear());
    if (root.dataset.autofocus === "true") {
      requestAnimationFrame(focusEmulator);
    }
  }

  dom.file.addEventListener("change", async () => {
    const file = dom.file.files && dom.file.files[0];
    if (!file) return;
    try {
      const bytes = new Uint8Array(await file.arrayBuffer());
      applyRom(bytes, file.name.replace(/\.nes$/i, ""));
      dom.select.value = "";
      focusEmulator();
    } catch (err) {
      toast(String(err.message || err));
    }
  });

  for (const button of document.querySelectorAll("#touch [data-bit]")) {
    const bit = Number(button.dataset.bit);
    const press = (event) => {
      event.preventDefault();
      if (!state.started) beginPlay();
      focusEmulator();
      state.touchBits |= bit;
      button.classList.add("active");
      if (button.setPointerCapture && event.pointerId !== undefined) {
        try { button.setPointerCapture(event.pointerId); } catch { /* ignore */ }
      }
    };
    const release = () => {
      state.touchBits &= ~bit;
      button.classList.remove("active");
    };
    button.addEventListener("pointerdown", press);
    button.addEventListener("pointerup", release);
    button.addEventListener("pointercancel", release);
    button.addEventListener("pointerleave", release);
  }

  window.addEventListener("keydown", (event) => handleKey(event, true));
  window.addEventListener("keyup", (event) => handleKey(event, false));
  window.addEventListener("blur", () => state.keys.clear());

  document.addEventListener("visibilitychange", () => {
    if (document.hidden && state.loaded && !state.paused) togglePause();
  });

  dom.screenWrap.addEventListener("dragover", (event) => {
    event.preventDefault();
    dom.screenWrap.classList.add("dragover");
  });
  dom.screenWrap.addEventListener("dragleave", () => dom.screenWrap.classList.remove("dragover"));
  dom.screenWrap.addEventListener("drop", async (event) => {
    event.preventDefault();
    dom.screenWrap.classList.remove("dragover");
    const file = event.dataTransfer.files && event.dataTransfer.files[0];
    if (!file) return;
    try {
      const bytes = new Uint8Array(await file.arrayBuffer());
      applyRom(bytes, file.name.replace(/\.nes$/i, ""));
      dom.select.value = "";
      focusEmulator();
    } catch (err) {
      toast(String(err.message || err));
    }
  });
}

async function boot() {
  for (const game of GAMES) {
    const option = document.createElement("option");
    option.value = game.file;
    option.textContent = `${game.title} · ${game.note}`;
    dom.select.append(option);
  }
  bindUi();
  showOverlay(TEXT.loading, TEXT.loadingSub);

  try {
    const instance = await instantiate();
    state.wasm = instance.exports;
    if (!state.wasm.gem_web_init()) throw new Error("gem_web_init 失败");
    state.wasm.gem_web_set_buttons(0, 0);
    setSound(true);
    await loadGame(GAMES[0]);
    if (location.hash === "#emulator") {
      requestAnimationFrame(() => requestAnimationFrame(scrollToEmulator));
    }
    state.rafId = requestAnimationFrame(loop);
  } catch (err) {
    console.error(err);
    setState(TEXT.initFailed);
    showOverlay(TEXT.initFailed, String(err.message || err));
    toast(String(err.message || err));
  }
}

boot();
