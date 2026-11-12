let GAMES = [
  {
    id: "nova",
    title: "Nova the Squirrel",
    author: "Nova Squirrel",
    license: "GPL-3.0",
    licenseKind: "open",
    source: "Homebrew Hub",
    sourceClass: "hh",
    sourceDetail: "github:homebrew-db",
    size: 262160,
    sizeLabel: "256 KiB",
    mapper: "MMC1",
    mapperKey: "MMC1",
    mapperNum: 1,
    prg: "256 KiB",
    chr: "0 KiB",
    year: 2019,
    date: "2019-04-30",
    image: "assets/nova.png",
    tags: ["Platformer", "Open Source"],
    battery: true,
    sha: "e4780e90…ec6960ab",
    page: "https://github.com/NovaSquirrel/NovaTheSquirrel",
    desc: "A fluid NES platformer starring Nova the Squirrel. Built with MMC1 and battery-backed save support."
  },
  {
    id: "falling",
    title: "Falling",
    author: "tragicmuffin",
    license: "MIT",
    licenseKind: "open",
    source: "GitHub",
    sourceClass: "",
    sourceDetail: "github:retrobrews",
    size: 40976,
    sizeLabel: "40 KiB",
    mapper: "NROM",
    mapperKey: "NROM",
    mapperNum: 0,
    prg: "32 KiB",
    chr: "8 KiB",
    year: 2018,
    date: "2018-01-07",
    image: "assets/falling.png",
    tags: ["Arcade", "Open Source"],
    battery: false,
    sha: "f597c76e…c67e943",
    page: "https://github.com/tragicmuffin/falling-nes",
    desc: "A simple NES game written in 6502 assembly. Dodge the falling objects and survive as long as you can."
  },
  {
    id: "thwaite",
    title: "Thwaite",
    author: "Damian Yerrick",
    license: "GPL-3.0",
    licenseKind: "open",
    source: "Homebrew Hub",
    sourceClass: "hh",
    sourceDetail: "github:homebrew-db",
    size: 24592,
    sizeLabel: "24 KiB",
    mapper: "NROM",
    mapperKey: "NROM",
    mapperNum: 0,
    prg: "16 KiB",
    chr: "8 KiB",
    year: 2011,
    date: "2011-06-12",
    image: "assets/thwaite.png",
    tags: ["Arcade", "Strategy", "Open Source"],
    battery: false,
    sha: "80e0918e…999843",
    page: "https://github.com/pinobatch/thwaite-nes",
    desc: "A homebrew arcade game by Damian Yerrick, with one- and two-player modes."
  },
  {
    id: "robo",
    title: "Robo-Ninja Climb",
    author: "Bite the Chili Productions",
    license: "Unknown",
    licenseKind: "unknown",
    source: "PDRoms",
    sourceClass: "pd",
    sourceDetail: "pdroms.de · NES",
    size: 49168,
    sizeLabel: "48 KiB",
    mapper: "CNROM",
    mapperKey: "CNROM",
    mapperNum: 3,
    prg: "32 KiB",
    chr: "16 KiB",
    year: 2017,
    date: "2017-11-17",
    image: "assets/robo.png",
    tags: ["Platformer", "Homebrew"],
    battery: false,
    sha: "3f3df474…f8569ab0",
    page: "https://pdroms.de/files/nintendo-entertainment-system-nes-famicom/robo-ninja-climb-nes-game",
    desc: "A short, challenging climbing and jumping game made for the 2017 NESdev competition."
  },
  {
    id: "croom",
    title: "Concentration Room",
    author: "Damian Yerrick",
    license: "GPL-3.0",
    licenseKind: "open",
    source: "Homebrew Hub",
    sourceClass: "hh",
    sourceDetail: "github:homebrew-db",
    size: 24592,
    sizeLabel: "24 KiB",
    mapper: "NROM",
    mapperKey: "NROM",
    mapperNum: 0,
    prg: "16 KiB",
    chr: "8 KiB",
    year: 2011,
    date: "2011-06-12",
    image: "assets/croom.png",
    tags: ["Puzzle", "Open Source"],
    battery: false,
    sha: "9b0b1e9b…df80507a",
    page: "https://github.com/pinobatch/croom",
    desc: "Concentration Room: a one- or two-player memory game with a story mode."
  },
  {
    id: "240p",
    title: "240p Test Suite",
    author: "Damian Yerrick",
    license: "GPL-2.0",
    licenseKind: "open",
    source: "Homebrew Hub",
    sourceClass: "hh",
    sourceDetail: "github:homebrew-db",
    size: 65552,
    sizeLabel: "64 KiB",
    mapper: "UxROM",
    mapperKey: "other",
    mapperNum: 2,
    prg: "64 KiB",
    chr: "0 KiB",
    year: 2015,
    date: "2015-10-24",
    image: "assets/240p.png",
    tags: ["Test Suite", "Utility", "Open Source"],
    battery: false,
    sha: "5be0c8b8…ac70c1736d",
    page: "https://github.com/pinobatch/240p-test-mini",
    desc: "A homebrew test suite for checking NES video output, audio, timing, and display geometry."
  },
  {
    id: "lala",
    title: "Lala The Magical",
    author: "Mojon Twins",
    license: "LGPL-3.0",
    licenseKind: "open",
    source: "GitHub",
    sourceClass: "",
    sourceDetail: "github:retrobrews",
    size: 81936,
    sizeLabel: "80 KiB",
    mapper: "Mapper 66",
    mapperKey: "other",
    mapperNum: 66,
    prg: "64 KiB",
    chr: "16 KiB",
    year: 2012,
    date: "2012-01-01",
    image: "assets/lala.png",
    tags: ["Platformer", "Open Source"],
    battery: false,
    sha: "c76222d5…955e0eb",
    page: "https://www.mojontwins.com/juegos_mojonos/lala-the-magical-nes/",
    desc: "A Mojon Twins platformer. Help Lala recover the three Power Gems from the Sky Palace."
  },
  {
    id: "tilt",
    title: "Super Tilt Bro.",
    author: "Sylvain Gadrat",
    license: "WTFPL",
    licenseKind: "open",
    source: "GitHub",
    sourceClass: "",
    sourceDetail: "github:retrobrews",
    size: 40976,
    sizeLabel: "40 KiB",
    mapper: "NROM",
    mapperKey: "NROM",
    mapperNum: 0,
    prg: "32 KiB",
    chr: "8 KiB",
    year: 2018,
    date: "2018-10-27",
    image: "assets/tilt.png",
    tags: ["Fighting", "Open Source"],
    battery: false,
    sha: "ed0369a6…4e2b3d8c",
    page: "https://github.com/sgadrat/super-tilt-bro",
    desc: "A NES homebrew inspired by Super Smash Bros. Two players fight on a tilting arena."
  }
];

const state = {
  view: "discover",
  query: "",
  license: "all",
  mapper: "all",
  source: "all",
  sort: "relevance",
  viewMode: "grid",
  libraryView: "grid",
  selected: "nova",
  page: 1
};
const PAGE_SIZE = 8;

let LIVE_MODE = false;
let TOTAL_GAMES = 221;
let LIVE_SOURCE_COUNT = 3;
let EMULATORS = { gem6502: { available: false }, nemu: { available: false } };
let downloads = [
  { id: "nova", status: "downloading", progress: 64, speed: "4.1 MiB/s", received: 167772, timer: null },
  { id: "thwaite", status: "queued", progress: 0, speed: "—", received: 0, timer: null },
  { id: "falling", status: "done", progress: 100, speed: "—", received: 40976, timer: null }
];
let libraryIds = ["falling", "lala", "tilt"];
const timers = new Map();

const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));
const gameById = (id) => GAMES.find((g) => g.id === id);
const fmtBytes = (bytes) => {
  if (bytes >= 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + " MiB";
  if (bytes >= 1024) {
    const kb = bytes / 1024;
    return (kb >= 100 || kb % 1 === 0 ? kb.toFixed(0) : kb.toFixed(1)) + " KiB";
  }
  return bytes + " B";
};

const IMAGE_MAP = {
  falling: "assets/falling.png",
  novathesquirrel: "assets/nova.png",
  nova: "assets/nova.png",
  thwaite: "assets/thwaite.png",
  croom: "assets/croom.png",
  lala: "assets/lala.png",
  "super-tilt-bro": "assets/tilt.png",
  "240p-test-suite": "assets/240p.png",
  "robo-ninja-climb": "assets/robo.png"
};

function initials(title) {
  return String(title || "?").split(/\s+/).filter(Boolean).slice(0, 2).map((part) => part[0].toUpperCase()).join("");
}

function placeholderHtml(g, className = "card-placeholder") {
  return `<div class="${className}"><div class="fallback-mark"><svg class="icon"><use href="#i-cart"/></svg><span>${escapeHtml(initials(g.title))}</span></div></div>`;
}

function thumbHtml(g) {
  if (g.image) return `<img class="thumb" src="${g.image}" alt="">`;
  return `<div class="thumb-placeholder">${escapeHtml(initials(g.title))}</div>`;
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (ch) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[ch]));
}

