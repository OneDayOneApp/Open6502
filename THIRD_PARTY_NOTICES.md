# Third-party notices

Open6502 itself is MIT licensed (see [LICENSE](LICENSE)). This file lists the
components that come from elsewhere, what they are licensed under, and what
that means when you redistribute a checkout or a release build.

## Components vendored in this repository

| Component | Path | Upstream | License | Notes |
| --- | --- | --- | --- | --- |
| NEMU / 6502EMU | `NEMU/` | [NJU-ProjectN/nemu](https://github.com/NJU-ProjectN/nemu) | Mulan PSL v2 | Fork with the MOS 6502/NES frontend. Keep `NEMU/LICENSE` and `NEMU/NOTICE` in any redistribution. |
| kconfig | `NEMU/tools/kconfig/` | Linux kernel | **GPL-2.0** | Build-time only: compiles the `conf` tool that turns `Kconfig` into `.config`. Not linked into the emulator; do not ship the `conf` binary without the GPL sources. |
| qemu-diff | `NEMU/tools/qemu-diff/` | QEMU / Red Hat | **GPL-3.0-or-later** | Development-only GDB remote protocol client used for difftest. Not built by default; if you distribute its binary, GPL-3.0 obligations apply. |
| MMC header | `NEMU/src/device/mmc.h` | Hewlett-Packard | HP notice | "Use consistent with the GNU GPL is permitted, provided that this copyright notice is preserved." Keep the notice; do not relicense this file. |
| msbasic | `NEMU/basic/` | [mist64/msbasic](https://github.com/mist64/msbasic) | BSD-2-Clause | Microsoft BASIC disassembly by Michael Steil and contributors; see `NEMU/basic/README.md`. |
| nlohmann/json 3.11.3 | `nesearch/third_party/nlohmann/json.hpp` | [nlohmann/json](https://github.com/nlohmann/json) | MIT | SPDX header already in the file. |
| cpp-httplib 0.18.3 | `nesearch/third_party/httplib.h` | [yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib) | MIT | Copyright notice already in the file. |
| Homebrew ROMs | `roms/`, `site/assets/play/roms/` | see `roms/README.md` | MIT (6 ROMs) | Per-ROM license texts in `roms/LICENSES/`; the site copy has a matching `LICENSES/` directory. |
| Demo game | `NEMU/nesgame/` | Open6502 | MIT | Built from `game.s` in this repository. |

## Linked or bundled at build time

| Component | License | Notes |
| --- | --- | --- |
| SDL2 | zlib | Optional frontend for GEM6502 and 6502EMU. The macOS release bundles `libSDL2-2.0.0.dylib` and ships `LICENSES/SDL2.txt`. |
| libcurl | curl license (MIT-style) | Linked by nesearch for catalog and downloads; resolved from the system on macOS/Linux. |
| libedit | BSD-3-Clause | macOS system library. The macOS `6502EMU` build links `/usr/lib/libedit.3.dylib`. |
| GNU readline | **GPL-3.0** | Some Linux hosts resolve `-lreadline` to GNU readline. Building and **shipping** a Linux `6502EMU` binary that links it puts that binary under GPL-3.0; prefer libedit or document the obligation. |

## Tools used but not distributed

Verilator, Icarus Verilog, cmake, the system C/C++ toolchain, `unzip`/`tar` and
the Python interpreter are ordinary build dependencies. Nothing from them is
vendored here; the only Python in the tree is project code (`nesearch`,
`sim/`, `NEMU/tools`).

## Assets and services

- Screenshots under `nesearch/ui/assets/` and `docs/images/` show third-party
  homebrew games. They are included for identification and documentation; the
  game artwork stays with its respective authors and is **not** covered by the
  MIT license.
- The site loads Google Tag Manager. That is a hosted service used for
  analytics, not distributed code.
