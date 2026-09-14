#include <Arduino.h>

#ifdef PSRAM_ONLY_DIAGNOSTIC

#include "esp_heap_caps.h"
#include "esp_system.h"

#include "Pins.h"

namespace {
constexpr size_t kPsramTestBytes = 512 * 1024;

bool verifyPsramPattern(uint8_t* buffer, size_t len, uint8_t seed) {
  for (size_t i = 0; i < len; ++i) {
    buffer[i] = static_cast<uint8_t>(seed + (i * 31u) + (i >> 3));
  }

  for (size_t i = 0; i < len; ++i) {
    const uint8_t expected = static_cast<uint8_t>(seed + (i * 31u) + (i >> 3));
    if (buffer[i] != expected) {
      Serial.printf("[PSRAM] VERIFY_FAIL offset=%u expected=0x%02X actual=0x%02X\n",
                    static_cast<unsigned>(i),
                    expected,
                    buffer[i]);
      return false;
    }
  }
  return true;
}

void runPsramDiagnostic() {
  Serial.println("[PSRAM] PSRAM-only diagnostic firmware");
  Serial.println("[PSRAM] Camera init and auto snapshot are intentionally disabled.");
  Serial.printf("[PSRAM] chip=%s rev=%u cpu=%uMHz sdk=%s\n",
                ESP.getChipModel(),
                static_cast<unsigned>(ESP.getChipRevision()),
                ESP.getCpuFreqMHz(),
                ESP.getSdkVersion());
  Serial.printf("[PSRAM] psramFound=%s\n", psramFound() ? "true" : "false");
  Serial.printf("[PSRAM] total=%u free=%u\n",
                static_cast<unsigned>(ESP.getPsramSize()),
                static_cast<unsigned>(ESP.getFreePsram()));
  Serial.printf("[PSRAM] heap_caps total=%u free=%u largest=%u\n",
                static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));

