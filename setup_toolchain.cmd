@echo off
REM Install the portable toolchain used by this project:
REM   MSYS2 + MinGW-w64 GCC + CMake + Ninja + SDL2 + Python
REM Everything lands in tools\msys64 (~1.4 GB). No admin rights, no system changes.
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
set "TOOLS=%ROOT%tools"
set "MSYS=%TOOLS%\msys64"
set "BASH=%MSYS%\usr\bin\bash.exe"
set "MIRROR=https://mirrors.tuna.tsinghua.edu.cn/msys2"
set "BASE=msys2-base-x86_64-20261120.tar.xz"

if exist "%BASH%" (
  echo Toolchain already present: %MSYS%
  goto :verify
)

if not exist "%TOOLS%" mkdir "%TOOLS%"
pushd "%TOOLS%" || exit /b 1

if not exist "%BASE%" (
  echo Downloading %BASE% ...
  curl.exe --ssl-no-revoke -L --retry 3 --retry-delay 2 -o "%BASE%" "%MIRROR%/distrib/x86_64/%BASE%"
  if errorlevel 1 (
    echo Download failed. Check your network or fetch the file manually from
    echo   %MIRROR%/distrib/x86_64/
    echo and place it in %TOOLS% as %BASE%
    popd & exit /b 1
  )
)

echo Extracting ...
tar -xf "%BASE%" -C "%TOOLS%"
if errorlevel 1 ( echo Extraction failed. & popd & exit /b 1 )
del /q "%BASE%"

REM Point pacman at the TUNA mirror (fast in CN; comment out to use defaults).
> "%MSYS%\etc\pacman.d\mirrorlist.msys" echo Server = %MIRROR%/msys/$arch
> "%MSYS%\etc\pacman.d\mirrorlist.mingw" echo Server = %MIRROR%/mingw/$repo

popd

:verify
if not exist "%BASH%" ( echo Toolchain install incomplete: %BASH% missing. & exit /b 1 )

echo Installing packages (gcc, cmake, ninja, SDL2, python) ...
"%BASH%" -lc "pacman -Syu --noconfirm --disable-download-timeout > /tmp/setup.log 2>&1; pacman -S --noconfirm --needed --disable-download-timeout mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-SDL2 mingw-w64-x86_64-pkgconf mingw-w64-x86_64-python >> /tmp/setup.log 2>&1"
if errorlevel 1 (
  echo Package installation failed. Tail of /tmp/setup.log:
  "%BASH%" -lc "tail -30 /tmp/setup.log"
  exit /b 1
)

"%BASH%" -lc "export PATH=/mingw64/bin:$PATH; gcc --version | head -1; cmake --version | head -1; ninja --version; pkg-config --modversion sdl2"
echo.
echo Toolchain ready at %MSYS%
echo Next: build.cmd
endlocal
