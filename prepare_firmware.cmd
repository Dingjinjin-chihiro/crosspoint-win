@echo off
REM Check out the firmware submodule and apply our two fixes to it.
REM Idempotent: patches that are already applied are skipped.
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
set "FW=%ROOT%Crosspoint"
set "PATCHES=%ROOT%patches"

if not exist "%FW%\src\main.cpp" (
  echo Fetching firmware submodule ...
  git -C "%ROOT%." submodule update --init --depth 1 Crosspoint
  if errorlevel 1 (
    echo Failed to fetch the firmware submodule.
    exit /b 1
  )
)

for %%p in ("%PATCHES%\*.patch") do (
  git -C "%FW%" apply --reverse --check "%%~fp" >nul 2>&1
  if not errorlevel 1 (
    echo [skip] already applied: %%~nxp
  ) else (
    git -C "%FW%" apply --check "%%~fp" >nul 2>&1
    if errorlevel 1 (
      echo [fail] patch does not apply: %%~nxp
      echo        The submodule is probably at a different commit than expected.
      echo        Expected: 9e04eec (crosspoint-reader tag 1.0.0)
      exit /b 1
    )
    git -C "%FW%" apply "%%~fp"
    if errorlevel 1 ( echo [fail] could not apply %%~nxp & exit /b 1 )
    echo [ok]   applied: %%~nxp
  )
)

echo Firmware ready at %FW%
endlocal
