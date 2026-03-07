@echo off
setlocal

set "ROOT=%~dp0"
cd /d "%ROOT%"

set "LIBRETRO_OUT=libretro\xm6_libretro_win64.dll"
set "LIBRETRO_RELEASE_DIR=libretro\Release_x64"
set "LIBRETRO_RELEASE_OUT=%LIBRETRO_RELEASE_DIR%\xm6_libretro.dll"

echo === Build xm6_libretro x64 Release ===
cmd /d /c "cd /d libretro && call build_libretro_msvc.bat win64"
if errorlevel 1 (
  echo Error: fallo el build de xm6_libretro x64.
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
  echo Error: no se pudo copiar xm6_libretro x64 a la carpeta Release_x64.
  exit /b 1
)

echo.
echo Build x64 completado.
echo   libretro:    %LIBRETRO_RELEASE_OUT%
exit /b 0
