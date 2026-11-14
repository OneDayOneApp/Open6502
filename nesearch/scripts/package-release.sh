#!/usr/bin/env bash
# Build a macOS release archive for nesearch.
#
#   bash nesearch/scripts/package-release.sh                 # build + stage + zip
#   bash nesearch/scripts/package-release.sh --no-build      # reuse existing binaries
#   bash nesearch/scripts/package-release.sh --out /tmp/dist --version 0.2.0
#
# Output: dist/nesearch-<version>-macos-<arch>.zip and a matching .sha256 file.
# The archive layout is the one run-ui.command and the docs expect:
#
#   nesearch-<version>-macos-<arch>/
#     nesearch  ui/  emulators/{gem6502,6502EMU}  lib/libSDL2-2.0.0.dylib
#     LICENSE  LICENSES/  README.md  run-ui.command
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
NES="$ROOT/nesearch"
PKG="$NES/packaging"

VERSION=""
ARCH=""
OUT="$ROOT/dist"
DO_BUILD=1
KEEP=0

usage() { sed -n '2,16p' "$0"; }

while [ $# -gt 0 ]; do
  case "$1" in
    --no-build) DO_BUILD=0; shift ;;
    --keep) KEEP=1; shift ;;
    --out) OUT=${2:?--out needs a directory}; shift 2 ;;
    --version) VERSION=${2:?--version needs a value}; shift 2 ;;
    --arch) ARCH=${2:?--arch needs a value}; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "package-release: unknown option: $1" >&2; exit 2 ;;
  esac
done

if [ "$(uname -s)" != "Darwin" ]; then
  echo "package-release: this script produces a macOS bundle; run it on macOS" >&2
  exit 1
fi

if [ -z "$VERSION" ]; then
  VERSION=$(sed -n 's/^project(nesearch VERSION \([0-9][0-9.]*\).*/\1/p' "$NES/CMakeLists.txt")
fi
if [ -z "$ARCH" ]; then
  ARCH=$(uname -m)
fi
[ -n "$VERSION" ] || { echo "package-release: cannot determine version" >&2; exit 1; }

for tool in install_name_tool codesign otool zip shasum; do
  command -v "$tool" >/dev/null 2>&1 || { echo "package-release: missing $tool" >&2; exit 1; }
done

# SDL2 can live at /opt/homebrew/opt/sdl2, behind an sdl2-compat alias, or in a
# custom prefix. Probe the usual locations and fall back to pkg-config.
SDL2_PREFIX=${SDL2_PREFIX:-}
if [ -z "$SDL2_PREFIX" ]; then
  for candidate in \
      /opt/homebrew/opt/sdl2 /usr/local/opt/sdl2 \
      "$(brew --prefix sdl2 2>/dev/null || true)" \
      "$(pkg-config --variable=prefix sdl2 2>/dev/null || true)"; do
    if [ -n "$candidate" ] && [ -f "$candidate/lib/libSDL2-2.0.0.dylib" ]; then
      SDL2_PREFIX="$candidate"
      break
    fi
  done
fi
SDL2_DYLIB="$SDL2_PREFIX/lib/libSDL2-2.0.0.dylib"
[ -n "$SDL2_PREFIX" ] && [ -f "$SDL2_DYLIB" ] \
  || { echo "package-release: SDL2 not found (set SDL2_PREFIX)" >&2; exit 1; }

