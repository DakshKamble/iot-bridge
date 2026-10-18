#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ================= CHANGE SETTINGS HERE =================
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

void connectProbe() {
  probeRetryMs = millis();
  probe.begin();
  probeReady = false;
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
      probeRetryMs = now;
    } else Serial.printf("[DS18B20] Probe temperature: %.2f C\n", temperature);
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
  Serial.println("\nMPU6050 / MPU6500 + DS18B20 | XIAO ESP32C3");
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
}

void loop() {
  serviceProbe(); // Runs even when the IMU is missing.
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

  // One burst: accel XYZ, temperature, gyro XYZ, each a signed 16-bit value.
  uint8_t data[14];
  if (!readRegisters(REG_SENSOR_DATA, data, sizeof(data))) {
    sensorReady = false;
    lastRetryMs = millis();
    Serial.println("[IMU] Read failed. Retrying...");
    return;
  }
  const float accelScale = 9.80665f / (16384.0f / (1 << ACCEL_RANGE));
  const float gyroScale = DEG_TO_RAD / (131.0f / (1 << GYRO_RANGE));
  const int16_t rawTemperature = signedValue(data + 6);
  const float temperature = chipIdentity == 0x70
      ? rawTemperature / 333.87f + 21.0f // MPU6500
      : rawTemperature / 340.0f + 36.53f; // MPU6050

  Serial.printf("\nTime: %lu ms\n", static_cast<unsigned long>(lastReadMs));
  Serial.printf("Acceleration (m/s^2) | X: %8.3f | Y: %8.3f | Z: %8.3f\n",
                signedValue(data) * accelScale, signedValue(data + 2) * accelScale,
                signedValue(data + 4) * accelScale);
  Serial.printf("Gyroscope (rad/s)   | X: %8.3f | Y: %8.3f | Z: %8.3f\n",
                signedValue(data + 8) * gyroScale, signedValue(data + 10) * gyroScale,
                signedValue(data + 12) * gyroScale);
  Serial.printf("Chip temperature    | %.2f C\n", temperature);
}
