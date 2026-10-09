# SAEMS — nghiệm thu firmware mới, server và app

## Phạm vi

Phiên này thay thế trạng thái **giữ firmware cũ** của
[phiên đọc dữ liệu trước](hardware-app-acceptance-2026-10-04.md).
Người dùng yêu cầu build/flash firmware mới và xác nhận đã ngắt tải relay.
Không gửi lệnh bật/tắt bơm, không sửa Google Doc, không commit/push Git.
Phiên kiểm thử đi qua nửa đêm giờ Bangkok; thời gian mẫu bên dưới có múi giờ.

- ESP32-S3 rev 0.2, flash 16 MB, COM3; module `firmware/esp32s3`, không phải
  project mẫu `hello_world` ở root.
- Samsung A24 SM-A245F, Android 16, USB debugging.
- Android Studio mở `mobile/saems_app`; emulator Pixel_6 chạy song song.
- Pi: `/home/admin/saems-mini-server`; không thay/xóa broker, source hoặc DB cũ.

## Flash có backup

Trước flash đã đọc toàn bộ flash 16 MB vào
`.local/esp32-before-new-firmware.bin`, SHA-256:
`29ba4091539132bc09f463c6eff0c7c808c6319ff29e388f8157d2c5b7f26984`.
NVS cũ 24 KiB tại `0x9000` được giữ và có bản riêng
`.local/esp32-original-nvs.bin`. SDK config trước kiểm thử cũng có backup.

Partition mới mở rộng NVS tới 192 KiB. Chỉ vùng mở rộng
`0xf000..0x39000` được erase trước flash, vì vùng này chứa PHY/app cũ;
không erase toàn chip và không xóa phần NVS cũ. Firmware/partition/bootloader
mới đã được flash thành công. Backup cho phép khôi phục, nhưng thao tác khôi phục
cũng ghi đè firmware/dữ liệu mới và cần được người dùng yêu cầu riêng.

## Đường dữ liệu hiện tại

```text
ESP32: DHT20 + BH1750 thật
  → MQTT TLS admin.tailb1f0ba.ts.net:8443
  → saems-mqtt → saems-ingestor → SQLite
  → HTTPS API có Bearer token → A24 (Wi-Fi/4G) + Pixel_6

A24 ở gần thiết bị → BLE GATT hoặc Wi-Fi AP → ESP32
```

Firmware mới publish `saems/farm-01/telemetry` trực tiếp, không còn phụ thuộc
bridge `smartfarm/farm-01/telemetry`. Legacy ingestor vẫn active để tương thích;
không xóa service hoặc lịch sử cũ.

## Kết quả và phạm vi bằng chứng

| Hạng mục | Kết quả |
| --- | --- |
| Firmware build/flash | Đạt với ESP-IDF 6.0.3, modular SAEMS, COM3 |
| DHT20/BH1750 | Đạt; nhiệt độ/độ ẩm không khí/ánh sáng từ cảm biến vật lý |
| Soil LM393 | Disabled; payload `null`, app `--`, không hiển thị phần trăm giả |
| SNTP + MQTT TLS | Đạt; đồng bộ khoảng 6 giây sau IP, kiểm tra hostname/trust bundle/ngày certificate được bật |
| Startup buffering | Quan sát được hai mẫu chờ lúc đồng bộ thời gian rồi gửi sau MQTT connect/PUBACK |
| BLE A24 | Đạt scan, connect, device info, telemetry read/notify và config read/write |
| BLE link | Ghi cấu hình qua link encrypted/bonded; chưa có owner/MITM authentication |
| AP A24 | Đạt kết nối WPA2 tới SAEMS-farm-01, IP 192.168.4.2; dữ liệu và config read/write qua 192.168.4.1 |
| A24 HTTPS qua 4G | Đạt cả APK cuối; Wi-Fi tắt, LTE/cellular hiện diện, dữ liệu mới 00:48:06 Bangkok: 29.2°C, 79.4% RH, 103.3 lux |
| Pixel_6 | Đạt Dashboard nguồn Server và lịch sử; không dùng emulator làm bằng chứng radio BLE |
| APK cuối | Đã build và cài trên cả A24/Pixel_6; kiểm tra Settings đúng nguồn đã lưu, không tự trở về Mock |
| Runtime APK cuối | Đọc log theo PID mới trên từng target không thấy lỗi flutter:E/AndroidRuntime:E trong đoạn kiểm tra; không phải cam kết không lỗi dài hạn |
| Flutter | Analyze không lỗi; 19 automated tests đạt, gồm lifecycle BLE và async Settings |
| Pi services | `saems-api`, `saems-mqtt`, `saems-ingestor`, `saems-legacy-ingestor` active |
| SQLite | `integrity_check=ok`; 0 nhóm trùng `device_id + boot_id + seq` |
| Cấu hình sau reset | Chu kỳ 10 giây đọc lại từ thiết bị sau warm reset; không coi đây là test mất điện NVS |
| Relay | Không thử điều khiển; tải ngắt, telemetry báo off |

AP đã thử trong vòng firmware cuối trước sửa UI Settings. BLE config read/write
và 4G đã kiểm thử lại trên APK cuối; cài lại không xóa preferences/token.
Bằng chứng riêng nằm ngoài Git ở `.local/`:
`saems-final-ble-config.png`, `saems-final-4g.png`,
`saems-final-pixel-history.png`, `saems-final-ble-stack.log`,
`saems-final-apk-a24-4g.png`, `saems-final-apk-pixel.png`.
Kết quả offline cache của phiên trước vẫn là bằng chứng của phiên trước,
không tự tính thành kiểm thử mới trên APK cuối.

