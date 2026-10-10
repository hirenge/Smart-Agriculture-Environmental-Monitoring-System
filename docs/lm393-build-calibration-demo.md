# SAEMS — build/flash, hiệu chuẩn LM393 và demo device → server → app

## Trạng thái và nguyên tắc

> Nếu chỉ cần chạy demo, dùng [RUN-DEMO.md](../scripts/windows/RUN-DEMO.md).
> File này giải thích thêm phần hiệu chuẩn và xử lý lỗi.

Que LM393 đã gắn, nhưng **chưa có hai mốc ADC đo thực tế**.
Firmware mới đọc `soil_raw`/`soil_digital`; phần trăm là `null` và trạng thái
`sensor_status.soil=uncalibrated` cho đến khi lưu hiệu chuẩn. App hiển thị `--`
là đúng, không phải mất dữ liệu. 3000/1200 trong Kconfig chỉ là số giữ chỗ.
Tỷ lệ sau hiệu chuẩn là chỉ số khô/ướt tương đối, không phải độ ẩm thể tích đất.

### Kết quả chuẩn bị ngày 2026-10-09

- Firmware cuối đã build ESP-IDF 6.0.3, target ESP32-S3, binary 1,316,448 byte
  (`0x141660`), còn khoảng 58% OTA slot 3 MiB.
- SHA-256: `d39e55dd3a1576115e1f5d2c6441073c4a4309f808414fde91a023b61fa52d3c`.
- 15 checks trên hàm C dùng trong driver đạt; helper PowerShell và Python kiểm
  tra cú pháp thành công. Đây không phải chứng nhận hiệu chuẩn vật lý.
- Flutter analyze không lỗi; 20 tests đạt, gồm regression bảo toàn `null` khi
  nhận ADC thô chưa hiệu chuẩn. Chưa chạy app trên Android thật trong phiên này.
- HTTPS `/health` trả ok; server đang nhận telemetry SMARTFARM cũ. Chưa có mẫu
  raw LM393 mới lên server trong phiên này.
- USB mất kết nối tại khoảng 40% lúc đọc flash; không tạo được backup hoàn chỉnh,
  **chưa erase và chưa flash**. Người dùng chọn cắm lại sau, chỉ chuẩn bị build/
  hướng dẫn. Lần sau phải kiểm tra COM và sao lưu lại, không dùng bản đọc dở.
- Chưa có Android target qua ADB; không nghiệm thu BLE/AP/app trên hardware
  trong phiên này. Kết quả phiên trước không tự áp dụng cho firmware build mới.
- Chưa commit/push các sửa đổi LM393 này lên GitHub.

Trong phiên này, UART trước flash cho thấy firmware SMARTFARM cũ đang gửi dữ liệu
giả lập; không lấy các phần trăm đó làm số đo đất. Đã yêu cầu sao lưu flash hiện
tại trước thay firmware/partition. Không gửi lệnh bơm; tải relay phải ngắt.

## 1. Kiểm tra đấu dây

| LM393 | ESP32-S3 |
| --- | --- |
| VCC | 3V3, không phải 5Vin |
| GND | GND chung |
| AO | GPIO4 / ADC1_CH3 |
| DO | GPIO5 |
| Đầu nối que | Hai chân que vào đầu nối của module LM393 |

DHT20/BH1750 vẫn chung SDA GPIO17, SCL GPIO18. Relay IN1 GPIO10, IN2 GPIO11.
Ngắt nguồn trước sửa dây. Không để AO/DO lên 5 V. Khi đo ướt chỉ đưa phần que
vào đất, giữ module LM393/dây nối/ESP32 khô. DO là mức logic của comparator;
biến trở chỉnh ngưỡng DO, không hiệu chuẩn phần trăm từ AO.

## 2. Build đúng firmware

Mở PowerShell; đường dẫn workspace thực tế đã đổi thành SAEMS:

```powershell
$saemsRepo = 'C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github'
Set-Location $saemsRepo
. 'C:\Espressif\tools\Microsoft.v6.0.3.PowerShell_profile.ps1'
```

Nếu chưa có SDK config cục bộ, dùng `idf.py menuconfig` trong
`firmware/esp32s3`, nhập Wi-Fi/MQTT/AP riêng tư. Helper sau chỉ dùng được trên
laptop hiện tại khi SDK config cũ và `.local/device-access.json` còn tồn tại:

