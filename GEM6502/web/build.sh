#!/usr/bin/env bash
# Build the GEM6502 core as a freestanding WebAssembly module.
#
# Requirements:
#   - clang with the wasm32 target (Homebrew LLVM works)
#   - a wasm-capable lld; the one shipped with the Rust toolchain is used by
#     default because it is already present on most developer machines.
#
# No Emscripten, no WASI and no JavaScript glue are generated: the module
# exports a small C ABI that player.js calls directly.
set -euo pipefail

cd "$(dirname "$0")"

CC=${CC:-clang}
OUT=${OUT:-gem6502.wasm}
BUILD=build
ROOT=$(cd .. && pwd)

if ! command -v "$CC" >/dev/null 2>&1; then
  echo "error: C compiler '$CC' not found" >&2
  exit 1
fi

find_rust_lld() {
  local host candidate

  if ! command -v rustc >/dev/null 2>&1; then
    return 1
  fi
  host=$(rustc -vV | sed -n 's/^host: //p')

  candidate="$(rustc --print sysroot)/lib/rustlib/$host/bin/rust-lld"
  if [[ -x "$candidate" ]]; then
    echo "$candidate"
    return 0
  fi

  # Homebrew's rustc does not ship rust-lld; fall back to rustup toolchains.
  for candidate in "$HOME"/.rustup/toolchains/*/lib/rustlib/"$host"/bin/rust-lld; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done
  return 1
}

if [[ -z "${RUST_LLD:-}" ]]; then
  if ! RUST_LLD=$(find_rust_lld); then
    echo "error: no wasm-capable lld found; set RUST_LLD=/path/to/rust-lld" >&2
    exit 1
  fi
fi

if [[ ! -x "$RUST_LLD" ]]; then
  echo "error: wasm linker '$RUST_LLD' not found" >&2
  exit 1
fi

# rust-lld on macOS loads libLLVM.dylib from the toolchain's lib directory.
# Scope this to the linker: exporting it would make clang load the wrong LLVM.
LLD_LIB=$(cd "$(dirname "$RUST_LLD")/../../../.." && pwd)/lib
run_lld() {
  if [[ "$(uname -s)" == "Darwin" ]]; then
    DYLD_LIBRARY_PATH="$LLD_LIB${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" "$RUST_LLD" "$@"
  else
    LD_LIBRARY_PATH="$LLD_LIB${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$RUST_LLD" "$@"
  fi
}

CFLAGS=(
  --target=wasm32-unknown-unknown
  -std=c11
  -O2
  -Wall
  -Wextra
  -nostdlibinc
  -fno-stack-protector
  -I src/shim/include
  -I "$ROOT/include"
)

CORE=(
  "$ROOT/src/gem6502.c"
  "$ROOT/src/ines.c"
  "$ROOT/src/render.c"
  "$ROOT/src/cartridge.c"
  "$ROOT/src/audio.c"
  src/gem_web.c
)

EXPORTS=(
  --export=gem_web_init
  --export=gem_web_destroy
  --export=gem_web_reset
  --export=gem_web_rom_buffer
  --export=gem_web_rom_capacity
  --export=gem_web_load_rom
  --export=gem_web_set_buttons
  --export=gem_web_run_frame
  --export=gem_web_framebuffer
  --export=gem_web_framebuffer_rgba
  --export=gem_web_take_audio
  --export=gem_web_audio_buffer
  --export=gem_web_error
  --export=gem_web_frames
  --export=gem_web_cycles
  --export=gem_web_halted
)

rm -rf "$BUILD"
mkdir -p "$BUILD"

obj() { echo "$BUILD/$(basename "${1%.c}").o"; }

for src in "${CORE[@]}"; do
  echo "  CC  ${src#"$ROOT"/}"
  "$CC" "${CFLAGS[@]}" -c "$src" -o "$(obj "$src")"
done

echo "  CC  src/shim/shim.c"
"$CC" "${CFLAGS[@]}" -fno-builtin -fno-builtin-memcpy -fno-builtin-memset \
  -c src/shim/shim.c -o "$BUILD/shim.o"

echo "  LD  $OUT"
run_lld -flavor wasm \
  --no-entry \
  --gc-sections \
  --strip-debug \
  -z stack-size=131072 \
  "${EXPORTS[@]}" \
  -o "$OUT" \
  "$(obj "$ROOT/src/gem6502.c")" \
  "$(obj "$ROOT/src/ines.c")" \
  "$(obj "$ROOT/src/render.c")" \
  "$(obj "$ROOT/src/cartridge.c")" \
  "$(obj "$ROOT/src/audio.c")" \
  "$BUILD/gem_web.o" \
  "$BUILD/shim.o"

echo "built $OUT ($(wc -c < "$OUT" | tr -d ' ') bytes)"
