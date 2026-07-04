#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SERVICE_NAME="${SAFIRE_SYSTEMD_SERVICE_NAME:-safire}"
NODE_PORT="${SAFIRE_NODE_PORT:-4888}"
PUBLIC_URL="${SAFIRE_PUBLIC_URL:-}"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
WATCHDOG_SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}-watchdog.service"
WATCHDOG_TIMER_FILE="/etc/systemd/system/${SERVICE_NAME}-watchdog.timer"
SAFIRE_LIBRARY_PATH="$PROJECT_ROOT/src/leveldb:/usr/local/lib:/usr/lib/x86_64-linux-gnu"
SERVICE_HOME="${SAFIRE_SERVICE_HOME:-${HOME:-/root}}"

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "systemd installation is only supported on Linux." >&2
  exit 1
fi

if ! command -v systemctl >/dev/null 2>&1; then
  echo "systemctl was not found. This host does not appear to use systemd." >&2
  exit 1
fi

if [[ ! -x "$PROJECT_ROOT/bin/Safire" ]]; then
  echo "Safire binary not found. Build it first with: scripts/build.sh linux" >&2
  exit 1
fi

run_as_root() {
  if [[ "${EUID:-$(id -u)}" -eq 0 ]]; then
    "$@"
  else
    sudo "$@"
  fi
}

start_args=(--node-port "$NODE_PORT" --headless)
if [[ -n "$PUBLIC_URL" ]]; then
  start_args+=(--public-url "$PUBLIC_URL")
fi

quoted_start_args=""
for arg in "${start_args[@]}"; do
  quoted_start_args+=" $(printf "%q" "$arg")"
done

tmp_service="$(mktemp "${TMPDIR:-/tmp}/safire-service.XXXXXX")"
tmp_watchdog_service="$(mktemp "${TMPDIR:-/tmp}/safire-watchdog-service.XXXXXX")"
tmp_watchdog_timer="$(mktemp "${TMPDIR:-/tmp}/safire-watchdog-timer.XXXXXX")"
trap 'rm -f "$tmp_service" "$tmp_watchdog_service" "$tmp_watchdog_timer"' EXIT

cat > "$tmp_service" <<SERVICE
[Unit]
Description=Safire node
After=network-online.target
Wants=network-online.target
StartLimitIntervalSec=300
StartLimitBurst=20

[Service]
Type=simple
WorkingDirectory=$PROJECT_ROOT
Environment=HOME=$SERVICE_HOME
Environment=LD_LIBRARY_PATH=$SAFIRE_LIBRARY_PATH
ExecStart=$PROJECT_ROOT/bin/Safire$quoted_start_args
Restart=always
RestartSec=10
TimeoutStopSec=30

[Install]
WantedBy=multi-user.target
SERVICE

cat > "$tmp_watchdog_service" <<SERVICE
[Unit]
Description=Safire API health watchdog
After=${SERVICE_NAME}.service

[Service]
Type=oneshot
WorkingDirectory=$PROJECT_ROOT
Environment=SAFIRE_NODE_PORT=$NODE_PORT
Environment=SAFIRE_SYSTEMD_SERVICE=${SERVICE_NAME}.service
ExecStart=$PROJECT_ROOT/scripts/server-watchdog.sh
SERVICE

cat > "$tmp_watchdog_timer" <<TIMER
[Unit]
Description=Run Safire API health watchdog every minute

[Timer]
OnBootSec=90
OnUnitActiveSec=60
Unit=${SERVICE_NAME}-watchdog.service

[Install]
WantedBy=timers.target
TIMER

run_as_root cp "$tmp_service" "$SERVICE_FILE"
run_as_root cp "$tmp_watchdog_service" "$WATCHDOG_SERVICE_FILE"
run_as_root cp "$tmp_watchdog_timer" "$WATCHDOG_TIMER_FILE"
run_as_root systemctl daemon-reload
run_as_root systemctl enable --now "${SERVICE_NAME}.service"
run_as_root systemctl enable --now "${SERVICE_NAME}-watchdog.timer"

echo "Installed and started ${SERVICE_NAME}.service"
echo "Installed and started ${SERVICE_NAME}-watchdog.timer"
echo "Health URL: http://127.0.0.1:${NODE_PORT}/api/status"
