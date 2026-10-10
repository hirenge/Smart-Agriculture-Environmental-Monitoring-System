# SAEMS Internet deployment and provisioning — 2026-10-04

## Latest hardware session: new firmware flashed

The later user request authorized build/flash after disconnecting relay loads.
COM3 now runs modular SAEMS: real DHT20/BH1750 → Internet MQTT TLS → normal
SAEMS ingestor → SQLite → HTTPS app. A24 BLE read/notify/config and WPA2 AP
telemetry/config were tested; A24 cellular HTTPS and Pixel_6 Dashboard/history
passed. Final APK installed on both; Flutter analyze and 19 tests pass.
See [new firmware acceptance](new-firmware-device-server-app-acceptance.md)
for backups, fixes, hashes and explicit limits. No pump command was sent.

REST provisioning currently also listens on the STA interface without owner
authentication; restrict to AP or add authentication before shared-LAN use.
BLE encryption/Just Works is not owner authentication. ESP32 Wi-Fi switching,
cold power-cycle, offline FIFO through power loss and relay remain unaccepted.

## Running deployment

| Component | Address / location |
| --- | --- |
| Dashboard / REST API | https://admin.tailb1f0ba.ts.net |
| MQTT TLS for ESP32 | mqtts://admin.tailb1f0ba.ts.net:8443 |
| API origin on Pi | 127.0.0.1:3000 |
| Private broker on Pi | 127.0.0.1:1884 |
| New deployment | /home/admin/saems-mini-server |
| Previous deployment, preserved | /home/admin/smartfarm-mini-server |
| Private local credentials | merge_github/.local/device-access.json (Git ignored) |

Tailscale Funnel provides public HTTPS and TLS-terminated MQTT. Remote clients
do not need Tailscale or the bkit_rooftop Wi-Fi. The Pi must remain powered on,
connected to the Internet, and logged into its current Tailscale account.
Funnel is tied to that account/hostname; changing the account requires updating
the URL and ESP32 MQTT URI. The existing LAN broker on port 1883 was not changed.
TLS terminates in tailscaled on the Pi; HTTP/MQTT plaintext is loopback-only.

Four systemd services are enabled across reboot: saems-mqtt, saems-ingestor,
saems-api and the read-only saems-legacy-ingestor compatibility receiver.
Funnel configuration is persisted using --bg. A real Pi reboot
has not been performed, so reboot survival remains an acceptance test.

## Authentication

- App/API: Authorization: Bearer <api_access_token>.
- Dashboard browser login: username saems, password = api_access_token.
- /health is public and contains no telemetry/secrets. Dashboard, /docs,
  /openapi.json and /api/v1 routes require authentication.
- Public mode refuses startup without a token of at least 32 characters.
- MQTT user saems_esp32 can publish only saems/farm-01/telemetry and read
  saems/farm-01/commands. saems_server reads telemetry and publishes commands.
- Credentials are generated on Pi, never committed or embedded in APKs.
  Local export was explicitly approved; the file has restricted Windows ACL.

This is a single-team prototype token, not multi-user RBAC. Rotate it if leaked.
Remote administration is NOT yet accepted: an actual attempt to run
ssh admin@100.104.200.110 returned
"tailscale: tailnet policy does not permit you to SSH to this node".
The earlier TCP port check did not prove SSH login authorization. The laptop
and Pi appear online but have different node owners; the tailnet administrator
must review both network access and Tailscale SSH rules for this user/node.
No access policy was changed or bypassed during this test.
See https://tailscale.com/docs/features/tailscale-ssh.
After the owner grants access, repeat the actual SSH login from another network.
SSH is not exposed by Funnel;
dashboard/API public access does not grant shell or system configuration access.
The public API supports telemetry reads and bounded pump commands; it does not
expose a server admin shell, arbitrary server configuration, or remote Wi-Fi
credential changes. Device Wi-Fi/MQTT provisioning is local through BLE/AP.
OTA, owner-authenticated BLE pairing, device command ACK, rate limiting and
per-device MQTT accounts for additional nodes remain separate enhancements.

## App installation and settings

The tested APK is mobile/saems_app/build/app/outputs/flutter-apk/app-debug.apk.
Build the Internet-targeted APK without secrets:

