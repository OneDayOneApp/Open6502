#!/bin/sh
# Basic regression tests for 6502EMU.  Set RUN_KLAUS=1 to also run Klaus
# Dormann's exhaustive 6502 functional test (network required for the first
# run).  Set KLAUS_BIN=/path/to/6502_functional_test.bin to reuse a local copy.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
EMU="$ROOT/build/6502EMU"
TMP=${TMPDIR:-/tmp}/6502emu-tests
mkdir -p "$TMP"

if [ ! -x "$EMU" ]; then
  echo "error: $EMU not found; build 6502EMU first" >&2
  exit 2
fi

# Always test the built-in program.
out=$("$EMU" -b --log="$TMP/builtin.log" 2>&1)
printf '%s\n' "$out" | grep -q "Hello from 6502EMU!"
echo "PASS: built-in image"

# Assemble and run a user program.
make -C "$ROOT/programs" hello.bin >/dev/null
out=$("$EMU" -b --log="$TMP/hello.log" "$ROOT/programs/hello.bin" 2>&1)
printf '%s\n' "$out" | grep -q "Hello, 6502EMU!"
echo "PASS: hello.bin"

# A raw image loaded at a non-default address and entered at 0x600.
cat > "$TMP/entry.s" <<'ASM'
.setcpu "6502"
.segment "CODE"
start:
    lda #0
    sta $F002
hang:
    jmp hang
ASM
cat > "$TMP/entry.cfg" <<'CFG'
MEMORY { RAM: start = $0600, size = $1000, file = %O; }
SEGMENTS { CODE: load = RAM, type = ro, start = $0600; }
CFG
ca65 "$TMP/entry.s" -o "$TMP/entry.o"
ld65 -C "$TMP/entry.cfg" -o "$TMP/entry.bin" "$TMP/entry.o"
"$EMU" -b --load-addr=0x0600 --pc=0x0600 --log="$TMP/entry.log" "$TMP/entry.bin" >/dev/null 2>&1
echo "PASS: custom load address"

# Microsoft BASIC 1.1, bundled as a raw image at $2000.
if [ -f "$ROOT/basic/msbasic.bin" ]; then
  out=$("$EMU" -b --load-addr=0x2000 --pc=0x4058 --max-inst=50000000 \
        --log="$TMP/basic.log" "$ROOT/basic/msbasic.bin" < "$ROOT/basic/demo.in" 2>&1)
  printf '%s\n' "$out" | grep -q "HELLO FROM BASIC"
  printf '%s\n' "$out" | grep -q " 25 "
  echo "PASS: Microsoft BASIC 1.1"
else
  echo "SKIP: basic/msbasic.bin not found"
fi

# 65C02 extension instructions
ca65 "$ROOT/tests/65c02.s" -o "$TMP/65c02.o"
ld65 -C "$ROOT/programs/6502.cfg" "$TMP/65c02.o" -o "$TMP/65c02.bin"
out=$("$EMU" -b --65c02 --log="$TMP/65c02.log" "$TMP/65c02.bin" 2>&1)
printf '%s\n' "$out" | grep -q "HIT GOOD TRAP"
echo "PASS: 65C02 extensions"

# NES golden-reference primitives (clock, ROM, state, NROM bus)
if [ -x "$ROOT/tests/nes_ref/run.sh" ]; then
  if [ "${RUN_KLAUS:-0}" = "1" ]; then
    RUN_KLAUS=1 KLAUS_BIN="${KLAUS_BIN:-}" "$ROOT/tests/nes_ref/run.sh"
  else
    "$ROOT/tests/nes_ref/run.sh"
  fi
fi

# NES frontend, PPU, input, and APU
if [ -f "$ROOT/nesgame/game.nes" ]; then
  "$EMU" --headless --no-audio --frames=180 --screenshot="$TMP/nes-a.ppm" \
    --wav="$TMP/nes.wav" "$ROOT/nesgame/game.nes" >/dev/null 2>&1
  "$EMU" --headless --no-audio --frames=180 --hold=right \
    --screenshot="$TMP/nes-b.ppm" "$ROOT/nesgame/game.nes" >/dev/null 2>&1
  test -s "$TMP/nes-a.ppm"
  test -s "$TMP/nes.wav"
  h1=$("$EMU" --headless --no-audio --frames=180 --hash "$ROOT/nesgame/game.nes" 2>&1 | grep '^6502EMU NES hash:')
  h2=$("$EMU" --headless --no-audio --frames=180 --hash "$ROOT/nesgame/game.nes" 2>&1 | grep '^6502EMU NES hash:')
  h3=$("$EMU" --headless --no-audio --frames=180 --hash --hold=right "$ROOT/nesgame/game.nes" 2>&1 | grep '^6502EMU NES hash:')
  test -n "$h1"
  test "$h1" = "$h2"
  test "$h1" != "$h3"
  # save at frame 60, resume for 60 more frames, compare with an
  # uninterrupted 120-frame run (ignore the frames= counter).
  state="$TMP/nes.state"
  "$EMU" --headless --no-audio --frames=60 --hash --save-state="$state"     "$ROOT/nesgame/game.nes" >/dev/null 2>&1
  resumed=$("$EMU" --headless --no-audio --frames=60 --hash --load-state="$state"     "$ROOT/nesgame/game.nes" 2>&1 | grep '^6502EMU NES hash:' | sed 's/ frames=.*//')
  uninterrupted=$("$EMU" --headless --no-audio --frames=120 --hash     "$ROOT/nesgame/game.nes" 2>&1 | grep '^6502EMU NES hash:' | sed 's/ frames=.*//')
  test "$resumed" = "$uninterrupted"
  python3 - "$TMP/nes.wav" <<'PYWAV'
import struct, sys, wave
with wave.open(sys.argv[1], "rb") as w:
    data = w.readframes(w.getnframes())
assert data, "empty APU WAV"
samples = struct.unpack("<%dh" % (len(data) // 2), data)
assert max(abs(x) for x in samples) > 0, "APU produced only silence"
PYWAV
  if cmp -s "$TMP/nes-a.ppm" "$TMP/nes-b.ppm"; then
    echo "FAIL: NES controller input did not change the frame" >&2
    exit 1
  fi
  echo "PASS: NES PPU/input/APU"
else
  echo "SKIP: nesgame/game.nes not found"
fi

if [ "${RUN_KLAUS:-0}" = "1" ]; then
  KLAUS=${KLAUS_BIN:-$TMP/6502_functional_test.bin}
  if [ ! -f "$KLAUS" ]; then
    echo "Downloading Klaus 6502 functional test..."
    curl -L --fail -o "$KLAUS" \
      https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/bin_files/6502_functional_test.bin
  fi
  # The test reports success by spinning at 0x3469.  A breakpoint there keeps
  # the regression run short; all errors spin at other addresses.
  set +e
  out=$("$EMU" -b --load-addr=0x0 --pc=0x400 --max-inst=200000000 \
        --stop-pc=0x3469 --log="$TMP/klaus.log" "$KLAUS" 2>&1)
  set -e
  printf '%s\n' "$out" | grep -q "PC:3469"
  echo "PASS: Klaus 6502 functional test"
fi

echo "All selected tests passed."
