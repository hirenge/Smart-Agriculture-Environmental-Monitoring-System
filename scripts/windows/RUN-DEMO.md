# RUN DEMO SAEMS: Device → Server → App

Cập nhật: 2026-10-10. Tài liệu này là luồng chạy chính cho ESP32-S3, mini-server
Pi 4, app Android, BLE, Wi-Fi AP, offline sync và Power Manager.

## 0. Chọn đúng terminal

Thư mục làm việc:

```text
C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github
```

Khuyến nghị dùng **PowerShell**. Dấu nhắc đúng phải có dạng:

```text
PS C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github>
```

Nếu đang ở Command Prompt với dấu nhắc `C:\...>`, nhập:

```text
C:\...> powershell
PS C:\...> Set-Location 'C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github'
```

Sau khi dấu nhắc đổi thành `PS ...>`, mới chạy các lệnh `.ps1` trong tài liệu.
Không chạy trực tiếp `.ps1` tại CMD vì máy đang liên kết đuôi này với Node.js,
dẫn đến lỗi `SyntaxError: Unexpected token '<'`.

Riêng device đã có wrapper dùng trực tiếp từ CMD:

```bat
scripts\windows\run_device.cmd -Action Flash -PowerProfile Direct
```

## 1. Chuẩn bị trước khi demo

- Cắm ESP32-S3 bằng cáp USB dữ liệu; mặc định script dùng `COM3`.
- Đóng Serial Monitor/terminal khác đang giữ COM3.
- Ngắt tải bơm khỏi relay trong các bài test chưa nghiệm thu công suất.
- Pi 4, MQTT ingestor, SQLite API và Tailscale Funnel phải đang chạy.
- Pixel 6 emulator phải được khởi động trong Android Studio Device Manager.
- Khi test A24, bật USB debugging và chấp nhận hộp thoại cấp quyền ADB.
- Không commit `.local/device-access.json` hoặc credentials Wi-Fi/MQTT.

Kiểm tra thiết bị Android:

```powershell
& 'C:\Users\Tien An\AppData\Local\Android\Sdk\platform-tools\adb.exe' devices
```

Phải thấy một dòng `emulator-... device` cho Pixel 6 và một serial không bắt đầu
bằng `emulator-` cho Samsung A24.

## 2. Một firmware, ba mode runtime

| Mode | Cơ chế năng lượng | BLE/AP | Dùng khi |
| --- | --- | --- | --- |
| `Direct` | Wi-Fi modem power-save | Bật | Demo server, BLE, Wi-Fi AP, cấu hình |
| `Light` | DFS, automatic Light Sleep, tickless idle | Bật | Thử giảm điện nhưng vẫn giữ kết nối |
| `Battery` | Timer Deep Sleep giữa các chu kỳ | Có trong cửa sổ thức | Chạy pin và test offline/reconnect |

Ba mode đều nằm trong cùng binary. Dùng `Direct` cho demo tổng thể trước, sau đó
chuyển mode qua app BLE/AP; không build hoặc flash lại. Ở Battery, ngắt BLE sau
khi lưu cấu hình vì một kết nối BLE đang hoạt động sẽ hoãn Deep Sleep.

## 3. Build và flash firmware một lần

PowerShell:

```powershell
.\scripts\windows\run_device.ps1 -Action Build -PowerProfile Direct
.\scripts\windows\run_device.ps1 -Action Flash -PowerProfile Direct
```

Hoặc từ CMD:

```bat
scripts\windows\run_device.cmd -Action Build -PowerProfile Direct
scripts\windows\run_device.cmd -Action Flash -PowerProfile Direct
```

Luồng flash bình thường chỉ đọc probe bảng phân vùng/NVS 28 KiB. Nó không backup
toàn bộ 16 MB. Chỉ thêm `-FullBackup` khi chủ động muốn backup toàn flash và chấp
nhận chờ lâu.

Kết quả đạt:

```text
BUILD OK: firmware/esp32s3/build-internet/saems_esp32s3.bin
FLASH OK: SAEMS firmware written to ESP32-S3.
```

Nếu board không phải COM3, thêm `-Port COMx` vào mọi lệnh device.

`-PowerProfile Direct` chỉ đặt mode mặc định khi NVS chưa có `power_mode`. Binary
này vẫn chứa đầy đủ Direct, Light Sleep và Deep Sleep.

## 4. Demo online: Device → MQTT TLS → Pi → HTTPS → App

