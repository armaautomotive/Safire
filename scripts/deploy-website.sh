#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TARGET="${1:-}"

usage() {
  cat <<'USAGE'
Usage: scripts/deploy-website.sh USER@HOST:/remote/web/root

Copies the static Safire website from web/ to the given SSH/scp target.
Example:
  scripts/deploy-website.sh root@safire.org:/var/www/safire.org
USAGE
}

if [[ -z "$TARGET" || "$TARGET" == "-h" || "$TARGET" == "--help" ]]; then
  usage
  exit 0
fi

if [[ ! -f "$PROJECT_ROOT/web/index.html" ]]; then
  echo "Missing web/index.html" >&2
  exit 1
fi

echo "Deploying Safire website to $TARGET"
scp -r "$PROJECT_ROOT/web/"* "$TARGET/"
echo "Website deployed."
