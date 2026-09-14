#include <Arduino.h>

#if defined(CAMERA_PSRAM_COLORBAR_DIAG)

#include "esp_heap_caps.h"

#include <FS.h>
#include <SD_MMC.h>

#define sensor_t camera_sensor_t
#include "esp_camera.h"
#undef sensor_t

#include "Pins.h"

namespace {
#if defined(CAMERA_COLORBAR_PSRAM_ON)
constexpr const char* kMode = "PSRAM_ON_COLORBAR";
#elif defined(CAMERA_COLORBAR_PSRAM_OFF)
constexpr const char* kMode = "PSRAM_OFF_COLORBAR";
#else
constexpr const char* kMode = "PSRAM_UNKNOWN_COLORBAR";
#endif

constexpr const char* kDiagDir = "/careguard/camera";
constexpr uint8_t kFrameCount = 3;

struct JpegMarkers {
  bool soi = false;
  bool eoi = false;
  bool dqt = false;
  bool dht = false;
  bool sos = false;
  bool sof = false;
  uint16_t sof_width = 0;
  uint16_t sof_height = 0;
};

const char* markerName(uint8_t marker) {
  switch (marker) {
    case 0xC0:
      return "SOF0";
    case 0xC2:
      return "SOF2";
    case 0xC4:
      return "DHT";
    case 0xD8:
      return "SOI";
    case 0xD9:
      return "EOI";
    case 0xDA:
      return "SOS";
    case 0xDB:
      return "DQT";
    case 0xDD:
      return "DRI";
    case 0xE0:
      return "APP0";
    case 0xFE:
      return "COM";
    default:
      return "MARKER";
  }
}

bool markerHasLength(uint8_t marker) {
  return !(marker == 0x01 || marker == 0xD8 || marker == 0xD9 ||
           (marker >= 0xD0 && marker <= 0xD7));
}

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return ~crc;
}

uint32_t crc32(const uint8_t* data, size_t len) {
  return crc32Update(0, data, len);
}

bool findNextMarker(const uint8_t* data, size_t len, size_t start, size_t* offset, uint8_t* marker) {
  for (size_t pos = start; pos + 1 < len; ++pos) {
    if (data[pos] != 0xFF) {
      continue;
    }
    while (pos < len && data[pos] == 0xFF) {
      ++pos;
    }
    if (pos >= len) {
      return false;
    }
    if (data[pos] == 0x00) {
      continue;
    }
    *offset = pos - 1;
    *marker = data[pos];
    return true;
  }
  return false;
}

JpegMarkers parseJpegMarkers(const uint8_t* data, size_t len, uint8_t frame_index) {
  JpegMarkers found;
  Serial.printf("[COLORBAR] frame=%u markers_begin\n", static_cast<unsigned>(frame_index));

  size_t offset = 0;
  uint8_t marker = 0;
  size_t search = 0;
  uint8_t printed = 0;
  while (findNextMarker(data, len, search, &offset, &marker)) {
    const bool has_length = markerHasLength(marker);
    uint16_t seg_len = 0;
    if (has_length && offset + 4 <= len) {
      seg_len = (static_cast<uint16_t>(data[offset + 2]) << 8) | data[offset + 3];
    }

    if (marker == 0xD8) {
      found.soi = true;
    } else if (marker == 0xD9) {
      found.eoi = true;
    } else if (marker == 0xDB) {
      found.dqt = true;
    } else if (marker == 0xC4) {
      found.dht = true;
    } else if (marker == 0xDA) {
      found.sos = true;
    } else if ((marker == 0xC0 || marker == 0xC2) && offset + 9 <= len) {
      found.sof = true;
      found.sof_height = (static_cast<uint16_t>(data[offset + 5]) << 8) | data[offset + 6];
      found.sof_width = (static_cast<uint16_t>(data[offset + 7]) << 8) | data[offset + 8];
    }

    if (printed < 24) {
      Serial.printf("[COLORBAR] frame=%u marker offset=0x%05X FF%02X %s",
                    static_cast<unsigned>(frame_index),
                    static_cast<unsigned>(offset),
                    marker,
                    markerName(marker));
      if (has_length) {
        Serial.printf(" len=%u", static_cast<unsigned>(seg_len));
      }
      if ((marker == 0xC0 || marker == 0xC2) && offset + 9 <= len) {
        Serial.printf(" sof=%ux%u",
                      static_cast<unsigned>(found.sof_width),
                      static_cast<unsigned>(found.sof_height));
      }
      Serial.println();
      ++printed;
    }

    if (marker == 0xD9) {
      break;
    }
    if (marker == 0xDA) {
      search = offset + 2;
      continue;
    }
    if (has_length) {
      if (seg_len < 2 || offset + 2 + seg_len > len) {
        Serial.printf("[COLORBAR] frame=%u marker_parse_stop invalid_segment offset=0x%05X len=%u\n",
                      static_cast<unsigned>(frame_index),
                      static_cast<unsigned>(offset),
                      static_cast<unsigned>(seg_len));
        break;
      }
      search = offset + 2 + seg_len;
    } else {
      search = offset + 2;
    }
  }

  Serial.printf("[COLORBAR] frame=%u markers soi=%s dqt=%s sof=%s dht=%s sos=%s eoi=%s sof_size=%ux%u\n",
                static_cast<unsigned>(frame_index),
                found.soi ? "OK" : "MISS",
                found.dqt ? "OK" : "MISS",
                found.sof ? "OK" : "MISS",
                found.dht ? "OK" : "MISS",
                found.sos ? "OK" : "MISS",
                found.eoi ? "OK" : "MISS",
                static_cast<unsigned>(found.sof_width),
                static_cast<unsigned>(found.sof_height));
  return found;
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
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QQVGA;
  config.fb_location = CAMERA_FB_IN_DRAM;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.jpeg_quality = 12;
  return config;
}