```powershell
flutter build apk --debug --dart-define=ENABLE_MOCK_MODE=false --dart-define=API_BASE_URL=https://admin.tailb1f0ba.ts.net
```

In Settings choose Server, URL https://admin.tailb1f0ba.ts.net, device farm-01
and API token from the private credential file. Cached/saved settings take
precedence over build-time defaults; change a previously saved mock/LAN source.
Only the ESP32 AP IP 192.168.4.1 is allowed cleartext HTTP by Android config.

## ESP32 provisioning

These steps apply to the modular firmware now running on COM3. The earlier
read-only phone session kept old firmware unchanged; the later session
explicitly authorized flash. It tested BLE/AP config read/write without
switching ESP32 Wi-Fi or sending relay commands.

1. Flash firmware/esp32s3, not the sample project at the repository root.
   In the ESP-IDF shell, use idf.py -B build-internet -p <actual COM port> flash monitor.
2. Android: turn Bluetooth on, grant Nearby Devices permission. Older Android
   versions also require location permission and possibly location services.
3. Direct connection -> Scan -> select SAEMS device -> Configure ESP32.
   Alternatively join the ESP32 Wi-Fi AP manually and choose Wi-Fi AP in app.
4. Read config, enter the new Internet-capable Wi-Fi SSID/password, MQTT URI
   mqtts://admin.tailb1f0ba.ts.net:8443 and mqtt_username/mqtt_password from the
   private file. Do not use the API token as the MQTT password.
5. Save, restart ESP32. Check Wi-Fi IP, time sync and MQTT TLS connection logs.
   Firmware validates server hostname, trusted certificate bundle and
   certificate dates. MQTT TLS waits for SNTP-valid time after Wi-Fi gets IP;
   never disable validation to make a failed TLS connection work.
6. Switch app to Server; verify telemetry via cellular data while ESP32 uses a
   different Wi-Fi/hotspot. BLE/AP still requires local proximity to the ESP32.

An ESP32-S3 requires compatible 2.4 GHz Wi-Fi. A captive portal or enterprise
network that requires a browser login is not handled by this provisioning.
The NEW modular firmware disables LM393 without its probe/calibration; missing sensor values
stay null. Internet TLS alone does not make sensor readings real.

## Evidence and limits

- Flutter analyze: no issues; 19 automated tests pass, including BLE frame
  reconstruction, BLE/AP config forwarding, lifecycle and async Settings.
  Automated tests are not substitutes for the separate A24 radio tests.
- Android APK builds successfully.
- ESP-IDF 6.0.3 firmware builds and was flashed on COM3. BLE encrypted config
  writes and MQTT TLS were tested after fixing host-task stack overflow and
  SNTP/time-date verification; full cold power-cycle/NVS durability is pending.
- Public Google DNS resolved Funnel addresses 103.84.155.217/103.84.155.153
  during testing. Tests deliberately connected to a public IP while preserving
  the hostname/SNI; normal clients should use DNS, never hardcode these IPs.
- Missing/wrong token and malformed Basic return 401; valid Bearer/Basic 200.
- Public MQTT TLS rejects wrong passwords; device publish ACL rejects another
  device's topic. Telemetry traversed public TLS -> ingestor -> SQLite -> HTTPS.
- Three temporary simulated samples published twice across reconnect resulted
  in exactly three rows. Only these test-run rows were removed afterwards.
- At the initial migration, old/new databases each contained 398 historical
  rows and passed SQLite integrity_check. The new database now also receives
  live legacy firmware samples. Old source/database/broker were kept unchanged.
- This confirms server-side dedup/reconnect, not ESP32 offline NVS durability.
  The 32-record firmware FIFO rejects new samples when full, preserving the
  in-flight head until PUBACK. Samples beyond capacity are dropped and logged.
- Earlier read-only hardware session: Samsung SM-A245F and ESP32 COM3 were connected.
  Real DHT20/BH1750 telemetry reached the phone through public HTTPS with Wi-Fi
  disabled and cellular transport active; Wi-Fi was restored afterwards.
  Emulator/phone offline-cache and real-phone history/settings persistence were
  tested. BLE permission/off-state checks passed then; later new-firmware
  BLE/AP tests passed as described above. ESP32 switching Wi-Fi, offline NVS
  recovery and cold power-cycle remain pending.
  See [hardware-app-acceptance-2026-10-04.md](hardware-app-acceptance-2026-10-04.md).

