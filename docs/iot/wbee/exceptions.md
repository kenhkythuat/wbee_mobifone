# WBEE Errors and Edge Cases

## Ma trận lỗi

| Điều kiện | Phát hiện | Xử lý | Log/kết quả |
|---|---|---|---|
| PLC có ký tự không phải hex | Decode thất bại | Bỏ frame | `PLC RS485 frame decode fail` |
| LRC sai | Tổng modulo 256 khác 0 | Bỏ frame | `PLC RS485 LRC error` |
| Sai slave/function/length | Header validation lỗi | Bỏ frame | `PLC RS485 unsupported frame` |
| PLC im lặng quá timeout | Counter > 300 s | Xóa cờ dữ liệu | Telemetry PLC thành `null` |
| Frame bị chia qua callback | Không có accumulator | Có thể bỏ cả hai phần | Decode/LRC error |
| Nhiều bit cùng bật | Duyệt từ bit 0 | Lấy bit đầu tiên | Các bit sau không được gửi |
| Raw bitfield bằng 0 | Không tìm thấy bit | Vẫn gửi `0` | Không phân biệt với bit 0 bật |
| MQTT publish lỗi | Không nhận URC thành công | Retry rồi reconnect | `Publish fail` |
| SIM chưa đăng ký mạng | Thiếu trạng thái registration | Retry/reset theo ngưỡng | Không vào MQTT |
| Payload không hỗ trợ | Không khớp parser | Bỏ qua | Chưa có response chuẩn |
| Manifest HTTP lỗi | Status/length lỗi | Thử URL fallback | OTA failed |
| Manifest sai device/address/size | Validation lỗi | Từ chối | `manifest ... invalid` |
| BIN sai Content-Length | HTTP length khác `size` | Dừng OTA | `binary length mismatch` |
| CRC sai | CRC stream/Flash khác manifest | Dừng OTA | CRC mismatch log |
| Version không mới hơn | So sánh version | Không tải BIN | `OTA no newer version` |
| BIN compile version cũ | Script chỉ đặt tên package | Vẫn có thể cài | Telemetry báo version compile thực |
| Mất nguồn khi tải staging | Chưa có pending metadata | Boot app cũ | Application hiện tại an toàn |
| App mới lỗi runtime | Không có rollback | Có thể reset lặp | Cần ST-LINK để phục hồi |

## Checklist không nhận PLC

1. Kiểm tra USART2 là `9600 8E1`.
2. Kiểm tra PLC gửi Modbus ASCII, không phải Modbus RTU.
3. Kiểm tra slave `02`, function `10` và byte count.
4. `CR LF` phải là byte `0x0D 0x0A`.
5. Tổng tất cả byte gồm LRC phải bằng `0x00` modulo 256.
6. Kiểm tra frame có bị chia qua nhiều callback hay không.

## Checklist telemetry null

1. Tìm log `PLC RS485 data updated`.
2. Kiểm tra log timeout 300 giây.
3. Kiểm tra start address có nằm trong mapping ở `config.h`.
4. Kiểm tra frame đã qua LRC và header validation.

## Checklist OTA

1. Thiết bị đang ở state `Subscribed`.
2. Payload chứa `ota_check` hoặc `ota_update`.
3. Manifest raw URL trả HTTP 200.
4. `device`, `app_addr`, `size` và CRC đúng.
5. Remote version lớn hơn `VERSION_WBEE`.
6. Bootloader đã được nạp tại `0x08000000`.

## Rủi ro đã biết

- MQTT cổng 1883 không TLS.
- HTTPS OTA đang tắt xác thực certificate.
- CRC32 không thay thế chữ ký số firmware.
- Parser motor dựa trên offset ký tự trong URC SIMCOM.
- Chưa có rollback tự động và acknowledgement đầy đủ cho config/command.
