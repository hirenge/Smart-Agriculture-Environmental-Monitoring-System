# SAEMS mini-server

Mini-server nhận telemetry MQTT từ ESP32-S3, kiểm tra schema, lưu SQLite và cung
cấp REST API/dashboard cho ứng dụng. Bản triển khai hiện tại chạy trên Raspberry
Pi 4 tại `/home/admin/saems-mini-server`.

## Kiến trúc đang chạy

```text
ESP32-S3
  └─ MQTT TLS :8443
      └─ Mosquitto (saems-mqtt, origin loopback :1884)
          └─ mqtt_ingestor.py (saems-ingestor)
              └─ SQLite data/telemetry.db
                  └─ FastAPI api.py (saems-api, loopback :3000)
                      └─ Tailscale Funnel HTTPS
                          ├─ Android app
                          └─ Web dashboard
```

Endpoint công khai hiện dùng:

- HTTPS API/dashboard: `https://admin.tailb1f0ba.ts.net`
- MQTT TLS: `mqtts://admin.tailb1f0ba.ts.net:8443`

Bốn service là `saems-mqtt`, `saems-ingestor`, `saems-api` và
`saems-legacy-ingestor`. Service legacy chỉ đọc topic firmware cũ để tương
thích; firmware mới publish trực tiếp vào broker TLS SAEMS.

## Thành phần mã nguồn

| Thành phần | Vai trò |
| --- | --- |
| `mqtt_ingestor.py` | Subscribe telemetry, validate, deduplicate và insert SQLite |
| `legacy_mqtt_ingestor.py` | Chuyển envelope topic cũ sang schema chung |
| `api.py` | API thiết bị, telemetry, command và dashboard |
| `mqtt_security.py` | TLS/certificate cho MQTT client |
| `dashboard/index.html` | Dashboard không phụ thuộc CDN |
| `scripts/` | Khởi chạy, theo dõi và health check trên Pi |
| `systemd/` | Mẫu service tự khởi động |
| `config/` | Mẫu Mosquitto listener và ACL |

SQLite khử trùng theo `device_id + boot_id + seq`. Bản tin QoS 1 được gửi lại
không tạo dòng trùng.

## Xác thực và credentials

- `/health` công khai để kiểm tra trạng thái.
- API dữ liệu dùng `Authorization: Bearer <token>`.
- Dashboard dùng username `saems`; password là API token.
- MQTT dùng tài khoản/ACL riêng cho ESP32 và server.
- Credentials cục bộ nằm trong `.local/device-access.json` hoặc file `.env`
  trên Pi; không được ghi vào README, source hoặc commit Git.

HTTPS cho app/API không đồng nghĩa với quyền SSH vào Pi. `Logs`, `Db` và
`Services` trong script Windows vẫn cần đường SSH hợp lệ tới Pi.

## Demo server từ thư mục script chung

Chạy tại thư mục gốc repository:

```powershell
Set-Location 'C:\Users\Tien An\esp\SAEMS_esp32_test\merge_github'

# Kiểm tra health và bản telemetry mới nhất qua Internet
.\scripts\windows\run_server.ps1 -Action Check

# Theo dõi bản ghi mới từ API/SQLite theo thời gian
.\scripts\windows\run_server.ps1 -Action Watch

# Các lệnh dưới cần SSH tới Pi
.\scripts\windows\run_server.ps1 -Action Services
.\scripts\windows\run_server.ps1 -Action Logs
.\scripts\windows\run_server.ps1 -Action Db
```

`Check` và `Watch` dùng HTTPS nên laptop không cần chung Wi-Fi với Pi. Mặc
định `Logs/Db/Services` dùng `admin@192.168.1.4`; có thể truyền `-PiHost`
khi đã có địa chỉ Tailscale/SSH hợp lệ.

Luồng demo ba lớp và các terminal cần mở nằm tại
[scripts/windows/RUN-DEMO.md](../../scripts/windows/RUN-DEMO.md).

## API chính

| Method | Endpoint | Mục đích |
| --- | --- | --- |
| GET | `/health` | Health check |
| GET | `/api/v1/devices` | Danh sách thiết bị |
| GET | `/api/v1/devices/{id}/status` | Trạng thái thiết bị |
| GET | `/api/v1/devices/{id}/telemetry/latest` | Mẫu mới nhất |
| GET | `/api/v1/devices/{id}/telemetry?limit=50` | Lịch sử |
| POST | `/api/v1/devices/{id}/commands/pump` | Xếp lệnh bơm vào MQTT |
| GET | `/docs` | OpenAPI UI có xác thực |

Lệnh bơm mẫu:

```json
{"state":"on","duration_s":30}
```

API trả `queued` chỉ có nghĩa broker đã nhận lệnh. Trạng thái relay phải được
xác nhận bằng telemetry tiếp theo; firmware vẫn chịu trách nhiệm auto-off.

## Cài đặt hoặc phục hồi trên Raspberry Pi

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients sqlite3 python3 python3-venv

cd /home/admin/saems-mini-server
python3 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install -r requirements.txt
cp .env.example .env
chmod 600 .env
```

Sau khi điền `.env`, chạy thủ công chỉ khi systemd đang dừng để tránh hai
ingestor/API cùng dùng một database hoặc port:

```bash
./scripts/run_ingestor.sh
./scripts/run_api.sh
./scripts/monitor_mqtt.sh
./scripts/check_system.sh
```

Kiểm tra service đang triển khai:

```bash
systemctl is-active saems-mqtt saems-ingestor saems-legacy-ingestor saems-api tailscaled
journalctl -u saems-mqtt -u saems-ingestor -u saems-api -f -o short-iso
curl http://127.0.0.1:3000/health
```

## Kiểm thử source server

Từ `services/mini-server`:

```bash
.venv/bin/python -m unittest discover -s tests -v
python3 -m py_compile api.py mqtt_ingestor.py legacy_mqtt_ingestor.py mqtt_security.py
```

## Giới hạn vận hành

- Không commit `.env`, database, MQTT password file, `.venv` hoặc log.
- Broker TCP 1883 cũ chỉ dành cho tương thích LAN; thiết bị mới dùng TLS 8443.
- CORS không mở wildcard trong public mode.
- Tailscale SSH hiện phụ thuộc ACL riêng; HTTPS vẫn hoạt động khi SSH bị từ chối.
- Trước khi thay service/config trên Pi phải sao lưu database và kiểm tra đường
  dẫn `/home/admin/saems-mini-server`.

Báo cáo hiện trạng: [IMPLEMENTATION-REPORT.md](../../docs/IMPLEMENTATION-REPORT.md).
