# STM32F103 Keil prototype

Early CareGuard prototype targeting STM32F103C8 with FreeRTOS and the STM32 Standard Peripheral Library.

- IDE/build entry: `Project.uvprojx`
- Application code: `User/src/`, `User/inc/`
- Board drivers: `Hardware/src/`, `Hardware/inc/`
- RTOS: `FreeRTOS/`
- Vendor support: `Library/`, `Start/`, `System/`
- Host-side helper scripts: `tools/`

The project uses Keil uVision 5 with ARMCC 5.06 and therefore requires Windows + Keil MDK for a real build. `Objects/`, `Listings/`, `*.uvguix.*`, JetBrains `.idea/` data and J-Link logs are generated/user-local and are ignored.
