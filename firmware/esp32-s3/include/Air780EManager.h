#pragma once

#include <Arduino.h>
#include "SharedTypes.h"

namespace Air780EManager {
void init();
void queryStatus();
void printStatus();
String sendCommand(const char* command, uint32_t timeout_ms);
bool ensureMqttConnected();
bool canAttemptAutoPublish();
bool publishTelemetry(const String& json);
bool pollDownlink(RemoteCommandEvent& event);
const ModuleState& state();
const GnssData& gnss();
}  // namespace Air780EManager
