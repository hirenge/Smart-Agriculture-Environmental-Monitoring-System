# SAEMS mobile app

Ứng dụng Flutter Android hiển thị dữ liệu môi trường và kết nối với ESP32-S3 qua
ba đường: HTTPS Internet, BLE và Wi-Fi AP. Mock mode chỉ dùng cho phát triển/test,
không phải dữ liệu nghiệm thu cảm biến.

## Chức năng hiện tại

- Dashboard, lịch sử, trạng thái thiết bị và nhãn nguồn dữ liệu.
- Nguồn `Server`: REST API HTTPS, Bearer token và SQLite cache cục bộ.
- Nguồn `Bluetooth`: scan/connect/read/notify telemetry và provisioning.
- Nguồn `Wi-Fi AP`: REST API firmware tại `http://192.168.4.1`.
- Provisioning Wi-Fi/MQTT và đổi `power_mode` qua BLE hoặc AP.
- Điều khiển bơm có giới hạn thời gian; kết quả phần cứng đọc lại từ telemetry.
- Token API lưu bằng Flutter Secure Storage và không xuất hiện trong log.
- Cleartext chỉ được cho phép với AP cục bộ `192.168.4.1`; Internet bắt buộc
  HTTPS.

## Cấu trúc ứng dụng

```text
lib/src/
├─ core/                 Cấu hình, theme và lỗi dùng chung
├─ models/               Telemetry, thiết bị và nguồn dữ liệu
├─ data/
│  ├─ server_api_transport.dart
│  ├─ wifi_ap_device_transport.dart
│  ├─ ble/               FlutterBluePlus và BLE frame reassembly
│  ├─ telemetry_cache.dart
│  └─ settings_store.dart
├─ repositories/         Hợp nhất remote/local cache
├─ state/                Riverpod state
└─ presentation/         Dashboard, history, settings, nearby, provisioning
```

## Chạy app bằng script demo

Chạy từ **thư mục gốc repository**. Pixel 6 phải được mở trước trong Android
Studio Device Manager; A24 phải bật USB debugging và chấp nhận quyền ADB.

```powershell
Set-Location 'C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github'

# Chạy và xem log trên Pixel 6 emulator
.\scripts\windows\run_app.ps1 -Action Run -Target Pixel6

# Chạy trên Samsung A24 thật
.\scripts\windows\run_app.ps1 -Action Run -Target A24

# Build hoặc cài APK debug
.\scripts\windows\run_app.ps1 -Action Build
.\scripts\windows\run_app.ps1 -Action Install -Target A24
```

Sau khi `Run`, thao tác trên màn hình điện thoại/emulator; terminal chỉ hiển
thị Flutter log. Nhấn `q` trong terminal khi muốn dừng phiên debug.

APK được tạo tại:

```text
mobile/saems_app/build/app/outputs/flutter-apk/app-debug.apk
```

Script dùng môi trường Flutter/Puro và Android SDK đã cài trên laptop hiện tại.
Trên máy khác, có thể dùng trực tiếp `flutter pub get` và `flutter run` sau
khi cập nhật đường dẫn SDK.

## Demo nguồn Server

1. Mở `Settings` và chọn nguồn `Server`.
2. URL là `https://admin.tailb1f0ba.ts.net`.
3. Nhập API access token từ file riêng tư `.local/device-access.json`.
4. Chọn `farm-01`, lưu cấu hình rồi mở Dashboard/History.
5. `data_mode` phải là `real`; giá trị phải khớp lệnh
   `run_server.ps1 -Action Latest`.

Không đưa token vào `--dart-define`, APK hoặc Git.

## Demo BLE offline

1. Để ESP32 ở `direct` hoặc `light` để BLE luôn sẵn sàng.
2. Cô lập Internet của A24:

   ```powershell
   .\scripts\windows\phone_ble_test.ps1 -Action Isolate
   ```

3. Trong app chọn kết nối trực tiếp/Bluetooth, scan `SAEMS-farm-01`, kết nối và
   xem telemetry.
4. Mở màn hình cấu hình ESP32 để đổi Wi-Fi/MQTT hoặc chọn `Light Sleep`.
5. Kết thúc demo và khôi phục mạng:

   ```powershell
   .\scripts\windows\phone_ble_test.ps1 -Action Restore
   ```

## Demo Wi-Fi AP offline

1. Kết nối A24 vào SSID AP của ESP32.
2. Có thể dùng script ADB để cô lập/khôi phục mạng:

   ```powershell
   .\scripts\windows\phone_ap_test.ps1 -Action Isolate
   .\scripts\windows\phone_ap_test.ps1 -Action Restore
   ```

3. Trong app chọn nguồn `Wi-Fi AP`, mở Dashboard và màn hình cấu hình ESP32.
4. Telemetry phải đến từ `192.168.4.1` dù server/Internet không truy cập được.

Trong `battery`, AP/BLE chỉ hoạt động ở cửa sổ thức. Dùng `direct` hoặc
`light` cho buổi demo cấu hình dài.

## Kiểm thử source app

Từ `mobile/saems_app`:

```powershell
flutter pub get
dart format --set-exit-if-changed .
flutter analyze
flutter test
flutter build apk --debug
```

Bộ test hiện có 21 test cho BLE lifecycle/frame, HTTP error redaction, BLE/AP
provisioning, settings, telemetry model và giao diện cơ bản.

## Giới hạn

- BLE/AP cần điện thoại thật để nghiệm thu radio; emulator không thay thế A24.
- BLE Just Works chưa có owner/MITM authentication.
- AP local chưa phải kênh quản trị qua Internet và không được port-forward.
- Relay/tải thật chỉ được test sau khi phần điện đã ở trạng thái an toàn.

Luồng demo đầy đủ: [scripts/windows/RUN-DEMO.md](../../scripts/windows/RUN-DEMO.md).
Báo cáo hiện trạng: [IMPLEMENTATION-REPORT.md](../../docs/IMPLEMENTATION-REPORT.md).
