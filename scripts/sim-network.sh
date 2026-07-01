#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BINARY="${SAFIRE_BINARY:-$PROJECT_ROOT/bin/Safire}"
SIM_ROOT="${SAFIRE_SIM_ROOT:-$PROJECT_ROOT/sim}"

COMMAND="${1:-run}"
if [[ $# -gt 0 ]]; then
  shift
fi

RUN_NAME="testnet"
NODE_COUNT=6
TOPOLOGY="line"
DURATION=120
PAYMENT_INTERVAL=15
BASE_API_PORT=5200
AMOUNT="0.1"
FEE="0"

usage() {
  cat <<'USAGE'
Usage:
  scripts/sim-network.sh run [options]
  scripts/sim-network.sh start [options]
  scripts/sim-network.sh monitor [options]
  scripts/sim-network.sh stop [options]
  scripts/sim-network.sh clean [options]

Options:
  --name NAME              Simulation name under ./sim (default: testnet)
  --nodes N                Number of virtual nodes (default: 6)
  --topology TYPE          line, star, ring, mesh, partition (default: line)
  --duration SECONDS       Monitor duration for run/monitor (default: 120)
  --payment-interval SEC   Seconds between random payments, 0 disables (default: 15)
  --base-api-port PORT     First API port (default: 5200)
  --amount SFR             Payment amount for generated transfers (default: 0.1)
  --fee SFR                Payment fee for generated transfers (default: 0)

Examples:
  scripts/sim-network.sh run --nodes 8 --topology line --duration 180
  scripts/sim-network.sh start --name sparse --nodes 12 --topology partition
  scripts/sim-network.sh monitor --name sparse --duration 300
  scripts/sim-network.sh clean --name sparse
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --name) RUN_NAME="$2"; shift 2 ;;
    --nodes) NODE_COUNT="$2"; shift 2 ;;
    --topology) TOPOLOGY="$2"; shift 2 ;;
    --duration) DURATION="$2"; shift 2 ;;
    --payment-interval) PAYMENT_INTERVAL="$2"; shift 2 ;;
    --base-api-port) BASE_API_PORT="$2"; shift 2 ;;
    --amount) AMOUNT="$2"; shift 2 ;;
    --fee) FEE="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
done

RUN_DIR="$SIM_ROOT/$RUN_NAME"
LOG_DIR="$RUN_DIR/logs"
STATE_FILE="$RUN_DIR/nodes.tsv"

node_dir() {
  printf "%s/node%03d" "$RUN_DIR" "$1"
}

node_port() {
  echo $((BASE_API_PORT + $1))
}

node_url() {
  printf "http://127.0.0.1:%s" "$(node_port "$1")"
}

json_value() {
  local key="$1"
  sed -n "s/.*\"$key\":\"\\([^\"]*\\)\".*/\\1/p"
}

http_get() {
  curl -sS --max-time 3 "$1" 2>/dev/null || true
}

http_post_form() {
  local url="$1"
  local body="$2"
  curl -sS --max-time 3 -X POST "$url" -H "Content-Type: application/x-www-form-urlencoded" --data "$body" 2>/dev/null || true
}

wait_for_api() {
  local url="$1"
  local label="$2"
  local attempts="${3:-40}"
  for _ in $(seq 1 "$attempts"); do
    if http_get "$url/api/status" | grep -q '"status":"ok"'; then
      return 0
    fi
    sleep 1
  done
  echo "Timed out waiting for $label at $url" >&2
  return 1
}

wait_for_file() {
  local path="$1"
  local label="$2"
  local attempts="${3:-40}"
  for _ in $(seq 1 "$attempts"); do
    if [[ -f "$path" ]]; then
      return 0
    fi
    sleep 1
  done
  echo "Timed out waiting for $label at $path" >&2
  return 1
}

peers_for_node() {
  local index="$1"
  local last=$((NODE_COUNT - 1))
  case "$TOPOLOGY" in
    star)
      if [[ "$index" -eq 0 ]]; then
        for i in $(seq 1 "$last"); do node_url "$i"; done
      else
        node_url 0
      fi
      ;;
    ring)
      node_url $(((index + NODE_COUNT - 1) % NODE_COUNT))
      node_url $(((index + 1) % NODE_COUNT))
      ;;
    mesh)
      for i in $(seq 0 "$last"); do
        if [[ "$i" -ne "$index" ]]; then
          node_url "$i"
        fi
      done
      ;;
    partition)
      local midpoint=$((NODE_COUNT / 2))
      if [[ "$index" -lt "$midpoint" ]]; then
        if [[ "$index" -gt 0 ]]; then node_url $((index - 1)); fi
        if [[ "$index" -lt $((midpoint - 1)) ]]; then node_url $((index + 1)); fi
      else
        if [[ "$index" -gt "$midpoint" ]]; then node_url $((index - 1)); fi
        if [[ "$index" -lt "$last" ]]; then node_url $((index + 1)); fi
      fi
      ;;
    line)
      if [[ "$index" -gt 0 ]]; then node_url $((index - 1)); fi
      if [[ "$index" -lt "$last" ]]; then node_url $((index + 1)); fi
      ;;
    *)
      echo "Unknown topology: $TOPOLOGY" >&2
      exit 2
      ;;
  esac
}

