#!/usr/bin/env node
/*
 * Save-state regression test.
 *
 *   node test/state.mjs [rom.nes ...]
 *
 * Saves a state, runs 90 frames, loads the state and runs 90 frames again.
 * The two frame hashes must match, on every bundled ROM.  With no arguments
 * it uses web/roms/*.nes.
 */
import { readFile } from "node:fs/promises";
import { readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const wasmPath = resolve(here, "..", "gem6502.wasm");
const romDir = resolve(here, "..", "roms");
const args = process.argv.slice(2);
const roms = args.length
  ? args
  : readdirSync(romDir).filter((name) => name.endsWith(".nes")).map((name) => join(romDir, name));

const bytes = await readFile(wasmPath);
const { instance } = await WebAssembly.instantiate(bytes, {});
const e = instance.exports;

function hashFramebuffer() {
  const ptr = e.gem_web_framebuffer_rgba();
  const data = new Uint8Array(e.memory.buffer, ptr, 256 * 240 * 4);
  let h = 2166136261 >>> 0;
  for (let i = 0; i < data.length; i++) {
    h ^= data[i];
    h = Math.imul(h, 16777619) >>> 0;
  }
  return h;
}

let failures = 0;
for (const romPath of roms) {
  if (!e.gem_web_init()) throw new Error("init failed");
  const rom = new Uint8Array(await readFile(romPath));
  new Uint8Array(e.memory.buffer).set(rom, e.gem_web_rom_buffer());
  if (e.gem_web_load_rom(e.gem_web_rom_buffer(), rom.length) !== 0) throw new Error("load rom failed: " + romPath);

  for (let i = 0; i < 150; i++) e.gem_web_run_frame();

  const size = e.gem_web_save_state();
  const saved = new Uint8Array(e.memory.buffer, e.gem_web_state_buffer(), size).slice();

  for (let i = 0; i < 90; i++) e.gem_web_run_frame();
  const before = hashFramebuffer();

  const dst = new Uint8Array(e.memory.buffer, e.gem_web_state_buffer(), saved.length);
  dst.set(saved);
  const rc = e.gem_web_load_state(saved.length);
  for (let i = 0; i < 90; i++) e.gem_web_run_frame();
  const after = hashFramebuffer();

  const ok = rc === 0 && before === after && size > 1000;
  if (!ok) failures++;
  console.log(`${ok ? "ok  " : "FAIL"} ${romPath.split("/").pop()} state=${size}B rc=${rc} hash ${before.toString(16)} -> ${after.toString(16)}`);
  e.gem_web_destroy();
}

process.exit(failures ? 1 : 0);
