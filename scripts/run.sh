#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY="$PROJECT_ROOT/bin/Safire"

if [[ ! -x "$BINARY" ]]; then
  echo "Safire binary not found. Building first..." >&2
  "$SCRIPT_DIR/build.sh"
fi

case "${OSTYPE:-}" in
  darwin*) echo "Starting Safire (macOS)." ;;
  linux-gnu*) echo "Starting Safire (Linux)." ;;
  *) echo "Starting Safire." ;;
esac

cd "$PROJECT_ROOT"
exec "$BINARY" "$@"
