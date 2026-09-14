#pragma once

#include <Arduino.h>
#include "SharedTypes.h"

namespace DisplayManager {
enum class Page {
  Dashboard,
  Network,
  Gnss,
  RawSensors,
  CameraStatus,
};

void init();
void drawSelfTest(const char* step,
                  const ModuleState& i2c_state,
                  const ModuleState& mpu_state,
                  const ModuleState& sht_state,
                  const ModuleState& max_state,
                  const ModuleState& air_state,
                  const ModuleState& camera_state);
void drawDashboard(const ModuleState& i2c_state,
                   const ModuleState& mpu_state,
                   const ModuleState& sht_state,
                   const ModuleState& max_state,
                   const ModuleState& air_state,
                   const ModuleState& camera_state,
                   const SensorData& data);
void drawPage(Page page,
              const ModuleState& i2c_state,
              const ModuleState& mpu_state,
              const ModuleState& sht_state,
              const ModuleState& max_state,
              const ModuleState& air_state,
              const ModuleState& camera_state,
              const GnssData& gnss,
              const SensorData& data,
              const ModuleState& storage_state = ModuleState{},
              const StorageData& storage = StorageData{},
              const ModuleState& audio_state = ModuleState{},
              const AudioData& audio = AudioData{},
              const RuntimeStatus& runtime = RuntimeStatus{});
void drawHeartFocus(const ModuleState& air_state,
                    const ModuleState& camera_state,
                    const SensorData& data);
void drawAudioLevel(const ModuleState& audio_state, const AudioData& audio);
void forceFullRefresh();
void drawDiagnostic();
void drawLocatorFlash();
void drawCameraFrame(const CameraFrame& frame);
void drawCameraCaptureFailed();
void drawCameraStatus(const char* title, const char* subtitle, uint16_t color);
const ModuleState& state();
}  // namespace DisplayManager
