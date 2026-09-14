#pragma once

#include <Arduino.h>

namespace Pins {
constexpr uint32_t kBaudRate = 115200;

namespace I2C {
constexpr int SDA = 10;
constexpr int SCL = 9;
}  // namespace I2C

namespace TFT {
constexpr int MOSI = 12;
constexpr int SCLK = 13;
constexpr int CS = 7;
constexpr int DC = 8;
constexpr int RST = -1;
constexpr int BL = 5;
constexpr int MISO = 11;
constexpr int WIDTH = 240;
constexpr int HEIGHT = 240;
}  // namespace TFT

namespace Air780E {
constexpr int RX = 18;  // LTE_TX -> ESP32 RX.
constexpr int TX = 17;  // LTE_RX -> ESP32 TX.
constexpr int EN = 6;   // LTE_EN.
constexpr int PWRON = 4;
}  // namespace Air780E

namespace Button {
constexpr int CAMERA_SHUTTER = Air780E::PWRON;  // Black button on GPIO4, active low after boot.
}  // namespace Button

namespace Storage {
constexpr int CD_IN = 14;
constexpr int DATA0 = 15;
constexpr int CLK = 16;
constexpr int CMD = 21;
}  // namespace Storage

namespace Audio {
constexpr int MIC_ADC = 3;
constexpr int SPK_EN = 42;
constexpr int SPK_PLUS = 46;
constexpr int SPK_MINUS = 45;
constexpr uint8_t SPK_LEDC_CHANNEL = 7;
}  // namespace Audio

namespace Camera {
constexpr int PWDN = -1;
constexpr int RESET = -1;
constexpr int XCLK = -1;
constexpr int SIOD = 10;
constexpr int SIOC = 9;
constexpr int D0 = 34;
constexpr int D1 = 48;
constexpr int D2 = 47;
constexpr int D3 = 33;
constexpr int D4 = 35;
constexpr int D5 = 37;
constexpr int D6 = 38;
constexpr int D7 = 39;
constexpr int PCLK = 36;
constexpr int VSYNC = 41;
constexpr int HREF = 40;
}  // namespace Camera

inline void printPinMap() {
  Serial.println("Pin map:");
  Serial.printf("  I2C SDA=%d SCL=%d\n", I2C::SDA, I2C::SCL);
  Serial.printf("  TFT MOSI=%d SCLK=%d CS=%d DC=%d RST=%d BL=%d MISO=%d\n",
                TFT::MOSI,
                TFT::SCLK,
                TFT::CS,
                TFT::DC,
                TFT::RST,
                TFT::BL,
                TFT::MISO);
  Serial.printf("  Air780E RX=%d TX=%d EN=%d PWRON=%d\n",
                Air780E::RX,
                Air780E::TX,
                Air780E::EN,
                Air780E::PWRON);
  Serial.printf("  Button camera shutter=%d active-low\n", Button::CAMERA_SHUTTER);
  Serial.printf("  TF CD=%d D0=%d CLK=%d CMD=%d\n",
                Storage::CD_IN,
                Storage::DATA0,
                Storage::CLK,
                Storage::CMD);
  Serial.printf("  Audio MIC=%d SPK_EN=%d SPK+=%d SPK-=%d\n",
                Audio::MIC_ADC,
                Audio::SPK_EN,
                Audio::SPK_PLUS,
                Audio::SPK_MINUS);
  Serial.printf("  Camera D0-D7=%d,%d,%d,%d,%d,%d,%d,%d PCLK=%d VSYNC=%d HREF=%d XCLK=%d\n",
                Camera::D0,
                Camera::D1,
                Camera::D2,
                Camera::D3,
                Camera::D4,
                Camera::D5,
                Camera::D6,
                Camera::D7,
                Camera::PCLK,
                Camera::VSYNC,
                Camera::HREF,
                Camera::XCLK);
}
}  // namespace Pins