  uint8_t* buffer = static_cast<uint8_t*>(
      heap_caps_malloc(kPsramTestBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  Serial.printf("[PSRAM] malloc_512KB=%s ptr=%p\n", buffer ? "OK" : "FAIL", buffer);
  if (buffer == nullptr) {
    return;
  }

  const bool pass_a = verifyPsramPattern(buffer, kPsramTestBytes, 0x3C);
  const bool pass_b = verifyPsramPattern(buffer, kPsramTestBytes, 0xA5);
  Serial.printf("[PSRAM] verify_512KB=%s\n", (pass_a && pass_b) ? "PASS" : "FAIL");
  heap_caps_free(buffer);
  Serial.printf("[PSRAM] free_after=%u\n", static_cast<unsigned>(ESP.getFreePsram()));
}
}  // namespace

void setup() {
  Serial.begin(Pins::kBaudRate);
  delay(1500);
  Serial.println();
  runPsramDiagnostic();
}

void loop() {
  delay(5000);
  runPsramDiagnostic();
}

#elif defined(TF_WRITE_PROBE_ENTRY)

#define TF_PROBE_EXTERNAL_MAIN
#include "tf_write_probe.cpp"

void setup() {
  Serial.begin(115200);
  delay(1800);
  Serial.println();
  tfWriteProbeRun();
}

void loop() {
  delay(5000);
}

#elif defined(CAMERA_JPEG_DIAGNOSTIC)

#include "Pins.h"

void setup() {
  Serial.begin(Pins::kBaudRate);
  delay(1500);
  Serial.println();
  Serial.println("[CAMJPEG] disabled in final firmware: no JPEG capture, no image files.");
}

void loop() {
  delay(5000);
  Serial.println("[CAMJPEG] disabled");
}

#elif defined(CAMERA_PROD_BOARD_DIAG)

#include "esp_heap_caps.h"

#include <FS.h>
#include <SD_MMC.h>

#define sensor_t camera_sensor_t
#include "esp_camera.h"
#undef sensor_t

#include "Pins.h"

namespace {
camera_config_t makeProdBoardDiagConfig() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Pins::Camera::D0;
  config.pin_d1 = Pins::Camera::D1;
  config.pin_d2 = Pins::Camera::D2;
  config.pin_d3 = Pins::Camera::D3;
  config.pin_d4 = Pins::Camera::D4;
  config.pin_d5 = Pins::Camera::D5;
  config.pin_d6 = Pins::Camera::D6;
  config.pin_d7 = Pins::Camera::D7;
  config.pin_xclk = Pins::Camera::XCLK;
  config.pin_pclk = Pins::Camera::PCLK;
  config.pin_vsync = Pins::Camera::VSYNC;
  config.pin_href = Pins::Camera::HREF;
  config.pin_sccb_sda = Pins::Camera::SIOD;
  config.pin_sccb_scl = Pins::Camera::SIOC;
  config.pin_pwdn = Pins::Camera::PWDN;
  config.pin_reset = Pins::Camera::RESET;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QQVGA;
  config.fb_location = CAMERA_FB_IN_DRAM;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.jpeg_quality = 12;
  return config;
}

void printFrameResult(uint8_t index, camera_fb_t* fb) {
  if (fb == nullptr) {
    Serial.printf("[CAM-PROD-BOARD] frame_%u fb=null\n", static_cast<unsigned>(index));
    return;
  }
  const bool soi = fb->len >= 2 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8;
  const bool eoi = fb->len >= 2 && fb->buf[fb->len - 2] == 0xFF && fb->buf[fb->len - 1] == 0xD9;
  Serial.printf("[CAM-PROD-BOARD] frame_%u fb=ok width=%u height=%u len=%u soi=%s eoi=%s format=%d\n",
                static_cast<unsigned>(index),
                static_cast<unsigned>(fb->width),
                static_cast<unsigned>(fb->height),
                static_cast<unsigned>(fb->len),
                soi ? "OK" : "BAD",
                eoi ? "OK" : "BAD",
                fb->format);
}

void runProdBoardCameraDiag() {
  Serial.println("[CAM-PROD-BOARD] diag start");
  Serial.println("[CAM-PROD-BOARD] board=esp32-s3-devkitc-1 flags=BOARD_HAS_PSRAM,USB_CDC no qio_opi");
  Serial.printf("[CAM-PROD-BOARD] psramFound=%s total=%u free=%u spiram_total=%u\n",
                psramFound() ? "true" : "false",
                static_cast<unsigned>(ESP.getPsramSize()),
                static_cast<unsigned>(ESP.getFreePsram()),
                static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)));
  pinMode(Pins::Storage::CD_IN, INPUT_PULLUP);
  if (!SD_MMC.setPins(Pins::Storage::CLK, Pins::Storage::CMD, Pins::Storage::DATA0)) {
    Serial.println("[CAM-PROD-BOARD] TF setPins failed");
  } else if (!SD_MMC.begin("/sdcard", true, false, 4000, 5)) {
    Serial.println("[CAM-PROD-BOARD] TF mount failed");
  } else {
    Serial.printf("[CAM-PROD-BOARD] TF mounted size=%lluMB used=%lluMB\n",
                  SD_MMC.cardSize() / (1024ULL * 1024ULL),
                  SD_MMC.usedBytes() / (1024ULL * 1024ULL));
  }

  const camera_config_t config = makeProdBoardDiagConfig();
  const esp_err_t err = esp_camera_init(&config);
  Serial.printf("[CAM-PROD-BOARD] esp_camera_init=0x%x\n", err);
  if (err != ESP_OK) {
    return;
  }
  camera_sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    sensor->set_pixformat(sensor, PIXFORMAT_JPEG);
    sensor->set_framesize(sensor, FRAMESIZE_QQVGA);
    sensor->set_quality(sensor, 12);
    sensor->set_colorbar(sensor, 0);
  }
  delay(1000);
  for (uint8_t i = 0; i < 5; ++i) {
    camera_fb_t* fb = esp_camera_fb_get();
    printFrameResult(i, fb);
    if (fb != nullptr) {
      esp_camera_fb_return(fb);
    }
    delay(200);
  }
  Serial.println("[CAM-PROD-BOARD] diag complete");
}
}  // namespace

void setup() {
  Serial.begin(Pins::kBaudRate);
  delay(8000);
  Serial.println();
  runProdBoardCameraDiag();
}

void loop() {
  delay(5000);
  Serial.println("[CAM-PROD-BOARD] idle");
}

