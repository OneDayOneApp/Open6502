/*
 * Browser player for the static catalog (GitHub Pages demo).
 *
 * One player, three input models:
 *   - keyboard (laptop) and gamepads;
 *   - an 8-way thumb pad + A/B/Start/Select buttons on touch screens;
 *   - the on-screen overlay for start / resume / restart.
 *
 * It reuses the same WebAssembly core the site ships for GEM6502, so anything
 * the catalog offers can be tried in the browser without installing nesearch.
 * ROM bytes are fetched from the catalog's download URL (raw GitHub, PDRoms,
 * archive.org); cross-origin failures are reported with a direct link.
 */
"use strict";

const WIDTH = 256;
const HEIGHT = 240;
const RGBA_BYTES = WIDTH * HEIGHT * 4;
const FRAME_MS = 1000 / 60.0988;
const MAX_CATCHUP = 2;
const BASE = "../assets/play/";
const SAMPLE_RATE = 44100;

/* Save states: one rolling slot per game.  The state itself lives in the
   wasm core (gem_web_save_state); the browser keeps the latest copy in memory
   and mirrors it to localStorage so a refresh does not lose it. */
const AUTOSAVE_FRAMES = 600;   /* ~10 s at 60.0988 fps */
const PERSIST_MS = 30000;      /* localStorage write cadence */
const SAVE_PREFIX = "nesearch:save:v1:";

const JOY = { A: 0x01, B: 0x02, SELECT: 0x04, START: 0x08, UP: 0x10, DOWN: 0x20, LEFT: 0x40, RIGHT: 0x80 };
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

const params = new URLSearchParams(location.search);
const romUrl = params.get("rom") || "";
const title = params.get("title") || "ROM";
const gameId = params.get("id") || "";

const dom = {
  app: document.getElementById("app"),
  stage: document.getElementById("stage"),
  stageArea: document.getElementById("stageArea"),
  canvas: document.getElementById("screen"),
  overlay: document.getElementById("overlay"),
  overlayKicker: document.getElementById("overlayKicker"),
  overlayTitle: document.getElementById("overlayTitle"),
  overlayText: document.getElementById("overlayText"),
  overlayHint: document.getElementById("overlayHint"),
  startBtn: document.getElementById("startBtn"),
  overlaySaveBtn: document.getElementById("overlaySaveBtn"),
  overlayLoadBtn: document.getElementById("overlayLoadBtn"),
  overlayState: document.getElementById("overlayState"),
  gameName: document.getElementById("gameName"),
  pauseBtn: document.getElementById("pauseBtn"),
  saveBtn: document.getElementById("saveBtn"),
  loadBtn: document.getElementById("loadBtn"),
  padSaveBtn: document.getElementById("padSaveBtn"),
  padLoadBtn: document.getElementById("padLoadBtn"),
  reloadBtn: document.getElementById("reloadBtn"),
  rotateBtn: document.getElementById("rotateBtn"),
  soundBtn: document.getElementById("soundBtn"),
  fullBtn: document.getElementById("fullBtn"),
  stats: document.getElementById("stats"),
  downloadLink: document.getElementById("downloadLink"),
  shareBtn: document.getElementById("shareBtn"),
  shareBar: document.getElementById("shareBar"),
  toast: document.getElementById("toast"),
};

const ctx = dom.canvas.getContext("2d", { alpha: false });
const image = ctx.createImageData(WIDTH, HEIGHT);

const state = {
  wasm: null,
  romBytes: null,
  running: false,
  paused: false,
  halted: false,
  started: false,
  sound: true,
  frames: 0,
  fpsFrames: 0,
  fpsAt: 0,
  fps: 0,
  keys: new Set(),
  touchBits: new Set(),
  dirBits: 0,
  padBits: 0,
  pulseMask: 0,
  pulseFrames: 0,
  lastTs: 0,
  frameAcc: 0,
  rafId: 0,
  slot: null,      /* manual checkpoint, only written by Save */
  autoSlot: null,  /* rolling checkpoint, written every 10 s and on pause */
  lastAutoSave: 0,
  lastPersist: 0,
  landscapeLock: false,
  landscapeFullscreen: false,
  inputLock: false,
  audio: { ctx: null, gain: null, nextTime: 0, sources: new Set() },
};

const coarse = window.matchMedia ? window.matchMedia("(pointer: coarse)") : null;

/* ------------------------------------------------------------------ shell */

let toastTimer = 0;
function toast(message) {
  dom.toast.textContent = message;
  dom.toast.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => dom.toast.classList.remove("show"), 2600);
}

function setTouchMode(isTouch) {
  document.body.classList.toggle("touch", isTouch);
  if (dom.shareBar) dom.shareBar.hidden = isTouch || dom.shareBar.dataset.ready !== "1";
  if (dom.shareBtn) dom.shareBtn.hidden = !isTouch || dom.shareBtn.dataset.ready !== "1";
}

function detectTouch() {
  const isTouch = Boolean(
    (coarse && coarse.matches) ||
    (navigator.maxTouchPoints > 0 && !(window.matchMedia && window.matchMedia("(pointer: fine)").matches))
  );
  setTouchMode(isTouch);
}

/*
 * The overlay is the only chrome the player really needs: loading, ready,
 * paused, crashed. `action` decides what the primary button does and whether
 * tapping anywhere on the screen triggers it.
 */
let overlayAction = null;

function showOverlay(options) {
  const opts = options || {};
  dom.overlayKicker.textContent = opts.kicker || "nesearch";
  dom.overlayTitle.textContent = opts.heading || "";
  dom.overlayText.textContent = opts.text || "";
  dom.overlayText.hidden = !opts.text;
  dom.overlayHint.textContent = opts.hint || "";
  dom.overlayHint.hidden = !opts.hint;
  overlayAction = opts.action || null;
  dom.startBtn.hidden = !overlayAction;
  dom.startBtn.disabled = !overlayAction;
  dom.startBtn.textContent = overlayAction === "resume" ? "Resume" : overlayAction === "restart" ? "Restart" : "Start";
  dom.overlay.dataset.action = overlayAction || "none";
  const canSave = Boolean(opts.save) && Boolean(state.wasm) && Boolean(state.romBytes) &&
    Boolean(stateApi()) && !state.halted;
  const canLoad = Boolean(opts.load) && hasAnySlot() && Boolean(stateApi());
  dom.overlaySaveBtn.hidden = !canSave;
  dom.overlayLoadBtn.hidden = !canLoad;
  updateOverlayState();
  dom.overlay.style.userSelect = opts.selectable ? "text" : "";
  dom.overlay.style.webkitUserSelect = opts.selectable ? "text" : "";
  dom.overlay.hidden = false;
}

