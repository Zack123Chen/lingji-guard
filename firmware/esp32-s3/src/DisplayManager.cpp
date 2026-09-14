#include "DisplayManager.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <math.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>

#define sensor_t camera_sensor_t
#include "esp_camera.h"
#undef sensor_t

#include "Pins.h"

namespace DisplayManager {
namespace {
Adafruit_ST7789 g_tft(&SPI, Pins::TFT::CS, Pins::TFT::DC, Pins::TFT::RST);
U8G2_FOR_ADAFRUIT_GFX g_u8g2;
ModuleState g_tft_state;
bool g_dashboard_shell_drawn = false;

constexpr uint32_t kTftSpiFrequency = 4000000;
constexpr uint8_t kTftRotation = 0;
constexpr uint16_t kSleepOutDelayMs = 120;
constexpr uint16_t kPanelCommandDelayMs = 10;
constexpr uint16_t kDisplayOnDelayMs = 20;
constexpr int16_t kHrCardX = 10;
constexpr int16_t kTempCardX = 125;
constexpr int16_t kCardY = 48;
constexpr int16_t kCardW = 105;
constexpr int16_t kCardH = 78;
constexpr int16_t kRowX = 12;
constexpr int16_t kRowValueX = 64;
constexpr int16_t kRowW = 164;
constexpr int16_t kRowH = 14;
constexpr int16_t kAudioRowY = 190;
constexpr int16_t kAudioBarX = 150;
constexpr int16_t kAudioBarY = 184;
constexpr int16_t kAudioBarW = 70;
constexpr int16_t kAudioBarH = 7;

const char* pageTitle(Page page) {
  switch (page) {
    case Page::Dashboard:
      return "CareGuard";
    case Page::Network:
      return "Vitals";
    case Page::Gnss:
      return "Cloud";
    case Page::RawSensors:
      return "Location";
    case Page::CameraStatus:
      return "Motion";
  }
  return "CareGuard";
}

uint8_t pageIndex(Page page) {
  switch (page) {
    case Page::Dashboard:
      return 1;
    case Page::Network:
      return 2;
    case Page::Gnss:
      return 3;
    case Page::RawSensors:
      return 4;
    case Page::CameraStatus:
      return 5;
  }
  return 1;
}

bool validNumber(float value) {
  return !isnan(value) && !isinf(value);
}

bool validNumber(double value) {
  return !isnan(value) && !isinf(value);
}

bool isAsciiText(const String& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    if (static_cast<uint8_t>(value[i]) >= 0x80) {
      return false;
    }
  }
  return true;
}

uint16_t statusColor(const ModuleState& state) {
  if (!state.ready) {
    return ST77XX_ORANGE;
  }
  if (state.status.indexOf("Fail") >= 0 || state.status.indexOf("Error") >= 0 ||
      state.status.indexOf("Failed") >= 0 || state.status.indexOf("Offline") >= 0 ||
      state.status.indexOf("missing") >= 0 || state.status.indexOf("Missing") >= 0 ||
      state.status.indexOf("err") >= 0 || state.status.indexOf("no devices") >= 0 ||
      state.status.indexOf("No Response") >= 0) {
    return ST77XX_RED;
  }
  return ST77XX_GREEN;
}

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return g_tft.color565(r, g, b);
}

String fixNumber(float value, uint8_t decimals) {
  if (!validNumber(value)) {
    return "--";
  }
  return String(value, static_cast<unsigned int>(decimals));
}

String fixNumber(double value, uint8_t decimals) {
  if (!validNumber(value)) {
    return "--";
  }
  return String(value, static_cast<unsigned int>(decimals));
}

String releaseTemperatureText(const SensorData& data, const ModuleState& sht_state) {
  if (validNumber(data.temperature_c)) {
    return fixNumber(data.temperature_c, 1);
  }
  if (sht_state.ready) {
    return "读取中";
  }
  return "离线";
}

String motionLabel(const SensorData& data) {
  if (data.motion == "running") {
    return "RUN";
  }
  if (data.motion == "walking") {
    return "WALK";
  }
  if (data.motion == "rest") {
    return "REST";
  }
  return "WAIT";
}

String bpmText(const SensorData& data) {
  if (!data.heart_contact) {
    return "--";
  }
  if (data.heart_bpm <= 0) {
    return "...";
  }
  return String(data.heart_bpm);
}

bool heartContact(const SensorData& data) {
  return data.heart_contact;
}

uint16_t heartColor(const SensorData& data) {
  if (!data.heart_contact) {
    return ST77XX_ORANGE;
  }
  if (data.heart_bpm <= 0) {
    return ST77XX_CYAN;
  }
  return ST77XX_RED;
}

String heartStatus(const SensorData& data) {
  if (!data.heart_contact) {
    return "HR OFF";
  }
  if (data.heart_bpm <= 0) {
    return "LEARNING";
  }
  return "HR ON";
}

String motionScoreText(const SensorData& data) {
  if (!validNumber(data.motion_score)) {
    return "--";
  }
  return fixNumber(data.motion_score, 2);
}

String cadenceText(const SensorData& data) {
  if (data.cadence_spm <= 0.5f) {
    return "--";
  }
  return fixNumber(data.cadence_spm, 0);
}

String stepText(const SensorData& data) {
  return String(static_cast<unsigned long>(data.step_count));
}

String compactSignal(uint32_t value) {
  if (value >= 10000) {
    return String(static_cast<float>(value) / 10000.0f, 1) + "万";
  }
  return String(static_cast<unsigned long>(value));
}

String opticalText(const SensorData& data) {
  return "IR " + compactSignal(data.ir) + " R " + compactSignal(data.red);
}

