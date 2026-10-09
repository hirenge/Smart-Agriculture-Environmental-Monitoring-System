"""Run idf.py while redacting local credentials from its default-value logs."""
import json
from pathlib import Path
import re
import subprocess
import sys

repo = Path(__file__).resolve().parents[2]
secrets_to_redact = set()
for path in (repo.parent / "sdkconfig", repo / "firmware/esp32s3/sdkconfig"):
    for line in path.read_text(encoding="utf-8").splitlines():
        key, separator, value = line.partition("=")
        if separator and ("PASSWORD" in key or "TOKEN" in key):
            try:
                decoded = json.loads(value)
                if isinstance(decoded, str) and decoded:
                    secrets_to_redact.add(decoded)
            except json.JSONDecodeError:
                pass
for name in ("device-access.json", "firmware-ap-access.json"):
    path = repo / ".local" / name
    if path.exists():
        values = json.loads(path.read_text(encoding="utf-8-sig"))
        secrets_to_redact.update(str(value) for key, value in values.items()
                                if value and re.search(r"password|token", key, re.I))
command = [sys.executable, "C:/esp/v6.0.3/esp-idf/tools/idf.py", *sys.argv[1:]]
process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, encoding="utf-8", errors="replace")
for line in process.stdout:
    for value in sorted(secrets_to_redact, key=len, reverse=True):
        line = line.replace(value, "[REDACTED]")
    print(line, end="", flush=True)
sys.exit(process.wait())