Mở ba PowerShell tại thư mục `merge_github`.

### Terminal 1 — log firmware ESP32

```powershell
.\scripts\windows\run_device.ps1 -Action Monitor
```

Thoát ESP-IDF monitor bằng `Ctrl+]`. Có thể dùng bản đọc hữu hạn thay thế:

```powershell
.\scripts\windows\run_device.ps1 -Action Read -Seconds 60
```

Log đạt cần có cảm biến thật, sample mới và MQTT kết nối/publish; không được có
`Hello world!` hoặc `data_mode: mock`.

### Terminal 2 — dữ liệu server qua Internet

```powershell
.\scripts\windows\run_server.ps1 -Action Watch
```

`Watch` gọi HTTPS API công khai và hiển thị bản ghi mới từ SQLite mỗi hai giây;
không cần SSH hoặc cùng Wi-Fi với Pi. Kiểm tra nhanh một lần:

```powershell
.\scripts\windows\run_server.ps1 -Action Check
```

Muốn xem riêng từng lớp trên Pi:

```powershell
# MQTT broker, ingestor và API journal theo thời gian thực
.\scripts\windows\run_server.ps1 -Action Logs

# Năm dòng SQLite mới nhất, cập nhật liên tục
.\scripts\windows\run_server.ps1 -Action Db

# Trạng thái service
.\scripts\windows\run_server.ps1 -Action Services
```

`Logs`, `Db` và `Services` dùng SSH, mặc định `admin@192.168.1.4`, nên laptop phải
truy cập được LAN của Pi hoặc phải truyền `-PiHost` là địa chỉ SSH hợp lệ. Nếu SSH
lỗi nhưng `Watch` vẫn có dữ liệu thì HTTPS Internet vẫn hoạt động bình thường.

### Terminal 3 — app trên Pixel 6

Khởi động Pixel 6 trong Android Studio trước, sau đó chạy:

```powershell
.\scripts\windows\run_app.ps1 -Action Run -Target Pixel6
```

Lệnh được nhập trong PowerShell trên laptop. Khi màn hình Pixel 6 hiện ra, dùng
chuột thao tác trực tiếp trên màn hình emulator. Giữ terminal mở để xem Flutter
log; nhấn `q` trong terminal khi muốn dừng app.

Trong app chọn nguồn `Server`, làm mới Dashboard và mở History. Demo online đạt
khi `farm-01`, `boot_id`, `seq`, nhiệt độ, độ ẩm, ánh sáng và độ ẩm đất khớp với
terminal server. `soil_moisture_pct` chỉ có giá trị sau khi hiệu chuẩn LM393.

## 5. Demo Wi-Fi AP offline trên A24

Điều kiện: firmware đang là `Direct`, A24 nối USB debugging và app đã được cài.
Không cần tắt Pi; script sẽ tắt mobile data của A24 và yêu cầu điện thoại nối AP
của ESP32.

```powershell
.\scripts\windows\phone_ap_test.ps1 -Action Connect
```

Nếu Android chưa tự nối, mở Wi-Fi trên A24 và chọn SSID `SAEMS-farm-01`. Sau đó
trong app:

1. Mở `Kết nối trực tiếp`.
2. Chọn `Dùng Wi-Fi trực tiếp`.
3. Quay lại Dashboard và làm mới.
4. Mở `Cấu hình Wi-Fi / MQTT cho ESP32`.
5. Chọn `Đọc cấu hình thiết bị`.

Đạt khi app đọc telemetry và cấu hình từ `http://192.168.4.1` trong lúc mobile
data tắt. Không gửi lệnh bật relay.

Khôi phục mạng A24:

```powershell
.\scripts\windows\phone_ap_test.ps1 -Action Restore
```

## 6. Demo BLE offline trên A24

Điều kiện: firmware `Direct`; Bluetooth trên A24 phải bật. Script sẽ tắt Wi-Fi và
mobile data nhưng không tự bật Bluetooth.

```powershell
.\scripts\windows\phone_ble_test.ps1 -Action Isolate
```

Trong app:

1. Mở `Kết nối trực tiếp`.
2. Chọn `Quét ESP32`.
3. Chọn `SAEMS-farm-01` và chấp nhận pairing nếu Android hỏi.
4. Quay lại Dashboard, làm mới và kiểm tra telemetry.
5. Mở màn hình provisioning và đọc cấu hình qua BLE.

