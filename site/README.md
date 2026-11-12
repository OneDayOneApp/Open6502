# Open6502 website

Static, dependency-free site for the Open6502 project. Published with GitHub
Pages from the `site/` directory on `main`.

## Pages

- `index.html` — **English default** landing page: an in-browser WebAssembly
  demo, the local play path, games, honest status, RTL/NEMU parity,
  architecture, area, roadmap, contribution and FAQ. It auto-detects Chinese
  OS/browser locales and redirects to `index.zh.html`; English is the fallback.
- `index.zh.html` — Chinese landing page with the same structure and assets.
- `docs.html` — **English default** technical documentation: quickstart,
  architecture, CPU/PPU/APU, bus, cartridge/mapper, verification, DiffTest,
  golden hashes, GEM6502, synthesis, 55nm roadmap, troubleshooting,
  contributing and further reading. It auto-detects Chinese locales and
  redirects to `docs.zh.html`.
- `docs.zh.html` — Chinese technical documentation with the same section IDs.
- `index.en.html`, `docs.en.html` — explicit English aliases. They never
  auto-redirect, so old links and “force English” links keep working.
- `404.html` — bilingual static not-found page.

### Language selection

The English default pages run a small inline script before paint:

1. If `localStorage.open6502:lang` is `zh` or `en`, that explicit choice wins.
2. Otherwise, if the primary locale (`navigator.languages[0]`, falling back
   to `navigator.language`) starts with `zh` (`zh-CN`, `zh-TW`, `zh-Hans`, …),
   redirect to the Chinese page.
3. Otherwise stay on English.

The nav language switch (`data-lang-switch`) stores the choice and keeps the
current `#section` anchor, so a reader can move between
`docs.html#verification` and `docs.zh.html#verification` directly. Each page
declares `hreflang` alternates and a matching canonical URL.

## Interactive WebAssembly demo

The `#play` section of both landing pages is a working GEM6502 emulator;
Play links jump straight to its `#emulator` card so the whole demo is visible. It
loads a ~30 KB freestanding WebAssembly build of the same core used by the
desktop player; rendering, input and audio scheduling are handled by
`assets/play/player.js`. No Emscripten runtime, no CDN, no network calls after
the page load.

| Path | Role |
| --- | --- |
| `assets/play/gem6502.wasm` | core build (CPU, PPU, APU, bus, mappers 0/1/2/3/4/7) |
| `assets/play/player.js` | Canvas, WebAudio, keyboard/gamepad/touch input, 60.0988 Hz pacing |
| `assets/play/roms/` | the six MIT-licensed test ROMs |
| `assets/css/play.css` | demo styles, scoped to `.emu*` |
| `sync-play.sh` | rebuild the wasm and refresh the three files above |

Each landing page sets two globals before loading the player:

```html
<script>
  window.GEM_PLAYER_BASE = "assets/play/";
  window.GEM_PLAYER_I18N = { running: "Running", pause: "Pause", /* … */ };
</script>
<script src="assets/play/player.js" defer></script>
```

`GEM_PLAYER_BASE` keeps the wasm and ROM URLs relative to the site root.
`GEM_PLAYER_I18N` overrides the player's UI strings: the English pages provide
an English dictionary, the Chinese page uses the built-in Chinese defaults.
Keyboard input is scoped to the demo: arrows, `P`, `R`, `M` and `Enter` are
only captured after the player has been clicked or tabbed into, so they keep
working for normal page navigation everywhere else.

The player is canonical in `GEM6502/web/`; the copies here exist because
GitHub Pages serves static files only. After changing the core, the player or
the bundled ROMs, run:

```sh
./site/sync-play.sh          # builds GEM6502/web/gem6502.wasm and copies it
```

Commit both trees. The site's `player.js` is not edited by hand; make changes
in `GEM6502/web/player.js` and re-sync.

### Checking the demo

```sh
cd site
python3 -m http.server 8765
# open http://127.0.0.1:8765/ and press Play
```

The wasm build is verified byte-for-byte against the native renderer with
`cd GEM6502 && make web-test`. In the browser, check that the overlay shows
"Click to start", the frame counter advances at ~60 fps, `P` pauses only when
the demo has focus, and the ROM selector can switch to the MMC3 titles.

## Structure

```text
site/
├── index.html          # English landing (default, auto-detects zh)
├── index.zh.html       # Chinese landing
├── index.en.html       # explicit English alias (no auto-detect)
├── docs.html           # English docs (default, auto-detects zh)
├── docs.zh.html        # Chinese docs
├── docs.en.html        # explicit English docs alias (no auto-detect)
├── 404.html            # bilingual not-found
├── robots.txt
├── sitemap.xml
├── sync-play.sh        # rebuild + copy the WebAssembly demo assets
├── .nojekyll
└── assets/
    ├── css/site.css
    ├── css/play.css        # styles for the in-page emulator
    ├── js/site.js          # progressive enhancement only
    ├── play/               # wasm, player.js, bundled ROMs
    └── img/                # logo, favicon, screenshots, frames
```

No build step, no external CDN, no tracking. Open `index.html` directly, or
serve the directory locally:

```sh
cd site
python3 -m http.server 8765
# http://127.0.0.1:8765/
```

## Deploy

The GitHub Actions workflow at `.github/workflows/pages.yml` uploads `site/` as
the Pages artifact on every push to `main` that touches the site. Pages must be
configured with **Build and deployment → Source: GitHub Actions**.

To deploy from a branch before merging, run the workflow manually from the
Actions tab (`workflow_dispatch`) after the workflow exists on `main`.

## Editing notes

- Keep all asset paths relative (`assets/...`, `docs.html`). The site is served
  from `https://onedayoneapp.github.io/Open6502/`, not a domain root.
- Keep the Chinese and English pages in sync. Section IDs, `data-*` hooks,
  image paths and commands must stay identical so the language switch and
  search keep working; only human-readable copy, `lang`, `title`, meta,
  `data-group` and `data-keywords` should differ.
- When adding a section, add it to both docs pages, the sidebar, the right-hand
  table of contents, and the sitemap alternates.
- Keep copy honest: verified numbers, known limitations and licence status must
  match `README.md`, `STATUS.md` and the repository docs. If they diverge, the
  repository code and latest test results win.
- The screenshots in `assets/img/` were captured from the bundled open-source
  ROMs with the NEMU headless frontend. `frame-rtl-title.png` comes from
  `docs/images/rtl-title.png`; it is pixel-identical to the NEMU title frame.
- The site is Chinese-first. Technical terms and commands stay in English.
- Root project licence is not published yet; do not add assets that require a
  different redistribution grant without checking `CONTRIBUTING.md`.
- The WebAssembly demo is rebuilt from `GEM6502/web/`; never edit
  `assets/play/player.js` in place. Run `./sync-play.sh` after core or player
  changes so the site, the standalone demo and the native renderer stay in
  agreement.
