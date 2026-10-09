# SAEMS – trạng thái mã nguồn đã chuẩn hóa

## Hiện tại: đã flash và kiểm thử firmware mới

- Firmware modular SAEMS đã build/flash COM3, không phải project hello_world.
  DHT20/BH1750 thật → MQTT TLS `:8443` → ingestor SAEMS → SQLite → HTTPS → app.
- A24 thật đã kiểm thử BLE read/notify/config, AP dữ liệu/config và HTTPS qua 4G;
  Pixel_6 đã kiểm thử Dashboard/lịch sử. APK cuối cài trên cả hai, 19 tests đạt.
- Đã sửa SNTP trước IP/thiếu kiểm tra ngày certificate, NimBLE stack overflow
  khi lưu config, stream BLE sau dispose và Settings khởi tạo nhầm Mock.
- LM393 vẫn disabled/null. Không gửi lệnh relay; chưa nghiệm thu cold power-cycle,
  offline FIFO qua mất điện, đổi Wi-Fi ESP32 hoặc sleep/wake/đo dòng.
- REST provisioning còn nghe trên STA, chưa có owner authentication; BLE Just
  Works cũng chưa xác thực owner/MITM. Tailscale SSH vẫn bị policy từ chối.
- Báo cáo, backup flash và hash artifact:
  [new-firmware-device-server-app-acceptance.md](new-firmware-device-server-app-acceptance.md).

Mục cập nhật ngày 2026-10-04 bên dưới ghi lịch sử phiên **chưa flash**;
không dùng trạng thái firmware cũ của phiên đó thay cho trạng thái hiện tại.

## Cập nhật Internet/BLE ngày 2026-10-04

- App BLE dùng FlutterBluePlus native (scan/connect/read/notify/config/command),
  không còn fake adapter ở source BLE; có runtime permission và màn hình
  provisioning Wi-Fi/MQTT qua BLE/AP, token API lưu FlutterSecureStorage.
- BLE config/command yêu cầu encrypted link, có bond key trong NVS; Just Works
  chưa có xác thực owner/MITM. Radio/pairing thật vẫn pending.
- Pi có bản triển khai riêng `/home/admin/saems-mini-server`, backup dữ liệu cũ,
  ba service tự chạy; bản Smartfarm/broker LAN cũ không bị xóa/ghi đè.
- HTTPS `https://admin.tailb1f0ba.ts.net`, MQTT TLS `:8443` chạy qua Funnel;
  API/Broker origin chỉ loopback và có xác thực/ACL. Không cần cùng Wi-Fi với Pi.
- Public TLS + auth + MQTT ACL + SQLite + API + dedup sau reconnect đã kiểm tra
  qua IP Internet công khai. Chỉ xóa fixture giả lập của lần test này.
- API giữ `data_mode`, `sensor_status` và metadata từ raw JSON; dữ liệu cũ thiếu
  metadata không được gán thành sensor thật.
- ESP32 MQTT TLS kiểm tra certificate bundle/hostname; có SNTP cho thời gian.
  Firmware FIFO đầy sẽ từ chối mẫu mới thay vì evict mẫu chờ PUBACK; vẫn giới
  hạn 32 mẫu, không bảo đảm lưu vô hạn.
- Flutter analyze không lỗi, 17 test đạt, APK build đạt; firmware ESP-IDF 6.0.3
  build đạt. Samsung/ESP32 đã được kết nối trong phiên nghiệm thu tiếp theo:
  app Wi-Fi/4G nhận dữ liệu DHT20/BH1750 thật qua bộ nhận tương thích chỉ đọc.
  Không flash; BLE/AP trên firmware mới và power-cycle vẫn chưa nghiệm thu.
- Bốn service Pi hiện active, gồm `saems-legacy-ingestor` đọc topic cũ;
  firmware đang chạy vẫn dùng MQTT LAN. SSH Tailscale bị policy từ chối,
  chưa thể coi quản trị từ xa đã hoàn tất dù HTTPS công khai hoạt động.

Kết quả thực nghiệm và cách tự thử app:
[hardware-app-acceptance-2026-10-04.md](hardware-app-acceptance-2026-10-04.md).

Chi tiết vận hành và 5 vai trò terminal theo tài liệu nhóm:
[internet-and-provisioning.md](internet-and-provisioning.md).

## Cấu trúc dùng để merge

```text
merge_github/
├── docs/
│   ├── current-progress.md
│   └── merge-plan.md
├── firmware/
│   └── esp32s3/
│       ├── .gitignore
│       ├── CMakeLists.txt
│       ├── README.md
│       ├── components/
│       │   ├── application_manager/
│       │   ├── config_manager/
│       │   ├── direct_connection/
│       │   ├── mqtt_manager/
│       │   ├── network_manager/
│       │   ├── power_manager/
│       │   ├── storage_manager/
│       │   ├── sync_manager/
│       │   └── sensor và relay drivers/
│       └── main/
│           ├── CMakeLists.txt
│           ├── Kconfig.projbuild
│           ├── idf_component.yml
│           └── main.c
├── services/
│   └── mini-server/
        ├── .env.example
        ├── .gitignore
        ├── README.md
        ├── api.py
        ├── mqtt_ingestor.py
        ├── requirements.txt
        ├── config/
        ├── dashboard/
        ├── data/.gitkeep
        ├── scripts/
│       └── systemd/
└── mobile/
    └── saems_app/
```

## Các lỗi đã xử lý

