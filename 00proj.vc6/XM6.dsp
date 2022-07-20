# Microsoft Developer Studio Project File - Name="XM6" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=XM6 - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "XM6.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "XM6.mak" CFG="XM6 - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "XM6 - Win32 Release" ("Win32 (x86) Application" 用)
!MESSAGE "XM6 - Win32 Debug" ("Win32 (x86) Application" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "XM6 - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\vm" /I "..\mfc" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_AFXDLL" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 imm32.lib winmm.lib version.lib dinput.lib dsound.lib dxguid.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "XM6 - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\vm" /I "..\mfc" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_AFXDLL" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 imm32.lib winmm.lib version.lib dinput.lib dsound.lib dxguid.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "XM6 - Win32 Release"
# Name "XM6 - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "vm"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\vm\adpcm.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\areaset.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\core_asm.asm

!IF  "$(CFG)" == "XM6 - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\vm\core_asm.asm
InputName=core_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -o $(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "XM6 - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\vm\core_asm.asm
InputName=core_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -o $(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\vm\cpu.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\cpudebug.c
# End Source File
# Begin Source File

SOURCE=..\vm\crtc.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\device.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\disk.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\dmac.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\event.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\fdc.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\fdd.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\fdi.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\fileio.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\filepath.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\fmgen.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\fmtimer.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\gvram.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\iosc.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\keyboard.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\log.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\memory.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\mercury.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\mfp.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\midi.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\mouse.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\neptune.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\opm.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\opmif.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\ppi.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\printer.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\rend_asm.asm

!IF  "$(CFG)" == "XM6 - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\vm\rend_asm.asm
InputName=rend_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -o $(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "XM6 - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\vm\rend_asm.asm
InputName=rend_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -o $(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\vm\render.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\rtc.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\sasi.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\scc.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\schedule.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\scsi.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\sprite.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\sram.cpp
# End Source File
# Begin Source File

SOURCE=..\cpu\star.asm

!IF  "$(CFG)" == "XM6 - Win32 Release"

# Begin Custom Build
InputDir=..\cpu
IntDir=.\Release
InputPath=..\cpu\star.asm
InputName=star

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(InputDir)\star $(InputDir)\star.asm 
	nasmw -f win32 -o $(IntDir)\$(InputName).obj $(InputPath) 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "XM6 - Win32 Debug"

# Begin Custom Build
InputDir=..\cpu
IntDir=.\Debug
InputPath=..\cpu\star.asm
InputName=star

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(InputDir)\star $(InputDir)\star.asm 
	nasmw -f win32 -o $(IntDir)\$(InputName).obj $(InputPath) 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\vm\sync.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\sysport.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\tvram.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\vc.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\vm.cpp
# End Source File
# Begin Source File

SOURCE=..\vm\windrv.cpp
# End Source File
# End Group
# Begin Group "mfc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\mfc\mfc_app.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_asm.asm

!IF  "$(CFG)" == "XM6 - Win32 Release"

# Begin Custom Build
IntDir=.\Release
InputPath=..\mfc\mfc_asm.asm
InputName=mfc_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -o $(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "XM6 - Win32 Debug"

# Begin Custom Build
IntDir=.\Debug
InputPath=..\mfc\mfc_asm.asm
InputName=mfc_asm

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -f win32 -o $(IntDir)\$(InputName).obj $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_cfg.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_cmd.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_com.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_cpu.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_dev.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_draw.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_frm.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_host.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_info.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_inp.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_midi.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_port.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_que.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_rend.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_res.rc
# ADD BASE RSC /l 0x411 /i "..\mfc"
# ADD RSC /l 0x411 /i "..\" /i "..\mfc"
# SUBTRACT RSC /x
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_sch.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_snd.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_stat.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_sub.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_sys.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_tkey.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_tool.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_ver.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_vid.cpp
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_w32.cpp
# End Source File
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "vm_header"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\vm\adpcm.h
# End Source File
# Begin Source File

SOURCE=..\vm\areaset.h
# End Source File
# Begin Source File

SOURCE=..\vm\cisc.h
# End Source File
# Begin Source File

SOURCE=..\vm\config.h
# End Source File
# Begin Source File

SOURCE=..\vm\core_asm.h
# End Source File
# Begin Source File

SOURCE=..\vm\cpu.h
# End Source File
# Begin Source File

SOURCE=..\vm\cpudebug.h
# End Source File
# Begin Source File

SOURCE=..\vm\crtc.h
# End Source File
# Begin Source File

SOURCE=..\vm\device.h
# End Source File
# Begin Source File

SOURCE=..\vm\disk.h
# End Source File
# Begin Source File

SOURCE=..\vm\dmac.h
# End Source File
# Begin Source File

SOURCE=..\vm\event.h
# End Source File
# Begin Source File

SOURCE=..\vm\fdc.h
# End Source File
# Begin Source File

SOURCE=..\vm\fdd.h
# End Source File
# Begin Source File

SOURCE=..\vm\fdi.h
# End Source File
# Begin Source File

SOURCE=..\vm\fileio.h
# End Source File
# Begin Source File

SOURCE=..\vm\filepath.h
# End Source File
# Begin Source File

SOURCE=..\vm\fmgen.h
# End Source File
# Begin Source File

SOURCE=..\vm\fmgeninl.h
# End Source File
# Begin Source File

SOURCE=..\vm\fmtimer.h
# End Source File
# Begin Source File

SOURCE=..\vm\gvram.h
# End Source File
# Begin Source File

SOURCE=..\vm\iosc.h
# End Source File
# Begin Source File

SOURCE=..\vm\keyboard.h
# End Source File
# Begin Source File

SOURCE=..\vm\log.h
# End Source File
# Begin Source File

SOURCE=..\vm\memory.h
# End Source File
# Begin Source File

SOURCE=..\vm\mercury.h
# End Source File
# Begin Source File

SOURCE=..\vm\mfp.h
# End Source File
# Begin Source File

SOURCE=..\vm\midi.h
# End Source File
# Begin Source File

SOURCE=..\vm\mouse.h
# End Source File
# Begin Source File

SOURCE=..\vm\neptune.h
# End Source File
# Begin Source File

SOURCE=..\vm\opm.h
# End Source File
# Begin Source File

SOURCE=..\vm\opmif.h
# End Source File
# Begin Source File

SOURCE=..\vm\os.h
# End Source File
# Begin Source File

SOURCE=..\vm\ppi.h
# End Source File
# Begin Source File

SOURCE=..\vm\printer.h
# End Source File
# Begin Source File

SOURCE=..\vm\rend_asm.h
# End Source File
# Begin Source File

SOURCE=..\vm\render.h
# End Source File
# Begin Source File

SOURCE=..\vm\renderin.h
# End Source File
# Begin Source File

SOURCE=..\vm\rtc.h
# End Source File
# Begin Source File

SOURCE=..\vm\sasi.h
# End Source File
# Begin Source File

SOURCE=..\vm\scc.h
# End Source File
# Begin Source File

SOURCE=..\vm\schedule.h
# End Source File
# Begin Source File

SOURCE=..\vm\scsi.h
# End Source File
# Begin Source File

SOURCE=..\vm\sprite.h
# End Source File
# Begin Source File

SOURCE=..\vm\sram.h
# End Source File
# Begin Source File

SOURCE=..\vm\starcpu.h
# End Source File
# Begin Source File

SOURCE=..\vm\sync.h
# End Source File
# Begin Source File

SOURCE=..\vm\sysport.h
# End Source File
# Begin Source File

SOURCE=..\vm\tvram.h
# End Source File
# Begin Source File

SOURCE=..\vm\vc.h
# End Source File
# Begin Source File

SOURCE=..\vm\vm.h
# End Source File
# Begin Source File

SOURCE=..\vm\windrv.h
# End Source File
# Begin Source File

SOURCE=..\vm\xm6.h
# End Source File
# End Group
# Begin Group "mfc_header"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\mfc\mfc.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_app.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_asm.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_cfg.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_com.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_cpu.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_dev.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_draw.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_frm.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_host.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_info.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_inp.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_midi.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_port.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_que.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_rend.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_res.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_sch.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_snd.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_stat.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_sub.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_sys.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_tkey.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_tool.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_ver.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_vid.h
# End Source File
# Begin Source File

SOURCE=..\mfc\mfc_w32.h
# End Source File
# End Group
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