#elif defined(CAMERA_MANAGER_ONLY_DIAG)

#include "CameraManager.h"
#include "Pins.h"
#include "SensorManager.h"

namespace {
void printManagerFrame(uint8_t index, bool ok, const CameraFrame& frame) {
  Serial.printf("[CAM-MGR] capture_%u %s", static_cast<unsigned>(index), ok ? "success" : "fail");
  if (ok) {
    const bool soi = frame.len >= 2 && frame.data[0] == 0xFF && frame.data[1] == 0xD8;
    const bool eoi = frame.len >= 2 && frame.data[frame.len - 2] == 0xFF && frame.data[frame.len - 1] == 0xD9;
    Serial.printf(" width=%u height=%u len=%u SOI=%s EOI=%s format=%d",
                  static_cast<unsigned>(frame.width),
                  static_cast<unsigned>(frame.height),
                  static_cast<unsigned>(frame.len),
                  soi ? "OK" : "BAD",
                  eoi ? "OK" : "BAD",
                  frame.format);
  }
  Serial.println();
}

void runCameraManagerOnlyDiag() {
  Serial.println("[CAM-MGR] diag start");
  Serial.println("[CAM-MGR] only Wire.begin via SensorManager::initI2C + CameraManager::init");
  SensorManager::initI2C();
  CameraManager::init();
  Serial.printf("[CAM-MGR] init result ready=%s status=%s\n",
                CameraManager::state().ready ? "true" : "false",
                CameraManager::state().status.c_str());
  delay(1000);
  for (uint8_t i = 0; i < 5; ++i) {
    CameraFrame frame;
    const bool ok = CameraManager::captureJpegFrame(frame);
    printManagerFrame(i, ok, frame);
    if (ok) {
      CameraManager::releaseFrame(frame);
    }
    delay(500);
  }
  Serial.println("[CAM-MGR] diag complete");
}
}  // namespace

void setup() {
  Serial.begin(Pins::kBaudRate);
  delay(20000);
  Serial.println();
  runCameraManagerOnlyDiag();
}

void loop() {
  delay(5000);
  Serial.println("[CAM-MGR] idle");
}

#elif defined(CAMERA_MANAGER_CLEAN_DIAG)

#include "CameraManager.h"
#include "Pins.h"
#include "SensorManager.h"

namespace {
void printCleanFrame(uint8_t index, bool ok, const CameraFrame& frame) {
  Serial.printf("[CAM-CLEAN] capture_%u %s\n", static_cast<unsigned>(index), ok ? "OK" : "FAIL");
  if (!ok) {
    return;
  }
  const bool soi = frame.len >= 2 && frame.data[0] == 0xFF && frame.data[1] == 0xD8;
  const bool eoi = frame.len >= 2 && frame.data[frame.len - 2] == 0xFF && frame.data[frame.len - 1] == 0xD9;
  Serial.printf("[CAM-CLEAN] width=%u height=%u len=%u SOI=%s EOI=%s format=%d\n",
                static_cast<unsigned>(frame.width),
                static_cast<unsigned>(frame.height),
                static_cast<unsigned>(frame.len),
                soi ? "OK" : "BAD",
                eoi ? "OK" : "BAD",
                frame.format);
}

void runCameraManagerCleanDiag() {
  Serial.println("[CAM-CLEAN] diag start");
  Serial.println("[CAM-CLEAN] SensorManager::initI2C once, no SCCB probe, no image settings, no dropFrames, no I2C recover");
  SensorManager::initI2C();
  CameraManager::init();
  Serial.printf("[CAM-CLEAN] init ready=%s status=%s\n",
                CameraManager::state().ready ? "true" : "false",
                CameraManager::state().status.c_str());
  for (uint8_t i = 0; i < 5; ++i) {
    CameraFrame frame;
    const bool ok = CameraManager::captureJpegFrame(frame);
    printCleanFrame(i, ok, frame);
    if (ok) {
      CameraManager::releaseFrame(frame);
    }
    delay(500);
  }
  Serial.println("[CAM-CLEAN] diag complete");
}
}  // namespace

void setup() {
  Serial.begin(Pins::kBaudRate);
  delay(12000);
  Serial.println();
  runCameraManagerCleanDiag();
}

