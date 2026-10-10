# Báo cáo hiện thực SAEMS: Device – Server – Mobile App

**Ngày cập nhật:** 10/10/2026

**Đề tài:** Smart Agriculture Environmental Monitoring System (SAEMS)

## 1. Phạm vi báo cáo

Báo cáo tổng hợp trạng thái mã nguồn hiện tại của ba kiến trúc:

1. ESP32-S3 và phần cứng cảm biến/relay.
2. Mini-server Raspberry Pi 4.
3. Ứng dụng Flutter Android.

Mục tiêu là đối chiếu yêu cầu đề tài với phần đã hiện thực, bằng chứng kiểm thử,
phần còn thiếu và quy trình demo thống nhất. Credentials, token, mật khẩu và dữ
liệu riêng tư không nằm trong báo cáo hoặc Git.

## 2. Kiến trúc tổng thể

```text
DHT20 + BH1750 + LM393 + Relay
              │
              ▼
        ESP32-S3 firmware
     ┌────────┼───────────────┐
     │        │               │
 MQTT TLS   BLE GATT      Wi-Fi AP REST
     │        │               │
     ▼        └──────┬────────┘
 Mosquitto           │
     │                ▼
 MQTT ingestor   Android app offline
     │
   SQLite
     │
 FastAPI + Dashboard
     │ HTTPS
     ▼
 Android app qua Internet
```

Các lớp dùng chung telemetry schema v1 và định danh bản tin bằng
`device_id + boot_id + seq`.

## 3. Kết quả hiện thực firmware/hardware

### 3.1 Manager theo Firmware Architecture v0.1

| Manager | Trạng thái | Vai trò đã hiện thực |
| --- | --- | --- |
| Application Manager | Hoàn thành source | State machine và điều phối chu kỳ |
| Config Manager | Hoàn thành source | NVS, provisioning, calibration, power mode |
| Sensor Manager | Đã chạy phần cứng | DHT20, BH1750, LM393 và trạng thái từng sensor |
| Data Manager | Hoàn thành source | JSON schema v1, `real`, null-safe |
| Network Manager | Đã chạy phần cứng | STA/AP, reconnect và offline boot |
| MQTT Manager | Đã chạy với server | MQTT TLS, QoS 1, command |
| Storage Manager | Đã hiện thực | FIFO NVS 32 bản ghi |
| Sync Manager | Đã hiện thực/test chức năng | Gửi bù và xóa sau PUBACK |
| Direct Connection | Đã test A24 | BLE GATT và Wi-Fi AP REST |
| Power Manager | Đã build | Direct, Light Sleep và Battery/Deep Sleep runtime |
| Relay Manager | Source hoàn thành | Boot OFF, command và auto-off; tải thật chưa nghiệm thu |

### 3.2 Cảm biến thật

- DHT20 và BH1750 đã đọc dữ liệu thật trên GPIO17/18.
- LM393 đã gắn que và hiệu chuẩn với mốc thực nghiệm `dry=4095`, `wet=2080`.
- Log đã quan sát `soil_raw`, `soil_moisture_pct` và trạng thái soil `ok`.
- Firmware gửi `null` khi cảm biến không sẵn sàng, không thay bằng dữ liệu giả.
- Chưa có mạch đo pin nên `battery_pct=null` là hành vi đúng.

### 3.3 Hoạt động độc lập và dự phòng mạng

Telemetry được lưu vào FIFO NVS trước khi gửi. Khi server/MQTT mất kết nối,
firmware tiếp tục lấy mẫu và tăng số bản pending. Khi kết nối trở lại, firmware
gửi từ đầu hàng đợi, chờ PUBACK rồi mới xóa. FIFO giới hạn 32 mẫu; khi đầy, hệ
thống giữ bản cũ đang chờ và từ chối mẫu mới thay vì xóa dữ liệu chưa sync.

Demo mất mạng/reconnect đã được thực hiện ở mức chức năng. Cold power-cycle khi
queue đang có dữ liệu vẫn nên được ghi nhận thêm thành bằng chứng nghiệm thu cuối.

### 3.4 Tối ưu năng lượng

Ba mode nằm trong cùng một firmware và đổi runtime qua app BLE/AP:

- `direct`: thiết bị thức liên tục.
- `light`: DFS, tickless idle và automatic Light Sleep giữa các chu kỳ.
- `battery`: lấy mẫu/sync trong cửa sổ thức rồi timer Deep Sleep.

Lựa chọn lưu trong NVS và áp dụng ở ranh giới chu kỳ. Source và build đã đạt;
cần đồng hồ đo dòng/pin để định lượng mức tiết kiệm và nghiệm thu wake/reconnect
trong nhiều chu kỳ thực tế.

## 4. Kết quả hiện thực mini-server

Mini-server chạy trên Pi 4 với bốn service:

- `saems-mqtt`: Mosquitto và ACL.
- `saems-ingestor`: MQTT → validator → SQLite.
- `saems-api`: FastAPI, dashboard và pump command.
- `saems-legacy-ingestor`: tương thích topic firmware cũ.

API HTTPS công khai và MQTT TLS cho phép ESP32/app dùng mạng khác với Pi. Origin
API/broker mới chỉ bind loopback; Tailscale Funnel đảm nhiệm TLS/routing. API dữ
liệu yêu cầu Bearer token, MQTT tách quyền thiết bị/server, database deduplicate
theo định danh telemetry.

