# Workspace guide

## Project map

- `apps/careguard-live/`: Vite/Node dashboard and `WeChatMiniProgram`; read its own `AGENTS.md` before editing.
- `firmware/esp32-s3/`: canonical ESP32-S3 PlatformIO project. Do not compile or merge code from `archive/source-snapshots/` as if it were current.
- `firmware/stm32f103-keil/`: Keil uVision 5 ARMCC project; keep paths inside `Project.uvprojx` relative.
- `docs/`: deliverables and source material. Preserve signatures, templates, user data and final artifacts.
- `artifacts/`: hardware evidence. Historical path strings are evidence, not active configuration.

## Verification

```bash
(cd apps/careguard-live && npm test && npm run build)
(cd firmware/esp32-s3 && pio run -e esp32-s3-devkitc-1)
```

The STM32 project requires Windows with Keil MDK/ARMCC. On other hosts, verify XML paths and referenced source files without claiming a successful firmware build.

## Safety

- Never stage `.env.local` or `archive/private/`.
- Keep public MQTT and HTTP inputs untrusted; follow `apps/careguard-live/AGENTS.md` security boundaries.
- A PlatformIO build is not proof of upload or hardware behavior. Preserve that evidence boundary in reports.