String legacyMotionLabel(const SensorData& data) {
  if (data.motion == "running") {
    return "RUN";
  }
  if (data.motion == "walking") {
    return "WALK";
  }
  if (data.motion == "rest") {
    return "REST";
  }
  return "REST";
}

uint16_t motionColor(const String& motion) {
  if (motion == "RUN") {
    return ST77XX_ORANGE;
  }
  if (motion == "WALK") {
    return ST77XX_CYAN;
  }
  if (motion == "REST") {
    return ST77XX_GREEN;
  }
  return ST77XX_WHITE;
}

void drawHeader(Page page) {
  g_dashboard_shell_drawn = false;
  g_tft.fillScreen(ST77XX_BLACK);
  g_tft.fillRect(0, 0, Pins::TFT::WIDTH, 26, ST77XX_BLUE);
  g_tft.setTextSize(1);
  g_tft.setTextColor(ST77XX_WHITE, ST77XX_BLUE);
  g_tft.setCursor(8, 8);
  g_tft.print(pageTitle(page));
  g_tft.setCursor(205, 8);
  g_tft.printf("%u/5", pageIndex(page));
}

void drawLine(int16_t y, const String& label, const String& value, uint16_t color = ST77XX_WHITE) {
  if (!g_tft_state.ready) {
    return;
  }
  g_tft.setTextSize(1);
  g_tft.setTextColor(color, ST77XX_BLACK);
  g_tft.setCursor(12, y);
  g_tft.print(label);
  g_tft.setCursor(82, y);
  g_tft.print(value);
}

void drawCenterText(int16_t y, const String& text, uint8_t size, uint16_t color, uint16_t bg = ST77XX_BLACK) {
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  g_tft.setTextSize(size);
  g_tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  const int16_t x = max<int16_t>(0, (Pins::TFT::WIDTH - w) / 2);
  g_tft.setTextColor(color, bg);
  g_tft.setCursor(x, y);
  g_tft.print(text);
  g_tft.setTextSize(1);
}

void drawCard(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border, uint16_t fill = ST77XX_BLACK) {
  g_tft.fillRoundRect(x, y, w, h, 6, fill);
  g_tft.drawRoundRect(x, y, w, h, 6, border);
}

void drawPill(int16_t x, int16_t y, const String& text, uint16_t color) {
  const int16_t w = min<int16_t>(110, 12 + text.length() * 6);
  g_tft.fillRoundRect(x, y, w, 20, 9, color);
  g_tft.setTextSize(1);
  g_tft.setTextColor(ST77XX_BLACK, color);
  g_tft.setCursor(x + 6, y + 6);
  g_tft.print(text.substring(0, 16));
}

void drawBigMetric(int16_t x,
                   int16_t y,
                   const String& label,
                   const String& value,
                   const String& unit,
                   uint16_t color) {
  g_tft.setTextSize(1);
  g_tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  g_tft.setCursor(x, y);
  g_tft.print(label);

  g_tft.setTextSize(3);
  g_tft.setTextColor(color, ST77XX_BLACK);
  g_tft.setCursor(x, y + 16);
  g_tft.print(value);

  g_tft.setTextSize(1);
  g_tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  g_tft.setCursor(x + 4 + value.length() * 18, y + 34);
  g_tft.print(unit);
}

void drawStatusDots(const ModuleState& mpu_state,
                    const ModuleState& sht_state,
                    const ModuleState& max_state,
                    const ModuleState& air_state,
                    const ModuleState& camera_state) {
  const int16_t y = 222;
  const char* labels[] = {"M", "T", "H", "4G", "C"};
  const ModuleState* states[] = {&mpu_state, &sht_state, &max_state, &air_state, &camera_state};
  int16_t x = 8;
  for (uint8_t i = 0; i < 5; ++i) {
    const uint16_t color = statusColor(*states[i]);
    g_tft.fillCircle(x + 5, y + 5, 4, color);
    g_tft.setTextSize(1);
    g_tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    g_tft.setCursor(x + 13, y + 2);
    g_tft.print(labels[i]);
    x += (i == 3) ? 45 : 38;
  }
}

void useChineseFont() {
  g_u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  g_u8g2.setFontMode(1);
  g_u8g2.setFontDirection(0);
}

void drawCn(int16_t x, int16_t baseline, const char* text, uint16_t color, uint16_t bg = ST77XX_BLACK) {
  useChineseFont();
  g_u8g2.setForegroundColor(color);
  g_u8g2.setBackgroundColor(bg);
  g_u8g2.setCursor(x, baseline);
  g_u8g2.print(text);
}

void drawCenterCn(int16_t y, const char* text, uint16_t color, uint16_t bg = ST77XX_BLACK) {
  useChineseFont();
  const int16_t w = g_u8g2.getUTF8Width(text);
  const int16_t x = max<int16_t>(0, (Pins::TFT::WIDTH - w) / 2);
  drawCn(x, y, text, color, bg);
}

void drawCnInRect(int16_t x,
                  int16_t y,
                  int16_t w,
                  int16_t h,
                  const String& text,
                  uint16_t color,
                  uint16_t bg,
                  bool centered = false) {
  g_tft.fillRect(x, y, w, h, bg);
  useChineseFont();
  const int16_t text_w = g_u8g2.getUTF8Width(text.c_str());
  const int16_t text_x = centered ? x + max<int16_t>(0, (w - text_w) / 2) : x;
  const int16_t baseline = y + h - 4;
  drawCn(text_x, baseline, text.c_str(), color, bg);
}

const char* motionCn(const SensorData& data) {
  if (data.motion == "running") {
    return "快速奔跑";
  }
  if (data.motion == "walking") {
    return "日常慢步";
  }
  if (data.motion == "rest") {
    return "安静休息";
  }
  return "等待数据";
}

