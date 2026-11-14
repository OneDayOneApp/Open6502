#!/usr/bin/env python3
"""Generate one static landing page per catalog entry for social previews.

Crawlers do not run JavaScript, so `index.html?game=<id>` cannot carry a
per-game preview. This script writes `game/<id>.html` with the game's own
Open Graph tags (title, description, title-screen image) and a small landing
page whose buttons jump into the player or into the catalog.

Usage: build-share-pages.py <catalog.json> <out-dir> <site-base-url>
"""

import html
import json
import os
import sys

FALLBACK_IMAGE = "https://onedayoneapp.github.io/Open6502/assets/img/nesearch/discover.png"
FALLBACK_DESCRIPTION = "nesearch — an open catalogue of NES games you can play in the browser."


def esc(value):
    return html.escape(str(value or ""), quote=True)


def safe_url(value, schemes=("https://",)):
    text = str(value or "")
    return text if any(text.startswith(scheme) for scheme in schemes) else ""


def page(game, base):
    game_id = esc(game.get("id", ""))
    title = esc(game.get("title") or game.get("id") or "NES game")
    author = esc(game.get("author") or "Unknown author")
    license_name = esc(game.get("license") or "Unknown")
    license_kind = game.get("license_kind") or "unknown"
    size = esc(game.get("size_label") or "")
    mapper = esc(game.get("mapper_name") or game.get("mapper") or "")
    year = esc(game.get("year") or "")
    description = esc(game.get("description") or "")
    shot = safe_url(game.get("screenshot"))
    image = shot or FALLBACK_IMAGE
    rom = safe_url(game.get("url"))
    page_url = base + "game/" + game_id + ".html"
    from urllib.parse import quote

    def q(value):
        return quote(str(value or ""), safe="")

    play_url = "../play.html?rom=" + q(rom) + "&title=" + q(game.get("title") or game.get("id")) + "&id=" + q(game.get("id"))
    catalog_url = "../?game=" + game_id

    summary_parts = [author, license_name]
    if size:
        summary_parts.append(size)
    if mapper:
        summary_parts.append("mapper " + mapper)
    summary = " · ".join(part for part in summary_parts if part)
    og_description = esc(f"{summary} — an open-source NES game you can play right in the browser.")

    chips = "".join(
        f'<span class="chip{" amber" if license_kind == "unknown" else ""}">{esc(text)}</span>'
        for text in (license_name, mapper, size, year)
        if text
    )
    if shot and rom:
        hero = (
            f'<a class="shot-link" href="{esc(play_url)}" title="Play {title} in the browser" aria-label="Play {title} in the browser">'
            f'<img class="shot" src="{esc(shot)}" alt="{title} title screen" onerror="this.style.display=\'none\'">'
            '<span class="play-hint">▶ Click to play</span>'
            "</a>"
        )
    elif shot:
        hero = f'<img class="shot" src="{esc(shot)}" alt="{title} title screen" onerror="this.style.display=\'none\'">'
    else:
        hero = ""
    play_button = (
        f'<a class="btn primary" href="{esc(play_url)}">▶ Play in browser</a>' if rom else ""
    )

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title} — nesearch</title>
<link rel="icon" href="../../assets/img/favicon.svg" type="image/svg+xml">
<link rel="canonical" href="{esc(page_url)}">
<meta name="description" content="{og_description}">
<meta property="og:type" content="article">
<meta property="og:site_name" content="Open6502">
<meta property="og:title" content="{title} — nesearch">
<meta property="og:description" content="{og_description}">
<meta property="og:url" content="{esc(page_url)}">
<meta property="og:image" content="{esc(image)}">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:title" content="{title} — nesearch">
<meta name="twitter:description" content="{og_description}">
<meta name="twitter:image" content="{esc(image)}">
<style>
  :root {{ color-scheme: dark; --bg:#0e1014; --panel:#151922; --line:#262c38; --text:#e8ecf4; --muted:#8b94a7; --red:#ff4d57; }}
  * {{ box-sizing:border-box; }}
  body {{ margin:0; background:var(--bg); color:var(--text); font:15px/1.6 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"PingFang SC","Microsoft YaHei",sans-serif; }}
  .wrap {{ max-width:720px; margin:0 auto; padding:28px 18px 56px; }}
  .top {{ display:flex; justify-content:space-between; align-items:center; gap:12px; margin-bottom:18px; font-size:13px; }}
  .top a {{ color:var(--muted); text-decoration:none; }}
  .top a:hover {{ color:var(--text); }}
  .card {{ border:1px solid var(--line); border-radius:14px; overflow:hidden; background:var(--panel); }}
  .shot {{ display:block; width:100%; background:#000; image-rendering:pixelated; }}
  .shot-link {{ display:block; position:relative; text-decoration:none; }}
  .shot-link .play-hint {{
    position:absolute; left:50%; top:50%; transform:translate(-50%,-50%);
    padding:10px 18px; border-radius:999px; background:rgba(255,77,87,.92); color:#19080b;
    font-weight:700; font-size:14px; opacity:0; transition:opacity .15s ease; pointer-events:none;
  }}
  .shot-link:hover .play-hint, .shot-link:focus-visible .play-hint {{ opacity:1; }}
  .shot-link:hover .shot {{ filter:brightness(1.08); }}
  .shot-link:focus-visible {{ outline:2px solid var(--red); outline-offset:-2px; }}
  .body {{ padding:18px 18px 20px; }}
  h1 {{ margin:0 0 4px; font-size:22px; letter-spacing:.2px; }}
  .author {{ margin:0 0 12px; color:var(--muted); font-size:13px; }}
  .chips {{ display:flex; flex-wrap:wrap; gap:6px; margin-bottom:14px; }}
  .chip {{ font:11px ui-monospace,SFMono-Regular,Menlo,monospace; padding:4px 8px; border-radius:6px; border:1px solid var(--line); color:var(--muted); background:#10141b; }}
  .chip.amber {{ color:#ffd39a; border-color:rgba(255,180,84,.25); }}
  .desc {{ margin:0 0 18px; color:#c3cad8; font-size:14px; }}
  .actions {{ display:flex; gap:10px; flex-wrap:wrap; }}
  .btn {{ display:inline-flex; align-items:center; gap:6px; padding:10px 16px; border-radius:9px; text-decoration:none; font-size:14px; border:1px solid var(--line); color:var(--text); }}
  .btn:hover {{ border-color:#3a4356; }}
  .btn.primary {{ background:var(--red); border-color:var(--red); color:#19080b; font-weight:700; }}
  .note {{ margin-top:16px; color:var(--muted); font-size:12px; }}
</style>
</head>
<body>
<div class="wrap">
  <div class="top">
    <a href="../">← nesearch catalog</a>
    <a href="https://github.com/OneDayOneApp/Open6502" target="_blank" rel="noopener">Open6502 ↗</a>
  </div>
  <div class="card">
    {hero}
    <div class="body">
      <h1>{title}</h1>
      <p class="author">by {author}</p>
      <div class="chips">{chips}</div>
      {f'<p class="desc">{description}</p>' if description else ''}
      <div class="actions">
        {play_button}
        <a class="btn" href="{catalog_url}">View in catalog</a>
      </div>
    </div>
  </div>
  <p class="note">This is the nesearch share page for {title}: the preview image is the game's own title screen.</p>
</div>
</body>
</html>
"""


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    catalog_path, out_dir, base = sys.argv[1], sys.argv[2], sys.argv[3]
    if not base.endswith("/"):
        base += "/"
    payload = json.load(open(catalog_path, encoding="utf-8"))
    games = payload.get("games", [])
    target = os.path.join(out_dir, "game")
    os.makedirs(target, exist_ok=True)
    for name in os.listdir(target):
        if name.endswith(".html"):
            os.remove(os.path.join(target, name))
    written = 0
    for game in games:
        game_id = game.get("id")
        if not game_id:
            continue
        path = os.path.join(target, game_id + ".html")
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(page(game, base))
        written += 1
    print(f"build-share-pages: {written} game pages with per-game Open Graph tags")


if __name__ == "__main__":
    main()
