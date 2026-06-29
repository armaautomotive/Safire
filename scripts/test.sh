#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

"$SCRIPT_DIR/build.sh"

echo "Smoke testing Safire startup and shutdown"
cd "$PROJECT_ROOT"
printf 'quit\n' | "$PROJECT_ROOT/bin/Safire"
