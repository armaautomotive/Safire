#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST_DIR="$PROJECT_ROOT/dist/mac"
WORK_DIR="$DIST_DIR/work"
APP_NAME="Safire"
APP_SOURCE="$PROJECT_ROOT/gui_client/Safire.app"
CORE_BINARY="$PROJECT_ROOT/bin/Safire"
CONFIG_FILE="${SAFIRE_CONF:-$PROJECT_ROOT/safire.conf}"
PACKAGE_VERSION="${SAFIRE_PACKAGE_VERSION:-$(git -C "$PROJECT_ROOT" describe --tags --always --dirty 2>/dev/null || date +%Y%m%d%H%M%S)}"
SIGN=0
NOTARIZE=0
SKIP_BUILD=0
UPLOAD_TARGET="${SAFIRE_UPLOAD_TARGET:-}"
APPLE_SIGN_IDENTITY="${APPLE_SIGN_IDENTITY:-Developer ID Application: Subject Reality Software (XSDMH4B293)}"
APPLE_NOTARY_PROFILE="${APPLE_NOTARY_PROFILE:-safire-notary}"

usage() {
  cat <<'USAGE'
Usage: scripts/package-mac.sh [options]

Builds a distributable Safire.app, zip, and dmg under dist/mac.

Options:
  --skip-build              Use existing bin/Safire and gui_client/Safire.app
  --sign                    Codesign the app using APPLE_SIGN_IDENTITY
  --notarize                Submit the dmg using APPLE_NOTARY_PROFILE
  --upload-target TARGET    Upload zip and dmg with scp, e.g. root@host:/var/www/download
  --version VERSION         Override package version
  -h, --help                Show this help

Environment:
  APPLE_SIGN_IDENTITY       Developer ID identity for codesign
  APPLE_NOTARY_PROFILE      notarytool keychain profile
  SAFIRE_CONF               safire.conf to bundle
  SAFIRE_UPLOAD_TARGET      scp upload target
  SAFIRE_PACKAGE_VERSION    package version label
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-build) SKIP_BUILD=1; shift ;;
    --sign) SIGN=1; shift ;;
    --notarize) NOTARIZE=1; SIGN=1; shift ;;
    --upload-target) UPLOAD_TARGET="$2"; shift 2 ;;
    --version) PACKAGE_VERSION="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

find_macdeployqt() {
  if command -v macdeployqt >/dev/null 2>&1; then
    command -v macdeployqt
    return
  fi
  local candidates=(
    "/usr/local/opt/qtbase/bin/macdeployqt"
    "/usr/local/opt/qt/bin/macdeployqt"
    "/opt/homebrew/opt/qtbase/bin/macdeployqt"
    "/opt/homebrew/opt/qt/bin/macdeployqt"
  )
  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return
    fi
  done
  for candidate in /usr/local/Cellar/qtbase/*/bin/macdeployqt /opt/homebrew/Cellar/qtbase/*/bin/macdeployqt; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return
    fi
  done
  return 1
}

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  "$PROJECT_ROOT/scripts/build.sh" mac
  "$PROJECT_ROOT/scripts/gui.sh"
fi

if [[ ! -x "$CORE_BINARY" ]]; then
  echo "Missing core binary: $CORE_BINARY" >&2
  exit 1
fi
if [[ ! -d "$APP_SOURCE" ]]; then
  echo "Missing GUI app: $APP_SOURCE" >&2
  exit 1
fi
if [[ ! -f "$CONFIG_FILE" ]]; then
  echo "Missing config file: $CONFIG_FILE" >&2
  exit 1
fi

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR" "$DIST_DIR"
APP_STAGE="$WORK_DIR/$APP_NAME.app"
cp -R "$APP_SOURCE" "$APP_STAGE"

MACDEPLOYQT="$(find_macdeployqt || true)"
if [[ -n "$MACDEPLOYQT" ]]; then
  "$MACDEPLOYQT" "$APP_STAGE"
else
  echo "Warning: macdeployqt not found. The app may require Qt on the target Mac." >&2
fi

mkdir -p "$APP_STAGE/Contents/Resources/bin"
cp "$CORE_BINARY" "$APP_STAGE/Contents/Resources/bin/Safire"
chmod +x "$APP_STAGE/Contents/Resources/bin/Safire"
cp "$CONFIG_FILE" "$APP_STAGE/Contents/Resources/safire.conf"

if [[ "$SIGN" -eq 1 ]]; then
  if ! security find-identity -v -p codesigning | grep -F "$APPLE_SIGN_IDENTITY" >/dev/null 2>&1; then
    echo "Codesign identity not found: $APPLE_SIGN_IDENTITY" >&2
    exit 1
  fi
  codesign --force --options runtime --timestamp --sign "$APPLE_SIGN_IDENTITY" "$APP_STAGE/Contents/Resources/bin/Safire"
  codesign --force --deep --options runtime --timestamp --sign "$APPLE_SIGN_IDENTITY" "$APP_STAGE"
  codesign --verify --deep --strict --verbose=2 "$APP_STAGE"
fi

ZIP_FILE="$DIST_DIR/${APP_NAME}-mac-${PACKAGE_VERSION}.zip"
DMG_ROOT="$WORK_DIR/dmg-root"
DMG_FILE="$DIST_DIR/${APP_NAME}-mac-${PACKAGE_VERSION}.dmg"
LATEST_ZIP_FILE="$DIST_DIR/${APP_NAME}-mac-latest.zip"
LATEST_DMG_FILE="$DIST_DIR/${APP_NAME}-mac-latest.dmg"

rm -f "$ZIP_FILE" "$DMG_FILE"
(
  cd "$WORK_DIR"
  ditto -c -k --sequesterRsrc --keepParent "$APP_NAME.app" "$ZIP_FILE"
)

mkdir -p "$DMG_ROOT"
cp -R "$APP_STAGE" "$DMG_ROOT/$APP_NAME.app"
ln -s /Applications "$DMG_ROOT/Applications"
hdiutil create -volname "$APP_NAME" -srcfolder "$DMG_ROOT" -ov -format UDZO "$DMG_FILE"

cp "$ZIP_FILE" "$LATEST_ZIP_FILE"
cp "$DMG_FILE" "$LATEST_DMG_FILE"

if [[ "$NOTARIZE" -eq 1 ]]; then
  xcrun notarytool submit "$DMG_FILE" --keychain-profile "$APPLE_NOTARY_PROFILE" --wait
  xcrun stapler staple "$DMG_FILE"
fi

if [[ -n "$UPLOAD_TARGET" ]]; then
  scp "$ZIP_FILE" "$DMG_FILE" "$LATEST_ZIP_FILE" "$LATEST_DMG_FILE" "$UPLOAD_TARGET"
fi

echo "Created:"
echo "  $ZIP_FILE"
echo "  $DMG_FILE"
echo "  $LATEST_ZIP_FILE"
echo "  $LATEST_DMG_FILE"
