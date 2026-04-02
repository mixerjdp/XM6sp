//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_h)
#define mfc_h

//---------------------------------------------------------------------------
//
//	#include
//
//---------------------------------------------------------------------------
#define WINVER					0x500
#define _WIN32_WINNT			0x410
#define VC_EXTRALEAN

// MFC
#include <afxwin.h>
#include <afxext.h>
#include <afxdlgs.h>
#include <afxcmn.h>
#include <afxmt.h>
#include <afxconv.h>

// Win32API
#include <imm.h>
#include <mmsystem.h>
#include <shlobj.h>

// DirectX
#define DIRECTSOUND_VERSION		0x500	// DirectX5 or higher
#include <dsound.h>
#define DIRECTINPUT_VERSION		0x500	// DirectX5 or higher
#include <dinput.h>

// C Runtime
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

//---------------------------------------------------------------------------
//
//	#define
//
//---------------------------------------------------------------------------
#if defined(_DEBUG)
#define new		DEBUG_NEW
#endif	// _DEBUG

#if defined(_MSC_VER) && defined(_M_IX86)
#define FASTCALL	__fastcall
#else
#define FASTCALL
#endif	// _MSC_VER

//---------------------------------------------------------------------------
//
//	Class declaration
//
//---------------------------------------------------------------------------
class VM;
										// Virtual machine

class CApp;
										// Application
class CFrmWnd;
										// Frame window
class CDrawView;
										// Draw view
class CStatusView;
										// Status view

class CSubWnd;
										// Sub window
class CSubTextWnd;
										// Sub window (text)
class CSubListWnd;
										// Sub window (list control)

class CComponent;
										// Component base
class CConfig;
										// Config component
class CScheduler;
										// Scheduler component
class CSound;
										// Sound component
class CInput;
										// Input component
class CPort;
										// Port component
class CMIDI;
										// MIDI component
class CTKey;
										// TrueKey component
class CHost;
										// Host component
class CInfo;
										// Info component

//---------------------------------------------------------------------------
//
//	Global
//
//---------------------------------------------------------------------------
extern VM *pVM;
										// Virtual machine
BOOL FASTCALL IsJapanese(void);
										// Japanese environment check
BOOL FASTCALL IsWinNT(void);
										// Windows NT environment check
BOOL FASTCALL Support932(void);
										// CP932 support check
BOOL FASTCALL IsMMX(void);
										// MMX environment check
BOOL FASTCALL IsCMOV(void);
										// CMOV support check
void FASTCALL GetMsg(UINT uID, CString& string);
										// Get message
VM* FASTCALL GetVM(void);
										// Get virtual machine
void FASTCALL LockVM(void);
										// Lock virtual machine
void FASTCALL UnlockVM(void);
										// Unlock virtual machine
BOOL FASTCALL FileOpenDlg(CWnd *pParent, LPTSTR lpszPath, UINT nFilterID);
										// File open dialog
BOOL FASTCALL FileSaveDlg(CWnd *pParent, LPTSTR lpszPath, LPCTSTR lpszExt, UINT nFilterID);
										// File save dialog
void FASTCALL SetInfoMsg(LPCTSTR lpszBuf, BOOL bRec);
										// Info message holder
int FASTCALL DrawTextWide(HDC hDC, LPCWSTR lpString, int nCount, LPRECT lpRect, UINT uFormat);
										// DrawTextW wrapper

#endif	// mfc_h
#endif	// _WIN32
