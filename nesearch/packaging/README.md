# nesearch __VERSION__ — macOS (__ARCH__)

NES 游戏搜索与下载工具：CLI + 本地 Web UI。本包已内置 GEM6502 与 NEMU/6502EMU，
在 UI 里下载 ROM 后可直接启动模拟器。

## 快速开始 / Quick start

```sh
./run-ui.command
```

或手动启动：

```sh
export NESEARCH_GEM6502="$PWD/emulators/gem6502"
export NESEARCH_NEMU="$PWD/emulators/6502EMU"
./nesearch ui --ui-dir "$PWD/ui"
```

首次使用建议先更新目录：

```sh
./nesearch update
./nesearch search falling
./nesearch download falling -o ~/Downloads
```

## 要求

- macOS 12 或更新，__ARCH__
- 需要联网访问 GitHub / Homebrew Hub / PDRoms / Internet Archive
- 本包未使用 Developer ID 签名；首次运行若被 Gatekeeper 拦截：

```sh
xattr -dr com.apple.quarantine nesearch-__VERSION__-macos-__ARCH__
```

## 内容

```
nesearch              CLI + UI 服务
ui/                   Web UI 静态资源
emulators/gem6502     GEM6502 SDL2 播放器
emulators/6502EMU     NEMU 6502/NES 前端
lib/libSDL2-2.0.0.dylib
LICENSE               MIT（nesearch、GEM6502、Open6502 代码）
LICENSES/             第三方许可证：NEMU (Mulan PSL v2)、SDL2 (zlib)、
                      nlohmann/json (MIT)、cpp-httplib (MIT)
```

## 数据目录

- 配置：`~/Library/Application Support/nesearch/config.json`
- 缓存：`~/Library/Caches/nesearch/catalog.json`
- 本地库：`~/Library/Application Support/nesearch/library.json`

## 许可 / License

- **nesearch 与 GEM6502**：MIT，见包根目录的 `LICENSE`（© 2026 OneDayOneApp）。
  允许使用、修改、再分发，保留版权与许可声明即可。
- **NEMU / 6502EMU**：Mulan PSL v2，见 `LICENSES/NEMU-MulanPSL2.txt`；
  fork 关系与树内第三方组件见 `LICENSES/NEMU-NOTICE.txt`。
- **SDL2**：zlib，见 `LICENSES/SDL2.txt`。
- **nlohmann/json、cpp-httplib**：MIT，见 `LICENSES/` 下对应文件。
- **ROM**：本包不附带任何 ROM。`nesearch` 下载的 ROM 遵循其各自许可证
  （内置目录中的条目会显示许可证，未知许可证用琥珀色标记）。
- 完整第三方清单见 `LICENSES/THIRD_PARTY_NOTICES.md` 与 `LICENSES/README.md`。
