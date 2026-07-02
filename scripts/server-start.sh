#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY="$PROJECT_ROOT/bin/Safire"
RUN_DIR="$PROJECT_ROOT/run"
LOG_DIR="$PROJECT_ROOT/logs"
DEFAULT_NODE_PORT="${SAFIRE_NODE_PORT:-4888}"
SAFIRE_LIBRARY_PATH="$PROJECT_ROOT/src/leveldb:/usr/local/lib:/usr/lib/x86_64-linux-gnu"
PID_FILE="${SAFIRE_PID_FILE:-$RUN_DIR/safire-server.pid}"
LOG_FILE="${SAFIRE_LOG_FILE:-$LOG_DIR/safire-server.log}"

mkdir -p "$RUN_DIR" "$LOG_DIR"

if [[ ! -x "$BINARY" ]]; then
  echo "Safire binary not found. Build it first with: scripts/build.sh linux" >&2
  exit 1
fi

if [[ -f "$PID_FILE" ]]; then
  existing_pid="$(cat "$PID_FILE" 2>/dev/null || true)"
  if [[ -n "${existing_pid:-}" ]] && kill -0 "$existing_pid" >/dev/null 2>&1; then
    echo "Safire server is already running with PID $existing_pid"
    exit 0
  fi
  echo "Removing stale PID file: $PID_FILE"
  rm -f "$PID_FILE"
fi

args=("$@")
if [[ ${#args[@]} -eq 0 ]]; then
  args=(--node-port "$DEFAULT_NODE_PORT" --headless)
  if [[ -n "${SAFIRE_PUBLIC_URL:-}" ]]; then
    args+=(--public-url "$SAFIRE_PUBLIC_URL")
  fi
fi

export LD_LIBRARY_PATH="$SAFIRE_LIBRARY_PATH:${LD_LIBRARY_PATH:-}"

cd "$PROJECT_ROOT"
echo "Starting Safire server: $BINARY ${args[*]}"
echo "LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
echo "Log file: $LOG_FILE"
nohup "$BINARY" "${args[@]}" >> "$LOG_FILE" 2>&1 &
pid="$!"
echo "$pid" > "$PID_FILE"

sleep 1
if ! kill -0 "$pid" >/dev/null 2>&1; then
  echo "Safire server failed to start. Last log lines:" >&2
  tail -n 40 "$LOG_FILE" >&2 || true
  rm -f "$PID_FILE"
  exit 1
fi

echo "Safire server started with PID $pid"
