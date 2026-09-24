# Smart Farm merge plan

Thư mục này được chuẩn bị để chép vào repository chung mà không sửa các file
firmware đang nằm ở root của repository.

```text
firmware/esp32s3/       ESP-IDF firmware gửi telemetry
services/mini-server/   MQTT ingestor, SQLite, API và dashboard
```

Nên đưa hai module vào một feature branch riêng. Không di chuyển hoặc xóa project
ESP-IDF ở root trong cùng pull request; việc chuẩn hóa firmware root nên được làm
trong một pull request riêng sau khi thống nhất với nhóm.

Trước khi commit:

```bash
git status --short
git diff --check
git ls-files | grep -E 'telemetry\.db|(^|/)\.env$|(^|/)sdkconfig$|(^|/)\.venv/|__pycache__|\.(bin|elf)$' && exit 1 || true
```

Không commit mật khẩu, `.env`, `sdkconfig`, database, `.venv`, thư mục build hoặc
firmware binary.
