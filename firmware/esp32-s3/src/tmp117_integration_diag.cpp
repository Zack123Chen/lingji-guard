#include <Arduino.h>
#include <Wire.h>

#include <MAX30105.h>

#include "CameraManager.h"

namespace {
constexpr int kSdaPin = 10;
constexpr int kSclPin = 9;
constexpr uint32_t kI2cClockHz = 100000;
constexpr uint16_t kWireTimeoutMs = 80;
constexpr uint8_t kTmp117Address = 0x48;
constexpr uint8_t kTmp117TempReg = 0x00;
constexpr uint16_t kTmpIterations = 100;
constexpr uint32_t kHeartPollDurationMs = 30000;
constexpr uint32_t kTmpPollPeriodMs = 1000;

struct Counter {
  uint16_t ok = 0;
  uint16_t fail = 0;
  uint16_t timeout = 0;
};

struct TmpRead {
  bool ok = false;
  bool timeout = false;
  uint8_t tx_error = 0;
  uint8_t rx_count = 0;
  uint32_t elapsed_ms = 0;
  uint16_t raw = 0;
  float temp_c = NAN;
};

MAX30105 g_max;
bool g_max_ready = false;
bool g_camera_init_attempted = false;
bool g_camera_ready = false;

bool isTimeout(uint8_t tx_error, uint32_t elapsed_ms) {
  return tx_error == 5 || elapsed_ms >= kWireTimeoutMs;
}

void configureWire100k(const char* reason) {
  Wire.begin(kSdaPin, kSclPin);
  Wire.setClock(kI2cClockHz);
  Wire.setTimeOut(kWireTimeoutMs);
  Serial.printf("[TMP-DIAG] wire_config reason=%s sda=%d scl=%d target_clock=%lu actual_clock=%lu timeout=%u\n",
                reason,
                kSdaPin,
                kSclPin,
                static_cast<unsigned long>(kI2cClockHz),
                static_cast<unsigned long>(Wire.getClock()),
                static_cast<unsigned>(kWireTimeoutMs));
}

void rebuildWire100k(const char* reason) {
  Serial.printf("[TMP-DIAG] wire_rebuild reason=%s\n", reason);
  Wire.end();
  delay(20);
  configureWire100k(reason);
}

TmpRead readTmp117() {
  TmpRead result;
  const uint32_t start = millis();
  Wire.beginTransmission(kTmp117Address);
  Wire.write(kTmp117TempReg);
  result.tx_error = Wire.endTransmission(false);
  if (result.tx_error != 0) {
    result.elapsed_ms = millis() - start;
    result.timeout = isTimeout(result.tx_error, result.elapsed_ms);
    return result;
  }

  result.rx_count = Wire.requestFrom(kTmp117Address, static_cast<uint8_t>(2));
  result.elapsed_ms = millis() - start;
  if (result.rx_count == 2) {
    const uint8_t msb = Wire.read();
    const uint8_t lsb = Wire.read();
    result.raw = (static_cast<uint16_t>(msb) << 8) | lsb;
    result.temp_c = static_cast<float>(static_cast<int16_t>(result.raw)) * 0.0078125f;
    result.ok = true;
  } else {
    while (Wire.available() > 0) {
      Wire.read();
    }
    result.timeout = result.elapsed_ms >= kWireTimeoutMs || result.rx_count == 0;
  }
  return result;
}

void recordTmpRead(const TmpRead& read, Counter& counter) {
  if (read.ok) {
    ++counter.ok;
  } else if (read.timeout) {
    ++counter.timeout;
  } else {
    ++counter.fail;
  }
}

void printStageHeader(const char* stage, const char* description) {
  Serial.printf("[TMP-DIAG] stage=%s\n", stage);
  Serial.printf("[TMP-DIAG] desc=%s\n", description);
  Serial.printf("[TMP-DIAG] wire_clock=%lu timeout=%u\n",
                static_cast<unsigned long>(Wire.getClock()),
                static_cast<unsigned>(kWireTimeoutMs));
  Serial.printf("[TMP-DIAG] MAX=%s camera=%s\n",
                g_max_ready ? "ready" : "not_ready",
                g_camera_init_attempted ? (g_camera_ready ? "ready" : "init_failed") : "not_initialized");
}

Counter runTmpBurst(const char* stage, const char* description) {
  Counter counter;
  printStageHeader(stage, description);
  for (uint16_t i = 1; i <= kTmpIterations; ++i) {
    const TmpRead read = readTmp117();
    recordTmpRead(read, counter);
    if (i <= 5 || i > kTmpIterations - 5 || !read.ok) {
      Serial.printf("[TMP-DIAG] sample[%03u] %s tx=%u rx=%u elapsed=%lums raw=0x%04X temp=%.2fC\n",
                    static_cast<unsigned>(i),
                    read.ok ? "OK" : (read.timeout ? "TIMEOUT" : "FAIL"),
                    static_cast<unsigned>(read.tx_error),
                    static_cast<unsigned>(read.rx_count),
                    static_cast<unsigned long>(read.elapsed_ms),
                    static_cast<unsigned>(read.raw),
                    read.temp_c);
    }
    delay(5);
  }
  Serial.printf("[TMP-DIAG] TMP ok=%u fail=%u timeout=%u\n",
                static_cast<unsigned>(counter.ok),
                static_cast<unsigned>(counter.fail),
                static_cast<unsigned>(counter.timeout));
  return counter;
}

void initMaxOnly() {
  Serial.println("[TMP-DIAG] MAX init start begin(Wire, I2C_SPEED_FAST), no heart polling");
  g_max_ready = g_max.begin(Wire, I2C_SPEED_FAST);
  if (!g_max_ready) {
    Serial.println("[TMP-DIAG] MAX=missing");
    return;
  }
  g_max.setup(
      0x3F,
      4,
      2,
      100,
      411,
      16384);
  g_max.setPulseAmplitudeRed(0x3F);
  g_max.setPulseAmplitudeIR(0x3F);
  g_max.setPulseAmplitudeGreen(0);
  g_max.clearFIFO();
  Serial.printf("[TMP-DIAG] MAX=ready wire_clock_after_max=%lu\n",
                static_cast<unsigned long>(Wire.getClock()));
}

void initCameraNoCapture() {
  Serial.println("[TMP-DIAG] CameraManager::init start, no capture");
  g_camera_init_attempted = true;
  CameraManager::init();
  g_camera_ready = CameraManager::state().ready;
  Serial.printf("[TMP-DIAG] camera=%s status=%s wire_clock_after_camera=%lu\n",
                g_camera_ready ? "ready" : "init_failed",
                CameraManager::state().status.c_str(),
                static_cast<unsigned long>(Wire.getClock()));
}

void runHeartPollingStage() {
  Counter counter;
  uint32_t sample_count = 0;
  uint32_t tmp_reads = 0;
  const uint32_t start = millis();
  uint32_t next_tmp_ms = start;

  printStageHeader("6_max_heart_poll_30s", "MAX3010x heart polling for 30s plus one TMP117 read per second");
  while (millis() - start < kHeartPollDurationMs) {
    if (g_max_ready) {
      g_max.check();
      const uint8_t available = g_max.available();
      for (uint8_t i = 0; i < available; ++i) {
        g_max.getFIFOIR();
        g_max.getFIFORed();
        g_max.nextSample();
        ++sample_count;
      }
    }

    const uint32_t now = millis();
    if (static_cast<int32_t>(now - next_tmp_ms) >= 0) {
      const TmpRead read = readTmp117();
      recordTmpRead(read, counter);
      ++tmp_reads;
      Serial.printf("[TMP-DIAG] heart_poll_tmp[%02lu] %s tx=%u rx=%u elapsed=%lums raw=0x%04X temp=%.2fC samples=%lu clock=%lu\n",
                    static_cast<unsigned long>(tmp_reads),
                    read.ok ? "OK" : (read.timeout ? "TIMEOUT" : "FAIL"),
                    static_cast<unsigned>(read.tx_error),
                    static_cast<unsigned>(read.rx_count),
                    static_cast<unsigned long>(read.elapsed_ms),
                    static_cast<unsigned>(read.raw),
                    read.temp_c,
                    static_cast<unsigned long>(sample_count),
                    static_cast<unsigned long>(Wire.getClock()));
      next_tmp_ms += kTmpPollPeriodMs;
    }
    delay(5);
  }

  Serial.printf("[TMP-DIAG] TMP ok=%u fail=%u timeout=%u\n",
                static_cast<unsigned>(counter.ok),
                static_cast<unsigned>(counter.fail),
                static_cast<unsigned>(counter.timeout));
  Serial.printf("[TMP-DIAG] heart_samples=%lu tmp_reads=%lu\n",
                static_cast<unsigned long>(sample_count),
                static_cast<unsigned long>(tmp_reads));
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
  Serial.println("[TMP-DIAG] integration diagnostic start");
  Serial.println("[TMP-DIAG] no TFT/TF/4G/audio/ADXL/SensorManager init; CameraManager init only at stage 4; no photo capture");

  configureWire100k("stage1_initial");
  runTmpBurst("1_wire_tmp_only", "Wire.begin(10,9) + 100kHz + only TMP117");

  initMaxOnly();
  runTmpBurst("2_after_max_init", "MAX3010x initialized, no heart polling");

  Wire.setClock(kI2cClockHz);
  Serial.printf("[TMP-DIAG] explicit Wire.setClock(100000) after MAX, actual_clock=%lu\n",
                static_cast<unsigned long>(Wire.getClock()));
  runTmpBurst("3_after_max_reset_100k", "after MAX init, explicit Wire.setClock(100000)");

  initCameraNoCapture();
  runTmpBurst("4_after_camera_init", "CameraManager::init(), no capture");

  rebuildWire100k("stage5_after_camera");
  runTmpBurst("5_after_camera_wire_rebuild", "after CameraManager init, Wire.end/begin 100k timeout 80");

  runHeartPollingStage();
  Serial.println("[TMP-DIAG] diagnostic complete");
}

void loop() {
  delay(1000);
}
