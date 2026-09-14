#include <Arduino.h>
#include <Wire.h>

namespace {
constexpr int kSdaPin = 10;
constexpr int kSclPin = 9;
constexpr uint32_t kI2cClockHz = 100000;
constexpr uint16_t kWireTimeoutMs = 80;

constexpr uint8_t kTmp117Address = 0x48;
constexpr uint8_t kTmp117TempReg = 0x00;
constexpr uint8_t kMax3010xAddress = 0x57;
constexpr uint8_t kOv2640Address = 0x30;
constexpr uint8_t kSccbAltAddress = 0x3C;

constexpr uint16_t kBaseIterations = 100;
constexpr uint16_t kSccbIterations = 50;

struct Counter {
  uint16_t ok = 0;
  uint16_t fail = 0;
  uint16_t timeout = 0;
};

struct ReadResult {
  bool ok = false;
  bool timeout = false;
  uint8_t first = 0;
  uint8_t second = 0;
  uint32_t elapsed_ms = 0;
  uint8_t tx_error = 0;
  uint8_t rx_count = 0;
};

void printIdleState(const char* label) {
  const int sda = digitalRead(kSdaPin);
  const int scl = digitalRead(kSclPin);
  Serial.printf("[I2C-SCCB] idle_%s SDA(GPIO%d)=%s SCL(GPIO%d)=%s\n",
                label,
                kSdaPin,
                sda == HIGH ? "HIGH" : "LOW",
                kSclPin,
                scl == HIGH ? "HIGH" : "LOW");
}

bool isTimeout(uint8_t tx_error, uint32_t elapsed_ms) {
  return tx_error == 5 || elapsed_ms >= kWireTimeoutMs;
}

uint8_t ackAddress(uint8_t address, uint32_t& elapsed_ms) {
  const uint32_t start = millis();
  Wire.beginTransmission(address);
  const uint8_t error = Wire.endTransmission(true);
  elapsed_ms = millis() - start;
  return error;
}

void recordAck(uint8_t address, Counter& counter, const char* label, uint16_t index) {
  uint32_t elapsed_ms = 0;
  const uint8_t error = ackAddress(address, elapsed_ms);
  if (error == 0) {
    counter.ok++;
  } else if (isTimeout(error, elapsed_ms)) {
    counter.timeout++;
  } else {
    counter.fail++;
  }

  Serial.printf("[I2C-SCCB] %s[%03u] addr=0x%02X %s err=%u elapsed=%lums\n",
                label,
                static_cast<unsigned>(index),
                address,
                error == 0 ? "ACK" : "NO_ACK",
                static_cast<unsigned>(error),
                static_cast<unsigned long>(elapsed_ms));
}

ReadResult readTwoBytes(uint8_t address, uint8_t reg) {
  ReadResult result;
  const uint32_t start = millis();

  Wire.beginTransmission(address);
  Wire.write(reg);
  result.tx_error = Wire.endTransmission(false);
  if (result.tx_error != 0) {
    result.elapsed_ms = millis() - start;
    result.timeout = isTimeout(result.tx_error, result.elapsed_ms);
    return result;
  }

  result.rx_count = Wire.requestFrom(address, static_cast<uint8_t>(2));
  result.elapsed_ms = millis() - start;
  if (result.rx_count == 2) {
    result.first = Wire.read();
    result.second = Wire.read();
    result.ok = true;
  } else {
    while (Wire.available() > 0) {
      Wire.read();
    }
    result.timeout = result.elapsed_ms >= kWireTimeoutMs || result.rx_count == 0;
  }
  return result;
}

ReadResult readOneByte(uint8_t address, uint8_t reg) {
  ReadResult result;
  const uint32_t start = millis();

  Wire.beginTransmission(address);
  Wire.write(reg);
  result.tx_error = Wire.endTransmission(false);
  if (result.tx_error != 0) {
    result.elapsed_ms = millis() - start;
    result.timeout = isTimeout(result.tx_error, result.elapsed_ms);
    return result;
  }

  result.rx_count = Wire.requestFrom(address, static_cast<uint8_t>(1));
  result.elapsed_ms = millis() - start;
  if (result.rx_count == 1) {
    result.first = Wire.read();
    result.ok = true;
  } else {
    while (Wire.available() > 0) {
      Wire.read();
    }
    result.timeout = result.elapsed_ms >= kWireTimeoutMs || result.rx_count == 0;
  }
  return result;
}

void recordRead(const ReadResult& result, Counter& counter) {
  if (result.ok) {
    counter.ok++;
  } else if (result.timeout) {
    counter.timeout++;
  } else {
    counter.fail++;
  }
}

void printCounter(const char* label, const Counter& counter) {
  Serial.printf("[I2C-SCCB] SUMMARY %-18s ok=%u fail=%u timeout=%u\n",
                label,
                static_cast<unsigned>(counter.ok),
                static_cast<unsigned>(counter.fail),
                static_cast<unsigned>(counter.timeout));
}

void runBaseI2cTest() {
  Counter tmp_ack;
  Counter max_ack;
  Counter tmp_read;

  Serial.printf("[I2C-SCCB] Phase A/base start iterations=%u\n", static_cast<unsigned>(kBaseIterations));
  for (uint16_t i = 1; i <= kBaseIterations; ++i) {
    recordAck(kTmp117Address, tmp_ack, "TMP117_ACK", i);
    delay(2);
    recordAck(kMax3010xAddress, max_ack, "MAX3010X_ACK", i);
    delay(2);

    const ReadResult tmp = readTwoBytes(kTmp117Address, kTmp117TempReg);
    recordRead(tmp, tmp_read);
    const int16_t raw = static_cast<int16_t>((static_cast<uint16_t>(tmp.first) << 8) | tmp.second);
    const float temp_c = static_cast<float>(raw) * 0.0078125f;
    Serial.printf("[I2C-SCCB] TMP117_READ[%03u] %s tx=%u rx=%u elapsed=%lums raw=0x%02X%02X temp=%.2fC\n",
                  static_cast<unsigned>(i),
                  tmp.ok ? "OK" : (tmp.timeout ? "TIMEOUT" : "FAIL"),
                  static_cast<unsigned>(tmp.tx_error),
                  static_cast<unsigned>(tmp.rx_count),
                  static_cast<unsigned long>(tmp.elapsed_ms),
                  tmp.first,
                  tmp.second,
                  temp_c);
    delay(5);
  }

  printCounter("0x48_ack", tmp_ack);
  printCounter("0x57_ack", max_ack);
  printCounter("tmp117_reg_0x00", tmp_read);
}

void runSccbTest() {
  Counter sccb_30_ack;
  Counter sccb_3c_ack;
  Counter ov_pid_read;
  Counter ov_ver_read;

  Serial.printf("[I2C-SCCB] Phase B/SCCB start iterations=%u\n", static_cast<unsigned>(kSccbIterations));
  printIdleState("before_sccb");

  for (uint16_t i = 1; i <= kSccbIterations; ++i) {
    recordAck(kOv2640Address, sccb_30_ack, "SCCB_0x30_ACK", i);
    delay(2);
    recordAck(kSccbAltAddress, sccb_3c_ack, "SCCB_0x3C_ACK", i);
    delay(2);

    if (sccb_30_ack.ok > 0) {
      const ReadResult pid = readOneByte(kOv2640Address, 0x0A);
      const ReadResult ver = readOneByte(kOv2640Address, 0x0B);
      recordRead(pid, ov_pid_read);
      recordRead(ver, ov_ver_read);
      Serial.printf("[I2C-SCCB] OV2640_ID[%03u] pid=%s tx=%u rx=%u value=0x%02X elapsed=%lums ver=%s tx=%u rx=%u value=0x%02X elapsed=%lums match=%s\n",
                    static_cast<unsigned>(i),
                    pid.ok ? "OK" : (pid.timeout ? "TIMEOUT" : "FAIL"),
                    static_cast<unsigned>(pid.tx_error),
                    static_cast<unsigned>(pid.rx_count),
                    pid.first,
                    static_cast<unsigned long>(pid.elapsed_ms),
                    ver.ok ? "OK" : (ver.timeout ? "TIMEOUT" : "FAIL"),
                    static_cast<unsigned>(ver.tx_error),
                    static_cast<unsigned>(ver.rx_count),
                    ver.first,
                    static_cast<unsigned long>(ver.elapsed_ms),
                    pid.ok && ver.ok && pid.first == 0x26 && ver.first == 0x42 ? "OV2640_0x2642" : "NO");
    }
    delay(5);
  }

  printIdleState("after_sccb");
  printCounter("0x30_ack", sccb_30_ack);
  printCounter("0x3C_ack", sccb_3c_ack);
  printCounter("ov2640_pid_0x0A", ov_pid_read);
  printCounter("ov2640_ver_0x0B", ov_ver_read);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serial_wait_start = millis();
  while (!Serial && millis() - serial_wait_start < 8000) {
    delay(10);
  }
  delay(10000);

  Serial.println();
  Serial.println("[I2C-SCCB] baseline diagnostic start");
  Serial.println("[I2C-SCCB] no TFT/TF/4G/audio/ADXL/MAX/TMP formal drivers; no esp_camera_init; no Wire.end; no I2C recover");
  Serial.printf("[I2C-SCCB] Wire.begin(SDA=%d, SCL=%d), clock=%luHz, timeout=%ums\n",
                kSdaPin,
                kSclPin,
                static_cast<unsigned long>(kI2cClockHz),
                static_cast<unsigned>(kWireTimeoutMs));

  Wire.begin(kSdaPin, kSclPin);
  Wire.setClock(kI2cClockHz);
  Wire.setTimeOut(kWireTimeoutMs);

  printIdleState("after_begin");
  runBaseI2cTest();
  runSccbTest();
  Serial.println("[I2C-SCCB] diagnostic complete");
}

void loop() {
  delay(1000);
}