function downloadFor(id) {
  return downloads.find((d) => d.id === id);
}

function statusLabel(item) {
  switch (item.status) {
    case "downloading": return "Downloading";
    case "queued": return "Queued";
    case "paused": return "Paused";
    case "done": return "Verified";
    case "failed": return "Failed";
    default: return "";
  }
}

function matches(g) {
  const q = state.query.trim().toLowerCase();
  if (q) {
    const hay = [g.title, g.author, g.license, g.source, g.mapper, g.tags.join(" ")].join(" ").toLowerCase();
    if (!q.split(/\s+/).every((token) => hay.includes(token))) return false;
  }
  if (state.license === "open" && g.licenseKind !== "open") return false;
  if (state.license === "unknown" && g.licenseKind !== "unknown") return false;
  if (state.source !== "all" && g.source !== state.source) return false;
  if (state.mapper !== "all") {
    if (state.mapper === "other" && !["other"].includes(g.mapperKey)) return false;
    if (state.mapper !== "other" && g.mapperKey !== state.mapper) return false;
  }
  return true;
}

function filteredGames() {
  const list = GAMES.filter(matches);
  if (state.sort === "title") list.sort((a, b) => a.title.localeCompare(b.title));
  if (state.sort === "size") list.sort((a, b) => b.size - a.size);
  if (state.sort === "newest") list.sort((a, b) => b.year - a.year);
  return list;
}

function cardHtml(g) {
  const item = downloadFor(g.id);
  const downloaded = (item && item.status === "done") || libraryIds.includes(g.id);
  const selected = state.selected === g.id;
  const pct = item ? item.progress : 0;
  const progress = item && (item.status === "downloading" || item.status === "paused" || item.status === "queued")
    ? `<div class="card-progress"><span style="width:${pct}%"></span></div>` : "";
  return `
    <article class="card${selected ? " selected" : ""}" data-id="${g.id}" tabindex="0" aria-label="${escapeHtml(g.title)} by ${escapeHtml(g.author)}">
      <div class="card-media">
        ${g.image ? `<div class="card-placeholder media-fallback"><div class="fallback-mark"><svg class="icon"><use href="#i-cart"/></svg><span>${escapeHtml(initials(g.title))}</span></div></div><img src="${g.image}" alt="" loading="lazy" onerror="this.style.display='none'">` : placeholderHtml(g)}
        <div class="card-topline">
          <span class="source-pill ${g.sourceClass}"><span class="dot"></span>${escapeHtml(g.source)}</span>
          <button class="card-dl" data-download="${g.id}" title="${downloaded ? "Already in library" : "Download " + escapeHtml(g.title)}" aria-label="Download">
            <svg class="icon"><use href="#${downloaded ? "i-check" : "i-download"}"/></svg>
          </button>
        </div>
        ${progress}
      </div>
      <div class="card-body">
        <h3 class="card-title">${escapeHtml(g.title)}</h3>
        <p class="card-author">${escapeHtml(g.author)}</p>
        <div class="card-meta"><span>${g.sizeLabel}</span><span class="sep">·</span><span>${escapeHtml(g.mapper || "-")}</span><span class="sep">·</span><span>${g.year || "-"}</span></div>
        <div class="card-foot">
          <span class="license-pill ${g.licenseKind === "unknown" ? "unknown" : ""}"><span class="dot"></span>${escapeHtml(g.license)}</span>
          <span class="card-kind">${g.id === "240p" ? "tool" : "game"}</span>
        </div>
      </div>
    </article>`;
}

function renderPager(pages, total) {
  const pager = $("#pager");
  if (!pager) return;
  if (total === 0) { pager.innerHTML = ""; return; }
  const current = state.page;
  const numbers = new Set([1, current - 1, current, current + 1, pages]);
  const ordered = [...numbers].filter((page) => page >= 1 && page <= pages).sort((a, b) => a - b);
  let html = `<button data-page="${current - 1}" aria-label="Previous page" ${current === 1 ? "disabled" : ""}><svg class="icon" style="transform:rotate(90deg)"><use href="#i-chevron"/></svg></button>`;
  let previous = 0;
  for (const page of ordered) {
    if (previous && page > previous + 1) html += '<button disabled>…</button>';
    html += `<button data-page="${page}" class="${page === current ? "active" : ""}">${page}</button>`;
    previous = page;
  }
  html += `<button data-page="${current + 1}" aria-label="Next page" ${current === pages ? "disabled" : ""}><svg class="icon" style="transform:rotate(-90deg)"><use href="#i-chevron"/></svg></button>`;
  pager.innerHTML = html;
  $$("[data-page]", pager).forEach((button) => {
    if (button.disabled) return;
    button.addEventListener("click", () => {
      state.page = Number(button.dataset.page);
      renderGrid();
      $("#main").scrollTop = 0;
    });
  });
}

function renderGrid() {
  const all = filteredGames();
  const total = all.length;
  const pages = Math.max(1, Math.ceil(total / PAGE_SIZE));
  if (state.page > pages) state.page = pages;
  if (state.page < 1) state.page = 1;
  const start = (state.page - 1) * PAGE_SIZE;
  const list = all.slice(start, start + PAGE_SIZE);
  const grid = $("#gameGrid");
  grid.classList.toggle("list", state.viewMode === "list");
  grid.innerHTML = list.map(cardHtml).join("");

  const empty = $("#emptyState");
  empty.hidden = list.length !== 0;
  grid.hidden = list.length === 0;
  if (all.length === 0) {
    $("#emptyCopy").textContent = state.query
      ? `No title, author, or tag matches “${state.query}”. Try a different term or clear the filters.`
      : "No games match the current filters.";
  }

  const catalogTotal = LIVE_MODE ? GAMES.length : TOTAL_GAMES;
  $("#resultCount").textContent = total;
  $("#navTotal").textContent = catalogTotal;
  $("#statusTotal").textContent = `${catalogTotal} entries indexed`;
  $("#sourceSummary").textContent = state.source === "all" ? (LIVE_MODE ? `${LIVE_SOURCE_COUNT} source${LIVE_SOURCE_COUNT === 1 ? "" : "s"}` : "3 sources") : state.source;
  $("#showingLabel").textContent = total === 0
    ? "No results"
    : `Showing ${start + 1}–${Math.min(start + PAGE_SIZE, total)} of ${total}`;
  renderPager(pages, total);

  $$(".card", grid).forEach((card) => {
    card.addEventListener("click", () => selectGame(card.dataset.id));
    card.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        selectGame(card.dataset.id);
      }
    });
  });
  $$("[data-download]", grid).forEach((btn) => {
    btn.addEventListener("click", (event) => {
      event.stopPropagation();
      startDownload(btn.dataset.download);
    });
  });
}

function selectGame(id) {
  state.selected = id;
  if (state.view !== "discover") {
    state.view = "discover";
    $("#app").classList.remove("no-detail");
    $$(".view").forEach((v) => { v.hidden = v.id !== "view-discover"; });
    $$(".nav-item").forEach((b) => b.classList.toggle("active", b.dataset.view === "discover"));
  }
  renderGrid();
  renderDetail();
  if (window.innerWidth <= 1220) $("#detail").classList.add("open");
  if (LIVE_MODE) {
    api(`/api/v1/games/${encodeURIComponent(id)}`)
      .then((data) => {
        if (!data.game || state.selected !== id) return;
        const mapped = mapApiGame(data.game);
        const index = GAMES.findIndex((game) => game.id === id);
        if (index >= 0) GAMES[index] = mapped;
        renderDetail();
      })
      .catch(() => {});
  }
}

function actionButton(item, id) {
  if (item.status === "done") {
    return `<button class="row-btn" data-reveal="${id}" title="Show in folder"><svg class="icon"><use href="#i-folder"/></svg></button>`;
  }
  if (item.status === "paused") {
    return `<button class="row-btn" data-resume="${id}" title="Resume"><svg class="icon"><use href="#i-play"/></svg></button>`;
  }
  if (item.status === "queued") {
    return `<button class="row-btn" data-remove="${id}" title="Remove from queue"><svg class="icon"><use href="#i-x"/></svg></button>`;
  }
  return `<button class="row-btn" data-pause="${id}" title="Pause"><svg class="icon"><use href="#i-pause"/></svg></button>`;
}

