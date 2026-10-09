"""Validate the actual pre-flash partition table before expanding NVS."""
import hashlib
from pathlib import Path
import struct

repo = Path(__file__).resolve().parents[2]
path = repo / ".local/esp32-before-new-firmware.bin"
data = path.read_bytes()
if len(data) != 0x1000000:
    raise RuntimeError("Incomplete 16-MB backup; do not flash")
entries = {}
for offset in range(0x8000, 0x8C00, 32):
    magic, kind, subtype, address, size, label, flags = struct.unpack_from(
        "<HBBII16sI", data, offset)
    if magic != 0x50AA:
        break
    name = label.split(b"\0", 1)[0].decode("ascii")
    entries[name] = (kind, subtype, address, size)
    print(f"Previous partition: {name} offset=0x{address:x} size=0x{size:x}")
if entries.get("nvs") != (1, 2, 0x9000, 0x6000):
    raise RuntimeError("Unexpected old NVS layout; review migration, do not erase")
new_layout = (repo / "firmware/esp32s3/partitions.csv").read_text(encoding="utf-8")
for line in new_layout.splitlines():
    fields = [field.strip() for field in line.split(",")]
    if fields[0] == "nvs":
        if int(fields[3], 0) != 0x9000 or int(fields[4], 0) != 0x30000:
            raise RuntimeError("Unexpected new NVS layout; review migration")
        break
else:
    raise RuntimeError("New NVS partition not found")
snapshot = repo / ".local/esp32-original-nvs.bin"
with snapshot.open("xb") as handle:
    handle.write(data[0x9000:0xF000])
print("Full flash backup SHA256:", hashlib.sha256(data).hexdigest())
print("Preserve old NVS [0x9000,0xf000); initialize ONLY expansion [0xf000,0x39000)")
print("Expansion overlaps obsolete phy/factory sectors, backed up before replacement")
