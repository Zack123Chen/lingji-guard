# Historical ESP32 source snapshot

These files were loose at the former workspace root and were not part of a valid build tree: the accompanying `CMakeLists.txt` referenced missing files and a non-existent parent include directory.

Content comparison showed that this snapshot predates the canonical grayscale/local-preview path in `firmware/esp32-s3/`. It is retained only because several files differ materially (JPEG storage, camera console, display and telemetry behavior). Do not build or merge this directory wholesale.

Eight byte-identical root duplicates were removed; their canonical copies remain in `firmware/esp32-s3/src/` and the pre-refactor root commit can recover the deleted duplicates if needed.