void loop() {
  delay(5000);
  Serial.println("[CAM-CLEAN] idle");
}

#else

#include "Air780EManager.h"
#include "AudioManager.h"
#include "CameraManager.h"
#include "CommandConsole.h"
#include "DisplayManager.h"
#include "Pins.h"
#include "SensorManager.h"
#include "StorageManager.h"
#include "TelemetryBuilder.h"

#include "esp_log.h"

namespace {
constexpr uint32_t kSensorIntervalMs = 1000;
constexpr uint32_t kMotionPollIntervalMs = 50;
constexpr uint32_t kHeartPollIntervalMs = 20;
constexpr uint32_t kNormalMqttIntervalMs = 5000;
constexpr uint32_t kEcoMqttIntervalMs = 15000;
constexpr uint32_t kNormalAutoSnapshotIntervalMs = 20000;
constexpr uint32_t kEcoAutoSnapshotIntervalMs = 60000;
constexpr uint32_t kPreviewDurationMs = 5000;
constexpr uint32_t kPreviewErrorDurationMs = 2000;
constexpr uint32_t kLocatorFlashDurationMs = 5000;
constexpr uint32_t kButtonDebounceMs = 45;
constexpr uint32_t kCameraOfflineAutoRetryMs = 60000;
constexpr uint32_t kTftFullRefreshIntervalMs = 60000;
constexpr bool kAutoSnapshotEnabled = false;
constexpr uint16_t kUiRed = 0xF800;
constexpr uint16_t kUiGreen = 0x07E0;
constexpr uint16_t kUiCyan = 0x07FF;
constexpr uint16_t kUiOrange = 0xFC00;

uint32_t g_last_sensor_ms = 0;
uint32_t g_last_motion_poll_ms = 0;
uint32_t g_last_heart_poll_ms = 0;
uint32_t g_last_mqtt_ms = 0;
uint32_t g_last_auto_snapshot_ms = 0;
uint32_t g_last_tft_full_refresh_ms = 0;
uint32_t g_last_audio_sample_id = 0;
uint32_t g_next_camera_auto_retry_ms = 0;
uint32_t g_preview_until_ms = 0;
DisplayManager::Page g_current_page = DisplayManager::Page::Dashboard;
bool g_camera_preview_latched = false;
uint8_t g_camera_consecutive_failures = 0;
int g_camera_shutter_last_raw = HIGH;
int g_camera_shutter_stable = HIGH;
uint32_t g_camera_shutter_last_change_ms = 0;
RuntimeStatus g_runtime;

uint32_t mqttIntervalMs() {
  return g_runtime.eco_mode ? kEcoMqttIntervalMs : kNormalMqttIntervalMs;
}

uint32_t autoSnapshotIntervalMs() {
  if (!kAutoSnapshotEnabled) {
    return 0;
  }
  return g_runtime.eco_mode ? kEcoAutoSnapshotIntervalMs : kNormalAutoSnapshotIntervalMs;
}

void recordCommandResult(const String& command, const String& result) {
  g_runtime.last_command = command.length() ? command : "none";
  g_runtime.last_command_result = result.length() ? result : "none";
  g_runtime.last_command_at_ms = millis();
}

bool isPreviewActive() {
  return g_camera_preview_latched || (g_preview_until_ms != 0 && millis() < g_preview_until_ms);
}

bool isAutoCameraReason(const char* reason) {
  return reason != nullptr && strcmp(reason, "Auto") == 0;
}

bool ensureCameraReadyForCapture(bool is_auto) {
  if (CameraManager::state().ready) {
    return true;
  }

  const uint32_t now = millis();
  if (is_auto && now < g_next_camera_auto_retry_ms) {
    Serial.println("[Auto] Camera is offline; retry is rate-limited to keep dashboard smooth.");
    return false;
  }

  Serial.println("Camera requested while camera is offline. Retrying camera init...");
  CameraManager::init();
  if (CameraManager::state().ready) {
    return true;
  }

  g_next_camera_auto_retry_ms = now + kCameraOfflineAutoRetryMs;
  if (is_auto) {
    Serial.println("[Auto] Camera retry failed; dashboard will keep running.");
  }
  return false;
}

void drawDashboard() {
  DisplayManager::drawPage(g_current_page,
                           SensorManager::i2cState(),
                           SensorManager::mpuState(),
                           SensorManager::shtState(),
                           SensorManager::maxState(),
                           Air780EManager::state(),
                           CameraManager::state(),
                           Air780EManager::gnss(),
                           SensorManager::data(),
                           StorageManager::state(),
                           StorageManager::data(),
                           AudioManager::state(),
                           AudioManager::data(),
                           g_runtime);
}

void forceDrawDashboard(const char* reason) {
  DisplayManager::forceFullRefresh();
  drawDashboard();
  g_last_tft_full_refresh_ms = millis();
  Serial.printf("[TFT] forced full dashboard redraw: %s\n",
                reason && reason[0] ? reason : "manual");
}

bool captureAndPreviewLocalFrame(const char* reason) {
  const bool is_auto = isAutoCameraReason(reason);
  if (!ensureCameraReadyForCapture(is_auto)) {
    if (!is_auto) {
      DisplayManager::drawCameraCaptureFailed();
      g_preview_until_ms = millis() + kPreviewErrorDurationMs;
    }
    return false;
  }

  DisplayManager::drawCameraStatus("正在拍摄", "本地灰度预览", kUiCyan);
  CameraFrame frame;
  const bool ok = CameraManager::captureFrame(frame);
  Serial.printf("[CAM] preview trigger=%s capture=%s width=%u height=%u len=%lu format=%d persistence=disabled\n",
                reason ? reason : "Snapshot",
                ok ? "OK" : "FAIL",
                static_cast<unsigned>(frame.width),
                static_cast<unsigned>(frame.height),
                static_cast<unsigned long>(frame.len),
                frame.format);
  if (ok) {
    DisplayManager::drawCameraFrame(frame);
    CameraManager::releaseFrame(frame);
    Serial.println("[CAM] preview framebuffer_return=OK");
    g_camera_preview_latched = false;
    g_preview_until_ms = millis() + kPreviewDurationMs;
    g_camera_consecutive_failures = 0;
    return true;
  }

  DisplayManager::drawCameraCaptureFailed();
  CameraManager::releaseFrame(frame);
  Serial.println("[CAM] preview framebuffer_return=OK");
  g_camera_preview_latched = false;
  g_preview_until_ms = millis() + kPreviewErrorDurationMs;
  if (g_camera_consecutive_failures < 255) {
    ++g_camera_consecutive_failures;
    if (g_camera_consecutive_failures >= 3) {
      CameraManager::markLimited();
      Serial.println("[CAM] Camera Limited after 3 consecutive local preview failures; automatic retry remains disabled.");
    }
  }
  return false;
}

bool triggerCameraSnapshot(const char* source) {
  g_camera_preview_latched = false;
  Serial.printf("[%s] Local grayscale preview requested.\n", source ? source : "Preview");
  return captureAndPreviewLocalFrame(source);
}

void handleAutoCameraSnapshot(uint32_t now) {
  if (!kAutoSnapshotEnabled) {
    (void)now;
    return;
  }
  if (now - g_last_auto_snapshot_ms < autoSnapshotIntervalMs()) {
    return;
  }

  g_last_auto_snapshot_ms = now;
  if (isPreviewActive()) {
    Serial.println("[Auto] Camera snapshot skipped because preview is active.");
    return;
  }

  captureAndPreviewLocalFrame("Auto");
}

void initCameraShutterButton() {
  pinMode(Pins::Button::CAMERA_SHUTTER, INPUT_PULLUP);
  g_camera_shutter_last_raw = digitalRead(Pins::Button::CAMERA_SHUTTER);
  g_camera_shutter_stable = g_camera_shutter_last_raw;
  g_camera_shutter_last_change_ms = millis();
  Serial.printf("Camera shutter button: GPIO%d active-low raw=%s. Short press shows one local preview frame.\n",
                Pins::Button::CAMERA_SHUTTER,
                g_camera_shutter_last_raw == LOW ? "LOW" : "HIGH");
}

void printCameraShutterButton() {
  const int raw = digitalRead(Pins::Button::CAMERA_SHUTTER);
  Serial.printf("Camera shutter GPIO%d raw=%s stable=%s preview=%s timeout=%lu\n",
                Pins::Button::CAMERA_SHUTTER,
                raw == LOW ? "LOW(pressed)" : "HIGH(released)",
                g_camera_shutter_stable == LOW ? "LOW(pressed)" : "HIGH(released)",
                isPreviewActive() ? "active" : "idle",
                static_cast<unsigned long>(g_preview_until_ms));
}

void handleCameraShutterButton() {
  const uint32_t now = millis();
  const int raw = digitalRead(Pins::Button::CAMERA_SHUTTER);
  if (raw != g_camera_shutter_last_raw) {
    g_camera_shutter_last_raw = raw;
    g_camera_shutter_last_change_ms = now;
  }

  if (now - g_camera_shutter_last_change_ms < kButtonDebounceMs || raw == g_camera_shutter_stable) {
    return;
  }

  const int previous = g_camera_shutter_stable;
  g_camera_shutter_stable = raw;
  if (previous == HIGH && g_camera_shutter_stable == LOW) {
    Serial.printf("[Button] GPIO%d shutter pressed.\n", Pins::Button::CAMERA_SHUTTER);
    triggerCameraSnapshot("Button");
  }
}

void printSystemStatus() {
  Serial.println("Module states:");
  Serial.printf("  TFT: %s\n", DisplayManager::state().status.c_str());
  Serial.printf("  I2C: %s\n", SensorManager::i2cState().status.c_str());
  Serial.printf("  MPU: %s\n", SensorManager::mpuState().status.c_str());
  Serial.printf("  TMP117: %s\n", SensorManager::shtState().status.c_str());
  Serial.printf("  MAX3010x: %s\n", SensorManager::maxState().status.c_str());
  Serial.printf("  Air780E: %s\n", Air780EManager::state().status.c_str());
  Serial.printf("  Camera: %s\n", CameraManager::state().status.c_str());
  Serial.printf("  TF: %s\n", StorageManager::state().status.c_str());
  Serial.printf("  Audio: %s noise=%u event=%s\n",
                AudioManager::state().status.c_str(),
                static_cast<unsigned>(AudioManager::data().noise_level),
                AudioManager::data().noise_event.c_str());
  Serial.printf("  Mode: %s mqtt=%lums camera=%lums lastCommand=%s result=%s\n",
                g_runtime.mode.c_str(),
                static_cast<unsigned long>(mqttIntervalMs()),
                static_cast<unsigned long>(autoSnapshotIntervalMs()),
                g_runtime.last_command.c_str(),
                g_runtime.last_command_result.c_str());
  printCameraShutterButton();
  const GnssData& gnss = Air780EManager::gnss();
  Serial.printf("  GNSS: %s", gnss.has_fix ? "Fixed" : "Searching");
  if (gnss.has_fix) {
    Serial.printf(" lat=%.6f lng=%.6f", gnss.lat, gnss.lng);
  } else {
    Serial.printf(" default lat=%.6f lng=%.6f",
                  LocationDefaults::kZhengxinLat,
                  LocationDefaults::kZhengxinLng);
  }
  Serial.println();
  StorageManager::printStatus();
  AudioManager::printStatus();
}

void publishTelemetryNow() {
  const String json = TelemetryBuilder::buildJson(SensorManager::data(),
                                                 Air780EManager::gnss(),
                                                 Air780EManager::state(),
                                                 CameraManager::state(),
                                                 StorageManager::state(),
                                                 StorageManager::data(),
                                                 AudioManager::state(),
                                                 AudioManager::data(),
                                                 g_runtime);
  const bool ok = Air780EManager::publishTelemetry(json);
  StorageManager::appendTelemetry(TelemetryBuilder::sequence(),
                                  SensorManager::data(),
                                  Air780EManager::gnss(),
                                  Air780EManager::state(),
                                  CameraManager::state(),
                                  AudioManager::data());
  Serial.printf("MQTT publish: %s\n", ok ? "OK" : "FAIL");
  if (!isPreviewActive()) {
    drawDashboard();
  }
}

void handleCameraCommand() {
  Serial.println("[CAM] manual camera retry/status requested.");
  CameraManager::init();
  DisplayManager::drawCameraStatus("相机状态",
                                   CameraManager::state().status.c_str(),
                                   CameraManager::state().ready ? kUiCyan : kUiRed);
  g_preview_until_ms = millis() + kPreviewDurationMs;
}

void applyRuntimeMode(bool eco_mode) {
  g_runtime.eco_mode = eco_mode;
  g_runtime.mode = eco_mode ? "eco" : "normal";
  g_last_mqtt_ms = millis();
  g_last_auto_snapshot_ms = millis();
}

void handleRemoteCommand(const RemoteCommandEvent& event) {
  Serial.printf("[Control] command=%s raw=%s\n", event.code.c_str(), event.raw.c_str());
  switch (event.command) {
    case RemoteCommand::Beep:
      AudioManager::startRecallBeep();
      recordCommandResult(event.code, "ok");
      return;
    case RemoteCommand::LightOn:
      DisplayManager::drawLocatorFlash();
      AudioManager::startRecallBeep();
      g_camera_preview_latched = false;
      g_preview_until_ms = millis() + kLocatorFlashDurationMs;
      recordCommandResult(event.code, "ok");
      return;
    case RemoteCommand::EcoMode:
      applyRuntimeMode(true);
      AudioManager::startAckBeep();
      recordCommandResult(event.code, "ok");
      return;
    case RemoteCommand::NormalMode:
      applyRuntimeMode(false);
      AudioManager::startAckBeep();
      recordCommandResult(event.code, "ok");
      return;
    case RemoteCommand::Capture:
      {
        Serial.println("[Control] Capture ignored: 图像仅支持本地按键预览");
        DisplayManager::drawCameraStatus("图像仅支持", "本地按键预览", kUiOrange);
        g_preview_until_ms = millis() + kPreviewErrorDurationMs;
        recordCommandResult(event.code, "local_preview_only");
      }
      AudioManager::startAckBeep();
      return;
    case RemoteCommand::Status:
      SensorManager::read();
      publishTelemetryNow();
      recordCommandResult(event.code, "ok");
      return;
    case RemoteCommand::None:
      recordCommandResult(event.code, "ignored");
      return;
  }
}

void handleCameraDumpCommand() {
  Serial.println("CAMFRAME_DISABLED image upload/dump is disabled; use the local shutter preview.");
}

void handleCameraJpegCommand() {
  Serial.println("CAMJPEG_DISABLED JPEG capture/upload is disabled in final firmware.");
}

void handleTftCommand() {
  Serial.println("[TFT] drawing final INVON color diagnostic.");
  DisplayManager::drawDiagnostic();
  forceDrawDashboard("serial_tft");
}

void handleCommand(CommandConsole::Command command) {
  switch (command) {
    case CommandConsole::Command::None:
      return;
    case CommandConsole::Command::Help:
      CommandConsole::printHelp();
      return;
    case CommandConsole::Command::I2c:
      SensorManager::scanI2C();
      drawDashboard();
      return;
    case CommandConsole::Command::Sensors:
      SensorManager::read();
      drawDashboard();
      return;
    case CommandConsole::Command::Air:
      Air780EManager::queryStatus();
      Air780EManager::printStatus();
      drawDashboard();
      return;
    case CommandConsole::Command::Pub:
      SensorManager::read();
      publishTelemetryNow();
      return;
    case CommandConsole::Command::Cam:
      handleCameraCommand();
      return;
    case CommandConsole::Command::CamDump:
      handleCameraDumpCommand();
      return;
    case CommandConsole::Command::CamJpeg:
      handleCameraJpegCommand();
      return;
    case CommandConsole::Command::Button:
      printCameraShutterButton();
      return;
    case CommandConsole::Command::Tf:
      StorageManager::printStatus();
      StorageManager::listCameraIndex();
      drawDashboard();
      return;
    case CommandConsole::Command::Tft:
      handleTftCommand();
      return;
    case CommandConsole::Command::Snap:
      triggerCameraSnapshot("SerialSnap");
      return;
    case CommandConsole::Command::Status:
      printSystemStatus();
      return;
    case CommandConsole::Command::Unknown:
      Serial.printf("Unknown command: %s\n", CommandConsole::lastInput().c_str());
      CommandConsole::printHelp();
      return;
  }
}

void initSerial() {
  Serial.begin(Pins::kBaudRate);
  delay(1200);
  Serial.println();
  Serial.println("ESP32-S3 smart pet collar prototype bring-up");
  Serial.printf("Chip: %s, CPU: %u MHz, SDK: %s\n",
                ESP.getChipModel(),
                ESP.getCpuFreqMHz(),
                ESP.getSdkVersion());
  Pins::printPinMap();
}
}  // namespace

