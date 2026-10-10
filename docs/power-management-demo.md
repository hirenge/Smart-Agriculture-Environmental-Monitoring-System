# Power Manager: một firmware, ba chế độ runtime

Firmware SAEMS chứa đồng thời cả ba chế độ. Chỉ cần flash một lần; sau đó app đổi
`power_mode` qua BLE hoặc Wi-Fi AP. Giá trị được lưu trong NVS và áp dụng ở ranh
giới chu kỳ lấy mẫu kế tiếp, không cần build, flash hay reset.

| `power_mode` | Cơ chế giữa hai lần lấy mẫu | Kết nối trực tiếp | Mục đích |
| --- | --- | --- | --- |
| `direct` | Wi-Fi modem power-save hoặc chờ chủ động | BLE/AP luôn sẵn sàng | Cấu hình, debug và demo |
| `light` | DFS + automatic Light Sleep + tickless idle | BLE/AP vẫn sẵn sàng | Vận hành có kết nối thường xuyên |
| `battery` | RTC timer Deep Sleep | BLE/AP có trong cửa sổ thức; BLE đang kết nối sẽ hoãn ngủ | Chạy pin, tiết kiệm điện cao nhất |

`direct` là chế độ bảo trì, không phải bằng chứng nghiệm thu tiết kiệm điện. Để đạt
yêu cầu đề tài, vận hành thực tế phải chọn `light` hoặc `battery` và đo dòng trên
phần cứng.

## Luồng của một chu kỳ

```text
thức/chạy → nạp power_mode từ NVS → đọc sensor → lưu FIFO NVS
          → thử gửi MQTT → đọc lại power_mode → nghỉ theo mode đã chọn
```

- `direct`: task chờ tới chu kỳ sau, Wi-Fi dùng modem power-save khi đã cấu hình.
- `light`: ESP-IDF tự hạ tần số và vào Light Sleep trong thời gian task rảnh.
- `battery`: firmware dành tối thiểu 10 giây cho BLE/AP và đồng bộ, tối đa 15 giây
  nếu còn dữ liệu chờ; sau đó tắt relay, dừng Wi-Fi và vào Deep Sleep bằng RTC timer.
- Telemetry luôn được ghi vào FIFO NVS trước khi gửi. Mất server không làm treo thiết bị; dữ liệu chờ được gửi bù ở các chu kỳ sau.
- Trong một chuỗi Deep Sleep, `boot_id` được giữ và `seq` tiếp tục tăng nhờ RTC memory.

## Build và flash đúng cách

Chạy tại `merge_github` trong PowerShell:

```powershell
.\scripts\windows\run_device.ps1 -Action Build -PowerProfile Direct
.\scripts\windows\run_device.ps1 -Action Flash -PowerProfile Direct
```

`-PowerProfile` nay chỉ đặt **mode mặc định lần đầu** nếu NVS chưa có `power_mode`.
Nó không tạo firmware thiếu các mode còn lại. Với sử dụng bình thường, build/flash
một lần bằng `Direct`, rồi chuyển mode qua app.

## Chuyển mode bằng app

Đây là phương án ưu tiên: A24 dùng BLE trực tiếp với ESP32 nên laptop vẫn giữ
Wi-Fi rooftop. Nếu app chưa hiện dropdown `Chế độ năng lượng`, chạy bản source mới
bằng `.\scripts\windows\run_app.ps1 -Action Run -Target A24`. Sau đó trên A24:

1. Mở `Kết nối trực tiếp`, kết nối `SAEMS-farm-01` bằng BLE hoặc Wi-Fi AP.
2. Mở `Cấu hình Wi-Fi / MQTT cho ESP32`.
3. Bấm `Đọc cấu hình thiết bị` và kiểm tra mode hiện tại.
4. Chọn `Direct`, `Light Sleep` hoặc `Battery – Deep Sleep`.
5. Với Battery nên đặt chu kỳ telemetry từ 60 giây trở lên.
6. Bấm `Lưu cấu hình vào ESP32`.
7. Nếu chọn Battery qua BLE, ngắt kết nối BLE sau khi lưu để thiết bị được ngủ sâu.

Mode mới được dùng khi mẫu hiện tại kết thúc. Các thay đổi Wi-Fi/MQTT khác vẫn cần
khởi động lại như thông báo trong app.

## Chuyển mode bằng PowerShell qua Wi-Fi AP

Laptop phải nối Wi-Fi `SAEMS-farm-01`:

```powershell
# Xem mode hiện tại
.\scripts\windows\power_mode.ps1 -Action Read

# Light Sleep, không flash
.\scripts\windows\power_mode.ps1 -Action Set -Mode Light

# Deep Sleep và đổi chu kỳ thành 60 giây
.\scripts\windows\power_mode.ps1 -Action Set -Mode Battery -TelemetryIntervalSeconds 60

# Trở lại chế độ cấu hình/debug
.\scripts\windows\power_mode.ps1 -Action Set -Mode Direct
```

Ở Battery, AP chỉ truy cập được khi ESP32 đang thức. Nếu request timeout, chờ lần
RTC wake kế tiếp hoặc nhấn EN một lần rồi chạy lại trong cửa sổ thức.

## Kiểm tra trên UART

```powershell
.\scripts\windows\run_device.ps1 -Action Monitor
```

Khi chuyển mode cần thấy một trong các log sau:

```text
Power mode changed: modem-save -> light-sleep
Power mode applied at sample boundary: direct -> light
Idle ... seconds; automatic light sleep enabled ...
```

Với Battery:

```text
Power mode applied at sample boundary: light -> battery
Battery window ended: pending=... maintenance=10s deadline=15s
Entering deep sleep for ... seconds
Wake reason: timer
Resumed RTC sequence: boot=... next=... limit=...
```

## Nghiệm thu yêu cầu năng lượng

1. Chuyển `direct → light → direct` từ app, không flash; xác nhận config đọc lại và
   UART ghi nhận đúng mode ở ranh giới chu kỳ.
2. Chuyển sang `battery`, ngắt BLE rồi quan sát ít nhất hai lần RTC wake. `boot_id`
   phải giữ nguyên, `seq` tăng và thiết bị phải vào Deep Sleep giữa hai mẫu.
3. Ngắt Internet/server trong 2–3 chu kỳ Battery. Thiết bị vẫn lấy mẫu, `pending`
   tăng và vẫn ngủ; khi mạng trở lại, `pending` phải giảm về 0.
4. Trong cửa sổ thức Battery, kết nối BLE/AP và chuyển về `direct`; thiết bị phải
   ngừng vào Deep Sleep từ ranh giới chu kỳ tiếp theo mà không flash.
5. Đo dòng nguồn ở `direct`, `light`, pha thức Battery và pha Deep Sleep. Board phát
   triển còn LED nguồn, USB-UART và regulator nên dòng toàn board cao hơn dòng chip.

Source code và log chỉ chứng minh logic. Yêu cầu tối ưu năng lượng chỉ được nghiệm
thu hoàn chỉnh sau khi có pin/nguồn đo và số đo dòng thực tế.
