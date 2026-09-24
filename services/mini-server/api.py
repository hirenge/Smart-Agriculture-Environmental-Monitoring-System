from datetime import datetime, timezone
from pathlib import Path
from typing import Optional
import sqlite3

from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles


BASE_DIR = Path(__file__).resolve().parent
DB_PATH = BASE_DIR / "data" / "telemetry.db"
ONLINE_TIMEOUT_SECONDS = 900

app = FastAPI(title="Smart Farm Mini Server API", version="0.2.0")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=False,
    allow_methods=["GET"],
    allow_headers=["*"],
)


def get_connection():
    if not DB_PATH.exists():
        raise HTTPException(503, "Telemetry database does not exist")
    connection = sqlite3.connect(DB_PATH, timeout=10)
    connection.row_factory = sqlite3.Row
    return connection


def row_to_telemetry(row):
    return {
        "device_id": row["device_id"],
        "boot_id": row["boot_id"],
        "seq": row["seq"],
        "timestamp": row["timestamp"],
        "received_at": row["received_at"],
        "temperature_c": row["temperature_c"],
        "humidity_pct": row["humidity_pct"],
        "soil_moisture_pct": row["soil_moisture_pct"],
        "light_value": row["light_value"],
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


@app.get("/health")
def health_check():
    exists = DB_PATH.exists()
    return {
        "status": "ok" if exists else "waiting_for_telemetry",
        "database_exists": exists,
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