const char* cloudLabel(const ModuleState& air_state) {
  if (air_state.status.indexOf("Pub OK") >= 0) {
    return "已上报";
  }
  if (air_state.status.indexOf("MQTT OK") >= 0) {
    return "云端在线";
  }
  if (air_state.status.indexOf("Net Ready") >= 0) {
    return "网络就绪";
  }
  if (air_state.status.indexOf("SIM Error") >= 0) {
    return "SIM异常";
  }
  if (air_state.status.indexOf("No Response") >= 0) {
    return "模块离线";
  }
  if (air_state.status.indexOf("Fail") >= 0 || air_state.status.indexOf("Offline") >= 0) {
    return "上报中断";
  }
  return "连接中";
}

uint16_t motionCnColor(const SensorData& data) {
  if (data.motion == "running") {
    return ST77XX_ORANGE;
  }
  if (data.motion == "walking") {
    return ST77XX_CYAN;
  }
  if (data.motion == "rest") {
    return ST77XX_GREEN;
  }
  return ST77XX_WHITE;
}

bool hasReliableHeartRate(const SensorData& data) {
  return data.heart_contact && data.heart_bpm > 0 && data.heart_sample_type != "no_contact";
}

String releaseHeartValue(const SensorData& data) {
  if (data.heart_bpm > 0) {
    return String(data.heart_bpm);
  }
  return data.heart_contact ? "检测中" : "未佩戴";
}

const char* heartSampleCn(const SensorData& data) {
  if (!data.heart_contact || data.heart_sample_type == "no_contact") {
    return "未接触";
  }
  if (data.heart_sample_type == "contact_settling") {
    return "贴合中";
  }
  if (data.heart_sample_type == "contact_learning") {
    return "学习中";
  }
  if (data.heart_sample_type == "weak_signal") {
    return "信号弱";
  }
  if (data.heart_sample_type == "signal_timeout") {
    return "等待波形";
  }
  if (data.heart_sample_type == "beat_detected") {
    return "已检测";
  }
  if (data.heart_sample_type == "beat_avg") {
    return "稳定";
  }
  if (data.heart_sample_type == "beat_reject") {
    return "抗干扰";
  }
  return "检测中";
}

bool isHealthAlarm(const SensorData& data) {
  return (validNumber(data.temperature_c) && data.temperature_c > 40.0f) ||
         (hasReliableHeartRate(data) && data.heart_bpm > 150);
}

String clipped(const String& value, uint8_t max_chars) {
  if (value.length() <= max_chars) {
    return value;
  }
  return value.substring(0, max_chars);
}

String networkLabel(const ModuleState& air_state) {
  if (air_state.status.indexOf("Pub OK") >= 0) {
    return "PUB OK";
  }
  if (air_state.status.indexOf("MQTT OK") >= 0) {
    return "MQTT OK";
  }
  if (air_state.status.indexOf("Net Ready") >= 0) {
    return "NET OK";
  }
  if (air_state.status.indexOf("SIM") >= 0) {
    return "SIM ERR";
  }
  if (air_state.status.indexOf("No Response") >= 0) {
    return "NO AT";
  }
  if (air_state.status.indexOf("Fail") >= 0 || air_state.status.indexOf("Offline") >= 0) {
    return "NET FAIL";
  }
  return "NET WAIT";
}

String networkCn(const ModuleState& air_state) {
  if (air_state.status.indexOf("Pub OK") >= 0) {
    return "已上报";
  }
  if (air_state.status.indexOf("MQTT OK") >= 0) {
    return "云端在线";
  }
  if (air_state.status.indexOf("Net Ready") >= 0) {
    return "网络就绪";
  }
  if (air_state.status.indexOf("SIM") >= 0) {
    return "SIM异常";
  }
  if (air_state.status.indexOf("No Response") >= 0) {
    return "模块无响应";
  }
  if (air_state.status.indexOf("Fail") >= 0 || air_state.status.indexOf("Offline") >= 0) {
    return "上报失败";
  }
  return "连接中";
}

String heartValue(const ModuleState& max_state, const SensorData& data) {
  if (data.heart_bpm > 0) {
    return String(data.heart_bpm);
  }
  if (data.heart_contact) {
    return "检测中";
  }
  if (data.ir > 0 || data.red > 0) {
    return "待佩戴";
  }
  if (!max_state.ready) {
    return "等待";
  }
  return "就绪";
}

String heartDetail(const ModuleState& max_state, const SensorData& data) {
  if (data.heart_bpm > 0) {
    return "心率稳定";
  }
  if (data.heart_contact) {
    return heartSampleCn(data);
  }
  if (data.ir > 0 || data.red > 0) {
    return "请贴合传感器";
  }
  if (!max_state.ready) {
    return "心率等待";
  }
  return "模块正常";
}

uint16_t heartCardColor(const ModuleState& max_state, const SensorData& data) {
  if (data.heart_bpm > 0) {
    return ST77XX_RED;
  }
  if (data.heart_contact) {
    return ST77XX_CYAN;
  }
  if (data.ir > 0 || data.red > 0) {
    return ST77XX_ORANGE;
  }
  if (!max_state.ready) {
    return ST77XX_ORANGE;
  }
  return ST77XX_ORANGE;
}

String temperatureValue(const ModuleState& sht_state, const SensorData& data) {
  if (validNumber(data.temperature_c)) {
    return fixNumber(data.temperature_c, 1);
  }
  return sht_state.ready ? "读取中" : "等待";
}

String temperatureDetail(const ModuleState& sht_state, const SensorData& data) {
  if (validNumber(data.temperature_c)) {
    return "体温正常采样";
  }
  return sht_state.ready ? "等待数据" : "温度等待";
}

