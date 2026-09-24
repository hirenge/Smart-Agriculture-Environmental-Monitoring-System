# Smart Farm mini server

Module nhận telemetry MQTT từ ESP32-S3, lưu vào SQLite và cung cấp REST API cùng
dashboard web. Dashboard không dùng CDN nên chạy được hoàn toàn trong mạng LAN.

## Luồng dữ liệu

```text
ESP32-S3 -> Mosquitto -> mqtt_ingestor.py -> SQLite -> FastAPI -> Dashboard
```

Hai tài khoản MQTT có quyền tách biệt:

- `smartfarm_esp32`: thiết bị chỉ được publish telemetry của `farm-01`.
- `smartfarm_server`: ingestor được subscribe telemetry của các thiết bị.

## Cài đặt

Trên Raspberry Pi:

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients sqlite3 python3 python3-venv

cd services/mini-server
python3 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install -r requirements.txt
cp .env.example .env
chmod 600 .env
```

Sửa `.env` và nhập mật khẩu của tài khoản `smartfarm_server`. Giá trị phải theo
cú pháp biến môi trường shell vì các script dùng `source .env`.

## Cấu hình Mosquitto

Tạo hai tài khoản. Chỉ dùng `-c` ở lệnh đầu vì tùy chọn này tạo mới và ghi đè
password file:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd smartfarm_esp32
sudo mosquitto_passwd /etc/mosquitto/passwd smartfarm_server
sudo install -m 640 -o root -g mosquitto config/acl.example /etc/mosquitto/acl
sudo install -m 644 config/mosquitto.conf.example /etc/mosquitto/conf.d/smartfarm.conf
sudo systemctl enable --now mosquitto
sudo systemctl restart mosquitto
```

Nếu Mosquitto báo cấu hình trùng `listener`, kiểm tra các file khác trong
`/etc/mosquitto/conf.d/` trước khi khởi động lại.

## Chạy thủ công

Terminal 1:

```bash
./scripts/run_ingestor.sh
```

Terminal 2:

```bash
./scripts/run_api.sh
```

Terminal 3, nếu cần quan sát MQTT:

```bash
./scripts/monitor_mqtt.sh
```

Mở dashboard tại `http://<IP_RASPBERRY_PI>:3000/`.

## API

- `GET /health`
- `GET /api/v1/devices/farm-01/status`
- `GET /api/v1/devices/farm-01/telemetry/latest`
- `GET /api/v1/devices/farm-01/telemetry?limit=50`
- `GET /docs`

Kiểm tra toàn bộ nhanh bằng:

```bash
./scripts/check_system.sh
```

## Chạy bằng systemd

Các file trong `systemd/` giả định repository nằm tại:

```text
/home/admin/Smart-Agriculture-Environmental-Monitoring-System
```

Nếu vị trí hoặc user khác, sửa đồng thời `User`, `WorkingDirectory`,
`EnvironmentFile` và `ExecStart`. Sau đó:

```bash
sudo install -m 644 systemd/smartfarm-ingestor.service.example /etc/systemd/system/smartfarm-ingestor.service
sudo install -m 644 systemd/smartfarm-api.service.example /etc/systemd/system/smartfarm-api.service
sudo systemctl daemon-reload
sudo systemctl enable --now smartfarm-ingestor smartfarm-api
```

## Dữ liệu và bảo mật

Database được tạo tại `data/telemetry.db` và không được commit. Schema dùng
`device_id + boot_id + seq` để khử bản tin QoS 1 trùng mà không bỏ dữ liệu sau
khi ESP32 reboot. Ingestor tự chuyển database cũ từ index `device_id + seq`.

Không commit `.env`, database, `.venv` hoặc log. MQTT cổng 1883 chỉ phù hợp demo
trong LAN tin cậy; triển khai bên ngoài LAN cần TLS và kiểm soát truy cập mạng.
