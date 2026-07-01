#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST_DIR="$PROJECT_ROOT/dist/linux"
WORK_DIR="$DIST_DIR/work"
PACKAGE_VERSION="${SAFIRE_PACKAGE_VERSION:-$(git -C "$PROJECT_ROOT" describe --tags --always --dirty 2>/dev/null || date +%Y%m%d%H%M%S)}"
PACKAGE_NAME="safire-linux-${PACKAGE_VERSION}"
CORE_BINARY="$PROJECT_ROOT/bin/Safire"
CONFIG_FILE="${SAFIRE_CONF:-$PROJECT_ROOT/safire.conf}"
SKIP_BUILD=0
UPLOAD_TARGET="${SAFIRE_UPLOAD_TARGET:-}"

usage() {
  cat <<'USAGE'
Usage: scripts/package-linux.sh [options]

Builds a Linux/server distribution tarball under dist/linux.

Options:
  --skip-build              Use existing bin/Safire
  --upload-target TARGET    Upload tarball with scp
  --version VERSION         Override package version
  -h, --help                Show this help

Environment:
  SAFIRE_CONF               safire.conf to bundle
  SAFIRE_UPLOAD_TARGET      scp upload target
  SAFIRE_PACKAGE_VERSION    package version label
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-build) SKIP_BUILD=1; shift ;;
    --upload-target) UPLOAD_TARGET="$2"; shift 2 ;;
    --version) PACKAGE_VERSION="$2"; PACKAGE_NAME="safire-linux-${PACKAGE_VERSION}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  "$PROJECT_ROOT/scripts/build.sh" linux
fi

if [[ ! -x "$CORE_BINARY" ]]; then
  echo "Missing core binary: $CORE_BINARY" >&2
  exit 1
fi
if [[ ! -f "$CONFIG_FILE" ]]; then
  echo "Missing config file: $CONFIG_FILE" >&2
  exit 1
fi

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/$PACKAGE_NAME/bin" "$WORK_DIR/$PACKAGE_NAME/scripts" "$DIST_DIR"

cp "$CORE_BINARY" "$WORK_DIR/$PACKAGE_NAME/bin/Safire"
chmod +x "$WORK_DIR/$PACKAGE_NAME/bin/Safire"
cp "$CONFIG_FILE" "$WORK_DIR/$PACKAGE_NAME/safire.conf"
cp "$PROJECT_ROOT/LICENSE" "$WORK_DIR/$PACKAGE_NAME/LICENSE"
cp "$PROJECT_ROOT/README.md" "$WORK_DIR/$PACKAGE_NAME/README.md"
cp "$PROJECT_ROOT/scripts/server-start.sh" "$WORK_DIR/$PACKAGE_NAME/scripts/server-start.sh"
cp "$PROJECT_ROOT/scripts/server-stop.sh" "$WORK_DIR/$PACKAGE_NAME/scripts/server-stop.sh"
cp "$PROJECT_ROOT/scripts/server-status.sh" "$WORK_DIR/$PACKAGE_NAME/scripts/server-status.sh"
cp "$PROJECT_ROOT/scripts/server-restart.sh" "$WORK_DIR/$PACKAGE_NAME/scripts/server-restart.sh"
chmod +x "$WORK_DIR/$PACKAGE_NAME/scripts/"*.sh

TARBALL="$DIST_DIR/${PACKAGE_NAME}.tar.gz"
LATEST_TARBALL="$DIST_DIR/safire-linux-latest.tar.gz"
rm -f "$TARBALL"
(
  cd "$WORK_DIR"
  tar -czf "$TARBALL" "$PACKAGE_NAME"
)
cp "$TARBALL" "$LATEST_TARBALL"

if [[ -n "$UPLOAD_TARGET" ]]; then
  scp "$TARBALL" "$LATEST_TARBALL" "$UPLOAD_TARGET"
fi

echo "Created:"
echo "  $TARBALL"
echo "  $LATEST_TARBALL"
