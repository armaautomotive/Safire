#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

target="${1:-}"
if [[ $# -gt 0 ]]; then
  shift
fi

case "${target:-}" in
  ""|default)
    case "${OSTYPE:-}" in
      darwin*) target="mac" ;;
      linux-gnu*) target="linux" ;;
      *) target="all" ;;
    esac
    ;;
  mac|linux|linux2|all|clean)
    ;;
  *)
    echo "Unknown build target: $target" >&2
    echo "Usage: scripts/build.sh [mac|linux|linux2|all|clean] [make args...]" >&2
    exit 2
    ;;
esac

check_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required command: $1" >&2
    exit 1
  fi
}

check_file() {
  if [[ ! -e "$1" ]]; then
    echo "Missing required dependency: $1" >&2
    exit 1
  fi
}

check_command make

if [[ "$target" == "mac" || "$target" == "all" ]]; then
  check_command clang++
  check_command pkg-config
  check_file /usr/local/opt/openssl/lib/libssl.dylib
  check_file /usr/local/opt/openssl/lib/libcrypto.dylib
  check_file /usr/local/opt/leveldb/lib/libleveldb.a
  check_file /usr/local/opt/snappy/lib/libsnappy.dylib
fi

echo "Building Safire ($target)"
cd "$PROJECT_ROOT"
make "$target" "$@"
