# Third-party licenses in this package

This archive contains compiled binaries, so every component that ended up in
them is listed here with its license text.

| Component | Where | License | Text |
| --- | --- | --- | --- |
| nesearch | `nesearch` | MIT, © 2026 OneDayOneApp | `../LICENSE` |
| GEM6502 | `emulators/gem6502` | MIT, © 2026 OneDayOneApp | `../LICENSE` |
| NEMU / 6502EMU | `emulators/6502EMU` | Mulan PSL v2 (fork of NJU-ProjectN/nemu) | `NEMU-MulanPSL2.txt`, `NEMU-NOTICE.txt` |
| SDL2 | `lib/libSDL2-2.0.0.dylib` | zlib | `SDL2.txt` |
| nlohmann/json 3.11.3 | compiled into `nesearch` | MIT, © 2013-2023 Niels Lohmann | `nlohmann-json-MIT.txt` |
| cpp-httplib 0.18.3 | compiled into `nesearch` | MIT, © 2017 Yuji Hirose | `cpp-httplib-MIT.txt` |
| libcurl | system `/usr/lib/libcurl.4.dylib` | curl license (MIT-style) | provided by macOS, not redistributed |
| libedit | system `/usr/lib/libedit.3.dylib` | BSD-3-Clause | provided by macOS, not redistributed |

Notes:

- `6502EMU` is built from the NEMU fork in the repository. The interactive
  monitor links the system libedit, **not** GNU readline, so this binary is not
  GPL-3.0. Builds made on Linux hosts that resolve `-lreadline` to GNU readline
  must be treated as GPL-3.0.
- NEMU's `tools/kconfig` (GPL-2.0) and `tools/qemu-diff` (GPL-3.0-or-later) are
  build/development tools; no code from them is linked into this package.
- Microsoft BASIC (`msbasic`, BSD-2-Clause) is only used by the 6502EMU BASIC
  demo when `basic/msbasic.bin` is present in a source checkout; it is not
  shipped in this archive.
- No ROMs are bundled. ROMs downloaded with `nesearch` keep their own licenses.
- The screenshots under `ui/assets/` show third-party homebrew games and remain
  with their respective authors.
