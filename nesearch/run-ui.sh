#!/bin/sh
# Launch the local UI with workspace-local config/cache/download paths.
# Use this in sandboxed environments where ~/Downloads is not writable.
set -eu
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$DIR"
export NESEARCH_CONFIG="$DIR/config.sandbox.json"
export NESEARCH_CACHE_DIR="$DIR/.runtime/cache"
export NESEARCH_DATA_DIR="$DIR/.runtime/data"
exec "$DIR/build/nesearch" ui "$@"
