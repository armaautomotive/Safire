#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
GUI_DIR="$PROJECT_ROOT/gui_client"
GUI_ARCH="${SAFIRE_MAC_ARCH:-arm64}"

if [[ "${1:-}" == "--arch" ]]; then
  GUI_ARCH="$2"
  shift 2
fi

case "$GUI_ARCH" in
  arm64|x86_64) ;;
  *) echo "Unsupported GUI architecture: $GUI_ARCH" >&2; exit 2 ;;
esac

find_qmake() {
  local brew_prefix
  local candidates=()

  if [[ "$GUI_ARCH" == "arm64" ]]; then
    candidates+=(
      "/opt/homebrew/opt/qt/bin/qmake"
      "/opt/homebrew/opt/qtbase/bin/qmake"
      "/opt/homebrew/opt/qt@5/bin/qmake"
    )
  else
    candidates+=(
      "/usr/local/opt/qt/bin/qmake"
      "/usr/local/opt/qtbase/bin/qmake"
      "/usr/local/opt/qt@5/bin/qmake"
      "$HOME/Qt/5.15.2/clang_64/bin/qmake"
      "$HOME/Qt/5.12.12/clang_64/bin/qmake"
      "$HOME/Qt/5.10.1/clang_64/bin/qmake"
    )
  fi

  local candidate
  for candidate in "${candidates[@]}"; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return
    fi
  done

  if [[ "$GUI_ARCH" == "arm64" ]]; then
    for candidate in /opt/homebrew/Cellar/qtbase/*/bin/qmake; do
      if [[ -x "$candidate" ]]; then
        echo "$candidate"
        return
      fi
    done
  else
    for candidate in /usr/local/Cellar/qtbase/*/bin/qmake; do
      if [[ -x "$candidate" ]]; then
        echo "$candidate"
        return
      fi
    done
  fi

  if command -v qmake >/dev/null 2>&1; then
    candidate="$(command -v qmake)"
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return
    fi
  fi

  return 1
}

QMAKE="$(find_qmake || true)"
if [[ -z "$QMAKE" ]]; then
  echo "Could not find qmake for $GUI_ARCH. Install Qt for that architecture, then rerun scripts/gui.sh." >&2
  exit 1
fi

qmake_arch="$(file "$QMAKE" 2>/dev/null || true)"
if [[ "$GUI_ARCH" == "arm64" && "$qmake_arch" != *"arm64"* ]]; then
  echo "Refusing to build arm64 GUI with non-arm64 qmake: $QMAKE" >&2
  echo "$qmake_arch" >&2
  exit 1
fi
if [[ "$GUI_ARCH" == "x86_64" && "$qmake_arch" != *"x86_64"* ]]; then
  echo "Refusing to build x86_64 GUI with non-x86_64 qmake: $QMAKE" >&2
  echo "$qmake_arch" >&2
  exit 1
fi

cd "$GUI_DIR"
arch_stamp="$GUI_DIR/.safire-gui-arch"
previous_arch=""
if [[ -f "$arch_stamp" ]]; then
  previous_arch="$(cat "$arch_stamp" 2>/dev/null || true)"
fi
if [[ "$previous_arch" != "$GUI_ARCH" ]]; then
  echo "Cleaning GUI objects for architecture switch: ${previous_arch:-none} -> $GUI_ARCH"
  rm -f ./*.o ./moc_*.cpp ./moc_*.o ./moc_predefs.h ./.qmake.stash ./Makefile
fi

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

"$QMAKE" "${qmake_args[@]}" QMAKE_APPLE_DEVICE_ARCHS="$GUI_ARCH" safire.pro
make "$@"
echo "$GUI_ARCH" > "$arch_stamp"

echo "GUI build complete: $GUI_DIR/Safire.app ($GUI_ARCH)"
