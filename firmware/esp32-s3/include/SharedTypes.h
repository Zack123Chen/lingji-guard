#pragma once

#include <Arduino.h>

struct ModuleState {
  bool ready = false;
  String status = "Not started";
};

namespace LocationDefaults {
constexpr double kZhengxinLat = 45.74303224082512;
constexpr double kZhengxinLng = 126.6314330493297;
}  // namespace LocationDefaults

struct SensorData {
  float ax = NAN;
  float ay = NAN;
  float az = NAN;
  float gx = NAN;
  float gy = NAN;
  float gz = NAN;
  float temperature_c = NAN;
  float humidity_percent = NAN;
  uint32_t ir = 0;
  uint32_t red = 0;
  bool heart_contact = false;
  int heart_bpm = 0;
  String heart_sample_type = "no_contact";
  String motion = "unknown";
  float motion_score = NAN;
  uint32_t step_count = 0;
  float cadence_spm = 0.0f;
};

struct GnssData {
  bool has_fix = false;
  double lat = NAN;
  double lng = NAN;
};

struct StorageData {
  bool card_present = false;
  bool mounted = false;
  uint32_t telemetry_rows = 0;
  uint32_t photo_count = 0;
  String last_photo_path = "";
  String last_photo_format = "";
  String last_status = "Not started";
};

struct AudioData {
  uint8_t noise_level = 0;
  String noise_event = "quiet";
  float raw_mean = NAN;
  float raw_rms = NAN;
  uint16_t raw_peak = 0;
  float noise_floor = NAN;
  uint32_t sample_id = 0;
};

struct PhotoStatus {
  bool saved = false;
  String path = "";
  String format = "";
  size_t len = 0;
  size_t written = 0;
  uint16_t width = 0;
  uint16_t height = 0;
  String status = "No photo";
};

struct RuntimeStatus {
  bool eco_mode = false;
  String mode = "normal";
  String last_command = "none";
  String last_command_result = "none";
  uint32_t last_command_at_ms = 0;
};

enum class RemoteCommand {
  None,
  Beep,
  LightOn,
  EcoMode,
  NormalMode,
  Capture,
  Status,
};

struct RemoteCommandEvent {
  RemoteCommand command = RemoteCommand::None;
  String code = "";
  String raw = "";
  uint32_t received_at_ms = 0;
};

struct CameraFrame {
  uint16_t width = 0;
  uint16_t height = 0;
  size_t len = 0;
  int format = -1;
  uint8_t* data = nullptr;
  void* handle = nullptr;
};