HTTPS đã dùng được từ A24 qua 4G. SSH quản trị từ xa là chức năng khác và hiện
vẫn phụ thuộc Tailscale ACL; khi SSH không dùng được, API HTTPS vẫn hoạt động.

## 5. Kết quả hiện thực mobile app

Ứng dụng hỗ trợ bốn nguồn:

| Nguồn | Mục đích | Trạng thái |
| --- | --- | --- |
| Server | Dữ liệu/lịch sử qua HTTPS | Đã test A24 và Pixel 6 |
| Bluetooth | Telemetry và provisioning offline | Đã test A24 |
| Wi-Fi AP | Telemetry/config tại `192.168.4.1` | Đã test A24 |
| Mock | Phát triển UI/test | Không dùng nghiệm thu sensor |

Token API được lưu trong secure storage. App có dashboard, history, diagnostics,
nearby/provisioning và settings. Power mode được gửi trong config BLE/AP; thiết
bị không cần flash lại khi đổi mode.

## 6. Bằng chứng build và kiểm thử gần nhất

| Hạng mục | Kết quả |
| --- | --- |
| ESP-IDF 6.0.3 build | Đạt, binary `saems_esp32s3.bin` |
| Kích thước app firmware | Khoảng 1,33 MiB; còn khoảng 58% OTA slot 3 MiB |
| Flutter tests | 21/21 đạt |
| Flutter Android | APK debug build/cài được trên Pixel 6 và A24 |
| DHT20/BH1750 | Đọc cảm biến thật |
| LM393 | Raw và % sau hiệu chuẩn |
| MQTT TLS → SQLite → HTTPS | Đã quan sát end-to-end |
| BLE read/notify/config | Đã test A24 |
| Wi-Fi AP telemetry/config | Đã test A24 |

## 7. Đối chiếu yêu cầu đề tài

| Yêu cầu | Mức đáp ứng | Ghi chú |
| --- | --- | --- |
| Đọc nhiệt độ/độ ẩm không khí | Đạt | DHT20 thật |
| Đọc ánh sáng | Đạt | BH1750 thật |
| Đọc độ ẩm đất | Đạt chức năng | LM393 đã hiệu chuẩn; cần kiểm tra nhiều mẫu đất |
| Server lưu trữ và API | Đạt | MQTT, SQLite, FastAPI |
| App xem dữ liệu Internet | Đạt | HTTPS + token |
| BLE khi không có Internet | Đạt chức năng | Đã test A24 |
| Wi-Fi AP khi không có Internet | Đạt chức năng | Đã test A24 |
| Offline queue và auto-sync | Đạt source/chức năng | Cần thêm bằng chứng cold power-cycle |
| Light/Deep Sleep | Đạt source/build | Cần đo dòng và nhiều chu kỳ wake |
| Điều khiển relay an toàn | Đạt source | Chưa nghiệm thu module/tải thật |
| Quản trị Pi từ mạng ngoài | Một phần | HTTPS đạt; SSH còn phụ thuộc ACL |

## 8. Quy trình demo thống nhất

Mở PowerShell tại root:

```powershell
Set-Location 'C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github'
```

Ba terminal online tối thiểu:

```powershell
# Terminal 1: firmware log
.\scripts\windows\run_device.cmd -Action Read -Seconds 120

# Terminal 2: API/SQLite qua HTTPS
.\scripts\windows\run_server.ps1 -Action Watch

# Terminal 3: app Pixel 6
.\scripts\windows\run_app.ps1 -Action Run -Target Pixel6
```

Demo offline trên A24:

```powershell
.\scripts\windows\phone_ble_test.ps1 -Action Isolate
.\scripts\windows\phone_ble_test.ps1 -Action Restore

.\scripts\windows\phone_ap_test.ps1 -Action Isolate
.\scripts\windows\phone_ap_test.ps1 -Action Restore
```

Tài liệu từng bước, log mong đợi và xử lý lỗi:
[scripts/windows/RUN-DEMO.md](../scripts/windows/RUN-DEMO.md).

## 9. Công việc còn lại đề xuất

1. Đo dòng Direct, Light Sleep và Deep Sleep bằng nguồn/đồng hồ đo phù hợp.
2. Test Battery mode qua nhiều chu kỳ wake, mất mạng và reconnect.
3. Test cold power-cycle khi FIFO đang pending và đối chiếu đủ sequence trên DB.
4. Xác nhận module relay active-low/active-high, logic 3,3 V và test tải an toàn.
5. Bổ sung mạch đo pin nếu đề tài yêu cầu `battery_pct` thật.
6. Giới hạn REST provisioning vào AP và bổ sung owner authentication cho BLE/AP.
7. Hoàn thiện Tailscale SSH ACL nếu cần quản trị Pi từ Internet.

## 10. Tài liệu module

- [Firmware ESP32-S3](../firmware/esp32s3/README.md)
- [Mini-server Raspberry Pi](../services/mini-server/README.md)
- [Mobile app Flutter](../mobile/saems_app/README.md)
- [API contract](api-contract.md)
- [BLE/Wi-Fi AP contract](direct-connection-protocol.md)
- [Demo Power Manager](power-management-demo.md)
- [Hiệu chuẩn LM393](lm393-build-calibration-demo.md)
