//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ MFC Sub-window (Win32) ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "schedule.h"
#include "opmif.h"
#include "adpcm.h"
#include "render.h"
#include "memory.h"
#include "keyboard.h"
#include "midi.h"
#include "mfc_sub.h"
#include "mfc_res.h"
#include "mfc_frm.h"
#include "mfc_com.h"
#include "mfc_snd.h"
#include "mfc_inp.h"
#include "mfc_draw.h"
#include "mfc_port.h"
#include "mfc_que.h"
#include "mfc_midi.h"
#include "mfc_sch.h"
#include "mfc_w32.h"

//===========================================================================
//
//	Component window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CComponentWnd::CComponentWnd()
{
	CFrmWnd *pFrmWnd;

	// Window parameter definition
	m_dwID = MAKEID('C', 'O', 'M', 'P');
	::GetMsg(IDS_SWND_COMPONENT, m_strCaption);
	m_nWidth = 58;
	m_nHeight = 10;

	// Get the first component
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pComponent = pFrmWnd->GetFirstComponent();
	ASSERT(m_pComponent);
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CComponentWnd::Setup()
{
	DWORD dwID;
	int nComponent;
	CString strText;
	CComponent *pComponent;

	ASSERT(this);

	// Clear
	Clear();

	// Guide
	SetString(0, 0, _T("No."));
	SetString(5, 0, _T("ID"));
	SetString(10, 0, _T("Flag"));
	SetString(17, 0, _T("Object"));
	SetString(27, 0, _T("Prev"));
	SetString(36, 0, _T("Next"));
	SetString(45, 0, _T("Description"));

	// Initialization
	pComponent = m_pComponent;
	nComponent = 1;

	// Loop
	while (pComponent) {
		// No.
		strText.Format(_T("%2d"), nComponent);
		SetString(0, nComponent, strText);

		// ID
		dwID = pComponent->GetID();
		strText.Format(_T("%c%c%c%c"),
			(dwID >> 24) & 0xff,
			(dwID >> 16) & 0xff,
			(dwID >> 8) & 0xff,
			dwID & 0xff);
		SetString(4, nComponent, strText);

		// Enable
		if (pComponent->IsEnable()) {
			SetString(9, nComponent,_T("Enable"));
		}

		// Pointer
		strText.Format(_T("%08X"), pComponent);
		SetString(16, nComponent, strText);

		// Previous pointer
		strText.Format(_T("%08X"), pComponent->GetPrevComponent());
		SetString(25, nComponent, strText);

		// Next pointer
		strText.Format(_T("%08X"), pComponent->GetNextComponent());
		SetString(34, nComponent, strText);

		// Name
		pComponent->GetDesc(strText);
		SetString(43, nComponent, strText);

		// Next
		pComponent = pComponent->GetNextComponent();
		nComponent++;
	}
}

//===========================================================================
//
//	OS info window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
COSInfoWnd::COSInfoWnd()
{
	// Window parameter definition
	m_dwID = MAKEID('O', 'S', 'I', 'N');
	::GetMsg(IDS_SWND_OSINFO, m_strCaption);
	m_nWidth = 30;
	m_nHeight = 13;
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL COSInfoWnd::Setup()
{
	int x;
	int y;
	CString strText;
	OSVERSIONINFO ovi;
	SYSTEM_INFO si;

	ASSERT(this);

	// Clear
	Clear();
	x = 25;
	y = 0;

	// Get OS version
	memset(&ovi, 0, sizeof(ovi));
	ovi.dwOSVersionInfoSize = sizeof(ovi);
	VERIFY(GetVersionEx(&ovi));

	// Set sequentially
	SetString(0, y, _T("MajorVersion"));
	strText.Format(_T("%5u"), ovi.dwMajorVersion);
	SetString(x, y, strText);
	y++;

	SetString(0, y, _T("MinorVersion"));
	strText.Format(_T("%5u"), ovi.dwMinorVersion);
	SetString(x, y, strText);
	y++;

	SetString(0, y, _T("BuildNumber"));
	strText.Format(_T("%5u"), LOWORD(ovi.dwBuildNumber));
	SetString(x, y, strText);
	y++;

	SetString(0, y, _T("Platform"));
	switch (ovi.dwPlatformId) {
		case VER_PLATFORM_WIN32_WINDOWS:
			strText = _T("Windows 9x");
			break;
		case VER_PLATFORM_WIN32_NT:
			strText = _T("Windows NT");
			break;
		default:
			strText = _T(" (Unknown)");
	}
	SetString(x - 5, y, strText);
	y++;

	SetString(0, y, _T("CSDVersion"));
	strText = ovi.szCSDVersion;
	SetString(x + 5 - strText.GetLength(), y, strText);
	y++;

	// Language information
	SetString(0, y, _T("System LangID"));
	strText.Format(_T("0x%04X"), ::GetSystemDefaultLangID());
	SetString(x - 1, y, strText);
	y++;

	// Language information
	SetString(0, y, _T("User LangID"));
	strText.Format(_T("0x%04X"), ::GetUserDefaultLangID());
	SetString(x - 1, y, strText);
	y++;

	// Get system information
	memset(&si, 0, sizeof(si));
	::GetSystemInfo(&si);

	// Set sequentially
	SetString(0, y, _T("Number of Processor"));
	strText.Format(_T("%5u"), si.dwNumberOfProcessors);
	SetString(x, y, strText);
	y++;

	SetString(0, y, _T("Processor Family"));
	strText.Format(_T("%5u"), si.wProcessorLevel);
	SetString(x, y, strText);
	y++;

	SetString(0, y, _T("Processor Model"));
	strText.Format(_T("%5u"), HIBYTE(si.wProcessorRevision));
	SetString(x, y, strText);
	y++;

	SetString(0, y, _T("Processor Revision"));
	strText.Format(_T("%5u"), LOBYTE(si.wProcessorRevision));
	SetString(x, y, strText);
	y++;

	// MMX
	SetString(0, y, _T("MMX  Support"));
	if (::IsMMX()) {
		SetString(x + 2, y, _T("Yes"));
	}
	else {
		SetString(x + 3, y, _T("No"));
	}
	y++;

	// CMOV
	SetString(0, y, _T("CMOV Support"));
	if (::IsCMOV()) {
		SetString(x + 2, y, _T("Yes"));
	}
	else {
		SetString(x + 3, y, _T("No"));
	}
}

//===========================================================================
//
//	Sound window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSoundWnd::CSoundWnd()
{
	CFrmWnd *pFrmWnd;

	// Window parameter definition
	m_dwID = MAKEID('S', 'N', 'D', ' ');
	::GetMsg(IDS_SWND_SOUND, m_strCaption);
	m_nWidth = 25;
	m_nHeight = 14;

	// Get scheduler
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);

	// Get OPM
	m_pOPMIF = (OPMIF*)::GetVM()->SearchDevice(MAKEID('O', 'P', 'M', ' '));
	ASSERT(m_pOPMIF);

	// Get ADPCM
	m_pADPCM = (ADPCM*)::GetVM()->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
	ASSERT(m_pADPCM);

	// Get sound component
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pSound = pFrmWnd->GetSound();
	ASSERT(m_pSound);
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CSoundWnd::Setup()
{
	int x;
	int y;
	CString strText;
	OPMIF::opmbuf_t opmbuf;
	ADPCM::adpcm_t adpcm;
	DWORD dwTime;

	ASSERT(this);
	ASSERT(m_pScheduler);
	ASSERT(m_pOPMIF);
	ASSERT(m_pSound);

	// Clear
	Clear();
	x = 20;
	y = 0;

	// Sound time
	dwTime = m_pScheduler->GetSoundTime();
	SetString(0, y, _T("Sound Time"));
	strText.Format(_T("%3d.%03dms"), dwTime / 2000, (dwTime % 2000) / 2);
	SetString(x - 4, y, strText);
	y++;

	// Get all buffer information
	m_pOPMIF->GetBufInfo(&opmbuf);

	// Display buffer information
	SetString(0, y, _T("Buffer Max"));
	strText.Format(_T("%5u"), opmbuf.max);
	SetString(x, y, strText);
	y++;
	SetString(0, y, _T("Buffer Num"));
	strText.Format(_T("%5u"), opmbuf.num);
	SetString(x, y, strText);
	y++;
	SetString(0, y, _T("Buffer Read"));
	strText.Format(_T("%5u"), opmbuf.read);
	SetString(x, y, strText);
	y++;
	SetString(0, y, _T("Buffer Write"));
	strText.Format(_T("%5u"), opmbuf.write);
	SetString(x, y, strText);
	y++;
	SetString(0, y, _T("Buffer Samples"));
	strText.Format(_T("%5u"), opmbuf.samples);
	SetString(x, y, strText);
	y++;
	SetString(0, y, _T("Buffer Rate"));
	strText.Format(_T("%5u"), opmbuf.rate);
	SetString(x, y, strText);
	y++;
	SetString(0, y, _T("Buffer Underrun"));
	strText.Format(_T("%5u"), opmbuf.under);
	SetString(x, y, strText);
	y++;
	SetString(0, y, _T("Buffer Overrun"));
	strText.Format(_T("%5u"), opmbuf.over);
	SetString(x, y, strText);
	y++;

	// Get ADPCM information
	m_pADPCM->GetADPCM(&adpcm);

	SetString(0, y, _T("ADPCM Sample"));
	strText.Format(_T("%06X"), adpcm.sample & 0xffffff);
	SetString(x - 1, y, strText);
	y++;

	SetString(0, y, _T("ADPCM Output"));
	strText.Format(_T("%06X"), adpcm.out & 0xffffff);
	SetString(x - 1, y, strText);
	y++;

	SetString(0, y, _T("ADPCM Num"));
	strText.Format(_T("%5u"), adpcm.number);
	SetString(x, y, strText);
	y++;

	SetString(0, y, _T("ADPCM Read"));
	strText.Format(_T("%5u"), adpcm.readpoint);
	SetString(x, y, strText);
	y++;

	SetString(0, y, _T("ADPCM Write"));
	strText.Format(_T("%5u"), adpcm.writepoint);
	SetString(x, y, strText);
}

//===========================================================================
//
//	Input window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CInputWnd::CInputWnd()
{
	CFrmWnd *pFrmWnd;

	// Window parameter definition
	m_dwID = MAKEID('I', 'N', 'P', ' ');
	::GetMsg(IDS_SWND_INPUT, m_strCaption);
	m_nWidth = 46;
	m_nHeight = 33;

	// Get input component
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pInput = pFrmWnd->GetInput();
	ASSERT(m_pInput);
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CInputWnd::Setup()
{
	int x;
	int y;

	ASSERT(this);

	// Clear
	Clear();

	// General input
	x = 0;
	y = 0;
	SetupInput(x, y);

	// Mouse
	x = 0;
	y = 4;
	SetupMouse(x, y);

	// Keyboard
	x = 26;
	y = 0;
	SetupKey(x, y);

	// Joystick A
	x = 0;
	y = 11;
	SetupJoy(x, y, 0);

	// Joystick B
	x = 25;
	y = 11;
	SetupJoy(x, y, 1);
}

//---------------------------------------------------------------------------
//
//	Setup (General)
//
//---------------------------------------------------------------------------
void FASTCALL CInputWnd::SetupInput(int x, int y)
{
	CString strText;

	ASSERT(this);
	ASSERT(m_pInput);
	ASSERT((x >= 0) && (x < m_nWidth));
	ASSERT((y >= 0) && (y < m_nHeight));

	// Process counter
	SetString(x, y, _T("Input Counter"));
	strText.Format(_T("%6u"), m_pInput->GetProcessCount());
	SetString(x + 16, y, strText);
	y++;

	// Active
	SetString(x, y, _T("Active"));
	if (m_pInput->IsActive()) {
		SetString(x + 16, y, _T("Active"));
	}
	else {
		SetString(x + 20, y, _T("No"));
	}
	y++;

	// Menu
	SetString(x, y, _T("Menu"));
	if (m_pInput->IsMenu()) {
		SetString(x + 18, y, _T("Menu"));
	}
	else {
		SetString(x + 20, y, _T("No"));
	}
}

//---------------------------------------------------------------------------
//
//	Setup(Mouse)
//
//---------------------------------------------------------------------------
void FASTCALL CInputWnd::SetupMouse(int x, int y)
{
	CString strText;
	int nPos[3];
	BOOL bBtn[3];

	ASSERT(this);
	ASSERT(m_pInput);
	ASSERT((x >= 0) && (x < m_nWidth));
	ASSERT((y >= 0) && (y < m_nHeight));

	// Acquire
	SetString(x, y, _T("Mouse Acquire"));
	strText.Format(_T("%6u"), m_pInput->GetAcquireCount(1));
	SetString(x + 16, y, strText);
	y++;

	// Mode
	SetString(x, y, _T("Mouse Mode"));
	if (m_pInput->GetMouseMode()) {
		SetString(x + 16, y, _T("Enable"));
	}
	else {
		SetString(x + 15, y, _T("Disable"));
	}
	y++;

	// Get information
	m_pInput->GetMouseInfo(nPos, bBtn);

	// X
	SetString(x, y, _T("Mouse X"));
	strText.Format(_T("%6d"), nPos[0]);
	SetString(x + 16, y, strText);
	y++;

	// Y
	SetString(x, y, _T("Mouse Y"));
	strText.Format(_T("%6d"), nPos[1]);
	SetString(x + 16, y, strText);
	y++;

	// Button (L)
	SetString(x, y, _T("Mouse Button(L)"));
	if (bBtn[0]) {
		SetString(x + 18, y, _T("Down"));
	}
	else {
		SetString(x + 20, y, _T("Up"));
	}
	y++;

	// Button (R)
	SetString(x, y, _T("Mouse Button(R)"));
	if (bBtn[1]) {
		SetString(x + 18, y, _T("Down"));
	}
	else {
		SetString(x + 20, y, _T("Up"));
	}
	y++;
}

//---------------------------------------------------------------------------
//
//	Setup(Keyboard)
//
//---------------------------------------------------------------------------
void FASTCALL CInputWnd::SetupKey(int x, int y)
{
	CString strText;
	LPCTSTR lpszID;
	int i;
	int nCount;
	BOOL buf[0x100];

	ASSERT(this);
	ASSERT(m_pInput);
	ASSERT((x >= 0) && (x < m_nWidth));
	ASSERT((y >= 0) && (y < m_nHeight));

	// Acquire
	SetString(x, y, _T("Key Acquire"));
	strText.Format(_T("%6u"), m_pInput->GetAcquireCount(0));
	SetString(x + 14, y, strText);
	y++;

	// Get key buffer
	m_pInput->GetKeyBuf(buf);

	// Count setting
	nCount = 0;

	// Key loop
	for (i=0; i<0x100; i++) {
		// Overflow check
		if (nCount >= 9) {
			break;
		}

		// Press check
		if (buf[i]) {
			// Get name
			lpszID = m_pInput->GetKeyID(i);
			ASSERT(lpszID);

			// Display
			SetString(x, y, lpszID);
			nCount++;
			y++;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Setup (Joystick)
//
//---------------------------------------------------------------------------
void FASTCALL CInputWnd::SetupJoy(int x, int y, int nJoy)
{
	int i;
	int nDevice;
	LONG lAxis;
	DWORD dwButton;
	CString strJoy;
	CString strText;

	ASSERT(this);
	ASSERT(m_pInput);
	ASSERT((x >= 0) && (x < m_nWidth));
	ASSERT((y >= 0) && (y < m_nHeight));
	ASSERT((nJoy >= 0) && (nJoy < 2));

	// Joy-A or Joy-B creation
	if (nJoy == 0) {
		strJoy = _T("Joy-A");
	}
	else {
		strJoy = _T("Joy-B");
	}

	// Device
	SetString(x, y, strJoy);
	SetString(x + 6, y, _T("Device"));
	nDevice = m_pInput->GetJoyDevice(nJoy);
	if (nDevice > 0) {
		strText.Format(_T("%2u"), nDevice - 1);
		SetString(x + 19, y, strText);
	}
	if (nDevice == 0) {
		SetString(x + 15, y, _T("(None)"));
	}
	if (nDevice < 0) {
		SetString(x + 14, y, _T("(Error)"));
	}
	y++;

	// Acquire
	SetString(x, y, strJoy);
	SetString(x + 6, y, _T("Acquire"));
	strText.Format(_T("%6u"), m_pInput->GetAcquireCount(nJoy + 2));
	SetString(x + 15, y, strText);
	y++;

	// Axis
	for (i=0; i<8; i++) {
		SetString(x, y, strJoy);
		strText.Format(_T("Axis%d"), i + 1);
		SetString(x + 6, y, strText);

		// Get data
		lAxis = m_pInput->GetJoyAxis(nJoy, i);

		if (lAxis >= 0x10000) {
			// Axis does not exist
			SetString(x + 16, y, _T("-----"));
		}
		else {
			// Display axis data
			strText.Format(_T("%5d"), lAxis);
			SetString(x + 16, y, strText);
		}
		y++;
	}

	// Button
	for (i=0; i<12; i++) {
		SetString(x, y, strJoy);
		strText.Format(_T("Button%d"), i + 1);
		SetString(x + 6, y, strText);

		// Get data
		dwButton = m_pInput->GetJoyButton(nJoy, i);

		if (dwButton >= 0x10000) {
			// Button does not exist
			SetString(x + 18, y, _T("---"));
		}
		else {
			if (dwButton & 0x80) {
				// On
				SetString(x + 19, y, _T("On"));
			}
			else {
				// Off
				SetString(x + 18, y, _T("Off"));
			}
		}
		y++;
	}
}

//===========================================================================
//
//	Port window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CPortWnd::CPortWnd()
{
	CFrmWnd *pFrmWnd;

	// Window parameter definition
	m_dwID = MAKEID('P', 'O', 'R', 'T');
	::GetMsg(IDS_SWND_PORT, m_strCaption);
	m_nWidth = 23;
	m_nHeight = 18;

	// Get port component
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pPort = pFrmWnd->GetPort();
	ASSERT(m_pPort);
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CPortWnd::Setup()
{
	int x;
	int y;
	CString strText;
	TCHAR szDevice[_MAX_PATH];
	DWORD dwHandle;
	BOOL bRun;
	CQueue::QUEUEINFO qi;

	ASSERT(this);

	// Initialization
	Clear();
	x = 15;
	y = 0;

	// Get COM information
	bRun = m_pPort->GetCOMInfo(szDevice, &dwHandle);

	// COM thread
	SetString(0, y, _T("COM Thread"));
	if (bRun) {
		SetString(x + 5, y, _T("Run"));
	}
	else {
		SetString(x + 4, y, _T("Stop"));
	}
	y++;

	// COM file
	SetString(0, y, _T("COM Device"));
	SetString(x, y, szDevice);
	y++;

	// COM Log
	SetString(0, y, _T("COM Log"));
	strText.Format(_T("%08X"), dwHandle);
	SetString(x, y, strText);
	y++;

	// TxGet queue information
	m_pPort->GetTxQueue(&qi);

	// Tx Read
	SetString(0, y, _T("Tx  Read"));
	strText.Format(_T("%4d"), qi.dwRead);
	SetString(x + 4, y, strText);
	y++;

	// Tx Write
	SetString(0, y, _T("Tx  Write"));
	strText.Format(_T("%4d"), qi.dwWrite);
	SetString(x + 4, y, strText);
	y++;

	// Tx Num
	SetString(0, y, _T("Tx  Num"));
	strText.Format(_T("%4d"), qi.dwNum);
	SetString(x + 4, y, strText);
	y++;

	// Tx Total
	SetString(0, y, _T("Tx  Total"));
	strText.Format(_T("%7d"), qi.dwTotalRead);
	SetString(x + 1, y, strText);
	y++;

	// RxGet queue information
	m_pPort->GetRxQueue(&qi);

	// Rx Read
	SetString(0, y, _T("Rx  Read"));
	strText.Format(_T("%4d"), qi.dwRead);
	SetString(x + 4, y, strText);
	y++;

	// Rx Write
	SetString(0, y, _T("Rx  Write"));
	strText.Format(_T("%4d"), qi.dwWrite);
	SetString(x + 4, y, strText);
	y++;

	// Rx Num
	SetString(0, y, _T("Rx  Num"));
	strText.Format(_T("%4d"), qi.dwNum);
	SetString(x + 4, y, strText);
	y++;

	// Rx Total
	SetString(0, y, _T("Rx  Total"));
	strText.Format(_T("%7d"), qi.dwTotalRead);
	SetString(x + 1, y, strText);
	y++;

	// LPTGet information
	bRun = m_pPort->GetLPTInfo(szDevice, &dwHandle);

	// LPT Thread
	SetString(0, y, _T("LPT Thread"));
	if (bRun) {
		SetString(x + 5, y, _T("Run"));
	}
	else {
		SetString(x + 4, y, _T("Stop"));
	}
	y++;

	// LPT Device
	SetString(0, y, _T("LPT Device"));
	SetString(x, y, szDevice);
	y++;

	// LPT Log
	SetString(0, y, _T("LPT Log"));
	strText.Format(_T("%08X"), dwHandle);
	SetString(x, y, strText);
	y++;

	// LPTGet queue information
	m_pPort->GetLPTQueue(&qi);

	// LPT Read
	SetString(0, y, _T("LPT Read"));
	strText.Format(_T("%4d"), qi.dwRead);
	SetString(x + 4, y, strText);
	y++;

	// LPT Write
	SetString(0, y, _T("LPT Write"));
	strText.Format(_T("%4d"), qi.dwWrite);
	SetString(x + 4, y, strText);
	y++;

	// LPT Num
	SetString(0, y, _T("LPT Num"));
	strText.Format(_T("%4d"), qi.dwNum);
	SetString(x + 4, y, strText);
	y++;

	// LPT Total
	SetString(0, y, _T("LPT Total"));
	strText.Format(_T("%7d"), qi.dwTotalRead);
	SetString(x + 1, y, strText);
	y++;
}

//===========================================================================
//
//	Bitmap window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CBitmapWnd::CBitmapWnd()
{
	CFrmWnd *pFrmWnd;

	// Window parameter definition
	m_dwID = MAKEID('B', 'M', 'A', 'P');
	::GetMsg(IDS_SWND_BITMAP, m_strCaption);
	m_nWidth = 46;
	m_nHeight = 8;

	// Get drawing window
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pView = pFrmWnd->GetView();
	ASSERT(m_pView);
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CBitmapWnd::Setup()
{
	int x;
	int y;
	CString string;
	CDrawView::DRAWINFO info;

	// Get data
	m_pView->GetDrawInfo(&info);

	// Clear
	Clear();
	x = 0;
	y = 0;

	// Bitmap
	SetString(x, y, "Bitmap Width");
	string.Format("%4d", info.nBMPWidth);
	SetString(x + 17, y, string);
	y++;
	SetString(x, y, "Render Width");
	string.Format("%4d", info.nRendWidth);
	SetString(x + 17, y, string);
	y++;
	SetString(x, y, "Render H Mul.");
	string.Format("%4d", info.nRendHMul);
	SetString(x + 17, y, string);
	y++;

	// BitBlt
	SetString(x, y, "BitBlt Width");
	string.Format("%4d", info.nWidth);
	SetString(x + 17, y, string);
	y++;
	SetString(x, y, "BitBlt X");
	string.Format("%4d", info.nLeft);
	SetString(x + 17, y, string);
	y++;
	SetString(x, y, "BitBlt Top");
	string.Format("%4d", info.nBltTop);
	SetString(x + 17, y, string);
	y++;
	SetString(x, y, "BitBlt Left");
	string.Format("%4d", info.nBltLeft);
	SetString(x + 17, y, string);
	y++;

	// Flags
	SetString(x, y, "Window Scale");
	switch (info.nScaleIndex) {
	case 0:
		SetString(x + 18, y, "1.0x");
		break;
	case 1:
		SetString(x + 18, y, "1.5x");
		break;
	case 2:
		SetString(x + 18, y, "2.0x");
		break;
	case 3:
		SetString(x + 18, y, "2.5x");
		break;
	default:
		SetString(x + 18, y, "3.0x");
		break;
	}

	// Move to right side
	x = 25;
	y = 0;

	// Bitmap
	SetString(x, y, "Bitmap Height");
	string.Format("%4d", info.nBMPHeight);
	SetString(x + 17, y, string);
	y++;
	SetString(x, y, "Render Height");
	string.Format("%4d", info.nRendHeight);
	SetString(x + 17, y, string);
	y++;
	SetString(x, y, "Render V Mul.");
	string.Format("%4d", info.nRendVMul);
	SetString(x + 17, y, string);
	y++;

	// BitBlt
	SetString(x, y, "BitBlt Height");
	string.Format("%4d", info.nHeight);
	SetString(x + 17, y, string);
	y++;
	SetString(x, y, "BitBlt Y");
	string.Format("%4d", info.nTop);
	SetString(x + 17, y, string);
	y++;
	SetString(x, y, "BitBlt Bottom");
	string.Format("%4d", info.nBltBottom + 1);
	SetString(x + 17, y, string);
	y++;
	SetString(x, y, "BitBlt Right");
	string.Format("%4d", info.nBltRight + 1);
	SetString(x + 17, y, string);
	y++;

	// Count
	SetString(x, y, "BitBlt Count");
	string.Format("%7d", info.dwDrawCount);
	SetString(x + 14, y, string);
}

//===========================================================================
//
//	MIDI driver window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CMIDIDrvWnd::CMIDIDrvWnd()
{
	CFrmWnd *pFrmWnd;

	// Window parameter definition
	m_dwID = MAKEID('M', 'D', 'R', 'V');
	::GetMsg(IDS_SWND_MIDIDRV, m_strCaption);
	m_nWidth = 38;
	m_nHeight = 11;

	// Get MIDI device
	m_pMIDI = (MIDI*)::GetVM()->SearchDevice(MAKEID('M', 'I', 'D', 'I'));
	ASSERT(m_pMIDI);

	// Get MIDI component
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);
	m_pMIDIDrv = pFrmWnd->GetMIDI();
	ASSERT(m_pMIDIDrv);
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CMIDIDrvWnd::Setup()
{
	int i;
	int y;
	MIDI::midi_t midi;
	CMIDI::MIDIINFO mi;
	DWORD dwStart;
	DWORD dwEnd;

	ASSERT(this);
	ASSERT(m_pMIDI);
	ASSERT(m_pMIDIDrv);

	// Get device work
	m_pMIDI->GetMIDI(&midi);

	// Clear
	Clear();

	// String display
	y = 1;
	for (i=0;; i++) {
		if (!DescTable[i]) {
			break;
		}

		SetString(0, y, DescTable[i]);
		y++;
	}

	// IN
	m_pMIDIDrv->GetInInfo(&mi);
	SetString(20, 0, _T("MIDI IN"));
	mi.dwBufNum = midi.recvnum;
	mi.dwBufRead = midi.recvread;
	mi.dwBufWrite = midi.recvwrite;
	SetupInfo(19, 1, &mi);

	// IN (exclusive counter)
	dwStart = m_pMIDI->GetExCount(2);
	dwEnd = m_pMIDI->GetExCount(3);
	SetupExCnt(19, 1, dwStart, dwEnd);

	// OUT
	m_pMIDIDrv->GetOutInfo(&mi);
	SetString(30, 0, _T("MIDI OUT"));
	mi.dwBufNum = midi.transnum;
	mi.dwBufRead = midi.transread;
	mi.dwBufWrite = midi.transwrite;
	SetupInfo(30, 1, &mi);

	// OUT (exclusive counter)
	dwStart = m_pMIDI->GetExCount(0);
	dwEnd = m_pMIDI->GetExCount(1);
	SetupExCnt(30, 1, dwStart, dwEnd);
}

//---------------------------------------------------------------------------
//
	// Setup (sub)
//
//---------------------------------------------------------------------------
void FASTCALL CMIDIDrvWnd::SetupInfo(int x, int y, CMIDI::LPMIDIINFO pInfo)
{
	CString strText;

	ASSERT(this);
	ASSERT((x >= 0) && (x < m_nWidth));
	ASSERT((y >= 0) && (y < m_nHeight));
	ASSERT(pInfo);

	// Device total
	strText.Format(_T("%8d"), pInfo->dwDevices);
	SetString(x, y, strText);
	y++;

	// Current device
	strText.Format(_T("%8d"), pInfo->dwDevice);
	SetString(x, y, strText);
	y++;

	// Short message
	strText.Format(_T("%8d"), pInfo->dwShort);
	SetString(x, y, strText);
	y++;

	// Long message
	strText.Format(_T("%8d"), pInfo->dwLong);
	SetString(x, y, strText);
	y++;

	// Header release
	strText.Format(_T("%8d"), pInfo->dwUnprepare);
	SetString(x, y, strText);
	y++;

	// Buffer available
	strText.Format(_T("%8d"), pInfo->dwBufNum);
	SetString(x, y, strText);
	y++;

	// Buffer Read
	strText.Format(_T("%8d"), pInfo->dwBufRead);
	SetString(x, y, strText);
	y++;

	// Buffer Write
	strText.Format(_T("%8d"), pInfo->dwBufWrite);
	SetString(x, y, strText);
}

//---------------------------------------------------------------------------
//
	// Setup (exclusive counter)
//
//---------------------------------------------------------------------------
void FASTCALL CMIDIDrvWnd::SetupExCnt(int x, int y, DWORD dwStart, DWORD dwEnd)
{
	CString strText;

	ASSERT(this);
	ASSERT((x >= 0) && (x < m_nWidth));
	ASSERT((y >= 0) && (y < m_nHeight));

	// y adjustment
	y += 8;

	// Exclusive start (F0) count
	strText.Format(_T("%8d"), dwStart);
	SetString(x, y, strText);
	y++;

	// Exclusive end (F7) count
	strText.Format(_T("%8d"), dwEnd);
	SetString(x, y, strText);
	y++;
}

//---------------------------------------------------------------------------
//
//	String table
//
//---------------------------------------------------------------------------
LPCTSTR CMIDIDrvWnd::DescTable[] = {
	_T("Num of Devices"),
	_T("Current Device"),
	_T("Short     Message"),
	_T("Exclusive Message"),
	_T("Unprepere Header"),
	_T("Buffer Num"),
	_T("Buffer Read"),
	_T("Buffer Write"),
	_T("Exclusive Start"),
	_T("Exclusive End"),
	NULL
};

//===========================================================================
//
//	Keyboard display window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CKeyDispWnd::CKeyDispWnd()
{
	// Logical work
	m_nMode = 0;
	memset(m_nKey, 0, sizeof(m_nKey));
	memset(m_bKey, 0, sizeof(m_bKey));
	m_nPoint = 0;

	// Display work
	m_pCG = NULL;
	m_pBits = NULL;
	m_hBitmap = NULL;
	m_nBMPWidth = 0;
	m_nBMPHeight = 0;
	m_nBMPMul = 0;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CKeyDispWnd, CWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_GETDLGCODE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CKeyDispWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	Memory *pMemory;
	CRect rectWnd;

	// Base class
	if (CWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Disable IME
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Get CGROM
	pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(pMemory);
	m_pCG = pMemory->GetCG();
	ASSERT(m_pCG);

	// Resize
	rectWnd.left = 0;
	rectWnd.top = 0;
	rectWnd.right = 616;
	rectWnd.bottom = 140;
	CalcWindowRect(&rectWnd);
	SetWindowPos(&wndTop, 0, 0, rectWnd.Width(), rectWnd.Height(), SWP_NOMOVE);

	// Bitmap作成
	SetupBitmap();

	return 0;
}

//---------------------------------------------------------------------------
//
//	ウィンドウ削除
//
//---------------------------------------------------------------------------
void CKeyDispWnd::OnDestroy()
{
	// Release bitmap
	if (m_hBitmap) {
		::DeleteObject(m_hBitmap);
		m_hBitmap = NULL;
		m_pBits = NULL;
	}

	// Base class
	CWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	ウィンドウ削除完了
//
//---------------------------------------------------------------------------
void CKeyDispWnd::PostNcDestroy()
{
	ASSERT(this);
	ASSERT(!m_hBitmap);
	ASSERT(!m_pBits);

	// Delete interface elements
	delete this;
}

//---------------------------------------------------------------------------
//
//	ウィンドウサイズ変更
//
//---------------------------------------------------------------------------
void CKeyDispWnd::OnSize(UINT nType, int cx, int cy)
{
	// Base classを先に実行
	CWnd::OnSize(nType, cx, cy);

	// Bitmap作成
	SetupBitmap();
}

//---------------------------------------------------------------------------
//
	// Create bitmap
//
//---------------------------------------------------------------------------
void FASTCALL CKeyDispWnd::SetupBitmap()
{
	int i;
	BITMAPINFOHEADER bmi;
	CRect rectClient;
	CClientDC dc(this);

	ASSERT(this);

	// If minimized, don't create bitmap
	if (IsIconic()) {
		return;
	}

	// Get client rectangle
	GetClientRect(&rectClient);
	ASSERT((rectClient.Width() != 0) || (rectClient.Height() != 0));

	// If bitmap exists
	if (m_hBitmap) {
		// If dimensions are the same
		if (m_nBMPWidth == (UINT)rectClient.Width()) {
			if (m_nBMPHeight == (UINT)rectClient.Height()) {
				// Do nothing
				return;
			}
		}
	}

	// VMロック
	::LockVM();

	// Release bitmap
	if (m_hBitmap) {
		::DeleteObject(m_hBitmap);
		m_hBitmap = NULL;
		m_pBits = NULL;
	}

	// Bitmap情報記憶
	m_nBMPWidth = rectClient.Width();
	m_nBMPHeight = rectClient.Height();
	m_nBMPMul = (m_nBMPWidth + 3) >> 2;
	m_nBMPMul <<= 2;

	// Bitmap情報作成(256色パレットつきビットマップ)
	memset(&bmi, 0, sizeof(bmi));
	bmi.biSize = sizeof(BITMAPINFOHEADER);
	bmi.biWidth = m_nBMPWidth;
	bmi.biHeight = m_nBMPHeight;
	bmi.biHeight = -(bmi.biHeight);
	bmi.biPlanes = 1;
	bmi.biBitCount = 8;
	bmi.biCompression = BI_RGB;
	bmi.biSizeImage = m_nBMPMul * m_nBMPHeight;

	// DC取得、DIBセクション作成
	m_hBitmap = ::CreateDIBSection(dc.m_hDC, (BITMAPINFO*)&bmi, DIB_RGB_COLORS,
									(void**)&m_pBits, NULL, 0);
	ASSERT(m_hBitmap);
	ASSERT(m_pBits);

	// BitmapClear
	if (m_hBitmap) {
		memset(m_pBits, 0, bmi.biSizeImage);
	}

	// Display内容は不定
	for (i=0; i<0x80; i++) {
		m_nKey[i] = 0;
	}

	// VMアンロック
	::UnlockVM();

	// Redrawを起こす
	Invalidate(FALSE);
}

//---------------------------------------------------------------------------
//
//	ウィンドウ背景描画
//
//---------------------------------------------------------------------------
BOOL CKeyDispWnd::OnEraseBkgnd(CDC* /*pDC*/)
{
	ASSERT(this);

	// Return TRUE without doing anything
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ウィンドウRedraw
//
//---------------------------------------------------------------------------
void CKeyDispWnd::OnPaint()
{
	CPaintDC dc(this);
	int i;

	ASSERT(this);

	// Reset all keys to undefined state
	for (i=0; i<0x80; i++) {
		m_nKey[i] = 0;
	}

	// Draw
	OnDraw(&dc);
}

//---------------------------------------------------------------------------
//
//	左Button押下
//
//---------------------------------------------------------------------------
void CKeyDispWnd::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
	CWnd *pWnd;
	int nKey;

	// Pointからキーインデックス取得
	nKey = PtInKey(point);

	// If not in range, do nothing
	if (nKey == 0) {
		return;
	}

	// Send message to parent window
	pWnd = GetParent();
	if (pWnd) {
		pWnd->PostMessage(WM_APP, (WPARAM)nKey, (LPARAM)WM_LBUTTONDOWN);
	}
}

//---------------------------------------------------------------------------
//
//	左Button離した
//
//---------------------------------------------------------------------------
void CKeyDispWnd::OnLButtonUp(UINT /*nFlags*/, CPoint point)
{
	CWnd *pWnd;
	int nKey;

	// Pointからキーインデックス取得
	nKey = PtInKey(point);

	// Send message to parent window
	pWnd = GetParent();
	if (pWnd) {
		pWnd->PostMessage(WM_APP, (WPARAM)nKey, (LPARAM)WM_LBUTTONUP);
	}
}

//---------------------------------------------------------------------------
//
//	右Button押下
//
//---------------------------------------------------------------------------
void CKeyDispWnd::OnRButtonDown(UINT /*nFlags*/, CPoint point)
{
	CWnd *pWnd;
	int nKey;

	// Pointからキーインデックス取得
	nKey = PtInKey(point);

	// If not in range, do nothing
	if (nKey == 0) {
		return;
	}

	// Send message to parent window
	pWnd = GetParent();
	if (pWnd) {
		pWnd->PostMessage(WM_APP, (WPARAM)nKey, (LPARAM)WM_RBUTTONDOWN);
	}
}

//---------------------------------------------------------------------------
//
//	右Button離した
//
//---------------------------------------------------------------------------
void CKeyDispWnd::OnRButtonUp(UINT /*nFlags*/, CPoint point)
{
	CWnd *pWnd;
	int nKey;

	// Pointからキーインデックス取得
	nKey = PtInKey(point);

	// Send message to parent window
	pWnd = GetParent();
	if (pWnd) {
		pWnd->PostMessage(WM_APP, (WPARAM)nKey, (LPARAM)WM_RBUTTONUP);
	}
}

//---------------------------------------------------------------------------
//
//	Mouse move
//
//---------------------------------------------------------------------------
void CKeyDispWnd::OnMouseMove(UINT /*nFlags*/, CPoint point)
{
	CWnd *pWnd;
	int nKey;

	// Pointからキーインデックス取得
	nKey = PtInKey(point);

	// If current matches, OK
	if (m_nPoint == nKey) {
		return;
	}

	// Updateして
	m_nPoint = nKey;

	// Send message to parent window
	pWnd = GetParent();
	if (pWnd) {
		pWnd->PostMessage(WM_APP, (WPARAM)nKey, (LPARAM)WM_MOUSEMOVE);
	}
}

//---------------------------------------------------------------------------
//
//	ダイアログコード取得
//	※ダイアログ内のチャイルドウィンドウとして使われた場合のみ
//
//---------------------------------------------------------------------------
UINT CKeyDispWnd::OnGetDlgCode()
{
	return DLGC_WANTMESSAGE;
}

//---------------------------------------------------------------------------
//
//	矩形内Get key
//
//---------------------------------------------------------------------------
int FASTCALL CKeyDispWnd::PtInKey(CPoint& point)
{
	int i;
	int nKey;
	CRect rect;

	ASSERT(this);

	// Initialization
	nKey = -1;

	// Check if inside rectangle
	for (i=0; i<=0x74; i++) {
		rect = RectTable[i];
		if (rect.top != 0) {
			// Move inside
			rect.top++;
			rect.left++;
			rect.right--;
			rect.bottom--;

			// Check
			if (rect.PtInRect(point)) {
				nKey = i;
				break;
			}
		}
	}

	// If not in range
	if (nKey < 0) {
		// CRの判定
		rect.top = RectTable[0x1c].top + 1;
		rect.bottom = RectTable[0x29].bottom - 1;
		rect.left = RectTable[0x1c].right + 1;
		rect.right = RectTable[0x0f].right - 1;
		if (rect.PtInRect(point)) {
			nKey = 0x1d;
		}
		else {
			// Others are 0
			nKey = 0;
		}
	}

	// SHIFTキー特殊処理
	if (nKey == 0x74) {
		nKey = 0x70;
	}

	return nKey;
}

//---------------------------------------------------------------------------
//
//	シフトモード指定
//
//	b7	SHIFT
//	b6	CTRL
//	b5	(未使用)
//	b4	(未使用)
//	b3	CAPS
//	b2	コード入力
//	b1	ローマ字
//	b0	かな
//
//---------------------------------------------------------------------------
void FASTCALL CKeyDispWnd::SetShiftMode(UINT nMode)
{
	int i;

	ASSERT(this);
	ASSERT(nMode < 0x100);

	// Mask reference bits
	nMode &= 0x89;

	// If mode hasn't changed, do nothing
	if (m_nMode == nMode) {
		return;
	}

	// Update mode
	m_nMode = nMode;

	// Reset all keys to undefined (without drawing)
	for (i=0; i<0x80; i++) {
		m_nKey[i] = 0;
	}
}

//---------------------------------------------------------------------------
//
//	リフレッシュ
//
//---------------------------------------------------------------------------
void FASTCALL CKeyDispWnd::Refresh(const BOOL *pKey)
{
	int i;
	UINT nCode;
	BOOL bFlag;
	CClientDC dc(this);

	ASSERT(this);
	ASSERT(pKey);

	// Change flag initialization
	bFlag = FALSE;

	// Loop
	for (i=0; i<0x80; i++) {
		// First copy
		m_bKey[i] = pKey[i];

		// Convert to number and compare
		if (m_bKey[i]) {
			nCode = 2;
		}
		else {
			nCode = 1;
		}

		// If different, set flag and break
		if (m_nKey[i] != nCode) {
			bFlag = TRUE;
			break;
		}
	}

	// If flag is down, stop here
	if (!bFlag) {
		return;
	}

	// Draw
	OnDraw(&dc);
}

//---------------------------------------------------------------------------
//
//  キーBatch setting
//
//---------------------------------------------------------------------------
void FASTCALL CKeyDispWnd::SetKey(const BOOL* pKey)
{
	ASSERT(this);
	ASSERT(pKey);

	// Copy key information
	memcpy(m_bKey, pKey, sizeof(m_bKey));
}

//---------------------------------------------------------------------------
//
//	ウィンドウ描画
//
//---------------------------------------------------------------------------
void FASTCALL CKeyDispWnd::OnDraw(CDC *pDC)
{
	HBITMAP hBitmap;
	CRect rect;
	CDC mDC;
	int i;
	UINT nCode;

	ASSERT(this);

	// Bitmapができていなければ、何もしない
	if (!m_hBitmap) {
		return;
	}

	// Physical key loop
	for (i=0; i<0x80; i++) {
		// Convert to number
		if (m_bKey[i]) {
			nCode = 2;
		}
		else {
			nCode = 1;
		}

		// Compare
		if (m_nKey[i] != nCode) {
			// Different, so save and draw
			m_nKey[i] = nCode;

			// Draw
			if (nCode == 2) {
				DrawKey(i, TRUE);
			}
			else {
				DrawKey(i, FALSE);
			}
		}
	}

	// Create memory DC
	ASSERT(m_pBits);
	VERIFY(mDC.CreateCompatibleDC(pDC));
	GetClientRect(&rect);

	// Bitmapセレクト
	hBitmap = (HBITMAP)::SelectObject(mDC.m_hDC, m_hBitmap);
	if (hBitmap) {
		// DIBカラーテーブル設定
		::SetDIBColorTable(mDC.m_hDC, 0, 16, PalTable);

		// BitBlt
		::BitBlt(pDC->m_hDC, 0, 0, rect.Width(), rect.Height(),
					mDC.m_hDC, 0, 0, SRCCOPY);

		// Restore
		::SelectObject(mDC.m_hDC, hBitmap);
	}

	// Delete memory DC
	mDC.DeleteDC();
}

//---------------------------------------------------------------------------
//
//	キー文字列取得
//
//---------------------------------------------------------------------------
LPCTSTR FASTCALL CKeyDispWnd::GetKeyString(int nKey)
{
	LPCTSTR lpszString;
	UINT nModeXor;

	ASSERT(this);
	ASSERT((nKey >= 0) && (nKey < 0x80));

	// Overflow check
	if (nKey > 0x74) {
		return NULL;
	}

	// First assign base value
	lpszString = NormalTable[nKey];

	// Kana check
	if (m_nMode & 1) {
		// Kana key range
		if ((nKey >= 2) && (nKey <= 0x34)) {
			// SHIFTにより分ける
			if (m_nMode & 0x80) {
				return KanaShiftTable[nKey - 2];
			}
			else {
				return KanaTable[nKey - 2];
			}
		}
	}

	// SHIFTチェック
	if (m_nMode & 0x80) {
		if ((nKey >= 2) && (nKey <= 0x0e)) {
			return MarkTable[nKey - 2];
		}
	}

	// Create alphanumeric mode
	nModeXor = m_nMode >> 4;
	nModeXor ^= m_nMode;
	if ((nModeXor & 0x08) == 0) {
		// For lowercase alphanumeric, swap
		if ((nKey >= 0x11) && (nKey <= 0x1a)) {
			lpszString = AnotherTable[nKey - 0x11];
		}
		if ((nKey >= 0x1e) && (nKey <= 0x26)) {
			lpszString = AnotherTable[nKey - 0x11];
		}
		if ((nKey >= 0x2a) && (nKey <= 0x30)) {
			lpszString = AnotherTable[nKey - 0x11];
		}
	}

	// Symbols are swapped with SHIFT
	if (m_nMode & 0x80) {
		if ((nKey >= 0x1b) && (nKey <= 0x1d)) {
			lpszString = AnotherTable[nKey - 0x11];
		}
		if ((nKey >= 0x27) && (nKey <= 0x29)) {
			lpszString = AnotherTable[nKey - 0x11];
		}
		if ((nKey >= 0x31) && (nKey <= 0x34)) {
			lpszString = AnotherTable[nKey - 0x11];
		}
	}

	return lpszString;
}

//---------------------------------------------------------------------------
//
//	キー描画
//
//---------------------------------------------------------------------------
void FASTCALL CKeyDispWnd::DrawKey(int nKey, BOOL bDown)
{
	int x;
	int y;
	int i;
	DWORD dwChar;
	CRect rectKey;
	const char *lpszKey;
	int nColor;
	int nWidth;

	ASSERT(this);
	ASSERT((nKey >= 0) && (nKey < 0x80));

	// Get string
	lpszKey = GetKeyString(nKey);
	if (!lpszKey) {
		// CRの特例を入れる
		if (nKey == 0x1d) {
			// CR
			rectKey.left = RectTable[0x1c].right - 1;
			rectKey.top = RectTable[0x1c].top;
			rectKey.right = RectTable[0x0f].right;
			rectKey.bottom = RectTable[0x29].bottom;

			// CR描画
			if (bDown) {
				DrawCRBox(10, 11, rectKey);
				DrawCRChar(rectKey.left, rectKey.top, 12);
			}
			else {
				DrawCRBox(10, 8, rectKey);
				DrawCRChar(rectKey.left, rectKey.top, 9);
			}
		}
		return;
	}

	// Get rectangle
	ASSERT(nKey <= 0x74);
	rectKey = RectTable[nKey];

	// Draw rectangle
	if (bDown) {
		DrawBox(10, 11, rectKey);
		nColor = 12;
	}
	else {
		DrawBox(10, 8, rectKey);
		nColor = 9;
	}

	// Count characters and convert to dots
	nWidth = (int)strlen(lpszKey);
	nWidth <<= 3;

	// Centering
	x = 0;
	if (nWidth < rectKey.Width()) {
		x = (rectKey.Width() >> 1);
		x -= (nWidth >> 1);
	}
	y = 0;
	if (rectKey.Height() > 16) {
		y = (rectKey.Height() >> 1);
		y -= (15 >> 1);
	}

	// Draw character
	for (i=0;; i++) {
		// End check
		if (lpszKey[i] == '\0') {
			break;
		}

		// 1文字取得
		dwChar = (DWORD)(lpszKey[i] & 0xff);

		// Half-width check
		if (dwChar < 0x80) {
			DrawChar(rectKey.left + x, rectKey.top + y + 1, nColor, dwChar);
			x += 8;
			continue;
		}
		if ((dwChar >= 0xa0) && (dwChar < 0xe0)) {
			DrawChar(rectKey.left + x, rectKey.top + y + 1, nColor, dwChar);
			x += 8;
			continue;
		}

		// Full-width
		dwChar <<= 8;
		dwChar |= (DWORD)(lpszKey[i + 1] & 0xff);
		i++;

		DrawChar(rectKey.left + x, rectKey.top + y + 1, nColor, dwChar);
		x += 16;
	}
}

//---------------------------------------------------------------------------
//
//	ボックス描画
//
//---------------------------------------------------------------------------
void FASTCALL CKeyDispWnd::DrawBox(int nColorOut, int nColorIn, RECT& rect)
{
	int x;
	int y;
	BYTE *p;

	ASSERT(this);
	ASSERT(m_hBitmap);
	ASSERT(m_pBits);
	ASSERT((nColorOut >= 0) && (nColorOut < 0x100));
	ASSERT((nColorIn >= 0) && (nColorIn < 0x100));
	ASSERT(rect.top >= 0);
	ASSERT(rect.top <= rect.bottom);
	ASSERT(rect.left >= 0);
	ASSERT(rect.left <= rect.right);

	// Calculate initial position
	p = m_pBits;
	p += (m_nBMPMul * rect.top);
	p += rect.left;

	// Top (leave 1 dot on left and right)
	memset(p + 1, nColorOut, (rect.right - rect.left - 2));
	p += m_nBMPMul;

	// Prepare
	x = rect.right - rect.left;
	x -= 2;
	rect.bottom -= 2;

	// yLoop
	for (y=rect.top; y<rect.bottom; y++) {
		// Per line
		p[0] = (BYTE)nColorOut;
		memset(p + 1, nColorIn, x);
		p[x + 1] = (BYTE)nColorOut;

		p += m_nBMPMul;
	}

	// Last (leave 1 dot on left and right)
	memset(p + 1, nColorOut, (rect.right - rect.left - 2));
}

//---------------------------------------------------------------------------
//
//	CRボックス描画
//
//---------------------------------------------------------------------------
void FASTCALL CKeyDispWnd::DrawCRBox(int nColorOut, int nColorIn, RECT& rect)
{
	int x;
	int y;
	BYTE *p;

	ASSERT(this);
	ASSERT(m_pBits);
	ASSERT(m_hBitmap);
	ASSERT(m_pBits);
	ASSERT((nColorOut >= 0) && (nColorOut < 0x100));
	ASSERT((nColorIn >= 0) && (nColorIn < 0x100));
	ASSERT(rect.top >= 0);
	ASSERT(rect.top <= rect.bottom);
	ASSERT(rect.left >= 0);
	ASSERT(rect.left <= rect.right);

	// Calculate initial position
	p = m_pBits;
	p += (m_nBMPMul * rect.top);
	p += rect.left;

	// Top (leave 1 dot on left and right)
	memset(p + 1, nColorOut, (rect.right - rect.left - 2));
	p += m_nBMPMul;

	// Prepare
	x = rect.right - rect.left;
	x -= 2;
	rect.bottom -= 2;

	// yLoop
	for (y=rect.top; y<rect.bottom; y++) {
		if ((y - rect.top) < 19) {
			// Normal
			p[0] = (BYTE)nColorOut;
			memset(p + 1, nColorIn, x);
			p[x + 1] = (BYTE)nColorOut;
		}
		else {
			// Bottom side
			p[10] = (BYTE)nColorOut;
			memset(p + 11, nColorIn, 41);
			p[x + 1] = (BYTE)nColorOut;
		}

		p += m_nBMPMul;
	}

	// Skip bottom (leave to 0x74:SHIFT)
}

//---------------------------------------------------------------------------
//
//	キャラクタ描画
//
//---------------------------------------------------------------------------
void FASTCALL CKeyDispWnd::DrawChar(int x, int y, int nColor, DWORD dwChar)
{
	const BYTE *pCG;
	BYTE *pBits;
	int i;
	int j;
	int nWidth;
	int nOffset;
	DWORD dwPattern;

	ASSERT(this);
	ASSERT(m_hBitmap);
	ASSERT(m_pBits);
	ASSERT((x >= 0) && (x < (int)m_nBMPWidth));
	ASSERT((y >= 0) && (y < (int)m_nBMPHeight));
	ASSERT((nColor >= 0) && (nColor < 0x100));
	ASSERT(dwChar <= 0x10000);
	ASSERT(m_pCG);

	// Calculate font address
	if (dwChar < 0x100) {
		// Half-width
		nWidth = 1;
		pCG = m_pCG + 0x3a800;
		pCG += (dwChar << 4);
	}
	else {
		// Full-width
		nWidth = 2;
		pCG = m_pCG;
		pCG += CalcCGAddr(dwChar);
	}

	// Calculate bit position
	pBits = m_pBits;
	pBits += (m_nBMPMul * y);
	pBits += x;

	// Calculate offset
	nOffset = m_nBMPMul - (nWidth << 3);

	if (nWidth == 1) {
		// Half-width
		for (i=0; i<8; i++) {
			// +1
			dwPattern = (DWORD)pCG[1];
			for (j=0; j<8; j++) {
				if (dwPattern & 0x80) {
					*pBits = (BYTE)nColor;
				}
				pBits++;
				dwPattern <<= 1;
			}
			pBits += nOffset;

			// -1
			dwPattern = (DWORD)pCG[0];
			for (j=0; j<8; j++) {
				if (dwPattern & 0x80) {
					*pBits = (BYTE)nColor;
				}
				pBits++;
				dwPattern <<= 1;
			}
			pBits += nOffset;

			// +2
			pCG += 2;
		}
	}
	else {
		// Full-width
		for (i=0; i<16; i++) {
			// +1
			dwPattern = (DWORD)pCG[1];
			for (j=0; j<8; j++) {
				if (dwPattern & 0x80) {
					*pBits = (BYTE)nColor;
				}
				pBits++;
				dwPattern <<= 1;
			}

			// -1
			dwPattern = (DWORD)pCG[0];
			for (j=0; j<8; j++) {
				if (dwPattern & 0x80) {
					*pBits = (BYTE)nColor;
				}
				pBits++;
				dwPattern <<= 1;
			}

			// +2
			pCG += 2;
			pBits += nOffset;
		}
	}
}

//---------------------------------------------------------------------------
//
//	CRキャラクタ描画
//
//---------------------------------------------------------------------------
void FASTCALL CKeyDispWnd::DrawCRChar(int x, int y, int nColor)
{
	BYTE *pBits;
	int i;

	ASSERT(this);
	ASSERT(m_pBits);
	ASSERT((x >= 0) && (x < (int)m_nBMPWidth));
	ASSERT((y >= 0) && (y < (int)m_nBMPHeight));
	ASSERT((nColor >= 0) && (nColor < 0x100));

	// Vertical side
	pBits = m_pBits;
	pBits += ((y + 7) * m_nBMPMul);
	pBits += (x + 34);
	for (i=0; i<24; i++) {
		if (i == 0) {
			memset(pBits, nColor, 5);
		}
		else {
			pBits[4] = (BYTE)nColor;
			if (i < 21) {
				pBits[0] = (BYTE)nColor;
			}
		}
		pBits += m_nBMPMul;
	}

	// Horizontal side
	pBits = m_pBits;
	pBits += ((y + 27) * m_nBMPMul);
	pBits += (x + 27);
	memset(pBits, (BYTE)nColor, 7);
	pBits += (m_nBMPMul * 3);
	memset(pBits, (BYTE)nColor, 11);

	// Arrow
	pBits = m_pBits;
	pBits += ((y + 28) * m_nBMPMul);
	pBits += (x + 20);
	pBits[m_nBMPMul * 0 + 0] = (BYTE)nColor;
	pBits[m_nBMPMul * 1 + 0] = (BYTE)nColor;
	pBits[m_nBMPMul * -1 + 1] = (BYTE)nColor;
	pBits[m_nBMPMul * -1 + 2] = (BYTE)nColor;
	pBits[m_nBMPMul * 2 + 1] = (BYTE)nColor;
	pBits[m_nBMPMul * 2 + 2] = (BYTE)nColor;
	pBits[m_nBMPMul * -2 + 3] = (BYTE)nColor;
	pBits[m_nBMPMul * -2 + 4] = (BYTE)nColor;
	pBits[m_nBMPMul * 3 + 3] = (BYTE)nColor;
	pBits[m_nBMPMul * 3 + 4] = (BYTE)nColor;
	pBits[m_nBMPMul * -3 + 5] = (BYTE)nColor;
	pBits[m_nBMPMul * -3 + 6] = (BYTE)nColor;
	pBits[m_nBMPMul * -2 + 6] = (BYTE)nColor;
	pBits[m_nBMPMul * -1 + 6] = (BYTE)nColor;
	pBits[m_nBMPMul * 4 + 5] = (BYTE)nColor;
	pBits[m_nBMPMul * 4 + 6] = (BYTE)nColor;
	pBits[m_nBMPMul * 3 + 6] = (BYTE)nColor;
	pBits[m_nBMPMul * 2 + 6] = (BYTE)nColor;
}

//---------------------------------------------------------------------------
//
//	全角CGROMアドレス算出
//
//---------------------------------------------------------------------------
int FASTCALL CKeyDispWnd::CalcCGAddr(DWORD dwChar)
{
	DWORD dwLow;

	ASSERT(this);
	ASSERT(m_pCG);
	ASSERT((dwChar >= 0x8000) && (dwChar < 0x10000));

	// ShiftJIS to JIS conversion (1)
	dwLow = (dwChar & 0xff);
	dwChar &= 0x3f00;
	dwChar <<= 1;
	dwChar += 0x1f00;

	// ShiftJIS to JIS conversion (2)
	if (dwLow < 0x7f) {
		dwLow -= 0x1f;
	}
	if (dwLow >= 0x9f) {
		dwChar += 0x0100;
		dwLow -= 0x7e;
	}
	else {
		if ((dwLow >= 0x7f) && (dwLow < 0x9f)) {
			dwLow -= 0x20;
		}
	}

	// Calculate address (upper)
	dwChar >>= 8;
	if (dwChar >= 0x30) {
		dwChar -= 7;
	}
	ASSERT(dwChar >= 0x21);
	dwChar -= 0x21;
	dwChar *= 0x5e;

	// Calculate address (lower)
	ASSERT(dwLow >= 0x21);
	dwLow -= 0x21;

	// Calculate address (combined)
	dwChar += dwLow;
	dwChar <<= 5;

	return dwChar;
}

//---------------------------------------------------------------------------
//
//	パレットテーブル
//
//---------------------------------------------------------------------------
RGBQUAD CKeyDispWnd::PalTable[0x10] = {
	{ 0x50, 0x50, 0x50, 0x00 },			//  0:背景色
	{ 0xff, 0x00, 0x00, 0x00 },			//  1:青
	{ 0x00, 0x00, 0xff, 0x00 },			//  2:赤
	{ 0xff, 0x00, 0xff, 0x00 },			//  3:紫
	{ 0x00, 0xff, 0x00, 0x00 },			//  4:緑
	{ 0xff, 0xff, 0x00, 0x00 },			//  5:水色
	{ 0x00, 0xff, 0xff, 0x00 },			//  6:黄
	{ 0xff, 0xff, 0xff, 0x00 },			//  7:白

	{ 0xd0, 0xd0, 0xd0, 0x00 },			//  8:キー内部色
	{ 0x08, 0x08, 0x08, 0x00 },			//  9:キー文字色
	{ 0x00, 0x00, 0x00, 0x00 },			// 10:キー外周色
	{ 0x1f, 0x1f, 0xd0, 0x00 },			// 11:キー内部色(押下)
	{ 0xf8, 0xf8, 0xf8, 0x00 },			// 12:キー文字色(押下)
	{ 0x00, 0x00, 0x00, 0x00 },			// 13:未使用
	{ 0x00, 0x00, 0x00, 0x00 },			// 14:未使用
	{ 0x00, 0x00, 0x00, 0x00 }			// 15:未使用
};

//---------------------------------------------------------------------------
//
//	位置テーブル
//
//---------------------------------------------------------------------------
const RECT CKeyDispWnd::RectTable[0x75] = {
	{   0,   0,   0,   0 },
	{   3,  30,  33,  51 },
	{  32,  30,  55,  51 },
	{  54,  30,  77,  51 },
	{  76,  30,  99,  51 },
	{  98,  30, 121,  51 },
	{ 120,  30, 143,  51 },
	{ 142,  30, 165,  51 },
	{ 164,  30, 187,  51 },
	{ 186,  30, 209,  51 },
	{ 208,  30, 231,  51 },
	{ 230,  30, 253,  51 },
	{ 252,  30, 275,  51 },
	{ 274,  30, 297,  51 },
	{ 296,  30, 319,  51 },
	{ 318,  30, 356,  51 },
	{   3,  50,  41,  71 },
	{  40,  50,  63,  71 },
	{  62,  50,  85,  71 },
	{  84,  50, 107,  71 },
	{ 106,  50, 129,  71 },
	{ 128,  50, 151,  71 },
	{ 150,  50, 173,  71 },
	{ 172,  50, 195,  71 },
	{ 194,  50, 217,  71 },
	{ 216,  50, 239,  71 },
	{ 238,  50, 261,  71 },
	{ 260,  50, 283,  71 },
	{ 282,  50, 305,  71 },
	{   0,   0,   0,   0 },
	{  50,  70,  73,  91 },
	{  72,  70,  95,  91 },
	{  94,  70, 117,  91 },
	{ 116,  70, 139,  91 },
	{ 138,  70, 161,  91 },
	{ 160,  70, 183,  91 },
	{ 182,  70, 205,  91 },
	{ 204,  70, 227,  91 },
	{ 226,  70, 249,  91 },
	{ 248,  70, 271,  91 },
	{ 270,  70, 293,  91 },
	{ 292,  70, 315,  91 },
	{  58,  90,  81, 111 },
	{  80,  90, 103, 111 },
	{ 102,  90, 125, 111 },
	{ 124,  90, 147, 111 },
	{ 146,  90, 169, 111 },
	{ 168,  90, 191, 111 },
	{ 190,  90, 213, 111 },
	{ 212,  90, 235, 111 },
	{ 234,  90, 257, 111 },
	{ 256,  90, 279, 111 },
	{ 278,  90, 301, 111 },
	{ 125, 110, 189, 131 },
	{ 361,  30, 397,  51 },
	{ 431,  30, 467,  51 },
	{ 361,  50, 397,  71 },
	{ 396,  50, 432,  71 },
	{ 431,  50, 467,  71 },
	{ 361,  70, 397, 111 },
	{ 396,  70, 432,  91 },
	{ 431,  70, 467, 111 },
	{ 396,  90, 432, 111 },
	{ 472,  30, 508,  51 },
	{ 507,  30, 543,  51 },
	{ 542,  30, 578,  51 },
	{ 577,  30, 613,  51 },
	{ 472,  50, 508,  71 },
	{ 507,  50, 543,  71 },
	{ 542,  50, 578,  71 },
	{ 577,  50, 613,  71 },
	{ 472,  70, 508,  91 },
	{ 507,  70, 543,  91 },
	{ 542,  70, 578,  91 },
	{ 577,  70, 613,  91 },
	{ 472,  90, 508, 111 },
	{ 507,  90, 543, 111 },
	{ 542,  90, 578, 111 },
	{ 577,  90, 613, 131 },
	{ 472, 110, 508, 131 },
	{ 507, 110, 543, 131 },
	{ 542, 110, 578, 131 },
	{ 507,   6, 543,  27 },
	{ 542,   6, 578,  27 },
	{ 577,   6, 613,  27 },
	{  68, 110,  98, 131 },
	{  97, 110, 127, 131 },
	{ 188, 110, 218, 131 },
	{ 217, 110, 247, 131 },
	{ 246, 110, 276, 131 },
	{ 361,   6, 397,  27 },
	{ 396,   6, 432,  27 },
	{ 431,   6, 467,  27 },
	{ 472,   6, 508,  27 },
	{ 396,  30, 432,  51 },
	{  33, 110,  69, 131 },
	{ 274, 110, 310, 131 },
	{   3,   6,  33,  27 },
	{  33,   6,  63,  27 },
	{  65,   6,  95,  27 },
	{  94,   6, 124,  27 },
	{ 123,   6, 153,  27 },
	{ 152,   6, 182,  27 },
	{ 181,   6, 211,  27 },
	{ 210,   6, 240,  27 },
	{ 239,   6, 269,  27 },
	{ 268,   6, 298,  27 },
	{ 297,   6, 327,  27 },
	{ 326,   6, 356,  27 },
	{   0,   0,   0,   0 },
	{   0,   0,   0,   0 },
	{   0,   0,   0,   0 },
	{   3,  90,  59, 111 },
	{   3,  70,  51,  91 },
	{ 361, 110, 414, 131 },
	{ 413, 110, 467, 131 },
	{ 300,  90, 356, 111 }
};

//---------------------------------------------------------------------------
//
//	文字列テーブル(通常)
//
//---------------------------------------------------------------------------
LPCTSTR CKeyDispWnd::NormalTable[] = {
	NULL,
	_T("ESC"),
	_T("1"),
	_T("2"),
	_T("3"),
	_T("4"),
	_T("5"),
	_T("6"),
	_T("7"),
	_T("8"),
	_T("9"),
	_T("0"),
	_T("-"),
	_T("^"),
	_T("\\"),
	_T("BS"),

	_T("TAB"),
	_T("Q"),
	_T("W"),
	_T("E"),
	_T("R"),
	_T("T"),
	_T("Y"),
	_T("U"),
	_T("I"),
	_T("O"),
	_T("P"),
	_T("@"),
	_T("["),
	NULL,

	_T("A"),
	_T("S"),
	_T("D"),
	_T("F"),
	_T("G"),
	_T("H"),
	_T("J"),
	_T("K"),
	_T("L"),
	_T(";"),
	_T(":"),
	_T("]"),

	_T("Z"),
	_T("X"),
	_T("C"),
	_T("V"),
	_T("B"),
	_T("N"),
	_T("M"),
	_T(","),
	_T("."),
	_T("/"),
	_T(" "),
	_T(" "),

	_T("HOME"),
	_T("DEL"),
	_T("RLUP"),
	_T("RLDN"),
	_T("UNDO"),
	_T("←"),
	_T("↑"),
	_T("→"),
	_T("↓"),

	_T("CLR"),
	_T("/"),
	_T("*"),
	_T("-"),
	_T("7"),
	_T("8"),
	_T("9"),
	_T("+"),
	_T("4"),
	_T("5"),
	_T("6"),
	_T("="),
	_T("1"),
	_T("2"),
	_T("3"),
	_T("ENT"),
	_T("0"),
	_T(","),
	_T("."),

	_T("記号"),
	_T("登録"),
	_T("HELP"),

	_T("XF1"),
	_T("XF2"),
	_T("XF3"),
	_T("XF4"),
	_T("XF5"),

	_T("かな"),
	_T("ﾛｰﾏ"),
	_T("ｺｰﾄﾞ"),
	_T("CAPS"),
	_T("INS"),
	_T("ひら"),
	_T("全角"),

	_T("BRK"),
	_T("CPY"),
	_T("F1"),
	_T("F2"),
	_T("F3"),
	_T("F4"),
	_T("F5"),
	_T("F6"),
	_T("F7"),
	_T("F8"),
	_T("F9"),
	_T("F10"),
	NULL,
	NULL,
	NULL,

	_T("SHIFT"),
	_T("CTRL"),
	_T("OPT.1"),
	_T("OPT.2"),

	_T("SHIFT")
};

//---------------------------------------------------------------------------
//
//	文字列テーブル(かな、通常)
//
//---------------------------------------------------------------------------
LPCTSTR CKeyDispWnd::KanaTable[] = {
	_T("ﾇ"),
	_T("ﾌ"),
	_T("ｱ"),
	_T("ｳ"),
	_T("ｴ"),
	_T("ｵ"),
	_T("ﾔ"),
	_T("ﾕ"),
	_T("ﾖ"),
	_T("ﾜ"),
	_T("ﾎ"),
	_T("ﾍ"),
	_T("ｰ"),
	_T("BS"),

	_T("TAB"),
	_T("ﾀ"),
	_T("ﾃ"),
	_T("ｲ"),
	_T("ｽ"),
	_T("ｶ"),
	_T("ﾝ"),
	_T("ﾅ"),
	_T("ﾆ"),
	_T("ﾗ"),
	_T("ｾ"),
	_T("ﾞ"),
	_T("ﾟ"),
	NULL,

	_T("ﾁ"),
	_T("ﾄ"),
	_T("ｼ"),
	_T("ﾊ"),
	_T("ｷ"),
	_T("ｸ"),
	_T("ﾏ"),
	_T("ﾉ"),
	_T("ﾘ"),
	_T("ﾚ"),
	_T("ｹ"),
	_T("ﾑ"),

	_T("ﾂ"),
	_T("ｻ"),
	_T("ｿ"),
	_T("ﾋ"),
	_T("ｺ"),
	_T("ﾐ"),
	_T("ﾓ"),
	_T("ﾈ"),
	_T("ﾙ"),
	_T("ﾒ"),
	_T("ﾛ")
};

//---------------------------------------------------------------------------
//
//	文字列テーブル(かな、SHIFT)
//
//---------------------------------------------------------------------------
LPCTSTR CKeyDispWnd::KanaShiftTable[] = {
	_T("ﾇ"),
	_T("ﾌ"),
	_T("ｧ"),
	_T("ｩ"),
	_T("ｪ"),
	_T("ｫ"),
	_T("ｬ"),
	_T("ｭ"),
	_T("ｮ"),
	_T("ｦ"),
	_T("ﾎ"),
	_T("ﾍ"),
	_T("ｰ"),
	_T("BS"),

	_T("TAB"),
	_T("ﾀ"),
	_T("ﾃ"),
	_T("ｲ"),
	_T("ｽ"),
	_T("ｶ"),
	_T("ﾝ"),
	_T("ﾅ"),
	_T("ﾆ"),
	_T("ﾗ"),
	_T("ｾ"),
	_T("ﾞ"),
	_T("｢"),
	NULL,

	_T("ﾁ"),
	_T("ﾄ"),
	_T("ｼ"),
	_T("ﾊ"),
	_T("ｷ"),
	_T("ｸ"),
	_T("ﾏ"),
	_T("ﾉ"),
	_T("ﾘ"),
	_T("ﾚ"),
	_T("ｹ"),
	_T("｣"),

	_T("ｯ"),
	_T("ｻ"),
	_T("ｿ"),
	_T("ﾋ"),
	_T("ｺ"),
	_T("ﾐ"),
	_T("ﾓ"),
	_T("､"),
	_T("｡"),
	_T("･"),
	_T(" ")
};

//---------------------------------------------------------------------------
//
//	文字列テーブル(記号)
//
//---------------------------------------------------------------------------
LPCTSTR CKeyDispWnd::MarkTable[] = {
	_T("!"),
	_T("\x22"),
	_T("#"),
	_T("$"),
	_T("%"),
	_T("&"),
	_T("'"),
	_T("("),
	_T(")"),
	_T(" "),
	_T("="),
	_T("~"),
	_T("|")
};

//---------------------------------------------------------------------------
//
//	文字列テーブル(その他)
//
//---------------------------------------------------------------------------
LPCTSTR CKeyDispWnd::AnotherTable[] = {
	_T("q"),
	_T("w"),
	_T("e"),
	_T("r"),
	_T("t"),
	_T("y"),
	_T("u"),
	_T("i"),
	_T("o"),
	_T("p"),
	_T("`"),
	_T("{"),
	NULL,

	_T("a"),
	_T("s"),
	_T("d"),
	_T("f"),
	_T("g"),
	_T("h"),
	_T("j"),
	_T("k"),
	_T("l"),
	_T("+"),
	_T("*"),
	_T("}"),

	_T("z"),
	_T("x"),
	_T("c"),
	_T("v"),
	_T("b"),
	_T("n"),
	_T("m"),
	_T("<"),
	_T(">"),
	_T("?"),
	_T("_")
};

//===========================================================================
//
//	Software keyboard window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSoftKeyWnd::CSoftKeyWnd()
{
	CFrmWnd *pWnd;

	// Window parameter definition
	m_dwID = MAKEID('S', 'K', 'E', 'Y');
	::GetMsg(IDS_SWND_SOFTKEY, m_strCaption);

	// Screen size
	m_nWidth = 88;
	m_nHeight = 10;

	// Keyboard取得
	m_pKeyboard = (Keyboard*)::GetVM()->SearchDevice(MAKEID('K', 'E', 'Y', 'B'));
	ASSERT(m_pKeyboard);

	// Get input
	pWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pWnd);
	m_pInput = pWnd->GetInput();

	// Work initialization
	m_nSoftKey = 0;
	m_pDispWnd = NULL;
}

//---------------------------------------------------------------------------
//
//	メッセージ マップ
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSoftKeyWnd, CSubWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_ACTIVATE()
	ON_MESSAGE(WM_APP, OnApp)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CSoftKeyWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	CSize size;
	CRect rect;
	UINT uID;
	Keyboard::keyboard_t kbd;

	ASSERT(this);

	// Base class
	if (CSubWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Create status bar
	uID = 0;
	m_StatusBar.Create(this);
	m_StatusBar.SetIndicators(&uID, 1);
	m_StatusBar.SetPaneInfo(0, 0, SBPS_STRETCH, 0);

	// Calculate width and height (fixed size regardless of font)
	size = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
	rect.left = 0;
	rect.top = 0;
	rect.right = 616;
	rect.bottom = 140 + size.cy;

	// Calculate optimal window size and resize
	CalcWindowRect(&rect);
	SetWindowPos(&wndTop, 0, 0, rect.Width(), rect.Height(), SWP_NOMOVE);

	// Move status bar
	GetClientRect(&rect);
	rect.bottom -= size.cy;
	m_StatusBar.MoveWindow(0, rect.bottom, rect.Width(), size.cy);

	// BMPウィンドウ
	m_pDispWnd = new CKeyDispWnd;
	m_pDispWnd->Create(NULL, NULL, WS_CHILD | WS_VISIBLE,
						rect, this, 0, NULL);

	// Initial setup (from first display, display current state as is)
	m_pKeyboard->GetKeyboard(&kbd);
	Analyze(&kbd);
	m_pDispWnd->SetKey(kbd.status);

	// Success
	return 0;
}

//---------------------------------------------------------------------------
//
//	ウィンドウ削除
//
//---------------------------------------------------------------------------
void CSoftKeyWnd::OnDestroy()
{
	// If there is an emulated key, break it
	// Only if VM is alive
	if (m_nSoftKey != 0) {
		if (::GetVM()) {
			ASSERT(m_pKeyboard);
			m_pKeyboard->BreakKey(m_nSoftKey);
			m_nSoftKey = 0;
		}
	}

	// Unlink from child window
	m_pDispWnd = NULL;

	// Base class
	CSubWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	アクティベート
//
//---------------------------------------------------------------------------
void CSoftKeyWnd::OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized)
{
	// Base class
	CSubWnd::OnActivate(nState, pWnd, bMinimized);

	// If popup window
	if (m_bPopup) {
		ASSERT(m_pInput);

		// On focus, allow input. On non-focus, disallow input
		if (nState == WA_INACTIVE) {
			// Disallow input
			m_pInput->Activate(FALSE);
		}
		else {
			// Allow input
			m_pInput->Activate(TRUE);
		}
	}
}

//---------------------------------------------------------------------------
//
//	User
//	Notifry from Subwindow
//
//---------------------------------------------------------------------------
LONG CSoftKeyWnd::OnApp(UINT uParam, LONG lParam)
{
	CString strStatus;
	LPCTSTR lpszKey;
	Keyboard::keyboard_t kbd;

	ASSERT(this);
	ASSERT(uParam <= 0x73);

	// Keyboard取得
	ASSERT(m_pKeyboard);
	m_pKeyboard->GetKeyboard(&kbd);

	// Dispatch
	switch (lParam) {
		// Left button pressed
		case WM_LBUTTONDOWN:
			// If there is already an emulated key, release it
			if (m_nSoftKey != 0) {
				m_pKeyboard->BreakKey(m_nSoftKey);
				m_nSoftKey = 0;
			}

			// If that key is released, press it
			if (!kbd.status[uParam]) {
				m_pKeyboard->MakeKey(uParam);

				// SHIFT,CTRL以外であれば記録
				if ((uParam != 0x70) && (uParam != 0x71)) {
					m_nSoftKey = uParam;
				}
			}
			else {
				// On。SHIFT,CTRLであれば離してやる
				if ((uParam == 0x70) || (uParam == 0x71)) {
					m_pKeyboard->BreakKey(uParam);
				}
				else {
					// Continue pressing
					m_pKeyboard->MakeKey(uParam);

					// SHIFT,CTRL以外であれば記録
					if ((uParam != 0x70) && (uParam != 0x71)) {
						m_nSoftKey = uParam;
					}
				}
			}
			break;

		// Left button released
		case WM_LBUTTONUP:
			// If there is already an emulated key, release it
			if (m_nSoftKey != 0) {
				m_pKeyboard->BreakKey(m_nSoftKey);
				m_nSoftKey = 0;
			}
			break;

		// Right button pressed
		case WM_RBUTTONDOWN:
			// If there is already an emulated key, release it
			if (m_nSoftKey != 0) {
				m_pKeyboard->BreakKey(m_nSoftKey);
				m_nSoftKey = 0;
			}

			// If that key is released, press it
			if (!kbd.status[uParam]) {
				m_pKeyboard->MakeKey(uParam);
			}
			else {
				// On。離してやる
				m_pKeyboard->BreakKey(uParam);
				if (m_nSoftKey == uParam) {
					m_nSoftKey = 0;
				}
			}
			break;

		// Right button released
		case WM_RBUTTONUP:
			break;

		// Mouse move
		case WM_MOUSEMOVE:
			strStatus.Empty();
			if (uParam != 0) {
				strStatus.Format(_T("Key%02X  "), uParam);
				lpszKey = m_pInput->GetKeyName(uParam);
				ASSERT(lpszKey);
				strStatus += lpszKey;
			}
			m_StatusBar.SetPaneText(0, strStatus);
			break;

		// Others (impossible)
		default:
			ASSERT(FALSE);
			break;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	リフレッシュ
//
//---------------------------------------------------------------------------
void FASTCALL CSoftKeyWnd::Refresh()
{
	Keyboard::keyboard_t kbd;

	ASSERT(this);

	// Status check
	if (!m_pDispWnd || !m_pKeyboard) {
		return;
	}

	// KeyboardGet data
	m_pKeyboard->GetKeyboard(&kbd);

	// Data analysis and setup
	Analyze(&kbd);

	// Display
	m_pDispWnd->Refresh(kbd.status);
}

//---------------------------------------------------------------------------
//
//	Keyboardデータ解析
//
//---------------------------------------------------------------------------
void FASTCALL CSoftKeyWnd::Analyze(Keyboard::keyboard_t *pKbd)
{
	int i;
	DWORD dwLed;
	UINT nMode;

	ASSERT(this);
	ASSERT(pKbd);

	// Create and notify shift mode value
	nMode = pKbd->led & 0x0f;
	if (pKbd->status[0x70]) {
		nMode |= 0x80;
	}
	if (pKbd->status[0x71]) {
		nMode |= 0x40;
	}
	m_pDispWnd->SetShiftMode(nMode);

	// SHIFTキー移植
	pKbd->status[0x74] = pKbd->status[0x70];

	// LEDキーから状態を得る場合
	dwLed = pKbd->led;
	for (i=0; i<7; i++) {
		// LEDが点灯していればON、そうでなthen OFF
		if (dwLed & 1) {
			pKbd->status[i + 0x5a] = TRUE;
		}
		else {
			pKbd->status[i + 0x5a] = FALSE;
		}
		// Next
		dwLed >>= 1;
	}
}

#endif	// _WIN32
