# SAEMS ESP32-S3 firmware

Firmware đọc cảm biến vật lý, kết nối Wi-Fi/MQTT và publish telemetry theo chu kỳ.
Project không chứa `hello_world_main.c`; `main/CMakeLists.txt` chỉ biên dịch firmware
SAEMS và các component cảm biến bên dưới.

## Sơ đồ chân đã chuẩn hóa

| Thiết bị | Chân thiết bị | ESP32-S3 | Chức năng |
| --- | --- | --- | --- |
| DHT20 | SDA | GPIO17 | I²C SDA |
| DHT20 | SCL | GPIO18 | I²C SCL |
| DHT20 | VCC | 3V3 | Nguồn và mức logic I²C 3,3 V |
| DHT20 | GND | GND | Mass chung |
| BH1750 | SDA | GPIO17 | I²C SDA, dùng chung bus với DHT20 |
| BH1750 | SCL | GPIO18 | I²C SCL, dùng chung bus với DHT20 |
| BH1750 | VCC | 3V3 | Nguồn an toàn mặc định |
| BH1750 | GND | GND | Mass chung |
| LM393 đất | AO | GPIO4 | ADC1_CH3, giá trị độ ẩm tương đối |
| LM393 đất | DO | GPIO5 | Mức ngưỡng từ biến trở trên module |
| LM393 đất | VCC | 3V3 | Giữ AO/DO không vượt mức ESP32-S3 |
| LM393 đất | GND | GND | Mass chung |
| Relay CH1 | IN1 | GPIO10 | Điều khiển bơm, khởi động ở OFF |
| Relay CH2 | IN2 | GPIO11 | Kênh dự phòng, khởi động ở OFF |
| Relay | DC+ | 5Vin | Nguồn module relay 5 V |
| Relay | DC- | GND | Mass chung với ESP32-S3 |

BH1750 là cảm biến số I²C, vì vậy SDA/SCL không phải chân ADC hoặc chân nhận
biết sáng/tối. GPIO4 mới là chân ADC1_CH3 dùng cho AO của LM393.

> Không kéo SDA/SCL hoặc bất kỳ GPIO nào của ESP32-S3 lên 5 V. DHT20 hỗ trợ
> nguồn rộng nhưng datasheet yêu cầu điện áp host và cảm biến tương thích; IC
> BH1750 chỉ làm việc ở VCC 2,4–3,6 V. Chỉ dùng 5 V cho một breakout BH1750 khi
> nhà sản xuất module xác nhận có bộ ổn áp và chuyển mức I²C. Cách đấu mặc định
> ở đây là cấp cả DHT20 và BH1750 từ 3V3.

Module relay SRD-05VDC-SL-C cần nguồn cuộn 5 V. Phải kiểm tra board relay thực tế
có nhận logic 3,3 V và kích mức thấp hay không. Firmware mặc định active-low nhưng
có thể đổi trong `menuconfig`. Không đấu tải điện lưới nếu chưa có cách điện, cầu
chì, vỏ bảo vệ và người có chuyên môn.

## Kiến trúc manager

```text
components/
├── application_manager/ # State machine và điều phối toàn hệ thống
├── sensor_manager/      # DHT20, BH1750 và LM393
├── data_manager/        # Telemetry JSON schema v1
├── config_manager/      # Cấu hình vận hành và credentials trong NVS
├── network_manager/     # Wi-Fi STA, AP hoặc AP+STA
├── mqtt_manager/        # Telemetry/command MQTT QoS 1
├── storage_manager/     # FIFO NVS bền qua reboot, tối đa 32 bản ghi
├── sync_manager/        # Gửi bù, chỉ xóa sau MQTT publish ACK
├── direct_connection/   # NimBLE GATT và REST server trên Wi-Fi AP
├── power_manager/       # Active, modem-save và guard sleep
└── relay/               # GPIO10/GPIO11, boot OFF và auto-off
main/
└── main.c                # Khởi tạo NVS và Application Manager
```

