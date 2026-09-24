# WBEE MQTT and Data Contract

## Tổng quan

WBEE nhận dữ liệu PLC qua Modbus ASCII, lưu snapshot mới nhất, tạo JSON và gửi cùng dữ liệu đến MQTT broker và màn hình UART5. SIMCOM A7680 được điều khiển bằng AT command qua USART1.

Nguồn dữ liệu được chọn lúc compile bằng `SENSOR_DATA_SOURCE`; cấu hình mặc định hiện tại là `SENSOR_SOURCE_PLC_RS485`.

## MQTT topics

Mẫu topic là `<FARM>/<SERIAL_NUMBER>/<suffix>`.

| Suffix | Hướng | QoS | Retain | Trạng thái triển khai |
|---|---|---:|---:|---|
| `status` | Device -> Server | 0 | 1 | Đang dùng, gồm online và Last Will offline |
| `telemetry` | Device -> Server | 0 | 0 | Đang dùng định kỳ |
| `config/state` | Device -> Server | 0 | 1 | Publish sau khi subscribe |
| `config/response` | Device -> Server | 0 | 0 | Có API, chưa được gọi trong luồng hiện tại |
| `command/response` | Device -> Server | 0 | 0 | ACK lệnh OTA và phản hồi command |
| `config/set` | Server -> Device | 0 | - | Đã subscribe, chưa có parser hoàn chỉnh |
| `config/get` | Server -> Device | 0 | - | Đã subscribe, chưa có handler hoàn chỉnh |
| `command/request` | Server -> Device | 0 | - | Lệnh điều khiển và trigger OTA |

## Telemetry payload

| Field | Type | Nguồn và ý nghĩa |
|---|---|---|
| `device_id` | string | `SERIAL_NUMBER` |
| `firmware_version` | string | `VERSION_WBEE` đã compile |
| `ph1` | number/null | Register `0x0000`, raw / 10 |
| `do` | number/null | Register `0x0002` |
| `ph2` | number/null | Register `0x0001`, raw / 10 |
| `turbidity` | number/null | Register `0x0020`, NTU |
| `ozone` | number/null | Register `0x0003` |
| `pressure_o2` | number/null | Register `0x0004` |
| `temp_data_1..3` | number/null | Register `0x0021..0x0023`, dự phòng |
| `input_x` | integer/null | Chỉ số bit 1 đầu tiên của register `0x0005` |
| `output_1` | integer/null | Chỉ số bit 1 đầu tiên của register `0x0006` |
| `output_2` | integer/null | Chỉ số bit 1 đầu tiên của register `0x0007` |
| `error_code` | integer/null | Chỉ số bit 1 đầu tiên của register `0x0009` |
| `rssi` | integer | dBm quy đổi từ `AT+CSQ` |

Ví dụ:

```json
{"device_id":"wb000002","firmware_version":"3.1","ph1":7.0,"do":11.0,"ph2":4.5,"turbidity":20.0,"ozone":1000.0,"pressure_o2":77.0,"temp_data_1":1.0,"temp_data_2":2.0,"temp_data_3":3.0,"input_x":2,"output_1":1,"output_2":4,"error_code":1,"rssi":-51}
```

Khi PLC timeout, các trường lấy từ PLC vẫn tồn tại trong JSON nhưng nhận giá trị `null`.

## Status payload

Online:

```json
{"device_id":"wb000002","status":"online","firmware_version":"3.1","rssi":-51}
```

Last Will/offline:

```json
{"device_id":"wb000002","status":"offline"}
```

## Config state payload

```json
{"device_id":"wb000002","telemetry_interval_s":15,"sensor_sample_interval_s":10}
```

Hai chu kỳ hiện là hằng compile-time; `config/set` chưa cập nhật chúng ở runtime.

## PLC frame contract

Frame hỗ trợ là function `0x10`, slave `0x02`:

```text
:<address><function><start><quantity><byte_count><data><lrc>\r\n
```

Frame chỉ được nhận khi:

1. Mọi ký tự payload là cặp hex hợp lệ.
2. Address và function khớp cấu hình.
3. Quantity không vượt `PLC_RS485_MAX_REGISTERS`.
4. Byte count bằng `quantity * 2`.
5. Tổng tất cả byte, kể cả LRC, bằng 0 modulo 256.

STM32 chỉ nhận thụ động và không gửi Modbus response về PLC.

## OTA command

Publish đến `mobi/water/<SERIAL_NUMBER>/command/request`. Payload chỉ cần chứa `ota_check` hoặc `ota_update`; code hiện tại cho hai lệnh cùng hành vi kiểm tra và tải nếu có phiên bản mới.

Ví dụ hợp lệ:

```json
{"command":"ota_update"}
```

Payload có `request_id` được khuyến nghị để server đối chiếu ACK:

```json
{"request_id":"req-123","command":"ota_update"}
```

Thiết bị phản hồi trước khi download firmware:

```json
{"request_id":"req-123","command":"ota_update","result":"received"}
```

Nếu không có `request_id`, ACK vẫn được gửi với `request_id` là chuỗi rỗng.

## Set serial number command

Request được gửi đến topic `command/request` của serial hiện tại:

```json
{"request_id":"req-serial-001","command":"set_serial_number","serial_number":"wb000003"}
```

ACK trước khi ghi Flash:

```json
{"request_id":"req-serial-001","command":"set_serial_number","result":"received"}
```

Kết quả sau khi ghi và verify Flash:

```json
{"request_id":"req-serial-001","command":"set_serial_number","result":"success"}
```

Cả hai response được publish trên `command/response` của serial cũ. Sau đó MCU
reset và dùng serial mới cho `device_id`, MQTT client ID và tất cả topic.

Validation: dài 3..23 ký tự, chỉ gồm `a-z`, `0-9`, `-`, `_`. Request sai nhận
`result=rejected`, `error_code=INVALID_SERIAL_NUMBER`. Lỗi ghi Flash nhận
`result=failed`, `error_code=FLASH_WRITE_FAILED`.

## OTA manifest

| Field | Quy tắc |
|---|---|
| `version` | Phải lớn hơn `VERSION_WBEE` để update |
| `device` | Phải bằng `wbee-stm32f103ret6` |
| `app_addr` | Phải bằng `0x08008000` |
| `size` | Kích thước BIN, từ 8 đến 229376 byte và không vượt staging |
| `crc32` | CRC32 toàn bộ BIN, khác 0 |
| `bin_url` | Raw HTTPS URL tải được bằng SIMCOM |

## Quy tắc xử lý

- Frame hợp lệ chỉ cập nhật các register có mặt; dữ liệu của frame trước vẫn được giữ.
- Timeout chỉ reset sau khi decode, LRC và header đều hợp lệ.
- MQTT telemetry gọi lại `AT+CSQ`; JSON LCD sử dụng RSSI cache gần nhất.
- Sau nhiều lần publish thất bại, state machine ngắt MQTT và quay lại kiểm tra mạng.
- OTA chỉ bắt đầu khi modem đang ở trạng thái `Subscribed`.
- Serial runtime trong Flash được ưu tiên hơn `SERIAL_NUMBER` compile-time.

## Giả định

- Một frame PLC hoàn chỉnh nằm trong một callback UART Receive-to-Idle.
- Server hiểu các trường bitfield là chỉ số bit, không phải giá trị raw register.
- Firmware không kiểm tra miền hợp lệ vật lý của từng cảm biến.
