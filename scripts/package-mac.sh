#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DIST_DIR="$PROJECT_ROOT/dist/mac"
WORK_DIR_BASE="$DIST_DIR/work"
APP_NAME="Safire"
APP_SOURCE="$PROJECT_ROOT/gui_client/Safire.app"
CORE_BINARY="$PROJECT_ROOT/bin/Safire"
CONFIG_FILE="${SAFIRE_CONF:-$PROJECT_ROOT/safire.conf}"
PACKAGE_VERSION="${SAFIRE_PACKAGE_VERSION:-$(git -C "$PROJECT_ROOT" describe --tags --always --dirty 2>/dev/null || date +%Y%m%d%H%M%S)}"
BUNDLE_ID="${SAFIRE_BUNDLE_ID:-org.safire.wallet}"
MAC_ARCH="${SAFIRE_MAC_ARCH:-arm64}"
SIGN=0
NOTARIZE=0
SKIP_BUILD=0
UPLOAD_TARGET="${SAFIRE_UPLOAD_TARGET:-}"
SAFIRE_ORG_UPLOAD_TARGET="${SAFIRE_ORG_UPLOAD_TARGET:-root@safire.org:/var/www/html/}"
APPLE_SIGN_IDENTITY="${APPLE_SIGN_IDENTITY:-Developer ID Application: Subject Reality Software (XSDMH4B293)}"
APPLE_NOTARY_PROFILE="${APPLE_NOTARY_PROFILE:-ads-notary}"

usage() {
  cat <<'USAGE'
Usage: scripts/package-mac.sh [options]

Builds a distributable Safire.app, zip, and dmg under dist/mac.

Options:
  --skip-build              Use existing bin/Safire and gui_client/Safire.app
  --sign                    Codesign the app using APPLE_SIGN_IDENTITY
  --notarize                Submit the dmg using APPLE_NOTARY_PROFILE
  --upload-target TARGET    Upload zip and dmg with scp, e.g. root@host:/var/www/download
  --deploy-safire-org       Build arm64 and upload zip/dmg to safire.org
  --version VERSION         Override package version
  --arch ARCH               Build architecture: arm64 or x86_64. Default: arm64
  -h, --help                Show this help

Environment:
  APPLE_SIGN_IDENTITY       Developer ID identity for codesign
  APPLE_NOTARY_PROFILE      notarytool keychain profile
  SAFIRE_BUNDLE_ID          bundle identifier, default org.safire.wallet
  SAFIRE_CONF               safire.conf to bundle
  SAFIRE_UPLOAD_TARGET      scp upload target
  SAFIRE_ORG_UPLOAD_TARGET  safire.org scp target, default root@safire.org:/var/www/html/
  SAFIRE_PACKAGE_VERSION    package version label
  SAFIRE_MAC_ARCH           Build architecture, default arm64
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --skip-build) SKIP_BUILD=1; shift ;;
    --sign) SIGN=1; shift ;;
    --notarize) NOTARIZE=1; SIGN=1; shift ;;
    --upload-target) UPLOAD_TARGET="$2"; shift 2 ;;
    --deploy-safire-org) MAC_ARCH="arm64"; UPLOAD_TARGET="$SAFIRE_ORG_UPLOAD_TARGET"; shift ;;
    --version) PACKAGE_VERSION="$2"; shift 2 ;;
    --arch) MAC_ARCH="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

case "$MAC_ARCH" in
  arm64|x86_64) ;;
  *) echo "Unsupported Mac architecture: $MAC_ARCH" >&2; exit 2 ;;
esac

WORK_DIR="$WORK_DIR_BASE-$MAC_ARCH"

brew_prefix_for_arch() {
  if [[ "$MAC_ARCH" == "arm64" ]]; then
    echo "${SAFIRE_HOMEBREW_PREFIX:-/opt/homebrew}"
  else
    echo "${SAFIRE_HOMEBREW_PREFIX:-/usr/local}"
  fi
}

find_macdeployqt() {
  local candidates=()
  if [[ "$MAC_ARCH" == "arm64" ]]; then
    candidates+=(
      "/opt/homebrew/opt/qtbase/bin/macdeployqt"
      "/opt/homebrew/opt/qt/bin/macdeployqt"
    )
  else
    candidates+=(
      "/usr/local/opt/qtbase/bin/macdeployqt"
      "/usr/local/opt/qt/bin/macdeployqt"
    )
  fi
  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return
    fi
  done
  if [[ "$MAC_ARCH" == "arm64" ]]; then
    for candidate in /opt/homebrew/Cellar/qtbase/*/bin/macdeployqt; do
      if [[ -x "$candidate" ]]; then
        echo "$candidate"
        return
      fi
    done
  else
    for candidate in /usr/local/Cellar/qtbase/*/bin/macdeployqt; do
      if [[ -x "$candidate" ]]; then
        echo "$candidate"
        return
      fi
    done
  fi
  if command -v macdeployqt >/dev/null 2>&1; then
    command -v macdeployqt
    return
  fi
  return 1
}

