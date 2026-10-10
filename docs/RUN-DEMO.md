# Demo SAEMS: Device → Server → App và chế độ offline

Chạy PowerShell tại:

```powershell
cd 'C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github'
```

Tải relay phải được ngắt trong lúc thử. Một firmware chứa cả Direct, Light Sleep
và Battery/Deep Sleep; app đổi mode qua BLE/AP mà không flash lại. Xem
[power-management-demo.md](power-management-demo.md).

## 1. Flash firmware một lần

Nếu lệnh cũ đang backup 16 MB, nhấn `Ctrl+C`. Sau đó chạy lại:

```powershell
.\scripts\windows\run_device.ps1 -Action Flash -PowerProfile Direct
```

Luồng mặc định chỉ đọc 28 KiB bảng phân vùng/NVS, rồi build và flash. Backup 16 MB không còn
nằm trong demo mặc định; chỉ chạy khi chủ động dùng `-FullBackup`.

## 2. Demo mạng bình thường

Khởi động Pixel 6 trong Android Studio Device Manager, sau đó mở ba PowerShell. Ba lệnh dưới
đây đều chạy liên tục cho đến khi bạn chủ động dừng.

Terminal 1 — firmware ESP32 liên tục:

```powershell
.\scripts\windows\run_device.ps1 -Action Monitor
```

Thoát bằng `Ctrl+]`.

Terminal 2 — dữ liệu mới qua HTTPS API và SQLite:

```powershell
.\scripts\windows\run_server.ps1 -Action Watch
```

Thoát bằng `Ctrl+C`.

Terminal 3 — cài và mở app trên màn hình Pixel 6:

```powershell
.\scripts\windows\run_app.ps1 -Action Run -Target Pixel6
```

Lệnh được nhập trong PowerShell; sau khi app hiện trên Pixel 6, dùng chuột thao tác trên màn
hình giả lập. Giữ terminal để xem Flutter log, nhấn `q` trong terminal khi muốn dừng app.

Trong app chọn nguồn `Server`. Đạt khi ESP32 có telemetry mới, server trả đúng `farm-01`, và
app hiển thị cùng dữ liệu. Trước hiệu chuẩn LM393, `soil_raw` có số nhưng phần trăm có thể là `--`.

Nếu muốn xem từng lớp server, mở thêm terminal:

```powershell
# MQTT broker + subscribe/DB insert + API request logs
.\scripts\windows\run_server.ps1 -Action Logs

# 5 dòng SQLite mới nhất, cập nhật mỗi 2 giây
.\scripts\windows\run_server.ps1 -Action Db

# Trạng thái các service
.\scripts\windows\run_server.ps1 -Action Services
```

`Watch` dùng HTTPS nên không cần SSH. `Logs`, `Db` và `Services` SSH vào Pi qua LAN
`192.168.1.4`; khi được hỏi, nhập mật khẩu user `admin` (terminal không hiện ký tự).
IP Tailscale `100.104.200.110` hiện bị tailnet ACL từ chối SSH, dù HTTPS vẫn hoạt động.

## 3. Demo offline bằng Wi-Fi AP

Không cần tắt Pi. Script tắt mobile data của A24 và nối điện thoại vào AP của ESP32, nên app
không thể dùng Internet/server trong bài test này:

Nếu cần chứng minh **server thật sự mất Internet**, rút dây mạng/tắt Wi-Fi của Pi sau khi hoàn
thành bước 2, rồi cắm lại sau bước 4. Không đổi cấu hình Wi-Fi của ESP32 trong bài test này.

```powershell
.\scripts\windows\phone_ap_test.ps1 -Action Connect
```

Trong app: `Kết nối trực tiếp` → `Dùng Wi-Fi trực tiếp` → quay lại Dashboard và làm mới.
Tiếp tục mở `Cấu hình Wi-Fi / MQTT cho ESP32` → `Đọc cấu hình thiết bị`.

Đạt khi app đọc được telemetry tại `http://192.168.4.1` và đọc được cấu hình trong lúc mobile
data đang tắt. Không thử bật relay.

Khôi phục mạng điện thoại:

```powershell
.\scripts\windows\phone_ap_test.ps1 -Action Restore
```

## 4. Demo offline bằng BLE

Script tắt Wi-Fi và mobile data của A24; Bluetooth phải được bật thủ công:

```powershell
.\scripts\windows\phone_ble_test.ps1 -Action Isolate
```

Trong app: `Kết nối trực tiếp` → `Quét ESP32` → chọn `SAEMS-farm-01`. Quay lại Dashboard,
làm mới, sau đó mở `Cấu hình Wi-Fi / MQTT cho ESP32` → `Đọc cấu hình thiết bị`.

Đạt khi app vẫn nhận telemetry và đọc cấu hình dù Wi-Fi/mobile data đều tắt. Không thử relay.

Khôi phục mạng điện thoại:

```powershell
.\scripts\windows\phone_ble_test.ps1 -Action Restore
```

## Kết luận nghiệm thu vòng này

- Online đạt: Device → MQTT TLS → Pi → HTTPS API → App.
- AP đạt: Device ↔ App qua Wi-Fi AP khi điện thoại không có Internet.
- BLE đạt: Device ↔ App qua BLE khi điện thoại không có Wi-Fi/mobile data.
- Power Manager: ba mode nằm trong một firmware và được lưu trong NVS; cần chuyển
  bằng app rồi nghiệm thu Light/Deep Sleep và dòng tiêu thụ trên board.
- Chưa nghiệm thu: phép đo dòng/pin và relay có tải.
