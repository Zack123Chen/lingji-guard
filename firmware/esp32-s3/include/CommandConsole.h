#pragma once

#include <Arduino.h>

namespace CommandConsole {
enum class Command {
  None,
  Help,
  I2c,
  Sensors,
  Air,
  Pub,
  Cam,
  CamDump,
  CamJpeg,
  Button,
  Tf,
  Tft,
  Snap,
  Status,
  Unknown,
};

void init();
Command poll();
void printHelp();
const String& lastInput();
}  // namespace CommandConsole