find_qt_plugins() {
  local candidates=()
  if [[ "$MAC_ARCH" == "arm64" ]]; then
    candidates+=(
      "/opt/homebrew/opt/qtbase/share/qt/plugins"
      "/opt/homebrew/opt/qt/share/qt/plugins"
    )
  else
    candidates+=(
      "/usr/local/opt/qtbase/share/qt/plugins"
      "/usr/local/opt/qt/share/qt/plugins"
    )
  fi

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -f "$candidate/platforms/libqcocoa.dylib" ]]; then
      echo "$candidate"
      return
    fi
  done

  if [[ "$MAC_ARCH" == "arm64" ]]; then
    for candidate in /opt/homebrew/Cellar/qtbase/*/share/qt/plugins; do
      if [[ -f "$candidate/platforms/libqcocoa.dylib" ]]; then
        echo "$candidate"
        return
      fi
    done
  else
    for candidate in /usr/local/Cellar/qtbase/*/share/qt/plugins; do
      if [[ -f "$candidate/platforms/libqcocoa.dylib" ]]; then
        echo "$candidate"
        return
      fi
    done
  fi

  return 1
}

is_system_dylib() {
  case "$1" in
    /usr/lib/*|/System/*|@*) return 0 ;;
    *) return 1 ;;
  esac
}

require_binary_arch() {
  local binary="$1"
  if ! file "$binary" | grep -q "$MAC_ARCH"; then
    echo "Expected $MAC_ARCH binary, got:" >&2
    file "$binary" >&2
    exit 1
  fi
}

copy_dylib_dependency() {
  local dep="$1"
  local lib_dir="$2"
  local dest="$lib_dir/$(basename "$dep")"

  if [[ ! -f "$dep" ]]; then
    echo "Missing dylib dependency: $dep" >&2
    exit 1
  fi

  if [[ ! -f "$dest" ]]; then
    mkdir -p "$lib_dir"
    cp -p "$dep" "$dest"
    chmod u+w "$dest"
    install_name_tool -id "@rpath/$(basename "$dest")" "$dest" || true
  fi
}

find_dylib_by_basename() {
  local name="$1"
  local candidate
  local brew_prefix
  brew_prefix="$(brew_prefix_for_arch)"

  for candidate in "$brew_prefix"/opt/*/lib/"$name" "$brew_prefix"/lib/"$name"; do
    if [[ -f "$candidate" ]]; then
      echo "$candidate"
      return
    fi
  done

  return 1
}