function renderDetail() {
  const g = gameById(state.selected);
  const detail = $("#detail");
  if (!g) {
    detail.innerHTML = `<div class="detail-empty">Select a game to see details.</div>`;
    return;
  }
  const item = downloadFor(g.id);
  const pct = item ? Math.round(item.progress) : 0;
  const isDownloading = item && (item.status === "downloading" || item.status === "queued" || item.status === "paused");
  const isDone = (item && item.status === "done") || libraryIds.includes(g.id);

  let primary;
  if (isDownloading) {
    primary = `<button class="btn primary" disabled><svg class="icon"><use href="#i-download"/></svg> ${item.status === "paused" ? "Paused" : item.status === "queued" ? "Queued" : "Downloading"} ${pct}%</button>`;
  } else if (isDone) {
    primary = `<button class="btn primary" disabled><svg class="icon"><use href="#i-check"/></svg> In library</button>`;
  } else {
    primary = `<button class="btn primary" data-download="${g.id}"><svg class="icon"><use href="#i-download"/></svg> Download · ${g.sizeLabel}</button>`;
  }

  detail.innerHTML = `
    <button class="detail-close" data-close-detail aria-label="Close details"><svg class="icon"><use href="#i-x"/></svg></button>
    <div class="detail-media">
      ${g.image ? `<div class="detail-placeholder media-fallback"><div class="fallback-mark"><svg class="icon"><use href="#i-cart"/></svg><span>${escapeHtml(initials(g.title))}</span></div></div><img src="${g.image}" alt="${escapeHtml(g.title)} screenshot" onerror="this.style.display='none'">` : placeholderHtml(g, "detail-placeholder")}
      <div class="detail-top-badges">
        <span class="detail-badge">${escapeHtml(g.source)}</span>
        <span class="detail-badge">${g.year}</span>
      </div>
    </div>
    <div class="detail-head">
      <h2 class="detail-title">${escapeHtml(g.title)}</h2>
      <p class="detail-author">by ${escapeHtml(g.author)}</p>
      <div class="detail-chips">
        <span class="mini-chip ${g.licenseKind === "unknown" ? "amber" : "green"}">${escapeHtml(g.license)}</span>
        <div class="mini-chip blue">${escapeHtml(g.mapper || "-")}${g.mapperNum >= 0 ? ` · mapper ${g.mapperNum}` : ""}</div>
        ${g.mirroring ? `<span class="mini-chip blue">Mirroring ${escapeHtml(g.mirroring)}</span>` : ""}
        <span class="mini-chip">${g.id === "240p" ? "Test suite" : "Game"}</span>
        ${g.battery ? '<span class="mini-chip violet">Battery save</span>' : ""}
      </div>
      <p class="detail-desc">${escapeHtml(g.desc)}</p>
      <div class="detail-actions">
        ${primary}
        <button class="btn icon-only" data-cli title="Copy CLI command" aria-label="Copy CLI command"><svg class="icon"><use href="#i-command"/></svg></button>
        <a class="btn icon-only" href="${g.page}" target="_blank" rel="noopener" title="Open source page" aria-label="Open source page"><svg class="icon"><use href="#i-external"/></svg></a>
      </div>
      ${isDone && LIVE_MODE ? playButtonsHtml(g.id) : ""}
      ${isDownloading ? `
        <div class="detail-progress">
          <div class="progress-track"><span style="width:${pct}%"></span></div>
          <div class="progress-caption"><span>${item.status === "paused" ? "Paused" : item.status === "queued" ? "Queued" : "Downloading"}</span><span>${pct}% · ${item.speed || "—"}</span></div>
        </div>` : ""}
    </div>

    <div class="section-label">ROM file</div>
    <div class="meta-grid">
      <div><div class="meta-label">Size</div><div class="meta-value">${g.sizeLabel}</div></div>
      <div><div class="meta-label">Mapper</div><div class="meta-value">${escapeHtml(g.mapper)} (${g.mapperNum})</div></div>
      <div><div class="meta-label">PRG</div><div class="meta-value">${g.prg}</div></div>
      <div><div class="meta-label">CHR</div><div class="meta-value">${g.chr}</div></div>
      <div><div class="meta-label">Region</div><div class="meta-value">NTSC</div></div>
      <div><div class="meta-label">Battery</div><div class="meta-value ${g.battery ? "green" : ""}">${g.battery ? "Yes" : "No"}</div></div>
    </div>

    <div class="section-label">Provenance</div>
    <div class="provenance">
      <div class="prov-row"><span>Source</span><span>${escapeHtml(g.source)} · ${escapeHtml(g.sourceDetail)}</span></div>
      <div class="prov-row"><span>License</span><span>${escapeHtml(g.license)}</span></div>
      <div class="prov-row"><span>SHA-256</span><span title="${g.sha}">${g.sha}</span></div>
      <a class="prov-link" href="${g.page}" target="_blank" rel="noopener">Open source page <svg class="icon" style="width:10px;height:10px"><use href="#i-external"/></svg></a>
    </div>`;

  const downloadBtn = $("[data-download]", detail);
  if (downloadBtn) downloadBtn.addEventListener("click", () => startDownload(g.id));
  const cliBtn = $("[data-cli]", detail);
  if (cliBtn) cliBtn.addEventListener("click", () => openCli(g.id));
  const closeBtn = $("[data-close-detail]", detail);
  if (closeBtn) closeBtn.addEventListener("click", () => detail.classList.remove("open"));
  $$("[data-play-game]", detail).forEach((button) => {
    button.addEventListener("click", () => playGame(button.dataset.playGame, button.dataset.emulator));
  });
}

function renderTray() {
  const active = downloads.filter((d) => ["downloading", "paused", "queued"].includes(d.status));
  const queued = downloads.filter((d) => d.status === "queued").length;
  const done = downloads.filter((d) => d.status === "done").length;
  $("#navDownloads").textContent = active.length;
  $("#navDownloads").style.display = active.length ? "" : "none";
  $("#traySub").textContent = active.length ? `${active.length} active${queued ? ` · ${queued} queued` : ""}` : `${done} completed`;

  const visible = [...downloads].sort((a, b) => {
    const rank = { downloading: 0, paused: 1, queued: 2, done: 3, failed: 4 };
    return rank[a.status] - rank[b.status];
  }).slice(0, 3);

  $("#trayItems").innerHTML = visible.map((item) => {
    const g = gameById(item.id) || { id: item.id, title: item.id, sizeLabel: "-", image: null };
    const pct = Math.round(item.progress);
    const status = item.status === "done"
      ? `<span class="green">Verified</span>`
      : item.status === "queued" ? "Queued" : item.status === "paused" ? "Paused" : `${item.speed || ""}`;
    return `
      <div class="dl-item ${item.status}" data-select="${item.id}" title="${escapeHtml(g.title)}">
        <div class="dl-top">
          <span class="dl-status-icon"><svg class="icon"><use href="#${item.status === "done" ? "i-check" : item.status === "queued" ? "i-clock" : "i-download"}"/></svg></span>
          <span class="dl-name">${escapeHtml(g.title)}</span>
          <span class="dl-pct">${item.status === "done" ? "100%" : pct + "%"}</span>
        </div>
        <div class="dl-meta"><span>${status}</span><span>${item.status === "done" ? g.sizeLabel : fmtBytes(item.received) + " / " + g.sizeLabel}</span></div>
        <div class="progress-track"><span style="width:${item.status === "queued" ? 0 : pct}%"></span></div>
      </div>`;
  }).join("") || `<div class="dl-item done"><div class="dl-top"><span class="dl-status-icon"><svg class="icon"><use href="#i-check"/></svg></span><span class="dl-name">No active transfers</span><span class="dl-pct">—</span></div><div class="dl-meta"><span>Everything is verified</span><span></span></div><div class="progress-track"><span style="width:100%"></span></div></div>`;

  $$("[data-select]", $("#trayItems")).forEach((el) => {
    el.addEventListener("click", () => {
      selectGame(el.dataset.select);
      setView("discover");
    });
  });
}

function addToLibrary(id) {
  if (!libraryIds.includes(id)) libraryIds.push(id);
}

