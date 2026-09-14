#include <Arduino.h>

#if defined(CAMERA_GRAYSCALE_COLORBAR_DIAG)

#include "esp_heap_caps.h"

#include <FS.h>
#include <SD_MMC.h>

#define sensor_t camera_sensor_t
#include "esp_camera.h"
#undef sensor_t

#include "Pins.h"

namespace {
constexpr const char* kMode = "GRAYSCALE_COLORBAR";
constexpr const char* kDiagDir = "/careguard/gray";
constexpr uint8_t kFrameCount = 3;

// GRAYSCALE QQVGA = 160 * 120 = 19200 bytes
constexpr uint16_t kWidth = 160;
constexpr uint16_t kHeight = 120;
constexpr uint32_t kExpectedLen = kWidth * kHeight;  // 19200

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
  }
  return ~crc;
}

uint32_t crc32(const uint8_t* data, size_t len) {
  return crc32Update(0, data, len);
}

camera_config_t makeConfig() {
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
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QQVGA;
  config.fb_location = CAMERA_FB_IN_DRAM;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  return config;
}

bool mountTfCard() {
  pinMode(Pins::Storage::CD_IN, INPUT_PULLUP);
  if (!SD_MMC.setPins(Pins::Storage::CLK, Pins::Storage::CMD, Pins::Storage::DATA0)) {
    Serial.println("[GRAY-CB] TF setPins=FAIL");
    return false;
  }
  if (!SD_MMC.begin("/sdcard", true, false, 4000, 5)) {
    Serial.println("[GRAY-CB] TF mount=FAIL");
    return false;
  }
  SD_MMC.mkdir("/careguard");
  SD_MMC.mkdir(kDiagDir);
  Serial.printf("[GRAY-CB] TF mount=OK size=%lluMB used=%lluMB\n",
                SD_MMC.cardSize() / (1024ULL * 1024ULL),
                SD_MMC.usedBytes() / (1024ULL * 1024ULL));
  return true;
}

