#include "AudioManager.h"

#include <math.h>

#include "Pins.h"

namespace AudioManager {
namespace {
constexpr uint16_t kMicSamples = 64;
constexpr uint16_t kMicPollIntervalMs = 110;
constexpr uint16_t kMicSampleSpacingUs = 120;
constexpr uint8_t kNoiseLearningSamples = 18;
constexpr uint16_t kMicDiagIntervalMs = 1000;

struct BeepStep {
  uint16_t freq;
  uint16_t duration_ms;
};

const BeepStep kRecallPattern[] = {
    {2400, 160}, {0, 90}, {2850, 160}, {0, 90}, {3200, 240}, {0, 0}};
const BeepStep kAckPattern[] = {
    {2600, 90}, {0, 0}};
const BeepStep kAlertPattern[] = {
    {3200, 120}, {0, 80}, {3200, 120}, {0, 80}, {3200, 120}, {0, 0}};

ModuleState g_audio_state;
AudioData g_audio_data;
uint16_t g_mic_samples[kMicSamples];
const BeepStep* g_pattern = nullptr;
uint8_t g_pattern_index = 0;
uint32_t g_next_step_ms = 0;
uint32_t g_last_mic_poll_ms = 0;
uint32_t g_last_mic_diag_ms = 0;
uint8_t g_noise_learning_count = 0;
bool g_speaker_ready = false;

void speakerOff() {
  ledcWriteTone(Pins::Audio::SPK_LEDC_CHANNEL, 0);
  digitalWrite(Pins::Audio::SPK_EN, LOW);
}

void applyStep(const BeepStep& step) {
  if (step.freq == 0) {
    speakerOff();
    return;
  }

  digitalWrite(Pins::Audio::SPK_MINUS, LOW);
  digitalWrite(Pins::Audio::SPK_EN, HIGH);
  ledcWriteTone(Pins::Audio::SPK_LEDC_CHANNEL, step.freq);
  ledcWrite(Pins::Audio::SPK_LEDC_CHANNEL, 128);
}

void startPattern(const BeepStep* pattern) {
  if (!g_speaker_ready) {
    g_audio_state.status = "SPK Offline";
    return;
  }
  g_pattern = pattern;
  g_pattern_index = 0;
  g_next_step_ms = 0;
}

void updateBeep() {
  if (g_pattern == nullptr || millis() < g_next_step_ms) {
    return;
  }

  const BeepStep& step = g_pattern[g_pattern_index];
  if (step.duration_ms == 0) {
    speakerOff();
    g_pattern = nullptr;
    g_audio_state.status = "Audio OK";
    return;
  }

  applyStep(step);
  g_next_step_ms = millis() + step.duration_ms;
  ++g_pattern_index;
  g_audio_state.status = step.freq == 0 ? "Beep gap" : "Beeping";
}

void updateNoise() {
  const uint32_t now = millis();
  if (now - g_last_mic_poll_ms < kMicPollIntervalMs) {
    return;
  }
  g_last_mic_poll_ms = now;

  uint32_t mean_sum = 0;
  for (uint16_t i = 0; i < kMicSamples; ++i) {
    const uint16_t raw = analogRead(Pins::Audio::MIC_ADC);
    g_mic_samples[i] = raw;
    mean_sum += raw;
    delayMicroseconds(kMicSampleSpacingUs);
  }
  const float mean = static_cast<float>(mean_sum) / kMicSamples;

  float sum_sq = 0.0f;
  uint16_t peak = 0;
  for (uint16_t i = 0; i < kMicSamples; ++i) {
    const float delta = static_cast<float>(g_mic_samples[i]) - mean;
    const uint16_t abs_delta = static_cast<uint16_t>(fabsf(delta) + 0.5f);
    peak = max<uint16_t>(peak, abs_delta);
    sum_sq += delta * delta;
  }

  const float rms = sqrtf(sum_sq / kMicSamples);
  if (g_noise_learning_count < kNoiseLearningSamples || !isfinite(g_audio_data.noise_floor)) {
    g_audio_data.noise_floor =
        (g_noise_learning_count == 0 || !isfinite(g_audio_data.noise_floor))
            ? rms
            : (g_audio_data.noise_floor * 0.82f + rms * 0.18f);
    ++g_noise_learning_count;
  } else {
    const bool near_floor = rms <= g_audio_data.noise_floor + 3.0f &&
                            peak <= static_cast<uint16_t>(g_audio_data.noise_floor * 2.0f + 18.0f);
    const float alpha = near_floor ? 0.025f : 0.002f;
    g_audio_data.noise_floor = g_audio_data.noise_floor * (1.0f - alpha) + rms * alpha;
  }

  const float rms_over_floor = max(0.0f, rms - g_audio_data.noise_floor);
  const float peak_over_floor = max(0.0f, static_cast<float>(peak) - g_audio_data.noise_floor * 2.0f);
  float level_float = max(rms_over_floor * 3.0f, peak_over_floor * 0.55f);
  if (rms_over_floor < 1.5f && peak_over_floor < 8.0f) {
    level_float = 0.0f;
  }
  const int level = constrain(static_cast<int>(roundf(level_float)), 0, 100);

  g_audio_data.raw_mean = mean;
  g_audio_data.raw_rms = rms;
  g_audio_data.raw_peak = peak;
  g_audio_data.noise_floor = max(0.0f, g_audio_data.noise_floor);
  g_audio_data.noise_level = static_cast<uint8_t>(level);
  ++g_audio_data.sample_id;

  if (level >= 70) {
    g_audio_data.noise_event = "loud";
  } else if (level >= 20) {
    g_audio_data.noise_event = "active";
  } else {
    g_audio_data.noise_event = "quiet";
  }
  if (g_pattern == nullptr) {
    g_audio_state.status = "Audio OK";
  }

  if (now - g_last_mic_diag_ms >= kMicDiagIntervalMs) {
    g_last_mic_diag_ms = now;
    Serial.printf("[MIC] mean=%.1f rms=%.1f peak=%u floor=%.1f level=%u event=%s\n",
                  g_audio_data.raw_mean,
                  g_audio_data.raw_rms,
                  static_cast<unsigned>(g_audio_data.raw_peak),
                  g_audio_data.noise_floor,
                  static_cast<unsigned>(g_audio_data.noise_level),
                  g_audio_data.noise_event.c_str());
  }
}
}  // namespace

void init() {
  pinMode(Pins::Audio::SPK_EN, OUTPUT);
  pinMode(Pins::Audio::SPK_MINUS, OUTPUT);
  digitalWrite(Pins::Audio::SPK_EN, LOW);
  digitalWrite(Pins::Audio::SPK_MINUS, LOW);
  ledcSetup(Pins::Audio::SPK_LEDC_CHANNEL, 2400, 8);
  ledcAttachPin(Pins::Audio::SPK_PLUS, Pins::Audio::SPK_LEDC_CHANNEL);
  speakerOff();
  g_speaker_ready = true;

  analogReadResolution(12);
  analogSetPinAttenuation(Pins::Audio::MIC_ADC, ADC_11db);
  pinMode(Pins::Audio::MIC_ADC, INPUT);
  g_audio_state.ready = true;
  g_audio_state.status = "Audio OK";
  Serial.printf("Audio initialized: MIC GPIO%d, speaker EN=%d +=%d -=%d.\n",
                Pins::Audio::MIC_ADC,
                Pins::Audio::SPK_EN,
                Pins::Audio::SPK_PLUS,
                Pins::Audio::SPK_MINUS);
}

void poll() {
  updateBeep();
  updateNoise();
}

void startRecallBeep() {
  startPattern(kRecallPattern);
}

void startAckBeep() {
  startPattern(kAckPattern);
}

void startAlertBeep() {
  startPattern(kAlertPattern);
}

void stopBeep() {
  g_pattern = nullptr;
  speakerOff();
  g_audio_state.status = "Audio OK";
}

void printStatus() {
  Serial.printf("Audio status: %s | mean=%.1f rms=%.1f peak=%u floor=%.1f noise=%u event=%s speaker=%s\n",
                g_audio_state.status.c_str(),
                g_audio_data.raw_mean,
                g_audio_data.raw_rms,
                static_cast<unsigned>(g_audio_data.raw_peak),
                g_audio_data.noise_floor,
                static_cast<unsigned>(g_audio_data.noise_level),
                g_audio_data.noise_event.c_str(),
                g_speaker_ready ? "ready" : "offline");
}

const ModuleState& state() {
  return g_audio_state;
}

const AudioData& data() {
  return g_audio_data;
}
}  // namespace AudioManager
