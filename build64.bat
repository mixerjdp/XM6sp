@echo off
setlocal

set "ROOT=%~dp0"
cd /d "%ROOT%"

set "VCVARS64=C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\x86_amd64\vcvarsx86_amd64.bat"
set "CORE_PROJ=main\XM6Core.vcxproj"
set "CORE_OUT=main\Release_DLL_x64\xm6core.dll"
set "LIBRETRO_OUT=libretro\xm6_libretro_win64.dll"
set "LIBRETRO_RELEASE_DIR=libretro\Release_x64"
set "LIBRETRO_RELEASE_OUT=%LIBRETRO_RELEASE_DIR%\xm6_libretro.dll"

if not exist "%VCVARS64%" goto :missing_vcvars

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

echo === Build xm6core x64 Release ===
call "%VCVARS64%"
if errorlevel 1 (
  echo Error: fallo al inicializar el entorno x64 de Visual Studio 2012.
  exit /b 1
)

msbuild "%CORE_PROJ%" /p:Configuration=Release /p:Platform=x64 /m
if errorlevel 1 (
  echo Error: fallo el build de xm6core x64.
  exit /b 1
)

if not exist "%CORE_OUT%" (
  echo Error: no se genero el binario esperado:
  echo   %CORE_OUT%
  exit /b 1
)

echo.
echo Build x64 completado.
echo   xm6core:     %CORE_OUT%
echo   libretro:    %LIBRETRO_RELEASE_OUT%
exit /b 0

:missing_vcvars
echo Error: no se encontro vcvarsx86_amd64.bat:
echo   %VCVARS64%
exit /b 1