```powershell
python scripts/windows/prepare_firmware_acceptance.py --soil-raw
Set-Location firmware/esp32s3
python ../../scripts/windows/idf_private.py -B build-internet build
```

Helper giữ Wi-Fi đã có, MQTT TLS `mqtts://admin.tailb1f0ba.ts.net:8443`, BLE và
AP WPA2; bật LM393 raw-readout dù NVS cũ còn `soil_enabled=false`. Không công bố
credentials. Runner che mật khẩu từ các thông báo Kconfig; không chia sẻ raw
`sdkconfig`, backup flash hay firmware binary.

Khi tự cấu hình menuconfig, bật:

- `SAEMS firmware → Enable LM393 soil moisture sensor`.
- `Read LM393 raw ADC for calibration even when soil is disabled in NVS`.
- Giữ TLS certificate date validation, SNTP và NimBLE host stack 8192.

**Chỉ build module này**, không chạy `idf.py build` ở root chứa project mẫu.
Kết quả phải là `saems_esp32s3.bin`, không phải `hello_world.bin`.

## 3. Flash và theo dõi device

Đóng mọi monitor/terminal đang giữ COM3. Kiểm tra tải relay đã ngắt.
Partition 16 MB của SAEMS cần NVS 192 KiB và OTA slot 3 MiB. Nếu board vẫn chạy
partition cũ, phải kiểm tra backup/partition rồi mới migrate; không dùng erase-all.
Không áp dụng lệnh erase expansion cho board đã có NVS 192 KiB.

Sau khi migration đã được kiểm tra, từ `firmware/esp32s3`:

```powershell
python ../../scripts/windows/idf_private.py -B build-internet -p COM3 flash
python ../../scripts/windows/read_device_log.py --port COM3 --seconds 30 --output lm393-demo.log
```

Để monitor liên tục thay vì finite reader:

```powershell
idf.py -B build-internet -p COM3 monitor
```

Thoát ESP-IDF monitor bằng Ctrl+]. Không mở hai monitor hoặc flash khi monitor
chưa đóng. Finite reader không chủ động toggle reset lines; monitor thông thường
có thể tác động reset. Không cần giữ monitor mở để ESP32 gửi dữ liệu.

Log cần thấy:

```text
DHT20 ready
BH1750 ready
LM393 ready: AO=GPIO4 DO=GPIO5
LM393 raw=... DO=... calibration=pending; pct=null
Wi-Fi có IP, đồng bộ SNTP, MQTT connected và telemetry publish
```

Nếu ADC gần 0 hoặc 4095 liên tục ở cả khô/ướt, kiểm tra nguồn/dây/que trước khi
chấp nhận mốc. Không tự gán số liệu giả để demo cho đẹp.

## 4. Đo hai mốc khô/ướt

1. Chọn một điều kiện **khô tham chiếu** nhất quán: đất khô mẫu của nhóm, hoặc
   que sạch/khô ngoài không khí nếu chỉ demo nhanh. Ghi rõ đã chọn điều kiện nào.
2. Đợi ổn định 30–60 giây, lấy ít nhất 5 dòng `LM393 raw=...`; chu kỳ hiện 10 giây.
   Lấy trung vị làm `DryRaw`, không lấy một lần đọc nhiễu.
3. Cắm que cùng độ sâu vào cùng loại đất đã làm ướt ở mức tham chiếu, không chỉ
   nhúng module vào nước. Đợi ổn định, lấy ít nhất 5 mẫu, trung vị làm `WetRaw`.
4. Hai mốc nằm 0..4095 và chênh ít nhất 100 count. Đây là guard chống khoảng đo
   quá nhỏ của project; nếu chênh nhỏ hơn, kiểm tra kết nối và đo lại.
5. Gửi hai số đo thực tế để được kiểm tra; không mặc định hướng ADC. Firmware
   hỗ trợ cả dry > wet và dry < wet.

Có thể đọc raw qua UART mà laptop vẫn kết nối Internet. Hoặc laptop tham gia AP
`SAEMS-farm-01` rồi chạy helper ở root repository; mật khẩu AP nằm riêng trong
`.local/firmware-ap-access.json`:

```powershell
Set-Location 'C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github'
.\scripts\windows\soil_calibration.ps1 -Action Read
```