function startDownload(id) {
  if (LIVE_MODE) {
    const g = gameById(id);
    api("/api/v1/downloads", { method: "POST", body: JSON.stringify({ ids: [id] }) })
      .then((data) => {
        downloads = (data.downloads || []).map(mapApiDownload);
        renderTray(); renderGrid(); renderDetail(); renderDownloads();
        showToast(`Queued ${g ? g.title : id}`);
      })
      .catch((error) => showToast(error.message, true));
    return;
  }
  const g = gameById(id);
  const existing = downloadFor(id);
  if (existing) {
    if (existing.status === "done") { showToast(`${g.title} is already in your library`); return; }
    if (existing.status === "queued") { showToast(`${g.title} is already queued`); return; }
    if (existing.status === "paused") { resumeDownload(id); return; }
    showToast(`${g.title} is already downloading`);
    return;
  }
  const item = { id, status: "downloading", progress: 0, speed: "2.8 MiB/s", received: 0, timer: null };
  downloads.push(item);
  renderTray();
  renderGrid();
  renderDetail();
  renderDownloads();
  showToast(`Queued ${g.title}`);
  runTimer(item);
}

function runTimer(item) {
  clearInterval(timers.get(item.id));
  const g = gameById(item.id);
  const timer = setInterval(() => {
    if (item.status !== "downloading") return;
    item.progress = Math.min(100, item.progress + 2 + Math.random() * 6);
    item.received = Math.round(g.size * item.progress / 100);
    item.speed = (2.8 + Math.random() * 3).toFixed(1) + " MiB/s";
    if (item.progress >= 100) {
      item.progress = 100;
      item.status = "done";
      item.received = g.size;
      item.speed = "—";
      clearInterval(timer);
      timers.delete(item.id);
      addToLibrary(item.id);
      showToast(`${g.title} downloaded and verified`);
    }
    renderTray();
    renderGrid();
    renderDetail();
    renderDownloads();
    renderLibrary();
  }, 650);
  timers.set(item.id, timer);
}

function pauseDownload(id) {
  if (LIVE_MODE) {
    api(`/api/v1/downloads/${encodeURIComponent(id)}/pause`, { method: "POST" })
      .then((data) => { downloads = (data.downloads || []).map(mapApiDownload); renderTray(); renderGrid(); renderDetail(); renderDownloads(); showToast(`Paused ${gameById(id)?.title || id}`, true); })
      .catch((error) => showToast(error.message, true));
    return;
  }
  const item = downloadFor(id);
  if (!item) return;
  item.status = "paused";
  item.speed = "—";
  clearInterval(timers.get(id));
  timers.delete(id);
  renderTray(); renderGrid(); renderDetail(); renderDownloads();
  showToast(`Paused ${gameById(id).title}`, true);
}

function resumeDownload(id) {
  if (LIVE_MODE) {
    api(`/api/v1/downloads/${encodeURIComponent(id)}/resume`, { method: "POST" })
      .then((data) => { downloads = (data.downloads || []).map(mapApiDownload); renderTray(); renderGrid(); renderDetail(); renderDownloads(); showToast(`Resumed ${gameById(id)?.title || id}`); })
      .catch((error) => showToast(error.message, true));
    return;
  }
  const item = downloadFor(id);
  if (!item) return;
  item.status = "downloading";
  runTimer(item);
  renderTray(); renderGrid(); renderDetail(); renderDownloads();
  showToast(`Resumed ${gameById(id).title}`);
}

function removeDownload(id) {
  if (LIVE_MODE) {
    api(`/api/v1/downloads/${encodeURIComponent(id)}/remove`, { method: "POST" })
      .then((data) => { downloads = (data.downloads || []).map(mapApiDownload); renderTray(); renderGrid(); renderDetail(); renderDownloads(); showToast("Removed from queue", true); })
      .catch((error) => showToast(error.message, true));
    return;
  }
  downloads = downloads.filter((d) => d.id !== id);
  clearInterval(timers.get(id));
  timers.delete(id);
  renderTray(); renderGrid(); renderDetail(); renderDownloads();
  showToast("Removed from queue", true);
}

function renderDownloads() {
  const active = downloads.filter((d) => d.status !== "done");
  const done = downloads.filter((d) => d.status === "done");
  $("#activeMeta").textContent = `${active.length} item${active.length === 1 ? "" : "s"}`;
  $("#downloadsBody").innerHTML = active.map((item) => {
    const g = gameById(item.id) || { id: item.id, title: item.id, author: "Unknown", source: "-", sizeLabel: "-" };
    const pct = Math.round(item.progress);
    return `
      <tr>
        <td><div class="game-cell">${thumbHtml(g)}<div><div class="game-name">${escapeHtml(g.title)}</div><div class="game-sub">${escapeHtml(g.author)}</div></div></div></td>
        <td><span class="mini-chip">${escapeHtml(g.source)}</span></td>
        <td><span class="inline-progress progress-track"><span style="width:${item.status === "queued" ? 0 : pct}%"></span></span><span class="mono">${item.status === "queued" ? "Queued" : pct + "%"}</span></td>
        <td class="mono">${item.status === "downloading" ? item.speed : "—"}</td>
        <td class="mono">${g.sizeLabel}</td>
        <td><div class="row-actions">${actionButton(item, item.id)}<button class="row-btn" data-remove="${item.id}" title="Remove"><svg class="icon"><use href="#i-trash"/></svg></button></div></td>
      </tr>`;
  }).join("") || `<tr><td colspan="6" style="text-align:center;color:var(--text-3);padding:24px">No active downloads</td></tr>`;

  $("#completedBody").innerHTML = done.map((item) => {
    const g = gameById(item.id) || { id: item.id, title: item.id, author: "Unknown", sha: "-", image: null };
    return `
      <tr>
        <td><div class="game-cell">${thumbHtml(g)}<div><div class="game-name">${escapeHtml(g.title)}</div><div class="game-sub">${escapeHtml(g.author)}</div></div></div></td>
        <td class="mono">${g.id}.nes</td>
        <td class="mono">${g.sha}</td>
        <td><span class="status-pill done"><svg class="icon" style="width:9px;height:9px"><use href="#i-check"/></svg> Verified</span></td>
        <td><div class="row-actions"><button class="row-btn" data-reveal="${g.id}" title="Show in folder"><svg class="icon"><use href="#i-folder"/></svg></button><button class="row-btn" data-remove="${g.id}" title="Remove from list"><svg class="icon"><use href="#i-x"/></svg></button></div></td>
      </tr>`;
  }).join("") || `<tr><td colspan="5" style="text-align:center;color:var(--text-3);padding:24px">No completed downloads yet</td></tr>`;

  bindRowActions($("#view-downloads"));
}

function libraryAllIds() {
  return [...new Set([...libraryIds, ...downloads.filter((d) => d.status === "done").map((d) => d.id)])];
}

function libraryFallbackGame(id) {
  return { id, title: id, author: "Unknown", license: "Unknown", mapper: "-", sizeLabel: "-", sha: "-", image: null };
}

function libraryCardHtml(g) {
  const gem = EMULATORS.gem6502 || {};
  const nemu = EMULATORS.nemu || {};
  const image = g.image || null;
  const media = image
    ? `<div class="card-placeholder media-fallback"><div class="fallback-mark"><svg class="icon"><use href="#i-cart"/></svg><span>${escapeHtml(initials(g.title))}</span></div></div><img src="${image}" alt="" loading="lazy" onerror="this.style.display='none'">`
    : placeholderHtml(g);
  return `<article class="library-card" data-library-id="${escapeHtml(g.id)}">
    <div class="library-media">${media}</div>
    <div class="library-body">
      <div class="library-title-row"><h3 class="library-title" title="${escapeHtml(g.title)}">${escapeHtml(g.title)}</h3><span class="status-pill done"><svg class="icon" style="width:9px;height:9px"><use href="#i-check"/></svg> Verified</span></div>
      <div class="library-meta">${escapeHtml(g.author)} · ${escapeHtml(g.sizeLabel || "-")} · ${escapeHtml(g.mapper || "-")}</div>
      <div class="library-tags"><span class="mini-chip ${g.licenseKind === "unknown" ? "amber" : "green"}">${escapeHtml(g.license || "Unknown")}</span><span class="mini-chip">${escapeHtml(g.id)}.nes</span></div>
      <div class="library-actions">
        <button class="btn play" data-play-game="${escapeHtml(g.id)}" data-emulator="gem6502" ${gem.available ? "" : "disabled"} title="${gem.available ? "Play in GEM6502" : "GEM6502 not found"}"><svg class="icon"><use href="#i-play"/></svg> GEM</button>
        <button class="btn play" data-play-game="${escapeHtml(g.id)}" data-emulator="nemu" ${nemu.available ? "" : "disabled"} title="${nemu.available ? "Play in NEMU" : "NEMU not found"}"><svg class="icon"><use href="#i-play"/></svg> NEMU</button>
        <span class="spacer"></span>
        <button class="row-btn" data-reveal="${escapeHtml(g.id)}" title="Show in folder"><svg class="icon"><use href="#i-folder"/></svg></button>
        <button class="row-btn" data-remove="${escapeHtml(g.id)}" title="Remove from list"><svg class="icon"><use href="#i-trash"/></svg></button>
      </div>
    </div>
  </article>`;
}

