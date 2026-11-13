#!/usr/bin/env node
/*
 * Headless checks for the GitHub Pages demo.
 *
 *   node nesearch/scripts/test-static-site.mjs [path/to/catalog.json]
 *
 * 1. exercises the static fetch shim (ui/static-api.js) the same way
 *    mockup.js does on boot;
 * 2. cross-checks the query engine against the real CLI (`nesearch search
 *    --json`) so filtering/sorting cannot drift from src/catalog.cpp;
 * 3. boots the WebAssembly player core and runs frames on a real ROM.
 */
import { createRequire } from "node:module";
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const require = createRequire(import.meta.url);
const HERE = dirname(fileURLToPath(import.meta.url));
const NES = resolve(HERE, "..");
const ROOT = resolve(NES, "..");

const failures = [];
function check(name, condition, detail) {
  if (condition) {
    console.log(`  ok   ${name}`);
  } else {
    failures.push(name);
    console.log(`  FAIL ${name}${detail ? ` — ${detail}` : ""}`);
  }
}

const catalogPath = process.argv[2] || join(ROOT, "site/nesearch/data/catalog.json");
const payload = JSON.parse(readFileSync(catalogPath, "utf8"));
const api = require(join(NES, "ui/static-api.js"));

/* ------------------------------------------------------------- query engine */

console.log(`static-api: ${catalogPath}`);
check("catalog has games", payload.games.length > 0, `${payload.games.length}`);
check("every game has an id and url", payload.games.every((g) => g.id && g.url));

const all = api.queryGames(payload.games, { limit: 0 });
check("limit 0 returns everything", all.games.length === payload.games.length);
check("total matches the catalog", all.total === payload.games.length);

const paged = api.queryGames(payload.games, { limit: 10, offset: 5 });
check("pagination slices the list", paged.games.length === 10 && paged.games[0].id === all.games[5].id);

const titleSort = api.queryGames(payload.games, { limit: 0, sort: "title" }).games;
check("sort=title is ascending", okOrder(titleSort, (a, b) => lower(a.title) <= lower(b.title)));

const sizeSort = api.queryGames(payload.games, { limit: 0, sort: "size" }).games;
check("sort=size is descending", okOrder(sizeSort, (a, b) => (a.size || 0) >= (b.size || 0)));

const unknownLicense = api.queryGames(payload.games, { limit: 0, license: "unknown" }).games;
check("license=unknown only returns unknown", unknownLicense.every((g) => lower(g.license) === "unknown"));

const openLicense = api.queryGames(payload.games, { limit: 0, license: "open" }).games;
check("license=open drops unknown", openLicense.every((g) => lower(g.license) !== "unknown"));

const nrom = api.queryGames(payload.games, { limit: 0, mapper: "NROM" }).games;
check("mapper=NROM filters", nrom.every((g) => g.mapper_name === "NROM"));

const other = api.queryGames(payload.games, { limit: 0, mapper: "other" }).games;
check("mapper=other excludes known mappers", other.every((g) => !["NROM", "MMC1", "CNROM", ""].includes(g.mapper_name)));

const search = api.queryGames(payload.games, { limit: 0, q: "tetris" }).games;
check("free-text search narrows", search.length > 0 && search.length < payload.games.length, `${search.length} hits`);

/* --------------------------------------------------------- fetch shim boot */

async function shimBoot() {
  const previousFetch = globalThis.fetch;
  const previousLocation = globalThis.location;
  globalThis.location = { href: "https://example.test/Open6502/nesearch/" };
  globalThis.fetch = async (input) => {
    const url = typeof input === "string" ? input : input.url;
    if (url.includes("data/catalog.json")) {
      return new Response(JSON.stringify(payload), { status: 200, headers: { "Content-Type": "application/json" } });
    }
    throw new Error(`unexpected fetch: ${url}`);
  };
  try {
    const ready = api.install({ dataUrl: "data/catalog.json" });
    await ready;
    const boot = await Promise.all([
      fetch("/api/v1/status").then((r) => r.json()),
      fetch("/api/v1/games?limit=1000").then((r) => r.json()),
      fetch("/api/v1/downloads").then((r) => r.json()),
      fetch("/api/v1/library").then((r) => r.json()),
      fetch("/api/v1/sources").then((r) => r.json()),
      fetch("/api/v1/emulators").then((r) => r.json()),
      fetch("/api/v1/config").then((r) => r.json()),
    ]);
    const [status, games, downloads, library, sources, emulators, config] = boot;
    check("GET /status", status.games === payload.games.length && status.sources === payload.sources.length);
    check("GET /games", Array.isArray(games.games) && games.total === payload.games.length);
    check("GET /downloads", Array.isArray(downloads.downloads) && downloads.downloads.length === 0);
    check("GET /library", Array.isArray(library.entries));
    check("GET /sources", sources.sources.length === payload.sources.length);
    check("GET /emulators exposes GEM6502", emulators.emulators.some((e) => e.id === "gem6502" && e.available));
    check("GET /config", typeof config.download_dir === "string");

    const detail = await fetch(`/api/v1/games/${encodeURIComponent(payload.games[0].id)}`).then((r) => r.json());
    check("GET /games/:id", detail.game && detail.game.id === payload.games[0].id);

    const missing = await fetch("/api/v1/games/does-not-exist");
    check("GET /games/:id 404", missing.status === 404);

    const play = await (async () => {
      let opened = "";
      globalThis.open = (href) => { opened = href; };
      const response = await fetch("/api/v1/play", {
        method: "POST",
        body: JSON.stringify({ id: payload.games[0].id, emulator: "gem6502" }),
      });
      return { opened, body: await response.json() };
    })();
    check("POST /play opens play.html", play.opened.startsWith("play.html?rom=") && play.body.ok === true, play.opened);

    const filter = await fetch("/api/v1/games?q=tetris&sort=title&limit=5").then((r) => r.json());
    check("GET /games honours query params", filter.games.length <= 5 && filter.total >= filter.games.length);
  } finally {
    globalThis.fetch = previousFetch;
    globalThis.location = previousLocation;
  }
}

