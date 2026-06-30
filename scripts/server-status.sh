#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RUN_DIR="$PROJECT_ROOT/run"
LOG_DIR="$PROJECT_ROOT/logs"
PID_FILE="${SAFIRE_PID_FILE:-$RUN_DIR/safire-server.pid}"
LOG_FILE="${SAFIRE_LOG_FILE:-$LOG_DIR/safire-server.log}"

if [[ ! -f "$PID_FILE" ]]; then
  echo "Safire server is stopped."
  echo "PID file: $PID_FILE"
  echo "Log file: $LOG_FILE"
  exit 3
fi

pid="$(cat "$PID_FILE" 2>/dev/null || true)"
if [[ -n "${pid:-}" ]] && kill -0 "$pid" >/dev/null 2>&1; then
  echo "Safire server is running."
  echo "PID: $pid"
  echo "PID file: $PID_FILE"
  echo "Log file: $LOG_FILE"
  ps -p "$pid" -o pid,etime,command
  exit 0
fi

echo "Safire server is stopped, but the PID file is stale."
echo "Stale PID file: $PID_FILE"
exit 1
