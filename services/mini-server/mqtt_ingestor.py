#!/usr/bin/env python3

import json
import logging
import os
import sqlite3
from datetime import datetime, timezone
from pathlib import Path

import paho.mqtt.client as mqtt


BASE_DIR = Path(__file__).resolve().parent
DATA_DIR = BASE_DIR / "data"
DB_PATH = DATA_DIR / "telemetry.db"

MQTT_HOST = os.getenv("MQTT_HOST", "192.168.1.4")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "smartfarm_server")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD")
MQTT_TOPIC = "smartfarm/+/telemetry"
MQTT_CLIENT_ID = "smartfarm-mini-server-ingestor"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def optional_float(payload: dict, key: str):
    value = payload.get(key)
    return None if value is None else float(value)


def initialize_database() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)

    with sqlite3.connect(DB_PATH, timeout=10) as connection:
        connection.execute("PRAGMA journal_mode=WAL")
        connection.execute(
            """
            CREATE TABLE IF NOT EXISTS telemetry (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                device_id TEXT NOT NULL,
                boot_id TEXT,
                seq INTEGER,
                timestamp TEXT NOT NULL,
                received_at TEXT NOT NULL,
                temperature_c REAL,
                humidity_pct REAL,
                soil_moisture_pct REAL,
                light_value REAL,
                light_unit TEXT,
                battery_pct REAL,
                pump_state TEXT,
                raw_json TEXT NOT NULL
            )
            """
        )

        columns = {
            row[1]
            for row in connection.execute("PRAGMA table_info(telemetry)")
        }
        if "boot_id" not in columns:
            connection.execute(
                "ALTER TABLE telemetry ADD COLUMN boot_id TEXT"
            )

        # The old index dropped valid data after a device rebooted and seq
        # restarted at 1. boot_id makes a sequence unique within one boot.
        connection.execute("DROP INDEX IF EXISTS uq_telemetry_device_seq")
        connection.execute(
            """
            CREATE UNIQUE INDEX IF NOT EXISTS
            uq_telemetry_device_boot_seq
            ON telemetry(device_id, boot_id, seq)
            WHERE boot_id IS NOT NULL AND seq IS NOT NULL
            """
        )
        connection.commit()


def on_connect(client, userdata, flags, reason_code, properties) -> None:
    if reason_code.is_failure:
        logging.error("MQTT connection failed: %s", reason_code)
        return

    logging.info("Connected to MQTT broker %s:%s", MQTT_HOST, MQTT_PORT)
    result, _ = client.subscribe(MQTT_TOPIC, qos=1)
    if result != mqtt.MQTT_ERR_SUCCESS:
        logging.error("Subscribe failed: %s", result)
    else:
        logging.info("Subscribed to %s", MQTT_TOPIC)


def on_message(client, userdata, message) -> None:
    try:
        topic_parts = message.topic.split("/")
        if (
            len(topic_parts) != 3
            or topic_parts[0] != "smartfarm"
            or topic_parts[2] != "telemetry"
        ):
            logging.warning("Ignoring unexpected topic: %s", message.topic)
            return

        payload = json.loads(message.payload.decode("utf-8"))
        if not isinstance(payload, dict):
            raise ValueError("Payload must be a JSON object")

        topic_device_id = topic_parts[1]
        device_id = str(payload.get("device_id") or topic_device_id)
        if device_id != topic_device_id:
            raise ValueError(
                f"device_id mismatch: topic={topic_device_id} "
                f"payload={device_id}"
            )

        boot_value = payload.get("boot_id")
        boot_id = str(boot_value) if boot_value is not None else None
        seq_value = payload.get("seq")
        seq = int(seq_value) if seq_value is not None else None
        timestamp = str(payload.get("timestamp") or utc_now())
        light_value = payload.get(
            "light_lux", payload.get("light_value", payload.get("light"))
        )
        light_unit = "lux" if "light_lux" in payload else None
        raw_json = json.dumps(
            payload, ensure_ascii=False, separators=(",", ":")
        )

        with sqlite3.connect(DB_PATH, timeout=10) as connection:
            cursor = connection.execute(
                """
                INSERT OR IGNORE INTO telemetry (
                    device_id, boot_id, seq, timestamp, received_at,
                    temperature_c, humidity_pct, soil_moisture_pct,
                    light_value, light_unit, battery_pct, pump_state, raw_json
                )
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    device_id,
                    boot_id,
                    seq,
                    timestamp,
                    utc_now(),
                    optional_float(payload, "temperature_c"),
                    optional_float(payload, "humidity_pct"),
                    optional_float(payload, "soil_moisture_pct"),
                    float(light_value) if light_value is not None else None,
                    light_unit,
                    optional_float(payload, "battery_pct"),
                    payload.get("pump_state"),
                    raw_json,
                ),
            )
            connection.commit()

        if cursor.rowcount == 0:
            logging.info(
                "QoS duplicate ignored: device=%s boot=%s seq=%s",
                device_id,
                boot_id,
                seq,
            )
        else:
            logging.info(
                "Telemetry saved: device=%s boot=%s seq=%s",
                device_id,
                boot_id,
                seq,
            )
    except (UnicodeDecodeError, json.JSONDecodeError):
        logging.exception("Invalid JSON payload on %s", message.topic)
    except (TypeError, ValueError, sqlite3.Error):
        logging.exception("Failed to process MQTT message on %s", message.topic)


def create_mqtt_client():
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=MQTT_CLIENT_ID,
        protocol=mqtt.MQTTv311,
        clean_session=False,
    )
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    client.on_connect = on_connect
    client.on_message = on_message
    return client


def main() -> None:
    if not MQTT_PASSWORD:
        raise RuntimeError("MQTT_PASSWORD environment variable is not set")

    initialize_database()
    logging.info("Database: %s", DB_PATH)
    client = create_mqtt_client()
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_forever()


if __name__ == "__main__":
    main()