String compactSignalAscii(uint32_t value) {
  if (value >= 10000) {
    return String(static_cast<float>(value) / 10000.0f, 1) + "w";
  }
  return String(static_cast<unsigned long>(value));
}

String opticalCompactAscii(const SensorData& data) {
  return "IR " + compactSignalAscii(data.ir) + " R " + compactSignalAscii(data.red);
}

String locationCn(const GnssData& gnss) {
  return (gnss.has_fix && validNumber(gnss.lat) && validNumber(gnss.lng)) ? "卫星定位" : "默认点";
}

uint16_t locationColor(const GnssData& gnss) {
  return (gnss.has_fix && validNumber(gnss.lat) && validNumber(gnss.lng)) ? ST77XX_GREEN : ST77XX_ORANGE;
}

String storageCn(const ModuleState& storage_state, const StorageData& storage) {
  (void)storage;
  if (storage_state.ready) {
    return "记录中";
  }
  if (storage_state.status.indexOf("Missing") >= 0) {
    return "未插卡";
  }
  if (storage_state.status.indexOf("Fail") >= 0) {
    return "卡异常";
  }
  return "等待卡";
}

String noiseCn(const AudioData& audio) {
  String text = String(static_cast<unsigned>(audio.noise_level));
  text += "% ";
  if (audio.noise_event == "loud") {
    text += "嘈杂";
  } else if (audio.noise_event == "active") {
    text += "有声";
  } else {
    text += "安静";
  }
  return text;
}

uint16_t noiseColor(const AudioData& audio) {
  if (audio.noise_event == "loud") {
    return ST77XX_ORANGE;
  }
  if (audio.noise_event == "active") {
    return ST77XX_CYAN;
  }
  return ST77XX_GREEN;
}

String commandCn(const RuntimeStatus& runtime) {
  String text = runtime.eco_mode ? "省电 " : "正常 ";
  if (runtime.last_command == "none") {
    text += "待命";
  } else {
    text += runtime.last_command;
  }
  return clipped(text, 18);
}

void drawAsciiRow(int16_t y, const char* label, const String& value, uint16_t color = ST77XX_WHITE) {
  g_tft.setTextSize(1);
  g_tft.setTextColor(rgb(176, 190, 197), ST77XX_BLACK);
  g_tft.setCursor(12, y);
  g_tft.print(label);
  g_tft.setTextColor(color, ST77XX_BLACK);
  g_tft.setCursor(54, y);
  g_tft.print(clipped(value, 28));
}

void drawReleaseHeader(const ModuleState& air_state, bool alarm) {
  const uint16_t header = alarm ? rgb(132, 38, 38) : rgb(0, 122, 128);
  g_tft.fillRect(0, 0, Pins::TFT::WIDTH, 34, header);
  drawCn(10, 24, "宠爱云护", ST77XX_WHITE, header);

  const uint16_t pill = statusColor(air_state);
  g_tft.fillRoundRect(150, 7, 80, 20, 9, pill);
  drawCnInRect(156, 8, 68, 17, networkCn(air_state), ST77XX_BLACK, pill, true);
}

void invalidateDashboardShell() {
  g_dashboard_shell_drawn = false;
}

void rawTftCommand(uint8_t command) {
  SPI.beginTransaction(SPISettings(kTftSpiFrequency, MSBFIRST, SPI_MODE0));
  digitalWrite(Pins::TFT::CS, LOW);
  digitalWrite(Pins::TFT::DC, LOW);
  SPI.transfer(command);
  digitalWrite(Pins::TFT::CS, HIGH);
  digitalWrite(Pins::TFT::DC, HIGH);
  SPI.endTransaction();
}

void applyPanelState() {
  digitalWrite(Pins::TFT::BL, HIGH);
  g_tft.enableSleep(false);
  delay(kSleepOutDelayMs);

  g_tft.setRotation(kTftRotation);

  // This panel's normal color state is the ST7789 INVON command.
  rawTftCommand(ST77XX_INVON);
  delay(kPanelCommandDelayMs);

  g_tft.enableDisplay(true);
  delay(kDisplayOnDelayMs);
}

void reinitializeDisplayController() {
  SPI.end();
  delay(2);
  SPI.begin(Pins::TFT::SCLK, Pins::TFT::MISO, Pins::TFT::MOSI, Pins::TFT::CS);
  pinMode(Pins::TFT::CS, OUTPUT);
  digitalWrite(Pins::TFT::CS, HIGH);
  g_tft.setSPISpeed(kTftSpiFrequency);
  g_tft.init(Pins::TFT::WIDTH, Pins::TFT::HEIGHT, SPI_MODE0);
  applyPanelState();
  g_tft.setTextWrap(false);
  g_u8g2.begin(g_tft);
  useChineseFont();
}

uint16_t bgColor() {
  return rgb(5, 10, 16);
}

uint16_t panelColor() {
  return rgb(14, 25, 35);
}

uint16_t panelSoftColor() {
  return rgb(9, 17, 25);
}

uint16_t mutedColor() {
  return rgb(152, 170, 178);
}