function applyLibraryView() {
  const grid = $("#libraryGrid");
  const table = $("#libraryTableWrap");
  if (!grid || !table) return;
  const isGrid = state.libraryView !== "list";
  grid.hidden = !isGrid;
  table.hidden = isGrid;
  $("#libraryGridBtn").classList.toggle("active", isGrid);
  $("#libraryListBtn").classList.toggle("active", !isGrid);
}

function renderLibrary() {
  const games = libraryAllIds().map((id) => gameById(id) || libraryFallbackGame(id));
  const grid = $("#libraryGrid");
  grid.innerHTML = games.map(libraryCardHtml).join("") || `<div class="library-empty"><h2>No ROMs yet</h2><p>Download a game from Discover to build your local collection.</p><button class="btn soft" id="browseFromLibrary">Browse games</button></div>`;
  $("#libraryBody").innerHTML = games.map((g) => `
      <tr>
        <td><div class="game-cell">${thumbHtml(g)}<div><div class="game-name">${escapeHtml(g.title)}</div><div class="game-sub">${escapeHtml(g.author)}</div></div></div></td>
        <td class="mono">${escapeHtml(g.id)}.nes</td>
        <td class="mono">${escapeHtml(g.mapper || "-")}</td>
        <td class="mono">${escapeHtml(g.sizeLabel || "-")}</td>
        <td class="mono">${g.sha || "-"}</td>
        <td><span class="status-pill done"><svg class="icon" style="width:9px;height:9px"><use href="#i-check"/></svg> Verified</span></td>
        <td><div class="row-actions"><button class="row-btn play-emu" data-play-game="${g.id}" data-emulator="gem6502" title="Play in GEM6502">G</button><button class="row-btn play-emu" data-play-game="${g.id}" data-emulator="nemu" title="Play in NEMU">N</button><button class="row-btn" data-reveal="${g.id}" title="Show in folder"><svg class="icon"><use href="#i-folder"/></svg></button><button class="row-btn" data-remove="${g.id}" title="Remove"><svg class="icon"><use href="#i-trash"/></svg></button></div></td>
      </tr>`).join("") || `<tr><td colspan="7" style="text-align:center;color:var(--text-3);padding:24px">No ROMs yet — download a game from Discover.</td></tr>`;
  applyLibraryView();
  bindRowActions($("#view-library"));
  const browse = $("#browseFromLibrary");
  if (browse) browse.addEventListener("click", () => setView("discover"));
}

function bindRowActions(root) {
  $$("[data-pause]", root).forEach((b) => b.addEventListener("click", () => pauseDownload(b.dataset.pause)));
  $$("[data-resume]", root).forEach((b) => b.addEventListener("click", () => resumeDownload(b.dataset.resume)));
  $$("[data-remove]", root).forEach((b) => b.addEventListener("click", () => removeDownload(b.dataset.remove)));
  $$("[data-play-game]", root).forEach((b) => b.addEventListener("click", () => playGame(b.dataset.playGame, b.dataset.emulator)));
  $$("[data-reveal]", root).forEach((b) => b.addEventListener("click", () => {
    if (LIVE_MODE) {
      api("/api/v1/open", { method: "POST", body: JSON.stringify({ id: b.dataset.reveal }) })
        .then((data) => showToast(`Opened ${data.path}`))
        .catch((error) => showToast(error.message, true));
      return;
    }
    showToast("Revealing in file manager (design preview)");
  }));
}

function setView(view) {
  state.view = view;
  $("#detail").classList.remove("open");
  $$(".view").forEach((el) => { el.hidden = el.id !== `view-${view}`; });
  $$(".nav-item").forEach((b) => b.classList.toggle("active", b.dataset.view === view));
  $("#app").classList.toggle("no-detail", view !== "discover");
  if (view === "downloads") renderDownloads();
  if (view === "library") renderLibrary();
  if (view === "sources") { /* static */ }
  if (view === "discover") { renderGrid(); renderDetail(); }
  $("#main").scrollTop = 0;
}

function openCli(forceId) {
  const g = gameById(forceId || state.selected);
  const q = state.query ? ` "${state.query}"` : " --all";
  const source = state.source === "all" ? "all" : state.source.toLowerCase().replace(/\s+/g, "-");
  const license = state.license === "all" ? "any" : state.license;
  const activeIds = downloads.filter((d) => d.status !== "done").map((d) => d.id);
  const downloadIds = activeIds.length ? activeIds : g ? [g.id] : [];
  const lines = [
    "# Search what the UI is showing",
    `nesearch search${q} --source ${source} --license ${license} --sort ${state.sort}`,
    "",
    "# Download the selected game",
    `nesearch download ${g ? g.id : "nova"} -o ~/Downloads/nesearch`,
    ...(downloadIds.length > 1 ? ["", "# Or download the whole active queue", `nesearch download ${downloadIds.join(" ")} --jobs 4`] : []),
    "",
    "# Open the UI (prints an http://127.0.0.1:<port> address)",
    "nesearch ui --port 8420"
  ];
  $("#cliCode").textContent = lines.join("\n");
  $("#cliModal").hidden = false;
  $("#copyCli").dataset.text = lines.join("\n");
}

function closeCli() {
  $("#cliModal").hidden = true;
}

function copyText(text) {
  if (navigator.clipboard && window.isSecureContext) {
    return navigator.clipboard.writeText(text);
  }
  return new Promise((resolve, reject) => {
    const ta = document.createElement("textarea");
    ta.value = text;
    ta.style.position = "fixed";
    ta.style.opacity = "0";
    document.body.appendChild(ta);
    ta.select();
    try {
      document.execCommand("copy");
      resolve();
    } catch (err) {
      reject(err);
    } finally {
      ta.remove();
    }
  });
}

let toastTimer = null;
function showToast(message, warn = false) {
  const toast = $("#toast");
  $("#toastText").textContent = message;
  toast.classList.toggle("warn", warn);
  $("svg use", toast).setAttribute("href", warn ? "#i-alert" : "#i-check");
  toast.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toast.classList.remove("show"), 2200);
}

function validSourceUrl(value) {
  return /^https:\/\//i.test(value) || /^\.{0,2}\//.test(value);
}

function openSourceModal() {
  $("#sourceModal").hidden = false;
  setTimeout(() => $("#sourceName").focus(), 40);
}

function closeSourceModal() {
  $("#sourceModal").hidden = true;
}

let customSources = 0;

function addCustomSource() {
  const name = $("#sourceName").value.trim();
  const url = $("#sourceUrl").value.trim();
  if (LIVE_MODE) {
    if (!name) { showToast("Give the source a display name", true); return; }
    if (!validSourceUrl(url)) { showToast("Use an HTTPS URL or a local path", true); return; }
    api("/api/v1/sources", { method: "POST", body: JSON.stringify({ name, url }) })
      .then(async () => {
        closeSourceModal();
        showToast(`${name} added to sources`);
        await Promise.all([reloadLiveGames(), reloadLiveSources()]);
      })
      .catch((error) => showToast(error.message, true));
    return;
  }
  if (!name) { showToast("Give the source a display name", true); return; }
  if (!validSourceUrl(url)) { showToast("Use an HTTPS URL or a local path", true); return; }

  customSources += 1;
  const card = document.createElement("div");
  card.className = "source-card custom-source-card";
  card.innerHTML = `
    <div class="source-card-head">
      <div class="source-logo" style="color:var(--blue);background:var(--blue-soft)"><svg class="icon"><use href="#i-db"/></svg></div>
      <div><div class="source-name"></div><div class="source-type">custom · JSON v1</div></div>
      <span class="source-status-dot"></span>
    </div>
    <p class="source-desc">User-configured catalog. Validation and refresh follow the same cache rules as built-in sources.</p>
    <div class="source-stats"><div class="source-stat"><b>0</b><span>entries</span></div><div class="source-stat"><b>just now</b><span>last sync</span></div><div class="source-stat"><b>manual</b><span>refresh</span></div></div>
    <div class="source-card-foot"><span class="muted" title="${escapeHtml(url)}">${escapeHtml(url.length > 26 ? url.slice(0, 25) + '…' : url)}</span><button class="btn small">Configure</button></div>`;
  $(".source-name", card).textContent = name;
  $(".source-card-foot .btn", card).addEventListener("click", () => showToast("Custom source configuration (design preview)"));
  $(".sources-grid").insertBefore(card, $("#addSource"));
  $("#sourceCountLabel").textContent = `${3 + customSources} online`;
  closeSourceModal();
  showToast(`${name} added to sources`);
}

