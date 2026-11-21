@echo off
REM Build the emulator against the Crosspoint firmware submodule.
REM Requires the portable toolchain from setup_toolchain.cmd.
setlocal

set "ROOT=%~dp0"
set "BASH=%ROOT%tools\msys64\usr\bin\bash.exe"
set "MINGW=%ROOT%tools\msys64\mingw64"

if not exist "%BASH%" (
  echo Toolchain missing: %BASH%
  echo Run setup_toolchain.cmd first.
  exit /b 1
)
if not exist "%ROOT%Crosspoint\src\main.cpp" (
  echo Firmware submodule missing.
  echo Run: git submodule update --init --recursive
  exit /b 1
)

"%BASH%" -lc "export PATH=/mingw64/bin:$PATH; export MSYSTEM=MINGW64; ROOT=$(cygpath -u '%ROOT%'); cd \"$ROOT\" || exit 1; cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCROSSPOINT_ROOT=\"$ROOT/Crosspoint\" && cmake --build build -j 8"
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

REM MinGW links the runtime dynamically; ship the DLLs next to the executable.
for %%d in (SDL2.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
  copy /y "%MINGW%\bin\%%d" "%ROOT%build\%%d" >nul
)

echo.
echo Build finished: %ROOT%build\crosspoint_emulator.exe
echo Run it with: run_emulator.cmd
endlocal
