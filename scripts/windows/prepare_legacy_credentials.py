"""Generate a Git-ignored runtime login file without displaying credentials.

For the explicitly approved read-only receiver on the same original Pi broker.
Only MQTT login keys are copied; Wi-Fi/AP credentials are never exported.
"""
import json
import os
from pathlib import Path
import re

repo = Path(__file__).resolve().parents[2]
source = repo.parent / "sdkconfig"
output = repo / ".local" / "legacy-reader.env"
names = {
    "CONFIG_SMARTFARM_MQTT_USERNAME": "MQTT_USERNAME",
    "CONFIG_SMARTFARM_MQTT_PASSWORD": "MQTT_PASSWORD",
}
selected = {}
for line in source.read_text(encoding="utf-8").splitlines():
    key, separator, value = line.partition("=")
    if separator and key in names:
        selected[names[key]] = json.loads(value)
if not all(selected.get(name) for name in names.values()):
    raise RuntimeError("Existing legacy MQTT login is incomplete")
# The known firmware uses simple printable ASCII; reject unsupported values
# rather than incorrectly reinterpreting an environment-file escape.
if any(not re.fullmatch(r"[A-Za-z0-9_@.\-]+", value) for value in selected.values()):
    raise RuntimeError("Credential encoding requires manual private setup")
os.umask(0o077)
output.parent.mkdir(exist_ok=True)
with output.open("x", encoding="utf-8") as handle:
    for key, value in selected.items():
        handle.write(f"{key}={value}\n")
print("Generated private legacy MQTT runtime file; no secret values displayed")
