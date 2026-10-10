# SAEMS ESP32-S3 firmware

Firmware này là chương trình chính cho thiết bị SAEMS, được viết bằng ESP-IDF
6.0.3. Nó đọc cảm biến thật, lưu dữ liệu khi mất mạng, đồng bộ MQTT khi mạng trở
lại, cung cấp BLE/Wi-Fi AP cho ứng dụng và quản lý ba chế độ năng lượng trong
cùng một binary. Không build project `hello_world` ở thư mục gốc repository.

## Phần cứng và sơ đồ chân

| Thiết bị | Chân thiết bị | ESP32-S3 | Chức năng |
| --- | --- | --- | --- |
| DHT20 | SDA | GPIO17 | I²C SDA |
| DHT20 | SCL | GPIO18 | I²C SCL |
| DHT20 | VCC/GND | 3V3/GND | Nguồn và mass |
| BH1750 | SDA | GPIO17 | I²C SDA dùng chung |
| BH1750 | SCL | GPIO18 | I²C SCL dùng chung |
| BH1750 | VCC/GND | 3V3/GND | Nguồn và mass |
| LM393 | AO | GPIO4 | ADC1_CH3 |
| LM393 | DO | GPIO5 | Ngưỡng khô/ướt của module |
| LM393 | VCC/GND | 3V3/GND | Nguồn và mass |
| Relay CH1 | IN1 | GPIO10 | Điều khiển bơm |
| Relay CH2 | IN2 | GPIO11 | Kênh dự phòng |
| Relay | DC+/DC- | 5Vin/GND chung | Nguồn cuộn relay |

BH1750 là cảm biến I²C, không phải ADC. Không đưa mức 5 V vào GPIO, SDA hoặc SCL
của ESP32-S3. Khi chưa nghiệm thu relay, phải ngắt tải bơm trước khi flash/reset.

## Kiến trúc firmware

```text
main/main.c
  └─ application_manager      State machine toàn hệ thống
      ├─ config_manager       Cấu hình và credentials trong NVS
      ├─ sensor_manager       DHT20, BH1750, LM393
      ├─ data_manager         Telemetry JSON schema v1
      ├─ storage_manager      FIFO NVS tối đa 32 bản ghi
      ├─ network_manager      Wi-Fi STA, AP và reconnect
      ├─ mqtt_manager         MQTT QoS 1 và command
      ├─ sync_manager         Gửi bù, xóa sau PUBACK
      ├─ direct_connection    BLE GATT và REST trên Wi-Fi AP
      ├─ power_manager        Direct, Light Sleep, Battery/Deep Sleep
      └─ relay                Boot OFF, giới hạn thời gian bật
```

Chu kỳ chính:

```text
READ_SENSOR → PROCESS_DATA → STORE_DATA → SYNC_DATA → IDLE/SLEEP
```

Telemetry được lưu trước khi publish. Nếu Wi-Fi, MQTT hoặc server mất kết nối,
thiết bị vẫn lấy mẫu và giữ bản ghi trong FIFO NVS. Khi MQTT kết nối lại,
`sync_manager` gửi từ bản cũ nhất và chỉ xóa sau PUBACK QoS 1. Khóa
`device_id + boot_id + seq` giúp server loại bản tin trùng.

## Dữ liệu cảm biến

- DHT20 trả `temperature_c` và `humidity_pct`.
- BH1750 trả `light_lux`.
- LM393 trả `soil_raw`, `soil_digital` và chỉ trả `soil_moisture_pct` sau khi
  đã lưu hai mốc ADC khô/ướt thực tế.
- Cảm biến lỗi hoặc chưa cấu hình trả `null`; firmware không tạo dữ liệu giả.
- `battery_pct` hiện là `null` vì chưa có mạch đo pin.
- `data_mode` của firmware thật luôn là `real`.

Mốc LM393 đang dùng trong đợt demo là `dry=4095`, `wet=2080`. Đây là mốc của
que/mẫu đất đã đo, không phải hằng số áp dụng cho mọi cảm biến hoặc loại đất.

## BLE, Wi-Fi AP và Power Manager

