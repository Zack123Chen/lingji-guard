#include "StorageManager.h"

#include <FS.h>
#include <SD_MMC.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <utime.h>

#include "CameraManager.h"
#include "Pins.h"

namespace StorageManager {
namespace {
constexpr const char* kSdMountPoint = "/sdcard";
constexpr const char* kRootDir = "/careguard";
constexpr const char* kTelemetryPath = "/careguard/telemetry.csv";
constexpr const char* kTelemetryFallbackPath = "/telemetry.csv";
constexpr int kSdMmcFrequency = 4000;
constexpr time_t kMinValidUnixTime = 1577836800;  // 2020-01-01
constexpr uint32_t kMountRetryIntervalMs = 5000;

ModuleState g_storage_state;
StorageData g_storage_data;
const char* g_telemetry_path = kTelemetryPath;
uint32_t g_next_mount_retry_ms = 0;

int monthIndex(const char* month) {
  static const char* const months[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
  };
  for (int i = 0; i < 12; ++i) {
    if (strncmp(month, months[i], 3) == 0) {
      return i;
    }
  }
  return -1;
}

bool validUnixTime(time_t value) {
  return value >= kMinValidUnixTime;
}

void setCompileTimeClockIfNeeded() {
  if (validUnixTime(time(nullptr))) {
    return;
  }

  char month_text[4] = {};
  int day = 0;
  int year = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  if (sscanf(__DATE__, "%3s %d %d", month_text, &day, &year) != 3 ||
      sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3) {
    return;
  }

  const int month = monthIndex(month_text);
  if (month < 0) {
    return;
  }

  tm build_time = {};
  build_time.tm_year = year - 1900;
  build_time.tm_mon = month;
  build_time.tm_mday = day;
  build_time.tm_hour = hour;
  build_time.tm_min = minute;
  build_time.tm_sec = second;
  build_time.tm_isdst = -1;

  const time_t build_epoch = mktime(&build_time);
  if (!validUnixTime(build_epoch)) {
    return;
  }

  timeval tv = {};
  tv.tv_sec = build_epoch;
  settimeofday(&tv, nullptr);
  Serial.printf("TF clock seeded from firmware build time: %ld\n",
                static_cast<long>(build_epoch));
}

String vfsPathFor(const String& path) {
  if (path.startsWith(kSdMountPoint)) {
    return path;
  }
  String vfs_path = kSdMountPoint;
  if (!path.startsWith("/")) {
    vfs_path += "/";
  }
  vfs_path += path;
  return vfs_path;
}

void applyFileTimestamp(const String& path) {
  setCompileTimeClockIfNeeded();
  const time_t now = time(nullptr);
  if (!validUnixTime(now)) {
    return;
  }

  const String vfs_path = vfsPathFor(path);
  struct utimbuf times = {};
  times.actime = now;
  times.modtime = now;
  if (utime(vfs_path.c_str(), &times) != 0) {
    Serial.printf("TF utime failed for %s\n", vfs_path.c_str());
  }
}

void markStorageOffline(const char* status) {
  SD_MMC.end();
  g_storage_data.mounted = false;
  g_storage_state.ready = false;
  g_storage_state.status = status;
  g_storage_data.last_status = status;
}

bool validNumber(double value) {
  return !isnan(value) && !isinf(value);
}

bool detectCardPresent() {
  return digitalRead(Pins::Storage::CD_IN) == LOW;
}

String fixNumber(double value, uint8_t decimals) {
  if (!validNumber(value)) {
    return "";
  }
  return String(value, static_cast<unsigned int>(decimals));
}

String csvText(const String& value) {
  String escaped = "\"";
  for (size_t i = 0; i < value.length(); ++i) {
    const char ch = value[i];
    if (ch == '"') {
      escaped += "\"\"";
    } else if (ch == '\r' || ch == '\n') {
      escaped += ' ';
    } else {
      escaped += ch;
    }
  }
  escaped += '"';
  return escaped;
}

void ensureTelemetryHeader() {
  if (SD_MMC.exists(g_telemetry_path)) {
    return;
  }
  File file = SD_MMC.open(g_telemetry_path, FILE_WRITE);
  if (!file) {
    Serial.printf("TF telemetry header open failed: path=%s\n", g_telemetry_path);
    markStorageOffline("TF CSV Fail");
    return;
  }
  file.println("millis,seq,state,hr,temp,ir,red,heartContact,motionScore,steps,cadence,lat,lng,gnssFix,air,camera,noiseLevel,noiseEvent,tfStatus");
  file.close();
  applyFileTimestamp(g_telemetry_path);
}

bool mountTfCard(uint8_t attempts, const char* reason) {
  SD_MMC.end();
  g_storage_data.mounted = false;
  g_storage_data.card_present = detectCardPresent();

  for (uint8_t attempt = 1; attempt <= attempts; ++attempt) {
    if (!SD_MMC.setPins(Pins::Storage::CLK, Pins::Storage::CMD, Pins::Storage::DATA0)) {
      g_storage_state.ready = false;
      g_storage_state.status = "TF Pins Fail";
      g_storage_data.last_status = g_storage_state.status;
      Serial.println("TF SD_MMC setPins failed.");
      return false;
    }

    if (SD_MMC.begin(kSdMountPoint, true, false, kSdMmcFrequency, 5)) {
      g_storage_data.mounted = true;
      g_storage_data.card_present = true;
      const bool root_exists_before = SD_MMC.exists(kRootDir);
      const bool root_mkdir = root_exists_before || SD_MMC.mkdir(kRootDir);
      Serial.printf("TF root dir: path=%s exists_before=%s mkdir_or_exists=%s exists_after=%s\n",
                    kRootDir,
                    root_exists_before ? "yes" : "no",
                    root_mkdir ? "yes" : "no",
                    SD_MMC.exists(kRootDir) ? "yes" : "no");
      g_telemetry_path = root_mkdir ? kTelemetryPath : kTelemetryFallbackPath;
      if (!root_mkdir) {
        Serial.printf("TF telemetry path fallback: %s\n", g_telemetry_path);
      }
      ensureTelemetryHeader();
      if (!g_storage_data.mounted) {
        Serial.printf("TF mounted but telemetry header write failed; storage disabled: %s\n",
                      g_storage_state.status.c_str());
        return false;
      }
      g_storage_state.ready = true;
      g_storage_state.status = "TF OK";
      g_storage_data.last_status = "Mounted";
      Serial.printf("TF mounted (%s): size=%lluMB used=%lluMB\n",
                    reason && reason[0] ? reason : "mount",
                    SD_MMC.cardSize() / (1024ULL * 1024ULL),
                    SD_MMC.usedBytes() / (1024ULL * 1024ULL));
      return true;
    }

    Serial.printf("TF mount attempt %u/%u failed (%s), CD GPIO%d=%s.\n",
                  static_cast<unsigned>(attempt),
                  static_cast<unsigned>(attempts),
                  reason && reason[0] ? reason : "mount",
                  Pins::Storage::CD_IN,
                  g_storage_data.card_present ? "LOW" : "HIGH");
    delay(80);
  }

  g_storage_state.ready = false;
  g_storage_state.status = g_storage_data.card_present ? "TF Mount Fail" : "TF Missing";
  g_storage_data.last_status = g_storage_state.status;
  g_next_mount_retry_ms = millis() + kMountRetryIntervalMs;
  return false;
}

bool ensureMountedOrRetry(const char* reason) {
  if (g_storage_data.mounted) {
    return true;
  }

  const uint32_t now = millis();
  if (now < g_next_mount_retry_ms) {
    g_storage_state.ready = false;
    g_storage_state.status = g_storage_data.card_present ? "TF Mount Fail" : "TF Missing";
    g_storage_data.last_status = g_storage_state.status;
    return false;
  }

  return mountTfCard(1, reason);
}

bool hasImageExtension(const String& name) {
  const int dot = name.lastIndexOf('.');
  if (dot < 0) {
    return false;
  }
  String ext = name.substring(dot + 1);
  ext.toLowerCase();
  return ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "pgm" || ext == "png";
}

uint32_t countImageFiles(File dir) {
  if (!dir || !dir.isDirectory()) {
    return 0;
  }

  uint32_t count = 0;
  File entry = dir.openNextFile();
  while (entry) {
    if (entry.isDirectory()) {
      count += countImageFiles(entry);
    } else if (hasImageExtension(String(entry.name()))) {
      ++count;
    }
    entry.close();
    entry = dir.openNextFile();
  }
  return count;
}

}  // namespace

void init() {
  setCompileTimeClockIfNeeded();
  pinMode(Pins::Storage::CD_IN, INPUT_PULLUP);
  delay(30);
  g_storage_data.card_present = detectCardPresent();

  Serial.printf("TF card detect GPIO%d raw=%s (LOW means inserted candidate).\n",
                Pins::Storage::CD_IN,
                g_storage_data.card_present ? "LOW" : "HIGH");

  mountTfCard(3, "boot");
}

bool appendTelemetry(uint32_t seq,
                     const SensorData& data,
                     const GnssData& gnss,
                     const ModuleState& air_state,
                     const ModuleState& camera_state,
                     const AudioData& audio) {
  if (!ensureMountedOrRetry("telemetry")) {
    return false;
  }

  ensureTelemetryHeader();
  File file = SD_MMC.open(g_telemetry_path, FILE_APPEND);
  if (!file) {
    Serial.printf("TF telemetry append open failed: path=%s\n", g_telemetry_path);
    markStorageOffline("TF CSV Fail");
    return false;
  }

  const bool gnss_fix = gnss.has_fix && validNumber(gnss.lat) && validNumber(gnss.lng);
  const double lat = gnss_fix ? gnss.lat : LocationDefaults::kZhengxinLat;
  const double lng = gnss_fix ? gnss.lng : LocationDefaults::kZhengxinLng;
  file.print(static_cast<unsigned long>(millis()));
  file.print(',');
  file.print(static_cast<unsigned long>(seq));
  file.print(',');
  file.print(csvText(data.motion));
  file.print(',');
  file.print(data.heart_bpm);
  file.print(',');
  file.print(fixNumber(data.temperature_c, 2));
  file.print(',');
  file.print(static_cast<unsigned long>(data.ir));
  file.print(',');
  file.print(static_cast<unsigned long>(data.red));
  file.print(',');
  file.print(data.heart_contact ? "true" : "false");
  file.print(',');
  file.print(fixNumber(data.motion_score, 4));
  file.print(',');
  file.print(static_cast<unsigned long>(data.step_count));
  file.print(',');
  file.print(fixNumber(data.cadence_spm, 1));
  file.print(',');
  file.print(fixNumber(lat, 8));
  file.print(',');
  file.print(fixNumber(lng, 8));
  file.print(',');
  file.print(gnss_fix ? "true" : "false");
  file.print(',');
  file.print(csvText(air_state.status));
  file.print(',');
  file.print(csvText(camera_state.status));
  file.print(',');
  file.print(static_cast<unsigned>(audio.noise_level));
  file.print(',');
  file.print(csvText(audio.noise_event));
  file.print(',');
  file.println(csvText(g_storage_state.status));
  file.close();
  applyFileTimestamp(g_telemetry_path);

  ++g_storage_data.telemetry_rows;
  g_storage_state.ready = true;
  g_storage_state.status = "TF Log OK";
  g_storage_data.last_status = "Telemetry saved";
  return true;
}

bool captureAndSaveJpeg(const char* reason, PhotoStatus& photo) {
  (void)reason;
  photo = {};
  photo.status = "Image persistence disabled";
  g_storage_data.last_status = photo.status;
  Serial.println("[TF] image persistence disabled; no JPEG/BMP/PGM will be written.");
  return false;
}

bool saveCameraFrame(const CameraFrame& frame, const char* reason, PhotoStatus& photo) {
  (void)frame;
  (void)reason;
  photo = {};
  photo.status = "Image persistence disabled";
  g_storage_data.last_status = photo.status;
  Serial.println("[TF] saveCameraFrame disabled; local preview only.");
  return false;
}

void printStatus() {
  ensureMountedOrRetry("status");
  g_storage_data.card_present = detectCardPresent() || g_storage_data.mounted;
  Serial.printf("TF status: %s | present=%s mounted=%s rows=%lu image_persistence=disabled\n",
                g_storage_state.status.c_str(),
                g_storage_data.card_present ? "yes" : "no",
                g_storage_data.mounted ? "yes" : "no",
                static_cast<unsigned long>(g_storage_data.telemetry_rows));
  if (g_storage_data.mounted) {
    File root = SD_MMC.open(kRootDir);
    const uint32_t image_count = countImageFiles(root);
    if (root) {
      root.close();
    }
    Serial.printf("TF image scan: root=%s image_files=%lu telemetry_exists=%s\n",
                  kRootDir,
                  static_cast<unsigned long>(image_count),
                  SD_MMC.exists(g_telemetry_path) ? "yes" : "no");
    Serial.printf("TF telemetry path: %s\n", g_telemetry_path);
  }
}

void listCameraIndex() {
  Serial.println("TF camera index disabled: image persistence is disabled.");
}

const ModuleState& state() {
  return g_storage_state;
}

const StorageData& data() {
  return g_storage_data;
}
}  // namespace StorageManager
