#!/bin/bash
# Launch the nesearch local UI with the bundled emulators.
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export NESEARCH_GEM6502="$DIR/emulators/gem6502"
export NESEARCH_NEMU="$DIR/emulators/6502EMU"
exec "$DIR/nesearch" ui --ui-dir "$DIR/ui" "$@"
