#include <Arduino.h>

#include <cstdarg>
#include <cstdio>

#include "esp_heap_caps.h"

namespace {
constexpr uint32_t kBaudRate = 115200;
constexpr size_t kTestBytes = 512 * 1024;
bool diagnostic_passed = false;

void logFlush() {
  Serial.flush();
  fflush(stdout);
}

void logLine(const char* message = "") {
  Serial.println(message);
  puts(message);
  logFlush();
}

void logPrintf(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  Serial.print(buffer);
  fputs(buffer, stdout);
  logFlush();
}

void printEnabled(const char* name, bool enabled) {
  logPrintf("[EXT-QSPI-PSRAM] %s=%s\n", name, enabled ? "y" : "n");
}

void halt(const char* reason) {
  logPrintf("[EXT-QSPI-PSRAM] HALT reason=%s\n", reason);
  logLine("[EXT-QSPI-PSRAM] Production firmware was not touched. Do not merge back on failure.");
  while (true) {
    delay(2000);
    logPrintf("[EXT-QSPI-PSRAM] HALT reason=%s\n", reason);
    logLine("[EXT-QSPI-PSRAM] If the PSRAM chip is unusually hot, power off immediately.");
  }
}

bool writeAndVerify(uint8_t* buffer, size_t len, uint8_t pattern) {
  memset(buffer, pattern, len);

  for (size_t i = 0; i < len; ++i) {
    if (buffer[i] != pattern) {
      logPrintf("[EXT-QSPI-PSRAM] VERIFY_FAIL pattern=0x%02X offset=%u expected=0x%02X actual=0x%02X\n",
                pattern,
                static_cast<unsigned>(i),
                pattern,
                buffer[i]);
      return false;
    }
  }

  logPrintf("[EXT-QSPI-PSRAM] verify pattern=0x%02X len=%u PASS\n",
            pattern,
            static_cast<unsigned>(len));
  return true;
}

void runDiagnostic() {
  logLine();
  logLine("[EXT-QSPI-PSRAM] external Quad PSRAM diagnostic");
  logLine("[EXT-QSPI-PSRAM] CameraManager/TFT/TF/4G/audio/sensors are intentionally not initialized.");
  logLine("[EXT-QSPI-PSRAM] Build target: qio_qspi + psram_type=qspi, not OPI/Octal PSRAM.");
  logLine("[EXT-QSPI-PSRAM] Two-stage hardware gate: run once with CAM header removed, then again with CAM header installed.");
  logPrintf("[EXT-QSPI-PSRAM] chip=%s rev=%u cpu=%uMHz sdk=%s\n",
            ESP.getChipModel(),
            static_cast<unsigned>(ESP.getChipRevision()),
            static_cast<unsigned>(ESP.getCpuFreqMHz()),
            ESP.getSdkVersion());
  printEnabled("CONFIG_SPIRAM", static_cast<bool>(
#if defined(CONFIG_SPIRAM)
                   true
#else
                   false
#endif
                   ));
  printEnabled("CONFIG_SPIRAM_MODE_QUAD", static_cast<bool>(
#if defined(CONFIG_SPIRAM_MODE_QUAD)
                   true
#else
                   false
#endif
                   ));
  printEnabled("CONFIG_SPIRAM_TYPE_ESPPSRAM64", static_cast<bool>(
#if defined(CONFIG_SPIRAM_TYPE_ESPPSRAM64)
                   true
#else
                   false
#endif
                   ));
  printEnabled("CONFIG_SPIRAM_TYPE_AUTO", static_cast<bool>(
#if defined(CONFIG_SPIRAM_TYPE_AUTO)
                   true
#else
                   false
#endif
                   ));
  logPrintf("[EXT-QSPI-PSRAM] CONFIG_SPIRAM_SIZE=%u\n",
#if defined(CONFIG_SPIRAM_SIZE)
                static_cast<unsigned>(CONFIG_SPIRAM_SIZE)
#else
                0U
#endif
                );

  const bool found = psramFound();
  const uint32_t psram_size = ESP.getPsramSize();
  const uint32_t free_psram = ESP.getFreePsram();
  const size_t spiram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

  logPrintf("[EXT-QSPI-PSRAM] psramFound()=%s\n", found ? "true" : "false");
  logPrintf("[EXT-QSPI-PSRAM] ESP.getPsramSize()=%u\n", static_cast<unsigned>(psram_size));
  logPrintf("[EXT-QSPI-PSRAM] ESP.getFreePsram()=%u\n", static_cast<unsigned>(free_psram));
  logPrintf("[EXT-QSPI-PSRAM] heap_caps_get_total_size(MALLOC_CAP_SPIRAM)=%u\n",
            static_cast<unsigned>(spiram_total));

  if (!found || psram_size == 0 || spiram_total == 0) {
    halt("psram_not_available");
  }

  uint8_t* buffer = static_cast<uint8_t*>(
      heap_caps_malloc(kTestBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  logPrintf("[EXT-QSPI-PSRAM] heap_caps_malloc(512KB,MALLOC_CAP_SPIRAM)=%p\n", buffer);
  if (buffer == nullptr) {
    logLine("[EXT-QSPI-PSRAM] malloc_512KB=FAIL");
    halt("malloc_512kb_failed");
  }
  logLine("[EXT-QSPI-PSRAM] malloc_512KB=OK");

  bool verify_pass = true;
  if (!writeAndVerify(buffer, kTestBytes, 0xA5)) {
    verify_pass = false;
  }
  if (verify_pass && !writeAndVerify(buffer, kTestBytes, 0x5A)) {
    verify_pass = false;
  }

  logPrintf("[EXT-QSPI-PSRAM] verify_512KB=%s\n", verify_pass ? "PASS" : "FAIL");
  if (!verify_pass) {
    heap_caps_free(buffer);
    halt("verify_512kb_failed");
  }

  heap_caps_free(buffer);
  logPrintf("[EXT-QSPI-PSRAM] ESP.getFreePsram() after free=%u\n",
            static_cast<unsigned>(ESP.getFreePsram()));
  logLine("[EXT-QSPI-PSRAM] PASS external Quad PSRAM is visible and read/write stable.");
  logLine("[EXT-QSPI-PSRAM] Gate opened: only after this PASS should JPEG QQVGA DRAM colorbar diag be added/run.");
  diagnostic_passed = true;
}
}  // namespace

void setup() {
  Serial.begin(kBaudRate);
  delay(5000);
  runDiagnostic();
}

void loop() {
  delay(5000);
  if (diagnostic_passed) {
    logLine("[EXT-QSPI-PSRAM] PASS heartbeat malloc_512KB=OK verify_512KB=PASS");
  }
}

#if defined(ESP_PLATFORM) && defined(ARDUINO_ARCH_ESP32)
extern "C" void app_main() {
  initArduino();
  setup();
  while (true) {
    loop();
  }
}
#endif