const API_TOKEN = new URLSearchParams(location.search).get("token") || "";

async function api(path, options = {}) {
  const headers = Object.assign({}, options.headers || {});
  if (options.body) headers["Content-Type"] = "application/json";
  if (API_TOKEN) headers["X-NESearch-Token"] = API_TOKEN;
  const response = await fetch(path, { credentials: "same-origin", ...options, headers });
  let data = {};
  try { data = await response.json(); } catch (error) { data = {}; }
  if (!response.ok) throw new Error(data.error || `HTTP ${response.status}`);
  return data;
}

function safeHttpUrl(value) {
  return /^https?:\/\//i.test(value || "") ? value : "#";
}

function mapApiGame(g) {
  const mapper = g.mapper_name || g.mapper || "";
  let mapperKey = "other";
  if (/NROM/i.test(mapper)) mapperKey = "NROM";
  else if (/MMC1/i.test(mapper)) mapperKey = "MMC1";
  else if (/CNROM/i.test(mapper)) mapperKey = "CNROM";
  const image = /^https?:\/\//i.test(g.screenshot || "") ? g.screenshot : (IMAGE_MAP[g.id] || null);
  const sha = g.sha256 ? `${g.sha256.slice(0, 8)}…${g.sha256.slice(-7)}` : "computed on download";
  return {
    id: g.id,
    title: g.title || g.id,
    author: g.author || "Unknown author",
    license: g.license || "Unknown",
    licenseKind: g.license_kind || ((g.license || "").toLowerCase() === "unknown" ? "unknown" : "open"),
    source: g.source || "GitHub",
    sourceClass: g.source === "Homebrew Hub" ? "hh" : g.source === "PDRoms" ? "pd" : "",
    sourceDetail: g.source_id || "github",
    size: g.size || 0,
    sizeLabel: g.size_label || fmtBytes(g.size || 0),
    mapper: mapper || "-",
    mapperKey,
    mapperNum: Number.isFinite(g.mapper_num) ? g.mapper_num : -1,
    prg: g.prg_kb ? `${g.prg_kb} KiB` : "-",
    chr: g.chr_kb ? `${g.chr_kb} KiB` : "-",
    mirroring: g.mirroring || "",
    year: g.year || "",
    date: g.date || "",
    image,
    tags: [],
    battery: !!g.battery,
    sha,
    page: safeHttpUrl(g.page_url || g.url),
    desc: g.description || "No description yet. Open the source page for details."
  };
}

function mapApiDownload(d) {
  return {
    id: d.id,
    status: d.state === "verifying" ? "downloading" : d.state,
    progress: d.progress || 0,
    speed: d.speed ? `${Number(d.speed).toFixed(1)} MiB/s` : "—",
    received: d.received || 0,
    total: d.total || 0,
    error: d.error || "",
    path: d.path || "",
    timer: null
  };
}

async function reloadLiveGames() {
  const data = await api("/api/v1/games?limit=1000");
  GAMES = (data.games || []).map(mapApiGame);
  TOTAL_GAMES = GAMES.length;
  state.page = 1;
  if (!gameById(state.selected)) state.selected = GAMES[0] ? GAMES[0].id : null;
  renderGrid();
  renderDetail();
  renderDownloads();
  renderLibrary();
}

let pollBusy = false;
async function pollLive() {
  if (!LIVE_MODE || pollBusy || document.hidden) return;
  pollBusy = true;
  try {
    const [downloadsData, libraryData] = await Promise.all([
      api("/api/v1/downloads"),
      api("/api/v1/library")
    ]);
    downloads = (downloadsData.downloads || []).map(mapApiDownload);
    libraryIds = (libraryData.entries || []).map((entry) => entry.id);
    renderLibraryCounts();
    renderTray();
    renderGrid();
    renderDetail();
    renderDownloads();
    renderLibrary();
  } catch (error) {
    // A transient poll failure should not break the UI.
  } finally {
    pollBusy = false;
  }
}

function sourceClass(source) {
  if (source.type === "github") return "github";
  if (source.type === "pdroms" || source.type === "json" || source.type === "metadata") return "pd";
  if (source.type === "archive") return "archive";
  return "hh";
}

function sourceDescription(source) {
  if (source.type === "github") return "Curated homebrew repo with raw ROM assets and per-entry metadata.";
  if (source.type === "homebrew") return "NES entries from the Homebrew Hub / homebrew-db catalog. ROMs resolve to the GitHub raw mirror.";
  if (source.type === "pdroms") return "PDRoms NES RSS feed; downloads are ZIP archives extracted to a single .nes on completion.";
  if (source.type === "archive") return "Internet Archive Advanced Search + metadata API. Collection/query is configurable; downloads use archive.org item files.";
  if (source.id === "tuxnes-mapper") return "TuxNES Bigass NES Mapper List: title → mapper / PRG / CHR / mirroring metadata used to enrich catalog entries (not a ROM download source).";
  if (source.id === "fogleman-screens") return "Fogleman NES title-screen gallery: title → screenshot URL metadata. No ROM downloads; used only to fill missing cover images.";
  if (source.type === "metadata") return "Metadata-only source; not a ROM download source.";
  return "User-configured catalog. Validation and refresh follow the same cache rules as built-in sources.";
}

function renderLiveSources(sources) {
  if (!Array.isArray(sources)) return;
  const online = sources.filter((source) => source.enabled !== false && source.status !== "offline").length;
  const entries = sources.filter((source) => source.type !== "metadata").reduce((sum, source) => sum + (source.entries || 0), 0);
  LIVE_SOURCE_COUNT = online;
  $("#sourceOnline").textContent = `${online}/${sources.length}`;
  $("#sourceRows").innerHTML = sources.map((source) => {
    const color = source.status === "offline" ? "var(--text-3)" :
      source.type === "github" ? "var(--green)" :
      (source.type === "pdroms" || source.type === "json" || source.type === "metadata") ? "var(--amber)" : "var(--violet)";
    return `<div class="source-status-row"><span class="dot" style="background:${color}"></span><span class="name">${escapeHtml(source.name)}</span><span class="count">${source.entries || 0}</span><span class="count">${source.status === "offline" ? "off" : "now"}</span></div>`;
  }).join("");
  $("#statusSources").textContent = `${online} source${online === 1 ? "" : "s"} online`;
  $("#sourceCountLabel").textContent = `${online} online`;
  $("#sourceEntryLabel").textContent = `${entries} entries`;
  const select = $("#sourceFilter");
  if (select) {
    const current = select.value;
    const catalogSources = sources.filter((source) => source.type !== "metadata");
    select.innerHTML = '<option value="all">All sources</option>' + catalogSources.map((source) => `<option value="${escapeHtml(source.name)}">${escapeHtml(source.name)}</option>`).join("");
    select.value = catalogSources.some((source) => source.name === current) ? current : "all";
  }
  const grid = $(".sources-grid");
  grid.innerHTML = sources.map((source) => {
    const cls = sourceClass(source);
    const typeLabel = source.type === "github" ? "GitHub API" :
      source.type === "homebrew" ? "homebrew-db · API" :
      source.type === "pdroms" ? "RSS · ZIP" :
      source.type === "archive" ? "Advanced Search" :
      source.type === "metadata" ? (source.id === "fogleman-screens" ? "metadata · HTML" : "metadata · TXT") : "custom · JSON v1";
    return `<div class="source-card">
      <div class="source-card-head">
        <div class="source-logo ${cls}"><svg class="icon"><use href="#${source.type === "github" ? "i-cart" : "i-db"}"/></svg></div>
        <div><div class="source-name">${escapeHtml(source.name)}</div><div class="source-type">${escapeHtml(typeLabel)}</div></div>
        <span class="source-status-dot" style="${source.enabled === false || source.status === "offline" ? "background:var(--text-3);box-shadow:none" : ""}"></span>
      </div>
      <p class="source-desc">${sourceDescription(source)}</p>
      <div class="source-stats"><div class="source-stat"><b>${source.entries || 0}</b><span>${source.id === "fogleman-screens" ? "screenshots" : source.type === "metadata" ? "records" : "entries"}</span></div><div class="source-stat"><b>${source.status === "offline" ? "offline" : source.enabled === false ? "off" : "now"}</b><span>status</span></div><div class="source-stat"><b>${source.type === "github" || source.type === "homebrew" ? "API" : source.type === "pdroms" ? "RSS" : source.type === "archive" ? "API" : source.type === "metadata" ? (source.id === "fogleman-screens" ? "HTML" : "TXT") : "JSON"}</b><span>type</span></div></div>
      <div class="source-card-foot"><span class="muted">${escapeHtml((source.url || "").slice(0, 32))}</span><button class="btn small source-config">Configure</button></div>
    </div>`;
  }).join("") + `<button class="add-source" id="addSource"><svg class="icon"><use href="#i-plus"/></svg><span>Add custom JSON source</span></button>`;
  $("#addSource").addEventListener("click", openSourceModal);
  $$(".source-config", grid).forEach((button) => button.addEventListener("click", () => showToast("Source configuration is a local setting")));
}

