from datetime import datetime, timezone
import json
import secrets
import base64
import os
from pathlib import Path
from typing import Literal, Optional
import sqlite3
from uuid import uuid4

import paho.mqtt.client as mqtt
from fastapi import FastAPI, HTTPException, Query, Request
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel, Field
from fastapi.staticfiles import StaticFiles
from mqtt_security import configure_mqtt_tls


BASE_DIR = Path(__file__).resolve().parent
DB_PATH = BASE_DIR / "data" / "telemetry.db"
ONLINE_TIMEOUT_SECONDS = 900
MQTT_HOST = os.getenv("MQTT_HOST", "192.168.1.4")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "saems_server")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD")
PUMP_COMMAND_MAX_SECONDS = int(
    os.getenv("PUMP_COMMAND_MAX_SECONDS", "60")
)
API_ACCESS_TOKEN = os.getenv("API_ACCESS_TOKEN", "")
API_PUBLIC_MODE = os.getenv("API_PUBLIC_MODE", "false").lower() in ("1", "true", "yes")
if API_PUBLIC_MODE and len(API_ACCESS_TOKEN) < 32:
    raise RuntimeError("Public API requires an API_ACCESS_TOKEN of at least 32 characters")


class PumpCommand(BaseModel):
    state: Literal["on", "off"]
    duration_s: int = Field(
        default=PUMP_COMMAND_MAX_SECONDS,
        ge=0,
        le=86400,
    )

app = FastAPI(title="SAEMS Mini Server API", version="0.3.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=[value for value in os.getenv("CORS_ORIGINS", "").split(",") if value],
    allow_credentials=False,
    allow_methods=["GET", "POST", "PUT"],
    allow_headers=["*"],
)


@app.middleware("http")
async def authenticate(request: Request, call_next):
    """Bearer for mobile/API clients, HTTP Basic for the same-origin dashboard."""
    if request.url.path == "/health" or request.method == "OPTIONS" or not API_ACCESS_TOKEN:
        return await call_next(request)
    authorization = request.headers.get("authorization", "")
    scheme, _, value = authorization.partition(" ")
    candidate = ""
    if scheme.lower() == "bearer":
        candidate = value
    elif scheme.lower() == "basic":
        try:
            username, separator, password = base64.b64decode(
                value, validate=True
            ).decode("utf-8").partition(":")
            if username == "saems" and separator:
                candidate = password
        except (ValueError, UnicodeError):
            pass
    if not candidate or not secrets.compare_digest(candidate.encode(), API_ACCESS_TOKEN.encode()):
        return JSONResponse(
            status_code=401,
            content={"detail": "Authentication required"},
            headers={"WWW-Authenticate": 'Basic realm="SAEMS", charset="UTF-8"'},
        )
    response = await call_next(request)
    response.headers["Cache-Control"] = "no-store"
    response.headers["X-Content-Type-Options"] = "nosniff"
    return response


def latest_rows(connection):
    """Return the newest telemetry row for every known device."""
    return connection.execute(
        """
        SELECT telemetry.*
        FROM telemetry
        INNER JOIN (
            SELECT device_id, MAX(id) AS latest_id
            FROM telemetry
            GROUP BY device_id
        ) latest ON latest.latest_id = telemetry.id
        ORDER BY telemetry.device_id ASC
        """
    ).fetchall()


def get_connection():
    if not DB_PATH.exists():
        raise HTTPException(503, "Telemetry database does not exist")
    connection = sqlite3.connect(DB_PATH, timeout=10)
    connection.row_factory = sqlite3.Row
    return connection


def row_to_telemetry(row):
    light_value = row["light_value"]
    # Preserve schema metadata, sensor health and null values from firmware.
    # Historical rows without data_mode must stay unknown, never become real.
    try:
        raw = json.loads(row["raw_json"])
    except (ValueError, TypeError):
        raw = {}
    metadata = {
        key: raw[key] for key in (
            "schema_version", "data_mode", "sensor_health", "sensor_status",
            "uptime_ms", "uptime_s", "timestamp_source", "soil_raw", "soil_digital",
        ) if isinstance(raw, dict) and key in raw
    }
    return {
        **metadata,
        "device_id": row["device_id"],
        "boot_id": row["boot_id"],
        "seq": row["seq"],
        "timestamp": row["timestamp"],
        "received_at": row["received_at"],
        "temperature_c": row["temperature_c"],
        "humidity_pct": row["humidity_pct"],
        "soil_moisture_pct": row["soil_moisture_pct"],
        # light_lux follows the ESP32 payload name. Keep light_value as a
        # compatibility alias for the existing dashboard and older clients.
        "light_lux": light_value,
        "light_value": light_value,
        "light_unit": row["light_unit"],
        "battery_pct": row["battery_pct"],
        "pump_state": row["pump_state"],
    }


def is_online(last_seen: str) -> bool:
    try:
        parsed = datetime.fromisoformat(last_seen)
        if parsed.tzinfo is None:
            parsed = parsed.replace(tzinfo=timezone.utc)
        return (
            datetime.now(timezone.utc) - parsed
        ).total_seconds() <= ONLINE_TIMEOUT_SECONDS
    except (TypeError, ValueError):
        return False


