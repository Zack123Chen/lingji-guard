#include "CameraManager.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"

#include <Wire.h>

#define sensor_t camera_sensor_t
#include "esp_camera.h"
#undef sensor_t

#include "Pins.h"
#include "SensorManager.h"

namespace CameraManager {
namespace {
ModuleState g_camera_state;
bool g_last_sccb_seen = false;

const char* frameSizeName(framesize_t size) {
  switch (size) {
    case FRAMESIZE_QQVGA:
      return "QQVGA";
    case FRAMESIZE_QVGA:
      return "QVGA";
    default:
      return "CUSTOM";
  }
}

bool readSccbReg8(uint8_t device_addr, uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(device_addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(device_addr, static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}

bool readSccbReg16(uint8_t device_addr, uint16_t reg, uint8_t& value) {
  Wire.beginTransmission(device_addr);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(device_addr, static_cast<uint8_t>(1)) != 1) {
    return false;
  }

  value = Wire.read();
  return true;
}

bool printCameraIdForAddress(uint8_t address) {
  uint8_t pid = 0;
  uint8_t ver = 0;
  bool found = false;
  Serial.printf("Camera SCCB probe at 0x%02X:\n", address);

  if (readSccbReg8(address, 0x0A, pid) && readSccbReg8(address, 0x0B, ver)) {
    found = true;
    Serial.printf("  8-bit ID regs 0x0A/0x0B = 0x%02X 0x%02X", pid, ver);
    if (pid == 0x26) {
      Serial.print(" (looks like OV2640)");
    } else if (pid == 0x76) {
      Serial.print(" (looks like OV7670)");
    } else if (pid == 0x56 && ver == 0x40) {
      Serial.print(" (looks like OV5640 low-byte style)");
    }
    Serial.println();
  } else {
    Serial.println("  8-bit ID regs 0x0A/0x0B read failed");
  }

  uint8_t id_high = 0;
  uint8_t id_low = 0;
  if (readSccbReg16(address, 0x300A, id_high) && readSccbReg16(address, 0x300B, id_low)) {
    found = true;
    Serial.printf("  16-bit ID regs 0x300A/0x300B = 0x%02X 0x%02X", id_high, id_low);
    if (id_high == 0x56 && id_low == 0x40) {
      Serial.print(" (looks like OV5640)");
    } else if (id_high == 0x28 && id_low == 0x40) {
      Serial.print(" (looks like OV2840; not supported by esp_camera here)");
    }
    Serial.println();
  } else {
    Serial.println("  16-bit ID regs 0x300A/0x300B read failed");
  }
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
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QQVGA;
  config.fb_location = CAMERA_FB_IN_DRAM;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.jpeg_quality = 12;
  return config;
}

void dropFrames(uint8_t count) {
  for (uint8_t index = 0; index < count; ++index) {
    camera_fb_t* stale = esp_camera_fb_get();
#ifdef CAMERA_MANAGER_ONLY_DIAG
    Serial.printf("[CAM-MGR] dropFrame_%u %s", static_cast<unsigned>(index), stale ? "success" : "fail");
    if (stale != nullptr) {
      const bool soi = stale->len >= 2 && stale->buf[0] == 0xFF && stale->buf[1] == 0xD8;
      const bool eoi = stale->len >= 2 && stale->buf[stale->len - 2] == 0xFF && stale->buf[stale->len - 1] == 0xD9;
      Serial.printf(" width=%u height=%u len=%u SOI=%s EOI=%s",
                    static_cast<unsigned>(stale->width),
                    static_cast<unsigned>(stale->height),
                    static_cast<unsigned>(stale->len),
                    soi ? "OK" : "BAD",
                    eoi ? "OK" : "BAD");
    }
    Serial.println();
#endif
    if (stale != nullptr) {
      esp_camera_fb_return(stale);
    }
    delay(30);
  }
}

void applyImageSettings(const char* stage) {
  camera_sensor_t* sensor = esp_camera_sensor_get();
  if (sensor == nullptr) {
    return;
  }

  sensor->set_colorbar(sensor, 0);
  sensor->set_special_effect(sensor, 0);
  sensor->set_whitebal(sensor, 0);
  sensor->set_awb_gain(sensor, 0);
  sensor->set_wb_mode(sensor, 0);
  sensor->set_exposure_ctrl(sensor, 1);
  sensor->set_aec2(sensor, 1);
  sensor->set_gain_ctrl(sensor, 1);
  sensor->set_gainceiling(sensor, GAINCEILING_8X);
  sensor->set_brightness(sensor, 0);
  sensor->set_contrast(sensor, 0);
  sensor->set_saturation(sensor, 0);
  sensor->set_dcw(sensor, 1);
  sensor->set_raw_gma(sensor, 1);
  sensor->set_lenc(sensor, 1);
  sensor->set_hmirror(sensor, 0);
  sensor->set_vflip(sensor, 0);
  sensor->set_pixformat(sensor, PIXFORMAT_GRAYSCALE);
  sensor->set_framesize(sensor, FRAMESIZE_QQVGA);
  Serial.printf("Camera grayscale settings applied (%s): frame=%s colorbar=0.\n",
                stage && stage[0] ? stage : "camera",
                frameSizeName(FRAMESIZE_QQVGA));
}

}  // namespace

void diagnoseSccb() {
  g_last_sccb_seen = false;
  Serial.println("Camera SCCB diagnostics:");
  g_last_sccb_seen |= printCameraIdForAddress(0x30);
  g_last_sccb_seen |= printCameraIdForAddress(0x3C);
  g_last_sccb_seen |= printCameraIdForAddress(0x21);
  if (!g_last_sccb_seen) {
    Serial.println("Camera SCCB did not answer. Schematic ties RESETB high and PWDN low; check camera power/ribbon/module.");
  }
}

void init() {
  g_last_sccb_seen = false;
  Serial.println("camera component=source-built");
  Serial.printf("camera_task_stack=%d\n", CONFIG_CAMERA_TASK_STACK_SIZE);
  Serial.println("camera format=GRAYSCALE");
  Serial.println("camera preview=local-only");
  Serial.println("image persistence=disabled");
  Serial.println("[CAM] manual SCCB probe disabled for clean local preview path.");
  Serial.println("[CAM] profile=GRAYSCALE QQVGA 160x120 DRAM fb=1");
  Serial.printf("[CAM] PSRAM usable=%s; camera deliberately uses DRAM\n",
                psramActuallyUsable() ? "yes" : "no");

  // The current module works with XCLK=-1 in this wiring. If a future camera
  // fails here, confirm whether the board exposes an XCLK/MCLK pad.
  const camera_config_t config = makeConfig();
  const esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    g_camera_state.ready = false;
    g_camera_state.status = "Init Failed";
    Serial.printf("Camera init failed: 0x%x (%s). Other modules will continue running.\n",
                  err,
                  esp_err_to_name(err));
    esp_camera_deinit();
    return;
  }

