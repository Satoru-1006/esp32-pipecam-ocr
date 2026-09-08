@echo off
setlocal

set "IDF_PATH=C:\esp32_build\esp-idf-v5.2.2"
set "PROJECT_DIR=%~dp0esp32_firmware"

if not exist "%IDF_PATH%\export.bat" (
    echo ESP-IDF was not found at %IDF_PATH%
    pause
    exit /b 1
)

if not exist "%PROJECT_DIR%\build\esp32_uvc_ap_stream.bin" (
    echo Firmware BIN was not found. Build the project first.
    pause
    exit /b 1
)

call "%IDF_PATH%\export.bat"
cd /d "%PROJECT_DIR%"
idf.py -p COM8 flash

if errorlevel 1 (
    echo Flash failed. Confirm that the scanner is connected as COM8.
) else (
    echo Flash succeeded. Restart the scanner and verify the LCD shows K1 OCR, K2 LIGHT, K3 VIEW.
)
pause
