#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RUN_DIR="$PROJECT_ROOT/run"
PID_FILE="${SAFIRE_PID_FILE:-$RUN_DIR/safire-server.pid}"

if [[ ! -f "$PID_FILE" ]]; then
  echo "Safire server is not running: no PID file at $PID_FILE"
  exit 0
fi

pid="$(cat "$PID_FILE" 2>/dev/null || true)"
if [[ -z "${pid:-}" ]]; then
  echo "PID file is empty. Removing $PID_FILE"
  rm -f "$PID_FILE"
  exit 0
fi

if ! kill -0 "$pid" >/dev/null 2>&1; then
  echo "Safire server PID $pid is not running. Removing stale PID file."
  rm -f "$PID_FILE"
  exit 0
fi

echo "Stopping Safire server PID $pid"
kill "$pid"

for _ in {1..30}; do
  if ! kill -0 "$pid" >/dev/null 2>&1; then
    rm -f "$PID_FILE"
    echo "Safire server stopped."
    exit 0
  fi
  sleep 1
done

echo "Safire server did not stop after 30 seconds; forcing shutdown."
kill -9 "$pid" >/dev/null 2>&1 || true
rm -f "$PID_FILE"
echo "Safire server stopped."
