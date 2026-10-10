#!/usr/bin/env python3
"""Read the existing firmware's LAN topic into SAEMS, without device writes.

Compatibility only: this does not upgrade the old ESP32's LAN MQTT to TLS.
Public API authentication and the new private TLS broker remain unchanged.
"""
import logging
import os
from types import SimpleNamespace

from dotenv import dotenv_values
import paho.mqtt.client as mqtt

import mqtt_ingestor as ingestion

LEGACY_TOPIC = "smartfarm/farm-01/telemetry"
SAEMS_TOPIC = "saems/farm-01/telemetry"


def adapt_message(message):
    if message.topic != LEGACY_TOPIC:
        return None
    # Preserve payload, boot_id, sequence, data_mode and sensor null values.
    # Only normalize the transport envelope for the existing ingestion validator.
    return SimpleNamespace(topic=SAEMS_TOPIC, payload=message.payload)


def main():
    ingestion.initialize_database()
    legacy_env = dotenv_values(os.getenv(
        "LEGACY_ENV_FILE", "/home/admin/smartfarm-mini-server/.env"
    ))
    username = os.getenv("LEGACY_MQTT_USERNAME") or legacy_env.get("MQTT_USERNAME")
    password = os.getenv("LEGACY_MQTT_PASSWORD") or legacy_env.get("MQTT_PASSWORD")
    if not username or not password:
        raise RuntimeError("Configure legacy MQTT credentials; secret values are not logged")
    host = os.getenv("LEGACY_MQTT_HOST", "127.0.0.1")
    port = int(os.getenv("LEGACY_MQTT_PORT", "1883"))
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2,
                         client_id="saems-legacy-telemetry-reader-farm01",
                         protocol=mqtt.MQTTv311, clean_session=False)
    client.username_pw_set(username, password)
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    def connected(client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            logging.error("Legacy broker rejected connection: %s", reason_code)
            return
        result, _ = client.subscribe(LEGACY_TOPIC, qos=1)
        if result != mqtt.MQTT_ERR_SUCCESS:
            logging.error("Legacy subscription failed: %s", result)
        else:
            logging.info("Read-only compatibility subscription: %s", LEGACY_TOPIC)

    def received(client, userdata, message):
        adapted = adapt_message(message)
        if adapted is not None:
            ingestion.on_message(client, userdata, adapted)

    client.on_connect = connected
    client.on_disconnect = ingestion.on_disconnect
    client.on_message = received
    client.connect_async(host, port, keepalive=60)
    client.loop_forever(retry_first_connection=True)


if __name__ == "__main__":
    main()