Helper Read chỉ đọc, không bật bơm. Mẫu chưa có telemetry thì chờ chu kỳ đầu.

## 5. Lưu hiệu chuẩn — không cần build/flash lần nữa

Laptop phải đang nối AP của **đúng ESP32**. Nhập hai mốc bạn vừa đo, không copy
3000/1200 hoặc số trong unit test. Script tự kiểm tra device_id, khoảng ADC,
lưu và đọc lại cấu hình; không đổi Wi-Fi/MQTT/relay.

```powershell
$saemsDry = [int](Read-Host 'ADC dry da do')
$saemsWet = [int](Read-Host 'ADC wet da do')
.\scripts\windows\soil_calibration.ps1 -Action Calibrate -DryRaw $saemsDry -WetRaw $saemsWet
```

Request local tương ứng là `PUT http://192.168.4.1/v1/config`:

```text
soil_enabled: true
soil_calibrated: true
soil_dry_raw: ADC khô thực tế
soil_wet_raw: ADC ướt thực tế
```

Nhấn EN/reset sau khi readback thành công, tải relay vẫn ngắt. Cấu hình nằm trong
NVS nên áp dụng qua reboot, không cần reflash. Log cần có `pct=...` và payload
`sensor_status.soil=ok`. Sửa một mốc ADC mà không xác nhận lại calibration sẽ
đưa phần trăm về chưa hiệu chuẩn. Không erase NVS sau khi lưu mốc.

App hiện **chưa có form nhập mốc soil**; màn hình provisioning mới nhập
Wi-Fi/MQTT/chu kỳ. Dùng helper AP ở trên, không tìm nút hiệu chuẩn chưa tồn tại.

## 6. Kiểm tra server qua Internet

ESP32 cần Wi-Fi 2.4 GHz có Internet, Pi cần có Internet và các service active.
App/laptop không cần cùng Wi-Fi với Pi để đọc HTTPS. BLE/AP vẫn phải ở gần ESP32.
MQTT chờ thời gian SNTP hợp lệ để kiểm tra ngày certificate; đừng tắt xác minh TLS.

Trả laptop về Wi-Fi có Internet sau hiệu chuẩn AP. Từ root repository:

```powershell
Invoke-RestMethod 'https://admin.tailb1f0ba.ts.net/health'
$saemsAccess = Get-Content .local/device-access.json -Raw | ConvertFrom-Json
$saemsHeaders = @{ Authorization = ('Bearer ' + $saemsAccess.api_access_token) }
Invoke-RestMethod 'https://admin.tailb1f0ba.ts.net/api/v1/devices/farm-01/telemetry/latest' -Headers $saemsHeaders |
    Select-Object boot_id,seq,received_at,data_mode,soil_raw,soil_digital,soil_moisture_pct,sensor_status
```

Trước hiệu chuẩn: `soil_raw` có số, `soil_moisture_pct=null`, soil uncalibrated.
Sau hiệu chuẩn/reset: soil ok, phần trăm thay đổi theo đất, raw vẫn được giữ.
Phải thấy seq/thời gian nhận mới, không chỉ xem dữ liệu cache/lịch sử cũ.
API đã hỗ trợ soil_raw/soil_digital; không cần sửa schema hoặc redeploy server
chỉ để nhận các trường bổ sung này. Không in biến `$saemsAccess` hoặc token.

Nếu có quyền SSH vào Pi, kiểm tra read-only:

```bash
cd /home/admin/saems-mini-server
systemctl is-active saems-api saems-mqtt saems-ingestor saems-legacy-ingestor
journalctl -u saems-ingestor -n 30 --no-pager
sqlite3 data/telemetry.db 'SELECT device_id,boot_id,seq,received_at FROM telemetry ORDER BY id DESC LIMIT 5;'
```

Không chạy thêm API/ingestor thủ công khi systemd đang quản lý. SSH Tailscale
từng bị policy từ chối; HTTPS hoạt động không tự cấp quyền quản trị Pi.

## 7. Build/chạy app

Samsung: cắm cáp dữ liệu, bật USB debugging và bấm Allow. Emulator Pixel_6:
Android Studio → Device Manager → Start; emulator chỉ test UI/API, không nghiệm
thu radio BLE thật. Kiểm tra target trước chạy:

