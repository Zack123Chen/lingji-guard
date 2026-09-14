# ESP32-S3 firmware

PlatformIO project for the current CareGuard collar prototype. The production entry is `src/main.cpp`; subsystem implementations are in `src/`, public project headers in `include/`, and isolated hardware diagnostics are selected by environments in `platformio.ini`.

## Build

```bash
pio run -e esp32-s3-devkitc-1
```

Other named environments are diagnostic targets. Their `build_src_filter`, CMake source selection and generated `sdkconfig` must stay aligned. Upload and serial monitoring require a currently detected board port; do not infer hardware success from compilation alone.

`components/esp32-camera/` is pinned as a nested repository. `docs/` holds the matching hardware schematic, and `captures/` contains hardware-derived evidence that is intentionally versioned; `.pio/` and `logs/*.log` are generated locally.
