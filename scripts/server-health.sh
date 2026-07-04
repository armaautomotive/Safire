#!/usr/bin/env bash
set -euo pipefail

DEFAULT_NODE_PORT="${SAFIRE_NODE_PORT:-4888}"
HEALTH_URL="${SAFIRE_HEALTH_URL:-http://127.0.0.1:${DEFAULT_NODE_PORT}/api/status}"
HEALTH_TIMEOUT="${SAFIRE_HEALTH_TIMEOUT:-5}"

response_file="$(mktemp "${TMPDIR:-/tmp}/safire-health.XXXXXX")"
trap 'rm -f "$response_file"' EXIT

if ! curl --fail --silent --show-error --max-time "$HEALTH_TIMEOUT" "$HEALTH_URL" > "$response_file"; then
  echo "Safire health check failed: no valid response from $HEALTH_URL" >&2
  exit 1
fi

if ! grep -q '"status":"ok"' "$response_file"; then
  echo "Safire health check failed: response did not report status ok" >&2
  cat "$response_file" >&2
  exit 1
fi

latest_block_id="$(sed -n 's/.*"latest_block_id":"\([^"]*\)".*/\1/p' "$response_file" | head -n 1)"
genesis_match="$(sed -n 's/.*"genesis_match":"\([^"]*\)".*/\1/p' "$response_file" | head -n 1)"

echo "Safire API is healthy: $HEALTH_URL"
if [[ -n "${latest_block_id:-}" ]]; then
  echo "Latest block: $latest_block_id"
fi
if [[ -n "${genesis_match:-}" ]]; then
  echo "Genesis match: $genesis_match"
fi
