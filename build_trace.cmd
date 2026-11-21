@echo off
REM Build the emulator with -finstrument-functions so every function entry/exit
REM can be traced for execution-flow analysis (no hardware flashing needed).
REM See sim/src/sim_trace.cpp and docs/DEBUGGING.md section 8.
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

REM The image decoders are fetched by CMake; a fresh build-trace dir has no cache
REM pointing at the local copies, so pin them here the same way build/ was seeded.
set "DEPS=%ROOT%..\deps"

REM -O3 -g rather than the RelWithDebInfo default -O2: -finstrument-functions stops
REM the pthread.h inline wrappers from inlining, which clashes with the
REM --whole-archive libwinpthread at link time (-O3 keeps the release inlining;
REM the link also uses --allow-multiple-definition, see CMakeLists.txt).
"%BASH%" -lc "export PATH=/mingw64/bin:$PATH; export MSYSTEM=MINGW64; ROOT=$(cygpath -u '%ROOT%'); DEPS=$(cygpath -u '%DEPS%'); cd \"$ROOT\" || exit 1; cmake -S . -B build-trace -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo '-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O3 -g' -DCROSSPOINT_TRACE_BUILD=ON -DCROSSPOINT_ROOT=\"$ROOT/Crosspoint\" -DFETCHCONTENT_SOURCE_DIR_PNGDEC=\"$DEPS/pngdec\" -DFETCHCONTENT_SOURCE_DIR_JPEGDEC=\"$DEPS/jpegdec\" -DFETCHCONTENT_SOURCE_DIR_ANIMATEDGIF=\"$DEPS/animatedgif\" && cmake --build build-trace -j 12"
if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

set "MINGW=%ROOT%tools\msys64\mingw64"
if exist "%MINGW%\bin\SDL2.dll" (
  if not exist "%ROOT%build-trace\SDL2.dll" copy /y "%MINGW%\bin\SDL2.dll" "%ROOT%build-trace\SDL2.dll" >nul
)

echo.
echo Trace build finished: %ROOT%build-trace\crosspoint_emulator.exe
echo Run with CROSSPOINT_TRACE=1 to record trace.log, then:
echo   python scripts\trace_analyze.py trace.log build-trace\crosspoint_emulator.exe
endlocal