Luồng mỗi chu kỳ là `READ_SENSOR → PROCESS_DATA → STORE_DATA → SYNC_DATA → IDLE`.
Telemetry luôn được lưu bền trước khi gửi MQTT. Khi chưa có Wi-Fi, firmware vẫn
khởi động cảm biến, lưu offline và quảng bá BLE; AP cũng có thể chạy độc lập.

Nếu một cảm biến chưa sẵn sàng hoặc đọc lỗi, firmware vẫn gửi bản tin nhưng trường
tương ứng là `null`; không thay bằng dữ liệu giả. `battery_pct` cũng là `null` vì
phần cứng hiện chưa có mạch đo pin.

Ví dụ khi chưa gắn que đo đất:

```json
{
  "schema_version": 1,
  "device_id": "farm-01",
  "boot_id": "1a2b3c4d",
  "seq": 1,
  "timestamp": null,
  "timestamp_source": "unsynced",
  "uptime_ms": 15000,
  "data_mode": "real",
  "sensor_health": "degraded",
  "sensor_status": {
    "dht20": "ok",
    "bh1750": "ok",
    "soil": "unavailable"
  },
  "temperature_c": 28.4,
  "humidity_pct": 72.1,
  "soil_moisture_pct": null,
  "light_lux": 135.8,
  "battery_pct": null,
  "pump_state": "off"
}
```

`sensor_health` là trạng thái tổng hợp để server/app cũ vẫn đọc được; object
`sensor_status` cho biết chính xác module nào lỗi hoặc chưa được bật. Khi chưa
đồng bộ được thời gian Internet, `timestamp` là `null` và server dùng thời gian
nhận bản tin; firmware không tự tạo một thời gian UTC giả.

## LM393 khi chưa có que cắm đất

`Enable LM393 soil moisture sensor` mặc định tắt. Không bật khi AO đang hở vì ADC
sẽ trôi và tạo số liệu giả. Khi đã gắn que nhưng chưa hiệu chuẩn, bật raw readout;
firmware gửi `soil_raw`, `soil_digital`, `soil_moisture_pct=null` và trạng thái
`uncalibrated`. Chỉ xuất phần trăm sau khi lưu `soil_calibrated=true` cùng hai
mốc đo thực tế. Key NVS cũ chưa có cờ này được coi là chưa hiệu chuẩn.

Hướng dẫn build/flash, đo hai mốc, lưu NVS không cần reflash và demo ba lớp:
[LM393 build/calibration/demo](../../docs/lm393-build-calibration-demo.md).

Sau khi có que đo:

1. Gắn que vào board LM393, cấp VCC 3V3 và để firmware vẫn chưa publish phần trăm.
2. Đọc log ADC khi que ở trạng thái khô cần quy ước; ghi lại `raw` làm `dry`.
3. Đọc lại trong đất/nước ở mức ướt cần quy ước; ghi lại `raw` làm `wet`.
4. Dùng helper `scripts/windows/soil_calibration.ps1` qua AP để lưu cả hai mốc,
   `soil_enabled=true`, `soil_calibrated=true`; helper kiểm tra readback.
5. Nhấn EN/reset, kiểm tra log `LM393 raw=... pct=... DO=...` và so sánh thực tế.
   Bản raw-readout đã flash không cần build/flash lại chỉ để đổi mốc.

Hai mặc định 3000/1200 chỉ là giá trị giữ chỗ trong cấu hình, không phải số hiệu
chuẩn cho que đo của bạn. Pin DO chỉ phản ánh ngưỡng đặt bằng biến trở trên module;
phần trăm được tính từ AO sau hiệu chuẩn.

## Cấu hình và build

```powershell
cd firmware/esp32s3
idf.py set-target esp32s3
idf.py menuconfig
```

Trong `SAEMS firmware`, nhập Wi-Fi, MQTT broker, tài khoản thiết bị, mật khẩu,
địa chỉ BH1750 và loại kích relay. Credentials mặc định nằm trong `sdkconfig` cục
bộ và không được commit. Thiết bị cũng nhận provisioning qua BLE hoặc `PUT
/v1/config` của AP, lưu vào NVS và áp dụng đầy đủ sau reboot; API đọc cấu hình
không trả lại mật khẩu.

