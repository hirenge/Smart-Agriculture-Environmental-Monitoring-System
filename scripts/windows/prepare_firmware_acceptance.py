"""Mechanically configure the ignored SDK config for an authorized hardware test.

Read Wi-Fi from the old local SDK config and device MQTT login from the private
export. Never display values. Preserve the previous SDK config in .local.
"""
import json
from pathlib import Path
import secrets
import shutil

repo = Path(__file__).resolve().parents[2]
private = repo / ".local"
private.mkdir(exist_ok=True)
config_path = repo / "firmware/esp32s3/sdkconfig"
original_path = repo.parent / "sdkconfig"
old = {}
for line in original_path.read_text(encoding="utf-8").splitlines():
    key, separator, value = line.partition("=")
    if separator and key in (
        "CONFIG_SMARTFARM_WIFI_SSID", "CONFIG_SMARTFARM_WIFI_PASSWORD"
    ):
        old[key] = json.loads(value)
if not old.get("CONFIG_SMARTFARM_WIFI_SSID"):
    raise RuntimeError("No existing Wi-Fi SSID; configure locally before flash")
access = json.loads((private / "device-access.json").read_text(encoding="utf-8-sig"))
ap_path = private / "firmware-ap-access.json"
if ap_path.exists():
    ap_access = json.loads(ap_path.read_text(encoding="utf-8"))
else:
    ap_access = {"ssid": "SAEMS-farm-01", "password": secrets.token_urlsafe(18)}
    with ap_path.open("x", encoding="utf-8") as handle:
        json.dump(ap_access, handle, indent=2)
backup = private / "sdkconfig-before-hardware-test"
if not backup.exists():
    shutil.copy2(config_path, backup)
settings = {
    "CONFIG_SAEMS_WIFI_SSID": json.dumps(old["CONFIG_SMARTFARM_WIFI_SSID"]),
    "CONFIG_SAEMS_WIFI_PASSWORD": json.dumps(old.get("CONFIG_SMARTFARM_WIFI_PASSWORD", "")),
    "CONFIG_SAEMS_MQTT_BROKER_URI": json.dumps("mqtts://admin.tailb1f0ba.ts.net:8443"),
    "CONFIG_SAEMS_MQTT_USERNAME": json.dumps(access["mqtt_username"]),
    "CONFIG_SAEMS_MQTT_PASSWORD": json.dumps(access["mqtt_password"]),
    "CONFIG_SAEMS_DEVICE_ID": json.dumps(access["device_id"]),
    "CONFIG_SAEMS_BLE_ENABLED": "y",
    "CONFIG_SAEMS_AP_ENABLED": "y",
    "CONFIG_SAEMS_AP_PASSWORD": json.dumps(ap_access["password"]),
    "CONFIG_SAEMS_SOIL_SENSOR_ENABLED": "n",
    "CONFIG_MBEDTLS_HAVE_TIME_DATE": "y",
    "CONFIG_LWIP_SNTP_MAX_SERVERS": "3",
    "CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE": "8192",
    "CONFIG_BT_NIMBLE_TASK_STACK_SIZE": "8192",
}
lines = []
seen = set()
for line in config_path.read_text(encoding="utf-8").splitlines():
    key = line.split("=", 1)[0]
    if line.startswith("# CONFIG_") and line.endswith(" is not set"):
        key = line[2:].removesuffix(" is not set")
    if key in settings:
        value = settings[key]
        line = f"# {key} is not set" if value == "n" else f"{key}={value}"
        seen.add(key)
    lines.append(line)
for key in settings.keys() - seen:
    value = settings[key]
    lines.append(f"# {key} is not set" if value == "n" else f"{key}={value}")
config_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
print("Prepared ignored SDK config: MQTT TLS, BLE and password-protected AP; soil disabled")
print("Original SDK config backed up privately; credential values omitted")
