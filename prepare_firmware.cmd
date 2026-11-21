@echo off
REM Fetch the firmware submodule (recursively, it has its own freeink-sdk submodule)
REM and apply any patches in patches\ (none are needed for the pinned 2.x firmware).
setlocal

set "ROOT=%~dp0"
set "BASH=%ROOT%tools\msys64\usr\bin\bash.exe"

if not exist "%BASH%" (
  echo Toolchain missing: %BASH%
  echo Run setup_toolchain.cmd first.
  exit /b 1
)

set "GITEXE="
for /f "delims=" %%i in ('where git 2^>nul') do if not defined GITEXE set "GITEXE=%%i"
if not defined GITEXE (
  echo Git not found on PATH. Install Git for Windows first.
  exit /b 1
)

if not exist "%ROOT%Crosspoint\src\main.cpp" (
  echo Fetching firmware submodule ...
  git -C "%ROOT%." submodule update --init --recursive Crosspoint
  if errorlevel 1 (
    echo Failed to fetch the firmware submodule.
    exit /b 1
  )
)
if not exist "%ROOT%Crosspoint\freeink-sdk\libs" (
  echo Fetching freeink-sdk submodule ...
  git -C "%ROOT%Crosspoint" submodule update --init --recursive freeink-sdk
  if errorlevel 1 (
    echo Failed to fetch freeink-sdk.
    exit /b 1
  )
)

"%BASH%" -lc "ROOT=$(cygpath -u '%ROOT%'); GIT=$(cygpath -u '%GITEXE%'); cd \"$ROOT/Crosspoint\" || exit 1; echo \"firmware: $(\"$GIT\" rev-parse --short HEAD)\"; shopt -s nullglob; for p in \"$ROOT\"/patches/*.patch; do name=$(basename \"$p\"); if \"$GIT\" apply --reverse --check \"$p\" >/dev/null 2>&1; then echo \"[skip] already applied: $name\"; elif \"$GIT\" apply --check \"$p\" >/dev/null 2>&1; then \"$GIT\" apply \"$p\" || exit 1; echo \"[ok]   applied: $name\"; else echo \"[fail] patch does not apply: $name\"; exit 1; fi; done; echo \"Firmware ready: $ROOT/Crosspoint\""
if errorlevel 1 (
  echo Failed to prepare the firmware.
  exit /b 1
)
endlocal
