# Reproducibility notes

## Firmware environment

- Target: ESP32-S3
- ESP-IDF recorded in `dependencies.lock`: `5.2.2`
- Direct managed component: Espressif `usb_stream` `1.5.1`
- Build entry point: `firmware/esp32-s3/CMakeLists.txt`

The source tree contains a previously generated local build under the original directory, but that build is intentionally not copied into Git history because it contains machine-specific compiler paths and generated objects. Rebuild with a locally installed ESP-IDF toolchain.

## Host environment

The Python path uses PySide6, OpenCV, NumPy, PaddleOCR, and PaddlePaddle. The Qt project uses Qt 6, Qt Network, Qt SQL, and an embedded Python 3.10 layout in the packaged configuration.

The original Qt source includes absolute Windows paths and the original packaged layout expects `hunhe/` or a colocated `Lib/` tree. This repository preserves that evidence rather than silently rewriting the source. For a portable clean build, replace those paths with repository-relative configuration and provide a matching Python runtime.

## Verification performed during curation

- File inventory and byte totals were calculated before staging.
- The supplied MP4 was decoded successfully and its first, middle, and final frames were inspected for project relevance.
- Firmware source, host source, models, documents, screenshots, and video were copied into an isolated staging tree.
- Generated caches and build directories were excluded from Git history and listed in `PROJECT_MANIFEST.md`.

No new hardware test, OCR accuracy benchmark, calibration study, or production validation was performed as part of publication.

