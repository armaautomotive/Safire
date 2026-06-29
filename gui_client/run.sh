#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP="$SCRIPT_DIR/Safire.app/Contents/MacOS/Safire"

find_qt_plugins() {
  local candidates=(
    "/usr/local/opt/qtbase/share/qt/plugins"
    "/usr/local/opt/qt/share/qt/plugins"
    "/opt/homebrew/opt/qtbase/share/qt/plugins"
    "/opt/homebrew/opt/qt/share/qt/plugins"
  )

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -f "$candidate/platforms/libqcocoa.dylib" ]]; then
      echo "$candidate"
      return
    fi
  done

  for candidate in /usr/local/Cellar/qtbase/*/share/qt/plugins /opt/homebrew/Cellar/qtbase/*/share/qt/plugins; do
    if [[ -f "$candidate/platforms/libqcocoa.dylib" ]]; then
      echo "$candidate"
      return
    fi
  done
}

if [[ ! -x "$APP" ]]; then
  echo "Safire.app is not built. Run ../scripts/gui.sh first." >&2
  exit 1
fi

qt_plugins="$(find_qt_plugins || true)"
if [[ -n "$qt_plugins" ]]; then
  export QT_PLUGIN_PATH="$qt_plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
fi

exec "$APP" "$@"
