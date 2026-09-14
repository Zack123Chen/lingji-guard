#include "SensorManager.h"

#include <MAX30105.h>
#include <SPI.h>
#include <Wire.h>
#include <heartRate.h>
#include <math.h>

#include "Pins.h"

namespace SensorManager {
namespace {
constexpr uint8_t kMaxI2cAddresses = 32;
constexpr uint32_t kHeartContactIrThreshold = 8000;
constexpr uint8_t kHeartRateWindow = 5;
constexpr uint16_t kMinBeatIntervalMs = 320;
constexpr uint16_t kMaxBeatIntervalMs = 2000;
constexpr uint16_t kHeartSamplePeriodMs = 10;
constexpr float kHeartDcAlpha = 0.02f;
constexpr float kHeartAcAlpha = 0.35f;
constexpr float kHeartEnvelopeAttack = 0.30f;
constexpr float kHeartEnvelopeDecay = 0.985f;
constexpr float kHeartMinEnvelope = 12.0f;
constexpr float kHeartPeakRatio = 0.35f;
constexpr float kHeartJumpRatio = 0.35f;
constexpr float kHeartMaxJumpBpm = 35.0f;
constexpr uint16_t kHeartSignalTimeoutMs = 8000;
constexpr float kMotionScoreAlpha = 0.30f;
constexpr float kStepThreshold = 0.22f;
constexpr uint16_t kMinStepIntervalMs = 280;
constexpr uint16_t kMaxStepIntervalMs = 1800;
constexpr float kCadenceAlpha = 0.35f;
constexpr float kCadenceDecay = 0.80f;
constexpr float kRunEnterScore = 0.62f;
constexpr float kRunExitScore = 0.45f;
constexpr float kWalkEnterScore = 0.20f;
constexpr float kWalkExitScore = 0.10f;
constexpr float kRunEnterCadence = 135.0f;
constexpr float kRunExitCadence = 105.0f;
constexpr float kWalkEnterCadence = 22.0f;
constexpr float kWalkExitCadence = 8.0f;
constexpr float kTemperatureAlpha = 0.25f;
constexpr float kHumidityAlpha = 0.25f;
constexpr float kMinValidTemperatureC = -20.0f;
constexpr float kMaxValidTemperatureC = 85.0f;
constexpr float kMinValidHumidity = 0.0f;
constexpr float kMaxValidHumidity = 100.0f;
constexpr uint8_t kTmp117Address = 0x48;
constexpr uint8_t kTmp117TempReg = 0x00;
constexpr float kTmp117LsbC = 0.0078125f;
constexpr uint8_t kAdxl362CsPin = 2;
constexpr uint8_t kAdxl362Read = 0x0B;
constexpr uint8_t kAdxl362Write = 0x0A;
constexpr uint8_t kAdxl362PartIdReg = 0x02;
constexpr uint8_t kAdxl362XDataLReg = 0x0E;
constexpr uint8_t kAdxl362FilterCtlReg = 0x2C;
constexpr uint8_t kAdxl362PowerCtlReg = 0x2D;
constexpr uint8_t kAdxl362PartId = 0xF2;
constexpr float kAdxl362LsbMs2 = 9.80665f / 1000.0f;

MAX30105 g_max3010x;
SPISettings g_adxl_spi_settings(1000000, MSBFIRST, SPI_MODE0);

ModuleState g_i2c_state;
ModuleState g_mpu_state;
ModuleState g_sht_state;
ModuleState g_max_state;
SensorData g_data;

uint8_t g_i2c_addresses[kMaxI2cAddresses] = {};
uint8_t g_i2c_count = 0;
uint16_t g_rates[kHeartRateWindow] = {};
uint8_t g_rate_spot = 0;
uint8_t g_valid_rate_count = 0;
uint32_t g_last_beat_ms = 0;
uint32_t g_last_heart_sample_ms = 0;
float g_ir_dc = 0.0f;
float g_heart_ac = 0.0f;
float g_prev_heart_ac = 0.0f;
float g_prev_heart_slope = 0.0f;
float g_heart_envelope = 0.0f;
float g_candidate_peak = 0.0f;
uint32_t g_candidate_peak_ms = 0;
float g_heart_bpm_ema = 0.0f;
bool g_heart_detector_ready = false;
bool g_prev_heart_contact = false;
float g_accel_mag_lp = NAN;
float g_motion_score_lp = 0.0f;
float g_prev_motion_score = 0.0f;
uint32_t g_last_step_ms = 0;
float g_temperature_lp = NAN;
float g_humidity_lp = NAN;
bool g_temperature_ready = false;
bool g_humidity_ready = false;

bool valueInRange(float value, float min_value, float max_value) {
  return value >= min_value && value <= max_value;
}

float updateFilteredValue(float filtered, bool& ready, float sample, float alpha) {
  if (!ready || isnan(filtered)) {
    ready = true;
    return sample;
  }
  return filtered + (sample - filtered) * alpha;
}

uint8_t adxlReadRegister(uint8_t reg) {
  SPI.beginTransaction(g_adxl_spi_settings);
  digitalWrite(kAdxl362CsPin, LOW);
  SPI.transfer(kAdxl362Read);
  SPI.transfer(reg);
  const uint8_t value = SPI.transfer(0x00);
  digitalWrite(kAdxl362CsPin, HIGH);
  SPI.endTransaction();
  return value;
}

void adxlWriteRegister(uint8_t reg, uint8_t value) {
  SPI.beginTransaction(g_adxl_spi_settings);
  digitalWrite(kAdxl362CsPin, LOW);
  SPI.transfer(kAdxl362Write);
  SPI.transfer(reg);
  SPI.transfer(value);
  digitalWrite(kAdxl362CsPin, HIGH);
  SPI.endTransaction();
}

bool readAdxl362Accel(float& ax, float& ay, float& az) {
  uint8_t raw[6] = {};
  SPI.beginTransaction(g_adxl_spi_settings);
  digitalWrite(kAdxl362CsPin, LOW);
  SPI.transfer(kAdxl362Read);
  SPI.transfer(kAdxl362XDataLReg);
  for (uint8_t i = 0; i < sizeof(raw); ++i) {
    raw[i] = SPI.transfer(0x00);
  }
  digitalWrite(kAdxl362CsPin, HIGH);
  SPI.endTransaction();

  const int16_t x = static_cast<int16_t>((static_cast<uint16_t>(raw[1]) << 8) | raw[0]);
  const int16_t y = static_cast<int16_t>((static_cast<uint16_t>(raw[3]) << 8) | raw[2]);
  const int16_t z = static_cast<int16_t>((static_cast<uint16_t>(raw[5]) << 8) | raw[4]);
  ax = static_cast<float>(x) * kAdxl362LsbMs2;
  ay = static_cast<float>(y) * kAdxl362LsbMs2;
  az = static_cast<float>(z) * kAdxl362LsbMs2;
  return true;
}

void initADXL362() {
  pinMode(kAdxl362CsPin, OUTPUT);
  digitalWrite(kAdxl362CsPin, HIGH);
  SPI.begin(Pins::TFT::SCLK, Pins::TFT::MISO, Pins::TFT::MOSI, kAdxl362CsPin);
  delay(20);

  const uint8_t part_id = adxlReadRegister(kAdxl362PartIdReg);
  if (part_id != kAdxl362PartId) {
    g_mpu_state.ready = false;
    g_mpu_state.status = "ADXL missing";
    Serial.printf("ADXL362 not found. PARTID=0x%02X\n", part_id);
    return;
  }

  adxlWriteRegister(kAdxl362FilterCtlReg, 0x13);
  adxlWriteRegister(kAdxl362PowerCtlReg, 0x02);
  g_mpu_state.ready = true;
  g_mpu_state.status = "ADXL OK";
  Serial.println("ADXL362 initialized.");
}

bool readTMP117(float& temperature_c) {
  Wire.beginTransmission(kTmp117Address);
  Wire.write(kTmp117TempReg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(kTmp117Address, static_cast<uint8_t>(2)) != 2) {
    return false;
  }
  const uint8_t msb = Wire.read();
  const uint8_t lsb = Wire.read();
  const int16_t raw = static_cast<int16_t>((static_cast<uint16_t>(msb) << 8) | lsb);
  temperature_c = static_cast<float>(raw) * kTmp117LsbC;
  return true;
}

void initTMP117() {
  float temperature = NAN;
  if (!readTMP117(temperature)) {
    g_sht_state.ready = false;
    g_sht_state.status = "TMP missing";
    Serial.println("TMP117 not found at 0x48.");
    return;
  }

  g_temperature_lp = temperature;
  g_temperature_ready = true;
  g_data.temperature_c = temperature;
  g_data.humidity_percent = NAN;
  g_sht_state.ready = true;
  g_sht_state.status = "TMP OK";
  Serial.printf("TMP117 initialized: %.2fC\n", temperature);
}

void initMAX3010x() {
  if (!g_max3010x.begin(Wire, I2C_SPEED_FAST)) {
    g_max_state.ready = false;
    g_max_state.status = "MAX missing";
    Serial.println("MAX30102/MAX30105 not found.");
    return;
  }

  g_max3010x.setup(
      0x3F,  // LED brightness. Keep contact signal below ADC saturation.
      4,     // Sample average.
      2,     // Red + IR.
      100,   // Sample rate.
      411,   // Pulse width.
      16384  // ADC range.
  );
  g_max3010x.setPulseAmplitudeRed(0x3F);
  g_max3010x.setPulseAmplitudeIR(0x3F);
  g_max3010x.setPulseAmplitudeGreen(0);
  g_max3010x.clearFIFO();
  configureI2CBus();
  g_max_state.ready = true;
  g_max_state.status = "MAX OK";
  Serial.println("MAX30102/MAX30105 initialized.");
}

void resetHeartRateEstimator() {
  g_rate_spot = 0;
  g_valid_rate_count = 0;
  memset(g_rates, 0, sizeof(g_rates));
  g_last_beat_ms = 0;
  g_last_heart_sample_ms = 0;
  g_ir_dc = 0.0f;
  g_heart_ac = 0.0f;
  g_prev_heart_ac = 0.0f;
  g_prev_heart_slope = 0.0f;
  g_heart_envelope = 0.0f;
  g_candidate_peak = 0.0f;
  g_candidate_peak_ms = 0;
  g_heart_bpm_ema = 0.0f;
  g_heart_detector_ready = false;
}

uint16_t medianHeartRate() {
  uint16_t sorted[kHeartRateWindow] = {};
  for (uint8_t i = 0; i < g_valid_rate_count; ++i) {
    sorted[i] = g_rates[i];
  }
  for (uint8_t i = 1; i < g_valid_rate_count; ++i) {
    const uint16_t value = sorted[i];
    int8_t j = static_cast<int8_t>(i) - 1;
    while (j >= 0 && sorted[j] > value) {
      sorted[j + 1] = sorted[j];
      --j;
    }
    sorted[j + 1] = value;
  }
  return sorted[g_valid_rate_count / 2];
}

bool shouldAcceptBpm(float bpm) {
  if (g_valid_rate_count < 2 || g_heart_bpm_ema <= 0.0f) {
    return true;
  }
  const float max_jump = max(kHeartMaxJumpBpm, g_heart_bpm_ema * kHeartJumpRatio);
  return fabsf(bpm - g_heart_bpm_ema) <= max_jump;
}

void pushHeartRate(float bpm);

bool recordBeatCandidate(uint32_t beat_ms) {
  if (g_last_beat_ms == 0) {
    g_last_beat_ms = beat_ms;
    g_data.heart_sample_type = "contact_learning";
    return true;
  }

  const uint32_t delta = beat_ms - g_last_beat_ms;
  if (delta < kMinBeatIntervalMs) {
    return false;
  }
  if (delta > kMaxBeatIntervalMs) {
    g_last_beat_ms = beat_ms;
    g_data.heart_sample_type = "contact_learning";
    return true;
  }

  const float bpm = 60000.0f / static_cast<float>(delta);
  if (bpm >= 30.0f && bpm <= 220.0f && shouldAcceptBpm(bpm)) {
    pushHeartRate(bpm);
    g_last_beat_ms = beat_ms;
    return true;
  }

  g_data.heart_sample_type = (g_valid_rate_count == 0) ? "contact_learning" : "beat_reject";
  return false;
}

void pushHeartRate(float bpm) {
  const uint16_t rounded_bpm = static_cast<uint16_t>(roundf(bpm));
  g_rates[g_rate_spot] = rounded_bpm;
  g_rate_spot = (g_rate_spot + 1) % kHeartRateWindow;
  if (g_valid_rate_count < kHeartRateWindow) {
    ++g_valid_rate_count;
  }

  if (g_heart_bpm_ema <= 0.0f) {
    g_heart_bpm_ema = bpm;
  } else {
    g_heart_bpm_ema = 0.70f * g_heart_bpm_ema + 0.30f * bpm;
  }

  g_data.heart_bpm = static_cast<int>(medianHeartRate());
  g_data.heart_sample_type = (g_valid_rate_count >= 3) ? "beat_avg" : "beat_detected";
}

void updateMotionState() {
  const float score = g_motion_score_lp;
  const float cadence = g_data.cadence_spm;
  const String current = g_data.motion;

  // Keep state transitions sticky so the release screen does not flicker near thresholds.
  if (current == "running") {
    if (score >= kRunExitScore || cadence >= kRunExitCadence) {
      return;
    }
    g_data.motion = (score >= kWalkExitScore || cadence >= kWalkExitCadence) ? "walking" : "rest";
    return;
  }

  if (current == "walking") {
    if (score >= kRunEnterScore || cadence >= kRunEnterCadence) {
      g_data.motion = "running";
    } else if (score < kWalkExitScore && cadence < kWalkExitCadence) {
      g_data.motion = "rest";
    }
    return;
  }

  if (score >= kRunEnterScore || cadence >= kRunEnterCadence) {
    g_data.motion = "running";
  } else if (score >= kWalkEnterScore || cadence >= kWalkEnterCadence) {
    g_data.motion = "walking";
  } else {
    g_data.motion = "rest";
  }
}

void updateCadenceAfterStep(uint32_t now) {
  const uint32_t previous_step_ms = g_last_step_ms;
  const uint32_t step_interval_ms = previous_step_ms == 0 ? 0 : now - previous_step_ms;

  ++g_data.step_count;
  if (step_interval_ms >= kMinStepIntervalMs && step_interval_ms <= kMaxStepIntervalMs) {
    const float instant_cadence = fminf(220.0f, 60000.0f / static_cast<float>(step_interval_ms));
    if (g_data.cadence_spm <= 0.5f) {
      g_data.cadence_spm = instant_cadence;
    } else {
      g_data.cadence_spm += (instant_cadence - g_data.cadence_spm) * kCadenceAlpha;
    }
  } else if (previous_step_ms == 0 || step_interval_ms > kMaxStepIntervalMs) {
    g_data.cadence_spm = 0.0f;
  }

  g_last_step_ms = now;
}

void decayCadenceIfIdle(uint32_t now) {
  if (g_last_step_ms == 0 || now - g_last_step_ms <= kMaxStepIntervalMs) {
    return;
  }

  g_data.cadence_spm *= kCadenceDecay;
  if (g_data.cadence_spm < 1.0f) {
    g_data.cadence_spm = 0.0f;
  }
}

void updateHeartRate(uint32_t ir, uint32_t red, uint32_t sample_ms) {
  g_data.ir = ir;
  g_data.red = red;
  g_data.heart_contact = ir >= kHeartContactIrThreshold;

  if (!g_data.heart_contact) {
    g_data.heart_bpm = 0;
    g_data.heart_sample_type = "no_contact";
    if (g_prev_heart_contact) {
      resetHeartRateEstimator();
    }
    g_prev_heart_contact = false;
    return;
  }

  if (!g_prev_heart_contact) {
    resetHeartRateEstimator();
    g_prev_heart_contact = true;
  }

  if (!g_heart_detector_ready) {
    g_ir_dc = static_cast<float>(ir);
    g_heart_detector_ready = true;
    g_last_heart_sample_ms = sample_ms;
    g_data.heart_sample_type = "contact_settling";
    return;
  }

  g_last_heart_sample_ms = sample_ms;
  g_ir_dc += (static_cast<float>(ir) - g_ir_dc) * kHeartDcAlpha;
  const float ac = static_cast<float>(ir) - g_ir_dc;
  g_prev_heart_ac = g_heart_ac;
  g_heart_ac += (ac - g_heart_ac) * kHeartAcAlpha;
  const float slope = g_heart_ac - g_prev_heart_ac;
  const float abs_ac = fabsf(g_heart_ac);
  if (abs_ac > g_heart_envelope) {
    g_heart_envelope = (1.0f - kHeartEnvelopeAttack) * g_heart_envelope + kHeartEnvelopeAttack * abs_ac;
  } else {
    g_heart_envelope = kHeartEnvelopeDecay * g_heart_envelope + (1.0f - kHeartEnvelopeDecay) * abs_ac;
  }

  if (slope > 0.0f && g_heart_ac > g_candidate_peak) {
    g_candidate_peak = g_heart_ac;
    g_candidate_peak_ms = sample_ms;
  }

  const float threshold = max(kHeartMinEnvelope, g_heart_envelope * kHeartPeakRatio);
  const bool local_peak = g_prev_heart_slope > 0.0f && slope <= 0.0f;
  const bool pba_beat = checkForBeat(static_cast<int32_t>(ir));
  if (pba_beat) {
    recordBeatCandidate(sample_ms);
  }

  if (local_peak && g_candidate_peak > threshold) {
    recordBeatCandidate(g_candidate_peak_ms);
  }

  if (local_peak) {
    g_candidate_peak = 0.0f;
    g_candidate_peak_ms = sample_ms;
  }
  g_prev_heart_slope = slope;

  if (g_valid_rate_count == 0) {
    g_data.heart_bpm = 0;
    if (g_heart_envelope < kHeartMinEnvelope) {
      g_data.heart_sample_type = "weak_signal";
    } else if (g_data.heart_sample_type != "contact_learning") {
      g_data.heart_sample_type = "contact_settling";
    }
    return;
  }
}

void updateMotion() {
  if (isnan(g_data.ax) || isnan(g_data.ay) || isnan(g_data.az)) {
    g_data.motion = "unknown";
    g_data.motion_score = NAN;
    return;
  }

  const float accel_mag = sqrtf(g_data.ax * g_data.ax + g_data.ay * g_data.ay + g_data.az * g_data.az);
  if (isnan(g_accel_mag_lp)) {
    g_accel_mag_lp = accel_mag;
  }
  g_accel_mag_lp = 0.85f * g_accel_mag_lp + 0.15f * accel_mag;

  const float dynamic_accel_g = fabsf(accel_mag - g_accel_mag_lp) / 9.81f;
  float gyro_mag = 0.0f;
  if (!isnan(g_data.gx) && !isnan(g_data.gy) && !isnan(g_data.gz)) {
    gyro_mag = sqrtf(g_data.gx * g_data.gx + g_data.gy * g_data.gy + g_data.gz * g_data.gz);
  }
  const float instant_score = dynamic_accel_g + 0.12f * gyro_mag;
  g_motion_score_lp += (instant_score - g_motion_score_lp) * kMotionScoreAlpha;
  g_data.motion_score = g_motion_score_lp;

  const uint32_t now = millis();
  if (g_prev_motion_score < kStepThreshold && g_motion_score_lp >= kStepThreshold &&
      now - g_last_step_ms >= kMinStepIntervalMs) {
    updateCadenceAfterStep(now);
  } else {
    decayCadenceIfIdle(now);
  }
  g_prev_motion_score = g_motion_score_lp;

  updateMotionState();
}
}  // namespace

void configureI2CBus() {
  Wire.begin(Pins::I2C::SDA, Pins::I2C::SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(80);
}

void initI2C() {
  configureI2CBus();
  g_i2c_state.ready = true;
  g_i2c_state.status = "I2C OK";
  Serial.println("I2C initialized at 100 kHz with 80 ms timeout.");
}

void recoverI2CBus(const char* reason) {
  Serial.printf("Recovering shared I2C bus after %s...\n", reason);
  Wire.end();
  delay(80);
  configureI2CBus();
  delay(80);
}

String i2cAddressesText() {
  if (g_i2c_count == 0) {
    return "none";
  }

  String result;
  for (uint8_t i = 0; i < g_i2c_count; ++i) {
    char buf[8];
    snprintf(buf, sizeof(buf), "0x%02X", g_i2c_addresses[i]);
    if (i > 0) {
      result += " ";
    }
    result += buf;
  }
  return result;
}

void scanI2C() {
  g_i2c_count = 0;
  Serial.println("Scanning I2C bus...");

  Wire.setTimeOut(8);
  for (uint8_t address = 1; address < 127; ++address) {
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error == 0) {
      if (g_i2c_count < kMaxI2cAddresses) {
        g_i2c_addresses[g_i2c_count] = address;
        ++g_i2c_count;
      }
      Serial.printf("  I2C device found at 0x%02X\n", address);
    }
    yield();
  }
  Wire.setTimeOut(80);

  if (g_i2c_count == 0) {
    Serial.println("  No I2C devices found.");
    g_i2c_state.status = "I2C no devices";
  } else {
    g_i2c_state.status = "I2C: " + i2cAddressesText();
  }
}

void initAll() {
  initADXL362();
  initTMP117();
  initMAX3010x();
}

void initTemperature() {
  initTMP117();
}

void pollMotionSensor() {
  if (!g_mpu_state.ready) {
    return;
  }

  float ax = NAN;
  float ay = NAN;
  float az = NAN;
  if (!readAdxl362Accel(ax, ay, az)) {
    g_mpu_state.status = "ADXL read err";
    return;
  }
  g_data.ax = ax;
  g_data.ay = ay;
  g_data.az = az;
  g_data.gx = NAN;
  g_data.gy = NAN;
  g_data.gz = NAN;
  updateMotion();
}

void pollHeartSensor() {
  if (!g_max_state.ready) {
    return;
  }

  const uint32_t now = millis();
  g_max3010x.check();
  const uint8_t available = g_max3010x.available();
  for (uint8_t i = 0; i < available; ++i) {
    const uint8_t remaining = available - i - 1;
    const uint32_t sample_ms = now - static_cast<uint32_t>(remaining) * kHeartSamplePeriodMs;
    updateHeartRate(g_max3010x.getFIFOIR(), g_max3010x.getFIFORed(), sample_ms);
    g_max3010x.nextSample();
  }

  if (g_data.heart_contact && g_last_heart_sample_ms != 0 &&
      now - g_last_heart_sample_ms > kHeartSignalTimeoutMs) {
    g_data.heart_contact = false;
    g_data.heart_bpm = 0;
    g_data.heart_sample_type = "signal_timeout";
    resetHeartRateEstimator();
    g_prev_heart_contact = false;
  }
}

void read() {
  pollMotionSensor();

  if (g_sht_state.ready) {
    float temperature = NAN;
    if (readTMP117(temperature)) {
      const bool temperature_valid = valueInRange(temperature,
                                                  kMinValidTemperatureC,
                                                  kMaxValidTemperatureC);
      if (temperature_valid) {
        g_temperature_lp = updateFilteredValue(g_temperature_lp,
                                               g_temperature_ready,
                                               temperature,
                                               kTemperatureAlpha);
        g_data.temperature_c = g_temperature_lp;
        g_data.humidity_percent = NAN;
        g_sht_state.status = "TMP OK";
      } else {
        g_sht_state.status = "TMP invalid";
        Serial.printf("TMP117 invalid sample: %.2fC\n", temperature);
      }
    } else {
      g_sht_state.status = "TMP read err";
      Serial.println("TMP117 read failed.");
    }
  }

  pollHeartSensor();

  printData();
}

void printData() {
  Serial.printf("ADXL A=%.2f,%.2f,%.2f motion=%s score=%.2f steps=%lu cadence=%.1f | TMP %.1fC | MAX IR=%lu RED=%lu BPM=%d %s\n",
                g_data.ax,
                g_data.ay,
                g_data.az,
                g_data.motion.c_str(),
                g_data.motion_score,
                static_cast<unsigned long>(g_data.step_count),
                g_data.cadence_spm,
                g_data.temperature_c,
                static_cast<unsigned long>(g_data.ir),
                static_cast<unsigned long>(g_data.red),
                g_data.heart_bpm,
                g_data.heart_sample_type.c_str());
}

const ModuleState& i2cState() {
  return g_i2c_state;
}

const ModuleState& mpuState() {
  return g_mpu_state;
}

const ModuleState& shtState() {
  return g_sht_state;
}

const ModuleState& maxState() {
  return g_max_state;
}

const SensorData& data() {
  return g_data;
}

uint8_t i2cCount() {
  return g_i2c_count;
}
}  // namespace SensorManager
