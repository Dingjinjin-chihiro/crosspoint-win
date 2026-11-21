@echo off
REM Build the emulator with debug symbols (RelWithDebInfo) into build-debug/.
REM Same toolchain + firmware flow as build.cmd; only the build type and dir differ.
setlocal

set "ROOT=%~dp0"
set "BASH=%ROOT%tools\msys64\usr\bin\bash.exe"

if not exist "%BASH%" (
  echo Toolchain missing: %BASH%
  echo Run setup_toolchain.cmd first.
  exit /b 1
)

call "%ROOT%prepare_firmware.cmd"
if errorlevel 1 exit /b 1

"%BASH%" -lc "export PATH=/mingw64/bin:$PATH; export MSYSTEM=MINGW64; ROOT=$(cygpath -u '%ROOT%'); cd \"$ROOT\" || exit 1; cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCROSSPOINT_ROOT=\"$ROOT/Crosspoint\" && cmake --build build-debug -j 8"
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

set "MINGW=%ROOT%tools\msys64\mingw64"
if exist "%MINGW%\bin\SDL2.dll" (
  if not exist "%ROOT%build-debug\SDL2.dll" copy /y "%MINGW%\bin\SDL2.dll" "%ROOT%build-debug\SDL2.dll" >nul
)

echo.
echo Debug build finished: %ROOT%build-debug\crosspoint_emulator.exe
echo Debug with: gdb --args build-debug\crosspoint_emulator.exe  (or F5 in VS Code)
endlocal
