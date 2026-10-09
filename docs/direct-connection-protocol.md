# Direct ESP32 connection protocol

This contract is implemented by the ESP32-S3 firmware for direct BLE and Wi-Fi
AP communication. Wi-Fi STA + MQTT remains the Internet/server transport.

## BLE GATT

Service UUID: `a0f00001-6c7a-4d70-9e3b-9a2c00000001`

| Characteristic | UUID suffix | Access |
| --- | --- | --- |
| Device Info | `0002` | read |
| Latest Telemetry | `0003` | read, notify |
| Device Config | `0004` | read, write |
| Command | `0005` | write |

Reads and writes are plain UTF-8 JSON (up to 512 bytes per characteristic).
Telemetry notifications always use the following frames, even with one frame:

```text
message_id_hi | message_id_lo | frame_index | frame_total | UTF-8 bytes...
```

- Frames may arrive out of order and are keyed by 16-bit `message_id`.
- Index starts at zero and must be smaller than `frame_total`.
- An incomplete message expires after five seconds.
- Commands reuse the JSON body used by the server pump endpoint.

## Wi-Fi AP REST

Default base URL: `http://192.168.4.1`.

| Method | Path |
| --- | --- |
| GET | `/v1/health` |
| GET | `/v1/status` |
| GET | `/v1/telemetry/latest` |
| GET | `/v1/telemetry/history?limit=100` |
| GET | `/v1/config` |
| PUT | `/v1/config` |
| POST | `/v1/commands/pump` |

Config writes may contain Wi-Fi/MQTT/AP credentials and manager settings. Secret
values are persisted but never returned by `GET /v1/config`. Transport and sensor
changes become fully active after device restart.

Config/command writes require an encrypted BLE link. Android bonds before
writing; ESP32 persists bond keys in NVS. No-input/no-output Just Works pairing
does not authenticate the owner's identity or provide MITM protection; provision
in a physically controlled area. Owner/passkey provisioning is future hardening.

The app permits cleartext HTTP only to the local AP address 192.168.4.1,
including release builds. Internet deployments use HTTPS. New Wi-Fi/MQTT
credentials become active after an ESP32 restart; reads do not return secrets.