BLE quảng bá thiết bị SAEMS và cung cấp device info, telemetry read/notify,
config read/write và command. Wi-Fi AP cung cấp REST API tại
`http://192.168.4.1`. Hợp đồng chi tiết nằm trong
[direct-connection-protocol.md](../../docs/direct-connection-protocol.md).

Một binary chứa đủ ba mode:

| Mode | Hoạt động |
| --- | --- |
| `direct` | Thiết bị thức liên tục, giữ BLE/AP và Wi-Fi/MQTT |
| `light` | Dynamic frequency scaling, tickless idle và automatic Light Sleep |
| `battery` | Cửa sổ thức để lấy mẫu/sync/cấu hình rồi timer Deep Sleep |

Mode được đổi qua app hoặc AP, lưu NVS và áp dụng ở ranh giới chu kỳ lấy mẫu;
không cần flash lại. Tham số `-PowerProfile` của script chỉ chọn mặc định khi
NVS chưa có `power_mode`.

## Build, flash và xem log bằng script

Tất cả lệnh dưới đây chạy từ **thư mục gốc repository**, không chạy trong
`firmware/esp32s3`:

```powershell
Set-Location 'C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github'

# Chỉ build
.\scripts\windows\run_device.cmd -Action Build -PowerProfile Direct

# Flash firmware hiện tại; mặc định COM3
.\scripts\windows\run_device.cmd -Action Flash -PowerProfile Direct

# Đọc log trong 60 giây hoặc mở monitor liên tục
.\scripts\windows\run_device.cmd -Action Read -Seconds 60
.\scripts\windows\run_device.cmd -Action Monitor
```

Wrapper `.cmd` tránh lỗi Windows mở file `.ps1` bằng Node.js. Nếu dùng
PowerShell trực tiếp:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\windows\run_device.ps1 -Action Build
```

Build hợp lệ tạo file:

```text
firmware/esp32s3/build-internet/saems_esp32s3.bin
```

## Hiệu chuẩn đất và đổi mode runtime

Hai script sau yêu cầu laptop đang kết nối Wi-Fi AP của ESP32:

```powershell
.\scripts\windows\soil_calibration.ps1 -Action Read
.\scripts\windows\soil_calibration.ps1 -Action Calibrate `
  -DryRaw 4095 -WetRaw 2080

.\scripts\windows\power_mode.ps1 -Action Set -Mode Light
.\scripts\windows\power_mode.ps1 -Action Read
```

Trong Battery mode, AP/BLE chỉ xuất hiện trong cửa sổ thức. Nếu cấu hình không
kịp, reset thiết bị hoặc chuyển về Direct/Light trong lần thức kế tiếp.

## Tiêu chí demo firmware

- Log có `DHT20 ready`, `BH1750 ready` và relay boot ở OFF.
- LM393 có `raw`, `pct` hợp lý sau hiệu chuẩn và `sensor_status.soil=ok`.
- Log có Wi-Fi/MQTT connected và payload `data_mode=real`.
- Khi ngắt mạng, số bản ghi pending tăng nhưng firmware vẫn lấy mẫu.
- Khi mạng trở lại, pending giảm về 0 sau PUBACK và server nhận đủ dữ liệu.
- Đổi `direct/light/battery` không cần build hoặc flash lại.

Luồng demo đầy đủ:
[scripts/windows/RUN-DEMO.md](../../scripts/windows/RUN-DEMO.md). Báo cáo hiện
trạng: [IMPLEMENTATION-REPORT.md](../../docs/IMPLEMENTATION-REPORT.md).

## Giới hạn và an toàn

- Chưa có phần cứng đo pin nên chưa thể báo phần trăm pin.
- Cần đo dòng thực tế để định lượng mức tiết kiệm của Light/Deep Sleep.
- Cần xác nhận active-low/active-high và mức logic 3,3 V của đúng module relay
  trước khi nối tải.
- BLE Just Works và REST AP chưa phải cơ chế xác thực chủ sở hữu mạnh; không dùng
  provisioning trên mạng dùng chung khi chưa bổ sung bảo mật.
