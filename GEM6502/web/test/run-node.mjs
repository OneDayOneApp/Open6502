#!/usr/bin/env node
/*
 * Headless smoke test for the GEM6502 WebAssembly build.
 *
 *   node test/run-node.mjs <rom.nes> <frames> <out.ppm>
 *
 * Runs the same core through the same C ABI that player.js uses and writes a
 * PPM.  Compare it with `gem6502 game <rom> <frames> <native.ppm>` to prove
 * that the browser build renders byte-identical frames.
 */
import { readFile, writeFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { dirname, resolve } from "node:path";

const here = dirname(fileURLToPath(import.meta.url));
const wasmPath = resolve(here, "..", "gem6502.wasm");

const [romPath, framesArg = "60", outPath = "frame.ppm"] = process.argv.slice(2);
if (!romPath) {
  console.error("usage: node test/run-node.mjs <rom.nes> [frames] [out.ppm]");
  process.exit(2);
}

const audio = [];
let mem;
let floor = 0;
const dec = (ptr) => {
  let s = "";
  for (let i = 0; mem[ptr + i] !== 0; i++) s += String.fromCharCode(mem[ptr + i]);
  return s;
};

const bytes = await readFile(wasmPath);
const { instance } = await WebAssembly.instantiate(bytes, {});
const e = instance.exports;

if (Object.keys(e).length === 0) throw new Error("module has no exports");

if (!e.gem_web_init()) throw new Error("gem_web_init failed");

const rom = await readFile(romPath);
const capacity = e.gem_web_rom_capacity();
if (rom.length > capacity) throw new Error(`ROM too large (${rom.length} > ${capacity})`);
mem = new Uint8Array(e.memory.buffer); // init may have grown (and detached) memory
mem.set(rom, e.gem_web_rom_buffer());
if (e.gem_web_load_rom(e.gem_web_rom_buffer(), rom.length) !== 0) {
  throw new Error(`load failed: ${dec(e.gem_web_error())}`);
}
console.log(`loaded ${romPath} (${rom.length} bytes) mapper OK`);

const frames = Number(framesArg);
for (let i = 0; i < frames; i++) {
  mem = new Uint8Array(e.memory.buffer);
  if (!e.gem_web_run_frame()) throw new Error(`frame ${i} did not complete (halted=${e.gem_web_halted()})`);
  const n = e.gem_web_take_audio(8192);
  if (n > 0) {
    mem = new Uint8Array(e.memory.buffer);
    const pcm = new Int16Array(mem.buffer, e.gem_web_audio_buffer(), n);
    audio.push(...pcm);
  }
}

mem = new Uint8Array(e.memory.buffer);
const fbPtr = e.gem_web_framebuffer();
const fb = new Uint8Array(mem.buffer, fbPtr, 256 * 240 * 3);
const header = Buffer.from("P6\n256 240\n255\n", "ascii");
await writeFile(outPath, Buffer.concat([header, Buffer.from(fb)]));

console.log(
  `frames=${e.gem_web_frames()} cycles=${e.gem_web_cycles()} halted=${e.gem_web_halted()} ` +
  `audio=${audio.length} samples`
);
console.log(`wrote ${outPath} (${fb.length} bytes RGB24)`);