```powershell
$saemsAdb = 'C:\Users\Tien An\AppData\Local\Android\Sdk\platform-tools\adb.exe'
& $saemsAdb devices -l
$env:PUB_CACHE = 'C:\puro-smartfarm\shared\pub_cache'
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
$env:ANDROID_HOME = 'C:\Users\Tien An\AppData\Local\Android\Sdk'
Set-Location 'C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github\mobile\saems_app'
$saemsFlutter = 'C:\puro-smartfarm\envs\smartfarm\flutter\bin\flutter.bat'
& $saemsFlutter pub get
& $saemsFlutter analyze
& $saemsFlutter test
```

A24 (chỉ dùng serial này nếu ADB hiện đúng thiết bị):

```powershell
& $saemsFlutter run -d R58W80K3NWK --dart-define=ENABLE_MOCK_MODE=false --dart-define=API_BASE_URL=https://admin.tailb1f0ba.ts.net --dart-define=DEFAULT_DEVICE_ID=farm-01
```

Terminal khác cho Pixel_6; thay emulator serial nếu ADB khác:

```powershell
& $saemsFlutter run -d emulator-5554 --dart-define=ENABLE_MOCK_MODE=false --dart-define=API_BASE_URL=https://admin.tailb1f0ba.ts.net --dart-define=DEFAULT_DEVICE_ID=farm-01
```

APK để cài tay:

```powershell
& $saemsFlutter build apk --debug --dart-define=ENABLE_MOCK_MODE=false --dart-define=API_BASE_URL=https://admin.tailb1f0ba.ts.net --dart-define=DEFAULT_DEVICE_ID=farm-01
& $saemsAdb -s R58W80K3NWK install -r build/app/outputs/flutter-apk/app-debug.apk
```

Trong app Cài đặt: nguồn **Server**, URL HTTPS trên, device `farm-01`, nhập token
API từ file riêng khi chưa lưu. Để trống token giữ token đã có. Cài đặt lưu trước
đó ưu tiên hơn build defaults; nếu còn nguồn Mô phỏng phải đổi lại.

## 8. Kịch bản demo 5 terminal / 3 đường kết nối

1. Windows COM3: monitor cảm biến/raw/pct/Wi-Fi/SNTP/MQTT.
2. Pi nếu có quyền SSH: `journalctl -u saems-ingestor -u saems-mqtt -f`.
3. Pi nếu có quyền SSH: `journalctl -u saems-api -f` hoặc terminal HTTPS GET từ laptop.
4. Windows: Flutter run A24.
5. Windows: Flutter run Pixel_6. Không tạo service trùng để đủ số terminal.

- **Internet:** đặt nguồn Server trên hai app; cắm que khô/đất ướt, che BH1750,
  quan sát số mới sau khoảng một chu kỳ. Soil chỉ có % sau hiệu chuẩn/reset.
  A24 tắt Wi-Fi, bật 4G; dữ liệu phải tiếp tục cập nhật. Bật Wi-Fi lại sau test.
- **BLE A24:** bật Bluetooth, cấp Nearby Devices; Kết nối → Quét ESP32 → chọn
  SAEMS. Dashboard nguồn BLE; nhận mẫu trực tiếp. Nếu đã kết nối thì ESP32 có
  thể không quảng bá trong scan mới. Không dùng Pixel_6 chứng minh BLE radio.
- **AP A24:** Android tham gia SAEMS-farm-01 → app chọn Wi-Fi trực tiếp.
  Đọc dữ liệu qua 192.168.4.1 không cần Internet. Nếu Android tự rời AP do
  “không có Internet”, chọn giữ kết nối; tạm tắt mobile data rồi khôi phục sau.
- Trả app về Server và điện thoại về Wi-Fi/4G sau demo. Không bấm nút bật bơm.

## 9. Giới hạn an toàn và việc còn thiếu

REST local còn listen cả STA và chưa owner authentication; dùng AP trong môi
trường kiểm soát, không mở port ESP32 ra Internet. BLE Just Works mã hóa nhưng
chưa xác thực owner/MITM. Probe điện trở chịu ảnh hưởng đất/độ sâu/ăn mòn;
đổi mẫu đất hoặc que nên đo lại. Chưa có kiểm thử tải relay, cold power-cycle,
FIFO khi mất điện hoặc app radio mới trong phiên này nếu điện thoại không cắm.
Build/flash không thể thay người dùng thực hiện hai điều kiện đất khô/ướt.