/* ------------------------------------------------------- CLI parity check */

function tryCli(query) {
  try {
    return cliSearch(query, {});
  } catch (error) {
    console.warn(`  warn: CLI unavailable (${String(error && error.message || error).split("\n")[0]})`);
    return null;
  }
}

function cliSearch(query, options) {
  const args = [join(NES, "build/nesearch"), "search", "--all", "--limit", "1000", "--json", "--offline"];
  if (query) args.splice(2, 0, query);
  for (const [flag, value] of Object.entries(options || {})) args.push(`--${flag}`, value);
  const output = execFileSync("env", [
    `NESEARCH_CONFIG=${join(NES, "config.sandbox.json")}`,
    `NESEARCH_CACHE_DIR=${join(NES, ".runtime/cache")}`,
    ...args,
  ], { encoding: "utf8" });
  return JSON.parse(output).games.map((game) => game.id);
}

function compareWithCli(label, query, options, staticOptions) {
  const expected = cliSearch(query, options);
  const actual = api.queryGames(payload.games, Object.assign({ limit: 0, q: query }, staticOptions)).games.map((g) => g.id);
  const sameSet = expected.length === actual.length && expected.slice().sort().join() === actual.slice().sort().join();
  check(`parity: ${label}`, sameSet, `${expected.length} cli vs ${actual.length} static`);
}

console.log("parity with `nesearch search`:");
const cliBaseline = tryCli("");
if (cliBaseline === null) {
  console.log("  skip parity checks: no CLI catalog cache (run `nesearch update` or `--refresh` first)");
} else {
  check("parity: empty query", cliBaseline.length === payload.games.length, `${cliBaseline.length} cli vs ${payload.games.length} static`);
  compareWithCli("tetris", "tetris", {}, {});
compareWithCli("multi-token", "mario bros", {}, {});
compareWithCli("license=unknown", "", { license: "unknown" }, { license: "unknown" });
compareWithCli("license=open", "", { license: "open" }, { license: "open" });
compareWithCli("mapper=other", "", { mapper: "other" }, { mapper: "other" });
compareWithCli("sort=size", "", { sort: "size" }, { sort: "size" });
compareWithCli("source=GitHub", "", { source: "GitHub" }, { source: "GitHub" });
}

/* ---------------------------------------------------- WebAssembly player */

async function playerTest() {
  const wasmPath = join(ROOT, "site/assets/play/gem6502.wasm");
  const romPath = findRom();
  check("gem6502.wasm present", existsSync(wasmPath));
  check("a test ROM is available", Boolean(romPath), romPath || "none found");
  if (!existsSync(wasmPath) || !romPath) return;

  const { instance } = await WebAssembly.instantiate(readFileSync(wasmPath), {});
  const wasm = instance.exports;
  check("gem_web_init", wasm.gem_web_init() === 1);

  const rom = readFileSync(romPath);
  new Uint8Array(wasm.memory.buffer).set(rom, wasm.gem_web_rom_buffer());
  const rc = wasm.gem_web_load_rom(wasm.gem_web_rom_buffer(), rom.length);
  check(`load ${romPath.split("/").pop()}`, rc === 0, rc === 0 ? "" : readCString(wasm));

  let produced = 0;
  for (let i = 0; i < 120; i += 1) produced += wasm.gem_web_run_frame();
  check("runs 120 frames", produced >= 100, `${produced} frames`);

  const pointer = wasm.gem_web_framebuffer_rgba();
  const pixels = new Uint8Array(wasm.memory.buffer, pointer, 256 * 240 * 4);
  let lit = 0;
  for (let i = 0; i < pixels.length; i += 4) {
    if (pixels[i] + pixels[i + 1] + pixels[i + 2] > 24) lit += 1;
  }
  check("renders a non-blank frame", lit > 500, `${lit} lit pixels`);
  check("reports frame count", Number(wasm.gem_web_frames()) >= 120);
}

function findRom() {
  const candidates = [
    join(ROOT, "site/assets/play/roms/falling.nes"),
    join(ROOT, "site/assets/play/roms/galaxypatrol.nes"),
  ];
  for (const candidate of candidates) if (existsSync(candidate)) return candidate;
  const downloads = join(NES, "downloads");
  if (existsSync(downloads)) {
    const rom = readdirSync(downloads).find((name) => name.endsWith(".nes"));
    if (rom) return join(downloads, rom);
  }
  return null;
}

function readCString(wasm) {
  const memory = new Uint8Array(wasm.memory.buffer);
  let pointer = wasm.gem_web_error();
  let end = pointer;
  while (end < memory.length && memory[end] !== 0) end += 1;
  return new TextDecoder().decode(memory.subarray(pointer, end));
}

function lower(value) {
  return String(value == null ? "" : value).toLowerCase();
}

function okOrder(list, predicate) {
  for (let i = 1; i < list.length; i += 1) if (!predicate(list[i - 1], list[i])) return false;
  return true;
}

console.log("player core:");
await playerTest();

console.log("fetch shim:");
await shimBoot();

if (failures.length) {
  console.error(`\n${failures.length} check(s) failed:\n  - ${failures.join("\n  - ")}`);
  process.exit(1);
}
console.log("\nall static-site checks passed");