bool savePgmAndVerify(const camera_fb_t* fb, uint8_t frame_index,
                      char* path_out, size_t path_len,
                      uint32_t* fb_crc_out, uint32_t* tf_crc_out) {
  snprintf(path_out, path_len, "%s/%s_%02u_%lu.pgm",
           kDiagDir, kMode, static_cast<unsigned>(frame_index),
           static_cast<unsigned long>(millis()));

  const uint32_t fb_crc = crc32(fb->buf, fb->len);
  if (fb_crc_out) *fb_crc_out = fb_crc;

  File file = SD_MMC.open(path_out, FILE_WRITE);
  if (!file) {
    Serial.printf("[GRAY-CB] frame=%u save=OPEN_FAIL path=%s\n",
                  static_cast<unsigned>(frame_index), path_out);
    return false;
  }

  // P5 PGM header + raw grayscale pixels (framebuffer IS the pixels)
  char header[64];
  int hdr_len = snprintf(header, sizeof(header), "P5\n%u %u\n255\n",
                         static_cast<unsigned>(kWidth),
                         static_cast<unsigned>(kHeight));
  size_t w1 = file.write(reinterpret_cast<const uint8_t*>(header), hdr_len);
  size_t w2 = file.write(fb->buf, fb->len);
  size_t file_size = file.size();
  file.close();

  Serial.printf("[GRAY-CB] frame=%u pgm hdr=%u/%d px=%u/%u file=%u expect=%u\n",
                static_cast<unsigned>(frame_index),
                static_cast<unsigned>(w1), hdr_len,
                static_cast<unsigned>(w2), static_cast<unsigned>(fb->len),
                static_cast<unsigned>(file_size),
                static_cast<unsigned>(hdr_len + fb->len));

  const uint32_t expected = static_cast<uint32_t>(hdr_len) + kExpectedLen;
  if (file_size != expected) {
    Serial.printf("[GRAY-CB] frame=%u save=SIZE_MISMATCH got=%u expected=%u\n",
                  static_cast<unsigned>(frame_index),
                  static_cast<unsigned>(file_size), static_cast<unsigned>(expected));
    return false;
  }

  // Re-read pixels from PGM to verify CRC
  File rfile = SD_MMC.open(path_out, FILE_READ);
  if (!rfile) {
    Serial.printf("[GRAY-CB] frame=%u verify=REOPEN_FAIL\n", static_cast<unsigned>(frame_index));
    return false;
  }
  uint8_t nl = 0;
  while (rfile.available() && nl < 3)
    if (rfile.read() == '\n') nl++;
  const size_t px_start = rfile.position();
  const size_t px_bytes = rfile.size() - px_start;

  if (px_bytes != kExpectedLen) {
    Serial.printf("[GRAY-CB] frame=%u verify=BAD_LEN got=%u expected=%u\n",
                  static_cast<unsigned>(frame_index),
                  static_cast<unsigned>(px_bytes), static_cast<unsigned>(kExpectedLen));
    rfile.close();
    return false;
  }

  uint8_t* rb = new uint8_t[px_bytes];
  rfile.seek(px_start);
  size_t rd = rfile.read(rb, px_bytes);
  rfile.close();
  if (rd != px_bytes) {
    Serial.printf("[GRAY-CB] frame=%u verify=READ_SHORT rd=%u\n",
                  static_cast<unsigned>(frame_index), static_cast<unsigned>(rd));
    delete[] rb;
    return false;
  }

  const uint32_t tf_crc = crc32(rb, px_bytes);
  if (tf_crc_out) *tf_crc_out = tf_crc;
  delete[] rb;

  bool ok = (fb_crc == tf_crc);
  Serial.printf("[GRAY-CB] frame=%u save=OK size=%u path=%s\n",
                static_cast<unsigned>(frame_index), static_cast<unsigned>(file_size), path_out);
  Serial.printf("[GRAY-CB] frame=%u CRC fb=0x%08lX tf=0x%08lX match=%s\n",
                static_cast<unsigned>(frame_index),
                static_cast<unsigned long>(fb_crc), static_cast<unsigned long>(tf_crc),
                ok ? "YES" : "NO");
  return ok;
}

void printRuntimeConfig() {
  Serial.printf("[GRAY-CB] mode=%s\n", kMode);
  Serial.printf("[GRAY-CB] compile PSRAM=%s\n",
#if defined(BOARD_HAS_PSRAM)
                "BOARD_HAS_PSRAM"
#else
                "NO_PSRAM"
#endif
                );
  Serial.printf("[GRAY-CB] psramFound=%s psramSize=%u freePsram=%u\n",
                psramFound() ? "true" : "false",
                static_cast<unsigned>(ESP.getPsramSize()),
                static_cast<unsigned>(ESP.getFreePsram()));
  Serial.printf("[GRAY-CB] pins D0-D7=%d,%d,%d,%d,%d,%d,%d,%d PCLK=%d VSYNC=%d HREF=%d XCLK=%d\n",
                Pins::Camera::D0, Pins::Camera::D1, Pins::Camera::D2, Pins::Camera::D3,
                Pins::Camera::D4, Pins::Camera::D5, Pins::Camera::D6, Pins::Camera::D7,
                Pins::Camera::PCLK, Pins::Camera::VSYNC, Pins::Camera::HREF,
                Pins::Camera::XCLK);
  Serial.println("[GRAY-CB] source_format=PIXFORMAT_GRAYSCALE");
  Serial.println("[GRAY-CB] expected_len=19200");
}

