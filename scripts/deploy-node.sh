#!/usr/bin/env bash
set -euo pipefail

BRANCH="${SAFIRE_DEPLOY_BRANCH:-master}"
REMOTE_DIR="${SAFIRE_REMOTE_DIR:-~/safire/Safire}"
REPO_URL="${SAFIRE_REPO_URL:-https://github.com/armaautomotive/Safire.git}"
START_ARGS=()
HOSTS=()

usage() {
  cat <<'USAGE'
Usage: scripts/deploy-node.sh [options] USER@HOST [USER@HOST ...] [-- Safire args]

Updates a Linux test node from Git, builds it, and restarts the background server.

Options:
  --branch BRANCH       Git branch to deploy. Default: master
  --remote-dir DIR      Remote checkout path. Default: ~/safire/Safire
  --repo URL            Git repo URL. Default: https://github.com/armaautomotive/Safire.git
  -h, --help            Show help

Examples:
  scripts/deploy-node.sh root@safire.org
  scripts/deploy-node.sh root@node1 root@node2 -- --node-port 4888 --enable-nat

The remote working tree must be clean except for untracked runtime files.
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --branch) BRANCH="$2"; shift 2 ;;
    --remote-dir) REMOTE_DIR="$2"; shift 2 ;;
    --repo) REPO_URL="$2"; shift 2 ;;
    --) shift; START_ARGS=("$@"); break ;;
    -h|--help) usage; exit 0 ;;
    *) HOSTS+=("$1"); shift ;;
  esac
done

if [[ ${#HOSTS[@]} -eq 0 ]]; then
  usage >&2
  exit 2
fi

quoted_args=""
if [[ ${#START_ARGS[@]} -gt 0 ]]; then
  for arg in "${START_ARGS[@]}"; do
    quoted_args+=" $(printf "%q" "$arg")"
  done
fi

for host in "${HOSTS[@]}"; do
  echo "Deploying Safire node to $host"
  ssh "$host" "set -euo pipefail
    if [[ ! -d $REMOTE_DIR/.git ]]; then
      mkdir -p \$(dirname $REMOTE_DIR)
      git clone $REPO_URL $REMOTE_DIR
    fi
    cd $REMOTE_DIR
    dirty=\$(git status --porcelain --untracked-files=no)
    if [[ -n \"\$dirty\" ]]; then
      echo 'Remote working tree has tracked local changes. Commit or stash them before deploy.' >&2
      git status --short --untracked-files=no >&2
      exit 1
    fi
    git fetch origin
    git checkout $BRANCH
    git pull --ff-only origin $BRANCH
    scripts/build.sh linux
    scripts/server-restart.sh$quoted_args
    scripts/server-status.sh"
done
