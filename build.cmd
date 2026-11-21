@echo off
REM Build the emulator against the Crosspoint firmware submodule.
REM Requires the portable toolchain from setup_toolchain.cmd.
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

REM Image decoders (PNGdec/JPEGDEC/AnimatedGIF) are fetched by CMake on the first
REM configure. ArduinoJson comes from third_party\, so no download is needed for it.
"%BASH%" -lc "export PATH=/mingw64/bin:$PATH; export MSYSTEM=MINGW64; ROOT=$(cygpath -u '%ROOT%'); cd \"$ROOT\" || exit 1; cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCROSSPOINT_ROOT=\"$ROOT/Crosspoint\" && cmake --build build -j 12"
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

REM CMake copies SDL2.dll next to the executable; this is just a safety net.
set "MINGW=%ROOT%tools\msys64\mingw64"
if exist "%MINGW%\bin\SDL2.dll" (
  if not exist "%ROOT%build\SDL2.dll" copy /y "%MINGW%\bin\SDL2.dll" "%ROOT%build\SDL2.dll" >nul
)

echo.
echo Build finished: %ROOT%build\crosspoint_emulator.exe
echo Run it with: run_emulator.cmd
endlocal
