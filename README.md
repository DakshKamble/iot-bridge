# XIAO ESP32C3 — MPU6050 / MPU6500

Simple MPU6050/MPU6500 firmware. Prints acceleration (m/s²), angular velocity (rad/s), and chip temperature (°C) to Serial every 500 ms. Retries if the sensor is missing or disconnected. Acceleration includes gravity; temperature is the chip temperature.

## Wiring

| XIAO ESP32C3 | MPU6050 breakout |
| --- | --- |
| 3V3 | VCC (use a breakout supporting 3.3 V) |
| GND | GND |
| D4 / GPIO6 | SDA |
| D5 / GPIO7 | SCL |
| GND | AD0 (address 0x68) |

Leave INT, XDA and XCL disconnected. I2C pull-ups must go to 3.3 V; add 4.7kΩ from SDA/SCL to 3V3 if the breakout lacks them.

## Settings and running

All settings are at the top of `src/main.cpp`: pins, preferred/alternate address, serial baud, sample interval, retry interval, ranges and filter bandwidth. The code tries both `0x68` and `0x69`. If changing serial baud, also update `monitor_speed` in `platformio.ini`.

On startup and after a detection failure, it prints SDA/SCL idle levels, scans the I2C bus at 100 kHz and reads WHO_AM_I from responding MPU addresses. Both lines should idle high. No responding addresses points to power, wiring, pull-ups or hardware; an address response with an unexpected chip identity means this firmware does not support that chip. WHO_AM_I `0x68` selects MPU6050; `0x70` selects MPU6500. The firmware uses direct I2C register access, configures the appropriate filters, verifies range settings, checks complete 14-byte sensor reads, and applies the corresponding temperature formula. It needs only Arduino and Wire, with no external sensor library. Unknown identities remain rejected.

```sh
pio run
pio run -t upload
pio device monitor
```

No Wi-Fi, MQTT, JSON, DS18B20 or strain-gauge code is included.

Your reported WHO_AM_I of `0x70` matches the MPU6500 register map, so the earlier MPU6050 library rejected it. See [TDK register map](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6500-Register-Map2.pdf). Successful compilation does not verify hardware readings; upload and inspect the serial output.
