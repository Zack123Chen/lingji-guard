#pragma once

#include <Arduino.h>
#include "SharedTypes.h"

namespace SensorManager {
void configureI2CBus();
void initI2C();
void recoverI2CBus(const char* reason);
void scanI2C();
void initTemperature();
void initAll();
void pollMotionSensor();
void pollHeartSensor();
void read();
void printData();

const ModuleState& i2cState();
const ModuleState& mpuState();
const ModuleState& shtState();
const ModuleState& maxState();
const SensorData& data();
String i2cAddressesText();
uint8_t i2cCount();
}  // namespace SensorManager