```powershell
idf.py fullclean
idf.py build
idf.py -p COM3 flash monitor
```

Khi chưa gắn que đất, giữ LM393 disabled. Build đã được xác nhận bằng ESP-IDF
6.0.3 với binary `saems_esp32s3.bin`; binary cuối `0x141310`, OTA slot 3 MiB
còn khoảng 58%. Đã flash COM3 và kiểm thử DHT20/BH1750, MQTT TLS, BLE/AP trên A24.
Kiểm thử trên bo mạch cần có log `DHT20 ready`, `BH1750 ready`,
`MQTT connected` và `Published ... payload=...`; không được có `Hello world!`.

## BLE, Wi-Fi AP và Power Manager

BLE dùng service `a0f00001-6c7a-4d70-9e3b-9a2c00000001` với device info,
telemetry read/notify, config read/write và command write. Telemetry dài được chia
frame bốn byte header. AP dùng WPA2 và phục vụ API local tại
`http://192.168.4.1`; hợp đồng chi tiết nằm trong `docs/direct-connection-protocol.md`.

Power Manager tích hợp cả ba mode trong một firmware. `power_mode` (`direct`,
`light`, `battery`) được app ghi qua BLE/AP, lưu trong NVS và áp dụng ở ranh giới
chu kỳ lấy mẫu mà không cần flash lại. `Direct` dùng Wi-Fi modem power-save và giữ
BLE/AP; `Light` bật dynamic frequency scaling, automatic Light Sleep và tickless
idle; `Battery` giữ BLE/AP trong cửa sổ thức ngắn rồi dùng timer Deep Sleep giữa
các chu kỳ. Battery luôn lưu telemetry vào FIFO NVS trước, dành tối đa 15 giây để
sync, sau đó vẫn ngủ nếu server mất mạng. `boot_id` và sequence được giữ trong RTC
memory qua Deep Sleep; cold boot cấp dải sequence mới từ NVS. Tham số
`run_device.ps1 -PowerProfile` chỉ chọn mode mặc định khi NVS chưa có cấu hình.
Xem [hướng dẫn nghiệm thu Power Manager](../../docs/power-management-demo.md).

## Điều khiển relay và xử lý mất mạng

Server gửi command tới topic:

```text
saems/farm-01/commands
```

Payload bật bơm:

```json
{"command":"pump","state":"on","duration_s":30}
```

Payload tắt bơm:

```json
{"command":"pump","state":"off","duration_s":0}
```

Firmware chỉ cho phép relay CH1 bật tối đa theo `Maximum pump ON duration in
seconds` trong `menuconfig`, mặc định 60 giây, sau đó tự tắt. Khi MQTT mất kết
nối, firmware tự reconnect và lưu tối đa 32 mẫu telemetry trong FIFO NVS để gửi
bù kể cả sau reboot. Bản ghi chỉ bị xóa sau ACK publish QoS 1. Relay CH1/CH2 luôn
khởi động ở OFF.
## Internet/BLE deployment update

MQTT TLS target: mqtts://admin.tailb1f0ba.ts.net:8443. Provision the device's MQTT
username/password over BLE or Wi-Fi AP; they differ from the app's API token.
Firmware uses the trusted certificate bundle, hostname validation and
certificate-date verification. SNTP starts/restarts after STA obtains IP;
MQTT TLS initialization waits for valid time while BLE/AP/sampling continue.
BLE config/command writes require encryption and bond keys are stored in NVS.
NimBLE host and AP HTTP task stacks are 8 KiB because config JSON/NVS writes
overflowed the default 4 KiB host stack during real-phone testing.
The 32-record offline FIFO preserves queued records on overflow and rejects the
new sample (logged), rather than evicting a pending head before its PUBACK.
See [hardware acceptance](../../docs/new-firmware-device-server-app-acceptance.md)
for actual BLE/AP/4G results and full-flash backup. Power profiles build
successfully; cold power-cycle, relay, sleep/wake and current measurement remain
pending on hardware. REST local currently
also listens through STA without owner authentication; restrict AP scope/add
authentication before shared-LAN use. BLE Just Works is not owner/MITM auth.
