#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"$SCRIPT_DIR/server-stop.sh"
"$SCRIPT_DIR/build.sh" linux
"$SCRIPT_DIR/server-start.sh" "$@"
