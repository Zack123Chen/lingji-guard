#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/ledc.h"
#include "esp_camera.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_CAMERA_TASK_STACK_SIZE != 4096
#error "CONFIG_CAMERA_TASK_STACK_SIZE must be 4096 for this diagnostic"
#endif

enum {
  PIN_CAM_PWDN = -1,
  PIN_CAM_RESET = -1,
  PIN_CAM_XCLK = -1,
  PIN_CAM_SIOD = 10,
  PIN_CAM_SIOC = 9,
  PIN_CAM_D0 = 34,
  PIN_CAM_D1 = 48,
  PIN_CAM_D2 = 47,
  PIN_CAM_D3 = 33,
  PIN_CAM_D4 = 35,
  PIN_CAM_D5 = 37,
  PIN_CAM_D6 = 38,
  PIN_CAM_D7 = 39,
  PIN_CAM_PCLK = 36,
  PIN_CAM_VSYNC = 41,
  PIN_CAM_HREF = 40,
};

static const char *TAG = "IDF-GRAY";
static const uint32_t EXPECTED_WIDTH = 160;
static const uint32_t EXPECTED_HEIGHT = 120;
static const uint32_t EXPECTED_LEN = 160 * 120;
static const char BASE64_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
  crc = ~crc;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return ~crc;
}

static uint32_t crc32_bytes(const uint8_t *data, size_t len) {
  return crc32_update(0, data, len);
}

static void print_base64_packet(const uint8_t *data, size_t len) {
  char line[77];
  size_t line_len = 0;
  size_t i = 0;

  while (i < len) {
    const uint32_t b0 = data[i++];
    const bool have_b1 = i < len;
    const uint32_t b1 = have_b1 ? data[i++] : 0;
    const bool have_b2 = i < len;
    const uint32_t b2 = have_b2 ? data[i++] : 0;
    const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

    line[line_len++] = BASE64_ALPHABET[(triple >> 18) & 0x3F];
    line[line_len++] = BASE64_ALPHABET[(triple >> 12) & 0x3F];
    line[line_len++] = have_b1 ? BASE64_ALPHABET[(triple >> 6) & 0x3F] : '=';
    line[line_len++] = have_b2 ? BASE64_ALPHABET[triple & 0x3F] : '=';

    if (line_len == 76) {
      line[line_len] = '\0';
      printf("%s\n", line);
      line_len = 0;
    }
  }

  if (line_len > 0) {
    line[line_len] = '\0';
    printf("%s\n", line);
  }
}

static camera_config_t make_camera_config(void) {
  camera_config_t config = {0};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = PIN_CAM_D0;
  config.pin_d1 = PIN_CAM_D1;
  config.pin_d2 = PIN_CAM_D2;
  config.pin_d3 = PIN_CAM_D3;
  config.pin_d4 = PIN_CAM_D4;
  config.pin_d5 = PIN_CAM_D5;
  config.pin_d6 = PIN_CAM_D6;
  config.pin_d7 = PIN_CAM_D7;
  config.pin_xclk = PIN_CAM_XCLK;
  config.pin_pclk = PIN_CAM_PCLK;
  config.pin_vsync = PIN_CAM_VSYNC;
  config.pin_href = PIN_CAM_HREF;
  config.pin_sccb_sda = PIN_CAM_SIOD;
  config.pin_sccb_scl = PIN_CAM_SIOC;
  config.pin_pwdn = PIN_CAM_PWDN;
  config.pin_reset = PIN_CAM_RESET;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QQVGA;
  config.jpeg_quality = 12;
  config.fb_location = CAMERA_FB_IN_DRAM;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  return config;
}

