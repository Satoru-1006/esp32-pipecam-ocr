# ESP32-S3 UVC AP Stream Firmware

This firmware replaces the fixed-PC-hotspot behavior with a board-hosted Wi-Fi AP.

- AP SSID: `PipeCam-XXXX` where `XXXX` is derived from the board MAC
- AP password: `12345678`
- Board IP: `192.168.4.1`
- MJPEG stream: `http://192.168.4.1:8080/stream`
- Status endpoint: `http://192.168.4.1:8081/status`
- Result endpoint: `http://192.168.4.1:8081/result`

## Physical Keys

The physical keys are ordered from left to right on the enclosure:

- `PWR`: hardware power key managed by IP5306
- `K3` (GPIO39): toggle laser/fill light
- `K2` (GPIO38): trigger OCR scan
- `K1` (GPIO21): toggle LCD view

The LCD header shows system, Wi-Fi, PC connection, and battery availability.
The footer shows `PWR / K3 LIGHT / K2 OCR / K1 VIEW` in the same physical
order as the keys. Battery percentage is
shown as `BAT:--` until battery sensing hardware is connected to the ESP32.
- Control endpoint: `http://192.168.4.1:8081/control`

## Build

Install ESP-IDF 5.2 or newer, then run:

```powershell
cd C:\Users\86198\Desktop\scan\esp32_firmware
idf.py set-target esp32s3
idf.py build
idf.py -p COM8 flash monitor
```

For this project, the ready-to-flash firmware is:

```text
C:\Users\86198\Desktop\scan\esp32_firmware\build\esp32_uvc_ap_stream.bin
```

Do not flash the similarly named firmware under `D:\文档\New project 5`; it is
a different project and does not contain this scanner UI.

Keep `D:\文档\New project 5\esp32s3_original_flash_2026-04-29.bin` before flashing. Flashing this project replaces the current `tcp_client` firmware.

## Hardware Pins

From the schematic:

```text
USB D-  -> GPIO19
USB D+  -> GPIO20
TFT_LED -> GPIO13
```

The firmware currently enables `TFT_LED` only as a backlight smoke check. It does not draw UI on the TFT.
