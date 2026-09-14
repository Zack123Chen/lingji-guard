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
constexpr const char* kCameraDir = "/careguard/camera";
constexpr const char* kTelemetryPath = "/careguard/telemetry.csv";
constexpr const char* kPhotoIndexPath = "/careguard/camera/index.csv";
constexpr int kSdMmcFrequency = 4000;
constexpr int kPixformatJpeg = 4;
constexpr time_t kMinValidUnixTime = 1577836800;  // 2020-01-01
constexpr uint32_t kMountRetryIntervalMs = 5000;

ModuleState g_storage_state;
StorageData g_storage_data;
uint32_t g_photo_sequence = 0;
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

String timestampLabel() {
  setCompileTimeClockIfNeeded();
  const time_t now = time(nullptr);
  if (!validUnixTime(now)) {
    return String(static_cast<unsigned long>(millis()));
  }

  tm broken = {};
  localtime_r(&now, &broken);
  char buffer[24] = {};
  strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &broken);
  return String(buffer);
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
  if (SD_MMC.exists(kTelemetryPath)) {
    return;
  }
  File file = SD_MMC.open(kTelemetryPath, FILE_WRITE);
  if (!file) {
    markStorageOffline("TF CSV Fail");
    return;
  }
  file.println("millis,seq,state,hr,temp,ir,red,heartContact,motionScore,steps,cadence,lat,lng,gnssFix,air,camera,noiseLevel,noiseEvent,tfStatus");
  file.close();
  applyFileTimestamp(kTelemetryPath);
}

void ensurePhotoIndexHeader() {
  if (SD_MMC.exists(kPhotoIndexPath)) {
    return;
  }
  File file = SD_MMC.open(kPhotoIndexPath, FILE_WRITE);
  if (!file) {
    markStorageOffline("TF Index Fail");
    return;
  }
  file.println("millis,seq,reason,path,format,width,height,len,written,status");
  file.close();
  applyFileTimestamp(kPhotoIndexPath);
}

