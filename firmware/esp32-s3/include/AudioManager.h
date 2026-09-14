#pragma once

#include <Arduino.h>
#include "SharedTypes.h"

namespace AudioManager {
void init();
void poll();
void startRecallBeep();
void startAckBeep();
void startAlertBeep();
void stopBeep();
void printStatus();
const ModuleState& state();
const AudioData& data();
}  // namespace AudioManager
