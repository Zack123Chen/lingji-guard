#pragma once

#include <Arduino.h>
#include "SharedTypes.h"

namespace CameraManager {
void init();
bool captureFrame(CameraFrame& frame);
bool captureJpegFrame(CameraFrame& frame);
void releaseFrame(CameraFrame& frame);
void diagnoseSccb();
bool psramActuallyUsable();
void markLimited();
const ModuleState& state();
}  // namespace CameraManager