## Read-only compatibility with old firmware (preserved)

The user authorized adding `legacy_mqtt_ingestor.py` without changing firmware.
`saems-legacy-ingestor` reads only `smartfarm/farm-01/telemetry` from the existing
LAN broker on 127.0.0.1:1883. It maps the topic envelope to SAEMS and reuses the
existing payload validation, SQLite storage and deduplication. Sensor payload,
boot ID, sequence and quality flags are preserved. It never publishes commands.
The three compatibility unit tests pass on the Pi.

Private credentials are in `/home/admin/saems-mini-server/.legacy.env` (mode 600),
with a restricted-ACL local copy `.local/legacy-reader.env`; both are Git ignored.
The service template is `services/mini-server/systemd/saems-legacy-ingestor.service.example`.
For this deployment, install it as `saems-legacy-ingestor.service` and enable it.
Stop only this receiver with `sudo systemctl disable --now saems-legacy-ingestor`;
this does not delete telemetry or change the original broker/firmware.

This bridge enables old ESP32 LAN MQTT -> SQLite -> public HTTPS -> app.
It does NOT move the old ESP32 to Internet MQTT TLS or add BLE/AP to that firmware.
The newly flashed firmware now publishes directly through Internet MQTT TLS;
this compatibility service is preserved, not required for its new samples.
The app's legacy soil value of 0% is not a calibrated soil measurement; its probe
is still missing. Unsynchronized device timestamps use server receipt time.

## Five terminal roles from the Google Doc

The original guide uses old Smartfarm paths/topics and simulated firmware.
Do not start duplicate ingestors/APIs now that systemd owns them. Equivalent
updated monitoring commands follow; terminal 5 needs the physical ESP32.

1. Pi: journalctl -u saems-ingestor -u saems-legacy-ingestor -f
2. Pi: journalctl -u saems-mqtt -f (broker/ACL diagnostic logs).
3. Pi: journalctl -u saems-api -f
4. Pi: watch -n 2 "sqlite3 /home/admin/saems-mini-server/data/telemetry.db 'SELECT device_id,boot_id,seq,received_at FROM telemetry ORDER BY id DESC LIMIT 5;'"
5. Windows: serial monitor on COM3 when needed. The later session authorized
   and completed flash; do not run duplicate monitors while flashing. Opening
   a serial port can affect reset lines; use the finite reader with reset
   lines disabled for routine diagnostics.

The initial deployment used persistent services; the first phone session used
finite serial reads without flash. The later new-firmware session used a full
flash backup, scoped partition migration, build/flash and finite UART checks.
Monitoring roles do not require starting duplicate services.
Public regression test on Pi:

```bash
cd /home/admin/saems-mini-server
.venv/bin/python scripts/check_internet.py
systemctl is-active saems-mqtt saems-ingestor saems-legacy-ingestor saems-api tailscaled
sudo tailscale funnel status
```

The test intentionally inserts then deletes only uniquely tagged test records.
Its latest-row assertions can race live telemetry; use an isolated test device
or per-fixture queries rather than interpreting that race as an ingestion fault.

## Recreate and stop the public exposure

Deploy this module to /home/admin/saems-mini-server, install requirements in
.venv, run scripts/prepare_runtime.py once. It generates a private broker
password/ACL and backs up the previous SQLite DB if present, without changing
the old deployment. Existing .env is not overwritten. Install the three
systemd/*.service.example templates using their .service names and copy
config/saems-private-mqtt.conf.example to config/mqtt.conf.
The example service paths target the actual admin user on this Pi.

After proving authentication locally, the enabled Funnel commands are:

```bash
sudo tailscale funnel --bg --https=443 http://127.0.0.1:3000
sudo tailscale funnel --bg --tls-terminated-tcp=8443 tcp://127.0.0.1:1884
```

To stop only this public deployment (does not erase source/history):

```bash
sudo tailscale funnel --https=443 off
sudo tailscale funnel --tls-terminated-tcp=8443 off
sudo systemctl disable --now saems-api saems-ingestor saems-legacy-ingestor saems-mqtt
```
