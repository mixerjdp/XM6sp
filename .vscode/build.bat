@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\vcvars32.bat"
msbuild 00proj.vc7\XM6.sln /p:Configuration=%1 /p:Platform=Win32 /m
