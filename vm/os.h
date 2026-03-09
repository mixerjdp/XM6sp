//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 PI.(ytanaka@ipc-tokai.or.jp)
//	[ OS ]
//
//---------------------------------------------------------------------------

#if !defined(os_h)
#define os_h

//---------------------------------------------------------------------------
//
//	Windows (Windows98/Me/2000/XP)
//
//---------------------------------------------------------------------------
#if defined(_WIN32)
#include <tchar.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#endif	// _WIN32

#endif	// os_h
