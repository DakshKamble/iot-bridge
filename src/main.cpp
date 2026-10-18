#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ================= CHANGE SETTINGS HERE =================
// Wi-Fi must be 2.4 GHz. Enter your network name and password here.
const char WIFI_SSID[] = "DakshNET 2.4";
const char WIFI_PASSWORD[] = "9650349609";
const char MQTT_HOST[] = "q20a2f28.ala.asia-southeast1.emqxsl.com";
const uint16_t MQTT_PORT = 8883;
const char MQTT_USERNAME[] = "daksh";
const char MQTT_PASSWORD[] = "daksh";
const char MQTT_TOPIC_PREFIX[] = "iot-bridge";
const uint32_t MQTT_PUBLISH_MS = 2000;
const uint32_t NETWORK_RETRY_MS = 10000;
const uint16_t MQTT_KEEPALIVE_SECONDS = 30;
const uint16_t MQTT_SOCKET_TIMEOUT_SECONDS = 5;
const uint32_t TLS_HANDSHAKE_SECONDS = 5;
const uint32_t TLS_TIMEOUT_MS = 5000;
const char NTP_SERVER1[] = "pool.ntp.org";
const char NTP_SERVER2[] = "time.google.com";
const int64_t MINIMUM_UNIX_TIME = 1767225600; // 2026-01-01 UTC
const size_t JSON_BUFFER_SIZE = 2048;
const size_t PAYLOAD_BUFFER_SIZE = 1536;
const uint16_t MQTT_BUFFER_SIZE = 2048;

const uint8_t SDA_PIN = 6;  // XIAO D4
const uint8_t SCL_PIN = 7;  // XIAO D5
const uint8_t MPU_ADDRESS = 0x68;
const uint8_t MPU_ALTERNATE_ADDRESS = 0x69;
const uint32_t I2C_CLOCK_HZ = 100000;
const uint32_t SERIAL_BAUD = 115200;
const uint32_t READ_INTERVAL_MS = 500;
const uint32_t RETRY_INTERVAL_MS = 3000;
const uint16_t I2C_TIMEOUT_MS = 50;
const uint32_t STARTUP_DELAY_MS = 500;
// Range codes: accelerometer 0=2g, 1=4g, 2=8g, 3=16g.
// Gyroscope 0=250, 1=500, 2=1000, 3=2000 degrees/second.
const uint8_t ACCEL_RANGE = 2;
const uint8_t GYRO_RANGE = 1;
const uint8_t FILTER_SETTING = 4; // About 20 Hz low-pass; use 1..6.
const uint8_t SAMPLE_RATE_DIVIDER = 9; // Internal sample rate: 1000/(1+divider) Hz.
// DS18B20: DQ directly to D7; 4.7k pull-up from D7 to 3V3. NO divider.
const uint8_t DS_PIN = 20; // XIAO D7
const uint32_t DS_INTERVAL_MS = 2000;
const uint32_t DS_CONVERSION_MS = 800; // 12-bit conversion needs up to 750ms.
// Y3 currently powered from 3.3V: OUT directly to D1, no divider.
// D6/GPIO21 has NO ADC. Verify OUT voltage before connecting.
const uint8_t STRAIN_PIN = 3; // XIAO D1 / ADC1
const uint32_t STRAIN_INTERVAL_MS = 500;
const uint16_t STRAIN_SAMPLES = 16;
// Leave slope zero until calibrated using known strain and measured output mV.
const float STRAIN_ZERO_MV = 0.0f;
const float MICROSTRAIN_PER_MV = 0.0f;

// ================= MQTT TLS CERTIFICATE =================
// EMQX Serverless DigiCert Global Root CA, expires 2031-11-10.
// https://github.com/emqx/MQTT-Client-Examples/blob/master/mqtt-client-ESP32/emqxsl-ca.crt
const char MQTT_ROOT_CA[] = R"PEM(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)PEM";

// ================= REGISTER DEFINITIONS =================
// TDK MPU6050 / MPU6500 register maps. No external sensor library needed.
const uint8_t REG_SAMPLE_RATE = 0x19;
const uint8_t REG_CONFIG = 0x1A;
const uint8_t REG_GYRO_CONFIG = 0x1B;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_ACCEL_CONFIG2 = 0x1D; // MPU6500 only
const uint8_t REG_SENSOR_DATA = 0x3B;
const uint8_t REG_POWER1 = 0x6B;
const uint8_t REG_POWER2 = 0x6C;
const uint8_t REG_IDENTITY = 0x75;