- Không đưa `hello_world_main.c` hoặc `pytest_hello_world.py` vào module firmware.
- `main/CMakeLists.txt` chỉ đăng ký `main.c`.
- Tên project và binary là `saems_esp32s3`, không còn `hello_world`.
- Credentials firmware nằm trong Kconfig và `sdkconfig` cục bộ.
- Sửa nhánh phục hồi NVS để kiểm tra kết quả khởi tạo lại, không kiểm tra biến
  lỗi cũ.
- Thêm `boot_id` để nhận diện phiên khởi động và khử bản tin QoS 1 trùng.
- Sequence telemetry được cấp phát theo block 1.000.000 trong NVS; sau reboot
  ESP32 tiếp tục sequence mới thay vì quay lại `1`.
- Tách quyền MQTT của thiết bị và server; ingestor không dùng tài khoản chỉ có
  quyền publish.
- README dùng đúng endpoint `/api/v1/devices/{device_id}/...`.
- Mini-server có endpoint `GET /api/v1/devices` để app lấy danh sách thiết bị
  và telemetry mới nhất; API chuẩn hóa tên `light_lux` theo payload ESP32.
- Script và systemd dùng Python/uvicorn trong `.venv`.
- Dashboard không phụ thuộc Chart.js CDN.
- Thay bộ sinh dữ liệu giả bằng driver DHT20 và BH1750 dùng chung I²C GPIO17/18.
- Thêm driver LM393: AO GPIO4/ADC1_CH3, DO GPIO5 và cấu hình hiệu chuẩn khô/ướt.
- Thêm driver relay GPIO10/GPIO11; cả hai kênh được đặt OFF khi khởi động.
- Thêm command `POST /api/v1/devices/{device_id}/commands/pump` qua MQTT; firmware
  nhận lệnh `on/off` trên `saems/{device_id}/commands` và có auto-off an toàn.
- Firmware tự reconnect Wi-Fi/MQTT; telemetry được lưu tối đa 32 mẫu trong FIFO
  NVS và gửi bù theo `boot_id + seq` kể cả sau reboot.
- Ingestor dùng reconnect của Paho ngay cả khi broker chưa sẵn sàng lúc service khởi động.
- Hoàn thiện mẫu service systemd cho ingestor và API, có tự khởi động và restart.
- Trường cảm biến chưa đọc được được gửi là JSON `null`, không tạo số giả.
- Thêm Sensor Manager để quản lý bus I²C, probe DHT20/BH1750, đọc đồng nhất một
  snapshot và báo trạng thái riêng cho từng cảm biến.
- Thêm Data Manager để tạo telemetry schema v1 gồm `data_mode: real`, thời gian,
  uptime, trạng thái tổng hợp và `sensor_status` chi tiết.
- Bổ sung Application, Config, Network, MQTT, Storage, Sync, BLE/AP và Power
  Manager theo Firmware Architecture v0.1.
- BLE GATT có device info, telemetry read/notify, config và pump command; Wi-Fi AP
  có REST API local tại `192.168.4.1`.
- Config Manager lưu provisioning Wi-Fi/MQTT/AP trong NVS nhưng không trả secret
  khi đọc cấu hình. Thiết bị vẫn boot BLE/offline khi chưa có SSID.
- Partition 16 MB có NVS 192 KiB, hai OTA slot 3 MiB và coredump.
- Firmware đầy đủ manager đã build/flash bằng ESP-IDF 6.0.3; binary cuối
  `0x141310`, còn khoảng 58% trong app slot 3 MiB.

## Giới hạn hiện tại

Firmware mới đã kiểm thử DHT20/BH1750, Wi-Fi, MQTT TLS và BLE/AP trên bo/A24 thật;
không còn chỉ là compile. Relay GPIO10/GPIO11 có boot OFF trong source nhưng chưa
kiểm thử tiếp điểm/tải hoặc active level. LM393 tắt vì thiếu que và hai mốc ADC;
`soil_moisture_pct` là `null`.

Gửi bù qua mất điện, queue đầy, light/deep sleep, đo dòng và wake/reconnect vẫn
chưa nghiệm thu. Quan sát startup buffering/PUBACK và warm reset không thay thế
kiểm thử NVS khi mất điện. REST local cần giới hạn interface AP/xác thực owner;
BLE encrypted Just Works chưa cung cấp owner/MITM authentication.

Firmware hiện dùng MQTT TLS `:8443`; app dùng HTTPS công khai tại Pi/Funnel.
Broker TCP 1883 chỉ được giữ cho tương thích firmware cũ. Không port-forward
REST HTTP của ESP32; HTTPS Internet không phải HTTPS trên AP local.

## Tiêu chí nghiệm thu

```text
Firmware build main/main.c và các component cảm biến, không có Hello world!
Firmware log địa chỉ I²C 0x38 và 0x23/0x5C được probe thành công
ESP32 log DHT20 ready, BH1750 ready và relay boot OFF
ESP32 log Wi-Fi connected, MQTT connected và Published boot=... seq=...
Payload có data_mode=real và sensor_status đúng với phần cứng đang gắn
Ingestor log Telemetry saved
SQLite chứa boot_id và seq
API trả đúng telemetry tại /api/v1/devices/farm-01/telemetry/latest
Dashboard cập nhật trong LAN mà không cần Internet
POST command trả `status: queued` và relay tự tắt theo giới hạn cấu hình
systemd tự khởi động `saems-ingestor` và `saems-api`
```
