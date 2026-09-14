if(CONFIG_CAREGUARD_TF_WRITE_PROBE)
  set(TF_WRITE_PROBE_SRCS "tf_write_probe.cpp")
  if(CONFIG_CAREGUARD_TF_WRITE_PROBE_WITH_CAMERA)
    list(APPEND TF_WRITE_PROBE_SRCS "SensorManager.cpp")
  endif()

  idf_component_register(
    SRCS ${TF_WRITE_PROBE_SRCS}
    INCLUDE_DIRS "../include"
  )

  target_compile_definitions(${COMPONENT_LIB} PRIVATE
    ARDUINO_USB_MODE=1
    ARDUINO_USB_CDC_ON_BOOT=1
    TF_PROBE_EXTERNAL_MAIN
  )
  if(CONFIG_CAREGUARD_TF_WRITE_PROBE_WITH_CAMERA)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE TF_PROBE_WITH_CAMERA)
  endif()
elseif(CONFIG_CAREGUARD_CAMERA_STACK_DIAG)
  idf_component_register(
    SRCS "camera_idf_gray_stack4096_diag.c"
    INCLUDE_DIRS "."
  )
else()
  idf_component_register(
    SRCS
      "Air780EManager.cpp"
      "AudioManager.cpp"
      "CameraManager.cpp"
      "CommandConsole.cpp"
      "DisplayManager.cpp"
      "SensorManager.cpp"
      "StorageManager.cpp"
      "TelemetryBuilder.cpp"
      "main.cpp"
    INCLUDE_DIRS "../include"
  )

  target_compile_definitions(${COMPONENT_LIB} PRIVATE
    BOARD_HAS_PSRAM
    ARDUINO_USB_MODE=1
    ARDUINO_USB_CDC_ON_BOOT=1
  )
endif()