bool sensorReady = false;
uint8_t sensorAddress = MPU_ADDRESS;
uint8_t chipIdentity = 0;
uint32_t lastReadMs = 0;
uint32_t lastRetryMs = 0;

OneWire oneWire(DS_PIN);
DallasTemperature probe(&oneWire);
DeviceAddress probeAddress;
bool probeReady = false;
bool probeConverting = false;
uint32_t probeStartedMs = 0;
uint32_t probeRetryMs = 0;
uint32_t lastStrainMs = 0;

// Latest sensor samples for MQTT. Invalid/unavailable data is not published as zero.
float acceleration[3] = {}, angularVelocity[3] = {};
bool imuValid = false, probeValid = false, strainSampled = false;
float probeTemperatureC = 0, strainOutputMv = 0;
uint32_t strainRaw = 0, imuSampleMs = 0, probeSampleMs = 0;
WiFiClientSecure tls;
PubSubClient mqtt(tls);
String deviceId, telemetryTopic, statusTopic;
bool wifiConfigured = false;
uint32_t lastWifiAttempt = 0, lastMqttAttempt = 0, lastPublishMs = 0;
uint32_t messageSequence = 0;

void startNetwork() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mac.toLowerCase();
  deviceId = "xiao-" + mac;
  telemetryTopic = String(MQTT_TOPIC_PREFIX) + "/" + deviceId + "/telemetry";
  statusTopic = String(MQTT_TOPIC_PREFIX) + "/" + deviceId + "/status";
  tls.setCACert(MQTT_ROOT_CA);
  tls.setHandshakeTimeout(TLS_HANDSHAKE_SECONDS);
  tls.setTimeout(TLS_TIMEOUT_MS);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_SECONDS);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_SECONDS);
  wifiConfigured = strlen(WIFI_SSID) > 0 && String(WIFI_SSID) != "YOUR_WIFI_SSID";
  Serial.printf("[MQTT] Device: %s | Topic: %s\n", deviceId.c_str(), telemetryTopic.c_str());
  if (!wifiConfigured) {
    Serial.println("[WiFi] Enter credentials at top of main.cpp. Serial sensors remain active.");
    return;
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt = millis();
  configTime(0, 0, NTP_SERVER1, NTP_SERVER2);
  Serial.println("[WiFi] Connecting; MQTT TLS waits for NTP time sync");
}

void serviceNetwork() {
  if (!wifiConfigured) return;
  static bool wifiWasConnected = false, timeReported = false;
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiWasConnected) Serial.println("[WiFi] Disconnected");
    wifiWasConnected = false;
    if (mqtt.connected()) mqtt.disconnect();
    if (now - lastWifiAttempt >= NETWORK_RETRY_MS) {
      lastWifiAttempt = now;
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      Serial.println("[WiFi] Retrying...");
    }
    return;
  }
  if (!wifiWasConnected) {
    Serial.printf("[WiFi] Connected | IP: %s | RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    wifiWasConnected = true;
  }
  if (time(nullptr) < MINIMUM_UNIX_TIME) return;
  if (!timeReported) {
    Serial.println("[TIME] UTC synchronized; TLS ready");
    timeReported = true;
  }
  if (!mqtt.connected() && now - lastMqttAttempt >= NETWORK_RETRY_MS) {
    lastMqttAttempt = now;
    Serial.println("[MQTT] Connecting with verified TLS...");
    if (mqtt.connect(deviceId.c_str(), MQTT_USERNAME, MQTT_PASSWORD,
                     statusTopic.c_str(), 1, true, "offline")) {
      mqtt.publish(statusTopic.c_str(), "online", true);
      Serial.println("[MQTT] Connected");
    } else Serial.printf("[MQTT] Connection failed | state=%d\n", mqtt.state());
  }
  mqtt.loop();
}