Đạt khi app vẫn đọc được telemetry/config trong lúc Wi-Fi và mobile data đều tắt.
Không gửi lệnh relay.

Khôi phục mạng A24:

```powershell
.\scripts\windows\phone_ble_test.ps1 -Action Restore
```

## 7. Hiệu chuẩn LM393

Chỉ thực hiện khi que đo đã gắn, AO nối GPIO4 và module cấp 3,3 V. Đo hai mốc của
chính mẫu đất/que đang dùng:

- `DryRaw`: mốc khô quy ước.
- `WetRaw`: mốc ướt quy ước.

Laptop phải kết nối trực tiếp Wi-Fi `SAEMS-farm-01`; nếu không script sẽ dừng với
thông báo `Laptop is not connected to the ESP32 AP`.

Đọc trạng thái hiện tại:

```powershell
.\scripts\windows\soil_calibration.ps1 -Action Read
```

Nếu `4095` và `2080` đúng là hai mốc đã đo thật của bạn:

```powershell
.\scripts\windows\soil_calibration.ps1 -Action Calibrate -DryRaw 4095 -WetRaw 2080
```

Sau thông báo `Calibration persisted and verified`, nhấn EN/reset ESP32. Không
cần build/flash lại chỉ để áp dụng mốc. Đạt khi telemetry có `soil_raw`,
`soil_moisture_pct` và `sensor_status.soil=ok`; phần trăm phải thay đổi hợp lý khi
độ ẩm thay đổi.

## 8. Demo Offline Resilience và gửi bù

Dùng `Direct` để dễ quan sát liên tục:

1. Mở device `Monitor` và server `Watch`.
2. Xác nhận `pending=0`, server đang nhận sequence mới.
3. Ngắt Wi-Fi/router mà ESP32 đang dùng hoặc ngắt Internet của Pi trong 2-3 chu kỳ.
4. Không reset/flash ESP32. Quan sát firmware vẫn đọc sensor và `pending` tăng.
5. Khôi phục mạng/server.
6. Quan sát MQTT tự reconnect, dữ liệu cũ được PUBACK và `pending` giảm về 0.
7. Kiểm tra `Watch` hoặc `Db`: sequence đã lưu phải xuất hiện trước sequence mới.

Firmware lưu tối đa 32 bản ghi trong FIFO NVS. Test ngắn phải nhỏ hơn giới hạn
này; queue 32 mẫu không phải bộ nhớ vô hạn.

## 9. Demo Light Sleep

Light Sleep vẫn giữ BLE/AP, nhưng các kết nối đang hoạt động có thể làm hệ thống
ngủ ít hơn. Ưu tiên dùng A24 qua BLE: laptop vẫn giữ Wi-Fi rooftop, không cần nối
AP ESP32. Nếu app chưa có ô `Chế độ năng lượng`, cài/chạy lại source app mới:

```powershell
.\scripts\windows\run_app.ps1 -Action Run -Target A24
```

Trên A24: `Kết nối trực tiếp` → quét/chọn `SAEMS-farm-01` → `Cấu hình Wi-Fi /
MQTT cho ESP32` → `Đọc cấu hình thiết bị` → chọn `Light Sleep` → lưu. Sau khi lưu,
ngắt BLE để giảm hoạt động nền rồi đọc UART trên laptop:

```powershell
.\scripts\windows\run_device.ps1 -Action Read -Seconds 60
```

Chỉ dùng phương án PowerShell dưới đây khi chủ động nối laptop vào Wi-Fi
`SAEMS-farm-01`; lúc đó laptop có thể tạm mất Internet/rooftop nhưng lệnh là local
và không cần server:

```powershell
.\scripts\windows\power_mode.ps1 -Action Set -Mode Light
.\scripts\windows\run_device.ps1 -Action Read -Seconds 60
```

Log cần có:

```text
Power mode applied at sample boundary: direct -> light
Idle ... seconds; automatic light sleep enabled ...
```

## 10. Demo Battery/Deep Sleep

Trong app đang kết nối BLE/AP, đặt chu kỳ telemetry từ 60 giây trở lên, chọn
`Battery – Deep Sleep`, lưu rồi ngắt BLE. Không flash lại. Qua AP có thể dùng:

```powershell
.\scripts\windows\power_mode.ps1 -Action Set -Mode Battery -TelemetryIntervalSeconds 60
.\scripts\windows\run_device.ps1 -Action Read -Seconds 120
```