void setup() {
  initSerial();
  esp_log_level_set("gpio", ESP_LOG_WARN);
  g_runtime.mode = "normal";
  g_runtime.last_command = "none";
  g_runtime.last_command_result = "none";
  DisplayManager::init();
  g_last_tft_full_refresh_ms = millis();

  SensorManager::initI2C();
  CameraManager::init();
  // esp_camera_init() leaves the shared Arduino Wire bus in a state where
  // TMP117 requestFrom() can time out. Restore the shared I2C bus once after
  // camera init; frame capture does not use Arduino Wire.
  SensorManager::recoverI2CBus("camera init");
  Serial.println("[CAM] boot JPEG self-test disabled; image persistence disabled.");
  SensorManager::scanI2C();
  SensorManager::initAll();
  const uint32_t heart_warmup_start_ms = millis();
  while (millis() - heart_warmup_start_ms < 700) {
    SensorManager::pollHeartSensor();
    delay(20);
  }
  SensorManager::read();
  drawDashboard();
  Serial.println("[TFT] live dashboard drawn after sensor init.");

  StorageManager::init();
  AudioManager::init();
  AudioManager::poll();
  g_last_audio_sample_id = AudioManager::data().sample_id;
  drawDashboard();
  Serial.println("[TFT] dashboard refreshed after TF/audio init.");

  Air780EManager::init();
  SensorManager::read();
  drawDashboard();
  Serial.println("[TFT] dashboard refreshed after Air780E init.");

  SensorManager::read();
  drawDashboard();
  Serial.println("[TFT] dashboard refreshed after camera init.");

  initCameraShutterButton();
  publishTelemetryNow();
  g_last_mqtt_ms = millis();
  g_last_auto_snapshot_ms = millis();
  CommandConsole::init();
}

