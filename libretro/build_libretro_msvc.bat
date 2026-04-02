@echo off
setlocal

set ARCH=%1
if "%ARCH%"=="" set ARCH=win32

set VCVARS=
set OUT=
set RELEASE_DIR=

if /I "%ARCH%"=="win32" (
  set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat
  set OUT=xm6_libretro_win32.dll
  set RELEASE_DIR=Release
) else if /I "%ARCH%"=="win64" (
  set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
  set OUT=xm6_libretro_win64.dll
  set RELEASE_DIR=Release_x64
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

set YMFM_SRC=..\vm\ymfm
set X68SOUND_SRC=..\vm\x68sound
set DEFINES=/D__LIBRETRO__ /DXM6CORE_MONOLITHIC /DXM6CORE_STATIC /DXM6_FORCE_DEFAULT_SRAM /DNDEBUG /D_NDEBUG /DMUSASHI_CNF=\"m68kconf_xm6.h\"
set INCLUDES=/I. /I..\vm /I..\cpu /I..\cpu\Musashi /I..\cpu\Musashi\softfloat
set YMFM_SOURCES=
if exist "%YMFM_SRC%\ymfm_opm.cpp" (
  set DEFINES=%DEFINES% /DXM6CORE_ENABLE_YMFM
  set INCLUDES=%INCLUDES% /I%YMFM_SRC%
  set YMFM_SOURCES=..\vm\ymfm_opm_engine.cpp %YMFM_SRC%\ymfm_opm.cpp
)
set X68SOUND_SOURCES=
if exist "%X68SOUND_SRC%\X68Sound.cpp" (
  set DEFINES=%DEFINES% /DXM6CORE_ENABLE_X68SOUND /DXM6CORE_X68SOUND_RENDER_ONLY
  set INCLUDES=%INCLUDES% /I%X68SOUND_SRC%
  set X68SOUND_SOURCES=%X68SOUND_SRC%\X68Sound.cpp
)
set COMMONFLAGS=/nologo /LD /O2 /MT /EHsc /std:c++14 /wd4018 /wd4244 /wd4267 /wd4996
set COMMONFLAGS=%COMMONFLAGS% /Zi /FS
set LINKFLAGS=/link /OUT:%OUT% /OPT:REF /OPT:ICF /DEBUG:FULL /PDB:%~dp0\%RELEASE_DIR%\xm6_libretro.pdb winmm.lib
set OBJDIR=build_%ARCH%

if not exist "%OBJDIR%" mkdir "%OBJDIR%"

set SOURCES=^
  xm6_libretro.cpp ^
  ..\vm\adpcm.cpp ^
  ..\vm\areaset.cpp ^
  ..\vm\cpu.cpp ^
  ..\vm\cpudebug.c ^
  ..\vm\crtc.cpp ^
  ..\vm\device.cpp ^
  ..\vm\disk.cpp ^
  ..\vm\dmac.cpp ^
  ..\vm\event.cpp ^
  ..\vm\fdc.cpp ^
  ..\vm\fdd.cpp ^
  ..\vm\fdi.cpp ^
  ..\vm\fileio.cpp ^
  ..\vm\filepath.cpp ^
  ..\vm\fmgen.cpp ^
  ..\vm\fmtimer.cpp ^
  ..\vm\gvram.cpp ^
  ..\vm\iosc.cpp ^
  ..\vm\keyboard.cpp ^
  ..\vm\log.cpp ^
  ..\vm\memory.cpp ^
  ..\vm\mercury.cpp ^
  ..\vm\mfp.cpp ^
  ..\vm\midi.cpp ^
  ..\vm\mouse.cpp ^
  ..\vm\neptune.cpp ^
  ..\vm\opm.cpp ^
  ..\vm\opmif.cpp ^
  ..\vm\ppi.cpp ^
  ..\vm\printer.cpp ^
  ..\vm\render.cpp ^
  ..\vm\renderfast_compositor.cpp ^
  ..\vm\rend_soft.cpp ^
  ..\vm\rtc.cpp ^
  ..\vm\sasi.cpp ^
  ..\vm\scc.cpp ^
  ..\vm\schedule.cpp ^
  ..\vm\scsi.cpp ^
  ..\vm\sprite.cpp ^
  ..\vm\sram.cpp ^
  ..\vm\sram_default.cpp ^
  ..\vm\sync.cpp ^
  ..\vm\sysport.cpp ^
  ..\vm\tvram.cpp ^
  ..\vm\vc.cpp ^
  ..\vm\vm.cpp ^
  ..\vm\windrv.cpp ^
  ..\vm\core_soft.cpp ^
  ..\vm\xm6core_diag.cpp ^
  ..\vm\xm6core_impl.cpp ^
  ..\vm\xm6core_runtime.cpp ^
  ..\vm\xm6core_stubs.cpp ^
  ..\vm\x68sound_bridge.cpp ^
  ..\vm\musashi_adapter.cpp ^
  ..\vm\musashi_mem.cpp ^
  ..\cpu\Musashi\softfloat\softfloat.cpp ^
  ..\cpu\Musashi\m68kcpu.cpp ^
  ..\cpu\Musashi\m68kops.cpp ^
  ..\cpu\Musashi\m68kdasm.cpp

cl %COMMONFLAGS% %DEFINES% %INCLUDES% /Fo"%OBJDIR%\\" %SOURCES% %YMFM_SOURCES% %X68SOUND_SOURCES% %LINKFLAGS%
if errorlevel 1 (
  if exist "%OUT%" del /q "%OUT%" >nul 2>nul
  echo Build failed.
  exit /b 1
)

if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"
copy /Y "%OUT%" "%RELEASE_DIR%\xm6_libretro.dll" >nul
if errorlevel 1 (
  echo Failed to copy %OUT% to %RELEASE_DIR%\xm6_libretro.dll
  exit /b 1
)

if exist "%OUT:.dll=.pdb%" (
  copy /Y "%OUT:.dll=.pdb%" "%RELEASE_DIR%\xm6_libretro.pdb" >nul
)

echo Built %OUT%
echo Copied %OUT% to %RELEASE_DIR%\xm6_libretro.dll
exit /b 0