# Prefer the license text shipped with the formula; sdl2-compat aliases drop it.
SDL2_LICENSE=""
for candidate in "$SDL2_PREFIX"/LICENSE* "$SDL2_PREFIX"/COPYING* "$(dirname "$SDL2_PREFIX")"/../../Cellar/sdl2*/*/LICENSE*; do
  if [ -f "$candidate" ]; then SDL2_LICENSE="$candidate"; break; fi
done
if [ -z "$SDL2_LICENSE" ]; then
  SDL2_LICENSE="$PKG/SDL2-LICENSE.txt"
  [ -f "$SDL2_LICENSE" ] || { echo "package-release: no SDL2 license text found" >&2; exit 1; }
fi
echo "  SDL2: $SDL2_PREFIX"

echo "package-release: nesearch $VERSION ($ARCH)"

if [ "$DO_BUILD" = "1" ]; then
  echo "  building nesearch"
  # Match the documented build recipe: Apple clang from the Command Line Tools.
  # Without the explicit compiler CMake may pick a clang whose libc++ headers do
  # not line up with the installed SDK, which breaks <filesystem>/<math.h>.
  CMAKE_ARGS=(-S "$NES" -B "$NES/build" -DCMAKE_BUILD_TYPE=Release)
  if [ -x /usr/bin/clang++ ]; then
    CMAKE_ARGS+=(-DCMAKE_CXX_COMPILER=/usr/bin/clang++)
  fi
  cmake "${CMAKE_ARGS[@]}" >/dev/null
  cmake --build "$NES/build" -j"$(sysctl -n hw.ncpu)" >/dev/null
  echo "  building GEM6502"
  make -C "$ROOT/GEM6502" >/dev/null
  echo "  building NEMU 6502EMU"
  make -C "$ROOT/NEMU" m6502_defconfig >/dev/null
  make -C "$ROOT/NEMU" -j"$(sysctl -n hw.ncpu)" >/dev/null
fi

for artifact in "$NES/build/nesearch" "$ROOT/GEM6502/gem6502" "$ROOT/NEMU/build/6502EMU"; do
  [ -x "$artifact" ] || { echo "package-release: missing $artifact (drop --no-build?)" >&2; exit 1; }
done

NAME="nesearch-$VERSION-macos-$ARCH"
WORK=$(mktemp -d "${TMPDIR:-/tmp}/nesearch-pkg.XXXXXX")
if [ "$KEEP" = "1" ]; then trap 'echo "package-release: staging kept at $WORK/$NAME"' EXIT; else trap 'rm -rf "$WORK"' EXIT; fi

STAGE="$WORK/$NAME"
mkdir -p "$STAGE/ui" "$STAGE/emulators" "$STAGE/lib" "$STAGE/LICENSES"

# --- binaries --------------------------------------------------------------
install -m 755 "$NES/build/nesearch" "$STAGE/nesearch"
install -m 755 "$ROOT/GEM6502/gem6502" "$STAGE/emulators/gem6502"
install -m 755 "$ROOT/NEMU/build/6502EMU" "$STAGE/emulators/6502EMU"
install -m 644 "$SDL2_DYLIB" "$STAGE/lib/libSDL2-2.0.0.dylib"

# UI assets (mockup.html is the production page; the assets are its fallbacks).
# Screenshots are documentation for the repository, not part of the shipped UI.
cp -R "$NES/ui/." "$STAGE/ui/"
rm -rf "$STAGE/ui/screenshots"

# --- launcher and docs ------------------------------------------------------
install -m 755 "$PKG/run-ui.command" "$STAGE/run-ui.command"
sed -e "s/__VERSION__/$VERSION/g" -e "s/__ARCH__/$ARCH/g" "$PKG/README.md" >"$STAGE/README.md"

# --- licenses ---------------------------------------------------------------
install -m 644 "$ROOT/LICENSE" "$STAGE/LICENSE"
install -m 644 "$ROOT/THIRD_PARTY_NOTICES.md" "$STAGE/LICENSES/THIRD_PARTY_NOTICES.md"
install -m 644 "$ROOT/NEMU/LICENSE" "$STAGE/LICENSES/NEMU-MulanPSL2.txt"
install -m 644 "$ROOT/NEMU/NOTICE" "$STAGE/LICENSES/NEMU-NOTICE.txt"
install -m 644 "$SDL2_LICENSE" "$STAGE/LICENSES/SDL2.txt"
install -m 644 "$PKG/nlohmann-json-MIT.txt" "$STAGE/LICENSES/nlohmann-json-MIT.txt"
install -m 644 "$PKG/cpp-httplib-MIT.txt" "$STAGE/LICENSES/cpp-httplib-MIT.txt"
install -m 644 "$PKG/LICENSES-README.md" "$STAGE/LICENSES/README.md"

# --- bundle SDL2 and re-sign -----------------------------------------------
# The emulators are built against Homebrew's SDL2; point them at the copy that
# ships next to them, then ad-hoc sign the Mach-O files (install_name_tool
# invalidates the signature and arm64 refuses to run unsigned binaries).
(
  cd "$STAGE"
  install_name_tool -id "@loader_path/libSDL2-2.0.0.dylib" lib/libSDL2-2.0.0.dylib 2>/dev/null
  for binary in emulators/gem6502 emulators/6502EMU; do
    linked=$(otool -L "$binary" | awk '/libSDL2/ {print $1; exit}')
    if [ "$linked" != "@loader_path/../lib/libSDL2-2.0.0.dylib" ]; then
      install_name_tool -change "$linked" "@loader_path/../lib/libSDL2-2.0.0.dylib" "$binary" 2>/dev/null
    fi
    codesign --force --sign - --timestamp=none "$binary" >/dev/null 2>&1
  done
  codesign --force --sign - --timestamp=none lib/libSDL2-2.0.0.dylib >/dev/null 2>&1
  codesign --force --sign - --timestamp=none nesearch >/dev/null 2>&1
)

# --- smoke test -------------------------------------------------------------
"$STAGE/nesearch" --version >/dev/null
for binary in "$STAGE/emulators/gem6502" "$STAGE/emulators/6502EMU"; do
  otool -L "$binary" | grep -q "@loader_path/../lib/libSDL2-2.0.0.dylib" \
    || { echo "package-release: $binary still points at Homebrew SDL2" >&2; exit 1; }
done

# --- zip --------------------------------------------------------------------
mkdir -p "$OUT"
ZIP="$OUT/$NAME.zip"
rm -f "$ZIP" "$ZIP.sha256"
(cd "$WORK" && zip -qry "$ZIP" "$NAME")
(cd "$OUT" && shasum -a 256 "$NAME.zip" >"$NAME.zip.sha256")

echo "  $(du -h "$ZIP" | cut -f1)  $ZIP"
echo "  $(cut -c1-16 "$ZIP.sha256")…  $(basename "$ZIP.sha256")"
echo "  contents: $(unzip -l "$ZIP" | tail -1 | awk '{print $2}') files, licenses in LICENSES/"
