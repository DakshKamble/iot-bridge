# XIAO ESP32C3 — MPU6050 / MPU6500 + DS18B20

Prints MPU6050/MPU6500 acceleration (m/s²) and angular velocity (rad/s) every 500 ms, DS18B20 probe temperature (°C) about every two seconds, and Y3 strain voltage every 500 ms. Retries missing digital sensors. Acceleration includes gravity. The DS18B20 is the only reported temperature; IMU chip temperature is ignored.

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

On startup and after a detection failure, it prints SDA/SCL idle levels, scans the I2C bus at 100 kHz and reads WHO_AM_I from responding MPU addresses. Both lines should idle high. No responding addresses points to power, wiring, pull-ups or hardware; an address response with an unexpected chip identity means this firmware does not support that chip. WHO_AM_I `0x68` selects MPU6050; `0x70` selects MPU6500. The firmware uses direct I2C register access, configures the appropriate filters, verifies range settings, checks complete 14-byte sensor reads and skips the chip-temperature bytes. The IMU needs only Arduino and Wire; the DS18B20 uses OneWire and DallasTemperature. Unknown identities remain rejected.

```sh
pio run
pio run -t upload
pio device monitor
```

Wi-Fi and MQTT TLS publish JSON telemetry; see the MQTT section below.

Your reported WHO_AM_I of `0x70` matches the MPU6500 register map, so the earlier MPU6050 library rejected it. See [TDK register map](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6500-Register-Map2.pdf). Successful compilation does not verify hardware readings; upload and inspect the serial output.

## DS18B20 probe on D7

Remove the previous divider: no resistor from D7 to GND and no 20k series resistor between data and D7. A DS18B20 uses bidirectional digital 1-Wire communication, not an analog output.

```text
XIAO USB 5V -------------------- DS18B20 VDD
XIAO GND ----------------------- DS18B20 GND
XIAO D7 / GPIO20 ------+--------- DS18B20 DQ (data)
                      |
                    4.7k ohm
                      |
XIAO 3V3 -------------+
```

For a three-wire DS18B20 probe, VDD may use 5V while DQ is pulled up to 3.3V. Never pull DQ up to 5V or connect an adapter's built-in 5V data pull-up directly to the ESP32. A bare DS18B20 also supports 3.3V power. Identify probe wires from its supplier's pinout; colors vary. Use external power, not parasite power.

A 4.7k pull-up is recommended. If you only have 10k resistors, two 10k resistors in parallel give 5k, close to 4.7k; connect that pair between D7 and 3V3. Do not reuse the divider topology.

Settings DS_PIN, DS_INTERVAL_MS and DS_CONVERSION_MS are at the top of src/main.cpp. The firmware waits for a complete 12-bit conversion, prints a fresh probe reading about every two seconds, reports read/CRC failures and retries missing probes. Probe service runs even if the IMU is absent. Only DS18B20 probe temperature is reported; IMU chip temperature is ignored.

Reference: [DS18B20 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf).

## Y3 strain amplifier: direct input on D1

Current wiring assumes the user powers the Y3 at **3.3V** and has no divider. OUT connects to **D1 / GPIO3**, an ADC1 input. **D6 / GPIO21 cannot measure analog voltage.** GND must be shared. Measure OUT with a multimeter before connecting: ESP32 GPIOs must not receive more than 3.3V. The board is specified for 5V, so correct operation/accuracy at 3.3V is not established. Never change its supply to 5V while OUT is connected directly.

The ESP32-C3 ADC's measurable range at the selected attenuation is narrower than its GPIO voltage limit: a signal approaching 3.3V may saturate. The serial rail message flags possible clipping; it does not establish whether a sensor is connected. Restore a suitable divider if the output exceeds the ADC range, and update the voltage scaling accordingly.

Settings at the top of src/main.cpp: STRAIN_PIN, STRAIN_INTERVAL_MS, STRAIN_SAMPLES, STRAIN_ZERO_MV and MICROSTRAIN_PER_MV. The serial output shows raw 12-bit counts and output millivolts every 500ms. Physical microstrain remains uncalibrated until a measured slope and offset are supplied. Strain sampling continues even when the IMU is missing.

