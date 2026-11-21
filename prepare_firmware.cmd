@echo off
REM Fetch the firmware submodule (if needed) and apply our two fixes.
REM Idempotent: patches that are already applied are skipped.
setlocal

set "ROOT=%~dp0"
set "BASH=%ROOT%tools\msys64\usr\bin\bash.exe"

if not exist "%BASH%" (
  echo Toolchain missing: %BASH%
  echo Run setup_toolchain.cmd first.
  exit /b 1
)

REM MSYS2's bash does not see the Windows Git on PATH; resolve it explicitly.
set "GITEXE="
for /f "delims=" %%i in ('where git 2^>nul') do if not defined GITEXE set "GITEXE=%%i"
if not defined GITEXE (
  echo Git not found on PATH. Install Git for Windows first.
  exit /b 1
)

if not exist "%ROOT%Crosspoint\src\main.cpp" (
  echo Fetching firmware submodule ...
  git -C "%ROOT%." submodule update --init --depth 1 Crosspoint
  if errorlevel 1 (
    echo Failed to fetch the firmware submodule.
    exit /b 1
  )
)

"%BASH%" -lc "ROOT=$(cygpath -u '%ROOT%'); GIT=$(cygpath -u '%GITEXE%'); cd \"$ROOT/Crosspoint\" || exit 1; expected=9e04eec072b1db251cca6dc570b619c3e39b5c36; head=$(\"$GIT\" rev-parse HEAD); if [ \"$head\" != \"$expected\" ]; then echo \"[warn] firmware is at $head, expected $expected\"; fi; for p in \"$ROOT\"/patches/*.patch; do name=$(basename \"$p\"); if \"$GIT\" apply --reverse --check \"$p\" >/dev/null 2>&1; then echo \"[skip] already applied: $name\"; elif \"$GIT\" apply --check \"$p\" >/dev/null 2>&1; then \"$GIT\" apply \"$p\" || exit 1; echo \"[ok]   applied: $name\"; else echo \"[fail] patch does not apply: $name\"; exit 1; fi; done; echo \"Firmware ready: $ROOT/Crosspoint\""
if errorlevel 1 (
  echo Failed to apply firmware patches.
  exit /b 1
)
endlocal