void loop() {
  handleCommand(CommandConsole::poll());
  handleCameraShutterButton();
  AudioManager::poll();
  const AudioData& audio = AudioManager::data();
  if (audio.sample_id != g_last_audio_sample_id) {
    g_last_audio_sample_id = audio.sample_id;
    if (!isPreviewActive() && g_current_page == DisplayManager::Page::Dashboard) {
      DisplayManager::drawAudioLevel(AudioManager::state(), audio);
    }
  }

  RemoteCommandEvent remote_event;
  if (Air780EManager::pollDownlink(remote_event)) {
    handleRemoteCommand(remote_event);
  }

  const uint32_t now = millis();
  if (now - g_last_heart_poll_ms >= kHeartPollIntervalMs) {
    g_last_heart_poll_ms = now;
    SensorManager::pollHeartSensor();
  }

  if (now - g_last_motion_poll_ms >= kMotionPollIntervalMs) {
    g_last_motion_poll_ms = now;
    SensorManager::pollMotionSensor();
  }

  if (!g_camera_preview_latched && g_preview_until_ms != 0 && now >= g_preview_until_ms) {
    g_preview_until_ms = 0;
    forceDrawDashboard("preview_timeout");
  }

  handleAutoCameraSnapshot(now);

  if (now - g_last_sensor_ms >= kSensorIntervalMs) {
    g_last_sensor_ms = now;
    SensorManager::read();
    if (!isPreviewActive()) {
      if (now - g_last_tft_full_refresh_ms >= kTftFullRefreshIntervalMs) {
        forceDrawDashboard("watchdog");
      } else {
        drawDashboard();
      }
    }
  }

  if (now - g_last_mqtt_ms >= mqttIntervalMs() && Air780EManager::canAttemptAutoPublish()) {
    g_last_mqtt_ms = now;
    publishTelemetryNow();
    if (!isPreviewActive()) {
      drawDashboard();
    }
  }

  delay(10);
}

#endif  // PSRAM_ONLY_DIAGNOSTIC
