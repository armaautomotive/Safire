#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RUN_DIR="$PROJECT_ROOT/run"
STATE_FILE="${SAFIRE_WATCHDOG_STATE_FILE:-$RUN_DIR/safire-watchdog.failures}"
FAILURE_THRESHOLD="${SAFIRE_WATCHDOG_FAILURE_THRESHOLD:-3}"
POST_RESTART_DELAY="${SAFIRE_WATCHDOG_POST_RESTART_DELAY:-5}"
SYSTEMD_SERVICE="${SAFIRE_SYSTEMD_SERVICE:-}"

mkdir -p "$RUN_DIR"

read_failure_count() {
  if [[ -f "$STATE_FILE" ]]; then
    cat "$STATE_FILE" 2>/dev/null || echo 0
  else
    echo 0
  fi
}

restart_safire() {
  if [[ -n "$SYSTEMD_SERVICE" ]] && command -v systemctl >/dev/null 2>&1; then
    echo "Restarting Safire through systemd service: $SYSTEMD_SERVICE"
    systemctl restart "$SYSTEMD_SERVICE"
    return
  fi

  echo "Restarting Safire through helper scripts."
  "$SCRIPT_DIR/server-stop.sh"
  "$SCRIPT_DIR/server-start.sh" "$@"
}

if "$SCRIPT_DIR/server-health.sh"; then
  rm -f "$STATE_FILE"
  exit 0
fi

failure_count="$(read_failure_count)"
case "$failure_count" in
  ''|*[!0-9]*) failure_count=0 ;;
esac
failure_count=$((failure_count + 1))
echo "$failure_count" > "$STATE_FILE"

echo "Safire health failure $failure_count/$FAILURE_THRESHOLD"
if [[ "$failure_count" -lt "$FAILURE_THRESHOLD" ]]; then
  exit 1
fi

rm -f "$STATE_FILE"
restart_safire "$@"
sleep "$POST_RESTART_DELAY"

"$SCRIPT_DIR/server-health.sh"