Battery thực hiện: thức dậy → đọc sensor → lưu NVS → cho BLE/AP hoạt động tối thiểu
10 giây và thử sync tối đa 15 giây →
tắt relay → dừng Wi-Fi → Deep Sleep. Khi server mất mạng, dữ liệu vẫn chờ trong
NVS và thiết bị vẫn ngủ thay vì treo.

Log đạt lần đầu:

```text
Wake reason: cold-boot-or-reset
Power profile initialized: deep-sleep
Battery window ended: pending=... maintenance=10s deadline=15s
Sample captured: ... pending=...
Entering deep sleep for ... seconds
```

Log đạt sau khi RTC timer đánh thức:

```text
Wake reason: timer
Resumed RTC sequence: boot=... next=... limit=...
```

Cùng một chuỗi Deep Sleep phải giữ `boot_id`, còn `seq` tăng. `pending>0` trong
lúc mất mạng là hợp lệ; sau khi mạng trở lại phải giảm về 0. BLE và Wi-Fi AP chỉ
nhìn thấy khi ESP32 đang thức là hành vi đúng thiết kế.

Muốn nghiệm thu năng lượng thật phải đo dòng ở nguồn board. LED nguồn, USB-UART
và regulator trên development board vẫn tiêu thụ điện dù chip đang Deep Sleep.

Sau bài test, trong cửa sổ thức Battery kết nối lại BLE/AP và chọn `Direct`; hoặc
qua AP chạy:

```powershell
.\scripts\windows\power_mode.ps1 -Action Set -Mode Direct
```

## 11. Lỗi thường gặp

### `SyntaxError: Unexpected token '<'` và có `Node.js`

Đang chạy `.ps1` từ CMD. Nhập `powershell` trước, hoặc dùng
`scripts\windows\run_device.cmd` cho các lệnh device. Lỗi xảy ra trước flash nên
không làm thay đổi ESP32.

### `COM3 is listed but cannot be opened`

Đóng ESP-IDF Monitor, Arduino Serial Monitor và terminal đọc COM3. Rút/cắm lại
cáp USB dữ liệu, chờ COM3 xuất hiện rồi chạy lại. Chỉ một chương trình được giữ
cổng serial tại một thời điểm.

### Script bắt đầu backup 16 MB

Bạn đã dùng `-FullBackup` hoặc đang chạy script cũ. Nhấn `Ctrl+C` nếu không chủ
động backup, rồi chạy lại lệnh không có `-FullBackup`.

### `Invoke-RestMethod ... timed out` khi hiệu chuẩn đất

Laptop chưa nối AP ESP32. Nối `SAEMS-farm-01`, kiểm tra laptop có IP
`192.168.4.x`, rồi chạy lại.

### `SSH log failed` hoặc `SQLite watch failed`

`Logs/Db/Services` cần SSH tới Pi. Dùng `Watch` để kiểm tra dữ liệu qua HTTPS mà
không cần SSH; hoặc sửa `-PiHost` thành địa chỉ Pi mà laptop truy cập được.

### Battery không thấy BLE/AP hoặc firmware có vẻ không chạy liên tục

Đúng thiết kế: Battery ngủ sâu và chỉ quảng bá BLE/AP trong cửa sổ thức. Chờ RTC
wake hoặc nhấn EN một lần, kết nối app và chuyển `power_mode` về `direct`; không
cần flash lại.

## 12. Tiêu chí kết luận

- Online đạt: ESP32 sensor thật → MQTT TLS → Pi/SQLite → HTTPS API → Pixel 6.
- Wi-Fi AP đạt: A24 đọc telemetry/config tại `192.168.4.1` khi mobile data tắt.
- BLE đạt: A24 đọc telemetry/config khi Wi-Fi và mobile data tắt.
- Offline đạt: ESP32 không treo, pending tăng khi mất mạng và về 0 sau reconnect.
- Soil đạt: mốc ADC thật đã lưu, phần trăm không còn null và thay đổi hợp lý.
- Runtime đạt: đổi `direct ↔ light ↔ battery` qua BLE/AP mà không flash lại.
- Light đạt: automatic Light Sleep được bật giữa các chu kỳ.
- Battery đạt: timer wake, `boot_id` giữ nguyên, `seq` tăng, pending được gửi bù.
- Năng lượng chỉ được nghiệm thu hoàn chỉnh sau khi có phép đo dòng thực tế.

Tài liệu chi tiết Power Manager: [power-management-demo.md](../../docs/power-management-demo.md).
