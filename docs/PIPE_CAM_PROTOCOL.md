# PipeCam protocol notes

The following interface is taken from the current ESP32-S3 firmware and host sources in this repository.

## Network defaults

| Item | Value |
| --- | --- |
| SoftAP SSID | `PipeCam-XXXX`, where `XXXX` is derived from the SoftAP MAC suffix |
| Default password in source | `12345678` |
| Board IP | `192.168.4.1` |
| Stream port | `8080` |
| Control port | `8081` |

The Python adapter and the Qt source contain older fixed-SSID display text (`PipeCam-2D55`) while the firmware generates the SSID from the board MAC. Treat the actual SSID printed by the board as authoritative.

## Stream

`GET http://192.168.4.1:8080/stream` returns a multipart response with boundary `esp32s3frame`. Each part is a JPEG and includes width, height, and sequence headers.

## Status

`GET http://192.168.4.1:8081/status` returns JSON fields including:

```json
{
  "status": "ok",
  "stream": "http://192.168.4.1:8080/stream",
  "clients": 1,
  "laser": 0,
  "result": "idle",
  "key1": 1,
  "key2": 1,
  "key3": 1,
  "realtime": 1,
  "capture_requests": 0,
  "capture_pending": 0
}
```

## Control

`GET /control` accepts query parameters used by the host:

- `laser=0|1`
- `realtime=0|1`
- `capture=1`
- `result=idle|processing|pass|fail|error`
- `beep=1`

## Result feedback

`POST /result` accepts form-style fields such as `state`, `text`, `confidence`, and `reason`. The device stores the latest result state for its LCD and indicator logic.

## Physical keys in the current firmware

| Key | GPIO | Current behavior |
| --- | ---: | --- |
| K1 | 21 | Toggle LCD view |
| K2 | 38 | Trigger OCR capture |
| K3 | 39 | Toggle laser / fill light |

