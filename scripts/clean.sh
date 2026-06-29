#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"
rm -f bin/Safire
rm -f src/*.o gui_client/*.o gui_client/moc_*.cpp gui_client/moc_*.o gui_client/moc_predefs.h
rm -rf build

echo "Cleaned build artifacts."
