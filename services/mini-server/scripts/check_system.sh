#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"

curl --fail --silent --show-error http://127.0.0.1:3000/health
echo
curl --fail --silent --show-error \
  http://127.0.0.1:3000/api/v1/devices/farm-01/telemetry/latest
echo
sqlite3 "$APP_DIR/data/telemetry.db" \
  "SELECT id, device_id, boot_id, seq, temperature_c, humidity_pct, received_at FROM telemetry ORDER BY id DESC LIMIT 5;"
