# nesearch UI prototype

Interactive UI for the CLI + UI dual-mode product. Opened directly (`file://`) it is a static design preview; served by `nesearch ui` it uses the live local API for search, downloads, library, and sources.

## Open it

```sh
open nesearch/ui/mockup.html
```

Or drag `mockup.html` into Chrome/Safari; without the local API it falls back to the sample data. With the built binary:

```sh
./build/nesearch ui
# open the printed http://127.0.0.1:<port>/?token=... URL
```

Screenshots under `assets/` are local; live catalog images come from the configured source URLs.

## Deep links

| URL | State |
|---|---|
| `mockup.html` | Discover with Nova selected |
| `mockup.html?view=downloads` | Download queue |
| `mockup.html?view=library` | Local library |
| `mockup.html?view=sources` | Source manager |
| `mockup.html?view=settings` | Settings |
| `mockup.html?q=contra` | Empty search state |
| `mockup.html?layout=list` | List view |
| `mockup.html?cli=1` | CLI command modal |
| `mockup.html?source=1` | Add JSON source modal |

## What is interactive

- Search, license/mapper filters, source filter, sort, grid/list toggle.
- Card selection and detail panel.
- Download button → queue → progress simulation → verified state.
- Pause/resume/remove in the panel or tray.
- Sidebar navigation, CLI command modal, add-source modal, copy button, toasts.
- Settings → Local server panel with `http://127.0.0.1:8420` copy/open actions.
- Keyboard: `⌘K`/`Ctrl+K` search, `/` search, `Esc` close.

## Scope

This is a visual/interaction specification, not the production frontend. The production UI will be served from `127.0.0.1` by the C++ core and use the API in [../UI_DESIGN.md](../UI_DESIGN.md#44-ui-api契约草案).

The 8 sample games use real homebrew metadata and screenshots for realism; they are not bundled ROMs.
