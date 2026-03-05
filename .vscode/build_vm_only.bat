@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\vcvars32.bat"

set "CHECK_SCRIPT=%~dp0check_vm_no_mfc.ps1"
set "VM_ONLY_SCRIPT=%~dp0build_vm_only.ps1"

if not exist "%CHECK_SCRIPT%" (
  echo Error: no se encontro %CHECK_SCRIPT%
  exit /b 1
)
if not exist "%VM_ONLY_SCRIPT%" (
  echo Error: no se encontro %VM_ONLY_SCRIPT%
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%CHECK_SCRIPT%"
if errorlevel 1 exit /b 1

set "CFG=%1"
if "%CFG%"=="" set "CFG=Debug"

powershell -NoProfile -ExecutionPolicy Bypass -File "%VM_ONLY_SCRIPT%" -Configuration %CFG%
exit /b %errorlevel%
