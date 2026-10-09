# SAEMS mini server

## Bản đang chạy trên Pi: HTTPS Internet

API/dashboard: `https://admin.tailb1f0ba.ts.net`; MQTT TLS:
`mqtts://admin.tailb1f0ba.ts.net:8443`.

Bản mới chạy tại `/home/admin/saems-mini-server` với bốn service `saems-mqtt`,
`saems-ingestor`, `saems-api`, `saems-legacy-ingestor`. Broker mới chỉ bind loopback `1884`, API bind
loopback `3000`; Tailscale Funnel cấp TLS/public routing. Bản cũ và broker LAN
`1883` không bị thay thế. Credentials mới nằm trong `device-access.json` riêng
tư, không được commit.

App/API dùng Bearer token; dashboard dùng username `saems`, password là API
token. `/health` công khai; các route dữ liệu, dashboard và docs có xác thực.
`API_PUBLIC_MODE=true` yêu cầu token ít nhất 32 ký tự. CORS mặc định không mở
wildcard; `CORS_ORIGINS` chỉ thêm origin web thực sự cần thiết.

Xem [runbook Internet/provisioning](../../docs/internet-and-provisioning.md)
cho cài app, cấu hình ESP32, bằng chứng kiểm thử, bảo trì và tắt public endpoint.
Không chạy thêm API/ingestor thủ công trùng với các service đang chạy.

Bộ nhận tương thích `legacy_mqtt_ingestor.py` chỉ đọc
`smartfarm/farm-01/telemetry` từ broker LAN cũ, giữ nguyên payload và dùng chung
validator/database/dedup. Không gửi lệnh và không thay firmware. Mật khẩu cũ
được cấu hình riêng trong `.legacy.env` (600, Git ignored). Ba unit test đạt.
Phiên sau đã flash firmware mới: MQTT TLS từ ESP32 tới `saems-ingestor` trực tiếp,
không phụ thuộc bridge cũ. Samsung nhận dữ liệu DHT20/BH1750 qua HTTPS bằng 4G;
BLE/AP read/config đã kiểm thử trên A24. Bridge vẫn được giữ để tương thích.
Chi tiết: [nghiệm thu firmware mới](../../docs/new-firmware-device-server-app-acceptance.md).
SSH qua Tailscale hiện bị policy từ chối; HTTPS không đồng nghĩa có quyền SSH.

Các hướng dẫn LAN bên dưới là phương án triển khai khác, không phải lệnh dùng
để thay thế broker đang chạy trên Pi. `run_api.sh` mặc định bind loopback; chỉ
đặt `API_BIND_HOST=0.0.0.0` khi chủ động cho phép LAN và đã cấu hình xác thực.

Module nhận telemetry MQTT từ ESP32-S3, lưu vào SQLite và cung cấp REST API cùng
dashboard web. Dashboard không dùng CDN nên chạy được hoàn toàn trong mạng LAN.

## Luồng dữ liệu

```text
ESP32-S3 -> Mosquitto -> mqtt_ingestor.py -> SQLite -> FastAPI -> Dashboard
```

Hai tài khoản MQTT có quyền tách biệt:

- `saems_esp32`: thiết bị chỉ được publish telemetry của `farm-01`.
- `saems_server`: ingestor được subscribe telemetry của các thiết bị.

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

Sửa `.env` và nhập mật khẩu của tài khoản `saems_server`. Giá trị phải theo
cú pháp biến môi trường shell vì các script dùng `source .env`.

## Cấu hình Mosquitto

Tạo hai tài khoản. Chỉ dùng `-c` ở lệnh đầu vì tùy chọn này tạo mới và ghi đè
password file:

```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd saems_esp32
sudo mosquitto_passwd /etc/mosquitto/passwd saems_server
sudo install -m 640 -o root -g mosquitto config/acl.example /etc/mosquitto/acl
sudo install -m 644 config/mosquitto.conf.example /etc/mosquitto/conf.d/saems.conf
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
- `GET /api/v1/devices`
- `GET /api/v1/devices/farm-01/status`
- `GET /api/v1/devices/farm-01/telemetry/latest`
- `GET /api/v1/devices/farm-01/telemetry?limit=50`
- `POST /api/v1/devices/farm-01/commands/pump`
- `GET /docs`

`light_lux` is the canonical field name from the ESP32 payload. The API also
keeps `light_value` as a compatibility alias for the existing dashboard and
older clients. The app should call `GET /api/v1/devices` at startup to obtain
the device list, then use the selected `device_id` with the status or telemetry
endpoint.

Điều khiển bơm dùng JSON:

```json
{"state":"on","duration_s":30}
```

hoặc:

```json
{"state":"off","duration_s":0}
```

API chỉ xác nhận broker đã nhận lệnh (`status: queued`); trạng thái phần cứng
được xác nhận ở telemetry kế tiếp của ESP32. Firmware luôn tự tắt bơm sau thời
gian an toàn cấu hình trong menuconfig.

Kiểm tra toàn bộ nhanh bằng:

```bash
./scripts/check_system.sh
```

## Kiểm thử mất mạng và reconnect

1. Chạy ingestor, API và theo dõi log ESP32. Khi MQTT bị ngắt, ESP32 ghi
   `MQTT offline; buffered telemetry` và vẫn đọc cảm biến.
2. Khôi phục broker hoặc Wi-Fi. ESP32 phải ghi `Flushed offline telemetry` rồi
   tiếp tục publish; ingestor phải ghi `Telemetry saved`.
3. Kiểm tra SQLite không có bản ghi trùng bằng khóa `device_id + boot_id + seq`:

```bash
sqlite3 data/telemetry.db \
  "SELECT device_id, boot_id, seq, COUNT(*) FROM telemetry GROUP BY device_id, boot_id, seq HAVING COUNT(*) > 1;"
```

Firmware modular mới giữ tối đa 32 mẫu trong FIFO NVS, bảo toàn mẫu chờ PUBACK
và từ chối mẫu mới khi đầy. Đã quan sát gửi mẫu startup chờ đồng bộ thời gian;
khả năng gửi bù đầy đủ sau mất mạng/mất điện vẫn chưa nghiệm thu trên board.
Thông số này áp dụng cho firmware mới đã flash, không phải firmware legacy.

## Chạy bằng systemd

Các file trong `systemd/` dùng đường dẫn bản triển khai hiện tại:

```text
/home/admin/saems-mini-server
```

Nếu vị trí hoặc user khác, sửa đồng thời `User`, `WorkingDirectory`,
`EnvironmentFile` và `ExecStart`. Sau đó:

```bash
sudo install -m 644 systemd/saems-ingestor.service.example /etc/systemd/system/saems-ingestor.service
sudo install -m 644 systemd/saems-api.service.example /etc/systemd/system/saems-api.service
sudo systemctl daemon-reload
sudo systemctl enable --now saems-ingestor saems-api
```

Kiểm tra sau khi cài:

```bash
systemctl status saems-ingestor saems-api --no-pager
journalctl -u saems-ingestor -f
curl http://127.0.0.1:3000/health
```

## Dữ liệu và bảo mật

Database được tạo tại `data/telemetry.db` và không được commit. ESP32 cấp phát
sequence theo block trong NVS nên sequence không quay lại `1` sau reboot. Schema
vẫn dùng `device_id + boot_id + seq` để khử bản tin QoS 1 trùng. Ingestor tự
chuyển database cũ từ index `device_id + seq`.

Không commit `.env`, database, `.venv` hoặc log. MQTT cổng 1883 chỉ phù hợp demo
trong LAN tin cậy; triển khai bên ngoài LAN cần TLS và kiểm soát truy cập mạng.
