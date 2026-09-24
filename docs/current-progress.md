# Smart Farm – trạng thái mã nguồn đã chuẩn hóa

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
│       └── main/
│           ├── CMakeLists.txt
│           ├── Kconfig.projbuild
│           ├── idf_component.yml
│           └── main.c
└── services/
    └── mini-server/
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
        └── systemd/
```

## Các lỗi đã xử lý

- Không đưa `hello_world_main.c` hoặc `pytest_hello_world.py` vào module firmware.
- `main/CMakeLists.txt` chỉ đăng ký `main.c`.
- Tên project và binary là `smartfarm_esp32s3`, không còn `hello_world`.
- Credentials firmware nằm trong Kconfig và `sdkconfig` cục bộ.
- Sửa nhánh phục hồi NVS để kiểm tra kết quả khởi tạo lại, không kiểm tra biến
  lỗi cũ.
- Thêm `boot_id` để `seq` bắt đầu lại sau reboot không làm SQLite bỏ dữ liệu.
- Tách quyền MQTT của thiết bị và server; ingestor không dùng tài khoản chỉ có
  quyền publish.
- README dùng đúng endpoint `/api/v1/devices/{device_id}/...`.
- Script và systemd dùng Python/uvicorn trong `.venv`.
- Dashboard không phụ thuộc Chart.js CDN.

## Giới hạn hiện tại

Firmware publish đúng JSON telemetry nhưng `read_sensor_snapshot()` vẫn sinh dữ
liệu mô phỏng. Việc đọc cảm biến vật lý cần biết model cảm biến, bus và chân GPIO;
không được ghi nhận là đã hoàn thành cho tới khi driver thật được tích hợp và thử
trên bo mạch.

MQTT hiện dùng TCP 1883 không mã hóa, phù hợp demo trong LAN tin cậy. TLS và điều
khiển bơm là các hạng mục tiếp theo.

## Tiêu chí nghiệm thu

```text
Firmware build chỉ có main/main.c, không có Hello world!
ESP32 log Wi-Fi connected, MQTT connected và Published boot=... seq=...
Ingestor log Telemetry saved
SQLite chứa boot_id và seq
API trả đúng telemetry tại /api/v1/devices/farm-01/telemetry/latest
Dashboard cập nhật trong LAN mà không cần Internet
```
