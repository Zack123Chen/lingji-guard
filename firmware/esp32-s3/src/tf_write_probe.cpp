#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(TF_PROBE_WITH_CAMERA)
#define sensor_t camera_sensor_t
#include "esp_camera.h"
#undef sensor_t
#include "SensorManager.h"
#endif

#ifndef TF_PROBE_FREQ_KHZ
#define TF_PROBE_FREQ_KHZ 4000
#endif

#if defined(TF_PROBE_SHORT_83_NAMES)
#define TF_PROBE_NAME_MODE "8.3"
#else
#define TF_PROBE_NAME_MODE "long"
#endif

namespace {
constexpr int kSdClk = 16;
constexpr int kSdCmd = 21;
constexpr int kSdData0 = 15;
constexpr uint32_t kBaud = 115200;
constexpr const char* kMountPoint = "/sdcard";
constexpr const char* kPayload = "careguard-tf-write-probe-v1\n";

void printErrno(const char* label) {
  const int e = errno;
  Serial.printf("%s errno=%d strerror=%s\n", label, e, strerror(e));
}

const char* cardTypeName(uint8_t type) {
  switch (type) {
    case CARD_NONE:
      return "NONE";
    case CARD_MMC:
      return "MMC";
    case CARD_SD:
      return "SD";
    case CARD_SDHC:
      return "SDHC";
    default:
      return "UNKNOWN";
  }
}

#if defined(TF_PROBE_WITH_CAMERA)
camera_config_t makeCameraConfig() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 34;
  config.pin_d1 = 48;
  config.pin_d2 = 47;
  config.pin_d3 = 33;
  config.pin_d4 = 35;
  config.pin_d5 = 37;
  config.pin_d6 = 38;
  config.pin_d7 = 39;
  config.pin_xclk = -1;
  config.pin_pclk = 36;
  config.pin_vsync = 41;
  config.pin_href = 40;
  config.pin_sccb_sda = 10;
  config.pin_sccb_scl = 9;
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QQVGA;
  config.fb_location = CAMERA_FB_IN_DRAM;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.jpeg_quality = 12;
  return config;
}

void initCameraForControlledProbe() {
  Serial.println("TF_PROBE camera_step=esp_camera_init begin");
  const camera_config_t config = makeCameraConfig();
  const esp_err_t err = esp_camera_init(&config);
  Serial.printf("TF_PROBE camera_step=esp_camera_init result=0x%x (%s)\n",
                err,
                esp_err_to_name(err));
  SensorManager::recoverI2CBus("camera init");
}
#endif

