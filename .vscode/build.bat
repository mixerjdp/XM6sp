@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\vcvars32.bat"

set "SOLUTION=main\XM6.sln"
if not exist "%SOLUTION%" set "SOLUTION=00proj.vc7\XM6.sln"

if not exist "%SOLUTION%" (
  echo Error: no se encontro XM6.sln en main\ ni en 00proj.vc7\
  exit /b 1
)

msbuild "%SOLUTION%" /p:Configuration=%1 /p:Platform=Win32 /m