patch_dylib_dependencies() {
  local target="$1"
  local lib_dir="$2"
  local mode="$3"
  local dep
  local local_name
  local found_dep
  local replacement_prefix

  if [[ "$mode" == "main" ]]; then
    replacement_prefix="@executable_path/../lib"
  else
    replacement_prefix="@loader_path"
  fi

  while IFS= read -r dep; do
    if [[ -z "$dep" ]]; then
      continue
    fi
    if [[ "$dep" == @loader_path/* || "$dep" == @rpath/* ]]; then
      local_name="$(basename "$dep")"
      if [[ ! -f "$lib_dir/$local_name" ]]; then
        found_dep="$(find_dylib_by_basename "$local_name" || true)"
        if [[ -n "$found_dep" ]]; then
          copy_dylib_dependency "$found_dep" "$lib_dir"
        fi
      fi
      if [[ -f "$lib_dir/$local_name" ]]; then
        install_name_tool -change "$dep" "$replacement_prefix/$local_name" "$target" || true
      fi
      continue
    fi
    if is_system_dylib "$dep"; then
      continue
    fi
    if [[ "$dep" != /* ]]; then
      continue
    fi
    copy_dylib_dependency "$dep" "$lib_dir"
    install_name_tool -change "$dep" "$replacement_prefix/$(basename "$dep")" "$target" || true
  done < <(otool -L "$target" | awk 'NR > 1 {print $1}')
}

bundle_binary_dependencies() {
  local binary="$1"
  local lib_dir="$2"
  local previous_count=-1
  local current_count=0
  local pass
  local dylib

  patch_dylib_dependencies "$binary" "$lib_dir" main

  for pass in {1..30}; do
    while IFS= read -r dylib; do
      patch_dylib_dependencies "$dylib" "$lib_dir" dylib
    done < <(find "$lib_dir" -type f -name '*.dylib' -print 2>/dev/null | sort)

    current_count="$(find "$lib_dir" -type f -name '*.dylib' -print 2>/dev/null | wc -l | tr -d ' ')"
    if [[ "$current_count" == "$previous_count" ]]; then
      break
    fi
    previous_count="$current_count"
  done
}

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  SAFIRE_MAC_ARCH="$MAC_ARCH" "$PROJECT_ROOT/scripts/build.sh" mac
  SAFIRE_MAC_ARCH="$MAC_ARCH" "$PROJECT_ROOT/scripts/gui.sh" --arch "$MAC_ARCH"
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
/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $BUNDLE_ID" "$APP_STAGE/Contents/Info.plist"

MACDEPLOYQT="$(find_macdeployqt || true)"
if [[ -n "$MACDEPLOYQT" ]]; then
  require_binary_arch "$MACDEPLOYQT"
  "$MACDEPLOYQT" "$APP_STAGE"
else
  echo "Warning: macdeployqt not found. The app may require Qt on the target Mac." >&2
fi

if [[ ! -f "$APP_STAGE/Contents/PlugIns/platforms/libqcocoa.dylib" ]]; then
  QT_PLUGINS="$(find_qt_plugins || true)"
  if [[ -z "$QT_PLUGINS" ]]; then
    echo "Missing Qt macOS platform plugin: libqcocoa.dylib" >&2
    exit 1
  fi
  mkdir -p "$APP_STAGE/Contents/PlugIns"
  cp -R "$QT_PLUGINS/platforms" "$APP_STAGE/Contents/PlugIns/platforms"
fi

if [[ ! -f "$APP_STAGE/Contents/PlugIns/platforms/libqcocoa.dylib" ]]; then
  echo "Packaged app is missing Contents/PlugIns/platforms/libqcocoa.dylib" >&2
  exit 1
fi

require_binary_arch "$APP_STAGE/Contents/MacOS/$APP_NAME"
require_binary_arch "$CORE_BINARY"

mkdir -p "$APP_STAGE/Contents/Resources/bin"
cp "$CORE_BINARY" "$APP_STAGE/Contents/Resources/bin/Safire"
chmod +x "$APP_STAGE/Contents/Resources/bin/Safire"
bundle_binary_dependencies "$APP_STAGE/Contents/Resources/bin/Safire" "$APP_STAGE/Contents/Resources/lib"
require_binary_arch "$APP_STAGE/Contents/Resources/bin/Safire"
cp "$CONFIG_FILE" "$APP_STAGE/Contents/Resources/safire.conf"

if [[ "$SIGN" -eq 1 ]]; then
  if ! security find-identity -v -p codesigning | grep -F "$APPLE_SIGN_IDENTITY" >/dev/null 2>&1; then
    echo "Codesign identity not found: $APPLE_SIGN_IDENTITY" >&2
    exit 1
  fi
  if [[ -d "$APP_STAGE/Contents/Resources/lib" ]]; then
    while IFS= read -r dylib; do
      codesign --force --options runtime --timestamp --sign "$APPLE_SIGN_IDENTITY" "$dylib"
    done < <(find "$APP_STAGE/Contents/Resources/lib" -type f -name '*.dylib' -print | sort)
  fi
  codesign --force --options runtime --timestamp --sign "$APPLE_SIGN_IDENTITY" "$APP_STAGE/Contents/Resources/bin/Safire"
  codesign --force --deep --options runtime --timestamp --sign "$APPLE_SIGN_IDENTITY" "$APP_STAGE"
  codesign --verify --deep --strict --verbose=2 "$APP_STAGE"
fi

ZIP_FILE="$DIST_DIR/${APP_NAME}-mac-${MAC_ARCH}-${PACKAGE_VERSION}.zip"
DMG_ROOT="$WORK_DIR/dmg-root"
DMG_FILE="$DIST_DIR/${APP_NAME}-mac-${MAC_ARCH}-${PACKAGE_VERSION}.dmg"
LATEST_ZIP_FILE="$DIST_DIR/${APP_NAME}-mac-latest.zip"
LATEST_DMG_FILE="$DIST_DIR/${APP_NAME}-mac-latest.dmg"

rm -f "$ZIP_FILE" "$DMG_FILE"
(
  cd "$WORK_DIR"
  ditto -c -k --sequesterRsrc --keepParent "$APP_NAME.app" "$ZIP_FILE"
)

mkdir -p "$DMG_ROOT"
ditto "$APP_STAGE" "$DMG_ROOT/$APP_NAME.app"
if [[ "$SIGN" -eq 1 ]]; then
  codesign --verify --deep --strict --verbose=2 "$DMG_ROOT/$APP_NAME.app"
fi
ln -s /Applications "$DMG_ROOT/Applications"
hdiutil create -volname "$APP_NAME" -srcfolder "$DMG_ROOT" -ov -format UDZO "$DMG_FILE"

if [[ "$NOTARIZE" -eq 1 ]]; then
  xcrun notarytool submit "$DMG_FILE" --keychain-profile "$APPLE_NOTARY_PROFILE" --wait
  xcrun stapler staple "$DMG_FILE"
fi

cp "$ZIP_FILE" "$LATEST_ZIP_FILE"
cp "$DMG_FILE" "$LATEST_DMG_FILE"

if [[ -n "$UPLOAD_TARGET" ]]; then
  scp "$ZIP_FILE" "$DMG_FILE" "$LATEST_ZIP_FILE" "$LATEST_DMG_FILE" "$UPLOAD_TARGET"
fi

echo "Created:"
echo "  $ZIP_FILE"
echo "  $DMG_FILE"
echo "  $LATEST_ZIP_FILE"
echo "  $LATEST_DMG_FILE"