function hideOverlay() {
  dom.overlay.hidden = true;
  overlayAction = null;
}

function runOverlayAction() {
  if (overlayAction === "restart") {
    reloadRom();
    return;
  }
  if (overlayAction === "start" || overlayAction === "resume") beginPlay(overlayAction === "start");
}

function updateFullButton() {
  const on = Boolean(document.fullscreenElement || document.webkitFullscreenElement) ||
    document.body.classList.contains("immersive");
  dom.fullBtn.dataset.mode = on ? "exit" : "enter";
  const label = on ? "Exit fullscreen" : "Enter fullscreen";
  dom.fullBtn.setAttribute("aria-label", label);
  dom.fullBtn.title = on ? "Exit fullscreen (F)" : "Fullscreen (F)";
}

function fitStage() {
  const area = dom.stageArea;
  if (!area) return;
  const availW = area.clientWidth;
  const availH = area.clientHeight;
  if (!availW || !availH) return;

  const raw = Math.min(availW / WIDTH, availH / HEIGHT);
  /* Integer scaling keeps pixels square; only take it when it barely wastes
     room (a 6% loss is invisible, a 30% loss is not). */
  const whole = Math.floor(raw);
  const scale = whole >= 1 && whole / raw >= 0.94 ? whole : raw;
  const width = Math.max(1, Math.round(WIDTH * scale));
  const height = Math.max(1, Math.round(HEIGHT * scale));
  dom.stage.style.width = `${width}px`;
  dom.stage.style.height = `${height}px`;

  /* Landscape mode parks the thumb pad in the margins beside the screen. */
  const side = Math.max(0, Math.round((availW - width) / 2));
  document.documentElement.style.setProperty("--side", `${side}px`);
}

function scheduleFit() {
  requestAnimationFrame(fitStage);
}

/* ------------------------------------------------------------------ audio */

