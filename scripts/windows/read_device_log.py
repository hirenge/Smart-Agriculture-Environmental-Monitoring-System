"""Read a finite ESP32 UART trace without changing flash or relay commands."""
import argparse
import re
import time
from pathlib import Path
import sys

import serial

parser = argparse.ArgumentParser()
parser.add_argument("--port", default="COM3")
parser.add_argument("--seconds", type=float, default=15)
parser.add_argument("--output", help="Safe log basename saved under ignored .local")
args = parser.parse_args()
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
port = serial.Serial()
port.port = args.port
port.baudrate = 115200
port.timeout = 0.25
port.dtr = False
port.rts = False
port.open()
try:
    deadline = time.monotonic() + args.seconds
    captured = bytearray()
    while time.monotonic() < deadline:
        captured.extend(port.read(8192))
finally:
    port.close()
text = captured.decode("utf-8", errors="replace")
safe_lines = []
for line in text.splitlines():
    if re.search(r"password|passwd|access_token|authorization|api_key", line, re.I):
        safe_lines.append("[credential-bearing UART line omitted]")
    else:
        safe_lines.append(line)
if args.output:
    if not re.fullmatch(r"[A-Za-z0-9_-]+\.log", args.output):
        raise ValueError("Specify a .log basename, not a path")
    output = Path(__file__).resolve().parents[2] / ".local" / args.output
    output.write_text("\n".join(safe_lines) + "\n", encoding="utf-8")
for line in safe_lines:
    print(line)
if not text.strip():
    print("No UART output in capture window; no reset or flash was requested.")