## MQTT over verified TLS

All code and settings remain in `src/main.cpp`. Enter WIFI_SSID and WIFI_PASSWORD at the top (2.4GHz network). MQTT login uses the credentials supplied in this chat. These credentials are in the source file; remove them before sharing it.

- Host: `q20a2f28.ala.asia-southeast1.emqxsl.com`
- Native MQTT TLS port: `8883` (ESP32 connection)
- WebSocket TLS port: `8084` (browser dashboard, path `/mqtt`)
- Embedded CA: EMQX Serverless DigiCert Global Root CA, expiry 2031-11-10
- Telemetry: `iot-bridge/xiao-<mac>/telemetry` (retained JSON, every 2000ms by default)
- Status: `iot-bridge/xiao-<mac>/status` (retained online, with offline last will)

The device ID and full topic are printed on boot. Subscribe to `iot-bridge/+/telemetry` and `iot-bridge/+/status` to see all devices. Network access must allow DNS, NTP and MQTT port 8883. The firmware verifies the broker certificate and hostname, waits for UTC clock synchronization, and retries dropped Wi-Fi/MQTT connections. It never uses insecure TLS.

Payload sections are `imu` (model, validity, X/Y/Z acceleration in m/s² and gyro in rad/s), `ds18b20` (validity and probe temperature in °C), and `strain` (sampled flag, ADC counts, direct output mV, rail flag and optional calibrated microstrain). **DS18B20 is the only temperature source in MQTT.** Digital sensor readings are marked invalid when a read fails; analog sampling cannot establish whether the Y3 is connected. Sequence, UTC Unix timestamp, uptime, RSSI and sample ages are included. Physical strain is null until calibrated.

Serial output prints the exact JSON, topic and byte count, followed by `TX OK`, `TX FAILED`, or `SKIPPED` with a reason. QoS 0 `TX OK` means the client wrote the message, with no broker acknowledgement; verify delivery with a subscriber. There is no offline queue. Retained samples can be old, so a dashboard should check timestamps and status. Connection attempts may pause sampling for several seconds.

## Local MQTT dashboard

Run from the project folder:

```sh
python3 -m http.server 8765 --bind 127.0.0.1 --directory dashboard
```

Open [the dashboard](http://127.0.0.1:8765). Serve it over HTTP instead of opening `index.html` directly because it uses JavaScript modules. No build or package installation is needed; MQTT.js is bundled locally.

The dashboard automatically connects with the defaults in `dashboard/config.js` using WSS port 8084 and `/mqtt`. Use Disconnect to edit the connection settings, then Connect to retry. It subscribes to both firmware topics and discovers device IDs automatically; select a device when multiple bridges publish. It only subscribes, and never publishes commands or test readings.

Readings match firmware schema version 1: external DS18B20 temperature in °C, acceleration XYZ in m/s², gyroscope XYZ in rad/s, Y3 output in mV, ADC counts and calibrated microstrain in µε. Invalid sensors display a dash; microstrain stays uncalibrated until firmware calibration is supplied. Analog samples cannot prove the Y3 is connected. The dashboard reports ADC rail warnings, sequence, RSSI, uptime, UTC sample time in your browser's local timezone, and the received JSON.

The broker connection and device freshness are tracked separately. Telemetry or sensor samples older than `staleAfterMs` (15 seconds by default), offline devices, and broker disconnects mark the readings stale. Retained telemetry without a timestamp has unknown age and is marked stale. Historical values remain visible with a stale label. Connections retry automatically. Credentials are present in `config.js` and visible to anyone who can access the served files; they are not saved to browser storage. Keep this dashboard local or use appropriately scoped broker credentials before sharing it.

Run the schema, freshness and dashboard lifecycle checks with Node.js:

```sh
node --test dashboard/telemetry.test.mjs
```
