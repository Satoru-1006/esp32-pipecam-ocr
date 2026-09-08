# Limitations and claim boundaries

- The field video and screenshots are demonstration evidence, not a controlled accuracy experiment.
- No dataset split, ground-truth protocol, confidence calibration, precision/recall report, or latency benchmark is asserted here.
- The firmware default Wi-Fi password is visible in source and must be changed for deployment.
- The firmware currently serves HTTP/MJPEG and JSON control endpoints without an authentication layer; use it on an isolated network.
- The Qt source retains historical Hikvision and Modbus wrappers. Their presence does not mean those paths are active in the current ESP32 workflow.
- The Qt project retains absolute local paths and an embedded-Python packaging assumption; a clean checkout is not promised to build without environment work.
- OCR model files and third-party components have independent provenance and licensing obligations.
- No industrial, safety, measurement, medical, or production-release claim follows from this repository.

