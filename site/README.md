# Open6502 website

Static, dependency-free site for the Open6502 project. Published with GitHub
Pages from the `site/` directory on `main`.

## Pages

- `index.html` — user-facing landing page: play path, games, honest status,
  RTL/NEMU parity, architecture, area, roadmap, contribution and FAQ.
- `docs.html` — technical documentation: quickstart, architecture, CPU/PPU/APU,
  bus, cartridge/mapper, verification, DiffTest, golden hashes, GEM6502,
  synthesis, 55nm roadmap, troubleshooting, contributing and further reading.
- `404.html` — static not-found page.

## Structure

```text
site/
├── index.html
├── docs.html
├── 404.html
├── robots.txt
├── sitemap.xml
├── .nojekyll
└── assets/
    ├── css/site.css
    ├── js/site.js          # progressive enhancement only
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
- Keep copy honest: verified numbers, known limitations and licence status must
  match `README.md`, `STATUS.md` and the repository docs. If they diverge, the
  repository code and latest test results win.
- The screenshots in `assets/img/` were captured from the bundled open-source
  ROMs with the NEMU headless frontend. `frame-rtl-title.png` comes from
  `docs/images/rtl-title.png`; it is pixel-identical to the NEMU title frame.
- The site is Chinese-first. Technical terms and commands stay in English.
- Root project licence is not published yet; do not add assets that require a
  different redistribution grant without checking `CONTRIBUTING.md`.