async function reloadLiveSources() {
  const data = await api("/api/v1/sources");
  renderLiveSources(data.sources || []);
}

function renderLibraryCounts() {
  const ids = libraryAllIds();
  $("#navLibrary").textContent = ids.length;
  const summary = $("#librarySummary");
  if (summary) {
    const bytes = ids.reduce((sum, id) => sum + ((gameById(id) || {}).size || 0), 0);
    summary.innerHTML = `<b>${ids.length}</b> ROMs · <b>${fmtBytes(bytes)}</b> · <b>${ids.length} verified</b> · <span style="color:var(--green)">local library</span>`;
  }
}

async function initLive() {
  if (!/^https?:$/.test(location.protocol)) return false;
  try {
    const [status, gamesData, downloadsData, libraryData, sourcesData, emulatorsData, configData] = await Promise.all([
      api("/api/v1/status"),
      api("/api/v1/games?limit=1000"),
      api("/api/v1/downloads"),
      api("/api/v1/library"),
      api("/api/v1/sources"),
      api("/api/v1/emulators"),
      api("/api/v1/config")
    ]);
    if (!Array.isArray(gamesData.games)) return false;
    LIVE_MODE = true;
    LIVE_SOURCE_COUNT = status.sources || 1;
    const serverCode = $(".server-box code");
    if (serverCode) serverCode.textContent = location.origin;
    const libraryPath = $("#libraryPath");
    if (libraryPath && status.download_dir) libraryPath.textContent = status.download_dir;
    GAMES = gamesData.games.map(mapApiGame);
    TOTAL_GAMES = GAMES.length;
    downloads = (downloadsData.downloads || []).map(mapApiDownload);
    libraryIds = (libraryData.entries || []).map((entry) => entry.id);
    if (!gameById(state.selected)) state.selected = GAMES[0] ? GAMES[0].id : null;
    renderLiveSources(sourcesData.sources || []);
    EMULATORS = {};
    (emulatorsData.emulators || []).forEach((emulator) => { EMULATORS[emulator.id] = emulator; });
    renderEmulatorSettings();
    if ($("#archiveQuery")) $("#archiveQuery").value = configData.archive_org_query || "";
    if ($("#archiveLimit")) $("#archiveLimit").value = configData.archive_org_limit || 12;
    if ($("#downloadDirInput")) $("#downloadDirInput").value = configData.download_dir || "";
    if ($("#jobsInput")) $("#jobsInput").value = configData.jobs || 4;
    if ($("#proxyInput")) $("#proxyInput").value = configData.proxy || "";
    if ($("#configPath") && configData.config_path) $("#configPath").textContent = configData.config_path;
    renderLibraryCounts();
    renderGrid();
    renderDetail();
    renderTray();
    renderDownloads();
    renderLibrary();
    if (state.selected) {
      api(`/api/v1/games/${encodeURIComponent(state.selected)}`)
        .then((data) => {
          if (!data.game) return;
          const mapped = mapApiGame(data.game);
          const index = GAMES.findIndex((game) => game.id === mapped.id);
          if (index >= 0) GAMES[index] = mapped;
          renderDetail();
          renderGrid();
        })
        .catch(() => {});
    }
    setInterval(pollLive, 900);
    pollLive();
    return true;
  } catch (error) {
    console.warn("nesearch: UI API unavailable, showing design preview", error);
    return false;
  }
}

async function refreshLive() {
  showToast("Refreshing sources…");
  try {
    await api("/api/v1/update", { method: "POST" });
    await reloadLiveGames();
    await reloadLiveSources();
    const status = await api("/api/v1/status");
    TOTAL_GAMES = status.games || TOTAL_GAMES;
    LIVE_SOURCE_COUNT = status.sources || LIVE_SOURCE_COUNT;
    renderGrid();
    showToast("Sources updated");
  } catch (error) {
    showToast(error.message, true);
  }
}

function playButtonsHtml(id) {
  const gem = EMULATORS.gem6502 || {};
  const nemu = EMULATORS.nemu || {};
  return `<div class="play-row">
    <button class="btn play" data-play-game="${id}" data-emulator="gem6502" ${gem.available ? "" : "disabled"} title="${gem.available ? "Play in GEM6502" : "GEM6502 not found"}"><svg class="icon"><use href="#i-play"/></svg> GEM6502</button>
    <button class="btn play" data-play-game="${id}" data-emulator="nemu" ${nemu.available ? "" : "disabled"} title="${nemu.available ? "Play in NEMU" : "NEMU not found"}"><svg class="icon"><use href="#i-play"/></svg> NEMU</button>
  </div>`;
}

function playGame(id, emulator) {
  if (!LIVE_MODE) {
    showToast("Run `nesearch ui` to launch local emulators", true);
    return;
  }
  api("/api/v1/play", { method: "POST", body: JSON.stringify({ id, emulator }) })
    .then((data) => showToast(`Opened in ${emulator === "gem6502" ? "GEM6502" : "NEMU"}`))
    .catch((error) => showToast(error.message, true));
}

async function saveRuntimeSettings() {
  if (!LIVE_MODE) return;
  try {
    await api("/api/v1/config", {
      method: "PUT",
      body: JSON.stringify({
        download_dir: $("#downloadDirInput").value.trim(),
        jobs: Number($("#jobsInput").value) || 4,
        proxy: $("#proxyInput").value.trim()
      })
    });
    showToast("Settings saved");
  } catch (error) {
    showToast(error.message, true);
  }
}

async function saveArchiveSettings() {
  if (!LIVE_MODE) return;
  try {
    await api("/api/v1/config", {
      method: "PUT",
      body: JSON.stringify({
        archive_org_query: $("#archiveQuery").value.trim(),
        archive_org_limit: Number($("#archiveLimit").value) || 12
      })
    });
    showToast("Internet Archive query saved — refresh sources to apply");
  } catch (error) {
    showToast(error.message, true);
  }
}

function renderEmulatorSettings() {
  const gem = EMULATORS.gem6502 || {};
  const nemu = EMULATORS.nemu || {};
  const gemState = $("#gemState");
  const nemuState = $("#nemuState");
  if (gemState) {
    gemState.textContent = gem.available ? "ready" : "not found";
    gemState.classList.toggle("ok", !!gem.available);
    gemState.classList.toggle("missing", !gem.available);
  }
  if (nemuState) {
    nemuState.textContent = nemu.available ? "ready" : "not found";
    nemuState.classList.toggle("ok", !!nemu.available);
    nemuState.classList.toggle("missing", !nemu.available);
  }
  if ($("#gemPath")) $("#gemPath").textContent = gem.path || "auto-detect";
  if ($("#nemuPath")) $("#nemuPath").textContent = nemu.path || "auto-detect";
}

