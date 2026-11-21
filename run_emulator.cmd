@echo off
REM Launch the emulator with its virtual SD card (sdcard\ next to this script).
setlocal

set "ROOT=%~dp0"
set "EXE=%ROOT%build\crosspoint_emulator.exe"

if not exist "%EXE%" (
  echo Emulator not built yet. Run setup_toolchain.cmd then build.cmd.
  exit /b 1
)

cd /d "%ROOT%"
start "" "%EXE%"
endlocal
