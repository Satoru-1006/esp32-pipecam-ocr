@echo off
cd /d "%~dp0"
python esp32_ocr_adapter.py
if errorlevel 1 pause
