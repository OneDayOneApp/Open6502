/*
 * Standalone player for the static catalog (GitHub Pages demo).
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

const dom = {
  canvas: document.getElementById("screen"),
  overlay: document.getElementById("overlay"),
  overlayTitle: document.getElementById("overlayTitle"),
  overlayText: document.getElementById("overlayText"),
  overlayHint: document.getElementById("overlayHint"),
  startBtn: document.getElementById("startBtn"),
  pauseBtn: document.getElementById("pauseBtn"),
  reloadBtn: document.getElementById("reloadBtn"),
  soundBtn: document.getElementById("soundBtn"),
  stats: document.getElementById("stats"),
  downloadLink: document.getElementById("downloadLink"),
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
  keys: new Set(),
  lastTs: 0,
  frameAcc: 0,
  rafId: 0,
  audio: { ctx: null, gain: null, nextTime: 0, sources: new Set() },
};

let toastTimer = 0;
function toast(message) {
  dom.toast.textContent = message;
  dom.toast.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => dom.toast.classList.remove("show"), 2600);
}

function showOverlay(heading, text, hint) {
  dom.overlayTitle.textContent = heading;
  dom.overlayText.textContent = text || "";
  dom.overlayHint.textContent = hint || "";
  dom.overlay.hidden = false;
}

function hideOverlay() {
  dom.overlay.hidden = true;
}

/* ------------------------------------------------------------------ audio */

