#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RUN_DIR="$PROJECT_ROOT/run"
LOG_DIR="$PROJECT_ROOT/logs"
PID_FILE="${SAFIRE_PID_FILE:-$RUN_DIR/safire-server.pid}"
LOG_FILE="${SAFIRE_LOG_FILE:-$LOG_DIR/safire-server.log}"
SYSTEMD_SERVICE="${SAFIRE_SYSTEMD_SERVICE:-safire.service}"

if command -v systemctl >/dev/null 2>&1 &&
   systemctl list-unit-files "$SYSTEMD_SERVICE" >/dev/null 2>&1 &&
   systemctl is-active --quiet "$SYSTEMD_SERVICE"; then
  echo "Safire server is running under systemd."
  echo "Service: $SYSTEMD_SERVICE"
  systemctl --no-pager --full status "$SYSTEMD_SERVICE" | sed -n '1,12p'
  echo
  if "$SCRIPT_DIR/server-health.sh"; then
    exit 0
  fi
  echo "Safire systemd service is running, but the API health check failed." >&2
  exit 2
fi

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
  echo
  if "$SCRIPT_DIR/server-health.sh"; then
    exit 0
  fi
  echo "Safire server process is running, but the API health check failed." >&2
  exit 2
fi

echo "Safire server is stopped, but the PID file is stale."
echo "Stale PID file: $PID_FILE"
exit 1