void drawReleaseShell(bool alarm) {
  g_tft.fillScreen(bgColor());
  drawReleaseHeader(ModuleState{}, alarm);

  g_tft.fillRoundRect(kHrCardX, kCardY, kCardW, kCardH, 7, panelColor());
  g_tft.drawRoundRect(kHrCardX, kCardY, kCardW, kCardH, 7, rgb(34, 72, 82));
  drawCn(kHrCardX + 10, kCardY + 19, "心率", mutedColor(), panelColor());

  g_tft.fillRoundRect(kTempCardX, kCardY, kCardW, kCardH, 7, panelColor());
  g_tft.drawRoundRect(kTempCardX, kCardY, kCardW, kCardH, 7, rgb(34, 72, 82));
  drawCn(kTempCardX + 10, kCardY + 19, "体温", mutedColor(), panelColor());

  g_tft.fillRoundRect(10, 134, 220, 74, 6, panelSoftColor());
  g_tft.drawRoundRect(10, 134, 220, 74, 6, rgb(25, 45, 55));
  drawCn(kRowX, 148, "定位", mutedColor(), panelSoftColor());
  drawCn(kRowX, 162, "心率光", mutedColor(), panelSoftColor());
  drawCn(kRowX, 176, "存储", mutedColor(), panelSoftColor());
  drawCn(kRowX, 190, "音量", mutedColor(), panelSoftColor());
  drawCn(kRowX, 204, "指令", mutedColor(), panelSoftColor());

  g_tft.fillRect(0, 214, Pins::TFT::WIDTH, 26, bgColor());
  const char* labels[] = {"动", "温", "心", "网", "摄"};
  int16_t x = 18;
  for (uint8_t i = 0; i < 5; ++i) {
    g_tft.drawCircle(x, 224, 6, rgb(70, 88, 96));
    drawCn(x + 10, 229, labels[i], mutedColor(), bgColor());
    x += 43;
  }

  g_dashboard_shell_drawn = true;
}

void drawAsciiValueInRect(int16_t x,
                          int16_t y,
                          int16_t w,
                          int16_t h,
                          const String& value,
                          uint8_t size,
                          uint16_t color,
                          uint16_t bg) {
  g_tft.fillRect(x, y, w, h, bg);
  g_tft.setTextSize(size);
  g_tft.setTextColor(color, bg);
  g_tft.setCursor(x, y + max<int16_t>(0, (h - 8 * size) / 2));
  g_tft.print(value);
  g_tft.setTextSize(1);
}

void drawValueInRect(int16_t x,
                     int16_t y,
                     int16_t w,
                     int16_t h,
                     const String& value,
                     uint8_t ascii_size,
                     uint16_t color,
                     uint16_t bg) {
  if (isAsciiText(value)) {
    drawAsciiValueInRect(x, y, w, h, value, ascii_size, color, bg);
  } else {
    drawCnInRect(x, y, w, h, value, color, bg);
  }
}

void drawReleaseRow(int16_t y, const String& value, uint16_t color) {
  drawValueInRect(kRowValueX, y - 11, kRowW, kRowH, value, 1, color, panelSoftColor());
}

void drawAudioBar(const AudioData& audio, uint16_t color) {
  g_tft.drawRect(kAudioBarX, kAudioBarY, kAudioBarW, kAudioBarH, rgb(44, 65, 74));
  g_tft.fillRect(kAudioBarX + 1, kAudioBarY + 1, kAudioBarW - 2, kAudioBarH - 2, panelSoftColor());
  const int16_t fill_w = map(static_cast<int>(audio.noise_level), 0, 100, 0, kAudioBarW - 2);
  if (fill_w > 0) {
    g_tft.fillRect(kAudioBarX + 1, kAudioBarY + 1, fill_w, kAudioBarH - 2, color);
  }
}

void drawReleaseDots(const ModuleState& mpu_state,
                     const ModuleState& sht_state,
                     const ModuleState& max_state,
                     const ModuleState& air_state,
                     const ModuleState& camera_state,
                     const SensorData& data) {
  const ModuleState* states[] = {&mpu_state, &sht_state, &max_state, &air_state, &camera_state};
  int16_t x = 18;
  for (uint8_t i = 0; i < 5; ++i) {
    uint16_t dot_color = statusColor(*states[i]);
    if (i == 1 && validNumber(data.temperature_c)) {
      dot_color = ST77XX_GREEN;
    } else if (i == 2 && (data.ir > 0 || data.red > 0)) {
      dot_color = ST77XX_GREEN;
    }
    g_tft.fillCircle(x, 224, 5, dot_color);
    x += 43;
  }
}

void drawMetricCard(int16_t x,
                    int16_t y,
                    int16_t w,
                    const char* label,
                    const String& value,
                    const char* unit,
                    uint16_t color,
                    bool muted = false) {
  const uint16_t fill = rgb(17, 24, 31);
  const uint16_t border = muted ? rgb(96, 79, 35) : rgb(37, 56, 65);
  g_tft.fillRoundRect(x, y, w, 70, 7, fill);
  g_tft.drawRoundRect(x, y, w, 70, 7, border);
  drawCn(x + 9, y + 18, label, rgb(176, 190, 197), fill);

  if (isAsciiText(value)) {
    g_tft.setTextSize(value.length() > 3 ? 2 : 3);
    g_tft.setTextColor(color, fill);
    g_tft.setCursor(x + 9, y + 34);
    g_tft.print(value);
  } else {
    drawCn(x + 14, y + 50, value.c_str(), color, fill);
  }

  g_tft.setTextSize(1);
  g_tft.setTextColor(rgb(176, 190, 197), fill);
  g_tft.setCursor(x + w - 34, y + 52);
  g_tft.print(unit);
}

void drawInfoRow(int16_t y, const char* label, const String& value, uint16_t color = ST77XX_WHITE) {
  drawCn(13, y, label, rgb(176, 190, 197));
  drawCn(76, y, value.c_str(), color);
}