static void print_diag_header(void) {
  printf("\n[IDF-GRAY] === camera idf grayscale stack4096 diag start ===\n");
  printf("[IDF-GRAY] camera_task_stack=%d\n", CONFIG_CAMERA_TASK_STACK_SIZE);
  printf("[IDF-GRAY] source_format=PIXFORMAT_GRAYSCALE\n");
  printf("[IDF-GRAY] expected_width=%" PRIu32 "\n", EXPECTED_WIDTH);
  printf("[IDF-GRAY] expected_height=%" PRIu32 "\n", EXPECTED_HEIGHT);
  printf("[IDF-GRAY] expected_len=%" PRIu32 "\n", EXPECTED_LEN);
  printf("[IDF-GRAY] pins XCLK=%d SDA=%d SCL=%d D0-D7=%d,%d,%d,%d,%d,%d,%d,%d PCLK=%d VSYNC=%d HREF=%d\n",
         PIN_CAM_XCLK, PIN_CAM_SIOD, PIN_CAM_SIOC, PIN_CAM_D0, PIN_CAM_D1,
         PIN_CAM_D2, PIN_CAM_D3, PIN_CAM_D4, PIN_CAM_D5, PIN_CAM_D6, PIN_CAM_D7,
         PIN_CAM_PCLK, PIN_CAM_VSYNC, PIN_CAM_HREF);
}

void app_main(void) {
  esp_log_level_set("*", ESP_LOG_INFO);
  vTaskDelay(pdMS_TO_TICKS(3000));
  print_diag_header();

  const camera_config_t config = make_camera_config();
  const esp_err_t init_err = esp_camera_init(&config);
  printf("[IDF-GRAY] esp_camera_init=0x%x\n", init_err);
  if (init_err != ESP_OK) {
    printf("[IDF-GRAY] STOP: camera init failed\n");
    return;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor == NULL) {
    printf("[IDF-GRAY] STOP: sensor=null\n");
    return;
  }

  const int fmt_ret = sensor->set_pixformat(sensor, PIXFORMAT_GRAYSCALE);
  const int size_ret = sensor->set_framesize(sensor, FRAMESIZE_QQVGA);
  const int colorbar_ret = sensor->set_colorbar(sensor, 1);
  printf("[IDF-GRAY] set_pixformat=%d set_framesize=%d set_colorbar=%d\n",
         fmt_ret, size_ret, colorbar_ret);
  vTaskDelay(pdMS_TO_TICKS(1500));

  for (int warmup_index = 1; warmup_index <= 3; ++warmup_index) {
    camera_fb_t *warmup = esp_camera_fb_get();
    if (warmup == NULL) {
      printf("[IDF-GRAY] warmup_index=%d capture=FAIL fb=null\n", warmup_index);
      continue;
    }
    printf("[IDF-GRAY] warmup_index=%d width=%u height=%u len=%u discard=YES\n",
           warmup_index, (unsigned)warmup->width, (unsigned)warmup->height,
           (unsigned)warmup->len);
    esp_camera_fb_return(warmup);
    vTaskDelay(pdMS_TO_TICKS(120));
  }

  for (int frame_index = 1; frame_index <= 3; ++frame_index) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb == NULL) {
      printf("[IDF-GRAY] frame_index=%d capture=FAIL fb=null\n", frame_index);
      continue;
    }

    const uint32_t crc = crc32_bytes(fb->buf, fb->len);
    printf("[IDF-GRAY] frame_index=%d\n", frame_index);
    printf("[IDF-GRAY] width=%u\n", (unsigned)fb->width);
    printf("[IDF-GRAY] height=%u\n", (unsigned)fb->height);
    printf("[IDF-GRAY] framebuffer_len=%u\n", (unsigned)fb->len);
    printf("[IDF-GRAY] framebuffer_crc32=0x%08" PRIX32 "\n", crc);
    printf("[IDF-GRAY] framebuffer_format=%d\n", (int)fb->format);

    if (fb->width == EXPECTED_WIDTH && fb->height == EXPECTED_HEIGHT &&
        fb->len == EXPECTED_LEN) {
      printf("[IDF-GRAY] FRAME_BASE64_BEGIN index=%d width=%" PRIu32 " height=%" PRIu32 " len=%" PRIu32 " crc32=0x%08" PRIX32 "\n",
             frame_index, EXPECTED_WIDTH, EXPECTED_HEIGHT, EXPECTED_LEN, crc);
      print_base64_packet(fb->buf, fb->len);
      printf("[IDF-GRAY] FRAME_BASE64_END index=%d\n", frame_index);
    } else {
      printf("[IDF-GRAY] frame_index=%d base64=SKIP dimension_or_len_mismatch\n",
             frame_index);
    }

    esp_camera_fb_return(fb);
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  printf("[IDF-GRAY] === diag complete ===\n");
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(5000));
    printf("[IDF-GRAY] idle\n");
  }
}
