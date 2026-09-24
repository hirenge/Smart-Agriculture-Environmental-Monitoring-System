# ESP32-S3 telemetry firmware

Firmware kết nối Wi-Fi, kết nối MQTT và publish telemetry mỗi 10 giây. Project
không chứa `hello_world_main.c`; `main/CMakeLists.txt` chỉ biên dịch `main.c`.

## Trạng thái dữ liệu cảm biến

`read_sensor_snapshot()` hiện tạo dữ liệu mô phỏng để kiểm thử toàn bộ pipeline.
Đây chưa phải dữ liệu từ cảm biến vật lý. Khi đã chốt loại cảm biến và GPIO, thay
nội dung hàm này bằng driver thật nhưng giữ nguyên `sensor_snapshot_t` và JSON.

Mỗi lần khởi động firmware tạo một `boot_id`. Server dùng bộ ba
`device_id + boot_id + seq` để nhận biết bản tin QoS 1 bị gửi lặp mà không làm mất
dữ liệu sau khi ESP32 khởi động lại.

## Cấu hình và build

```powershell
cd firmware/esp32s3
idf.py set-target esp32s3
idf.py menuconfig
```

Mở `Smart Farm firmware`, nhập Wi-Fi, MQTT broker, tài khoản thiết bị và mật
khẩu. Các giá trị nhạy cảm được lưu trong `sdkconfig` cục bộ và không được commit.

```powershell
idf.py fullclean
idf.py build
idf.py -p COM5 flash monitor
```

Kết quả đúng phải có log `SMARTFARM`, `MQTT connected` và
`Published boot=... seq=...`; không được có `Hello world!`.