void drawReleaseStatusDots(const ModuleState& mpu_state,
                           const ModuleState& sht_state,
                           const ModuleState& max_state,
                           const ModuleState& air_state,
                           const ModuleState& camera_state) {
  const char* labels[] = {"动", "温", "心", "网", "摄"};
  const ModuleState* states[] = {&mpu_state, &sht_state, &max_state, &air_state, &camera_state};
  int16_t x = 16;
  for (uint8_t i = 0; i < 5; ++i) {
    const uint16_t color = statusColor(*states[i]);
    g_tft.fillCircle(x, 222, 5, color);
    drawCn(x + 8, 227, labels[i], ST77XX_WHITE);
    x += 44;
  }
}

void drawReleaseDashboard(const ModuleState& mpu_state,
                          const ModuleState& i2c_state,
                          const ModuleState& sht_state,
                          const ModuleState& max_state,
                          const ModuleState& air_state,
                          const ModuleState& camera_state,
                          const GnssData& gnss,
                          const SensorData& data,
                          const ModuleState& storage_state,
                          const StorageData& storage,
                          const ModuleState& audio_state,
                          const AudioData& audio,
                          const RuntimeStatus& runtime) {
  const bool alarm = isHealthAlarm(data);
  if (!g_dashboard_shell_drawn) {
    drawReleaseShell(alarm);
  }
  drawReleaseHeader(air_state, alarm);

  const uint16_t card_fill = panelColor();
  const String hr_value = heartValue(max_state, data);
  const uint16_t hr_color = heartCardColor(max_state, data);
  g_tft.drawRoundRect(kHrCardX, kCardY, kCardW, kCardH, 7, hr_color);
  drawValueInRect(kHrCardX + 10, 72, 88, 28, hr_value, data.heart_bpm > 0 ? 3 : 1, hr_color, card_fill);
  drawCnInRect(kHrCardX + 10, 104, 86, 15, heartDetail(max_state, data), mutedColor(), card_fill);

  const String temp_value = temperatureValue(sht_state, data);
  const uint16_t temp_color = validNumber(data.temperature_c) ? ST77XX_CYAN : ST77XX_ORANGE;
  g_tft.drawRoundRect(kTempCardX, kCardY, kCardW, kCardH, 7, temp_color);
  drawValueInRect(kTempCardX + 10, 72, 70, 28, temp_value, validNumber(data.temperature_c) ? 3 : 1, temp_color, card_fill);
  drawAsciiValueInRect(kTempCardX + 82, 88, 16, 16, "C", 1, mutedColor(), card_fill);
  drawCnInRect(kTempCardX + 10, 104, 86, 15, temperatureDetail(sht_state, data), mutedColor(), card_fill);

  drawReleaseRow(148, locationCn(gnss), locationColor(gnss));
  drawReleaseRow(162, opticalCompactAscii(data), data.ir > 0 ? ST77XX_WHITE : statusColor(max_state));
  drawReleaseRow(176, storageCn(storage_state, storage), statusColor(storage_state));
  drawAudioLevel(audio_state, audio);
  drawReleaseRow(204, commandCn(runtime), runtime.last_command_result == "ok" ? ST77XX_GREEN : ST77XX_WHITE);
  drawReleaseDots(mpu_state, sht_state, max_state, air_state, camera_state, data);
  (void)i2c_state;
}
}  // namespace

void init() {
  pinMode(Pins::TFT::BL, OUTPUT);
  digitalWrite(Pins::TFT::BL, HIGH);

  reinitializeDisplayController();
  g_dashboard_shell_drawn = false;
  g_tft_state.ready = true;
  g_tft_state.status = "TFT OK";
  Serial.printf("TFT initialized at %lu Hz SPI.\n", static_cast<unsigned long>(kTftSpiFrequency));

  g_tft.fillScreen(ST77XX_BLACK);
  g_tft.fillRect(0, 0, Pins::TFT::WIDTH, 34, rgb(0, 137, 145));
  g_tft.setTextSize(2);
  g_tft.setTextColor(ST77XX_WHITE, rgb(0, 137, 145));
  g_tft.setCursor(10, 9);
  g_tft.print("CareGuard");
  g_tft.setTextSize(3);
  g_tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);
  drawCenterCn(96, "实时看护", ST77XX_YELLOW, ST77XX_BLACK);
  drawCenterCn(124, "主屏启动", ST77XX_WHITE, ST77XX_BLACK);
  g_dashboard_shell_drawn = false;
}

void drawSelfTest(const char* step,
                  const ModuleState& i2c_state,
                  const ModuleState& mpu_state,
                  const ModuleState& sht_state,
                  const ModuleState& max_state,
                  const ModuleState& air_state,
                  const ModuleState& camera_state) {
  if (!g_tft_state.ready) {
    return;
  }

  g_dashboard_shell_drawn = false;
  g_tft.fillScreen(ST77XX_BLACK);
  g_tft.fillRect(0, 0, Pins::TFT::WIDTH, 34, rgb(22, 79, 69));
  drawCn(10, 23, "宠爱云护", ST77XX_WHITE, rgb(22, 79, 69));
  g_tft.fillRoundRect(142, 7, 84, 20, 9, ST77XX_YELLOW);
  drawCn(146, 23, "自检中", ST77XX_BLACK, ST77XX_YELLOW);

  drawCenterCn(57, step, rgb(110, 231, 183));

  const char* labels[] = {"TFT", "I2C", "运动", "温度", "心率", "4G", "摄像头"};
  const ModuleState* states[] = {
      &g_tft_state, &i2c_state, &mpu_state, &sht_state, &max_state, &air_state, &camera_state};
  for (uint8_t i = 0; i < 7; ++i) {
    const int16_t y = 78 + i * 20;
    const uint16_t color = statusColor(*states[i]);
    g_tft.fillCircle(24, y - 4, 5, color);
    drawCn(38, y, labels[i], ST77XX_WHITE);
    drawCn(102, y, states[i]->status.c_str(), color);
  }

  g_tft.fillRect(0, 218, Pins::TFT::WIDTH, 22, rgb(17, 24, 31));
  drawCenterCn(234, "答辩演示模式", rgb(176, 190, 197), rgb(17, 24, 31));
}

