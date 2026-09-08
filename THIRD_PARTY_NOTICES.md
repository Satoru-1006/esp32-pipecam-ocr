# Third-party material and provenance

This repository preserves third-party material that was already present in the local project. It is listed for review, not relicensed by this repository.

- ESP-IDF and Espressif components, including `usb_stream`, under `firmware/esp32-s3/managed_components/`. Upstream license files are retained where present.
- Qt 6 and Qt Network / SQL APIs used by the C++ host source.
- Python, PySide6, OpenCV, NumPy, PaddleOCR, PaddlePaddle, and related runtime packages referenced by the host source or included in the full local snapshot release asset.
- PaddleOCR model files under `host/qt/mainwindow/models/` and their original archives.
- The historical `cmvcamera.*` wrapper references a vendor camera SDK header (`MvCameraControl.h`) that is not redistributed here.

Before distributing binaries or using the model files commercially, verify the current upstream licenses and any vendor SDK terms.

