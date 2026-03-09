@echo off
setlocal

set "ROOT=%~dp0"
cd /d "%ROOT%"

set "VCVARS32=C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\vcvars32.bat"
set "CORE_PROJ=main\XM6Core.vcxproj"
set "CORE_OUT=main\Release_DLL\xm6core.dll"
set "LIBRETRO_OUT=libretro\xm6_libretro_win32.dll"
set "LIBRETRO_RELEASE_DIR=libretro\Release"
set "LIBRETRO_RELEASE_OUT=%LIBRETRO_RELEASE_DIR%\xm6_libretro.dll"

if not exist "%VCVARS32%" goto :missing_vcvars
if not exist "%CORE_PROJ%" goto :missing_core

echo === Build xm6_libretro Win32 Release ===
cmd /d /c "cd /d libretro && call build_libretro_msvc.bat win32"
if errorlevel 1 (
  echo Error: fallo el build de xm6_libretro Win32.
  exit /b 1
)

if not exist "%LIBRETRO_OUT%" (
  echo Error: no se genero el binario esperado:
  echo   %LIBRETRO_OUT%
  exit /b 1
)

if not exist "%LIBRETRO_RELEASE_DIR%" mkdir "%LIBRETRO_RELEASE_DIR%"
copy /Y "%LIBRETRO_OUT%" "%LIBRETRO_RELEASE_OUT%" >nul
if errorlevel 1 (
  echo Error: no se pudo copiar xm6_libretro Win32 a la carpeta Release.
  exit /b 1
)

call "%VCVARS32%"
if errorlevel 1 (
  echo Error: fallo al reinicializar el entorno de Visual Studio 2012.
  exit /b 1
)

echo === Build xm6core Win32 Release ===
msbuild "%CORE_PROJ%" /p:Configuration=Release /p:Platform=Win32 /m
if errorlevel 1 (
  echo Error: fallo el build de xm6core Win32.
  exit /b 1
)

if not exist "%CORE_OUT%" (
  echo Error: no se genero el binario esperado:
  echo   %CORE_OUT%
  exit /b 1
)

echo.
echo Build Win32 completado.
echo   xm6core:    %CORE_OUT%
echo   libretro:   %LIBRETRO_RELEASE_OUT%
exit /b 0

:missing_vcvars
echo Error: no se encontro vcvars32.bat:
echo   %VCVARS32%
exit /b 1

:missing_core
echo Error: no se encontro el proyecto:
echo   %CORE_PROJ%
exit /b 1
