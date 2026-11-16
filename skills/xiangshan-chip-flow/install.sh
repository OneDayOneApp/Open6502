#!/usr/bin/env bash
# Install this skill into a DSH skills directory.
#   ./install.sh            -> ~/.dsh/skills/xiangshan-chip-flow
#   ./install.sh --repo     -> ./.dsh/skills/xiangshan-chip-flow
set -euo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mode=${1:---user}

case "$mode" in
  --user) dest="$HOME/.dsh/skills" ;;
  --repo) dest="$PWD/.dsh/skills" ;;
  *) echo "usage: $0 [--user|--repo]" >&2; exit 2 ;;
esac

target="$dest/xiangshan-chip-flow"
mkdir -p "$target"
cp -R "$here"/. "$target"/

echo "installed: $target"
echo "restart the session (or reload skills) and ask for 'xiangshan-chip-flow'."
