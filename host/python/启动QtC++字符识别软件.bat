@echo off
taskkill /IM mainwindow.exe /F >nul 2>nul
del /q "%~dp0mainwindow\__pycache__\Py_Module.cpython-310.pyc" >nul 2>nul
del /q "%~dp0mainwindow\release\__pycache__\Py_Module.cpython-310.pyc" >nul 2>nul
cd /d "%~dp0mainwindow\release"
start "" mainwindow.exe
