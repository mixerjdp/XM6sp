@echo off
setlocal

set CFG=%1
if "%CFG%"=="" set CFG=Debug
shift
set SYS_DIR=%1

if /I "%CFG%"=="Debug" (
  set CRT=/MDd
) else (
  set CRT=/MD
)

call "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\vcvars32.bat"
if errorlevel 1 exit /b 1

set "XM6_ROOT=%CD%"

cl /nologo /EHsc %CRT% /D_CRT_SECURE_NO_WARNINGS /I"%XM6_ROOT%\vm" "%XM6_ROOT%\main\xm6core_smoke.cpp" ^
  /link /nologo /LIBPATH:"%XM6_ROOT%\main\%CFG%_DLL" xm6core.lib /OUT:"%XM6_ROOT%\main\%CFG%_DLL\xm6core_smoke.exe"
if errorlevel 1 (
  exit /b 1
)

if "%SYS_DIR%"=="" (
  "%XM6_ROOT%\main\%CFG%_DLL\xm6core_smoke.exe"
) else (
  "%XM6_ROOT%\main\%CFG%_DLL\xm6core_smoke.exe" %SYS_DIR%
)
set SMOKE_RC=%ERRORLEVEL%

exit /b %SMOKE_RC%