function bindEvents() {
  $("#searchInput").addEventListener("input", (event) => {
    state.query = event.target.value;
    state.page = 1;
    $("#clearSearch").hidden = !state.query;
    renderGrid();
  });
  $("#clearSearch").addEventListener("click", () => {
    state.query = "";
    $("#searchInput").value = "";
    $("#clearSearch").hidden = true;
    renderGrid();
    $("#searchInput").focus();
  });
  $("#sourceFilter").addEventListener("change", (event) => {
    state.source = event.target.value;
    state.page = 1;
    renderGrid();
  });
  $("#sortSelect").addEventListener("change", (event) => {
    state.sort = event.target.value;
    state.page = 1;
    renderGrid();
  });
  $("#gridBtn").addEventListener("click", () => {
    state.viewMode = "grid";
    $("#gridBtn").classList.add("active");
    $("#listBtn").classList.remove("active");
    renderGrid();
  });
  $("#listBtn").addEventListener("click", () => {
    state.viewMode = "list";
    $("#listBtn").classList.add("active");
    $("#gridBtn").classList.remove("active");
    renderGrid();
  });
  $("#libraryGridBtn").addEventListener("click", () => { state.libraryView = "grid"; applyLibraryView(); });
  $("#libraryListBtn").addEventListener("click", () => { state.libraryView = "list"; applyLibraryView(); });
  $$(".chip[data-license]").forEach((chip) => {
    chip.addEventListener("click", () => {
      state.license = chip.dataset.license;
      state.page = 1;
      $$(".chip[data-license]").forEach((c) => c.classList.toggle("active", c === chip));
      renderGrid();
    });
  });
  $$(".chip[data-mapper]").forEach((chip) => {
    chip.addEventListener("click", () => {
      state.mapper = chip.dataset.mapper;
      state.page = 1;
      $$(".chip[data-mapper]").forEach((c) => c.classList.toggle("active", c === chip));
      renderGrid();
    });
  });
  $("#clearFilters").addEventListener("click", () => {
    state.query = "";
    state.license = "all";
    state.mapper = "all";
    state.source = "all";
    state.page = 1;
    $("#searchInput").value = "";
    $("#clearSearch").hidden = true;
    $("#sourceFilter").value = "all";
    $$(".chip[data-license]").forEach((c) => c.classList.toggle("active", c.dataset.license === "all"));
    $$(".chip[data-mapper]").forEach((c) => c.classList.toggle("active", c.dataset.mapper === "all"));
    renderGrid();
  });
  $("#filterInfo").addEventListener("click", () => showToast("Filters apply instantly to the local index"));
  $("#refreshSources").addEventListener("click", () => {
    if (LIVE_MODE) refreshLive(); else showToast("Source refresh requires the local UI server");
  });
  $("#refreshAll").addEventListener("click", () => {
    if (LIVE_MODE) refreshLive(); else showToast("Refreshing all sources…");
  });
  $$(".nav-item").forEach((btn) => btn.addEventListener("click", () => setView(btn.dataset.view)));
  $("#settingsBtn").addEventListener("click", () => setView("settings"));
  if ($("#downloadDirInput")) $("#downloadDirInput").addEventListener("change", saveRuntimeSettings);
  if ($("#jobsInput")) $("#jobsInput").addEventListener("change", saveRuntimeSettings);
  if ($("#proxyInput")) $("#proxyInput").addEventListener("change", saveRuntimeSettings);
  if ($("#archiveQuery")) $("#archiveQuery").addEventListener("change", saveArchiveSettings);
  if ($("#archiveLimit")) $("#archiveLimit").addEventListener("change", saveArchiveSettings);
  $("#cliBtn").addEventListener("click", () => openCli());
  $("#closeCli").addEventListener("click", closeCli);
  $("#closeCli2").addEventListener("click", closeCli);
  $("#cliModal").addEventListener("click", (event) => { if (event.target === $("#cliModal")) closeCli(); });
  $("#copyCli").addEventListener("click", () => {
    copyText($("#copyCli").dataset.text || $("#cliCode").textContent)
      .then(() => showToast("CLI commands copied"))
      .catch(() => showToast("Clipboard unavailable — select the command manually", true));
  });
  $("#trayExpand").addEventListener("click", () => setView("downloads"));
  $("#tray").addEventListener("click", (event) => {
    if (event.target.closest(".tray-head")) setView("downloads");
  });
  $("#pauseAll").addEventListener("click", () => {
    downloads.filter((d) => d.status === "downloading").forEach((d) => pauseDownload(d.id));
    showToast("All active downloads paused", true);
  });
  $("#clearDone").addEventListener("click", () => {
    if (LIVE_MODE) {
      downloads.filter((d) => d.status === "done").forEach((d) => removeDownload(d.id));
      return;
    }
    downloads = downloads.filter((d) => d.status !== "done");
    renderTray(); renderDownloads();
    showToast("Cleared completed transfers");
  });
  $("#verifyAll").addEventListener("click", () => {
    if (LIVE_MODE) {
      api("/api/v1/library/verify", { method: "POST" })
        .then((data) => {
          const results = data.results || [];
          const bad = results.filter((item) => item.status !== "ok").length;
          showToast(bad ? `${bad} file${bad === 1 ? "" : "s"} need attention` : `All ${results.length} ROMs verified`, bad > 0);
        })
        .catch((error) => showToast(error.message, true));
      return;
    }
    showToast("All 12 ROMs verified");
  });
  $("#openFolder").addEventListener("click", () => {
    if (LIVE_MODE) {
      api("/api/v1/open", { method: "POST", body: JSON.stringify({}) })
        .then((data) => showToast(`Opened ${data.path}`))
        .catch((error) => showToast(error.message, true));
      return;
    }
    showToast("Revealing ~/Downloads/nesearch (design preview)");
  });
  $("#addSource").addEventListener("click", openSourceModal);
  $("#closeSource").addEventListener("click", closeSourceModal);
  $("#sourceModal").addEventListener("click", (event) => { if (event.target === $("#sourceModal")) closeSourceModal(); });
  $("#testSource").addEventListener("click", () => {
    const url = $("#sourceUrl").value.trim();
    if (!validSourceUrl(url)) { showToast("Enter an HTTPS URL or a local path first", true); return; }
    if (LIVE_MODE) {
      api("/api/v1/sources/test", { method: "POST", body: JSON.stringify({ url }) })
        .then((data) => showToast(`Catalog reachable · ${data.entries} entries`))
        .catch((error) => showToast(error.message, true));
      return;
    }
    showToast("Catalog reachable · schema v1 · 24 entries (preview)");
  });
  $("#addSourceBtn").addEventListener("click", addCustomSource);
  $("#copyServer").addEventListener("click", () => {
    const address = LIVE_MODE ? location.origin : "http://127.0.0.1:8420";
    copyText(address).then(() => showToast("Server address copied")).catch(() => showToast("Clipboard unavailable", true));
  });
  $("#openServer").addEventListener("click", () => {
    if (LIVE_MODE) { window.open(location.origin, "_blank", "noopener"); return; }
    showToast("Start `nesearch ui` to open the local server", true);
  });
  document.addEventListener("click", (event) => {
    const sw = event.target.closest(".switch");
    if (sw) sw.classList.toggle("on");
  });
  $$(".source-card .btn").forEach((b) => b.addEventListener("click", () => showToast("Source configuration is a design preview")));
  $("#trayItems").addEventListener("click", (event) => {
    const item = event.target.closest("[data-select]");
    if (item) { selectGame(item.dataset.select); setView("discover"); }
  });
  document.addEventListener("keydown", (event) => {
    const meta = event.metaKey || event.ctrlKey;
    if (meta && event.key.toLowerCase() === "k") {
      event.preventDefault();
      $("#searchInput").focus();
      $("#searchInput").select();
    } else if (event.key === "Escape") {
      if (!$("#cliModal").hidden) closeCli();
      else if (document.activeElement === $("#searchInput")) $("#searchInput").blur();
    } else if (event.key === "/" && !["INPUT", "TEXTAREA", "SELECT"].includes(document.activeElement.tagName)) {
      event.preventDefault();
      $("#searchInput").focus();
    }
  });
}

bindEvents();
renderGrid();
renderDetail();
renderTray();
renderDownloads();
renderLibrary();

const params = new URLSearchParams(location.search);
const initialView = params.get("view");
if (initialView && ["discover", "downloads", "library", "sources", "settings"].includes(initialView)) {
  setView(initialView);
}
if (params.get("q")) {
  state.query = params.get("q");
  $("#searchInput").value = state.query;
  $("#clearSearch").hidden = false;
  renderGrid();
}
if (params.get("layout") === "list") {
  state.viewMode = "list";
  $("#listBtn").classList.add("active");
  $("#gridBtn").classList.remove("active");
  renderGrid();
}
if (params.get("library") === "list") state.libraryView = "list";
if (params.get("library") === "grid") state.libraryView = "grid";
if (params.get("detail") === "1") $("#detail").classList.add("open");
if (params.get("source") === "1") openSourceModal();
if (params.get("cli") === "1") openCli();

initLive();
