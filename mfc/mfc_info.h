//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ MFC Info ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_info_h)
#define mfc_info_h

#include "mfc_com.h"

//===========================================================================
//
//	Info
//
//===========================================================================
class CInfo : public CComponent
{
public:
	// Constant values
	enum {
		InfoBufMax = 100				// Maximum info buffer characters
	};

public:
	// Basic methods
	CInfo(CFrmWnd *pWnd, CStatusBar *pBar);
										// Constructor
	BOOL FASTCALL Init();
										// Initialization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL ApplyCfg(const Config *pConfig);
										// Apply settings
	void FASTCALL Enable(BOOL bEnable);
										// Enable control

	// Caption
	void FASTCALL ResetCaption();
										// Reset caption
	void FASTCALL UpdateCaption();
										// Update caption

	// Info
	void FASTCALL SetInfo(CString& strInfo);
										// Set info string
	void FASTCALL SetMessageString(const CString& strMessage) const;
										// Set status bar message
	void FASTCALL UpdateInfo();
										// Update info

	// Status bar
	void FASTCALL ResetStatus();
										// Reset status
	void FASTCALL UpdateStatus();
										// Update status
	void FASTCALL DrawStatus(int nPane, HDC hDC, CRect& rectDraw);
										// Draw status

	// Status view
	void FASTCALL SetStatusView(CStatusView *pView);
										// Set status view

private:
	// Constant values
	enum {
		CapTimeLong = 1500,				// Caption update interval (long)
		CapTimeShort = 300,				// Caption update interval (short)
		InfoTime = 2000,				// Info display duration
		PaneMax = 5,					// Status bar max panes
		DiskTypeTime = 12				// Disk type polling interval
	};

	// Caption
	BOOL FASTCALL CheckParcent();
										// Percent check
	BOOL FASTCALL CheckVM();
										// VM file check
	BOOL FASTCALL CheckMO();
										// MO file check
	BOOL FASTCALL CheckCD();
										// CD file check
	void FASTCALL SetCaption();
										// Set caption
	CString m_strRun;
										// Running message
	CString m_strStop;
										// Stopped message
	BOOL m_bRun;
										// Scheduler running
	BOOL m_bCount;
										// Progress count
	int m_nParcent;
										// Progress (-1 means unknown)
	DWORD m_dwTick;
										// Elapsed time (GetTickCount)
	DWORD m_dwTime;
										// Elapsed time (GetTotalTime)
	TCHAR m_szVM[_MAX_PATH];
										// VM file path
	TCHAR m_szVMFull[_MAX_PATH];
										// VM file path (full)
	TCHAR m_szMO[_MAX_PATH];
										// MO file path
	TCHAR m_szMOFull[_MAX_PATH];
										// MO file path (full)
	TCHAR m_szCD[_MAX_PATH];
										// CD file path
	TCHAR m_szCDFull[_MAX_PATH];
										// CD file path (full)
	CScheduler *m_pSch;
										// Scheduler (Win)
	Scheduler *m_pScheduler;
										// Scheduler
	SASI *m_pSASI;
										// SASI
	SCSI *m_pSCSI;
										// SCSI

	// Info
	BOOL m_bInfo;
										// Info validity
	BOOL m_bPower;
										// Power ON/OFF
	CString m_strInfo;
										// Info string
	CString m_strPower;
										// Power OFF string
	TCHAR m_szInfo[InfoBufMax];
										// Status bar string
	DWORD m_dwInfo;
										// Info display duration

	// Status bar
	COLORREF FASTCALL StatusFloppy(LPTSTR szText, int nDrive) const;
										// Status FD
	COLORREF FASTCALL StatusHardDisk(LPTSTR szText);
										// Status HD
	COLORREF FASTCALL StatusTimer(LPTSTR szText) const;
										// Status TIMER
	COLORREF FASTCALL StatusPower(LPTSTR szText) const;
										// Status POWER
	COLORREF m_colStatus[PaneMax];
										// Status display color
	TCHAR m_szStatus[PaneMax][_MAX_PATH];
										// Status bar string
	BYTE m_bmpDrive[2][0x100];
										// Status bar drive BMP
	FDD *m_pFDD;
										// FDD
	RTC *m_pRTC;
										// RTC
	DWORD m_dwNumber;
										// Update counter
	DWORD m_dwDiskID;
										// Disk BUSY state
	DWORD m_dwDiskTime;
										// Disk type polling duration

	// Status bar
	CStatusBar *m_pStatusBar;
										// Status bar

	// Status view
	CStatusView *m_pStatusView;
										// Status view

	// Component initialization
	BOOL m_bFloppyLED;
										// Floppy LED state
	BOOL m_bPowerLED;
										// Power LED
	BOOL m_bCaptionInfo;
										// Caption info display
};

#endif	// mfc_info_h
#endif	// _WIN32
