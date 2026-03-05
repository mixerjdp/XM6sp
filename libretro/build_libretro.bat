@echo off
setlocal

if "%CXX%"=="" set CXX=g++

%CXX% -std=gnu++14 -O2 -Wall -Wextra -Wno-cast-function-type -D__LIBRETRO__ ^
  -I. -I..\vm ^
  -shared -o xm6_libretro.dll xm6_libretro.cpp

if errorlevel 1 (
  echo Build failed.
  exit /b 1
)

echo Built xm6_libretro.dll
exit /b 0
