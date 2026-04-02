//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC Info ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "schedule.h"
#include "sasi.h"
#include "scsi.h"
#include "fdd.h"
#include "fdi.h"
#include "rtc.h"
#include "memory.h"
#include "fileio.h"
#include "config.h"
#include "mfc_frm.h"
#include "mfc_com.h"
#include "mfc_stat.h"
#include "mfc_sch.h"
#include "mfc_cfg.h"
#include "mfc_res.h"
#include "mfc_info.h"

//===========================================================================
//
//	Info
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CInfo::CInfo(CFrmWnd *pWnd, CStatusBar *pBar) : CComponent(pWnd)
{
	int nPane;

	// Component parameter
	m_dwID = MAKEID('I', 'N', 'F', 'O');
	m_strDesc = _T("Info Center");

	// Option
	m_strRun.Empty();
	m_strStop.Empty();
	m_szVM[0] = _T('\0');
	m_szVMFull[0] = _T('\0');
	m_szMO[0] = _T('\0');
	m_szMOFull[0] = _T('\0');
	m_szCD[0] = _T('\0');
	m_szCDFull[0] = _T('\0');
	m_bRun = FALSE;
	m_nParcent = -1;
	m_bCount = FALSE;
	m_dwTime = 0;
	m_dwTick = 0;
	m_pSch = NULL;
	m_pScheduler = NULL;
	m_pSASI = NULL;
	m_pSCSI = NULL;

	// Info
	m_bInfo = FALSE;
	m_bPower = FALSE;
	m_dwInfo = 0;
	m_strInfo.Empty();
	m_szInfo[0] = _T('\0');

	// Status
	for (nPane=0; nPane<PaneMax; nPane++) {
		m_colStatus[nPane] = (COLORREF)-1;
		m_szStatus[nPane][0] = _T('\0');
	}
	m_bFloppyLED = FALSE;
	m_bPowerLED = FALSE;
	m_bCaptionInfo = FALSE;
	m_pFDD = NULL;
	m_pRTC = NULL;
	m_dwNumber = 0;
	m_dwDiskID = 0;
	m_dwDiskTime = DiskTypeTime;

	// Status bar
	ASSERT(pBar);
	m_pStatusBar = pBar;

	// Status view
	m_pStatusView = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInfo::Init()
{
	CString strIdle;

	ASSERT(this);

	// Base class
	if (!CComponent::Init()) {
		return FALSE;
	}

	// Option
	::GetMsg(IDS_CAPTION_RUN, m_strRun);
	::GetMsg(IDS_CAPTION_STOP, m_strStop);
	m_pSch = (CScheduler*)SearchComponent(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pSch);
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);
	m_pSASI = (SASI*)::GetVM()->SearchDevice(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(m_pSASI);
	m_pSCSI = (SCSI*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(m_pSCSI);

	// Info
	::SetInfoMsg(m_szInfo, TRUE);
	::GetMsg(AFX_IDS_IDLEMESSAGE, strIdle);
	if (strIdle.GetLength() < 0x100) {
		_tcscpy(m_szInfo, (LPCTSTR)strIdle);
	}
	::GetMsg(IDS_POWEROFF, m_strPower);
	m_bPower = ::GetVM()->IsPower();

	// Status
	m_pFDD = (FDD*)::GetVM()->SearchDevice(MAKEID('F', 'D', 'D', ' '));
	ASSERT(m_pFDD);
	m_pRTC = (RTC*)::GetVM()->SearchDevice(MAKEID('R', 'T', 'C', ' '));
	ASSERT(m_pRTC);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::Cleanup()
{
	ASSERT(this);


	// Base class
	CComponent::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::ApplyCfg(const Config* pConfig)
{
	ASSERT(this);
	ASSERT(pConfig);

	// Status
	m_bFloppyLED = pConfig->floppy_led;
	m_bPowerLED = pConfig->power_led;
	m_bCaptionInfo = pConfig->caption_info;
}

//---------------------------------------------------------------------------
//
//	Enable
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::Enable(BOOL bEnable)
{
	CConfig *pConfig;
	Config cfg;
	int i;
	int x;
	int y;
	BITMAPINFOHEADER *bmi;
	Memory *pMemory;
	const BYTE *pCG;
	const BYTE *pChr;
	BYTE *pBits;
	BYTE byteFont;

	// When enabled (may start emulation)
	if (bEnable) {
		// Get config manager
		pConfig = (CConfig*)SearchComponent(MAKEID('C', 'F', 'G', ' '));
		ASSERT(pConfig);

		// Get config data
		pConfig->GetConfig(&cfg);

		// Power OFF: clear and ON count
		if (cfg.power_off) {
			// Power OFF state
			m_bPower = FALSE;
			m_bRun = FALSE;
			m_bCount = TRUE;
		}
		else {
			// Power ON state
			m_bPower = TRUE;
			m_bRun = TRUE;
			m_bCount = FALSE;
		}
		m_dwTick = ::GetTickCount();

		// Drive bitmap create
		pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
		ASSERT(pMemory);
		pCG = pMemory->GetCG();
		ASSERT(pCG);

		// Drive loop
		for (i=0; i<2; i++) {
			// Size check
			ASSERT((sizeof(BITMAPINFOHEADER) + 8 * 8 * 3) <= sizeof(m_bmpDrive[i]));

			// Clear
			memset(m_bmpDrive[i], 0, sizeof(m_bmpDrive[i]));

			// Bitmap header setup
			bmi = (BITMAPINFOHEADER*)&m_bmpDrive[i][0];
			bmi->biSize = sizeof(BITMAPINFOHEADER);
			bmi->biWidth = 8;
			bmi->biHeight = -8;
			bmi->biPlanes = 1;
			bmi->biBitCount = 24;
			bmi->biCompression = BI_RGB;
			bmi->biSizeImage = 8 * 8 * 3;

			// Address setup
			pChr = pCG + 0x3a000 + (('0' + i) << 3);
			pBits = &m_bmpDrive[i][sizeof(BITMAPINFOHEADER)];

			// x, y loop
			for (y=0; y<8; y++) {
				// Get CGROM data
				byteFont = pChr[y ^ 1];

				// Convert 1 bit at a time
				for (x=0; x<8; x++) {
					if (byteFont & 0x80) {
						pBits[0] = 208;
						pBits[1] = 208;
						pBits[2] = 208;
					}
					byteFont <<= 1;
					pBits += 3;
				}
			}
		}
	}

	// Base class
	CComponent::Enable(bEnable);
}

//===========================================================================
//
//	Option
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Option caption reset
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::ResetCaption()
{
	CString strCap;

	ASSERT(this);
	ASSERT(m_pSch);

	// Invert current state
	m_bRun = !m_pSch->IsEnable();

	// Update, percentage state opposite
	if (m_bRun) {
		m_bCount = FALSE;
		m_nParcent = -1;
	}
	else {
		m_bCount = TRUE;
		m_nParcent = -1;
	}

	// Set message according to current state
	if (m_pSch->IsEnable()) {
		strCap = m_strRun;
	}
	else {
		strCap = m_strStop;
	}

	// Set
	m_pFrmWnd->SetWindowText(strCap);
	if (m_pStatusView) {
		m_pStatusView->SetCaptionString(strCap);
	}
}

//---------------------------------------------------------------------------
//
//	Option caption update
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::UpdateCaption()
{
	BOOL bParcent;
	BOOL bVM;
	BOOL bMO;
	BOOL bCD;

	ASSERT(this);

	// Parcent check
	bParcent = CheckParcent();

	// VM check
	bVM = CheckVM();

	// MO check
	bMO = CheckMO();

	// CD check
	bCD = CheckCD();

	// If anything changed, set caption
	if (bParcent || bVM || bMO || bCD) {
		SetCaption();
	}
}

//---------------------------------------------------------------------------
//
//	Parcent check
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInfo::CheckParcent()
{
	BOOL bRun;
	DWORD dwDiff;
	DWORD dwTick;
	int nParcent;

	ASSERT(this);
	ASSERT(m_pSch);
	ASSERT(m_pScheduler);

	// Backup previous state
	bRun = m_bRun;

	// Get current state
	m_bRun = m_pSch->IsEnable();

	// When stopped, calculate relative time
	if (!m_bRun) {
		// Was stopped before
		if (!bRun) {
			// If already calculated, explain and update
			if (m_bCount) {
				m_bCount = FALSE;
				return TRUE;
			}
			return FALSE;
		}

		// Not calculated
		m_bCount = FALSE;

		// Percentage invalid
		m_nParcent = -1;

		// Changed
		return TRUE;
	}

	// If execution started or relative time not calculated (STOP to RUN transition)
	if (!m_bCount || !bRun) {
		// Start calculation
		m_bCount = TRUE;
		m_dwTick = ::GetTickCount();
		m_dwTime = m_pScheduler->GetTotalTime();

		// Percentage invalid
		m_nParcent = -1;

		// Changed
		return TRUE;
	}

	// Only RUN to RUN case
	ASSERT(m_bCount);
	ASSERT(bRun);
	ASSERT(m_bRun);

	// Get elapsed time
	dwDiff = ::GetTickCount();
	dwDiff -= m_dwTick;
	if (m_nParcent >= 0) {
		// Previously percentage was calculated
		if (dwDiff < CapTimeLong) {
			// No change
			return FALSE;
		}
	}
	else {
		// If not yet calculated
		if (dwDiff < CapTimeShort) {
			// No change
			return FALSE;
		}
	}

	// Get elapsed time, real time
	dwTick = ::GetTickCount();
	m_dwTime = m_pScheduler->GetTotalTime() - m_dwTime;
	m_dwTick = dwTick - m_dwTick;

	// Calculate by division
	if ((m_dwTime == 0) || (m_dwTick == 0)) {
		nParcent = 0;
	}
	else {
		// VM time is 0.5us unit
		nParcent = (int)m_dwTime;
		nParcent /= (int)m_dwTick;
		if (nParcent > 0) {
			nParcent /= 2;
		}
	}

	// Reset
	m_dwTime = m_pScheduler->GetTotalTime();
	m_dwTick = dwTick;

	// Round (4 or 5 up)
	if ((nParcent % 10) >= 5) {
		nParcent /= 10;
		nParcent++;
	}
	else {
		nParcent /= 10;
	}

	// If changed, update and return TRUE
	if (m_nParcent != nParcent) {
		m_nParcent = nParcent;
		return TRUE;
	}

	// Same as before, return FALSE
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	VM file check
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInfo::CheckVM()
{
	Filepath path;
	LPCTSTR lpszPath;
	LPCTSTR lpszFileExt;

	ASSERT(this);
	ASSERT(m_pSASI);

	// Get VM path
	::GetVM()->GetPath(path);
	lpszPath = path.GetPath();

	// Check if previous and current paths match
	if (_tcscmp(lpszPath, m_szVMFull) == 0) {
		// No change
		return FALSE;
	}

	// Copy all to previous
	_tcscpy(m_szVMFull, lpszPath);

	// Extract only filename and extension
	lpszFileExt = path.GetFileExt();

	// Update
	_tcscpy(m_szVM, lpszFileExt);

	// Changed
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	MO file check
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInfo::CheckMO()
{
	Filepath path;
	LPCTSTR lpszPath;
	LPCTSTR lpszFileExt;

	ASSERT(this);
	ASSERT(m_pSASI);

	// Get MO path
	m_pSASI->GetPath(path);
	lpszPath = path.GetPath();

	// Check if previous and current paths match
	if (_tcscmp(lpszPath, m_szMOFull) == 0) {
		// No change
		return FALSE;
	}

	// Copy all to previous
	_tcscpy(m_szMOFull, lpszPath);

	// Extract only filename and extension
	lpszFileExt = path.GetFileExt();

	// Update
	_tcscpy(m_szMO, lpszFileExt);

	// Changed
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	CD file check
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInfo::CheckCD()
{
	Filepath path;
	LPCTSTR lpszPath;
	LPCTSTR lpszFileExt;

	ASSERT(this);
	ASSERT(m_pSCSI);

	// Get CD path
	m_pSCSI->GetPath(path, FALSE);
	lpszPath = path.GetPath();

	// Check if previous and current paths match
	if (_tcscmp(lpszPath, m_szCDFull) == 0) {
		// No change
		return FALSE;
	}

	// Copy all to previous
	_tcscpy(m_szCDFull, lpszPath);

	// Extract only filename and extension
	lpszFileExt = path.GetFileExt();

	// Update
	_tcscpy(m_szCD, lpszFileExt);

	// Changed
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Option caption set
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::SetCaption()
{
	CString strCap;
	CString strSub;

	ASSERT(this);
	ASSERT(m_pFrmWnd);

	// Run or stop
	if (m_bRun) {
		// Running
		strCap = m_strRun;
	}
	else {
		// Stopped
		strCap = m_strStop;
	}

	// Set (if not showing info in caption)
	if (m_bCaptionInfo == FALSE) {
		strCap = _T("XM6");

		// Caption
		m_pFrmWnd->SetWindowText(strCap);

		// Status view
		if (m_pStatusView) {
			m_pStatusView->SetCaptionString(strCap);
		}
	}

	// % display
	if (m_nParcent >= 0) {
		strSub.Format(_T(" - %3d%%"), m_nParcent);
		strCap += strSub;
	}

	// VM
	if (_tcslen(m_szVM) > 0) {
		strSub.Format(_T(" [%s] "), m_szVM);
		strCap += strSub;
	}

	// MO
	if (_tcslen(m_szMO) > 0) {
		strSub.Format(_T(" (%s) "), m_szMO);
		strCap += strSub;
	}

	// CD
	if (_tcslen(m_szCD) > 0) {
		strSub.Format(_T(" {%s} "), m_szCD);
		strCap += strSub;
	}

	// Set (if showing info in caption)
	if (m_bCaptionInfo) {
		// Caption
		m_pFrmWnd->SetWindowText(strCap);

		// Status view
		if (m_pStatusView) {
			m_pStatusView->SetCaptionString(strCap);
		}
	}
}

//===========================================================================
//
//	Info
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Info set
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::SetInfo(CString& strInfo)
{
	ASSERT(this);

	// Message save
	m_strInfo = strInfo;

	// Flag, backup, time record
	m_bInfo = TRUE;
	m_dwInfo = ::GetTickCount();

	// Set to status bar pane
	ASSERT(m_pStatusBar);
	m_pStatusBar->SetPaneText(0, m_strInfo, TRUE);

	// Set to status view
	if (m_pStatusView) {
		m_pStatusView->SetInfoString(m_strInfo);
	}
}

//---------------------------------------------------------------------------
//
//	Message string set
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::SetMessageString(const CString& strMessage) const
{
	LPCTSTR lpszMessage;

	// Convert to LPCTSTR
	lpszMessage = (LPCTSTR)strMessage;

	// Discard const to call C function
	::SetInfoMsg(lpszMessage, FALSE);
}

//---------------------------------------------------------------------------
//
//	Info update
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::UpdateInfo()
{
	BOOL bPower;
	DWORD dwDiff;
	CString strText;

	ASSERT(this);

	// Check power state
	bPower = ::GetVM()->IsPower();
	if (m_bPower && !bPower) {
		// Power changed from ON to OFF. Show power message
		SetInfo(m_strPower);
	}
	// Update power state
	m_bPower = bPower;

	// If no info flag or OFF
	if (!m_bInfo) {
		// Do nothing
		return;
	}

	// Time calculation. Minimum 2000ms
	dwDiff = ::GetTickCount();
	dwDiff -= m_dwInfo;
	if (dwDiff < InfoTime) {
		// Still showing
		return;
	}

	// Turn info flag OFF
	m_bInfo = FALSE;

	// Restore message string
	strText = m_szInfo;

	// Set to status bar pane
	ASSERT(m_pStatusBar);
	m_pStatusBar->SetPaneText(0, strText, TRUE);

	// Set to status view
	if (m_pStatusView) {
		strText.Empty();
		m_pStatusView->SetInfoString(strText);
	}
}

//===========================================================================
//
//	Status
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Status reset
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::ResetStatus()
{
	int nPane;

	ASSERT(this);

	// Clear all and reset
	for (nPane=0; nPane<PaneMax; nPane++) {
		// Clear color
		m_colStatus[nPane] = (COLORREF)-1;

		// Clear text
		m_szStatus[nPane][0] = _T('\0');
	}
}

//---------------------------------------------------------------------------
//
//	Status update
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::UpdateStatus()
{
	COLORREF colStatus[PaneMax];
	TCHAR szStatus[PaneMax][_MAX_PATH];
	int nPane;
	BOOL bDraw;
	BOOL bAll;
	CString strNumber;

	ASSERT(this);
	ASSERT(m_pStatusBar);

	// VM lock
	::LockVM();

	// Create text and color
	ASSERT(PaneMax == 5);
	colStatus[0] = StatusFloppy(szStatus[0], 0);
	colStatus[1] = StatusFloppy(szStatus[1], 1);
	colStatus[2] = StatusHardDisk(szStatus[2]);
	colStatus[3] = StatusTimer(szStatus[3]);
	colStatus[4] = StatusPower(szStatus[4]);

	// VM unlock
	::UnlockVM();

	// Create serial number (changes this number, different meaning)
	strNumber.Format(_T("%08X"), m_dwNumber);

	// Compare view and draw
	bAll = FALSE;
	for (nPane=0; nPane<5; nPane++) {
		// Compare, whether to redraw
		bDraw = FALSE;
		if (m_colStatus[nPane] != colStatus[nPane]) {
			bDraw = TRUE;
		}
		if (_tcscmp(m_szStatus[nPane], szStatus[nPane]) != 0) {
			bDraw = TRUE;
		}

		// Skip if no need to draw
		if (!bDraw) {
			continue;
		}

		// Copy
		m_colStatus[nPane] = colStatus[nPane];
		_tcscpy(m_szStatus[nPane], szStatus[nPane]);

		// Draw
		m_pStatusBar->SetPaneText(nPane + 1, strNumber, TRUE);

		// Status view
		if (m_pStatusView) {
			m_pStatusView->DrawStatus(nPane);
		}

		// Flag up
		bAll = TRUE;
	}

	// Serial update
	if (bAll) {
		m_dwNumber++;
	}
}

//---------------------------------------------------------------------------
//
//	Status FD
//
//---------------------------------------------------------------------------
COLORREF FASTCALL CInfo::StatusFloppy(LPTSTR lpszText, int nDrive) const
{
	COLORREF colStatus;
	int nStatus;
	BOOL bPower;
	FDD::drv_t drv;
	char name[60];
	LPCTSTR lpszName;

	ASSERT(this);
	ASSERT(lpszText);
	ASSERT((nDrive == 0) || (nDrive == 1));
	ASSERT(m_pFDD);
	ASSERT(m_pSch);

	// First, get FDD data
	bPower = ::GetVM()->IsPower();
	lpszText[0] = _T('\0');
	colStatus = RGB(1, 1, 1);
	nStatus = m_pFDD->GetStatus(nDrive);

	// Blink
	if (nStatus & FDST_BLINK) {
		// Blink, which is lighter
		if ((nStatus & FDST_CURRENT) && bPower) {
			// On
			colStatus = RGB(15, 159, 15);
		}
		else {
			// Off
			colStatus = RGB(0, 0, 0);
		}
	}

	// Insert or eject
	if (nStatus & FDST_INSERT) {
		// Gray
		colStatus = RGB(95, 95, 95);

		// Get disk name
		m_pFDD->GetName(nDrive, name);
		lpszName = A2CT(name);
		_tcscpy(lpszText, lpszName);

		// Access
		if (m_bFloppyLED) {
			// Motor ON & not select
			if ((nStatus & FDST_MOTOR) && (nStatus & FDST_SELECT)) {
				colStatus = RGB(208, 31, 31);
			}
		}
		else {
			// No access
			if (nStatus & FDST_ACCESS) {
				colStatus = RGB(208, 31, 31);
			}
		}

		// Gray if power OFF
		if (!bPower) {
			colStatus = RGB(95, 95, 95);
		}
	}

	// If scheduler stopped, handle
	if (!m_pSch->IsEnable()) {
		// Get drive info
		m_pFDD->GetDrive(nDrive, &drv);
		if (!(nStatus & FDST_INSERT)) {
			// Disk is not set
			if (drv.next) {
				// Next disk exists
				colStatus = RGB(95, 95, 95);

				drv.next->GetName(name, 0);
				lpszName = A2CT(name);
				_tcscpy(lpszText, lpszName);
			}
		}
	}

	// If eject prohibited, show final state
	if (!(nStatus & FDST_EJECT)) {
		colStatus |= (COLORREF)0x80000000;
	}

	return colStatus;
}

//---------------------------------------------------------------------------
//
//	Status HD
//
//---------------------------------------------------------------------------
COLORREF FASTCALL CInfo::StatusHardDisk(LPTSTR lpszText)
{
	DWORD dwID;
	COLORREF color;

	ASSERT(this);
	ASSERT(lpszText);
	ASSERT(m_pSASI);

	// Power check
	if (!::GetVM()->IsPower()) {
		// Power not available. Gray
		_tcscpy(lpszText, _T("HD BUSY"));
		m_dwDiskID = 0;
		m_dwDiskTime = DiskTypeTime;
		return RGB(0, 0, 0);
	}

	// Get device
	dwID = m_pSASI->GetBusyDevice();
	switch (dwID) {
		// SASI-HD
		case MAKEID('S', 'A', 'H', 'D'):
			color = RGB(208, 31, 31);
			break;

		// SCSI-HD
		case MAKEID('S', 'C', 'H', 'D'):
			color = RGB(208, 31, 31);
			break;

		// SCSI-MO
		case MAKEID('S', 'C', 'M', 'O'):
			color = RGB(208, 32, 31);
			break;

		// SCSI-CD
		case MAKEID('S', 'C', 'C', 'D'):
			color = RGB(208, 31, 32);
			break;

		// Not BUSY
		case 0:
			color = RGB(0, 0, 0);
			break;

		// Others (should not occur)
		default:
			ASSERT(FALSE);
			_tcscpy(lpszText, _T("HD BUSY"));
			return RGB(0, 0, 0);
	}

	// If valid device
	if (color != RGB(0, 0, 0)) {
		// Same as previous device
		if (dwID == m_dwDiskID) {
			// Time +1 (max is DiskTypeTime)
			if (m_dwDiskTime < DiskTypeTime) {
				m_dwDiskTime++;
			}
		}
		else {
			// Device changed, reset and start time count
			m_dwDiskID = dwID;
			m_dwDiskTime = 0;
		}
	}
	else {
		// If not BUSY, time check
		if (m_dwDiskTime >= DiskTypeTime) {
			// Return to HD BUSY
			ASSERT(m_dwDiskTime == DiskTypeTime);
			_tcscpy(lpszText, _T("HD BUSY"));
			m_dwDiskID = 0;

			// Gray
			return RGB(0, 0, 0);
		}

		// Most likely, show last device with different color
		m_dwDiskTime++;
		dwID = m_dwDiskID;
		color = RGB(0, 0, 1);
	}

	// Text create
	switch (dwID) {
		// SASI-HD
		case MAKEID('S', 'A', 'H', 'D'):
			_tcscpy(lpszText, _T("HD BUSY"));
			break;

		// SCSI-HD
		case MAKEID('S', 'C', 'H', 'D'):
			_tcscpy(lpszText, _T("HD BUSY"));
			break;

		// SCSI-MO
		case MAKEID('S', 'C', 'M', 'O'):
			_tcscpy(lpszText, _T("MO BUSY"));
			break;

		// SCSI-CD
		case MAKEID('S', 'C', 'C', 'D'):
			_tcscpy(lpszText, _T("CD BUSY"));
			break;

		// Others (should not occur)
		default:
			ASSERT(FALSE);
	}

	return color;
}

//---------------------------------------------------------------------------
//
//	Status TIMER
//
//---------------------------------------------------------------------------
COLORREF FASTCALL CInfo::StatusTimer(LPTSTR lpszText) const
{
	ASSERT(this);
	ASSERT(lpszText);
	ASSERT(m_pRTC);

	// Text
	_tcscpy(lpszText, _T("TIMER"));

	// Timer ON? (power has no effect)
	if (m_pRTC->GetTimerLED()) {
		return RGB(208, 31, 31);
	}

	// Off
	return RGB(0, 0, 0);
}

//---------------------------------------------------------------------------
//
//	Status POWER
//
//---------------------------------------------------------------------------
COLORREF FASTCALL CInfo::StatusPower(LPTSTR lpszText) const
{
	VM *pVM;

	ASSERT(this);
	ASSERT(lpszText);
	ASSERT(m_pRTC);

	// Get VM
	pVM = m_pRTC->GetVM();
	ASSERT(pVM);

	// Text
	_tcscpy(lpszText, _T("POWER"));

	// Power OFF
	if (!pVM->IsPower()) {
		if (m_bPowerLED) {
			// Blue
			return RGB(12, 23, 129);
		}
		else {
			// Red
			return RGB(208, 31, 31);
		}
	}

	// Power switch ON
	if (pVM->IsPowerSW()) {
		// Blue or green
		if (m_bPowerLED) {
			return RGB(50, 50, 255);
		}
		else {
			return RGB(31, 208, 31);
		}
	}

	// When power switch OFF, ALARM output uses different color
	if (m_pRTC->GetAlarmOut()) {
		// Blue or green
		if (m_bPowerLED) {
			return RGB(50, 50, 255);
		}
		else {
			return RGB(31, 208, 31);
		}
	}
	if (m_bPowerLED) {
		// Blue
		return RGB(12, 23, 129);
	}
	else {
		// Off
		return RGB(0, 0, 0);
	}
}

//---------------------------------------------------------------------------
//
//	Status draw
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::DrawStatus(int nPane, HDC hDC, CRect& rectDraw)
{
	BOOL bHalf;
	COLORREF colStatus;
	HDC hMemDC;
	HBITMAP hBitmap;
	HBITMAP hDefBitmap;
	HFONT hFont;
	HFONT hDefFont;
	CRect rectMem;
	CRect rectLine;
	int nLine;

	ASSERT(this);
	ASSERT((nPane >= 0) && (nPane < PaneMax));
	ASSERT(hDC);

	// Set color and half flag (gray stripe pattern)
	colStatus = m_colStatus[nPane];
	bHalf = FALSE;
	if (colStatus & 0x80000000) {
		bHalf = TRUE;
		colStatus &= (COLORREF)(0x7fffffff);
	}

	// Set drawing rectangle
	rectMem.left = 0;
	rectMem.top = 0;
	rectMem.right = rectDraw.Width();
	rectMem.bottom = rectDraw.Height();

	// Create compatible DC
	hMemDC = ::CreateCompatibleDC(hDC);

	// Create bitmap and select
	hBitmap = ::CreateCompatibleBitmap(hDC, rectDraw.Width(), rectDraw.Height());
	hDefBitmap = (HBITMAP)::SelectObject(hMemDC, hBitmap);
	ASSERT(hDefBitmap);

	// Create font and select
	hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	hDefFont = (HFONT)::SelectObject(hMemDC, hFont);
	ASSERT(hDefFont);

	// Fill
	::SetBkColor(hMemDC, colStatus);
	::ExtTextOut(hMemDC, 0, 0, ETO_OPAQUE, &rectMem, NULL, 0, NULL);

	// Gray pattern
	if (bHalf) {
		// Draw gray stripes
		rectLine = rectMem;
		::SetBkColor(hMemDC, RGB(0, 0, 0));
		for (nLine=0; nLine<rectMem.bottom; nLine+=2) {
			rectLine.top = nLine;
			rectLine.bottom = nLine + 1;
			::ExtTextOut(hMemDC, 0, 0, ETO_OPAQUE, &rectLine, NULL, 0, NULL);
		}
		::SetBkColor(hMemDC, colStatus);
	}

	// Text draw (shadow, outline)
	if (!m_pStatusView || (nPane < 2) || (colStatus != 0)) {
		// To place border, shift 1 pixel right and down
		rectMem.left++;
		rectMem.top++;
		rectMem.right++;
		rectMem.bottom++;

		// Text outline (black)
		::SetTextColor(hMemDC, RGB(0, 0, 0));
		::SetBkMode(hMemDC, TRANSPARENT);
		::DrawText(hMemDC, m_szStatus[nPane], (int)_tcslen(m_szStatus[nPane]),
						 &rectMem, DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		// Return first, 1 pixel shift back
		rectMem.left--;
		rectMem.top--;
		rectMem.right--;
		rectMem.bottom--;

		// Main text (white)
		::SetTextColor(hMemDC, RGB(255, 255, 255));
		::SetBkMode(hMemDC, TRANSPARENT);
		::DrawText(hMemDC, m_szStatus[nPane], (int)_tcslen(m_szStatus[nPane]),
						 &rectMem, DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	// Draw floppy disk bitmap if needed
	if ((nPane < 2) && (colStatus != 0)) {
		// If not monochrome
		if (!m_pStatusView || (colStatus != RGB(1, 1, 1))) {
			::SetDIBitsToDevice(hMemDC, 0, 0, 8, 8, 0, 0, 0, 8,
								&m_bmpDrive[nPane][sizeof(BITMAPINFOHEADER)],
								(BITMAPINFO*)&m_bmpDrive[nPane][0],
								DIB_RGB_COLORS);
		}
	}

	// BitBlt
	::BitBlt(hDC, rectDraw.left, rectDraw.top, rectDraw.Width(), rectDraw.Height(),
						hMemDC, 0, 0, SRCCOPY);

	// Return font
	::SelectObject(hMemDC, hDefFont);

	// Return bitmap
	::SelectObject(hMemDC, hDefBitmap);
	::DeleteObject(hBitmap);

	// Return compatible DC
	::DeleteDC(hMemDC);
}

//===========================================================================
//
//	Others
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Status view notice
//
//---------------------------------------------------------------------------
void FASTCALL CInfo::SetStatusView(CStatusView *pView)
{
	ASSERT(this);

	// Unless NULL, save
	m_pStatusView = pView;
}

#endif	// _WIN32
