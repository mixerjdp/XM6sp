@echo off
setlocal

set ARCH=%1
if "%ARCH%"=="" set ARCH=win32

set VCVARS=
set OUT=

if /I "%ARCH%"=="win32" (
  set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat
  set OUT=xm6_libretro_win32.dll
) else if /I "%ARCH%"=="win64" (
  set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
  set OUT=xm6_libretro_win64.dll
) else (
  echo Usage: build_libretro_msvc.bat [win32^|win64]
  exit /b 1
)

if not exist "%VCVARS%" (
  echo Missing Visual Studio environment script:
  echo   %VCVARS%
  exit /b 1
)

call "%VCVARS%"
if errorlevel 1 (
  echo Failed to initialize Visual Studio build environment.
  exit /b 1
)

cl /nologo /LD /O2 /MT /EHsc /std:c++14 ^
  /I. /I..\vm xm6_libretro.cpp ^
  /link /DEF:xm6_libretro.def /OUT:%OUT%

if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

echo Built %OUT%
exit /b 0
