#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"
cd "$APP_DIR"

set -a
source ./.env
set +a
exec mosquitto_sub \
  -h "$MQTT_HOST" \
  -p "$MQTT_PORT" \
  -u "$MQTT_USERNAME" \
  -P "$MQTT_PASSWORD" \
  -t "smartfarm/+/telemetry" \
  -q 1 \
  -v
