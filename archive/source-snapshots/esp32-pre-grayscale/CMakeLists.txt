if(CONFIG_CAMERA_TASK_STACK_SIZE STREQUAL "4096")
  idf_component_register(
    SRCS "camera_idf_gray_stack4096_diag.c"
    INCLUDE_DIRS "."
  )
else()
  idf_component_register(
    SRCS "external_qspi_psram_diag.cpp"
    INCLUDE_DIRS "../include"
  )

  target_compile_definitions(${COMPONENT_LIB} PRIVATE
    BOARD_HAS_PSRAM
    ARDUINO_USB_MODE=1
    ARDUINO_USB_CDC_ON_BOOT=1
  )
endif()