  g_camera_state.ready = true;
  g_camera_state.status = "本地预览";
  applyImageSettings("init");
  Serial.println("Camera initialized for local-only grayscale preview. JPEG/storage/upload disabled.");
}

bool captureFrame(CameraFrame& frame) {
  frame = {};
  if (!g_camera_state.ready) {
    Serial.println("Camera grayscale capture skipped: camera is not ready.");
    return false;
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (fb == nullptr) {
    g_camera_state.status = "预览失败";
    return false;
  }

  frame.width = fb->width;
  frame.height = fb->height;
  frame.len = fb->len;
  frame.format = fb->format;
  frame.data = fb->buf;
  frame.handle = fb;
  const bool shape_ok = frame.width == 160 && frame.height == 120 &&
                        frame.len == 160UL * 120UL &&
                        frame.format == PIXFORMAT_GRAYSCALE;
  g_camera_state.status = shape_ok ? "本地预览" : "预览尺寸异常";
  Serial.printf("[CAM] GRAYSCALE %ux%u len=%u format=%d shape=%s\n",
                static_cast<unsigned>(frame.width),
                static_cast<unsigned>(frame.height),
                static_cast<unsigned>(frame.len),
                frame.format,
                shape_ok ? "OK" : "BAD");
  if (!shape_ok) {
    releaseFrame(frame);
    return false;
  }
  return true;
}

bool captureJpegFrame(CameraFrame& frame) {
  frame = {};
  Serial.println("[CAM] JPEG capture disabled: local grayscale preview only.");
  return false;
}

void releaseFrame(CameraFrame& frame) {
  if (frame.handle != nullptr) {
    esp_camera_fb_return(static_cast<camera_fb_t*>(frame.handle));
  }
  frame = {};
}

bool psramActuallyUsable() {
  return psramFound() &&
         ESP.getPsramSize() >= 512 * 1024 &&
         heap_caps_get_total_size(MALLOC_CAP_SPIRAM) >= 512 * 1024;
}

void markLimited() {
  g_camera_state.ready = false;
  g_camera_state.status = "Camera Limited";
}

const ModuleState& state() {
  return g_camera_state;
}
}  // namespace CameraManager
