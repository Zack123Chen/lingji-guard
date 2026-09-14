#include "CommandConsole.h"

namespace CommandConsole {
namespace {
String g_buffer;
String g_last_input;
}  // namespace

void init() {
  g_buffer.reserve(32);
  printHelp();
}

Command poll() {
  while (Serial.available() > 0) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      g_last_input = g_buffer;
      g_last_input.trim();
      g_last_input.toLowerCase();
      g_buffer = "";

      if (g_last_input.length() == 0) {
        return Command::None;
      }
      if (g_last_input == "help" || g_last_input == "?") {
        return Command::Help;
      }
      if (g_last_input == "i2c") {
        return Command::I2c;
      }
      if (g_last_input == "sensors") {
        return Command::Sensors;
      }
      if (g_last_input == "air") {
        return Command::Air;
      }
      if (g_last_input == "pub") {
        return Command::Pub;
      }
      if (g_last_input == "cam") {
        return Command::Cam;
      }
      if (g_last_input == "camdump") {
        return Command::CamDump;
      }
      if (g_last_input == "camjpeg") {
        return Command::CamJpeg;
      }
      if (g_last_input == "btn" || g_last_input == "button") {
        return Command::Button;
      }
      if (g_last_input == "tf" || g_last_input == "sd") {
        return Command::Tf;
      }
      if (g_last_input == "tft" || g_last_input == "screen" || g_last_input == "lcd") {
        return Command::Tft;
      }
      if (g_last_input == "snap" || g_last_input == "snapshot") {
        return Command::Snap;
      }
      if (g_last_input == "status") {
        return Command::Status;
      }
      return Command::Unknown;
    }

    if (isPrintable(ch) && g_buffer.length() < 48) {
      g_buffer += ch;
    }
  }

  return Command::None;
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  help     - show this list");
  Serial.println("  i2c      - scan I2C bus");
  Serial.println("  sensors  - read sensors now");
  Serial.println("  air      - query Air780E now");
  Serial.println("  pub      - publish one telemetry JSON by MQTT");
  Serial.println("  cam      - retry camera init and show camera status");
  Serial.println("  camdump  - disabled; RGB565 camera path is not used");
  Serial.println("  camjpeg  - dump one JPEG camera frame over USB serial");
  Serial.println("  btn      - print camera shutter button pin level");
  Serial.println("  tf       - print TF card status and camera index preview");
  Serial.println("  tft      - show TFT color self-test, then force dashboard redraw");
  Serial.println("  snap     - capture and save one QQVGA JPEG photo to TF");
  Serial.println("  status   - print module states");
}

const String& lastInput() {
  return g_last_input;
}
}  // namespace CommandConsole