void publishTelemetry() {
  if (millis() - lastPublishMs < MQTT_PUBLISH_MS) return;
  lastPublishMs = millis();
  StaticJsonDocument<JSON_BUFFER_SIZE> doc;
  doc["schema_version"] = 1;
  doc["device_id"] = deviceId;
  doc["sequence"] = messageSequence++;
  doc["uptime_ms"] = millis();
  if (time(nullptr) >= MINIMUM_UNIX_TIME) doc["timestamp"] = int64_t(time(nullptr));
  else doc["timestamp"] = nullptr;
  if (WiFi.status() == WL_CONNECTED) doc["wifi_rssi_dbm"] = WiFi.RSSI();
  else doc["wifi_rssi_dbm"] = nullptr;

  JsonObject imu = doc.createNestedObject("imu");
  imu["valid"] = imuValid;
  if (chipIdentity) imu["model"] = chipIdentity == 0x70 ? "MPU6500" : "MPU6050";
  if (imuValid) {
    JsonArray a = imu.createNestedArray("acceleration_m_s2");
    JsonArray g = imu.createNestedArray("gyro_rad_s");
    for (uint8_t i = 0; i < 3; ++i) { a.add(acceleration[i]); g.add(angularVelocity[i]); }
    imu["sample_age_ms"] = millis() - imuSampleMs;
  }
  // DS18B20 is the ONLY temperature included; IMU chip temperature is ignored.
  JsonObject ds = doc.createNestedObject("ds18b20");
  ds["valid"] = probeValid;
  if (probeValid) {
    ds["temperature_c"] = probeTemperatureC;
    ds["sample_age_ms"] = millis() - probeSampleMs;
  } else ds["temperature_c"] = nullptr;

  JsonObject strain = doc.createNestedObject("strain");
  strain["sampled"] = strainSampled; // Analog sampling cannot detect an unplugged module.
  strain["calibrated"] = MICROSTRAIN_PER_MV != 0;
  strain["microstrain"] = nullptr;
  if (strainSampled) {
    strain["adc_raw"] = strainRaw;
    strain["output_mv"] = strainOutputMv;
    strain["adc_near_rail"] = strainRaw <= 5 || strainRaw >= 4090;
    strain["sample_age_ms"] = millis() - lastStrainMs;
    if (MICROSTRAIN_PER_MV != 0)
      strain["microstrain"] = (strainOutputMv - STRAIN_ZERO_MV) * MICROSTRAIN_PER_MV;
  }
  char payload[PAYLOAD_BUFFER_SIZE];
  if (doc.overflowed() || measureJson(doc) >= sizeof(payload)) {
    Serial.println("[JSON] Buffer too small; skipped");
    return;
  }
  const size_t length = serializeJson(doc, payload, sizeof(payload));
  Serial.printf("[JSON] Topic: %s | %u bytes\n", telemetryTopic.c_str(), unsigned(length));
  Serial.println(payload);
  if (!mqtt.connected()) {
    const char *reason = !wifiConfigured ? "Wi-Fi credentials missing" :
        WiFi.status() != WL_CONNECTED ? "Wi-Fi disconnected" :
        time(nullptr) < MINIMUM_UNIX_TIME ? "waiting for NTP" : "MQTT disconnected";
    Serial.printf("[MQTT] SKIPPED: %s\n", reason);
  } else if (mqtt.publish(telemetryTopic.c_str(), payload, true)) {
    Serial.println("[MQTT] TX OK | retained QoS 0; no broker acknowledgement");
  } else Serial.printf("[MQTT] TX FAILED | state=%d\n", mqtt.state());
}

void serviceStrain() {
  if (millis() - lastStrainMs < STRAIN_INTERVAL_MS) return;
  lastStrainMs = millis();
  uint32_t rawTotal = 0, mvTotal = 0;
  for (uint16_t i = 0; i < STRAIN_SAMPLES; ++i) {
    rawTotal += analogRead(STRAIN_PIN);
    mvTotal += analogReadMilliVolts(STRAIN_PIN);
  }
  const uint32_t raw = rawTotal / STRAIN_SAMPLES;
  const float outputMv = mvTotal / float(STRAIN_SAMPLES);
  strainRaw = raw;
  strainOutputMv = outputMv;
  strainSampled = true;
  Serial.printf("[Y3] GPIO%u | ADC: %lu / 4095 | OUT: %.1f mV",
                STRAIN_PIN, static_cast<unsigned long>(raw), outputMv);
  if (MICROSTRAIN_PER_MV != 0) {
    Serial.printf(" | Strain: %.2f microstrain\n",
                  (outputMv - STRAIN_ZERO_MV) * MICROSTRAIN_PER_MV);
  } else Serial.println(" | Uncalibrated: voltage only");
  if (raw <= 5 || raw >= 4090)
    Serial.println("[Y3] ADC near a rail; reading may be clipped, or input may be disconnected");
}

