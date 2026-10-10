# SAEMS

SAEMS là viết tắt của **Smart Agriculture Environmental Monitoring System**.
Repository được tổ chức theo từng module để firmware, mini-server và ứng dụng di
động có thể phát triển độc lập nhưng dùng chung một hợp đồng dữ liệu.

```text
firmware/esp32s3/       Firmware ESP-IDF cho ESP32-S3 và phần cứng cảm biến
services/mini-server/   MQTT ingestor, SQLite, REST API và dashboard LAN/HTTPS
mobile/saems_app/       Ứng dụng Flutter cho server, BLE và Wi-Fi AP
docs/                   API, giao thức kết nối trực tiếp và tiến độ kiểm thử
```

Sau khi merge nhánh nhóm, repository có hai project ESP-IDF. `components/` và
`main/` ở root là firmware baseline từ `feature/Hien/firmware`, được giữ
nguyên để không làm mất mã của thành viên khác. Firmware tích hợp đã dùng để demo
Device–Server–App nằm tại `firmware/esp32s3/`; toàn bộ script Windows trong
`scripts/windows/` build và flash project này. Không chạy `idf.py build` ở
root khi mục tiêu là demo SAEMS tích hợp.

## Tài liệu chính

- [Báo cáo hiện thực Device – Server – App](docs/IMPLEMENTATION-REPORT.md)
- [Hướng dẫn demo bằng script](scripts/windows/RUN-DEMO.md)
- [Firmware ESP32-S3](firmware/esp32s3/README.md)
- [Mini-server Raspberry Pi](services/mini-server/README.md)
- [Mobile app Flutter](mobile/saems_app/README.md)

Tất cả script Windows được gọi từ thư mục gốc repository:

```powershell
Set-Location 'C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github'
.\scripts\windows\run_device.cmd -Action Read -Seconds 60
.\scripts\windows\run_server.ps1 -Action Watch
.\scripts\windows\run_app.ps1 -Action Run -Target Pixel6
```

## Trạng thái

- Firmware cảm biến thật, NVS offline queue, MQTT sync, BLE GATT, Wi-Fi AP REST,
  relay safety và Power Manager đã được hiện thực và compile bằng ESP-IDF 6.0.3.
- Direct, Light Sleep và Battery/Deep Sleep cùng nằm trong một firmware;
  `power_mode` được đổi qua app BLE/AP, lưu NVS và áp dụng ở chu kỳ kế tiếp mà
  không cần flash lại.
- Mini-server đã triển khai HTTPS tại `https://admin.tailb1f0ba.ts.net` và MQTT TLS
  tại `mqtts://admin.tailb1f0ba.ts.net:8443`, có token API và ACL MQTT; đã kiểm tra
  qua IP Funnel công khai. Dữ liệu/source server cũ vẫn được giữ nguyên.
- App đã kết nối BLE native, có provisioning BLE/AP và token lưu bảo mật;
  APK build đạt, 21 test đạt; bản cuối đã cài trên Samsung A24 và Pixel_6.
- Firmware mới đã flash COM3, đọc DHT20/BH1750 thật và gửi MQTT TLS trực tiếp
  lên Pi; Samsung nhận dữ liệu HTTPS bằng 4G. BLE read/notify/config và Wi-Fi
  AP read/config đã kiểm tra trên A24 thật. Bộ nhận firmware cũ vẫn được giữ.
- Chưa nghiệm thu relay, cold power-cycle, offline FIFO qua mất điện hoặc
  ESP32 đổi Wi-Fi. REST local đang truy cập được cả qua STA; cần giới hạn AP
  và bổ sung owner authentication cho BLE/AP trước khi dùng trên LAN chung.
- Quản trị SSH khác mạng chưa hoàn tất: Tailscale SSH đang bị policy từ chối.
- LM393 đã gắn que, đọc ADC thật và hiệu chuẩn bằng mốc `dry=4095`, `wet=2080`;
  firmware đã trả phần trăm và trạng thái soil `ok`.

Đã bổ sung chế độ đọc ADC thô LM393 khi gắn que, guard hiệu chuẩn và helper lưu
hai mốc vào NVS. Hướng dẫn chi tiết:
[build/flash và demo device–server–app](docs/lm393-build-calibration-demo.md).
Phần trăm không được bật bằng các mốc giữ chỗ; flash mới cần kết nối USB ổn định.

Cách chạy nhanh bằng script:
[scripts/windows/RUN-DEMO.md](scripts/windows/RUN-DEMO.md).

Chi tiết firmware: [firmware/esp32s3/README.md](firmware/esp32s3/README.md).
Ma trận giai đoạn 3–9: [docs/firmware-stages-3-9.md](docs/firmware-stages-3-9.md).
Triển khai Internet và kiểm thử điện thoại:
[docs/internet-and-provisioning.md](docs/internet-and-provisioning.md).
Kết quả test điện thoại/ESP32 thực tế:
[docs/new-firmware-device-server-app-acceptance.md](docs/new-firmware-device-server-app-acceptance.md).
Phiên cũ giữ nguyên firmware:
[docs/hardware-app-acceptance-2026-10-04.md](docs/hardware-app-acceptance-2026-10-04.md).
