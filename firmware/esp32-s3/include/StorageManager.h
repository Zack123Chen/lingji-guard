#pragma once

#include <Arduino.h>
#include "SharedTypes.h"

namespace StorageManager {
void init();
bool appendTelemetry(uint32_t seq,
                     const SensorData& data,
                     const GnssData& gnss,
                     const ModuleState& air_state,
                     const ModuleState& camera_state,
                     const AudioData& audio);
bool captureAndSaveJpeg(const char* reason, PhotoStatus& photo);
bool saveCameraFrame(const CameraFrame& frame, const char* reason, PhotoStatus& photo);
void printStatus();
void listCameraIndex();
const ModuleState& state();
const StorageData& data();
}  // namespace StorageManager
