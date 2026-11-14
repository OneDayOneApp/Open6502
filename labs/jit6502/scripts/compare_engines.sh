#!/bin/sh
# End-to-end check for the JIT lab.
#
#   1. Klaus must pass on the interpreter and on both JIT modes.
#   2. Both JIT modes must produce bit-identical state for a fixed instruction
#      budget (unchained mode keeps the budget exact).
#   3. The random differential tester must agree on state, cycles and memory.
#   4. Benchmarks print interpreter vs JIT throughput.
set -eu

BIN=./bin/jit6502
KLAUS=${KLAUS:-/tmp/klaus.bin}
fail=0

note() { printf '\n== %s\n' "$*"; }
bad() { printf 'FAIL: %s\n' "$*"; fail=1; }

if [ ! -f "$KLAUS" ]; then
  echo "downloading Klaus functional test to $KLAUS ..."
  curl -fsSL -o "$KLAUS" \
    https://raw.githubusercontent.com/Klaus2m5/6502_65C02_functional_tests/master/bin_files/6502_functional_test.bin
fi

note "Klaus functional test"
for mode in interp jit-no-chain jit-chain; do
  case $mode in
    interp) args="--engine=interp" ;;
    jit-no-chain) args="--engine=jit --no-chain" ;;
    jit-chain) args="--engine=jit --chain" ;;
  esac
  out=$($BIN klaus "$KLAUS" $args)
  echo "$out" | grep -q 'result = PASS' || bad "klaus $mode"
  echo "$out" | grep -E 'result|inst'
done

note "State equality at a fixed instruction budget"
for n in 100000 1000000 5000000; do
  a=$($BIN run "$KLAUS" --entry=0x400 --max-inst=$n --quiet --engine=interp | grep '^state')
  b=$($BIN run "$KLAUS" --entry=0x400 --max-inst=$n --quiet --engine=jit --no-chain | grep '^state')
  if [ "$a" = "$b" ]; then
    echo "N=$n identical: $a"
  else
    bad "state differs at N=$n"
    echo "  interp: $a"
    echo "  jit:    $b"
  fi
done

note "Random differential test"
$BIN selfcheck 20000 || bad "selfcheck"

note "Benchmarks (20M instructions each)"
for b in arith_loop copy_loop call_loop; do
  [ -f "bench/$b.bin" ] || python3 tools/make_bench.py bench >/dev/null
  i=$($BIN bench "bench/$b.bin" --engine=interp --max-inst=20000000 --repeat=3 --code=8000-10000 | awk '/speed/{print $3}')
  j=$($BIN bench "bench/$b.bin" --engine=jit --max-inst=20000000 --repeat=3 --code=8000-10000 | awk '/speed/{print $3}')
  printf '%-12s interp %8s MIPS   jit %8s MIPS\n' "$b" "$i" "$j"
done

note "NES: interpreter vs JIT frames"
ROM=../../roms/falling.nes
if [ -f "$ROM" ]; then
  $BIN nes "$ROM" --frames=60 --screenshot=/tmp/nes-interp.ppm --engine=interp >/dev/null
  $BIN nes "$ROM" --frames=60 --screenshot=/tmp/nes-jit.ppm --engine=jit >/dev/null
  $BIN nes "$ROM" --frames=120 --wav=/tmp/nes-audio.wav --engine=jit >/dev/null
  python3 - <<'PYEOF'
import wave
w = wave.open('/tmp/nes-audio.wav')
n = w.getnframes()
data = w.readframes(n)
print(f"audio: {n} samples, {len(data)} bytes", "OK" if n > 50000 else "EMPTY")
PYEOF
  if cmp -s /tmp/nes-interp.ppm /tmp/nes-jit.ppm; then
    echo "falling.nes frame 60: identical ($(shasum -a 256 /tmp/nes-jit.ppm | cut -c1-16)...)"
  else
    bad "NES frame mismatch between interp and jit"
  fi
else
  echo "skip: $ROM not found"
fi

note "GEM6502 DiffTest (trace + checkpoints)"
if [ -x ../../GEM6502/gem6502 ]; then
  if ! ./scripts/difftest_gem6502.sh >/tmp/difftest-gem.log 2>&1; then
    bad "GEM6502 difftest"
    tail -20 /tmp/difftest-gem.log
  else
    grep -E 'PASS|DIFFTEST' /tmp/difftest-gem.log
  fi
else
  echo "skip: build GEM6502 first (make -C ../../GEM6502)"
fi

if [ "$fail" -ne 0 ]; then
  echo "\nSOME CHECKS FAILED"
  exit 1
fi
echo "\nALL CHECKS PASSED"