bool runPosixProbe(uint32_t suffix) {
  char file_path[96] = {};
  char dir_path[96] = {};
#if defined(TF_PROBE_SHORT_83_NAMES)
  const unsigned id = static_cast<unsigned>(suffix % 100000UL);
  snprintf(file_path, sizeof(file_path), "%s/CGP%05u.TXT", kMountPoint, id);
  snprintf(dir_path, sizeof(dir_path), "%s/CGD%05u", kMountPoint, id);
#else
  snprintf(file_path, sizeof(file_path), "%s/.cg_tf_probe_%lu.txt", kMountPoint, static_cast<unsigned long>(suffix));
  snprintf(dir_path, sizeof(dir_path), "%s/.cg_tf_probe_dir_%lu", kMountPoint, static_cast<unsigned long>(suffix));
#endif

  Serial.printf("TF_PROBE_POSIX dir=%s file=%s\n", dir_path, file_path);

  errno = 0;
  const int mkdir_ret = mkdir(dir_path, 0777);
  Serial.printf("TF_PROBE_POSIX mkdir_ret=%d\n", mkdir_ret);
  if (mkdir_ret != 0) {
    printErrno("TF_PROBE_POSIX mkdir_fail");
    return false;
  }

  errno = 0;
  const int fd = open(file_path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
  Serial.printf("TF_PROBE_POSIX open_write_fd=%d\n", fd);
  if (fd < 0) {
    printErrno("TF_PROBE_POSIX open_write_fail");
    rmdir(dir_path);
    return false;
  }

  errno = 0;
  const ssize_t written = write(fd, kPayload, strlen(kPayload));
  Serial.printf("TF_PROBE_POSIX write_ret=%ld expected=%u\n",
                static_cast<long>(written),
                static_cast<unsigned>(strlen(kPayload)));
  if (written < 0 || static_cast<size_t>(written) != strlen(kPayload)) {
    printErrno("TF_PROBE_POSIX write_fail");
    close(fd);
    unlink(file_path);
    rmdir(dir_path);
    return false;
  }

  errno = 0;
  const int fsync_ret = fsync(fd);
  Serial.printf("TF_PROBE_POSIX fsync_ret=%d\n", fsync_ret);
  if (fsync_ret != 0) {
    printErrno("TF_PROBE_POSIX fsync_fail");
    close(fd);
    unlink(file_path);
    rmdir(dir_path);
    return false;
  }

  errno = 0;
  const int close_ret = close(fd);
  Serial.printf("TF_PROBE_POSIX close_write_ret=%d\n", close_ret);
  if (close_ret != 0) {
    printErrno("TF_PROBE_POSIX close_write_fail");
    unlink(file_path);
    rmdir(dir_path);
    return false;
  }

  errno = 0;
  const int read_fd = open(file_path, O_RDONLY);
  Serial.printf("TF_PROBE_POSIX open_read_fd=%d\n", read_fd);
  if (read_fd < 0) {
    printErrno("TF_PROBE_POSIX open_read_fail");
    unlink(file_path);
    rmdir(dir_path);
    return false;
  }

  char buffer[64] = {};
  errno = 0;
  const ssize_t read_len = read(read_fd, buffer, sizeof(buffer) - 1);
  Serial.printf("TF_PROBE_POSIX read_ret=%ld expected=%u\n",
                static_cast<long>(read_len),
                static_cast<unsigned>(strlen(kPayload)));
  if (read_len < 0) {
    printErrno("TF_PROBE_POSIX read_fail");
    close(read_fd);
    unlink(file_path);
    rmdir(dir_path);
    return false;
  }
  const bool match = static_cast<size_t>(read_len) == strlen(kPayload) &&
                     memcmp(buffer, kPayload, strlen(kPayload)) == 0;
  Serial.printf("TF_PROBE_POSIX compare=%s data=%s\n", match ? "OK" : "BAD", buffer);

  errno = 0;
  const int close_read_ret = close(read_fd);
  Serial.printf("TF_PROBE_POSIX close_read_ret=%d\n", close_read_ret);
  if (close_read_ret != 0) {
    printErrno("TF_PROBE_POSIX close_read_fail");
  }

  errno = 0;
  const int unlink_ret = unlink(file_path);
  Serial.printf("TF_PROBE_POSIX unlink_ret=%d\n", unlink_ret);
  if (unlink_ret != 0) {
    printErrno("TF_PROBE_POSIX unlink_fail");
  }

  errno = 0;
  const int rmdir_ret = rmdir(dir_path);
  Serial.printf("TF_PROBE_POSIX rmdir_ret=%d\n", rmdir_ret);
  if (rmdir_ret != 0) {
    printErrno("TF_PROBE_POSIX rmdir_fail");
  }

  return match && close_read_ret == 0 && unlink_ret == 0 && rmdir_ret == 0;
}

bool runArduinoProbe(uint32_t suffix) {
  char file_path[72] = {};
#if defined(TF_PROBE_SHORT_83_NAMES)
  const unsigned id = static_cast<unsigned>(suffix % 100000UL);
  snprintf(file_path, sizeof(file_path), "/CGA%05u.TXT", id);
#else
  snprintf(file_path, sizeof(file_path), "/.cg_tf_probe_arduino_%lu.txt", static_cast<unsigned long>(suffix));
#endif
  Serial.printf("TF_PROBE_ARDUINO file=%s\n", file_path);

  errno = 0;
  File out = SD_MMC.open(file_path, FILE_WRITE);
  Serial.printf("TF_PROBE_ARDUINO open_write=%s\n", out ? "OK" : "FAIL");
  if (!out) {
    printErrno("TF_PROBE_ARDUINO open_write_fail");
    return false;
  }

  errno = 0;
  const size_t written = out.write(reinterpret_cast<const uint8_t*>(kPayload), strlen(kPayload));
  Serial.printf("TF_PROBE_ARDUINO write_ret=%u expected=%u\n",
                static_cast<unsigned>(written),
                static_cast<unsigned>(strlen(kPayload)));
  if (written != strlen(kPayload)) {
    printErrno("TF_PROBE_ARDUINO write_fail");
    out.close();
    SD_MMC.remove(file_path);
    return false;
  }

  errno = 0;
  out.flush();
  printErrno("TF_PROBE_ARDUINO after_flush");
  out.close();
  printErrno("TF_PROBE_ARDUINO after_close_write");

  errno = 0;
  File in = SD_MMC.open(file_path, FILE_READ);
  Serial.printf("TF_PROBE_ARDUINO open_read=%s\n", in ? "OK" : "FAIL");
  if (!in) {
    printErrno("TF_PROBE_ARDUINO open_read_fail");
    SD_MMC.remove(file_path);
    return false;
  }

  char buffer[64] = {};
  errno = 0;
  const int read_len = in.read(reinterpret_cast<uint8_t*>(buffer), sizeof(buffer) - 1);
  Serial.printf("TF_PROBE_ARDUINO read_ret=%d expected=%u\n",
                read_len,
                static_cast<unsigned>(strlen(kPayload)));
  if (read_len < 0) {
    printErrno("TF_PROBE_ARDUINO read_fail");
    in.close();
    SD_MMC.remove(file_path);
    return false;
  }
  const bool match = static_cast<size_t>(read_len) == strlen(kPayload) &&
                     memcmp(buffer, kPayload, strlen(kPayload)) == 0;
  Serial.printf("TF_PROBE_ARDUINO compare=%s data=%s\n", match ? "OK" : "BAD", buffer);
  in.close();

  errno = 0;
  const bool remove_ok = SD_MMC.remove(file_path);
  Serial.printf("TF_PROBE_ARDUINO remove=%s\n", remove_ok ? "OK" : "FAIL");
  if (!remove_ok) {
    printErrno("TF_PROBE_ARDUINO remove_fail");
  }
  return match && remove_ok;
}

void runTfProbe() {
  Serial.printf("TF_PROBE_BEGIN framework=%s camera=%s freq_khz=%u name_mode=%s\n",
#if defined(TF_PROBE_HYBRID)
                "arduino+espidf",
#else
                "arduino",
#endif
#if defined(TF_PROBE_WITH_CAMERA)
                "yes",
#else
                "no",
#endif
                static_cast<unsigned>(TF_PROBE_FREQ_KHZ),
                TF_PROBE_NAME_MODE);

#if defined(TF_PROBE_WITH_CAMERA)
  initCameraForControlledProbe();
#endif

  SD_MMC.end();
  errno = 0;
  const bool pins_ok = SD_MMC.setPins(kSdClk, kSdCmd, kSdData0);
  Serial.printf("TF_PROBE setPins(clk=%d cmd=%d d0=%d)=%s\n",
                kSdClk,
                kSdCmd,
                kSdData0,
                pins_ok ? "OK" : "FAIL");
  if (!pins_ok) {
    printErrno("TF_PROBE setPins_fail");
  }

  errno = 0;
  const bool begin_ok = SD_MMC.begin(kMountPoint, true, false, TF_PROBE_FREQ_KHZ, 5);
  Serial.printf("TF_PROBE begin(mount=%s mode1bit=true format=false freq=%u max_files=5)=%s\n",
                kMountPoint,
                static_cast<unsigned>(TF_PROBE_FREQ_KHZ),
                begin_ok ? "OK" : "FAIL");
  if (!begin_ok) {
    printErrno("TF_PROBE begin_fail");
    Serial.println("TF_PROBE_DONE result=FAIL stage=begin");
    return;
  }

  const uint8_t type = SD_MMC.cardType();
  Serial.printf("TF_PROBE cardType=%s(%u) cardSize=%llu totalBytes=%llu usedBytes=%llu\n",
                cardTypeName(type),
                static_cast<unsigned>(type),
                SD_MMC.cardSize(),
                SD_MMC.totalBytes(),
                SD_MMC.usedBytes());

  const uint32_t suffix = millis();
  const bool posix_ok = runPosixProbe(suffix);
  const bool arduino_ok = runArduinoProbe(suffix + 1);
  Serial.printf("TF_PROBE_DONE result=%s posix=%s arduino=%s\n",
                (posix_ok && arduino_ok) ? "PASS" : "FAIL",
                posix_ok ? "PASS" : "FAIL",
                arduino_ok ? "PASS" : "FAIL");
}
}  // namespace

void tfWriteProbeRun() {
  runTfProbe();
}

#if !defined(TF_PROBE_EXTERNAL_MAIN)
void setup() {
  Serial.begin(kBaud);
  delay(1800);
  Serial.println();
  tfWriteProbeRun();
}

void loop() {
  delay(5000);
}
#endif