bool ensureMounted() {
  if (!g_storage_data.mounted) {
    g_storage_state.ready = false;
    g_storage_state.status = g_storage_data.card_present ? "TF Mount Fail" : "TF Missing";
    g_storage_data.last_status = g_storage_state.status;
    return false;
  }
  return true;
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
      SD_MMC.mkdir(kRootDir);
      SD_MMC.mkdir(kCameraDir);
      ensureTelemetryHeader();
      if (!g_storage_data.mounted) {
        Serial.printf("TF mounted but telemetry header write failed; storage disabled: %s\n",
                      g_storage_state.status.c_str());
        return false;
      }
      ensurePhotoIndexHeader();
      if (!g_storage_data.mounted) {
        Serial.printf("TF mounted but photo index write failed; storage disabled: %s\n",
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

String photoFormatName(int format) {
  if (format == kPixformatJpeg) {
    return "jpg";
  }
  return "raw";
}

String makePhotoPath(uint32_t seq, const char* reason, const String& format) {
  String safe_reason = String(reason && reason[0] ? reason : "snap");
  safe_reason.toLowerCase();
  for (size_t i = 0; i < safe_reason.length(); ++i) {
    const char ch = safe_reason[i];
    if (!isalnum(ch) && ch != '_' && ch != '-') {
      safe_reason.setCharAt(i, '_');
    }
  }

  String path = kCameraDir;
  path += "/IMG_";
  path += String(static_cast<unsigned long>(seq));
  path += "_";
  path += timestampLabel();
  path += "_";
  path += String(static_cast<unsigned long>(millis()));
  path += "_";
  path += safe_reason;
  path += ".";
  path += format;
  return path;
}

void appendPhotoIndex(uint32_t seq, const char* reason, const PhotoStatus& photo) {
  if (!ensureMounted()) {
    return;
  }
  ensurePhotoIndexHeader();
  File file = SD_MMC.open(kPhotoIndexPath, FILE_APPEND);
  if (!file) {
    markStorageOffline("TF Index Fail");
    return;
  }

  file.print(static_cast<unsigned long>(millis()));
  file.print(',');
  file.print(static_cast<unsigned long>(seq));
  file.print(',');
  file.print(csvText(String(reason && reason[0] ? reason : "snap")));
  file.print(',');
  file.print(csvText(photo.path));
  file.print(',');
  file.print(csvText(photo.format));
  file.print(',');
  file.print(static_cast<unsigned>(photo.width));
  file.print(',');
  file.print(static_cast<unsigned>(photo.height));
  file.print(',');
  file.print(static_cast<unsigned long>(photo.len));
  file.print(',');
  file.print(static_cast<unsigned long>(photo.written));
  file.print(',');
  file.println(csvText(photo.status));
  file.close();
  applyFileTimestamp(kPhotoIndexPath);
}

bool jpegHasSoi(const CameraFrame& frame) {
  return frame.len >= 2 && frame.data != nullptr &&
         frame.data[0] == 0xFF && frame.data[1] == 0xD8;
}

bool jpegHasEoi(const CameraFrame& frame) {
  return frame.len >= 2 && frame.data != nullptr &&
         frame.data[frame.len - 2] == 0xFF && frame.data[frame.len - 1] == 0xD9;
}

bool frameLooksLikeJpeg(const CameraFrame& frame) {
  return frame.format == kPixformatJpeg &&
         frame.data != nullptr &&
         frame.len >= 512 &&
         jpegHasSoi(frame) &&
         jpegHasEoi(frame);
}

bool saveFrameToFile(const CameraFrame& frame, const char* reason, PhotoStatus& photo) {
  photo = {};
  photo.width = frame.width;
  photo.height = frame.height;
  photo.format = photoFormatName(frame.format);

  if (!ensureMountedOrRetry("photo")) {
    photo.status = g_storage_state.status;
    return false;
  }
  if (frame.data == nullptr || frame.len == 0 || frame.width == 0 || frame.height == 0) {
    photo.status = "Empty frame";
    g_storage_data.last_status = photo.status;
    return false;
  }
  if (!frameLooksLikeJpeg(frame)) {
    photo.status = "JPEG check failed";
    g_storage_data.last_status = photo.status;
    Serial.printf("[CAM] JPEG %ux%u len=%u soi=%s eoi=%s\n",
                  static_cast<unsigned>(frame.width),
                  static_cast<unsigned>(frame.height),
                  static_cast<unsigned>(frame.len),
                  jpegHasSoi(frame) ? "OK" : "BAD",
                  jpegHasEoi(frame) ? "OK" : "BAD");
    return false;
  }

  const uint32_t seq = ++g_photo_sequence;
  photo.path = makePhotoPath(seq, reason, photo.format);
  File file = SD_MMC.open(photo.path.c_str(), FILE_WRITE);
  if (!file) {
    photo.status = "Open failed";
    markStorageOffline("TF Photo Fail");
    appendPhotoIndex(seq, reason, photo);
    return false;
  }

  size_t written = 0;
  written = file.write(frame.data, frame.len);
  const bool write_ok = written == frame.len;
  file.close();
  applyFileTimestamp(photo.path);
  photo.len = frame.len;
  photo.written = written;
  photo.saved = write_ok;
  photo.status = photo.saved ? "Saved" : "Short write";
  Serial.printf("[CAM] save=%s bytes=%u written=%u result=%s\n",
                photo.path.c_str(),
                static_cast<unsigned>(frame.len),
                static_cast<unsigned>(written),
                photo.saved ? "OK" : "FAIL");
  if (photo.saved) {
    ++g_storage_data.photo_count;
    g_storage_data.last_photo_path = photo.path;
    g_storage_data.last_photo_format = photo.format;
    g_storage_state.ready = true;
    g_storage_state.status = "TF Photo OK";
  } else {
    g_storage_state.status = "TF Photo Fail";
  }
  g_storage_data.last_status = photo.status;
  appendPhotoIndex(seq, reason, photo);
  return photo.saved;
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
  File file = SD_MMC.open(kTelemetryPath, FILE_APPEND);
  if (!file) {
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
  applyFileTimestamp(kTelemetryPath);

  ++g_storage_data.telemetry_rows;
  g_storage_state.ready = true;
  g_storage_state.status = "TF Log OK";
  g_storage_data.last_status = "Telemetry saved";
  return true;
}

bool captureAndSaveJpeg(const char* reason, PhotoStatus& photo) {
  if (!ensureMountedOrRetry("capture")) {
    photo.status = g_storage_state.status;
    return false;
  }

  CameraFrame frame;
  if (!CameraManager::captureJpegFrame(frame)) {
    photo = {};
    photo.status = "JPEG capture failed";
    g_storage_data.last_status = photo.status;
    return false;
  }

  const bool ok = saveFrameToFile(frame, reason, photo);
  CameraManager::releaseFrame(frame);
  return ok;
}

bool saveCameraFrame(const CameraFrame& frame, const char* reason, PhotoStatus& photo) {
  return saveFrameToFile(frame, reason, photo);
}

void printStatus() {
  ensureMountedOrRetry("status");
  g_storage_data.card_present = detectCardPresent() || g_storage_data.mounted;
  Serial.printf("TF status: %s | present=%s mounted=%s rows=%lu photos=%lu last=%s\n",
                g_storage_state.status.c_str(),
                g_storage_data.card_present ? "yes" : "no",
                g_storage_data.mounted ? "yes" : "no",
                static_cast<unsigned long>(g_storage_data.telemetry_rows),
                static_cast<unsigned long>(g_storage_data.photo_count),
                g_storage_data.last_photo_path.c_str());
}

void listCameraIndex() {
  if (!ensureMountedOrRetry("index")) {
    printStatus();
    return;
  }
  File file = SD_MMC.open(kPhotoIndexPath, FILE_READ);
  if (!file) {
    Serial.println("TF camera index is not available.");
    return;
  }
  Serial.println("TF camera index tail:");
  uint16_t printed = 0;
  while (file.available() && printed < 30) {
    Serial.write(file.read());
    ++printed;
  }
  if (file.available()) {
    Serial.println("\n... index continues on TF card");
  }
  file.close();
}

const ModuleState& state() {
  return g_storage_state;
}

const StorageData& data() {
  return g_storage_data;
}
}  // namespace StorageManager
