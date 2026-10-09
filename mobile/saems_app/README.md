# SAEMS mobile app

Flutter Android client for the Smart Agriculture Environmental Monitoring System.

## Current scope

- Dashboard with source and `data_mode` labels.
- Mock telemetry every 10 seconds.
- Mini-server latest/history/status and pump command support.
- Local SQLite fallback cache.
- Wi-Fi AP REST client for the firmware API at `192.168.4.1`.
- Real BLE scan/connect/read/notify/config/command using flutter_blue_plus;
  Android runtime permission flow and persistent selected remote ID.
- Wi-Fi/MQTT provisioning over BLE or the local Wi-Fi AP.
- Settings and diagnostics without exposing API tokens.

BLE selection now uses the native adapter, not FakeBleAdapter or mock telemetry.
FakeBleAdapter is retained only for automated protocol tests. Real BLE/AP radio
compatibility and Wi-Fi switching still require a phone and an ESP32-S3.

Internet server deployment and hardware checklist:
[Internet and provisioning](../../docs/internet-and-provisioning.md).

## Run

```powershell
flutter pub get
flutter run --dart-define=ENABLE_MOCK_MODE=true
```

Use the deployed Internet mini-server (do not compile tokens into the APK):

```powershell
flutter run `
  --dart-define=ENABLE_MOCK_MODE=false `
  --dart-define=API_BASE_URL=https://admin.tailb1f0ba.ts.net `
  --dart-define=DEFAULT_DEVICE_ID=farm-01
```

In Settings select Server, enter the HTTPS URL and the API access token from the
private device-access.json. The token is stored in FlutterSecureStorage, not in
shared preferences or logs. Existing saved settings override build defaults.

The network security configuration permits cleartext only for the local AP
192.168.4.1, including release builds. Internet connections must use HTTPS.

For BLE: Direct connection -> Scan -> select the SAEMS device -> Configure ESP32.
For AP: join the ESP32 AP in Android Wi-Fi settings, select Wi-Fi AP in the app,
then Configure ESP32. Save the new Wi-Fi/MQTT credentials and restart the ESP32.

## Verify

```powershell
dart format --set-exit-if-changed .
flutter analyze
flutter test
flutter build apk --debug
```