void drawDashboard(const ModuleState& i2c_state,
                   const ModuleState& mpu_state,
                   const ModuleState& sht_state,
                   const ModuleState& max_state,
                   const ModuleState& air_state,
                   const ModuleState& camera_state,
                   const SensorData& data) {
  drawPage(Page::Dashboard,
           i2c_state,
           mpu_state,
           sht_state,
           max_state,
           air_state,
           camera_state,
           GnssData{},
           data);
}

void drawPage(Page page,
              const ModuleState& i2c_state,
              const ModuleState& mpu_state,
              const ModuleState& sht_state,
              const ModuleState& max_state,
              const ModuleState& air_state,
              const ModuleState& camera_state,
              const GnssData& gnss,
              const SensorData& data,
              const ModuleState& storage_state,
              const StorageData& storage,
              const ModuleState& audio_state,
              const AudioData& audio,
              const RuntimeStatus& runtime) {
  if (!g_tft_state.ready) {
    return;
  }

  if (page == Page::Dashboard) {
    drawReleaseDashboard(mpu_state,
                         i2c_state,
                         sht_state,
                         max_state,
                         air_state,
                         camera_state,
                         gnss,
                         data,
                         storage_state,
                         storage,
                         audio_state,
                         audio,
                         runtime);
    return;
  }

  drawHeader(page);
  const String motion = motionLabel(data);
  const bool contact = heartContact(data);
  char buf[64];

  switch (page) {
    case Page::Dashboard:
      break;

    case Page::Network:
      drawCenterText(42, bpmText(data), 5, heartColor(data));
      drawCenterText(96, contact ? data.heart_sample_type : "No heart contact", 1, ST77XX_WHITE);
      drawCard(18, 128, 204, 74, contact ? ST77XX_GREEN : ST77XX_ORANGE);
      drawLine(144, "IR", String(static_cast<unsigned long>(data.ir)), contact ? ST77XX_GREEN : ST77XX_ORANGE);
      drawLine(164, "RED", String(static_cast<unsigned long>(data.red)), ST77XX_WHITE);
      drawLine(184, "Sample", data.heart_sample_type, contact ? ST77XX_GREEN : ST77XX_ORANGE);
      drawStatusDots(mpu_state, sht_state, max_state, air_state, camera_state);
      break;

    case Page::Gnss:
      drawCenterText(42, air_state.status, 2, statusColor(air_state));
      drawCard(16, 82, 212, 90, statusColor(air_state));
      drawLine(102, "Broker", "EMQX China");
      drawLine(122, "Topic", "HIT/PetData");
      drawLine(142, "Rate", "5 sec");
      drawLine(190, "Mode", "Real hardware", ST77XX_GREEN);
      drawStatusDots(mpu_state, sht_state, max_state, air_state, camera_state);
      break;

    case Page::RawSensors:
      drawCenterText(40, gnss.has_fix ? "GPS FIXED" : "GPS SEARCH", 2, gnss.has_fix ? ST77XX_GREEN : ST77XX_ORANGE);
      if (gnss.has_fix) {
        drawCard(16, 82, 212, 86, ST77XX_GREEN);
        drawLine(104, "Lat", fixNumber(gnss.lat, 6));
        drawLine(126, "Lng", fixNumber(gnss.lng, 6));
        drawLine(188, "Upload", "Location enabled", ST77XX_GREEN);
      } else {
        drawCard(16, 82, 212, 86, ST77XX_ORANGE);
        drawCenterText(104, "--", 4, ST77XX_ORANGE);
        drawCenterText(150, "No fake location", 1, ST77XX_WHITE);
        drawLine(190, "GNSS", "Searching", ST77XX_ORANGE);
      }
      drawStatusDots(mpu_state, sht_state, max_state, air_state, camera_state);
      break;

    case Page::CameraStatus:
      snprintf(buf, sizeof(buf), "%.1f", fabsf(data.ax) + fabsf(data.ay) + fabsf(data.az));
      drawCenterText(40, motion, 3, motionColor(motion));
      drawCard(18, 94, 204, 88, motionColor(motion));
      drawLine(112, "Score", motionScoreText(data));
      drawLine(132, "Steps", stepText(data));
      drawLine(152, "Cadence", cadenceText(data) + " spm");
      drawLine(174, "Accel", String(buf));
      drawLine(202, "Camera", camera_state.status, statusColor(camera_state));
      drawStatusDots(mpu_state, sht_state, max_state, air_state, camera_state);
      break;
  }
}

void drawHeartFocus(const ModuleState& air_state,
                    const ModuleState& camera_state,
                    const SensorData& data) {
  if (!g_tft_state.ready) {
    return;
  }

  g_dashboard_shell_drawn = false;
  g_tft.fillScreen(ST77XX_BLACK);
  g_tft.fillRect(0, 0, Pins::TFT::WIDTH, 34, ST77XX_RED);
  drawCn(10, 23, "心率监测", ST77XX_WHITE, ST77XX_RED);
  drawCn(190, 23, "实时", ST77XX_WHITE, ST77XX_RED);

  drawCenterText(48, String(data.heart_bpm), 6, ST77XX_RED);
  drawCenterText(112, "BPM", 2, ST77XX_WHITE);

  drawCard(18, 148, 204, 52, ST77XX_RED);
  drawInfoRow(166, "状态", data.heart_sample_type, ST77XX_GREEN);
  drawLine(182, "IR", String(static_cast<unsigned long>(data.ir)), ST77XX_WHITE);

  g_tft.setTextSize(1);
  g_tft.setTextColor(statusColor(air_state), ST77XX_BLACK);
  g_tft.setCursor(8, 222);
  g_tft.print(air_state.status);
  g_tft.setTextColor(statusColor(camera_state), ST77XX_BLACK);
  g_tft.setCursor(150, 222);
  g_tft.print(camera_state.status);
}

