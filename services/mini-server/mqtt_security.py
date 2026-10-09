"""Shared MQTT TLS settings for API publishers and the ingestor."""
import os
import ssl


def configure_mqtt_tls(client):
    if os.getenv("MQTT_TLS", "false").lower() not in ("1", "true", "yes"):
        return
    client.tls_set(
        ca_certs=os.getenv("MQTT_CA_FILE") or None,
        tls_version=ssl.PROTOCOL_TLS_CLIENT,
    )
    client.tls_insecure_set(False)