void connectProbe() {
  probeRetryMs = millis();
  probe.begin();
  probeReady = false;
  probeValid = false;
  probeConverting = false;
  // Find a DS18B20 (family 0x28), rather than accepting any 1-Wire device.
  for (uint8_t i = 0; i < probe.getDeviceCount(); ++i) {
    if (probe.getAddress(probeAddress, i) && probeAddress[0] == 0x28) {
      probeReady = true;
      break;
    }
  }
  if (!probeReady) {
    Serial.printf("[DS18B20] Not detected on GPIO%u. Check DQ and pull-up to 3V3.\n", DS_PIN);
    return;
  }
  probe.setResolution(probeAddress, 12);
  probe.setWaitForConversion(false);
  probe.requestTemperatures();
  probeStartedMs = millis();
  probeConverting = true;
  Serial.println("[DS18B20] Detected; starting temperature conversion");
}

void serviceProbe() {
  const uint32_t now = millis();
  if (!probeReady) {
    if (now - probeRetryMs >= RETRY_INTERVAL_MS) connectProbe();
    return;
  }
  if (probeConverting && now - probeStartedMs >= DS_CONVERSION_MS) {
    const float temperature = probe.getTempC(probeAddress); // Includes scratchpad CRC check.
    if (temperature == DEVICE_DISCONNECTED_C || temperature < -55 || temperature > 125) {
      Serial.println("[DS18B20] Read failed/disconnected. Retrying...");
      probeReady = false;
      probeValid = false;
      probeRetryMs = now;
    } else {
      probeTemperatureC = temperature;
      probeSampleMs = now;
      probeValid = true;
      Serial.printf("[DS18B20] Probe temperature: %.2f C\n", temperature);
    }
    probeConverting = false;
  }
  if (probeReady && !probeConverting && now - probeStartedMs >= DS_INTERVAL_MS) {
    probe.requestTemperatures();
    probeStartedMs = millis();
    probeConverting = true;
  }
}

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(sensorAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegisters(uint8_t reg, uint8_t *data, uint8_t count) {
  Wire.beginTransmission(sensorAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(sensorAddress, count) != count) return false;
  for (uint8_t i = 0; i < count; ++i) data[i] = Wire.read();
  return true;
}

bool checkSensor(uint8_t address) {
  sensorAddress = address;
  uint8_t identity = 0;
  if (!readRegisters(REG_IDENTITY, &identity, 1)) return false;
  Serial.printf("[IMU] Address=0x%02X WHO_AM_I=0x%02X\n", address, identity);
  if (identity != 0x68 && identity != 0x70) {
    Serial.println("[IMU] Unsupported identity; expected MPU6050=0x68 or MPU6500=0x70");
    return false;
  }
  chipIdentity = identity;

  if (!writeRegister(REG_POWER1, 0x80)) return false; // Reset.
  delay(100);
  if (!writeRegister(REG_POWER1, 0x01)) return false; // Wake, use PLL clock.
  delay(100);
  bool configured = writeRegister(REG_POWER2, 0x00) && // Enable all axes.
                    writeRegister(REG_CONFIG, FILTER_SETTING) &&
                    writeRegister(REG_GYRO_CONFIG, GYRO_RANGE << 3) &&
                    writeRegister(REG_ACCEL_CONFIG, ACCEL_RANGE << 3) &&
                    writeRegister(REG_SAMPLE_RATE, SAMPLE_RATE_DIVIDER);
  if (configured && chipIdentity == 0x70)
    configured = writeRegister(REG_ACCEL_CONFIG2, FILTER_SETTING);
  if (!configured) {
    Serial.println("[IMU] Configuration write failed");
    return false;
  }
  // Confirm ranges before using their scale factors.
  uint8_t gyroConfig = 0, accelConfig = 0;
  if (!readRegisters(REG_GYRO_CONFIG, &gyroConfig, 1) ||
      !readRegisters(REG_ACCEL_CONFIG, &accelConfig, 1) ||
      ((gyroConfig >> 3) & 3) != GYRO_RANGE ||
      ((accelConfig >> 3) & 3) != ACCEL_RANGE) {
    Serial.println("[IMU] Range configuration verification failed");
    return false;
  }
  delay(100); // Allow the filters and sensor outputs to settle.
  return true;
}

void connectSensor() {
  lastRetryMs = millis();
  sensorReady = false;
  imuValid = false;
  Serial.printf("\n[I2C] Idle levels: SDA=%d SCL=%d (both should be 1)\n",
                digitalRead(SDA_PIN), digitalRead(SCL_PIN));
  if (digitalRead(SDA_PIN) == LOW || digitalRead(SCL_PIN) == LOW) {
    Serial.println("[I2C] Bus held LOW. Check power, wiring and pull-ups to 3.3V.");
    return;
  }
  Serial.println("[I2C] Scanning addresses...");
  uint8_t found = 0;
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    uint8_t result = Wire.endTransmission();
    if (result == 0) {
      Serial.printf("[I2C] Device found at 0x%02X\n", address);
      ++found;
    } else if (result != 2) {
      Serial.printf("[I2C] Bus error %u; stopping scan\n", result);
      return;
    }
  }
  if (found == 0) Serial.println("[I2C] No devices responded");
  sensorReady = checkSensor(MPU_ADDRESS) || checkSensor(MPU_ALTERNATE_ADDRESS);
  if (sensorReady) {
    Serial.printf("[IMU] %s ready at 0x%02X\n",
                  chipIdentity == 0x70 ? "MPU6500" : "MPU6050", sensorAddress);
  } else Serial.println("[IMU] No supported sensor initialized. Retrying...");
}