function ensureAudio() {
  if (!state.audio.ctx) {
    const Ctor = window.AudioContext || window.webkitAudioContext;
    if (!Ctor) {
      state.sound = false;
      dom.soundBtn.disabled = true;
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

  const buffer = audio.ctx.createBuffer(1, samples.length, SAMPLE_RATE);
  const channel = buffer.getChannelData(0);
  for (let i = 0; i < samples.length; i += 1) channel[i] = samples[i] / 32768;

  const source = audio.ctx.createBufferSource();
  source.buffer = buffer;
  source.connect(audio.gain);
  source.onended = () => audio.sources.delete(source);
  audio.sources.add(source);

  const now = audio.ctx.currentTime;
  let start = Math.max(audio.nextTime, now + 0.04);
  if (start - now > 0.35) start = now + 0.04;
  source.start(start);
  audio.nextTime = start + buffer.duration;
}

function stopAudio() {
  for (const source of state.audio.sources) {
    try { source.stop(); } catch (error) { /* already stopped */ }
  }
  state.audio.sources.clear();
  state.audio.nextTime = 0;
}

function setSound(on) {
  state.sound = on;
  dom.soundBtn.dataset.mode = on ? "on" : "off";
  dom.soundBtn.setAttribute("aria-pressed", String(on));
  dom.soundBtn.setAttribute("aria-label", on ? "Mute sound" : "Unmute sound");
  dom.soundBtn.title = on ? "Sound on (M)" : "Sound off (M)";
  if (!on) stopAudio();
}

/* -------------------------------------------------------------- emulation */

function readString(pointer) {
  const memory = new Uint8Array(state.wasm.memory.buffer);
  let end = pointer;
  while (end < memory.length && memory[end] !== 0) end += 1;
  return new TextDecoder().decode(memory.subarray(pointer, end));
}

function loadRomBytes(bytes) {
  const wasm = state.wasm;
  const capacity = wasm.gem_web_rom_capacity();
  if (bytes.length > capacity) throw new Error(`ROM is too large: ${bytes.length} bytes (limit ${capacity})`);
  if (bytes.length > 0) {
    new Uint8Array(wasm.memory.buffer).set(bytes, wasm.gem_web_rom_buffer());
  }
  const rc = wasm.gem_web_load_rom(wasm.gem_web_rom_buffer(), bytes.length);
  if (rc !== 0) throw new Error(readString(wasm.gem_web_error()));
  state.halted = false;
}

function drawFrame() {
  const wasm = state.wasm;
  const pointer = wasm.gem_web_framebuffer_rgba();
  const source = new Uint8ClampedArray(wasm.memory.buffer, pointer, RGBA_BYTES);
  image.data.set(source);
  ctx.putImageData(image, 0, 0);
}

function touchBits() {
  let bits = 0;
  for (const bit of state.touchBits) bits |= bit;
  return bits;
}

function currentButtons() {
  let bits = state.padBits | state.dirBits | touchBits();
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

function stepFrame() {
  const wasm = state.wasm;
  pollGamepad();
  wasm.gem_web_set_buttons(currentButtons(), 0);
  if (state.pulseFrames > 0) state.pulseFrames -= 1;
  const produced = wasm.gem_web_run_frame();
  const count = wasm.gem_web_take_audio(8192);
  if (count > 0 && state.sound) {
    const memory = new Uint8Array(wasm.memory.buffer);
    const pcm = new Int16Array(memory.buffer, wasm.gem_web_audio_buffer(), count);
    pushSamples(pcm);
  }
  state.frames += 1;
  if (!produced && wasm.gem_web_halted()) {
    state.halted = true;
    state.running = false;
    stopAudio();
    dom.pauseBtn.disabled = true;
    dom.pauseBtn.dataset.mode = "play";
    updateStateButtons();
    showOverlay({
      heading: "CPU halted",
      text: title,
      hint: "The game stopped itself. Restart, or return to the last saved state.",
      action: "restart",
      load: true,
    });
    return false;
  }
  return true;
}

function updateStats() {
  const wasm = state.wasm;
  if (!wasm) return;
  const frames = Number(wasm.gem_web_frames()).toLocaleString("en-US");
  const fps = state.fps > 0 ? `${state.fps.toFixed(0)} fps · ` : "";
  dom.stats.textContent = `${fps}${frames} frames`;
}

function loop(timestamp) {
  if (!state.running) return;
  if (!state.lastTs) {
    state.lastTs = timestamp;
    state.fpsAt = timestamp;
    state.rafId = requestAnimationFrame(loop);
    return;
  }
  const elapsed = timestamp - state.lastTs;
  state.lastTs = timestamp;
  state.frameAcc = Math.min(state.frameAcc + elapsed, FRAME_MS * MAX_CATCHUP * 2);

  let budget = MAX_CATCHUP;
  while (state.frameAcc >= FRAME_MS && budget > 0) {
    state.frameAcc -= FRAME_MS;
    budget -= 1;
    if (!stepFrame()) {
      drawFrame();
      return;
    }
  }
  if (state.frameAcc > FRAME_MS * MAX_CATCHUP) state.frameAcc = 0;
  drawFrame();

  /* Rolling checkpoint: cheap in memory, mirrored to localStorage at most
     every PERSIST_MS so a refresh keeps the latest one. */
  if (state.started && state.frames - state.lastAutoSave >= AUTOSAVE_FRAMES) {
    const persist = Date.now() - state.lastPersist >= PERSIST_MS;
    if (captureIntoSlot({ persist, auto: true })) {
      if (persist) state.lastPersist = Date.now();
    }
  }

  state.fpsFrames += 1;
  if (timestamp - state.fpsAt >= 500) {
    const span = timestamp - state.fpsAt;
    if (span > 0) state.fps = (state.fpsFrames * 1000) / span;
    state.fpsFrames = 0;
    state.fpsAt = timestamp;
    updateStats();
  }
  state.rafId = requestAnimationFrame(loop);
}

/* ------------------------------------------------------------- wake lock */

let wakeLock = null;

function acquireWakeLock() {
  if (!("wakeLock" in navigator) || document.visibilityState !== "visible" || wakeLock) return;
  try {
    navigator.wakeLock.request("screen").then(
      (lock) => {
        wakeLock = lock;
        lock.addEventListener("release", () => { wakeLock = null; });
      },
      () => { wakeLock = null; }
    );
  } catch (error) {
    wakeLock = null;
  }
}

function releaseWakeLock() {
  if (!wakeLock) return;
  try { wakeLock.release(); } catch (error) { /* already released */ }
  wakeLock = null;
}

/* ---------------------------------------------------------------- player */

function startRunning() {
  if (state.running || state.halted) return;
  state.running = true;
  state.paused = false;
  state.lastTs = 0;
  state.frameAcc = 0;
  state.fpsFrames = 0;
  state.fpsAt = 0;
  dom.pauseBtn.disabled = false;
  dom.pauseBtn.dataset.mode = "pause";
  dom.pauseBtn.setAttribute("aria-label", "Pause");
  updateStateButtons();
  state.rafId = requestAnimationFrame(loop);
}

function pauseRunning(showCard) {
  if (!state.running) return;
  state.running = false;
  state.paused = true;
  cancelAnimationFrame(state.rafId);
  stopAudio();
  state.keys.clear();
  state.touchBits.clear();
  state.dirBits = 0;
  state.padBits = 0;
  dom.pauseBtn.dataset.mode = "play";
  dom.pauseBtn.setAttribute("aria-label", "Resume");
  releaseWakeLock();
  if (showCard !== false) {
    /* Pausing is a natural checkpoint: keep the exact frame we stopped on
       in the rolling slot (a deliberate Save stays untouched). */
    captureIntoSlot({ persist: true, auto: true });
    showOverlay({
      heading: "Paused",
      text: title,
      hint: document.body.classList.contains("touch") ? "Tap Resume to keep playing." : "Press P or click Resume to keep playing.",
      action: "resume",
      save: true,
      load: true,
    });
  }
}

function touchStartHint() {
  if (!document.body.classList.contains("touch")) {
    return "Click Start or press any key. Audio needs one user gesture.";
  }
  return window.matchMedia && window.matchMedia("(orientation: portrait)").matches
    ? "Tap Start or the screen. Rotate for a bigger picture · audio needs one tap."
    : "Tap Start or the screen. Audio needs one tap.";
}

function beginPlay(fresh) {
  if (!state.wasm) return;
  if (state.halted) {
    reloadRom();
    return;
  }
  ensureAudio();
  state.started = true;
  state.lastAutoSave = state.frames;
  hideOverlay();
  /* One synthetic START press gets past most title screens, so a thumb-only
     player is never stuck on "press start". */
  if (fresh) {
    state.pulseMask = JOY.START;
    state.pulseFrames = 8;
  }
  startRunning();
  acquireWakeLock();
}

function togglePause() {
  if (!state.wasm || state.halted) return;
  if (state.running) {
    pauseRunning(true);
  } else if (state.started) {
    hideOverlay();
    startRunning();
    acquireWakeLock();
  } else {
    beginPlay(true);
  }
}

function reloadRom() {
  if (!state.wasm || !state.romBytes) return;
  cancelAnimationFrame(state.rafId);
  state.running = false;
  stopAudio();
  try {
    loadRomBytes(state.romBytes);
  } catch (error) {
    showOverlay({
      heading: "Cannot load",
      text: String((error && error.message) || error),
      hint: "This ROM may use a mapper GEM6502 does not support.",
    });
    return;
  }
  state.halted = false;
  state.started = true;
  state.lastAutoSave = state.frames;
  dom.pauseBtn.disabled = false;
  hideOverlay();
  startRunning();
  state.pulseMask = JOY.START;
  state.pulseFrames = 8;
  updateStateButtons();
  acquireWakeLock();
}

async function loadRom() {
  if (!romUrl) {
    showOverlay({
      heading: "No ROM",
      text: "The ?rom= parameter is missing.",
      hint: "Pick a game from the catalog to play it here.",
    });
    return false;
  }
  showOverlay({ heading: "Loading", text: "Reading the ROM", hint: romUrl.length > 90 ? `${romUrl.slice(0, 88)}…` : romUrl });
  try {
    const response = await fetch(romUrl);
    if (!response.ok) throw new Error(`Cannot read the ROM (HTTP ${response.status})`);
    state.romBytes = new Uint8Array(await response.arrayBuffer());
  } catch (error) {
    const message = String((error && error.message) || error);
    showOverlay({
      heading: "Load failed",
      text: message,
      hint: "If this is a cross-origin block, use Download ROM and play it locally.",
      selectable: true,
    });
    return false;
  }
  try {
    loadRomBytes(state.romBytes);
  } catch (error) {
    showOverlay({
      heading: "Cannot load",
      text: String((error && error.message) || error),
      hint: "This ROM may use a mapper GEM6502 does not support.",
      selectable: true,
    });
    return false;
  }
  // Warm up a few frames so the paused card shows the title screen.
  for (let i = 0; i < 30; i += 1) {
    if (!state.wasm.gem_web_run_frame()) break;
  }
  drawFrame();
  state.paused = true;
  state.started = false;
  dom.reloadBtn.disabled = false;
  dom.pauseBtn.disabled = true;
  dom.pauseBtn.dataset.mode = "play";
  dom.pauseBtn.setAttribute("aria-label", "Play");
  showOverlay({
    heading: title,
    text: "",
    hint: touchStartHint(),
    action: "start",
    load: true,
  });
  updateStats();
  updateStateButtons();
  return true;
}

async function boot() {
  document.title = `${title} — nesearch`;
  dom.gameName.textContent = title;
  dom.gameName.title = title;
  if (romUrl) {
    dom.downloadLink.href = romUrl;
    dom.downloadLink.hidden = false;
  }
  state.slot = readSlot("manual");
  state.autoSlot = readSlot("auto");
  updateStateButtons();
  updateRotateButton();

  try {
    const response = await fetch(`${BASE}gem6502.wasm`);
    if (!response.ok) throw new Error(`Cannot read gem6502.wasm (HTTP ${response.status})`);
    const instance = WebAssembly.instantiateStreaming
      ? await WebAssembly.instantiateStreaming(response.clone(), {}).catch(async () => {
          const bytes = await response.arrayBuffer();
          return WebAssembly.instantiate(bytes, {});
        })
      : await WebAssembly.instantiate(await response.arrayBuffer(), {});
    state.wasm = instance.instance.exports;
    if (!state.wasm.gem_web_init()) throw new Error("gem_web_init failed");
    state.wasm.gem_web_set_buttons(0, 0);
    state.sound = true;
    dom.soundBtn.disabled = false;
    updateStateButtons();
    await loadRom();
  } catch (error) {
    const message = String((error && error.message) || error);
    showOverlay({
      heading: "Initialisation failed",
      text: message,
      hint: "Check your network connection and reload the page.",
      selectable: true,
    });
    toast(message);
  }
}

/* -------------------------------------------------------------- fullscreen */

function fullscreenElement() {
  return document.fullscreenElement || document.webkitFullscreenElement || null;
}

function toggleImmersive() {
  const on = !document.body.classList.contains("immersive");
  document.body.classList.toggle("immersive", on);
  updateFullButton();
  setInputLock(on);
  scheduleFit();
}

function toggleFullscreen() {
  if (fullscreenElement()) {
    const exit = document.exitFullscreen || document.webkitExitFullscreen;
    if (exit) exit.call(document);
    if (document.body.classList.contains("immersive")) document.body.classList.remove("immersive");
    updateFullButton();
    scheduleFit();
    return;
  }
  const target = document.documentElement;
  const request = target.requestFullscreen || target.webkitRequestFullscreen;
  if (!request) {
    toggleImmersive();
    return;
  }
  Promise.resolve(request.call(target))
    .then(() => {
      /* Keep this inside the click's activation window: Chrome only grants
         keyboard lock right after entering fullscreen. */
      requestKeyboardLock();
      setInputLock(true);
    })
    .catch(() => toggleImmersive());
}

/* -------------------------------------------------------------- states */

/* The core exposes gem_web_state_*; older builds simply lack them. */
function stateApi() {
  const wasm = state.wasm;
  if (!wasm) return null;
  return wasm.gem_web_state_buffer && wasm.gem_web_state_size &&
    wasm.gem_web_save_state && wasm.gem_web_load_state ? wasm : null;
}

function hashText(text) {
  let hash = 5381;
  for (let i = 0; i < text.length; i += 1) hash = ((hash << 5) + hash + text.charCodeAt(i)) >>> 0;
  return hash.toString(36);
}

function slotKey(kind) {
  const base = gameId ? SAVE_PREFIX + gameId : romUrl ? SAVE_PREFIX + "h" + hashText(romUrl) : "";
  if (!base) return "";
  return kind === "auto" ? `${base}:auto` : base;
}

function bytesToBase64(bytes) {
  let binary = "";
  const chunk = 0x8000;
  for (let i = 0; i < bytes.length; i += chunk) {
    binary += String.fromCharCode.apply(null, bytes.subarray(i, i + chunk));
  }
  return btoa(binary);
}

function base64ToBytes(text) {
  const binary = atob(text);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) bytes[i] = binary.charCodeAt(i);
  return bytes;
}

function readSlot(kind) {
  const key = slotKey(kind);
  if (!key) return null;
  try {
    const raw = localStorage.getItem(key);
    if (!raw) return null;
    const record = JSON.parse(raw);
    if (!record || record.v !== 1 || typeof record.data !== "string") return null;
    const bytes = base64ToBytes(record.data);
    if (!bytes.length) return null;
    return { bytes, at: Number(record.at) || 0, frames: Number(record.frames) || 0 };
  } catch (error) {
    return null;
  }
}

function persistSlot(slot, kind) {
  const key = slotKey(kind);
  if (!key || !slot) return false;
  const record = JSON.stringify({
    v: 1,
    at: slot.at,
    frames: slot.frames,
    data: bytesToBase64(slot.bytes),
  });
  try {
    localStorage.setItem(key, record);
    return true;
  } catch (error) {
    /* Quota: drop the other games' slots and retry once. */
    try {
      for (let i = localStorage.length - 1; i >= 0; i -= 1) {
        const other = localStorage.key(i);
        if (other && other.indexOf(SAVE_PREFIX) === 0 && other !== key) localStorage.removeItem(other);
      }
      localStorage.setItem(key, record);
      return true;
    } catch (fallbackError) {
      return false;
    }
  }
}

function captureState() {
  const wasm = stateApi();
  if (!wasm) return null;
  const size = wasm.gem_web_save_state();
  if (!size || size < 0) return null;
  const pointer = wasm.gem_web_state_buffer();
  return new Uint8Array(wasm.memory.buffer, pointer, size).slice();
}

function restoreState(bytes) {
  const wasm = stateApi();
  if (!wasm || !bytes || !bytes.length) return false;
  if (bytes.length > wasm.gem_web_state_size()) return false;
  const pointer = wasm.gem_web_state_buffer();
  new Uint8Array(wasm.memory.buffer, pointer, bytes.length).set(bytes);
  if (wasm.gem_web_load_state(bytes.length) !== 0) return false;
  stopAudio();
  state.halted = false;
  state.paused = false;
  /* The pixel buffer is not part of the record: render one frame so the
     restored screen is visible right away. */
  wasm.gem_web_run_frame();
  drawFrame();
  return true;
}

function formatAge(at) {
  if (!at) return "just now";
  const seconds = Math.max(0, Math.round((Date.now() - at) / 1000));
  if (seconds < 5) return "just now";
  if (seconds < 60) return `${seconds}s ago`;
  const minutes = Math.round(seconds / 60);
  if (minutes < 60) return `${minutes}m ago`;
  return `${Math.round(minutes / 60)}h ago`;
}

function preferredSlot() {
  if (!state.started) {
    return state.autoSlot ? { kind: "auto", slot: state.autoSlot } : { kind: "checkpoint", slot: state.slot };
  }
  return state.slot ? { kind: "checkpoint", slot: state.slot } : { kind: "auto", slot: state.autoSlot };
}

function hasAnySlot() {
  return Boolean(state.slot || state.autoSlot);
}

function updateOverlayState() {
  if (!dom.overlayState) return;
  const available = Boolean(state.wasm) && Boolean(state.romBytes) && Boolean(stateApi());
  dom.overlayState.hidden = !available;
  if (!available) return;
  const parts = [];
  if (state.slot) parts.push(`checkpoint ${formatAge(state.slot.at)}`);
  if (state.autoSlot) parts.push(`auto ${formatAge(state.autoSlot.at)}`);
  dom.overlayState.textContent = parts.length
    ? `${parts.join(" · ")} · auto-saves every 10 s`
    : "No state yet · auto-saves every 10 s while playing";
}

function updateStateButtons() {
  const loaded = Boolean(state.wasm) && Boolean(state.romBytes) && Boolean(stateApi());
  if (dom.saveBtn) {
    dom.saveBtn.disabled = !loaded || state.halted || !state.started;
    dom.saveBtn.title = state.halted ? "Cannot save after a crash" : "Save state (F2)";
  }
  if (dom.loadBtn) {
    const target = preferredSlot();
    dom.loadBtn.disabled = !loaded || !target.slot;
    dom.loadBtn.title = target.slot
      ? `Load ${target.kind === "auto" ? "last session" : "checkpoint"} (F3) · ${formatAge(target.slot.at)}`
      : "No saved state yet";
  }
  if (dom.overlaySaveBtn) dom.overlaySaveBtn.disabled = !loaded || state.halted || !state.started;
  if (dom.overlayLoadBtn) dom.overlayLoadBtn.disabled = !hasAnySlot();
  if (dom.padSaveBtn) {
    dom.padSaveBtn.disabled = !loaded || state.halted || !state.started;
    dom.padSaveBtn.title = state.halted ? "Cannot save after a crash" : "Save checkpoint";
  }
  if (dom.padLoadBtn) {
    dom.padLoadBtn.disabled = !loaded || !hasAnySlot();
    dom.padLoadBtn.title = hasAnySlot() ? "Load saved state" : "No saved state yet";
  }
  if (dom.rotateBtn) updateRotateButton();
}

function captureIntoSlot(options) {
  const opts = options || {};
  if (!state.wasm || !state.romBytes || state.halted || !stateApi()) return false;
  const bytes = captureState();
  if (!bytes) return false;
  const slot = { bytes, at: Date.now(), frames: state.frames };
  if (opts.auto) state.autoSlot = slot;
  else state.slot = slot;
  state.lastAutoSave = state.frames;
  if (opts.persist) persistSlot(slot, opts.auto ? "auto" : "manual");
  updateStateButtons();
  updateOverlayState();
  return true;
}

function saveNow(manual) {
  if (!state.wasm || !state.romBytes) {
    if (manual) toast("The game is not loaded yet");
    return false;
  }
  if (state.halted) {
    if (manual) toast("The CPU halted — restart or load the last state");
    return false;
  }
  if (!state.started) {
    if (manual) toast("Start the game before saving");
    return false;
  }
  if (!captureIntoSlot({ persist: true })) {
    if (manual) toast("Save states are not available in this build");
    return false;
  }
  state.lastPersist = Date.now();
  if (manual) toast("Checkpoint saved");
  return true;
}

function loadNow(manual, prefer) {
  if (!state.wasm || !state.romBytes) {
    if (manual) toast("The game is not loaded yet");
    return false;
  }
  if (!state.slot) state.slot = readSlot("manual");
  if (!state.autoSlot) state.autoSlot = readSlot("auto");
  const target = prefer === "auto"
    ? (state.autoSlot ? { kind: "auto", slot: state.autoSlot } : { kind: "checkpoint", slot: state.slot })
    : preferredSlot();
  if (!target.slot) {
    if (manual) toast("No saved state yet");
    return false;
  }
  ensureAudio();
  if (!restoreState(target.slot.bytes)) {
    if (manual) toast("This state could not be restored");
    return false;
  }
  state.started = true;
  state.lastAutoSave = state.frames;
  hideOverlay();
  if (!state.running) startRunning();
  else {
    state.lastTs = 0;
    state.frameAcc = 0;
  }
  updateStateButtons();
  acquireWakeLock();
  if (manual) toast(`Loaded ${target.kind === "auto" ? "last session" : "checkpoint"} · ${formatAge(target.slot.at)}`);
  return true;
}

/* -------------------------------------------------------------- rotation */

function updateRotateButton() {
  if (!dom.rotateBtn) return;
  dom.rotateBtn.setAttribute("aria-pressed", String(state.landscapeLock));
  const label = state.landscapeLock ? "Unlock rotation" : "Rotate to landscape";
  dom.rotateBtn.setAttribute("aria-label", label);
  dom.rotateBtn.title = state.landscapeLock ? "Unlock rotation (L)" : "Rotate to landscape (L)";
}

function exitFullscreen() {
  const exit = document.exitFullscreen || document.webkitExitFullscreen;
  if (!exit) return;
  try { exit.call(document); } catch (error) { /* already out */ }
}

function syncLandscapeState() {
  if (!fullscreenElement() && state.landscapeLock) {
    state.landscapeLock = false;
    state.landscapeFullscreen = false;
    updateRotateButton();
  }
}

/*
 * Android Chrome: fullscreen + screen.orientation.lock() really rotates the
 * page.  iOS Safari and desktop cannot lock orientation, so the button
 * explains what to do instead of silently doing nothing.
 */
function toggleLandscape() {
  const orientation = window.screen && (screen.orientation || screen.mozOrientation || screen.msOrientation);
  if (state.landscapeLock) {
    state.landscapeLock = false;
    try { if (orientation && orientation.unlock) orientation.unlock(); } catch (error) { /* unsupported */ }
    if (state.landscapeFullscreen && fullscreenElement()) exitFullscreen();
    state.landscapeFullscreen = false;
    updateRotateButton();
    scheduleFit();
    toast("Rotation unlocked");
    return;
  }
  const target = document.documentElement;
  const request = target.requestFullscreen || target.webkitRequestFullscreen;
  if (!request) {
    toast("Rotate the phone to landscape — this browser cannot lock it.");
    return;
  }
  const alreadyFullscreen = Boolean(fullscreenElement());
  Promise.resolve(request.call(target))
    .then(() => {
      if (!orientation || typeof orientation.lock !== "function") {
        throw new Error("orientation lock unsupported");
      }
      return orientation.lock("landscape");
    })
    .then(() => {
      state.landscapeLock = true;
      state.landscapeFullscreen = !alreadyFullscreen;
      updateRotateButton();
      scheduleFit();
      toast("Landscape locked");
    })
    .catch(() => {
      /* Do not leave the page in fullscreen when the rotation did not happen. */
      if (!alreadyFullscreen && fullscreenElement()) exitFullscreen();
      const landscape = window.matchMedia && window.matchMedia("(orientation: landscape)").matches;
      const touch = document.body.classList.contains("touch");
      toast(landscape ? "Already in landscape"
        : touch ? "Rotate the phone to landscape — this browser cannot lock it."
        : "Rotation lock is only available on phones.");
      scheduleFit();
    });
}

/* ---------------------------------------------------- immersive fullscreen */
/*
 * While the page is fullscreen (or in the iOS immersive fallback) the browser
 * should only see game input.  We can stop page-level defaults and, where the
 * Keyboard Lock API exists (Chrome/Edge, fullscreen only), hand browser keys
 * to the page.  OS-level shortcuts (Alt+Tab, Cmd+Q, Ctrl+Alt+Del) and the
 * browser's own fullscreen exit cannot be taken away from the user — pressing
 * Esc still leaves, which is the intended way out.
 */

/* Alphanumerics are included so browser shortcuts such as Ctrl+W/R/T/N/P/S
   are delivered to the page on engines that honour the lock; Escape stays
   free because it is the documented way out of fullscreen. */
const KEYBOARD_LOCK_CODES = [
  "F1", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
  "KeyA", "KeyB", "KeyC", "KeyD", "KeyE", "KeyF", "KeyG", "KeyH", "KeyI",
  "KeyJ", "KeyK", "KeyL", "KeyM", "KeyN", "KeyO", "KeyP", "KeyQ", "KeyR",
  "KeyS", "KeyT", "KeyU", "KeyV", "KeyW", "KeyX", "KeyY", "KeyZ",
  "Digit0", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6",
  "Digit7", "Digit8", "Digit9",
  "BrowserBack", "BrowserForward", "BrowserRefresh", "BrowserStop",
  "BrowserSearch", "BrowserFavorites", "BrowserHome",
  "LaunchMail", "MediaPlayPause", "MediaStop", "MediaTrackNext", "MediaTrackPrevious",
  "ContextMenu", "PrintScreen",
];

function requestKeyboardLock() {
  if (!navigator.keyboard || typeof navigator.keyboard.lock !== "function") return;
  try {
    const request = navigator.keyboard.lock(KEYBOARD_LOCK_CODES);
    if (request && typeof request.catch === "function") request.catch(() => {});
  } catch (error) {
    /* key codes unsupported in this browser; the keydown guard still applies */
  }
}

function releaseKeyboardLock() {
  if (!navigator.keyboard || typeof navigator.keyboard.unlock !== "function") return;
  try { navigator.keyboard.unlock(); } catch (error) { /* no lock held */ }
}

function immersiveInputActive() {
  return Boolean(fullscreenElement()) || document.body.classList.contains("immersive");
}

/* Keys the browser would otherwise act on; game keys (including F2/F3) pass. */
function isBrowserKey(event) {
  if (event.ctrlKey || event.metaKey || event.altKey) return true;
  const code = event.code || "";
  if (/^F\d{1,2}$/.test(code)) return code !== "F2" && code !== "F3";
  if (code === "Backspace" || code === "PageUp" || code === "PageDown" ||
      code === "Home" || code === "End") return true;
  return code.indexOf("Browser") === 0 || code.indexOf("Launch") === 0 ||
    code.indexOf("Media") === 0;
}

function setInputLock(on) {
  if (on === state.inputLock) return;
  state.inputLock = on;
  document.body.classList.toggle("input-lock", on);
  if (on) {
    requestKeyboardLock();
    const touch = document.body.classList.contains("touch");
    toast(fullscreenElement()
      ? "Immersive mode · shortcuts blocked · Esc exits"
      : touch ? "Immersive mode · shortcuts blocked · fullscreen button exits"
        : "Immersive mode · shortcuts blocked · Esc exits");
  } else {
    releaseKeyboardLock();
    toast("Fullscreen exited · shortcuts restored");
  }
}

function bindImmersiveGuards() {
  document.addEventListener("keydown", (event) => {
    if (!state.inputLock || !isBrowserKey(event)) return;
    event.preventDefault();
    event.stopImmediatePropagation();
  }, true);

  document.addEventListener("contextmenu", (event) => {
    if (state.inputLock) event.preventDefault();
  }, true);

  document.addEventListener("selectstart", (event) => {
    if (state.inputLock) event.preventDefault();
  }, true);

  document.addEventListener("dragstart", (event) => {
    if (state.inputLock) event.preventDefault();
  }, true);

  document.addEventListener("auxclick", (event) => {
    if (state.inputLock) event.preventDefault();
  }, true);

  document.addEventListener("mousedown", (event) => {
    if (state.inputLock && event.button === 1) event.preventDefault();
  }, true);

  document.addEventListener("dblclick", (event) => {
    if (state.inputLock) event.preventDefault();
  }, true);

  document.addEventListener("wheel", (event) => {
    if (state.inputLock) event.preventDefault();
  }, { passive: false });

  document.addEventListener("touchmove", (event) => {
    if (state.inputLock) event.preventDefault();
  }, { passive: false });

  ["gesturestart", "gesturechange", "gestureend"].forEach((name) => {
    document.addEventListener(name, (event) => {
      if (state.inputLock) event.preventDefault();
    }, { passive: false });
  });
}

/* ----------------------------------------------------------------- input */

function vibrate() {
  if (navigator.vibrate) {
    try { navigator.vibrate(8); } catch (error) { /* unsupported */ }
  }
}

function bindHoldButtons(root) {
  root.querySelectorAll("[data-bit]").forEach((button) => {
    const bit = JOY[button.dataset.bit];
    if (!bit) return;
    const pointers = new Set();

    const sync = () => {
      const on = pointers.size > 0;
      button.classList.toggle("on", on);
      button.setAttribute("aria-pressed", String(on));
      if (on) state.touchBits.add(bit);
      else state.touchBits.delete(bit);
    };

    button.addEventListener("pointerdown", (event) => {
      event.preventDefault();
      if (!state.started) beginPlay(true);
      pointers.add(event.pointerId);
      sync();
      vibrate();
      if (button.setPointerCapture) {
        try { button.setPointerCapture(event.pointerId); } catch (error) { /* ignore */ }
      }
    });

    const release = (event) => {
      if (!pointers.has(event.pointerId)) return;
      pointers.delete(event.pointerId);
      sync();
    };
    button.addEventListener("pointerup", release);
    button.addEventListener("pointercancel", release);
    button.addEventListener("lostpointercapture", release);
    button.addEventListener("contextmenu", (event) => event.preventDefault());
  });
}

function bindDpad(dpad) {
  const petals = {
    up: dpad.querySelector(".up"),
    down: dpad.querySelector(".down"),
    left: dpad.querySelector(".left"),
    right: dpad.querySelector(".right"),
  };
  let active = null;
  let origin = null;

  const highlight = (bits) => {
    for (const [name, bit] of Object.entries({
      up: JOY.UP, down: JOY.DOWN, left: JOY.LEFT, right: JOY.RIGHT,
    })) {
      if (petals[name]) petals[name].classList.toggle("on", Boolean(bits & bit));
    }
  };

  const update = (event) => {
    const rect = origin || dpad.getBoundingClientRect();
    const nx = (event.clientX - (rect.left + rect.width / 2)) / (rect.width / 2);
    const ny = (event.clientY - (rect.top + rect.height / 2)) / (rect.height / 2);
    let bits = 0;
    /* A 0.18 dead zone stops a resting thumb from drifting; 0.34 per axis
       gives clean 45° diagonals without making the axes hard to hit. */
    if (Math.hypot(nx, ny) >= 0.18) {
      if (Math.abs(nx) >= 0.34) bits |= nx < 0 ? JOY.LEFT : JOY.RIGHT;
      if (Math.abs(ny) >= 0.34) bits |= ny < 0 ? JOY.UP : JOY.DOWN;
    }
    state.dirBits = bits;
    highlight(bits);
  };

  const end = (event) => {
    if (event.pointerId !== active) return;
    active = null;
    origin = null;
    state.dirBits = 0;
    highlight(0);
  };

  dpad.addEventListener("pointerdown", (event) => {
    event.preventDefault();
    if (!state.started) beginPlay(true);
    active = event.pointerId;
    origin = dpad.getBoundingClientRect();
    if (dpad.setPointerCapture) {
      try { dpad.setPointerCapture(event.pointerId); } catch (error) { /* ignore */ }
    }
    update(event);
    vibrate();
  });
  dpad.addEventListener("pointermove", (event) => {
    if (event.pointerId === active) update(event);
  });
  dpad.addEventListener("pointerup", end);
  dpad.addEventListener("pointercancel", end);
  dpad.addEventListener("lostpointercapture", end);
  dpad.addEventListener("contextmenu", (event) => event.preventDefault());
}

function handleKey(event, down) {
  const tag = event.target && event.target.tagName;
  if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") return;
  if (event.ctrlKey || event.metaKey || event.altKey) return;
  const code = event.code;
  if (code === "KeyP" && down) {
    togglePause();
    event.preventDefault();
    return;
  }
  if (code === "KeyR" && down) {
    reloadRom();
    event.preventDefault();
    return;
  }
  if (code === "KeyM" && down) {
    ensureAudio();
    setSound(!state.sound);
    event.preventDefault();
    return;
  }
  if (code === "KeyF" && down) {
    toggleFullscreen();
    event.preventDefault();
    return;
  }
  if (code === "F2" && down) {
    saveNow(true);
    event.preventDefault();
    return;
  }
  if (code === "F3" && down) {
    loadNow(true);
    event.preventDefault();
    return;
  }
  if (code === "KeyL" && down) {
    toggleLandscape();
    event.preventDefault();
    return;
  }
  if (!(code in KEYMAP)) return;
  event.preventDefault();
  if (!state.started && down) beginPlay(true);
  if (down) state.keys.add(code);
  else state.keys.delete(code);
}

/* ------------------------------------------------------------------ share */

function shareUrl() {
  if (typeof NEShare !== "undefined" && NEShare.enabled() && gameId) return NEShare.gameUrl(gameId);
  return location.href;
}

function copyOrShare() {
  if (typeof NEShare === "undefined" || !NEShare.enabled()) {
    toast("Sharing turns on over https on the published site.");
    return;
  }
  const url = shareUrl();
  if (NEShare.canNative()) {
    NEShare.native({ url, title, text: NEShare.shareText(title) }).then((ok) => {
      if (!ok) toast("System share is not available");
    });
    return;
  }
  NEShare.copy(url).then((ok) => toast(ok ? "Link copied" : url));
}

function bindShare() {
  const canShare = typeof NEShare !== "undefined" && NEShare.enabled();
  if (dom.shareBar) {
    dom.shareBar.dataset.ready = canShare ? "1" : "0";
    dom.shareBar.hidden = !canShare || document.body.classList.contains("touch");
  }
  if (dom.shareBtn) {
    dom.shareBtn.dataset.ready = canShare ? "1" : "0";
    dom.shareBtn.hidden = !canShare || !document.body.classList.contains("touch");
  }
  if (canShare && dom.shareBar) {
    const nativeButton = dom.shareBar.querySelector('[data-share="native"]');
    if (nativeButton) nativeButton.hidden = !NEShare.canNative();
    dom.shareBar.addEventListener("click", (event) => {
      const button = event.target.closest("[data-share]");
      if (!button) return;
      const url = shareUrl();
      const kind = button.dataset.share;
      if (kind === "copy") {
        NEShare.copy(url).then((ok) => toast(ok ? "Link copied" : url));
        return;
      }
      if (kind === "native") {
        NEShare.native({ url, title, text: NEShare.shareText(title) }).then((ok) => {
          if (!ok) toast("System share is not available");
        });
        return;
      }
      NEShare.share(kind, { url, title });
    });
  }
  if (dom.shareBtn) dom.shareBtn.addEventListener("click", copyOrShare);
}

/* ------------------------------------------------------------------- boot */

function bindUi() {
  dom.startBtn.addEventListener("click", (event) => {
    event.stopPropagation();
    runOverlayAction();
  });
  dom.overlay.addEventListener("click", (event) => {
    if (event.target.closest("button, a")) return;
    runOverlayAction();
  });

  dom.pauseBtn.addEventListener("click", togglePause);
  dom.saveBtn.addEventListener("click", () => saveNow(true));
  dom.loadBtn.addEventListener("click", () => loadNow(true));
  dom.padSaveBtn.addEventListener("click", () => saveNow(true));
  dom.padLoadBtn.addEventListener("click", () => loadNow(true));
  dom.rotateBtn.addEventListener("click", toggleLandscape);
  dom.reloadBtn.addEventListener("click", reloadRom);
  dom.overlaySaveBtn.addEventListener("click", (event) => {
    event.stopPropagation();
    saveNow(true);
  });
  dom.overlayLoadBtn.addEventListener("click", (event) => {
    event.stopPropagation();
    loadNow(true);
  });
  dom.soundBtn.addEventListener("click", () => {
    ensureAudio();
    setSound(!state.sound);
  });
  dom.fullBtn.addEventListener("click", toggleFullscreen);

  bindDpad(document.getElementById("dpad"));
  bindHoldButtons(document.getElementById("pad"));

  window.addEventListener("keydown", (event) => handleKey(event, true));
  window.addEventListener("keyup", (event) => handleKey(event, false));
  window.addEventListener("blur", () => {
    state.keys.clear();
    state.touchBits.clear();
    state.dirBits = 0;
  });

  /* A game that keeps running in a hidden tab burns battery and skips audio. */
  document.addEventListener("visibilitychange", () => {
    if (document.hidden) {
      releaseWakeLock();
      if (state.running) pauseRunning(true);
    }
  });

  document.addEventListener("fullscreenchange", () => {
    updateFullButton();
    syncLandscapeState();
    setInputLock(immersiveInputActive());
    scheduleFit();
  });
  document.addEventListener("webkitfullscreenchange", () => {
    updateFullButton();
    syncLandscapeState();
    setInputLock(immersiveInputActive());
    scheduleFit();
  });

  if (window.ResizeObserver) {
    const observer = new ResizeObserver(() => fitStage());
    observer.observe(dom.stageArea);
  }
  window.addEventListener("resize", scheduleFit);
  window.addEventListener("orientationchange", () => setTimeout(scheduleFit, 120));
  if (window.visualViewport) {
    window.visualViewport.addEventListener("resize", scheduleFit);
  }

  if (coarse) {
    const onChange = () => detectTouch();
    if (coarse.addEventListener) coarse.addEventListener("change", onChange);
    else if (coarse.addListener) coarse.addListener(onChange);
  }
}

detectTouch();
bindUi();
bindImmersiveGuards();
bindShare();
setInputLock(immersiveInputActive());
updateFullButton();
fitStage();
scheduleFit();
boot();