def publish_pump_command(device_id: str, command: PumpCommand, request_id: str):
    if not MQTT_PASSWORD:
        raise HTTPException(503, "MQTT_PASSWORD is not configured")
    if any(character in device_id for character in "/#+ "):
        raise HTTPException(400, "Invalid device_id")

    topic = f"saems/{device_id}/commands"
    payload = json.dumps(
        {
            "command": "pump",
            "state": command.state,
            "duration_s": command.duration_s,
            "request_id": request_id,
        },
        separators=(",", ":"),
    )
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=f"saems-api-{uuid4().hex[:12]}",
        protocol=mqtt.MQTTv311,
        clean_session=True,
    )
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    configure_mqtt_tls(client)
    client.reconnect_delay_set(min_delay=1, max_delay=10)
    try:
        client.connect(MQTT_HOST, MQTT_PORT, keepalive=30)
        client.loop_start()
        info = client.publish(topic, payload, qos=1, retain=False)
        if info.rc != mqtt.MQTT_ERR_SUCCESS:
            raise HTTPException(503, f"MQTT publish failed: {info.rc}")
        info.wait_for_publish(timeout=5)
        if not info.is_published():
            raise HTTPException(504, "MQTT publish acknowledgement timed out")
    except HTTPException:
        raise
    except ValueError as error:
        raise HTTPException(504, "MQTT publish acknowledgement timed out") from error
    except (OSError, mqtt.MQTTException) as error:
        raise HTTPException(503, f"MQTT unavailable: {error}") from error
    finally:
        client.loop_stop()
        try:
            client.disconnect()
        except mqtt.MQTTException:
            pass


@app.get("/health")
def health_check():
    exists = DB_PATH.exists()
    return {
        "status": "ok" if exists else "waiting_for_telemetry",
        "database_exists": exists,
    }


@app.get("/api/v1/devices")
def list_devices():
    """List devices known by the ingestor, with their latest telemetry."""
    connection = get_connection()
    try:
        rows = latest_rows(connection)
        items = []
        for row in rows:
            items.append(
                {
                    "device_id": row["device_id"],
                    "online": is_online(row["received_at"]),
                    "last_seen": row["received_at"],
                    "telemetry": row_to_telemetry(row),
                }
            )
        return {"count": len(items), "items": items}
    finally:
        connection.close()


@app.post("/api/v1/devices/{device_id}/commands/pump")
def command_pump(device_id: str, command: PumpCommand):
    """Publish a pump command for the ESP32 device over MQTT."""
    request_id = uuid4().hex
    publish_pump_command(device_id, command, request_id)
    return {
        "status": "queued",
        "device_id": device_id,
        "request_id": request_id,
        "state": command.state,
        "duration_s": command.duration_s,
    }


@app.get("/api/v1/devices/{device_id}/status")
def get_device_status(device_id: str):
    connection = get_connection()
    try:
        row = connection.execute(
            """
            SELECT * FROM telemetry
            WHERE device_id = ?
            ORDER BY received_at DESC LIMIT 1
            """,
            (device_id,),
        ).fetchone()
        if row is None:
            raise HTTPException(404, f"Device {device_id} was not found")
        return {
            "device_id": device_id,
            "online": is_online(row["received_at"]),
            "last_seen": row["received_at"],
            "telemetry": row_to_telemetry(row),
        }
    finally:
        connection.close()


@app.get("/api/v1/devices/{device_id}/telemetry/latest")
def get_latest_telemetry(device_id: str):
    connection = get_connection()
    try:
        row = connection.execute(
            """
            SELECT * FROM telemetry
            WHERE device_id = ?
            ORDER BY received_at DESC LIMIT 1
            """,
            (device_id,),
        ).fetchone()
        if row is None:
            raise HTTPException(404, f"No telemetry for {device_id}")
        return row_to_telemetry(row)
    finally:
        connection.close()


@app.get("/api/v1/devices/{device_id}/telemetry")
def get_telemetry(
    device_id: str,
    limit: int = Query(default=100, ge=1, le=1000),
    from_ts: Optional[str] = Query(default=None, alias="from"),
    to_ts: Optional[str] = Query(default=None, alias="to"),
):
    connection = get_connection()
    try:
        conditions = ["device_id = ?"]
        parameters = [device_id]
        if from_ts:
            conditions.append("timestamp >= ?")
            parameters.append(from_ts)
        if to_ts:
            conditions.append("timestamp <= ?")
            parameters.append(to_ts)

        query = f"""
            SELECT * FROM (
                SELECT * FROM telemetry
                WHERE {' AND '.join(conditions)}
                ORDER BY received_at DESC LIMIT ?
            ) ORDER BY received_at ASC
        """
        rows = connection.execute(query, [*parameters, limit]).fetchall()
        return {
            "device_id": device_id,
            "count": len(rows),
            "items": [row_to_telemetry(row) for row in rows],
        }
    finally:
        connection.close()


app.mount(
    "/",
    StaticFiles(directory=BASE_DIR / "dashboard", html=True),
    name="dashboard",
)