node_args() {
  local index="$1"
  local port
  port="$(node_port "$index")"
  local args=()
  if [[ "$index" -eq 0 ]]; then
    args+=(genesis main)
  fi
  args+=(--api-port "$port" --public-url "$(node_url "$index")")
  while IFS= read -r peer; do
    if [[ -n "$peer" ]]; then
      args+=(--peer "$peer")
    fi
  done < <(peers_for_node "$index")
  printf "%s\n" "${args[@]}"
}

start_one_node() {
  local index="$1"
  local dir
  dir="$(node_dir "$index")"
  mkdir -p "$dir" "$LOG_DIR"
  local log_file="$LOG_DIR/node$(printf "%03d" "$index").log"
  local args=()
  while IFS= read -r arg; do
    args+=("$arg")
  done < <(node_args "$index")

  (
    cd "$dir"
    export LD_LIBRARY_PATH="$PROJECT_ROOT/src/leveldb:/usr/local/lib:/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
    nohup "$BINARY" "${args[@]}" > "$log_file" 2>&1 &
    echo "$!" > safire.pid
  )
  local pid
  pid="$(cat "$dir/safire.pid")"
  printf "%s\t%s\t%s\t%s\n" "$index" "$pid" "$(node_port "$index")" "$(node_url "$index")" >> "$STATE_FILE"
}

stop_nodes() {
  if [[ ! -f "$STATE_FILE" ]]; then
    echo "No simulation state found at $STATE_FILE"
    return 0
  fi
  while IFS=$'\t' read -r index pid port url; do
    if [[ -n "${pid:-}" ]] && kill -0 "$pid" >/dev/null 2>&1; then
      echo "Stopping node $index pid $pid"
      kill "$pid" >/dev/null 2>&1 || true
    fi
  done < "$STATE_FILE"
}

start_network() {
  if [[ ! -x "$BINARY" ]]; then
    echo "Safire binary not found. Build first with: scripts/build.sh" >&2
    exit 1
  fi
  if [[ "$NODE_COUNT" -lt 1 ]]; then
    echo "Node count must be at least 1" >&2
    exit 2
  fi
  if [[ -e "$RUN_DIR" ]]; then
    echo "Simulation already exists: $RUN_DIR" >&2
    echo "Run: scripts/sim-network.sh clean --name $RUN_NAME" >&2
    exit 1
  fi
  mkdir -p "$RUN_DIR" "$LOG_DIR"
  : > "$STATE_FILE"

  echo "Starting Safire simulation '$RUN_NAME'"
  echo "Topology: $TOPOLOGY, nodes: $NODE_COUNT, base API port: $BASE_API_PORT"

  start_one_node 0
  wait_for_api "$(node_url 0)" "node 0"

  local genesis_conf
  genesis_conf="$(node_dir 0)/safire.conf"
  wait_for_file "$genesis_conf" "genesis safire.conf" 60

  if [[ "$NODE_COUNT" -gt 1 ]]; then
    for i in $(seq 1 $((NODE_COUNT - 1))); do
      mkdir -p "$(node_dir "$i")"
      cp "$genesis_conf" "$(node_dir "$i")/safire.conf"
      start_one_node "$i"
    done
    for i in $(seq 1 $((NODE_COUNT - 1))); do
      wait_for_api "$(node_url "$i")" "node $i"
    done
  fi

  echo "Joining wallets to the test network"
  http_post_form "$(node_url 0)/api/wallet/setname" "name=sim0" >/dev/null
  if [[ "$NODE_COUNT" -gt 1 ]]; then
    for i in $(seq 1 $((NODE_COUNT - 1))); do
      http_post_form "$(node_url "$i")/api/wallet/join" "name=sim$i" >/dev/null
    done
  fi

  echo "Simulation started: $RUN_DIR"
}