function ensureAudio() {
  if (!state.audio.ctx) {
    const Ctor = window.AudioContext || window.webkitAudioContext;
    if (!Ctor) {
      state.sound = false;
      dom.soundBtn.textContent = "声音：不可用";
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
  dom.soundBtn.textContent = on ? "声音：开" : "声音：关";
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
  if (bytes.length > capacity) throw new Error(`ROM 太大：${bytes.length} 字节（上限 ${capacity}）`);
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

function currentButtons() {
  let bits = 0;
  for (const code of state.keys) bits |= KEYMAP[code] || 0;
  return bits;
}

function stepFrame() {
  const wasm = state.wasm;
  wasm.gem_web_set_buttons(currentButtons(), 0);
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
    dom.pauseBtn.disabled = true;
    showOverlay("CPU 已停止", title, "按 R 或点击「重载」重新载入");
    return false;
  }
  return true;
}

function updateStats() {
  const wasm = state.wasm;
  if (!wasm) return;
  dom.stats.innerHTML = `<b>${Number(wasm.gem_web_frames()).toLocaleString("en-US")}</b> frames · ${Number(
    wasm.gem_web_cycles()
  ).toLocaleString("en-US")} cycles`;
}

function loop(timestamp) {
  if (!state.running) return;
  if (!state.lastTs) state.lastTs = timestamp;
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

  state.fpsFrames += 1;
  if (timestamp - state.fpsAt >= 500) {
    updateStats();
    state.fpsFrames = 0;
    state.fpsAt = timestamp;
  }
  state.rafId = requestAnimationFrame(loop);
}

function startRunning() {
  if (state.running || state.halted) return;
  state.running = true;
  state.paused = false;
  state.lastTs = 0;
  state.frameAcc = 0;
  dom.pauseBtn.disabled = false;
  dom.pauseBtn.textContent = "暂停";
  state.rafId = requestAnimationFrame(loop);
}

function pauseRunning() {
  if (!state.running) return;
  state.running = false;
  state.paused = true;
  cancelAnimationFrame(state.rafId);
  stopAudio();
  dom.pauseBtn.textContent = "继续";
  dom.pauseBtn.disabled = false;
  showOverlay("已暂停", title, "点击「继续」或按 P 恢复");
}

function beginPlay() {
  ensureAudio();
  hideOverlay();
  state.started = true;
  startRunning();
}

async function loadRom() {
  if (!romUrl) {
    showOverlay("没有 ROM", "缺少 ?rom= 参数", "回到目录选择一款游戏");
    return false;
  }
  showOverlay("加载中", "正在读取 ROM", romUrl.length > 90 ? romUrl.slice(0, 88) + "…" : romUrl);
  try {
    const response = await fetch(romUrl);
    if (!response.ok) throw new Error(`无法读取 ROM（HTTP ${response.status}）`);
    state.romBytes = new Uint8Array(await response.arrayBuffer());
  } catch (error) {
    const message = String((error && error.message) || error);
    showOverlay("加载失败", message, "若为跨域限制，可点击右上角「下载 ROM」在本机播放");
    dom.overlayText.style.userSelect = "text";
    return false;
  }
  try {
    loadRomBytes(state.romBytes);
  } catch (error) {
    showOverlay("无法载入", String((error && error.message) || error), "该 ROM 可能不在 GEM6502 支持的 mapper 范围内");
    return false;
  }
  // Warm up a few frames so the paused card shows the title screen.
  for (let i = 0; i < 30; i += 1) {
    if (!state.wasm.gem_web_run_frame()) break;
  }
  drawFrame();
  state.paused = true;
  showOverlay("点击开始", `${title} · 音频需要一次用户操作`, "点击「开始」或按任意键");
  dom.pauseBtn.disabled = true;
  dom.reloadBtn.disabled = false;
  updateStats();
  return true;
}

async function boot() {
  document.title = `nesearch — ${title}`;
  dom.downloadLink.href = romUrl || "#";
  dom.downloadLink.hidden = !romUrl;

  try {
    const response = await fetch(`${BASE}gem6502.wasm`);
    if (!response.ok) throw new Error(`无法读取 gem6502.wasm（HTTP ${response.status}）`);
    const instance = WebAssembly.instantiateStreaming
      ? await WebAssembly.instantiateStreaming(response.clone(), {}).catch(async () => {
          const bytes = await response.arrayBuffer();
          return WebAssembly.instantiate(bytes, {});
        })
      : await WebAssembly.instantiate(await response.arrayBuffer(), {});
    state.wasm = instance.instance.exports;
    if (!state.wasm.gem_web_init()) throw new Error("gem_web_init 失败");
    state.wasm.gem_web_set_buttons(0, 0);
    state.sound = true;
    dom.soundBtn.disabled = false;
    await loadRom();
  } catch (error) {
    showOverlay("初始化失败", String((error && error.message) || error), "请检查网络后刷新页面");
    toast(String((error && error.message) || error));
  }
}

/* ------------------------------------------------------------------ input */

function handleKey(event, down) {
  const tag = event.target && event.target.tagName;
  if (tag === "INPUT" || tag === "TEXTAREA" || tag === "SELECT") return;
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
  if (!(code in KEYMAP)) return;
  event.preventDefault();
  if (!state.started && down) beginPlay();
  if (down) state.keys.add(code);
  else state.keys.delete(code);
}

function togglePause() {
  if (!state.wasm || state.halted) return;
  if (state.running) pauseRunning();
  else if (state.started) {
    hideOverlay();
    startRunning();
  } else beginPlay();
}

function reloadRom() {
  if (!state.wasm || !state.romBytes) return;
  cancelAnimationFrame(state.rafId);
  state.running = false;
  stopAudio();
  try {
    loadRomBytes(state.romBytes);
  } catch (error) {
    showOverlay("无法载入", String((error && error.message) || error), "");
    return;
  }
  state.started = true;
  dom.pauseBtn.disabled = false;
  dom.pauseBtn.textContent = "暂停";
  startRunning();
}

function bindShare() {
  const bar = document.getElementById("shareBar");
  if (!bar) return;
  if (typeof NEShare === "undefined" || !NEShare.enabled()) return;
  bar.hidden = false;
  const nativeButton = bar.querySelector('[data-share="native"]');
  if (nativeButton) nativeButton.hidden = !NEShare.canNative();
  bar.addEventListener("click", (event) => {
    const button = event.target.closest("[data-share]");
    if (!button) return;
    const url = location.href;
    const kind = button.dataset.share;
    if (kind === "copy") {
      NEShare.copy(url).then((ok) => toast(ok ? "链接已复制" : url));
      return;
    }
    if (kind === "native") {
      NEShare.native({ url, title, text: NEShare.shareText(title) }).then((ok) => {
        if (!ok) toast("系统分享不可用");
      });
      return;
    }
    NEShare.share(kind, { url, title });
  });
}

bindShare();

dom.startBtn.addEventListener("click", () => beginPlay());
dom.pauseBtn.addEventListener("click", () => togglePause());
dom.reloadBtn.addEventListener("click", () => reloadRom());
dom.soundBtn.addEventListener("click", () => setSound(!state.sound));
dom.canvas.addEventListener("click", () => { if (!state.started) beginPlay(); });
window.addEventListener("keydown", (event) => handleKey(event, true));
window.addEventListener("keyup", (event) => handleKey(event, false));
window.addEventListener("blur", () => state.keys.clear());

boot();
