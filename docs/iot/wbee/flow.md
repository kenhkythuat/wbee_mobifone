# WBEE Runtime Flow

## Khởi động và MQTT

```mermaid
sequenceDiagram
    participant MCU as STM32
    participant SIM as SIMCOM A7680
    participant Broker as MQTT Broker
    participant PLC
    participant LCD

    MCU->>MCU: Init GPIO, UART, TIM6, IWDG, ADC
    MCU->>PLC: Arm USART2 Receive-to-Idle
    MCU->>LCD: Gửi telemetry ban đầu + CRLF
    MCU->>SIM: Bật modem và kiểm tra SIM/network
    MCU->>SIM: Start MQTT, acquire client, cấu hình Last Will
    MCU->>SIM: Connect broker
    MCU->>Broker: Subscribe config/set, config/get, command/request
    MCU->>Broker: Publish online, config/state, telemetry
```

State machine chính:

```text
Off -> On -> InternetReady -> MqttReady -> Subscribed
```

## PLC đến MQTT và LCD

```mermaid
flowchart TD
    RX[USART2 Receive-to-Idle] --> HEX{ASCII hex hợp lệ?}
    HEX -- Không --> DLOG[Log decode fail]
    HEX -- Có --> LRC{LRC hợp lệ?}
    LRC -- Không --> LLOG[Log LRC error]
    LRC -- Có --> HEADER{Address/function/length đúng?}
    HEADER -- Không --> ULOG[Log unsupported frame]
    HEADER -- Có --> MAP[Map register vào snapshot]
    MAP --> RESET[Reset timeout PLC]
    RESET --> JSON[Tạo telemetry JSON]
    JSON --> MQTT[Publish theo chu kỳ]
    JSON --> LCD[Gửi UART5 + CRLF]
```

PLC chủ động gửi. STM32 không polling PLC. Các frame start address khác nhau cùng cập nhật vào một snapshot.

## Công việc định kỳ

TIM6 tạo tick 1 giây:

1. Tăng bộ đếm publish MQTT.
2. Tăng bộ đếm cập nhật LCD.
3. Gọi `plc_rs485_tick_1s()`.
4. Khi đủ `INTERVAL_PUPLISH_DATA`, đặt cờ publish.
5. Vòng lặp chính publish telemetry; thao tác AT command không chạy trong ISR.
6. Khi counter LCD lớn hơn 5, gửi JSON qua UART5.

## Timeout PLC

```mermaid
flowchart TD
    TICK[Tick 1 giây] --> LIMIT{Counter > timeout?}
    LIMIT -- Không --> KEEP[Giữ snapshot gần nhất]
    LIMIT -- Có --> CLEAR[Xóa toàn bộ cờ has_*]
    CLEAR --> NULL[Telemetry dùng null]
    FRAME[Frame hợp lệ mới] --> UPDATE[Cập nhật snapshot]
    UPDATE --> RESET[Counter=0 và online=true]
```

## OTA

```mermaid
sequenceDiagram
    participant Server
    participant App as STM32 Application
    participant GitHub
    participant Flash
    participant Boot as Bootloader

    Server->>App: MQTT payload chứa ota_update
    App->>Server: command/response result=received
    App->>GitHub: GET manifest.json
    App->>App: Validate device, version, address, size
    App->>GitHub: GET BIN theo từng chunk
    App->>Flash: Ghi staging tại 0x08040000
    App->>App: Kiểm tra stream CRC và Flash CRC
    App->>Flash: Ghi pending metadata tại 0x0807F000
    App->>App: NVIC_SystemReset
    Boot->>Flash: Kiểm tra metadata và staging CRC
    Boot->>Flash: Copy sang application 0x08008000
    Boot->>Boot: Kiểm tra CRC và vector table
    Boot->>Flash: Xóa pending metadata
    Boot->>App: Set VTOR/MSP và jump
```

Nếu manifest không mới hơn, application tiếp tục chạy. Bootloader hiện không có rollback image thứ hai.

## Đổi serial number

```mermaid
sequenceDiagram
    participant Server
    participant Old as Device topic serial cũ
    participant Flash
    participant New as Device topic serial mới

    Server->>Old: set_serial_number + request_id
    Old->>Old: Validate serial mới
    Old->>Server: result=received
    Old->>Flash: Erase/program/verify config page
    Flash-->>Old: Save OK
    Old->>Server: result=success
    Old->>Old: NVIC_SystemReset
    New->>Server: status online + telemetry
```

Nếu ACK `received` không gửi được sau ba lần, serial không được ghi. Khi ghi
thành công, serial cũ vẫn được dùng để gửi `success`; serial mới chỉ có hiệu lực
sau reboot.
