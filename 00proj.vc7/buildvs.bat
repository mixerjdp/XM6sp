@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   Script de Compilacion para XM6 (Release / Win32)
echo   Usando Visual Studio 2022 MSBuild
echo ========================================================
echo.

set "MSBUILD_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"

if not exist "%MSBUILD_PATH%" (
    echo [ERROR] No se pudo encontrar MSBuild en: "%MSBUILD_PATH%"
    pause
    exit /b 1
)

echo Iniciando compilacion de XM6.sln...
echo.

:: Ejecutar la compilacion
"%MSBUILD_PATH%" XM6.sln /p:Configuration=Release /p:Platform=Win32 /t:Build /m

if %ERRORLEVEL% equ 0 (
    echo.
    echo ========================================================
    echo  [EXITO] La compilacion ha finalizado correctamente.
    echo ========================================================
) else (
    echo.
    echo ========================================================
    echo  [ERROR] Hubo errores durante la compilacion.
    echo ========================================================
)

pause
