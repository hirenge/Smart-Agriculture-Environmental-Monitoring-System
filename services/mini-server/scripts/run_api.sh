#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
cd "$APP_DIR"

if [[ -f .env ]]; then
  set -a
  source ./.env
  set +a
fi

exec ./.venv/bin/uvicorn api:app --host "${API_BIND_HOST:-127.0.0.1}" --port "${API_PORT:-3000}"