void runDiagnostic() {
  Serial.println("[GRAY-CB] === diag start ===");
  Serial.println("[GRAY-CB] no TFT/4G/audio/SensorManager; direct GRAYSCALE colorbar only");
  printRuntimeConfig();

  bool tf_ok = mountTfCard();

  const camera_config_t config = makeConfig();
  const esp_err_t err = esp_camera_init(&config);
  Serial.printf("[GRAY-CB] esp_camera_init=0x%x\n", err);
  if (err != ESP_OK) {
    Serial.println("[GRAY-CB] STOP: camera init failed");
    return;
  }

  camera_sensor_t* sensor = esp_camera_sensor_get();
  if (sensor == nullptr) {
    Serial.println("[GRAY-CB] STOP: sensor=null");
    return;
  }

  sensor->set_pixformat(sensor, PIXFORMAT_GRAYSCALE);
  sensor->set_framesize(sensor, FRAMESIZE_QQVGA);
  sensor->set_colorbar(sensor, 1);
  Serial.println("[GRAY-CB] set_colorbar=1 pixformat=GRAYSCALE framesize=QQVGA");
  delay(800);

  for (uint8_t i = 1; i <= kFrameCount; ++i) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) {
      Serial.printf("[GRAY-CB] frame=%u capture=FAIL fb=null\n", static_cast<unsigned>(i));
      continue;
    }

    bool dim_ok = (fb->width == kWidth) && (fb->height == kHeight);
    bool len_ok = (fb->len == kExpectedLen);
    uint32_t fb_crc_val = crc32(fb->buf, fb->len);

    Serial.printf("[GRAY-CB] frame=%u capture=OK source_format=PIXFORMAT_GRAYSCALE\n", static_cast<unsigned>(i));
    Serial.printf("[GRAY-CB] frame=%u width=%u height=%u fb_len=%u\n",
                  static_cast<unsigned>(i),
                  static_cast<unsigned>(fb->width),
                  static_cast<unsigned>(fb->height),
                  static_cast<unsigned>(fb->len));
    Serial.printf("[GRAY-CB] frame=%u dim=%s len=%s fb_crc32=0x%08lX format=%d\n",
                  static_cast<unsigned>(i),
                  dim_ok ? "160x120_OK" : "BAD",
                  len_ok ? "19200_OK" : "BAD",
                  static_cast<unsigned long>(fb_crc_val),
                  fb->format);

    if (fb->len > 0) {
      uint8_t pmin = 255, pmax = 0;
      uint32_t psum = 0;
      for (size_t j = 0; j < fb->len; ++j) {
        if (fb->buf[j] < pmin) pmin = fb->buf[j];
        if (fb->buf[j] > pmax) pmax = fb->buf[j];
        psum += fb->buf[j];
      }
      Serial.printf("[GRAY-CB] frame=%u pixels min=%u max=%u mean=%u\n",
                    static_cast<unsigned>(i),
                    static_cast<unsigned>(pmin), static_cast<unsigned>(pmax),
                    static_cast<unsigned>(psum / fb->len));
    }

    char path[128] = {};
    uint32_t tf_crc = 0;
    bool crc_ok = false;
    if (tf_ok) {
      crc_ok = savePgmAndVerify(fb, i, path, sizeof(path), nullptr, &tf_crc);
    } else {
      Serial.printf("[GRAY-CB] frame=%u save=SKIP (no TF)\n", static_cast<unsigned>(i));
    }

    Serial.printf("[GRAY-CB] frame=%u FINAL dim=%s len=%s fb_crc=0x%08lX tf_crc=0x%08lX crc_match=%s\n",
                  static_cast<unsigned>(i),
                  dim_ok ? "160x120" : "MISMATCH",
                  len_ok ? "19200" : "MISMATCH",
                  static_cast<unsigned long>(fb_crc_val),
                  static_cast<unsigned long>(tf_crc),
                  crc_ok ? "YES" : "NO");

    esp_camera_fb_return(fb);
    delay(200);
  }
  Serial.println("[GRAY-CB] === diag complete ===");
}
}  // namespace

void setup() {
  Serial.begin(Pins::kBaudRate);
  delay(8000);
  Serial.println();
  runDiagnostic();
}

void loop() {
  delay(5000);
  Serial.println("[GRAY-CB] idle");
}

#endif  // CAMERA_GRAYSCALE_COLORBAR_DIAG
