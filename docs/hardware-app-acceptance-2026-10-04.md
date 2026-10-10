# SAEMS — nghiệm thu đọc dữ liệu ngày 2026-10-04

> Đây là lịch sử phiên giữ firmware cũ. Phiên sau đã được phép flash và kiểm thử
> BLE/AP/MQTT TLS; xem [báo cáo firmware mới](new-firmware-device-server-app-acceptance.md).

## Phạm vi và thiết bị

Theo yêu cầu: giữ nguyên firmware; chỉ đọc dữ liệu, không gửi lệnh relay,
không provision Wi-Fi/MQTT, không flash và không xóa dữ liệu.
Người dùng cho phép thêm bộ nhận tương thích trên Pi.

- Laptop: Android Studio mở module `mobile/saems_app`.
- Điện thoại: Samsung SM-A245F, Android 16, USB debugging.
- Emulator: Pixel_6, chạy song song để debug UI/offline cache.
- ESP32-S3: COM3, firmware cũ đang gửi topic `smartfarm/farm-01/telemetry`.
- Pi: bản mới `/home/admin/saems-mini-server`; bản cũ được giữ nguyên.
- HTTPS: `https://admin.tailb1f0ba.ts.net`; device `farm-01`, nguồn `Server`.
- Đã đọc tab “Hiện thực Mobile App” trong Google Doc nhóm. Đường dẫn/tên
  Smartfarm và cấu hình LAN/mock cũ được đối chiếu với module SAEMS hiện tại;
  không sửa Google Doc.

## Đường dữ liệu đã kiểm tra

```text
ESP32 firmware cũ → MQTT LAN :1883 → saems-legacy-ingestor
    → SQLite SAEMS → API HTTPS có token → app Samsung dùng Wi-Fi hoặc 4G
```

Bộ nhận chỉ subscribe đúng topic telemetry của farm-01; không publish command.
Payload giữ nguyên, chỉ đổi envelope topic để dùng validator/dedup hiện có.
Cả bốn service API, broker mới, ingestor mới và legacy ingestor đang active.
Ba unit test tương thích đạt; SQLite integrity_check đạt, không có nhóm trùng
`device_id + boot_id + seq` tại thời điểm kiểm tra. Mẫu mới đến khoảng mỗi 10 giây.

## Kết quả

| Hạng mục | Kết quả thực tế |
| --- | --- |
| App chạy và debug trên Samsung | Đạt; phiên Flutter debug đang mở |
| Dashboard qua Wi-Fi | Đạt; nguồn Server, dữ liệu mới từ ESP32 |
| Dashboard qua 4G | Đạt; Wi-Fi = 0, cellular transport hiện diện, hostname resolve IP công khai |
| Bằng chứng 4G | 23:34:36 giờ Bangkok: 29.4°C, 78.8% RH, 125.8 lux; bơm báo off |
| Lịch sử | Đạt trên điện thoại và emulator; dùng nút làm mới để lấy mẫu mới |
| Offline cache | Đạt trên emulator và Samsung; giữ dữ liệu đã tải, báo Bộ nhớ cục bộ/Dữ liệu cũ khi ngắt mạng |
| Lưu cài đặt/token | Đạt qua Flutter hot restart; không nhập lại token, tiếp tục nhận dữ liệu nguồn Server |
| Quyền BLE trên Samsung | BLUETOOTH_SCAN và BLUETOOTH_CONNECT được cấp |
| Bluetooth đang tắt | App báo “Hãy bật Bluetooth rồi quét lại.”; chưa nghiệm thu radio |
| BLE/AP với ESP32 đang chạy | Chưa đạt điều kiện test: firmware cũ không có BLE GATT/AP của bản modular mới |
| Relay | Không thử điều khiển; chỉ đọc trạng thái off |
| SSH quản trị từ khác mạng | Chưa đạt: Tailscale SSH từ chối theo tailnet policy |
| ESP32 dùng Wi-Fi khác / MQTT TLS | Chưa thử; firmware hiện tại vẫn dùng broker LAN cũ |
| NVS gửi bù, reboot/power cycle | Chưa nghiệm thu trên firmware modular mới |

Wi-Fi điện thoại được bật lại sau thử 4G. Sau thử offline khoảng 22 giây,
cả Wi-Fi và dữ liệu di động được khôi phục về bật. Không lấy kết quả quét Bluetooth
mô phỏng của emulator làm bằng chứng tương thích BLE với phần cứng thật.
Ảnh thử nghiệm lưu riêng ngoài Git tại `.local/saems-phone-4g.png`,
`.local/saems-phone-wifi.png`, `.local/saems-live-history.png`,
`.local/saems-offline-cache.png`, `.local/saems-phone-offline.png`.

## Giới hạn dữ liệu

DHT20/BH1750 có dữ liệu thay đổi, firmware báo data_mode real và sensor status
valid. Đây là xác nhận đường đọc/truyền, không phải hiệu chuẩn độ chính xác.
LM393 thiếu que đo/hiệu chuẩn: số 0% từ firmware cũ không được nghiệm thu là
độ ẩm đất thật. Timestamp thiết bị chưa đồng bộ; app dùng received_at của server.
Boot ID đã thay đổi trong phiên thử nên không tuyên bố thiết bị chưa từng reboot.

## Tự thử tiếp trên app

1. Cài đặt: nguồn Server, URL HTTPS trên, farm-01; token từ file riêng
   `.local/device-access.json`, không chép vào mã nguồn hoặc Git.
2. Tổng quan: chờ khoảng 10–15 giây, kiểm tra nhãn Server và số liệu mới.
3. Lịch sử: mở tab và nhấn nút làm mới; kiểm tra các điểm vừa nhận.
4. Tắt Wi-Fi, giữ dữ liệu di động: kiểm tra Dashboard vẫn cập nhật; bật lại
   Wi-Fi khi xong. Đường này đã đạt trên Samsung.
5. Thử offline cache: ngắt Wi-Fi và dữ liệu di động, kiểm tra nhãn Bộ nhớ cục bộ;
   có mạng trở lại phải chuyển về Server. Đã thử trên emulator và Samsung.
6. BLE/AP đầy đủ cần quyết định riêng cho phép flash firmware modular mới.
   Khi đó bật Bluetooth/cấp Nearby Devices, quét SAEMS rồi kiểm tra đọc dữ liệu;
   chỉ provision/điều khiển sau khi có cho phép và xác nhận an toàn tải relay.

## Quản trị server từ khác mạng

API/dashboard HTTPS dùng được không cần cùng Wi-Fi hoặc cài Tailscale.
Đây không phải giao diện quản trị hệ điều hành hay sửa mọi cấu hình server.
`ssh admin@100.104.200.110` đã được thử thực tế nhưng nhận:
`tailscale: tailnet policy does not permit you to SSH to this node`.
Chủ tailnet cần cấp đúng network access và SSH rules rồi kiểm tra lại đăng nhập.
Không thay policy hoặc tắt Tailscale SSH để vượt quyền trong phiên này.

Thông tin triển khai và bộ nhận tương thích:
[internet-and-provisioning.md](internet-and-provisioning.md).