int16_t signedValue(const uint8_t *data) {
  const uint16_t value = (uint16_t(data[0]) << 8) | data[1];
  return value < 0x8000 ? int16_t(value) : int16_t(int32_t(value) - 65536);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(STARTUP_DELAY_MS);
  Serial.println("\nMPU6050 / MPU6500 + DS18B20 + Y3 | XIAO ESP32C3");
  if (STRAIN_SAMPLES == 0 || STRAIN_SAMPLES > 1024 ||
      (STRAIN_PIN != 2 && STRAIN_PIN != 3 && STRAIN_PIN != 4)) {
    Serial.println("[SETTINGS] Strain needs ADC1 GPIO2/3/4 and 1..1024 averaging samples");
    while (true) delay(1000);
  }
  pinMode(STRAIN_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(STRAIN_PIN, ADC_11db);
  Serial.printf("[Y3] Direct analog input on GPIO%u; no voltage divider\n", STRAIN_PIN);
  if (DS_CONVERSION_MS < 750 || DS_INTERVAL_MS < DS_CONVERSION_MS) {
    Serial.println("[SETTINGS] DS conversion must be >=750ms and interval >=conversion");
    while (true) delay(1000);
  }
  if (ACCEL_RANGE > 3 || GYRO_RANGE > 3 || FILTER_SETTING < 1 || FILTER_SETTING > 6) {
    Serial.println("[SETTINGS] Range codes must be 0..3; filter must be 1..6");
    while (true) delay(1000);
  }
  if (!Wire.begin(SDA_PIN, SCL_PIN, I2C_CLOCK_HZ)) {
    Serial.println("[I2C] Controller initialization failed");
    while (true) delay(1000);
  }
  Wire.setTimeOut(I2C_TIMEOUT_MS);
  connectProbe();
  connectSensor();
  startNetwork();
}

void serviceImu() {
  if (!sensorReady) {
    if (millis() - lastRetryMs >= RETRY_INTERVAL_MS) connectSensor();
    delay(1);
    return;
  }
  if (millis() - lastReadMs < READ_INTERVAL_MS) {
    delay(1);
    return;
  }
  lastReadMs = millis();

  // One burst: accel XYZ, unused chip-temperature bytes, then gyro XYZ.
  uint8_t data[14];
  if (!readRegisters(REG_SENSOR_DATA, data, sizeof(data))) {
    sensorReady = false;
    imuValid = false;
    lastRetryMs = millis();
    Serial.println("[IMU] Read failed. Retrying...");
    return;
  }
  const float accelScale = 9.80665f / (16384.0f / (1 << ACCEL_RANGE));
  const float gyroScale = DEG_TO_RAD / (131.0f / (1 << GYRO_RANGE));

  for (uint8_t i = 0; i < 3; ++i) {
    acceleration[i] = signedValue(data + i * 2) * accelScale;
    angularVelocity[i] = signedValue(data + 8 + i * 2) * gyroScale;
  }
  imuValid = true;
  imuSampleMs = millis();
  Serial.printf("\nTime: %lu ms\n", static_cast<unsigned long>(lastReadMs));
  Serial.printf("Acceleration (m/s^2) | X: %8.3f | Y: %8.3f | Z: %8.3f\n",
                signedValue(data) * accelScale, signedValue(data + 2) * accelScale,
                signedValue(data + 4) * accelScale);
  Serial.printf("Gyroscope (rad/s)   | X: %8.3f | Y: %8.3f | Z: %8.3f\n",
                signedValue(data + 8) * gyroScale, signedValue(data + 10) * gyroScale,
                signedValue(data + 12) * gyroScale);
}

void loop() {
  serviceProbe();
  serviceStrain();
  serviceImu();
  serviceNetwork();
  publishTelemetry();
  delay(1);
}
