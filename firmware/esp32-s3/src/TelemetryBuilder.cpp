#include "TelemetryBuilder.h"

#include <math.h>

namespace TelemetryBuilder {
namespace {
constexpr const char* kDeviceId = "CareGuardESP32S3";
uint32_t g_sequence = 0;

bool validNumber(float value) {
  return !isnan(value) && !isinf(value);
}

bool validNumber(double value) {
  return !isnan(value) && !isinf(value);
}

void appendComma(String& out, bool& first) {
  if (!first) {
    out += ',';
  }
  first = false;
}

void appendStringField(String& out, bool& first, const char* key, const String& value) {
  appendComma(out, first);
  out += '"';
  out += key;
  out += "\":\"";
  for (size_t i = 0; i < value.length(); ++i) {
    const char ch = value[i];
    if (ch == '"' || ch == '\\') {
      out += '\\';
    }
    out += ch;
  }
  out += '"';
}

void appendBoolField(String& out, bool& first, const char* key, bool value) {
  appendComma(out, first);
  out += '"';
  out += key;
  out += "\":";
  out += value ? "true" : "false";
}

void appendIntField(String& out, bool& first, const char* key, long value) {
  appendComma(out, first);
  out += '"';
  out += key;
  out += "\":";
  out += String(value);
}

void appendUIntField(String& out, bool& first, const char* key, unsigned long value) {
  appendComma(out, first);
  out += '"';
  out += key;
  out += "\":";
  out += String(value);
}

void appendFloatField(String& out, bool& first, const char* key, double value, uint8_t decimals) {
  if (!validNumber(value)) {
    return;
  }
  appendComma(out, first);
  out += '"';
  out += key;
  out += "\":";
  out += String(value, static_cast<unsigned int>(decimals));
}

}  // namespace

String buildJson(const SensorData& data,
                 const GnssData& gnss,
                 const ModuleState& air_state,
                 const ModuleState& camera_state,
                 const ModuleState& storage_state,
                 const StorageData& storage,
                 const ModuleState& audio_state,
                 const AudioData& audio,
                 const RuntimeStatus& runtime) {
  ++g_sequence;

  String out;
  out.reserve(760);
  out += '{';
  bool first = true;

  appendStringField(out, first, "id", kDeviceId);
  appendUIntField(out, first, "seq", static_cast<unsigned long>(g_sequence));
  appendUIntField(out, first, "ts", static_cast<unsigned long>(millis()));
  appendStringField(out, first, "sampleReason", "realtime_5s_publish");
  appendStringField(out, first, "state", data.motion);
  appendIntField(out, first, "hr", data.heart_bpm);
  appendStringField(out, first, "hrSampleType", data.heart_sample_type);
  appendFloatField(out, first, "temp", data.temperature_c, 1);
  appendBoolField(out, first, "isSimulator", false);

  if (gnss.has_fix && validNumber(gnss.lat) && validNumber(gnss.lng)) {
    appendFloatField(out, first, "lat", gnss.lat, 6);
    appendFloatField(out, first, "lng", gnss.lng, 6);
    appendBoolField(out, first, "hasGps", true);
    appendStringField(out, first, "locationSource", "gnss");
    appendStringField(out, first, "coordinateSystem", "wgs84");
  } else {
    appendFloatField(out, first, "lat", LocationDefaults::kZhengxinLat, 12);
    appendFloatField(out, first, "lng", LocationDefaults::kZhengxinLng, 12);
    appendBoolField(out, first, "hasGps", false);
    appendStringField(out, first, "locationSource", "default_zhengxin");
    appendStringField(out, first, "coordinateSystem", "gcj02");
  }

  appendFloatField(out, first, "humidity", data.humidity_percent, 1);
  appendIntField(out, first, "ir", static_cast<long>(data.ir));
  appendIntField(out, first, "red", static_cast<long>(data.red));
  appendBoolField(out, first, "heartContact", data.heart_contact);
  appendStringField(out, first, "motion", data.motion);
  appendFloatField(out, first, "motionScore", data.motion_score, 3);
  appendUIntField(out, first, "steps", static_cast<unsigned long>(data.step_count));
  appendFloatField(out, first, "cadence", data.cadence_spm, 1);
  appendStringField(out, first, "air", air_state.status);
  appendStringField(out, first, "camera", camera_state.status);
  appendStringField(out, first, "tf", storage_state.status);
  appendBoolField(out, first, "tfReady", storage_state.ready);
  appendUIntField(out, first, "tfRows", static_cast<unsigned long>(storage.telemetry_rows));
  appendBoolField(out, first, "imagePersistence", false);
  appendStringField(out, first, "cameraPreview", "local-only");
  appendStringField(out, first, "audio", audio_state.status);
  appendUIntField(out, first, "noiseLevel", audio.noise_level);
  appendStringField(out, first, "noiseEvent", audio.noise_event);
  appendStringField(out, first, "mode", runtime.mode);
  appendStringField(out, first, "lastCommand", runtime.last_command);
  appendStringField(out, first, "lastCommandResult", runtime.last_command_result);
  appendUIntField(out, first, "lastCommandAt", static_cast<unsigned long>(runtime.last_command_at_ms));
  out += '}';
  return out;
}

uint32_t sequence() {
  return g_sequence;
}
}  // namespace TelemetryBuilder
