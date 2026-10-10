#!/usr/bin/env python3
"""Test public TLS, auth, MQTT ACL and dedup without printing credentials.

Run on the Pi. All generated test telemetry is tagged with a unique boot_id
and removed afterwards; historical/device rows are never touched.
"""
import base64
import http.client
import json
from pathlib import Path
import socket
import sqlite3
import ssl
import threading
import time
import urllib.request
from uuid import uuid4

import paho.mqtt.client as mqtt

APP_DIR = Path(__file__).resolve().parents[1]
HOST = "admin.tailb1f0ba.ts.net"
access = json.loads((APP_DIR / "device-access.json").read_text())
env = dict(line.split("=", 1) for line in (APP_DIR / ".env").read_text().splitlines()
           if line and not line.startswith("#") and "=" in line)
with urllib.request.urlopen(
    "https://dns.google/resolve?name=" + HOST + "&type=A", timeout=15
) as response:
    answers = json.load(response)["Answer"]
PUBLIC_IP = next(item["data"] for item in answers if item["type"] == 1)
print("Testing public Funnel IP", PUBLIC_IP, "(not the LAN/Tailscale IP)")


def request(path, authorization=None):
    connection = http.client.HTTPSConnection(HOST, timeout=20,
                                             context=ssl.create_default_context())
    connection._create_connection = lambda address, timeout, source_address=None: (
        socket.create_connection((PUBLIC_IP, address[1]), timeout, source_address)
    )
    try:
        headers = {"Authorization": authorization} if authorization else {}
        connection.request("GET", path, headers=headers)
        response = connection.getresponse()
        return response.status, response.read()
    finally:
        connection.close()


for path in ("/", "/docs", "/api/v1/devices"):
    assert request(path)[0] == 401, path
assert request("/api/v1/devices", "Bearer incorrect")[0] == 401
assert request("/api/v1/devices", "Basic !!!")[0] == 401
assert request("/health")[0] == 200
bearer = "Bearer " + access["api_access_token"]
assert request("/api/v1/devices", bearer)[0] == 200
basic = "Basic " + base64.b64encode(
    ("saems:" + access["api_access_token"]).encode()
).decode()
assert request("/", basic)[0] == 200
print("PASS: public HTTPS certificate/hostname, Bearer/Basic and 401 protection")


class PublicMqtt(mqtt.Client):
    def _create_socket_connection(self):
        return socket.create_connection((PUBLIC_IP, 8443), timeout=15)


def connect(username, password, expect_success=True):
    event = threading.Event()
    result = []
    client = PublicMqtt(mqtt.CallbackAPIVersion.VERSION2,
                        client_id="saems-test-" + uuid4().hex[:12],
                        protocol=mqtt.MQTTv5)
    client.username_pw_set(username, password)
    client.tls_set_context(ssl.create_default_context())

    def connected(client, userdata, flags, reason, properties):
        result.append(not reason.is_failure)
        event.set()

    client.on_connect = connected
    try:
        client.connect(HOST, 8443)
        client.loop_start()
        assert event.wait(15), "MQTT connection timed out"
        assert result[0] == expect_success, "Unexpected MQTT authentication result"
    except Exception:
        client.disconnect()
        client.loop_stop()
        raise
    return client


bad = connect(access["mqtt_username"], "incorrect", expect_success=False)
bad.disconnect()
bad.loop_stop()
print("PASS: public MQTT TLS rejects wrong credentials")

boot_id = "internet-test-" + uuid4().hex
device_id = access["device_id"]
unauthorized_id = "acl-test-" + uuid4().hex[:12]
database = APP_DIR / "data" / "telemetry.db"
client = None
try:
    # QoS replay after a reconnect models duplicate delivery, not physical
    # ESP32 NVS durability (that still needs a board/power-cycle test).
    for attempt in range(2):
        client = connect(access["mqtt_username"], access["mqtt_password"])
        for seq in range(1, 4):
            payload = json.dumps({"device_id": device_id, "boot_id": boot_id,
                                  "seq": seq, "data_mode": "simulated",
                                  "sensor_health": "connectivity_test",
                                  "temperature_c": None, "soil_moisture_pct": None})
            published = client.publish(f"saems/{device_id}/telemetry", payload, qos=1)
            published.wait_for_publish(timeout=15)
            assert published.is_published(), "Telemetry PUBACK timeout"
        if attempt == 0:
            denied = threading.Event()
            reasons = []

            def on_publish(client, userdata, mid, reason, properties):
                reasons.append(reason.is_failure)
                denied.set()

            client.on_publish = on_publish
            client.publish(f"saems/{unauthorized_id}/telemetry",
                           json.dumps({"device_id": unauthorized_id,
                                       "boot_id": boot_id, "seq": 1}), qos=1)
            assert denied.wait(15) and any(reasons), "Device write ACL not enforced"
        client.disconnect()
        client.loop_stop()
        client = None
    deadline = time.monotonic() + 15
    count = 0
    while time.monotonic() < deadline:
        with sqlite3.connect(database) as connection:
            count = connection.execute(
                "SELECT COUNT(*) FROM telemetry WHERE device_id=? AND boot_id=?",
                (device_id, boot_id),
            ).fetchone()[0]
        if count == 3:
            break
        time.sleep(0.25)
    assert count == 3, f"Expected exactly three deduplicated rows, got {count}"
    status, body = request(f"/api/v1/devices/{device_id}/telemetry/latest", bearer)
    latest = json.loads(body)
    assert status == 200 and latest["data_mode"] == "simulated"
    assert latest["soil_moisture_pct"] is None
    print("PASS: MQTT TLS -> ingestor -> SQLite -> public HTTPS")
    print("PASS: device topic ACL and replay dedup after MQTT reconnect")
finally:
    if client is not None:
        client.disconnect()
        client.loop_stop()
    with sqlite3.connect(database) as connection:
        connection.execute("DELETE FROM telemetry WHERE boot_id=?", (boot_id,))
        connection.commit()
    print("Removed only this run's temporary simulated test records")