void drawAudioLevel(const ModuleState& audio_state, const AudioData& audio) {
  if (!g_tft_state.ready || !g_dashboard_shell_drawn) {
    return;
  }

  const uint16_t color = statusColor(audio_state) == ST77XX_RED ? ST77XX_RED : noiseColor(audio);
  drawValueInRect(kRowValueX, kAudioRowY - 11, 82, kRowH, noiseCn(audio), 1, color, panelSoftColor());
  drawAudioBar(audio, color);
}

void forceFullRefresh() {
  if (!g_tft_state.ready) {
    return;
  }
  reinitializeDisplayController();
  invalidateDashboardShell();
}

void drawDiagnostic() {
  if (!g_tft_state.ready) {
    return;
  }

  invalidateDashboardShell();
  g_tft.fillScreen(ST77XX_BLACK);
  g_tft.fillRect(0, 0, 120, 120, ST77XX_BLACK);
  g_tft.fillRect(120, 0, 120, 120, ST77XX_WHITE);
  g_tft.fillRect(0, 120, 120, 120, ST77XX_RED);
  g_tft.fillRect(120, 120, 120, 120, ST77XX_CYAN);
  g_tft.setTextSize(1);
  g_tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  g_tft.setCursor(8, 8);
  g_tft.print("BLACK");
  g_tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
  g_tft.setCursor(128, 8);
  g_tft.print("WHITE");
  g_tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  g_tft.setCursor(8, 224);
  g_tft.print("RED");
  g_tft.setTextColor(ST77XX_BLACK, ST77XX_CYAN);
  g_tft.setCursor(128, 224);
  g_tft.print("CYAN");

  g_tft.fillRoundRect(30, 84, 180, 72, 8, ST77XX_BLACK);
  g_tft.drawRoundRect(30, 84, 180, 72, 8, ST77XX_WHITE);
  g_tft.setTextSize(2);
  g_tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  g_tft.setCursor(58, 101);
  g_tft.print("TFT TEST");
  g_tft.setTextSize(1);
  g_tft.setCursor(68, 132);
  g_tft.print("INVON 0x21");
}

void drawLocatorFlash() {
  if (!g_tft_state.ready) {
    return;
  }

  g_dashboard_shell_drawn = false;
  g_tft.fillScreen(ST77XX_YELLOW);
  drawCenterCn(86, "寻宠闪光", ST77XX_BLACK, ST77XX_YELLOW);
  drawCenterCn(118, "请听蜂鸣声", ST77XX_BLACK, ST77XX_YELLOW);
  g_tft.setTextSize(2);
  g_tft.setTextColor(ST77XX_BLACK, ST77XX_YELLOW);
  g_tft.setCursor(42, 150);
  g_tft.print("CareGuard");
  g_tft.setTextSize(1);
}

void drawCameraFrame(const CameraFrame& frame) {
  if (!g_tft_state.ready) {
    return;
  }

  if (frame.format != PIXFORMAT_GRAYSCALE || frame.data == nullptr ||
      frame.width != 160 || frame.height != 120 || frame.len != 160UL * 120UL) {
    drawCameraCaptureFailed();
    return;
  }

  constexpr int16_t kPreviewX = 40;
  constexpr int16_t kPreviewY = 55;
  constexpr uint16_t kPreviewW = 160;
  constexpr uint16_t kPreviewH = 120;

  g_dashboard_shell_drawn = false;
  g_tft.fillScreen(ST77XX_BLACK);
  drawCenterCn(28, "即时图像预览", ST77XX_CYAN);
  g_tft.drawRect(kPreviewX - 1, kPreviewY - 1, kPreviewW + 2, kPreviewH + 2, ST77XX_CYAN);

  uint16_t line[kPreviewW];
  for (uint16_t y = 0; y < kPreviewH; ++y) {
    const uint8_t* src = frame.data + static_cast<size_t>(y) * kPreviewW;
    for (uint16_t x = 0; x < kPreviewW; ++x) {
      const uint8_t gray = src[x];
      line[x] = static_cast<uint16_t>(((gray & 0xF8) << 8) |
                                      ((gray & 0xFC) << 3) |
                                      (gray >> 3));
    }
    g_tft.drawRGBBitmap(kPreviewX, kPreviewY + y, line, kPreviewW, 1);
  }

  drawCenterCn(208, "仅本地显示，未存档", ST77XX_ORANGE);
  g_tft.setTextSize(1);
}

void drawCameraCaptureFailed() {
  drawCameraStatus("图像采集失败", "即将返回主页", ST77XX_RED);
}

void drawCameraStatus(const char* title, const char* subtitle, uint16_t color) {
  if (!g_tft_state.ready) {
    return;
  }

  g_dashboard_shell_drawn = false;
  g_tft.fillScreen(ST77XX_BLACK);
  drawCenterCn(82, title && title[0] ? title : "相机状态", color);
  drawCenterCn(118, subtitle && subtitle[0] ? subtitle : "仅本地预览", ST77XX_WHITE);
  g_tft.drawRect(24, 150, 192, 34, color);
  drawCenterCn(174, "不保存，不上传", color);
  g_tft.setTextSize(1);
}

const ModuleState& state() {
  return g_tft_state;
}
}  // namespace DisplayManager
