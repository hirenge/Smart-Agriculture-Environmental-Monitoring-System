# SAEMS firmware – kết quả giai đoạn 3–9

Tài liệu này phân biệt rõ “đã hiện thực”, “đã compile” và “đã test phần cứng”.

| Giai đoạn | Phạm vi | Kết quả mã nguồn | Xác minh hiện tại |
| --- | --- | --- | --- |
| 3 | Application/Data Manager | State machine và telemetry schema v1, không sinh số giả | Compile đạt |
| 4 | Config Manager | NVS cho Wi-Fi/MQTT/AP, interval, LM393 và cờ manager; không trả secret | Compile đạt; test reboot pending |
| 5 | Network/MQTT Manager | STA/APSTA/AP-only, reconnect, topic SAEMS, command và QoS 1 | Compile đạt; test router/broker pending |
| 6 | Storage/Sync Manager | FIFO NVS 32 bản ghi, store-first, ACK-before-delete, gửi bù | Compile đạt; test mất nguồn/mất mạng pending |
| 7 | BLE/Wi-Fi AP Manager | NimBLE GATT, telemetry frame, provisioning, local REST API | Compile đạt; test Android/AP pending |
| 8 | Relay/Safety | CH1/CH2 boot OFF, pump command, timer auto-off | Compile đạt; test relay/bơm pending |
| 9 | Power/Partition/Hardening | Modem-save, guard sleep, NVS/OTA/coredump partitions, tài liệu test | Compile đạt; đo dòng và OTA flow pending |

## Tiêu chí test trên bo mạch

1. Flash firmware, xóa NVS ở lần test provisioning đầu và kiểm tra không có log
   `Hello world!`.
2. Không cấu hình Wi-Fi: thiết bị vẫn đọc sensor, lưu queue và quảng bá BLE.
3. Gửi credentials qua BLE hoặc AP, reboot, xác nhận STA và MQTT kết nối.
4. Tắt broker/router, tạo nhiều mẫu, reboot ESP32, bật mạng và xác nhận gửi bù theo
   `boot_id + seq` mà không xóa record trước PUBACK.
5. Kiểm tra relay luôn OFF khi boot/reset và tự OFF khi hết `duration_s` hoặc giới
   hạn `CONFIG_SAEMS_RELAY_MAX_ON_SECONDS`.
6. Đo dòng active và modem-save. Không bật deep sleep mặc định trước khi test wake
   source, thời gian reconnect và yêu cầu nhận lệnh thời gian thực.

## Phần chưa thể hoàn tất chỉ bằng source code

- LM393 cần que đo và hai mốc ADC khô/ướt của đúng mẫu đất.
- BLE/AP cần test tương thích với điện thoại thật.
- Relay cần xác nhận active-low/active-high và logic 3,3 V của module thực tế.
- HTTPS Internet thuộc lớp mini-server/reverse proxy, không chạy trên AP local của
  firmware và cần domain/certificate/máy chủ triển khai.
