#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
GUI_DIR="$PROJECT_ROOT/gui_client"

find_qmake() {
  if command -v qmake >/dev/null 2>&1; then
    command -v qmake
    return
  fi

  local brew_prefix
  if command -v brew >/dev/null 2>&1; then
    for formula in qt qtbase qt@5; do
      brew_prefix="$(brew --prefix "$formula" 2>/dev/null || true)"
      if [[ -n "$brew_prefix" && -x "$brew_prefix/bin/qmake" ]]; then
        echo "$brew_prefix/bin/qmake"
        return
      fi
    done
  fi

  local candidates=(
    "/usr/local/opt/qt/bin/qmake"
    "/usr/local/opt/qtbase/bin/qmake"
    "/usr/local/opt/qt@5/bin/qmake"
    "/opt/homebrew/opt/qt/bin/qmake"
    "/opt/homebrew/opt/qtbase/bin/qmake"
    "/opt/homebrew/opt/qt@5/bin/qmake"
    "$HOME/Qt/5.15.2/clang_64/bin/qmake"
    "$HOME/Qt/5.12.12/clang_64/bin/qmake"
    "$HOME/Qt/5.10.1/clang_64/bin/qmake"
  )

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return
    fi
  done

  for candidate in /usr/local/Cellar/qtbase/*/bin/qmake /opt/homebrew/Cellar/qtbase/*/bin/qmake; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return
    fi
  done

  return 1
}

QMAKE="$(find_qmake || true)"
if [[ -z "$QMAKE" ]]; then
  echo "Could not find qmake. Install Qt or add qmake to PATH, then rerun scripts/gui.sh." >&2
  exit 1
fi

cd "$GUI_DIR"
qmake_args=()
qt_prefix="$(cd -P "$(dirname "$QMAKE")/.." && pwd)"
if [[ -d "$qt_prefix/share/qt/mkspecs" ]]; then
  qt_conf="$(mktemp "${TMPDIR:-/tmp}/safire-qtconf.XXXXXX")"
  trap 'rm -f "$qt_conf"' EXIT
  {
    echo "[Paths]"
    echo "Prefix=$qt_prefix"
    echo "Binaries=bin"
    echo "Headers=include"
    echo "Libraries=lib"
    echo "LibraryExecutables=share/qt/libexec"
    echo "ArchData=share/qt"
    echo "Data=share/qt"
    echo "HostData=share/qt"
    echo "HostLibraryExecutables=share/qt/libexec"
    echo "Plugins=share/qt/plugins"
    echo "Qml2Imports=share/qt/qml"
  } > "$qt_conf"
  qmake_args+=("-qtconf" "$qt_conf")
fi

if [[ -x "$qt_prefix/share/qt/libexec/moc" ]]; then
  qmake_args+=("QMAKE_MOC=$qt_prefix/share/qt/libexec/moc")
fi
if [[ -x "$qt_prefix/share/qt/libexec/uic" ]]; then
  qmake_args+=("QMAKE_UIC=$qt_prefix/share/qt/libexec/uic")
fi
if [[ -x "$qt_prefix/share/qt/libexec/rcc" ]]; then
  qmake_args+=("QMAKE_RCC=$qt_prefix/share/qt/libexec/rcc")
fi

"$QMAKE" "${qmake_args[@]}" safire.pro
make "$@"

echo "GUI build complete: $GUI_DIR/Safire.app"