bool mountTfCard() {
  pinMode(Pins::Storage::CD_IN, INPUT_PULLUP);
  if (!SD_MMC.setPins(Pins::Storage::CLK, Pins::Storage::CMD, Pins::Storage::DATA0)) {
    Serial.println("[COLORBAR] TF setPins=FAIL");
    return false;
  }
  if (!SD_MMC.begin("/sdcard", true, false, 4000, 5)) {
    Serial.println("[COLORBAR] TF mount=FAIL");
    return false;
  }
  SD_MMC.mkdir("/careguard");
  SD_MMC.mkdir(kDiagDir);
  Serial.printf("[COLORBAR] TF mount=OK size=%lluMB used=%lluMB\n",
                SD_MMC.cardSize() / (1024ULL * 1024ULL),
                SD_MMC.usedBytes() / (1024ULL * 1024ULL));
  return true;
}

bool saveFrame(const camera_fb_t* fb, uint8_t frame_index, char* path, size_t path_len) {
  snprintf(path,
           path_len,
           "%s/%s_%02u_%lu.jpg",
           kDiagDir,
           kMode,
           static_cast<unsigned>(frame_index),
           static_cast<unsigned long>(millis()));
  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("[COLORBAR] frame=%u save=OPEN_FAIL path=%s\n",
                  static_cast<unsigned>(frame_index),
                  path);
    return false;
  }
  const size_t written = file.write(fb->buf, fb->len);
  file.close();
  Serial.printf("[COLORBAR] frame=%u save=%s path=%s written=%u\n",
                static_cast<unsigned>(frame_index),
                written == fb->len ? "OK" : "SHORT",
                path,
                static_cast<unsigned>(written));
  return written == fb->len;
}

void printRuntimeConfig() {
  Serial.printf("[COLORBAR] mode=%s\n", kMode);
#if defined(BOARD_HAS_PSRAM)
  Serial.println("[COLORBAR] compile BOARD_HAS_PSRAM=1");
#else
  Serial.println("[COLORBAR] compile BOARD_HAS_PSRAM=0");
#endif
  Serial.printf("[COLORBAR] psramFound=%s psramSize=%u freePsram=%u spiramTotal=%u spiramFree=%u\n",
                psramFound() ? "true" : "false",
                static_cast<unsigned>(ESP.getPsramSize()),
                static_cast<unsigned>(ESP.getFreePsram()),
                static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  Serial.printf("[COLORBAR] camera pins D0-D7=%d,%d,%d,%d,%d,%d,%d,%d PCLK=%d VSYNC=%d HREF=%d XCLK=%d SIOD=%d SIOC=%d\n",
                Pins::Camera::D0,
                Pins::Camera::D1,
                Pins::Camera::D2,
                Pins::Camera::D3,
                Pins::Camera::D4,
                Pins::Camera::D5,
                Pins::Camera::D6,
                Pins::Camera::D7,
                Pins::Camera::PCLK,
                Pins::Camera::VSYNC,
                Pins::Camera::HREF,
                Pins::Camera::XCLK,
                Pins::Camera::SIOD,
                Pins::Camera::SIOC);
  Serial.println("[COLORBAR] camera config JPEG QQVGA DRAM fb_count=1 grab=WHEN_EMPTY quality=12");
}

void captureFrame(uint8_t frame_index) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (fb == nullptr) {
    Serial.printf("[COLORBAR] frame=%u capture=FAIL fb=null\n", static_cast<unsigned>(frame_index));
    return;
  }

  const bool soi = fb->len >= 2 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8;
  const bool eoi = fb->len >= 2 && fb->buf[fb->len - 2] == 0xFF && fb->buf[fb->len - 1] == 0xD9;
  const uint32_t crc = crc32(fb->buf, fb->len);
  Serial.printf("[COLORBAR] frame=%u capture=OK width=%u height=%u len=%u format=%d crc32=0x%08lX soi=%s eoi=%s\n",
                static_cast<unsigned>(frame_index),
                static_cast<unsigned>(fb->width),
                static_cast<unsigned>(fb->height),
                static_cast<unsigned>(fb->len),
                fb->format,
                static_cast<unsigned long>(crc),
                soi ? "OK" : "BAD",
                eoi ? "OK" : "BAD");
  parseJpegMarkers(fb->buf, fb->len, frame_index);

  char path[128] = {};
  saveFrame(fb, frame_index, path, sizeof(path));
  esp_camera_fb_return(fb);
}

void runDiagnostic() {
  Serial.println("[COLORBAR] diag start");
  Serial.println("[COLORBAR] no TFT/4G/audio/SensorManager/CameraManager; minimal SD_MMC only for evidence files");
  printRuntimeConfig();

  if (!mountTfCard()) {
    Serial.println("[COLORBAR] stop: TF unavailable");
    return;
  }

  const camera_config_t config = makeConfig();
  const esp_err_t err = esp_camera_init(&config);
  Serial.printf("[COLORBAR] esp_camera_init=0x%x\n", err);
  if (err != ESP_OK) {
    return;
  }

  camera_sensor_t* sensor = esp_camera_sensor_get();
  if (sensor == nullptr) {
    Serial.println("[COLORBAR] sensor=null");
    return;
  }
  sensor->set_colorbar(sensor, 1);
  Serial.println("[COLORBAR] set_colorbar=1");
  delay(800);

  for (uint8_t i = 1; i <= kFrameCount; ++i) {
    captureFrame(i);
    delay(200);
  }
  Serial.println("[COLORBAR] diag complete");
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
  Serial.println("[COLORBAR] idle");
}

#endif  // CAMERA_PSRAM_COLORBAR_DIAG
