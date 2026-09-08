@echo off
cd /d "%~dp0"
set "PIP_TMP=%cd%\.pip-tmp"
if not exist "%PIP_TMP%" mkdir "%PIP_TMP%"
set "TEMP=%PIP_TMP%"
set "TMP=%PIP_TMP%"

python -m pip install --disable-pip-version-check --user --no-cache-dir ^
  paddlepaddle==3.3.1 ^
  paddleocr==3.5.0

if errorlevel 1 (
    echo.
    echo OCR依赖安装失败。请检查网络，关闭其它Python程序后重新运行本脚本。
    pause
    exit /b 1
)

echo.
echo OCR依赖安装完成。
pause
