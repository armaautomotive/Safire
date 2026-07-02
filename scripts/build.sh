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
  mac|mac-arm|mac-arm64|mac-x86|mac-x86_64|linux|linux2|all|clean)
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

brew_prefix_for_arch() {
  case "$1" in
    arm64) echo "${SAFIRE_HOMEBREW_PREFIX:-/opt/homebrew}" ;;
    x86_64) echo "${SAFIRE_HOMEBREW_PREFIX:-/usr/local}" ;;
    *) echo "Unsupported Mac architecture: $1" >&2; exit 2 ;;
  esac
}

openssl_prefix_for_brew() {
  local brew_prefix="$1"
  if [[ -d "$brew_prefix/opt/openssl" ]]; then
    echo "$brew_prefix/opt/openssl"
  elif [[ -d "$brew_prefix/opt/openssl@3" ]]; then
    echo "$brew_prefix/opt/openssl@3"
  else
    echo "$brew_prefix/opt/openssl"
  fi
}

check_command make

mac_arch="${SAFIRE_MAC_ARCH:-arm64}"
case "$target" in
  mac-arm|mac-arm64)
    target="mac"
    mac_arch="arm64"
    ;;
  mac-x86|mac-x86_64)
    target="mac"
    mac_arch="x86_64"
    ;;
esac

if [[ "$target" == "mac" || "$target" == "all" ]]; then
  if [[ "$target" == "all" ]]; then
    mac_arch="${SAFIRE_MAC_ARCH:-arm64}"
  fi
  brew_prefix="$(brew_prefix_for_arch "$mac_arch")"
  openssl_prefix="$(openssl_prefix_for_brew "$brew_prefix")"
  check_command clang++
  check_command pkg-config
  check_file "$openssl_prefix/lib/libssl.dylib"
  check_file "$openssl_prefix/lib/libcrypto.dylib"
  check_file "$brew_prefix/opt/leveldb/lib/libleveldb.a"
  check_file "$brew_prefix/opt/snappy/lib/libsnappy.dylib"
fi

if [[ "$target" == "mac" || "$target" == "all" ]]; then
  echo "Building Safire ($target mac_arch=$mac_arch)"
else
  echo "Building Safire ($target)"
fi
cd "$PROJECT_ROOT"
if [[ "$target" == "mac" || "$target" == "all" ]]; then
  make "$target" MAC_ARCH="$mac_arch" HOMEBREW_PREFIX="$brew_prefix" OPENSSL_PREFIX="$openssl_prefix" "$@"
else
  make "$target" "$@"
fi
