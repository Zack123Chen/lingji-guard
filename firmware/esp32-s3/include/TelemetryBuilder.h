#pragma once

#include <Arduino.h>

#include "SharedTypes.h"

namespace TelemetryBuilder {
String buildJson(const SensorData& data,
                 const GnssData& gnss,
                 const ModuleState& air_state,
                 const ModuleState& camera_state,
                 const ModuleState& storage_state,
                 const StorageData& storage,
                 const ModuleState& audio_state,
                 const AudioData& audio,
                 const RuntimeStatus& runtime);
uint32_t sequence();
}  // namespace TelemetryBuilder