wallet_address() {
  local index="$1"
  http_get "$(node_url "$index")/api/wallet/status" | json_value "public_key"
}

wallet_balance() {
  local index="$1"
  http_get "$(node_url "$index")/api/wallet/status" | json_value "balance"
}

send_random_payment() {
  if [[ "$NODE_COUNT" -lt 2 ]]; then
    return 0
  fi
  local sender=$((RANDOM % NODE_COUNT))
  local recipient=$((RANDOM % NODE_COUNT))
  if [[ "$recipient" -eq "$sender" ]]; then
    recipient=$(((recipient + 1) % NODE_COUNT))
  fi
  local balance
  balance="$(wallet_balance "$sender")"
  if [[ -z "$balance" ]]; then
    return 0
  fi
  if ! awk "BEGIN { exit !($balance > ($AMOUNT + $FEE)) }"; then
    return 0
  fi
  local recipient_address
  recipient_address="$(wallet_address "$recipient")"
  if [[ -z "$recipient_address" ]]; then
    return 0
  fi
  local body="recipient=$recipient_address&amount=$AMOUNT&fee=$FEE"
  local response
  response="$(http_post_form "$(node_url "$sender")/api/wallet/send" "$body")"
  echo "payment node$sender -> node$recipient amount=$AMOUNT response=${response:-empty}"
}

monitor_network() {
  if [[ ! -f "$STATE_FILE" ]]; then
    echo "No simulation state found at $STATE_FILE" >&2
    exit 1
  fi

  local start_epoch
  start_epoch="$(date +%s)"
  local next_payment_epoch="$start_epoch"
  while true; do
    local now
    now="$(date +%s)"
    if [[ "$DURATION" -gt 0 && $((now - start_epoch)) -ge "$DURATION" ]]; then
      break
    fi
    if [[ "$PAYMENT_INTERVAL" -gt 0 && "$now" -ge "$next_payment_epoch" ]]; then
      send_random_payment
      next_payment_epoch=$((now + PAYMENT_INTERVAL))
    fi

    echo "---- $(date '+%H:%M:%S') ----"
    local tips=""
    local bad=0
    while IFS=$'\t' read -r index pid port url; do
      local status
      status="$(http_get "$url/api/wallet/status")"
      if [[ -z "$status" || "$status" != *'"status":"ok"'* ]]; then
        printf "node%03d down or no wallet response\n" "$index"
        bad=$((bad + 1))
        continue
      fi
      local latest hash balance joined peer_sync supply_diff peers
      latest="$(echo "$status" | json_value "latest_block_id")"
      hash="$(echo "$status" | json_value "latest_block_hash")"
      balance="$(echo "$status" | json_value "balance")"
      joined="$(echo "$status" | json_value "joined")"
      peer_sync="$(echo "$status" | json_value "peer_sync")"
      supply_diff="$(echo "$status" | json_value "supply_difference")"
      peers="$(echo "$status" | json_value "local_peers")"
      printf "node%03d block=%s hash=%s balance=%s joined=%s peer_sync=%s peers=%s diff=%s\n" \
        "$index" "${latest:-?}" "${hash:0:12}" "${balance:-?}" "${joined:-?}" "${peer_sync:-?}" "${peers:-?}" "${supply_diff:-?}"
      tips="${tips}${latest}:${hash}"$'\n'
      if [[ "${supply_diff:-0}" != "0" && "${supply_diff:-0}" != "0.0" ]]; then
        bad=$((bad + 1))
      fi
    done < "$STATE_FILE"
    local unique_tips
    unique_tips="$(printf "%s" "$tips" | sed '/^$/d' | sort -u | wc -l | tr -d ' ')"
    echo "unique tips: ${unique_tips:-0}; warnings: $bad"
    sleep 5
  done
}

clean_network() {
  stop_nodes
  if [[ "$RUN_DIR" == "$SIM_ROOT/"* || "$RUN_DIR" == "$SIM_ROOT"/* ]]; then
    echo "Deleting simulation directory: $RUN_DIR"
    rm -rf "$RUN_DIR"
  else
    echo "Refusing to delete unexpected path: $RUN_DIR" >&2
    exit 1
  fi
}

case "$COMMAND" in
  run)
    start_network
    trap 'clean_network' EXIT
    monitor_network
    trap - EXIT
    clean_network
    ;;
  start)
    start_network
    ;;
  monitor)
    monitor_network
    ;;
  stop)
    stop_nodes
    ;;
  clean)
    clean_network
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    echo "Unknown command: $COMMAND" >&2
    usage >&2
    exit 2
    ;;
esac