Sau test: chu kỳ gửi 10 giây; A24/Pixel_6 đều nguồn Server, Dashboard nhận mẫu
cùng thời điểm. A24 Wi-Fi và mobile data đã trả về trạng thái bật; Android Studio,
Pixel_6 và hai phiên Flutter debug được giữ mở để tiếp tục theo dõi.

## Các lỗi tìm thấy và sửa

1. **TLS/thời gian:** SNTP khởi động trước khi có IP dẫn tới không đồng bộ;
   đồng thời `MBEDTLS_HAVE_TIME_DATE` mặc định tắt. Network Manager khởi tạo
   SNTP ở trạng thái chưa chạy và start/restart khi có IP. Application Manager
   chỉ khởi tạo MQTT TLS khi clock hợp lệ; cảm biến/BLE/AP/offline queue vẫn chạy.
   Bật kiểm tra ngày certificate và ba SNTP servers; không tắt xác minh TLS.
   Cách khởi động này phù hợp
   [hướng dẫn SNTP của Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_netif_programming.html).
2. **BLE GATT 133/reboot:** UART/backtrace chỉ ra stack overflow trong NimBLE
   host task khi JSON/config/NVS đi qua stack mặc định 4 KiB. Tăng host stack
   lên 8 KiB, HTTP task tương tự. Ghi lại thành công không reboot; log cuối
   `ESP_OK`, `encrypted=1`, `bonded=1`, stack high-water mark 3316 byte.
   Không xóa bond hoặc bỏ yêu cầu encryption để làm test chạy.
3. **App BLE lifecycle:** notification có thể đến sau dispose hoặc kết nối
   hoàn thành muộn làm mở lại adapter đã đóng. Thêm guard, chờ open/cleanup,
   dispose idempotent và regression test; giữ short GATT writes phù hợp MTU.
4. **Settings sai nguồn:** form trong IndexedStack lấy Mock trước khi preferences
   load xong rồi không cập nhật. Chờ settings thật, theo dõi đổi source bên ngoài,
   giữ draft chưa lưu và không fallback Mock khi build tắt mock; thêm widget test.

## Artifacts cuối

| Artifact | SHA-256 |
| --- | --- |
| `firmware/esp32s3/build-internet/saems_esp32s3.bin` | `4d85a333c35db9201139b076b4b4b45170b52c709340af88c20590615e780212` |
| `mobile/saems_app/build/app/outputs/flutter-apk/app-debug.apk` | `0cea0a108555ffd5d8338a03ecc0e09fb045957cfae2d2f11894f50b34991527` |

Firmware 1,315,600 byte (`0x141310`), còn khoảng 58% OTA slot 3 MiB.
APK không chứa API token; dùng token đã lưu bảo mật trên thiết bị.
Backup, SDK config, binary, credentials, ảnh và log đã được kiểm tra Git ignore.
Runner `scripts/windows/idf_private.py` che credentials trong các build sau.
Build đầu có thông báo Kconfig chứa mật khẩu Wi-Fi/MQTT/AP; không chia sẻ raw log
hoặc firmware binary. Nếu log đã chia sẻ, cần xoay những mật khẩu liên quan.

## Cách tiếp tục thử an toàn

1. App: Cài đặt → nguồn Server, URL `https://admin.tailb1f0ba.ts.net`,
   device `farm-01`. Để trống ô token khi muốn giữ token đã lưu.
2. Xem Tổng quan/Lịch sử, tắt Wi-Fi điện thoại để kiểm tra 4G; bật lại sau test.
3. BLE: bật Bluetooth/quyền Nearby Devices, Kết nối → Quét ESP32 → chọn SAEMS.
   Thiết bị đã kết nối có thể không quảng bá và không xuất hiện trong scan mới.
4. AP: vào Wi-Fi `SAEMS-farm-01`, lấy mật khẩu từ file riêng
   `.local/firmware-ap-access.json`, sau đó chọn Wi-Fi trực tiếp trong app.
   Không dùng mật khẩu API hoặc MQTT làm mật khẩu AP.
5. Chỉ đọc cấu hình hoặc thử chu kỳ gửi; không nhập lại Wi-Fi/MQTT nếu chưa cần.
   Không thử nút bơm khi chưa xác nhận tải và polarity module relay.

## Chưa nghiệm thu / việc ưu tiên tiếp theo

- **REST local chưa được giới hạn vào interface AP.** Đã xác nhận GET cấu hình
  qua STA `192.168.1.13` mà không cần token. Source dùng HTTP listener mặc định
  và config/command handlers chưa có owner authentication. Cần giới hạn AP
  hoặc bổ sung xác thực trước khi coi thiết bị an toàn trên LAN dùng chung.
  Không gửi thử command để chứng minh điều này và không port-forward ESP32.
- BLE Just Works mã hóa nhưng chưa xác thực chủ sở hữu/MITM; cần onboarding
  owner và chế độ provisioning có giới hạn thời gian.
- ESP32 vẫn dùng Wi-Fi BKIT_ROOFTOP trong lần này; chưa đổi sang hotspot/Wi-Fi
  khác. A24 dùng 4G đã chứng minh truy cập API không cần cùng Wi-Fi với Pi.
- Chưa cold power-cycle ESP32/Pi, chưa test đầy đủ offline FIFO qua mất điện,
  queue đầy hoặc lỗi NVS. Warm reset/config read không thay thế các bài này.
- LM393 còn thiếu que và hiệu chuẩn; relay active level/tiếp điểm thực chưa
  nghiệm thu; chưa đo điện năng hoặc test sleep/wake của Power Manager.
- Remote SSH Tailscale vẫn bị policy từ chối; HTTPS app không cấp quyền quản trị
  hệ điều hành/server. Không sửa/bypass policy trong phiên này.
