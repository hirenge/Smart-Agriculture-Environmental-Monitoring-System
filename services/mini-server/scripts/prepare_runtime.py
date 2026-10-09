#!/usr/bin/env python3
"""Initialize a new SAEMS deployment; never overwrite existing runtime secrets."""
import json
import os
from pathlib import Path
import secrets
import sqlite3
import subprocess

APP_DIR = Path(__file__).resolve().parents[1]
os.umask(0o077)
data = APP_DIR / "data"
data.mkdir(exist_ok=True)
(data / "mqtt").mkdir(exist_ok=True)
env_path = APP_DIR / ".env"
access_path = APP_DIR / "device-access.json"

if not env_path.exists():
    server_password = secrets.token_urlsafe(32)
    device_password = secrets.token_urlsafe(32)
    access_token = secrets.token_urlsafe(32)
    env_path.write_text(
        f"MQTT_HOST=127.0.0.1\nMQTT_PORT=1884\nMQTT_USERNAME=saems_server\n"
        f"MQTT_PASSWORD={server_password}\nPUMP_COMMAND_MAX_SECONDS=60\n"
        f"API_BIND_HOST=127.0.0.1\nAPI_PORT=3000\nAPI_PUBLIC_MODE=true\n"
        f"API_ACCESS_TOKEN={access_token}\nCORS_ORIGINS=\nMQTT_TLS=false\n",
        encoding="utf-8",
    )
    access_path.write_text(json.dumps({
        "api_username": "saems",
        "api_access_token": access_token,
        "mqtt_username": "saems_esp32",
        "mqtt_password": device_password,
        "device_id": "farm-01",
    }, indent=2), encoding="utf-8")
    passwd_path = APP_DIR / "config" / "mqtt-passwd"
    for index, (username, password) in enumerate([
        ("saems_server", server_password), ("saems_esp32", device_password),
    ]):
        arguments = ["mosquitto_passwd", "-b"]
        if index == 0:
            arguments.append("-c")
        subprocess.run([*arguments, str(passwd_path), username, password], check=True)
    (APP_DIR / "config" / "mqtt-acl").write_text(
        "user saems_server\ntopic read saems/+/telemetry\n"
        "topic write saems/+/commands\n"
        "user saems_esp32\ntopic write saems/farm-01/telemetry\n"
        "topic read saems/farm-01/commands\n",
        encoding="utf-8",
    )

target = data / "telemetry.db"
legacy = Path("/home/admin/smartfarm-mini-server/data/telemetry.db")
if not target.exists() and legacy.exists():
    with sqlite3.connect(legacy) as source, sqlite3.connect(target) as destination:
        source.backup(destination)
    print("Existing telemetry preserved in the SAEMS database copy")

print("Runtime ready; secret values were not printed")
