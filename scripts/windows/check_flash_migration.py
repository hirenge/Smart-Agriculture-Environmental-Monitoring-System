"""Validate the actual pre-flash partition table before expanding NVS."""
import hashlib
import argparse
from pathlib import Path
import struct

repo = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--backup", default="esp32-before-new-firmware.bin",
                    help="Backup basename under ignored .local")
parser.add_argument("--snapshot", default="esp32-original-nvs.bin",
                    help="New NVS snapshot basename under ignored .local")
parser.add_argument("--base-address", type=lambda value: int(value, 0), default=0,
                    help="Flash address represented by byte zero of the input")
args = parser.parse_args()
for name in (args.backup, args.snapshot):
    if Path(name).name != name or not name.endswith(".bin"):
        raise ValueError("Specify .bin basenames, not paths")
path = repo / ".local" / args.backup
data = path.read_bytes()
if args.base_address == 0 and len(data) != 0x1000000:
    raise RuntimeError("Incomplete 16-MB backup; do not flash")
table_offset = 0x8000 - args.base_address
if table_offset < 0 or table_offset + 0xC00 > len(data):
    raise RuntimeError("Input does not contain the ESP32 partition table")
entries = {}
for offset in range(table_offset, table_offset + 0xC00, 32):
    magic, kind, subtype, address, size, label, flags = struct.unpack_from(
        "<HBBII16sI", data, offset)
    if magic != 0x50AA:
        break
    name = label.split(b"\0", 1)[0].decode("ascii")
    entries[name] = (kind, subtype, address, size)
    print(f"Previous partition: {name} offset=0x{address:x} size=0x{size:x}")
old_nvs = entries.get("nvs")
if old_nvs not in ((1, 2, 0x9000, 0x6000), (1, 2, 0x9000, 0x30000)):
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
snapshot = repo / ".local" / args.snapshot
old_end = old_nvs[2] + old_nvs[3]
print("Flash input SHA256:", hashlib.sha256(data).hexdigest())
if old_end == 0xF000:
    nvs_start = 0x9000 - args.base_address
    nvs_end = old_end - args.base_address
    if nvs_start < 0 or nvs_end > len(data):
        raise RuntimeError("Input does not contain the old NVS partition")
    with snapshot.open("xb") as handle:
        handle.write(data[nvs_start:nvs_end])
    print(f"Old NVS snapshot: {snapshot.name} ({nvs_end - nvs_start} bytes)")
    print("MIGRATION_REQUIRED=1")
    print("Preserve old NVS [0x9000,0xf000); initialize ONLY expansion [0xf000,0x39000)")
    print("Expansion overlaps obsolete phy/factory sectors, backed up before replacement")
else:
    print("MIGRATION_REQUIRED=0")
    print("NVS layout already matches; preserve [0x9000,0x39000), NO expansion erase needed")
