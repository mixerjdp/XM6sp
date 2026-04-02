//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC Command Processing ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "fdd.h"
#include "fdi.h"
#include "rtc.h"
#include "keyboard.h"
#include "sasi.h"
#include "sram.h"
#include "memory.h"
#include "render.h"
#include "fileio.h"
#include "mfc_frm.h"
#include "mfc_res.h"
#include "mfc_draw.h"
#include "mfc_cpu.h"
#include "mfc_sys.h"
#include "mfc_dev.h"
#include "mfc_w32.h"
#include "mfc_ver.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_snd.h"
#include "mfc_inp.h"
#include "mfc_cfg.h"
#include "mfc_vid.h"
#include "mfc_rend.h"
#include "mfc_stat.h"
#include "mfc_tool.h"

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOpen()
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];

	// Execute common dialog
	::GetVM()->GetPath(path);
	_tcscpy(szPath, path.GetPath());
	if (!::FileOpenDlg(this, szPath, IDS_XM6OPEN)) {
		ResetCaption();
		return;
	}
	path.SetPath(szPath);

	// Pre-open processing
	if (!OnOpenPrep(path)) {
		return;
	}

	// Sub-open
	OnOpenSub(path);
}


void CFrmWnd::OnFastOpen()
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];
	CString quickLoadPath;
	quickLoadPath.Format(_T("%s\\%s.xm6"), (LPCTSTR)m_strSaveStatePath, (LPCTSTR)m_strXM6FileName);

	_tcsncpy(szPath, (LPCTSTR)quickLoadPath, _MAX_PATH - 1);
	szPath[_MAX_PATH - 1] = _T('\0');
	path.SetPath(szPath);


	// Pre-open processing
	if (!OnOpenPrep(path)) {
		return;
	}

	// Sub-open
	OnOpenSub(path);
}

//---------------------------------------------------------------------------
//
//	Open UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOpenUI(CCmdUI *pCmdUI)
{
	BOOL bPower;
	BOOL bSW;
	Filepath path;
	CMenu *pMenu;
	CMenu *pSubMenu;
	CString strExit;
	TCHAR szMRU[_MAX_PATH];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	int nEnable;
	int i;

	// Get power state and file path (under VM lock)
	::LockVM();
	bPower = ::GetVM()->IsPower();
	bSW = ::GetVM()->IsPowerSW();
	::GetVM()->GetPath(path);
	::UnlockVM();

	// Open
	pCmdUI->Enable(bPower);

	// Get submenu
	if (m_bPopupMenu) {
		pMenu = m_PopupMenu.GetSubMenu(0);
	}
	else {
		pMenu = &m_Menu;
	}
	ASSERT(pMenu);
	// File menu is first
	pSubMenu = pMenu->GetSubMenu(0);
	ASSERT(pSubMenu);

	// Overwrite UI (workaround for ON_UPDATE_COMMAND_UI timing)
	if (bPower && (_tcslen(path.GetPath()) > 0)) {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_GRAYED);
	}

	// Save as UI
	if (bPower) {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_GRAYED);
	}
	else {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_ENABLED);
	}

	// Reset UI
	if (bPower) {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_GRAYED);
	}

	// Interrupt UI
	if (bPower) {
		pSubMenu->EnableMenuItem(6, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(6, MF_BYPOSITION | MF_GRAYED);
	}

	// Power switch UI
	if (bSW) {
		pSubMenu->EnableMenuItem(7, MF_BYPOSITION | MF_CHECKED);
	}
	else {
		pSubMenu->EnableMenuItem(7, MF_BYPOSITION | MF_UNCHECKED);
	}

	// Open separator and delete all following menus
	while (pSubMenu->GetMenuItemCount() > 9) {
		pSubMenu->RemoveMenu(9, MF_BYPOSITION);
	}

	// If no MRU, add exit menu and return
	if (GetConfig()->GetMRUNum(4) == 0) {
		::GetMsg(IDS_EXIT, strExit);
		pSubMenu->AppendMenu(MF_STRING, IDM_EXIT, strExit);
		return;
	}

	// On/off constants configuration
	if (bPower) {
		nEnable = MF_BYCOMMAND | MF_GRAYED;
	}
	else {
		nEnable = MF_BYCOMMAND | MF_ENABLED;
	}

	// MRU processing - Add
	for (i=0; i<9; i++) {
		// Try to get
		GetConfig()->GetMRUFile(4, i, szMRU);
		if (szMRU[0] == _T('\0')) {
			break;
		}

		// Add to menu
		_tsplitpath(szMRU, szDrive, szDir, szFile, szExt);
		if (_tcslen(szDir) > 1) {
			_tcscpy(szDir, _T("\\...\\"));
		}
		_stprintf(szMRU, _T("&%d "), i + 1);
		_tcscat(szMRU, szDrive);
		_tcscat(szMRU, szDir);
		_tcscat(szMRU, szFile);
		_tcscat(szMRU, szExt);

		pSubMenu->AppendMenu(MF_STRING, IDM_XM6_MRU0 + i, szMRU);
		pSubMenu->EnableMenuItem(IDM_XM6_MRU0 + i, nEnable);
	}

	// Add separator
	pSubMenu->AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);

	// Add exit menu
	::GetMsg(IDS_EXIT, strExit);
	pSubMenu->AppendMenu(MF_STRING, IDM_EXIT, strExit);
}

//---------------------------------------------------------------------------
//
//	Pre-open check
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::OnOpenPrep(const Filepath& path, BOOL bWarning)
{
	Fileio fio;
	CString strMsg;
	CString strFmt;
	char cHeader[0x10];
	int nRecVer;
	int nNowVer;
	DWORD dwMajor;
	DWORD dwMinor;

	ASSERT(this);

	// Check file existence
	if (!fio.Open(path, Fileio::ReadOnly)) {
		if (bWarning) {
			::GetMsg(IDS_XM6LOADFILE, strMsg);
			MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		}
		return FALSE;
	}

	// Read header
	memset(cHeader, 0, sizeof(cHeader));
	fio.Read(cHeader, sizeof(cHeader));
	fio.Close();

	// Get recorded version
	cHeader[0x0a] = '\0';
	nRecVer = ::strtoul(&cHeader[0x09], NULL, 16);
	nRecVer <<= 8;
	cHeader[0x0d] = '\0';
	nRecVer |= ::strtoul(&cHeader[0x0b], NULL, 16);

	// Get current version
	::GetVM()->GetVersion(dwMajor, dwMinor);
	nNowVer = (int)((dwMajor << 8) | dwMinor);

	// Header check
	cHeader[0x09] = '\0';
	if (strcmp(cHeader, "XM6 DATA ") != 0) {
		if (bWarning) {
			::GetMsg(IDS_XM6LOADHDR, strMsg);
			MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		}
		return FALSE;
	}

	// Version check
	if (nNowVer < nRecVer) {
		// Recorded version is newer (unknown format)
		::GetMsg(IDS_XM6LOADVER, strMsg);
		strFmt.Format(strMsg,
						nNowVer >> 8, nNowVer & 0xff,
						nRecVer >> 8, nRecVer & 0xff);
		MessageBox(strFmt, NULL, MB_ICONSTOP | MB_OK);
		return FALSE;
	}

	// Continue
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Sub-open
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::OnOpenSub(const Filepath& path)
{
	BOOL bRun;
	BOOL bSound;
	CString strMsg;
	DWORD dwPos;
	Filepath diskpath;
	int nDrive;

	// Stop scheduler and sound
	bRun = GetScheduler()->IsEnable();
	GetScheduler()->Enable(FALSE);
	::LockVM();
	::UnlockVM();
	bSound = GetSound()->IsEnable();
	GetSound()->Enable(FALSE);

	// Load
	AfxGetApp()->BeginWaitCursor();

	// VM
	dwPos = ::GetVM()->Load(path);
	//MessageBox("dwPos: " + dwPos, NULL, MB_ICONSTOP | MB_OK);
	if (dwPos == 0) {
		AfxGetApp()->EndWaitCursor();

		// Failure is dangerous due to half interrupt; always reset
		::GetVM()->Reset();
		GetSound()->Enable(bSound);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();

		// LoadError
		::GetMsg(IDS_XM6LOADERR, strMsg);
		CString msg;
		msg.Format(_T("File read (VM): %u"), dwPos);
		MessageBox(msg, NULL, MB_ICONSTOP | MB_OK);

		return FALSE;
	}

	// MFC
	if (!LoadComponent(path, dwPos)) {
		AfxGetApp()->EndWaitCursor();

		// Failure is dangerous due to half interrupt; always reset
		::GetVM()->Reset();
		GetSound()->Enable(bSound);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();

		// LoadError
		::GetMsg(IDS_XM6LOADERR, strMsg);
		MessageBox("File read failed (MFC)", NULL, MB_ICONSTOP | MB_OK);
		return FALSE;
	}

	// Load finish
	AfxGetApp()->EndWaitCursor();

	// Add FD, MO, CD to MRU (measure for resume from v2.04)
	for (nDrive=0; nDrive<2; nDrive++) {
		if (m_pFDD->IsReady(nDrive, FALSE)) {
			m_pFDD->GetPath(nDrive, diskpath);
			GetConfig()->SetMRUFile(nDrive, diskpath.GetPath());
		}
	}
	if (m_pSASI->IsReady()) {
		m_pSASI->GetPath(diskpath);
		GetConfig()->SetMRUFile(2, diskpath.GetPath());
	}
	if (m_pSCSI->IsReady(FALSE)) {
		m_pSCSI->GetPath(diskpath, FALSE);
		GetConfig()->SetMRUFile(3, diskpath.GetPath());
	}

	// If saved with scheduler stopped, remain stopped (v2.04)
	if (GetScheduler()->HasSavedEnable()) {
		bRun = GetScheduler()->GetSavedEnable();
	}

	// Clear execution counter
	m_dwExec = 0;

	// Success
	GetSound()->Enable(bSound);
	GetScheduler()->Reset();
	GetScheduler()->Enable(bRun);
	ResetCaption();

	// Add to MRU
	GetConfig()->SetMRUFile(4, path.GetPath());

	// Show information message
	::GetMsg(IDS_XM6LOADOK, strMsg);
	SetInfo(strMsg);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Save/Overwrite
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSave()
{
	Filepath path;

	// Get current VM path
	::GetVM()->GetPath(path);

	// If clear, return
	if (path.IsClear()) {
		return;
	}

	// Sub-save
	OnSaveSub(path);
}

//---------------------------------------------------------------------------
//
//	Overwrite UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSaveUI(CCmdUI *pCmdUI)
{
	Filepath path;

	// Prohibit if power is off
	if (!::GetVM()->IsPower()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Get current VM path
	::GetVM()->GetPath(path);

	// Prohibit if clear
	if (path.IsClear()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Allow
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	Fast save
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSaveAs()
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];

	// Execute common dialog
	::GetVM()->GetPath(path);
	_tcscpy(szPath, path.GetPath());
	/*if (!::FileSaveDlg(this, szPath, _T("xm6"), IDS_XM6OPEN)) {
		ResetCaption();

		return;
	}*/

	 CString fileString;
	 fileString.Format(_T("%s\\%s.xm6"), (LPCTSTR)m_strSaveStatePath, (LPCTSTR)m_strXM6FileName);

	 /* QUICK STATE SAVE HERE */


	/* int msgboxID = MessageBox(
		 cadenaArchivo, "SAVED",
         4     );*/



	 path.SetPath((LPCTSTR)fileString);


	// Sub-save
	OnSaveSub(path);
}

//---------------------------------------------------------------------------
//
//	Save as UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSaveAsUI(CCmdUI *pCmdUI)
{
	// Only if power is on
	pCmdUI->Enable(::GetVM()->IsPower());
}

//---------------------------------------------------------------------------
//
//	Sub-save
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnSaveSub(const Filepath& path)
{
	BOOL bRun;
	BOOL bSound;
	CString strMsg;
	DWORD dwPos;

	// Stop scheduler and sound
	bRun = GetScheduler()->IsEnable();
	GetScheduler()->Enable(FALSE);
	::LockVM();
	::UnlockVM();
	bSound = GetSound()->IsEnable();
	GetSound()->Enable(FALSE);

	AfxGetApp()->BeginWaitCursor();

	// Notify scheduler the state at save time (v2.04)
	GetScheduler()->SetSavedEnable(bRun);

	// VM
	// Make sure to include a full filename; adjust as needed
	dwPos = ::GetVM()->Save(path);

	if (dwPos== 0) {
		AfxGetApp()->EndWaitCursor();

		// Save failed
		GetSound()->Enable(bSound);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();

		// Save error
		::GetMsg(IDS_XM6SAVEERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		return;
	}

	// MFC
	if (!SaveComponent(path, dwPos)) {
		AfxGetApp()->EndWaitCursor();

		// Save failed
		GetSound()->Enable(bSound);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();

		// Save error
		::GetMsg(IDS_XM6SAVEERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		return;
	}

	// Clear execution counter
	m_dwExec = 0;
	AfxGetApp()->EndWaitCursor();

	// Success
	GetSound()->Enable(bSound);
	GetScheduler()->Reset();
	GetScheduler()->Enable(bRun);
	ResetCaption();

	// Add to MRU
	GetConfig()->SetMRUFile(4, path.GetPath());

	// Show information message
	::GetMsg(IDS_XM6SAVEOK, strMsg);
	SetInfo(strMsg);
}

//---------------------------------------------------------------------------
//
//	MRU
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMRU(UINT uID)
{
	TCHAR szMRU[_MAX_PATH];
	Filepath path;

	ASSERT(uID >= IDM_XM6_MRU0);

	// Convert uID
	uID -= IDM_XM6_MRU0;
	ASSERT(uID <= 8);

	// Get MRU, create path
	GetConfig()->GetMRUFile(4, (int)uID, szMRU);
	if (szMRU[0] == _T('\0')) {
		return;
	}
	path.SetPath(szMRU);

	// Pre-open processing
	if (!OnOpenPrep(path)) {
		return;
	}

	// Normal open
	if (OnOpenSub(path)) {
		// Update default directory
		Filepath::SetDefaultDir(szMRU);
	}
}

//---------------------------------------------------------------------------
//
//	MRU UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMRUUI(CCmdUI *pCmdUI)
{
	// Only if power is on
	pCmdUI->Enable(::GetVM()->IsPower());
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void CFrmWnd::OnReset()
{
	SRAM *pSRAM;
	DWORD Sw[0x100];
	DWORD dwDevice;
	DWORD dwAddr;
	CString strReset;
	CString strSub;
	BOOL bFlag;
	int i;


	// Operation not available if power is off
	if (!::GetVM()->IsPower()) {
		return;
	}

	::LockVM();

	// Reset and redraw
	::GetVM()->Reset();
	//OutputDebugString("\n\nExecuted GetVM->Reset\n\n");
	GetView()->Refresh();
	ResetCaption();

	// Get memory switches
	pSRAM = (SRAM*)::GetVM()->SearchDevice(MAKEID('S', 'R', 'A', 'M'));
	ASSERT(pSRAM);
	for (i=0; i<0x100; i++) {
		Sw[i] = pSRAM->ReadOnly(0xed0000 + i);
	}

	::UnlockVM();
	//OutputDebugString("\n\nExecuted UnlockVm\n\n");

	// Load reset message
	::GetMsg(IDS_RESET, strReset);

	// Compare beginning of memory switches
	if (memcmp(Sw, SigTable, sizeof(DWORD) * 7) != 0) {
		SetInfo(strReset);
		return;
	}

	// Get boot device
	dwDevice = Sw[0x18];
	dwDevice <<= 8;
	dwDevice |= Sw[0x19];

	// Discriminate boot device
	bFlag = FALSE;
	if (dwDevice == 0x0000) {
		// STD
		strSub = _T("STD)");
		bFlag = TRUE;
	}
	if (dwDevice == 0xa000) {
		// ROM
		dwAddr = Sw[0x0c];
		dwAddr = (dwAddr << 8) | Sw[0x0d];
		dwAddr = (dwAddr << 8) | Sw[0x0e];
		dwAddr = (dwAddr << 8) | Sw[0x0f];

		// FC0000~FC001C and EA0020~EA003C are SCSI#
		strSub.Format(_T("ROM $%06X)"), dwAddr);
		if ((dwAddr >= 0xfc0000) && (dwAddr < 0xfc0020)) {
			strSub.Format(_T("SCSI%1d)"), (dwAddr & 0x001f) >> 2);
		}
		if ((dwAddr >= 0xea0020) && (dwAddr < 0xea0040)) {
			strSub.Format(_T("SCSI%1d)"), (dwAddr & 0x001f) >> 2);
		}
		bFlag = TRUE;
	}
	if (dwDevice == 0xb000) {
		// RAM
		dwAddr = Sw[0x10];
		dwAddr = (dwAddr << 8) | Sw[0x11];
		dwAddr = (dwAddr << 8) | Sw[0x12];
		dwAddr = (dwAddr << 8) | Sw[0x13];
		strSub.Format(_T("RAM $%06X)"), dwAddr);
		bFlag = TRUE;
	}
	if ((dwDevice & 0xf0ff) == 0x9070) {
		strSub.Format(_T("2HD%1d)"), (dwDevice & 0xf00) >> 8);
		bFlag = TRUE;
	}
	if ((dwDevice & 0xf0ff) == 0x8000) {
		strSub.Format(_T("HD%1d)"), (dwDevice & 0xf00) >> 8);
		bFlag = TRUE;
	}
	if (!bFlag) {
		strSub = _T("Unknown)");
	}

	// Show
	strReset += _T(" (");
	strReset += strSub;
	SetInfo(strReset);
	OutputDebugString("\n\nExecuted OnReset old...\n\n");
}

//---------------------------------------------------------------------------
//
//	Reset UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnResetUI(CCmdUI *pCmdUI)
{
	// Only when power is ON
	pCmdUI->Enable(::GetVM()->IsPower());
}



void CFrmWnd::OnResetNew()
{
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	ShellExecute(GetSafeHwnd(), "open", buffer, NULL, NULL, 1);

	OutputDebugString("\n\nOnResetNew executed...\n\n");
	PostMessage(WM_CLOSE, 0, 0);
}

void CFrmWnd::OnScc()
{
	if (m_strXM6FileName.GetLength() > 0)
	{
		m_pConfig->CustomInit(FALSE);
		m_pConfig->Cleanup2();

		CString sz;
		sz.Format(_T("\n\nConfiguration saved for %s\n\n"), (LPCTSTR)m_strXM6FileName);
		OutputDebugStringW(CT2W(sz));

		MessageBox(sz, "Configuration", MB_OK);
	}
	else
	{
		MessageBox("No configuration was saved because no game was loaded", "Configuration", MB_OK);
	}
}

void CFrmWnd::OnSccUI(CCmdUI* pCmdUI)
{
	// Only if power is on
	pCmdUI->Enable(::GetVM()->IsPower());
}

// Save global configuration
void CFrmWnd::OnSgc()
{
	m_pConfig->CustomInit(TRUE);
	m_pConfig->Cleanup2();

	CString sz;
	sz.Format(_T("\n\nGlobal configuration saved\n\n"));
	OutputDebugStringW(CT2W(sz));

	MessageBox(sz, "Configuration", MB_OK);
}

void CFrmWnd::OnSgcUI(CCmdUI* pCmdUI)
{
	// Only if power is on
	pCmdUI->Enable(::GetVM()->IsPower());
}

void CFrmWnd::OnSgcr()
{
	m_pConfig->CustomInit(TRUE);
	m_pConfig->Cleanup2();
	OnResetNew();
}

void CFrmWnd::OnSgcrUI(CCmdUI* pCmdUI)
{
	// Only if power is on
	pCmdUI->Enable(::GetVM()->IsPower());
}


//---------------------------------------------------------------------------
//
//	SRAM signature table
//
//---------------------------------------------------------------------------
const DWORD CFrmWnd::SigTable[] = {
	0x82, 0x77, 0x36, 0x38, 0x30, 0x30, 0x30
};

//---------------------------------------------------------------------------
//
//	Interrupt
//
//---------------------------------------------------------------------------
void CFrmWnd::OnInterrupt()
{
	CString strIntr;

	// Only when power is ON
	if (::GetVM()->IsPower()) {
		// Interrupt NMI
		::LockVM();
		::GetVM()->Interrupt();
		::UnlockVM();

		// Set info message
		::GetMsg(IDS_INTERRUPT, strIntr);
		SetInfo(strIntr);
	}
}

//---------------------------------------------------------------------------
//
//	Interrupt UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnInterruptUI(CCmdUI *pCmdUI)
{
	// Only when power is ON
	pCmdUI->Enable(::GetVM()->IsPower());
}

//---------------------------------------------------------------------------
//
//	Power switch
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPower()
{
	BOOL bPower;

	::LockVM();

	if (::GetVM()->IsPowerSW()) {
		// If on, turn off
		::GetVM()->PowerSW(FALSE);
		::UnlockVM();
		return;
	}

	// If power was OFF, save state and turn ON
	bPower = ::GetVM()->IsPower();
	::GetVM()->PowerSW(TRUE);

	// If previous was OFF and scheduler was not running, start scheduler
	if (!bPower && !GetScheduler()->IsEnable()) {
		GetScheduler()->Enable(TRUE);
	}

	::UnlockVM();

	// Reset (Show in status bar)
	OnReset();
}

//---------------------------------------------------------------------------
//
//	Power switch UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPowerUI(CCmdUI *pCmdUI)
{
	// Check if on, otherwise uncheck
	pCmdUI->SetCheck(::GetVM()->IsPowerSW());
}

//---------------------------------------------------------------------------
//
//	Exit
//
//---------------------------------------------------------------------------
void CFrmWnd::OnExit()
{
	PostMessage(WM_CLOSE, 0, 0);
}

//---------------------------------------------------------------------------
//
//	Floppy disk processing
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFD(UINT uID)
{
	int nDrive;

	// Determine drive
	nDrive = 0;
	if (uID >= IDM_D1OPEN) {
		nDrive = 1;
		uID -= (IDM_D1OPEN - IDM_D0OPEN);
	}

	switch (uID) {
		// Open
		case IDM_D0OPEN:
			OnFDOpen(nDrive);
			break;

		// Eject
		case IDM_D0EJECT:
			OnFDEject(nDrive);
			break;

		// Write protect
		case IDM_D0WRITEP:
			OnFDWriteP(nDrive);
			break;

		// Forced ejection
		case IDM_D0FORCE:
			OnFDForce(nDrive);
			break;

		// Invalid insert
		case IDM_D0INVALID:
			OnFDInvalid(nDrive);
			break;

		// Others
		default:
			if (uID >= IDM_D0_MRU0) {
				// MRU
				uID -= IDM_D0_MRU0;
				ASSERT(uID <= 8);
				OnFDMRU(nDrive, (int)uID);
			}
			else {
				// Media
				uID -= IDM_D0_MEDIA0;
				ASSERT(uID <= 15);
				OnFDMedia(nDrive, (int)uID);
			}
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Open floppy ROM
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDOpen(int nDrive)
{
	Filepath path;
	CString strMsg;
	TCHAR szPath[_MAX_PATH];
	FDI *pFDI;

	ASSERT((nDrive == 0) || (nDrive == 1));
	ASSERT(m_pFDD);

	// Execute common dialog
	memset(szPath, 0, sizeof(szPath));
	if (!::FileOpenDlg(this, szPath, IDS_FDOPEN)) {
		ResetCaption();
		return;
	}
	path.SetPath(szPath);

	/* FLOPPY FILES (ROMS) ARE OPENED HERE */


	CString sz;
	sz.Format(_T("%s"),  szPath);
    CString fileName= sz.Mid(sz.ReverseFind('\\')+1);

	m_strXM6FilePath = szPath;
	m_strXM6FileName = fileName;



	// VM lock
	::LockVM();

	// Assign disk
	if (!m_pFDD->Open(nDrive, path)) {
		GetScheduler()->Reset();
		::UnlockVM();

		// Open error
		::GetMsg(IDS_FDERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		ResetCaption();
		return;
	}

	// Get FDI before restarting VM
	pFDI = m_pFDD->GetFDI(nDrive);

	// Success
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// Add to MRU
	GetConfig()->SetMRUFile(nDrive, szPath);

	// Success but BAD FDI warning
	if (pFDI->GetID() == MAKEID('B', 'A', 'D', ' ')) {
		::GetMsg(IDS_BADFDI_WARNING, strMsg);
		//MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
	}
}

//---------------------------------------------------------------------------
//
//	Floppy ejection
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDEject(int nDrive)
{
	ASSERT(m_pFDD);
	ASSERT((nDrive == 0) || (nDrive == 1));

	// VM lock and execute
	::LockVM();
	m_pFDD->Eject(nDrive, FALSE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Write protect
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDWriteP(int nDrive)
{
	ASSERT(m_pFDD);
	ASSERT((nDrive == 0) || (nDrive == 1));

	// Toggle write protect
	::LockVM();
	m_pFDD->WriteP(nDrive, !m_pFDD->IsWriteP(nDrive));
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Forced ejection
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDForce(int nDrive)
{
	ASSERT(m_pFDD);
	ASSERT((nDrive == 0) || (nDrive == 1));

	// VM lock and execute
	::LockVM();
	m_pFDD->Eject(nDrive, TRUE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Invalid floppy insert
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDInvalid(int nDrive)
{
	ASSERT(m_pFDD);
	ASSERT((nDrive == 0) || (nDrive == 1));

	// VM lock and execute
	::LockVM();
	m_pFDD->Invalid(nDrive);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Floppy media (disks)
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDMedia(int nDrive, int nMedia)
{
	Filepath path;

	ASSERT((nDrive == 0) || (nDrive == 1));
	ASSERT((nMedia >= 0) && (nMedia <= 15));

	// VM lock
	::LockVM();

	// Confirm just in case
	if (nMedia < m_pFDD->GetDisks(nDrive)) {
		m_pFDD->GetPath(nDrive, path);

		// Open
		m_pFDD->Open(nDrive, path, nMedia);
	}

	// VM unlock
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Open from saved MRU
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDMRU(int nDrive, int nMRU)
{
	TCHAR szMRU[_MAX_PATH];
	Filepath path;
	BOOL bResult;
	FDI *pFDI;
	CString strMsg;

	ASSERT((nDrive == 0) || (nDrive == 1));
	ASSERT((nMRU >= 0) && (nMRU <= 8));

	// Get MRU, create path
	GetConfig()->GetMRUFile(nDrive, nMRU, szMRU);
	if (szMRU[0] == _T('\0')) {
		return;
	}
	path.SetPath(szMRU);

	CString sz;
	sz.Format(_T("%s"),  szMRU);
    CString fileName= sz.Mid(sz.ReverseFind('\\')+1);


	/* When loading ROM from MRU list */
	m_strXM6FilePath = szMRU;
	m_strXM6FileName = fileName;


	// VM lock
	::LockVM();

	// Try to assign disk
	bResult = m_pFDD->Open(nDrive, path);
	pFDI = m_pFDD->GetFDI(nDrive);
	GetScheduler()->Reset();
	ResetCaption();

	// VM unlock
	::UnlockVM();

	// If successful, update directory and add to MRU
	if (bResult) {
		// Update default directory
		Filepath::SetDefaultDir(szMRU);

		// Add to MRU
		GetConfig()->SetMRUFile(nDrive, szMRU);

		// BAD FDI warning
		if (pFDI->GetID() == MAKEID('B', 'A', 'D', ' ')) {
			::GetMsg(IDS_BADFDI_WARNING, strMsg);
			//MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Floppy open UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDOpenUI(CCmdUI *pCmdUI)
{
	CMenu *pMenu;
	CMenu *pSubMenu;
	UINT nEnable;
	int nDrive;
	int nStat;
	int nDisks;
	int nMedia;
	char szShort[_MAX_PATH];
	LPTSTR lpszShort;
	int i;
	TCHAR szMRU[_MAX_PATH];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];

	ASSERT(this);
	ASSERT(m_pFDD);

	// Determine drive
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// Eject prohibited, disk already open
	::LockVM();
	nStat = m_pFDD->GetStatus(nDrive);
	m_nFDDStatus[nDrive] = nStat;
	nDisks = m_pFDD->GetDisks(nDrive);
	nMedia = m_pFDD->GetMedia(nDrive);
	::UnlockVM();
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		pCmdUI->Enable(FALSE);
	}
	else {
		pCmdUI->Enable(TRUE);
	}

	// Get submenu
	if (m_bPopupMenu) {
		pMenu = m_PopupMenu.GetSubMenu(0);
	}
	else {
		pMenu = &m_Menu;
	}
	// Floppy 0 and Floppy 1 align after File (F)
	pSubMenu = pMenu->GetSubMenu(nDrive + 1);

	// Ejection UI (workaround for ON_UPDATE_COMMAND_UI timing)
	if ((nStat & FDST_INSERT) && (nStat & FDST_EJECT)) {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_GRAYED);
	}

	// Write protect UI
	if (m_pFDD->IsReadOnly(nDrive) || !(nStat & FDST_INSERT)) {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_GRAYED);
	}
	else {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_ENABLED);
	}

	// Forced ejection UI
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_GRAYED);
	}

	// Invalid insert UI
	if (!(nStat & FDST_INSERT) && !(nStat & FDST_INVALID)) {
		pSubMenu->EnableMenuItem(5, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(5, MF_BYPOSITION | MF_GRAYED);
	}

	// Delete subsequent menus
	while (pSubMenu->GetMenuItemCount() > 6) {
		pSubMenu->RemoveMenu(6, MF_BYPOSITION);
	}

	// Multi-disk processing
	if (nDisks > 1) {
		// On/off constants configuration
		if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
			nEnable = MF_BYCOMMAND | MF_ENABLED;
		}
		else {
			nEnable = MF_BYCOMMAND | MF_GRAYED;
		}

		// Separator
		pSubMenu->AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);

		// Media (disks) loop
		ASSERT(nDisks <= 16);
		for (i=0; i<nDisks; i++) {
			// Disk names stored as char*; convert to TCHAR
			m_pFDD->GetName(nDrive, szShort, i);
			lpszShort = A2T(szShort);

			// Add
			if (nDrive == 0) {
				pSubMenu->AppendMenu(MF_STRING, IDM_D0_MEDIA0 + i, lpszShort);
				pSubMenu->EnableMenuItem(IDM_D0_MEDIA0 + i, nEnable);
			}
			else {
				pSubMenu->AppendMenu(MF_STRING, IDM_D1_MEDIA0 + i, lpszShort);
				pSubMenu->EnableMenuItem(IDM_D1_MEDIA0 + i, nEnable);
			}
		}

		// Radio button configuration
		if (nDrive == 0) {
			pSubMenu->CheckMenuRadioItem(IDM_D0_MEDIA0, IDM_D0_MEDIAF,
										IDM_D0_MEDIA0 + nMedia, MF_BYCOMMAND);
		}
		else {
			pSubMenu->CheckMenuRadioItem(IDM_D1_MEDIA0, IDM_D1_MEDIAF,
										IDM_D1_MEDIA0 + nMedia, MF_BYCOMMAND);
		}
	}

	// MRU processing - Separator
	if (GetConfig()->GetMRUNum(nDrive) == 0) {
		return;
	}
	pSubMenu->AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);

	// On/off constants configuration
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		nEnable = MF_BYCOMMAND | MF_GRAYED;
	}
	else {
		nEnable = MF_BYCOMMAND | MF_ENABLED;
	}




	// MRU processing - Add
	for (i=0; i<9; i++) {
		// Try to get
		GetConfig()->GetMRUFile(nDrive, i, szMRU);
		if (szMRU[0] == _T('\0')) {
			break;
		}

		// Add to menu
		_tsplitpath(szMRU, szDrive, szDir, szFile, szExt);
		if (_tcslen(szDir) > 1) {
			_tcscpy(szDir, _T("\\...\\"));
		}
		_stprintf(szMRU, _T("&%d "), i + 1);
		_tcscat(szMRU, szDrive);
		_tcscat(szMRU, szDir);
		_tcscat(szMRU, szFile);
		_tcscat(szMRU, szExt);
		if (nDrive == 0) {
			pSubMenu->AppendMenu(MF_STRING, IDM_D0_MRU0 + i, szMRU);
			pSubMenu->EnableMenuItem(IDM_D0_MRU0 + i, nEnable);
		}
		else {
			pSubMenu->AppendMenu(MF_STRING, IDM_D1_MRU0 + i, szMRU);
			pSubMenu->EnableMenuItem(IDM_D1_MRU0 + i, nEnable);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Floppy ejection UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDEjectUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determine drive
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// Get status
	nStat = m_nFDDStatus[nDrive];

	// Can eject if inserted and ejection not prohibited
	if ((nStat & FDST_INSERT) && (nStat & FDST_EJECT)) {
		pCmdUI->Enable(TRUE);
		return;
	}
	pCmdUI->Enable(FALSE);
}

//---------------------------------------------------------------------------
//
//	Write protect UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDWritePUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determine drive
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// Get status
	nStat = m_nFDDStatus[nDrive];

	// Check according to write protect
	pCmdUI->SetCheck(m_pFDD->IsWriteP(nDrive));

	// Disable if read only or not inserted
	if (m_pFDD->IsReadOnly(nDrive) || !(nStat & FDST_INSERT)) {
		pCmdUI->Enable(FALSE);
		return;
	}
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	Forced ejection UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDForceUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determine drive
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// Get status
	nStat = m_nFDDStatus[nDrive];

	// Enabled only when ejection prohibited
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		pCmdUI->Enable(TRUE);
		return;
	}
	pCmdUI->Enable(FALSE);
}

//---------------------------------------------------------------------------
//
//	Invalid floppy insert UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDInvalidUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determine drive
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// Get status
	nStat = m_nFDDStatus[nDrive];

	// Enabled only when not inserted and not invalid
	if (!(nStat & FDST_INSERT) && !(nStat & FDST_INVALID)) {
		pCmdUI->Enable(TRUE);
		return;
	}
	pCmdUI->Enable(FALSE);
}

//---------------------------------------------------------------------------
//
//	Floppy media UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDMediaUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determine drive
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// Get status
	nStat = m_nFDDStatus[nDrive];

	// Eject prohibited, disk already open
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		pCmdUI->Enable(FALSE);
	}
	else {
		pCmdUI->Enable(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	MRU UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDMRUUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determine drive
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// Get status
	nStat = m_nFDDStatus[nDrive];

	// Eject prohibited, disk already open
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		pCmdUI->Enable(FALSE);
	}
	else {
		pCmdUI->Enable(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	MO disk open
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOOpen()
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];
	CString strMsg;

	ASSERT(this);
	ASSERT(m_pSASI);

	// Execute common dialog
	memset(szPath, 0, sizeof(szPath));
	if (!::FileOpenDlg(this, szPath, IDS_MOOPEN)) {
		ResetCaption();
		return;
	}
	path.SetPath(szPath);

	// VM lock
	::LockVM();

	// Assign MO disk
	if (!m_pSASI->Open(path)) {
		GetScheduler()->Reset();
		::UnlockVM();

		// Open error
		::GetMsg(IDS_MOERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		ResetCaption();
		return;
	}

	// Success
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// Add to MRU
	GetConfig()->SetMRUFile(2, szPath);
}

//---------------------------------------------------------------------------
//
//	MO disk open UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOOpenUI(CCmdUI *pCmdUI)
{
	int i;
	BOOL bValid;
	BOOL bReady;
	BOOL bWriteP;
	BOOL bReadOnly;
	BOOL bLocked;
	TCHAR szMRU[_MAX_PATH];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	CMenu *pMenu;
	CMenu *pSubMenu;
	UINT nEnable;

	ASSERT(this);
	ASSERT(m_pSASI);

	// Get window status (under lock)
	::LockVM();
	bValid = m_pSASI->IsValid();
	bReady = m_pSASI->IsReady();
	bWriteP = m_pSASI->IsWriteP();
	bReadOnly = m_pSASI->IsReadOnly();
	bLocked = m_pSASI->IsLocked();
	::UnlockVM();

	// Open
	if (bValid) {
		if (bReady && bLocked) {
			pCmdUI->Enable(FALSE);
		}
		else {
			pCmdUI->Enable(TRUE);
		}
	}
	else {
		pCmdUI->Enable(FALSE);
	}

	// Get submenu
	if (m_bPopupMenu) {
		pMenu = m_PopupMenu.GetSubMenu(0);
	}
	else {
		pMenu = &m_Menu;
	}
	ASSERT(pMenu);
	// MO menu follows File, Floppy 0, Floppy 1
	pSubMenu = pMenu->GetSubMenu(3);
	ASSERT(pSubMenu);

	// Ejection UI (workaround for ON_UPDATE_COMMAND_UI timing)
	if (bReady && !bLocked) {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_GRAYED);
	}

	// Write protect UI
	if (bReady && !bReadOnly) {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_GRAYED);
	}
	else {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_ENABLED);
	}

	// Forced ejection UI
	if (bReady && bLocked) {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_GRAYED);
	}

	// Delete subsequent menus
	while (pSubMenu->GetMenuItemCount() > 5) {
		pSubMenu->RemoveMenu(5, MF_BYPOSITION);
	}

	// MRU processing - Separator
	if (GetConfig()->GetMRUNum(2) == 0) {
		return;
	}
	pSubMenu->AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);

	// On/off constants configuration
	nEnable = MF_BYCOMMAND | MF_GRAYED;
	if (bValid) {
		// Window enabled
		if (!bReady || !bLocked) {
			// If no media or not locked, can insert
			nEnable = MF_BYCOMMAND | MF_ENABLED;
		}
	}

	// MRU processing - Add
	for (i=0; i<9; i++) {
		// Try to get
		GetConfig()->GetMRUFile(2, i, szMRU);
		if (szMRU[0] == _T('\0')) {
			break;
		}

		// Add to menu
		_tsplitpath(szMRU, szDrive, szDir, szFile, szExt);
		if (_tcslen(szDir) > 1) {
			_tcscpy(szDir, _T("\\...\\"));
		}
		_stprintf(szMRU, _T("&%d "), i + 1);
		_tcscat(szMRU, szDrive);
		_tcscat(szMRU, szDir);
		_tcscat(szMRU, szFile);
		_tcscat(szMRU, szExt);

		pSubMenu->AppendMenu(MF_STRING, IDM_MO_MRU0 + i, szMRU);
		pSubMenu->EnableMenuItem(IDM_MO_MRU0 + i, nEnable);
	}
}

//---------------------------------------------------------------------------
//
//	MO disk ejection
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOEject()
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// VM lock and execute
	::LockVM();
	m_pSASI->Eject(FALSE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	MO disk ejection UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOEjectUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// If no disk, prohibit
	if (!m_pSASI->IsReady()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// If locked, prohibit
	if (m_pSASI->IsLocked()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Enable
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	MO write protect
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOWriteP()
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// VM lock and execute
	::LockVM();
	m_pSASI->WriteP(!m_pSASI->IsWriteP());
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	MO write protect UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOWritePUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// If no disk, cannot check
	if (!m_pSASI->IsReady()) {
		pCmdUI->SetCheck(0);
		pCmdUI->Enable(FALSE);
		return;
	}

	// If read only, cannot check
	if (m_pSASI->IsReadOnly()) {
		pCmdUI->SetCheck(1);
		pCmdUI->Enable(FALSE);
		return;
	}

	// Check according to write protect, enable
	pCmdUI->SetCheck(m_pSASI->IsWriteP());
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	MO forced ejection
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOForce()
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// VM lock and execute
	::LockVM();
	m_pSASI->Eject(TRUE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	MO forced ejection UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOForceUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// If no disk, prohibit
	if (!m_pSASI->IsReady()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// If not locked, prohibit
	if (!m_pSASI->IsLocked()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Enable
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	MO MRU
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOMRU(UINT uID)
{
	int nMRU;
	TCHAR szMRU[_MAX_PATH];
	Filepath path;
	BOOL bResult;

	ASSERT(this);
	ASSERT(m_pSASI);
	ASSERT((uID >= IDM_MO_MRU0) && (uID <= IDM_MO_MRU8));

	// MRU index
	nMRU = uID - IDM_MO_MRU0;
	ASSERT((nMRU >= 0) && (nMRU <= 8));

	// Get MRU
	GetConfig()->GetMRUFile(2, nMRU, szMRU);
	if (szMRU[0] == _T('\0')) {
		return;
	}
	path.SetPath(szMRU);

	// VM lock
	::LockVM();

	// Open
	bResult = m_pSASI->Open(path);
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// If successful, update directory and add to MRU
	if (bResult) {
		// Update default directory
		Filepath::SetDefaultDir(szMRU);

		// Add to MRU
		GetConfig()->SetMRUFile(2, szMRU);
	}
}

//---------------------------------------------------------------------------
//
//	MO MRU UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOMRUUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// Disabled if window not enabled
	if (!m_pSASI->IsValid()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// If locked, prohibit
	if (m_pSASI->IsLocked()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Enable
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	CD-ROM open
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDOpen()
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];
	CString strMsg;

	ASSERT(this);
	ASSERT(m_pSCSI);

	// Execute common dialog
	memset(szPath, 0, sizeof(szPath));
	if (!::FileOpenDlg(this, szPath, IDS_CDOPEN)) {
		ResetCaption();
		return;
	}
	path.SetPath(szPath);

	// VM lock
	::LockVM();

	// Assign CD disk
	if (!m_pSCSI->Open(path, FALSE)) {
		GetScheduler()->Reset();
		::UnlockVM();

		// Open error
		::GetMsg(IDS_CDERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		ResetCaption();
		return;
	}

	// Success
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// Add to MRU
	GetConfig()->SetMRUFile(3, szPath);
}

//---------------------------------------------------------------------------
//
//	CD-ROM open UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDOpenUI(CCmdUI *pCmdUI)
{
	int i;
	BOOL bValid;
	BOOL bReady;
	BOOL bLocked;
	TCHAR szMRU[_MAX_PATH];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	CMenu *pMenu;
	CMenu *pSubMenu;
	UINT nEnable;

	ASSERT(this);
	ASSERT(m_pSCSI);

	// Get window status (under lock)
	::LockVM();
	bValid = m_pSCSI->IsValid(FALSE);
	bReady = m_pSCSI->IsReady(FALSE);
	bLocked = m_pSCSI->IsLocked(FALSE);
	::UnlockVM();

	// Open
	if (bValid) {
		if (bReady && bLocked) {
			pCmdUI->Enable(FALSE);
		}
		else {
			pCmdUI->Enable(TRUE);
		}
	}
	else {
		pCmdUI->Enable(FALSE);
	}

	// Get submenu
	if (m_bPopupMenu) {
		pMenu = m_PopupMenu.GetSubMenu(0);
	}
	else {
		pMenu = &m_Menu;
	}
	ASSERT(pMenu);

	// CD menu follows File, Floppy 0, Floppy 1, MO
	pSubMenu = pMenu->GetSubMenu(4);
	ASSERT(pSubMenu);

	// Ejection UI (workaround for ON_UPDATE_COMMAND_UI timing)
	if (bReady && !bLocked) {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_GRAYED);
	}

	// Forced ejection UI
	if (bReady && bLocked) {
		pSubMenu->EnableMenuItem(3, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(3, MF_BYPOSITION | MF_GRAYED);
	}

	// Delete subsequent menus
	while (pSubMenu->GetMenuItemCount() > 4) {
		pSubMenu->RemoveMenu(4, MF_BYPOSITION);
	}

	// MRU processing - Separator
	if (GetConfig()->GetMRUNum(3) == 0) {
		return;
	}
	pSubMenu->AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);

	// On/off constants configuration
	nEnable = MF_BYCOMMAND | MF_GRAYED;
	if (bValid) {
		// Window enabled
		if (!bReady || !bLocked) {
			// If no media or not locked, can insert
			nEnable = MF_BYCOMMAND | MF_ENABLED;
		}
	}

	// MRU processing - Add
	for (i=0; i<9; i++) {
		// Try to get
		GetConfig()->GetMRUFile(3, i, szMRU);
		if (szMRU[0] == _T('\0')) {
			break;
		}

		// Add to menu
		_tsplitpath(szMRU, szDrive, szDir, szFile, szExt);
		if (_tcslen(szDir) > 1) {
			_tcscpy(szDir, _T("\\...\\"));
		}
		_stprintf(szMRU, _T("&%d "), i + 1);
		_tcscat(szMRU, szDrive);
		_tcscat(szMRU, szDir);
		_tcscat(szMRU, szFile);
		_tcscat(szMRU, szExt);

		pSubMenu->AppendMenu(MF_STRING, IDM_CD_MRU0 + i, szMRU);
		pSubMenu->EnableMenuItem(IDM_CD_MRU0 + i, nEnable);
	}
}

//---------------------------------------------------------------------------
//
//	CD-ROM ejection
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDEject()
{
	ASSERT(this);
	ASSERT(m_pSCSI);

	// VM lock and execute
	::LockVM();
	m_pSCSI->Eject(FALSE, FALSE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	CD-ROM ejection UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDEjectUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSCSI);

	// If no disk, prohibit
	if (!m_pSCSI->IsReady(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// If locked, prohibit
	if (m_pSCSI->IsLocked(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Enable
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	CD-ROM forced ejection
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDForce()
{
	ASSERT(this);
	ASSERT(m_pSCSI);

	// VM lock and execute
	::LockVM();
	m_pSCSI->Eject(TRUE, FALSE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	CD-ROM forced ejection UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDForceUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSCSI);

	// If no disk, prohibit
	if (!m_pSCSI->IsReady(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// If not locked, prohibit
	if (!m_pSCSI->IsLocked(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Enable
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	CD-ROM MRU
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDMRU(UINT uID)
{
	int nMRU;
	TCHAR szMRU[_MAX_PATH];
	Filepath path;
	BOOL bResult;

	ASSERT(this);
	ASSERT(m_pSCSI);
	ASSERT((uID >= IDM_CD_MRU0) && (uID <= IDM_CD_MRU8));

	// MRU index
	nMRU = uID - IDM_CD_MRU0;
	ASSERT((nMRU >= 0) && (nMRU <= 8));

	// Get MRU
	GetConfig()->GetMRUFile(3, nMRU, szMRU);
	if (szMRU[0] == _T('\0')) {
		return;
	}
	path.SetPath(szMRU);

	// VM lock
	::LockVM();

	// Open
	bResult = m_pSCSI->Open(path, FALSE);
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// If successful, update directory and add to MRU
	if (bResult) {
		// Update default directory
		Filepath::SetDefaultDir(szMRU);

		// Add to MRU
		GetConfig()->SetMRUFile(3, szMRU);
	}
}

//---------------------------------------------------------------------------
//
//	CD-ROM MRU UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDMRUUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSCSI);

	// Disabled if window not enabled
	if (!m_pSCSI->IsValid(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// If locked, prohibit
	if (m_pSCSI->IsLocked(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Enable
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	Macro for subwindow command/UI
//
//---------------------------------------------------------------------------
#define ON_SUB_WINDOW(id, wndcls)	do { \
								CSubWnd *pWnd = GetView()->SearchSWnd(id); \
								if (pWnd) { pWnd->DestroyWindow(); return; } \
								wndcls *pNewWnd = new wndcls; \
								pNewWnd->Init(GetView()); \
								} while (0)
#define ON_UPDATE_SUB_WINDOW(id)	if (GetView()->SearchSWnd(id)) pCmdUI->SetCheck(1); \
								else pCmdUI->SetCheck(0);

//---------------------------------------------------------------------------
//
//	Log
//
//---------------------------------------------------------------------------
void CFrmWnd::OnLog()
{
	ON_SUB_WINDOW(MAKEID('L', 'O', 'G', 'L'), CLogWnd);
}

//---------------------------------------------------------------------------
//
//	Log UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnLogUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('L', 'O', 'G', 'L'));
}

//---------------------------------------------------------------------------
//
//	Scheduler
//
//---------------------------------------------------------------------------
void CFrmWnd::OnScheduler()
{
	ON_SUB_WINDOW(MAKEID('S', 'C', 'H', 'E'), CSchedulerWnd);
}

//---------------------------------------------------------------------------
//
//	Scheduler UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSchedulerUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'C', 'H', 'E'));
}

//---------------------------------------------------------------------------
//
//	Device
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDevice()
{
	ON_SUB_WINDOW(MAKEID('D', 'E', 'V', 'I'), CDeviceWnd);
}

//---------------------------------------------------------------------------
//
//	Device UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDeviceUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('D', 'E', 'V', 'I'));
}

//---------------------------------------------------------------------------
//
//	CPU registers
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCPUReg()
{
	ON_SUB_WINDOW(MAKEID('M', 'P', 'U', 'R'), CCPURegWnd);
}

//---------------------------------------------------------------------------
//
//	CPU registers UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCPURegUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('M', 'P', 'U', 'R'));
}

//---------------------------------------------------------------------------
//
//	Interrupt
//
//---------------------------------------------------------------------------
void CFrmWnd::OnInt()
{
	ON_SUB_WINDOW(MAKEID('I', 'N', 'T', ' '), CIntWnd);
}

//---------------------------------------------------------------------------
//
//	Interrupt UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnIntUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('I', 'N', 'T', ' '));
}

//---------------------------------------------------------------------------
//
//	Disassemble
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDisasm()
{
	CDisasmWnd *pWnd;
	int i;
	BOOL flag;

	// Check 8 types
	flag = FALSE;
	for (i=0; i<8; i++) {
		pWnd = (CDisasmWnd*)GetView()->SearchSWnd(MAKEID('D', 'I', 'S', 'A' + i));
		if (pWnd) {
			pWnd->PostMessage(WM_CLOSE, 0, 0);
			flag = TRUE;
		}
	}

	// New creation
	if (!flag) {
		pWnd = new CDisasmWnd(0);
		VERIFY(pWnd->Init(GetView()));
	}
}

//---------------------------------------------------------------------------
//
//	Disassemble UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDisasmUI(CCmdUI *pCmdUI)
{
	CSubWnd *pSubWnd;
	int i;

	// Check 8 types
	for (i=0; i<8; i++) {
		pSubWnd = GetView()->SearchSWnd(MAKEID('D', 'I', 'S', 'A' + i));
		if (pSubWnd) {
			pCmdUI->SetCheck(1);
			return;
		}
	}

	pCmdUI->SetCheck(0);
}

//---------------------------------------------------------------------------
//
//	Memory
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMemory()
{
	CMemoryWnd *pWnd;
	int i;
	BOOL flag;

	// Check 8 types
	flag = FALSE;
	for (i=0; i<8; i++) {
		pWnd = (CMemoryWnd*)GetView()->SearchSWnd(MAKEID('M', 'E', 'M', 'A' + i));
		if (pWnd) {
			pWnd->PostMessage(WM_CLOSE, 0, 0);
			flag = TRUE;
		}
	}

	// New creation
	if (!flag) {
		pWnd = new CMemoryWnd(0);
		VERIFY(pWnd->Init(GetView()));
	}
}

//---------------------------------------------------------------------------
//
//	Memory UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMemoryUI(CCmdUI *pCmdUI)
{
	CSubWnd *pSubWnd;
	int i;

	// Check 8 types
	for (i=0; i<8; i++) {
		pSubWnd = GetView()->SearchSWnd(MAKEID('M', 'E', 'M', 'A' + i));
		if (pSubWnd) {
			pCmdUI->SetCheck(1);
			return;
		}
	}

	pCmdUI->SetCheck(0);
}

//---------------------------------------------------------------------------
//
//	Breakpoint
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBreakP()
{
	ON_SUB_WINDOW(MAKEID('B', 'R', 'K', 'P'), CBreakPWnd);
}

//---------------------------------------------------------------------------
//
//	Breakpoint UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBreakPUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('B', 'R', 'K', 'P'));
}

//---------------------------------------------------------------------------
//
//	MFP
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMFP()
{
	ON_SUB_WINDOW(MAKEID('M', 'F', 'P', ' '), CMFPWnd);
}

//---------------------------------------------------------------------------
//
//	MFP UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMFPUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('M', 'F', 'P', ' '));
}

//---------------------------------------------------------------------------
//
//	DMAC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDMAC()
{
	ON_SUB_WINDOW(MAKEID('D', 'M', 'A', 'C'), CDMACWnd);
}

//---------------------------------------------------------------------------
//
//	DMAC UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDMACUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('D', 'M', 'A', 'C'));
}

//---------------------------------------------------------------------------
//
//	CRTC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCRTC()
{
	ON_SUB_WINDOW(MAKEID('C', 'R', 'T', 'C'), CCRTCWnd);
}

//---------------------------------------------------------------------------
//
//	CRTC UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCRTCUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('C', 'R', 'T', 'C'));
}

//---------------------------------------------------------------------------
//
//	VC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnVC()
{
	ON_SUB_WINDOW(MAKEID('V', 'C', ' ', ' '), CVCWnd);
}

//---------------------------------------------------------------------------
//
//	VC UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnVCUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('V', 'C', ' ', ' '));
}

//---------------------------------------------------------------------------
//
//	RTC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRTC()
{
	ON_SUB_WINDOW(MAKEID('R', 'T', 'C', ' '), CRTCWnd);
}

//---------------------------------------------------------------------------
//
//	RTC UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRTCUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('R', 'T', 'C', ' '));
}

//---------------------------------------------------------------------------
//
//	OPM
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOPM()
{
	ON_SUB_WINDOW(MAKEID('O', 'P', 'M', ' '), COPMWnd);
}

//---------------------------------------------------------------------------
//
//	OPM UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOPMUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('O', 'P', 'M', ' '));
}

//---------------------------------------------------------------------------
//
//	Keyboard
//
//---------------------------------------------------------------------------
void CFrmWnd::OnKeyboard()
{
	ON_SUB_WINDOW(MAKEID('K', 'E', 'Y', 'B'), CKeyboardWnd);
}

//---------------------------------------------------------------------------
//
//	Keyboard UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnKeyboardUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('K', 'E', 'Y', 'B'));
}

//---------------------------------------------------------------------------
//
//	FDD
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDD()
{
	ON_SUB_WINDOW(MAKEID('F', 'D', 'D', ' '), CFDDWnd);
}

//---------------------------------------------------------------------------
//
//	FDD UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDDUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('F', 'D', 'D', ' '));
}

//---------------------------------------------------------------------------
//
//	FDC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDC()
{
	ON_SUB_WINDOW(MAKEID('F', 'D', 'C', ' '), CFDCWnd);
}

//---------------------------------------------------------------------------
//
//	FDC UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDCUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('F', 'D', 'C', ' '));
}

//---------------------------------------------------------------------------
//
//	SCC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSCC()
{
	ON_SUB_WINDOW(MAKEID('S', 'C', 'C', ' '), CSCCWnd);
}

//---------------------------------------------------------------------------
//
//	SCC UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSCCUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'C', 'C', ' '));
}

//---------------------------------------------------------------------------
//
//	CYNTHIA
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCynthia()
{
	ON_SUB_WINDOW(MAKEID('C', 'Y', 'N', 'T'), CCynthiaWnd);
}

//---------------------------------------------------------------------------
//
//	CYNTHIA UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCynthiaUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('C', 'Y', 'N', 'T'));
}

//---------------------------------------------------------------------------
//
//	SASI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSASI()
{
	ON_SUB_WINDOW(MAKEID('S', 'A', 'S', 'I'), CSASIWnd);
}

//---------------------------------------------------------------------------
//
//	SASI UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSASIUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'A', 'S', 'I'));
}

//---------------------------------------------------------------------------
//
//	MIDI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMIDI()
{
	ON_SUB_WINDOW(MAKEID('M', 'I', 'D', 'I'), CMIDIWnd);
}

//---------------------------------------------------------------------------
//
//	MIDI UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMIDIUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('M', 'I', 'D', 'I'));
}

//---------------------------------------------------------------------------
//
//	SCSI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSCSI()
{
	ON_SUB_WINDOW(MAKEID('S', 'C', 'S', 'I'), CSCSIWnd);
}

//---------------------------------------------------------------------------
//
//	SCSI UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSCSIUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'C', 'S', 'I'));
}

//---------------------------------------------------------------------------
//
//	Text screen
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTVRAM()
{
	ON_SUB_WINDOW(MAKEID('T', 'V', 'R', 'M'), CTVRAMWnd);
}

//---------------------------------------------------------------------------
//
//	Text screen UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTVRAMUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('T', 'V', 'R', 'M'));
}

//---------------------------------------------------------------------------
//
//	Graphic screen 1024x1024
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG1024()
{
	ON_SUB_WINDOW(MAKEID('G', '1', '0', '2'), CG1024Wnd);
}

//---------------------------------------------------------------------------
//
//	Graphic screen 1024x1024 UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG1024UI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('G', '1', '0', '2'));
}

//---------------------------------------------------------------------------
//
//	16 color graphic screen
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG16(UINT uID)
{
	CG16Wnd *pWnd;
	int index;

	// Accept index
	index = (int)(uID - IDM_G16P0);
	ASSERT((index >= 0) || (index <= 3));

	// If already exists, close
	pWnd = (CG16Wnd*)GetView()->SearchSWnd(MAKEID('G', '1', '6', ('A' + index)));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// Create subwindow
	pWnd = new CG16Wnd(index);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	16 color graphic screen UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG16UI(CCmdUI *pCmdUI)
{
	int index;
	CSubWnd *pSubWnd;

	// Accept index
	index = (int)(pCmdUI->m_nID - IDM_G16P0);
	ASSERT((index >= 0) || (index <= 3));

	pSubWnd = GetView()->SearchSWnd(MAKEID('G', '1', '6', ('A' + index)));
	if (pSubWnd) {
		pCmdUI->SetCheck(1);
	}
	else {
		pCmdUI->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	256 color graphic screen
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG256(UINT uID)
{
	CG256Wnd *pWnd;
	int index;

	// Accept index
	index = (int)(uID - IDM_G256P0);
	ASSERT((index == 0) || (index == 1));

	// If already exists, close
	pWnd = (CG256Wnd*)GetView()->SearchSWnd(MAKEID('G', '2', '5', ('A' + index)));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// Create subwindow
	pWnd = new CG256Wnd(index);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	256 color graphic screen UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG256UI(CCmdUI *pCmdUI)
{
	int index;
	CSubWnd *pSubWnd;

	// Accept index
	index = (int)(pCmdUI->m_nID - IDM_G256P0);
	ASSERT((index == 0) || (index == 1));

	pSubWnd = GetView()->SearchSWnd(MAKEID('G', '2', '5', ('A' + index)));
	if (pSubWnd) {
		pCmdUI->SetCheck(1);
	}
	else {
		pCmdUI->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	65536 color graphic screen
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG64K()
{
	CG64KWnd *pWnd;

	// If already exists, close
	pWnd = (CG64KWnd*)GetView()->SearchSWnd(MAKEID('G', '6', '4', 'K'));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// Create subwindow
	pWnd = new CG64KWnd;
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	65536 color graphic screen UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG64KUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('G', '6', '4', 'K'));
}

//---------------------------------------------------------------------------
//
//	PCG
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPCG()
{
	CPCGWnd *pWnd;

	// If already exists, close
	pWnd = (CPCGWnd*)GetView()->SearchSWnd(MAKEID('P', 'C', 'G', ' '));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// Create subwindow
	pWnd = new CPCGWnd;
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	PCG UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPCGUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('P', 'C', 'G', ' '));
}

//---------------------------------------------------------------------------
//
//	BG screen
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBG(UINT uID)
{
	CBGWnd *pWnd;
	int index;

	// Accept index
	index = (int)(uID - IDM_BG0);
	ASSERT((index == 0) || (index == 1));

	// If already exists, close
	pWnd = (CBGWnd*)GetView()->SearchSWnd(MAKEID('B', 'G', ('0' + index), ' '));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// Create subwindow
	pWnd = new CBGWnd(index);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	BG screen UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBGUI(CCmdUI *pCmdUI)
{
	int index;
	CSubWnd *pSubWnd;

	// Accept index
	index = (int)(pCmdUI->m_nID - IDM_BG0);
	ASSERT((index == 0) || (index == 1));

	pSubWnd = GetView()->SearchSWnd(MAKEID('B', 'G', ('0' + index), ' '));
	if (pSubWnd) {
		pCmdUI->SetCheck(1);
	}
	else {
		pCmdUI->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	Palette
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPalet()
{
	CPaletteWnd *pWnd;

	// If already exists, close
	pWnd = (CPaletteWnd*)GetView()->SearchSWnd(MAKEID('P', 'A', 'L', ' '));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// Create subwindow
	pWnd = new CPaletteWnd(FALSE);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	Palette UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPaletUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('P', 'A', 'L', ' '));
}

//---------------------------------------------------------------------------
//
//	Text buffer
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTextBuf()
{
	CRendBufWnd *pWnd;

	// If already exists, close
	pWnd = (CRendBufWnd*)GetView()->SearchSWnd(MAKEID('T', 'E', 'X', 'B'));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// Create subwindow
	pWnd = new CRendBufWnd(0);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	Text buffer UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTextBufUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('T', 'E', 'X', 'B'));
}

//---------------------------------------------------------------------------
//
//	Graphic buffer
//
//---------------------------------------------------------------------------
void CFrmWnd::OnGrpBuf(UINT uID)
{
	CRendBufWnd *pWnd;
	int index;

	// Accept index
	index = (int)(uID - IDM_REND_GP0);
	ASSERT((index >= 0) || (index <= 4));

	// If already exists, close
	pWnd = (CRendBufWnd*)GetView()->SearchSWnd(MAKEID('G', 'P', ('0' + index), 'B'));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// Create subwindow
	pWnd = new CRendBufWnd(index + 1);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	Graphic buffer UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnGrpBufUI(CCmdUI *pCmdUI)
{
	int index;
	CSubWnd *pSubWnd;

	// Accept index
	index = (int)(pCmdUI->m_nID - IDM_REND_GP0);
	ASSERT((index >= 0) || (index <= 4));

	pSubWnd = GetView()->SearchSWnd(MAKEID('G', 'P', ('0' + index), 'B'));
	if (pSubWnd) {
		pCmdUI->SetCheck(1);
	}
	else {
		pCmdUI->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	PCG buffer
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPCGBuf()
{
	ON_SUB_WINDOW(MAKEID('P', 'C', 'G', 'B'), CPCGBufWnd);
}

//---------------------------------------------------------------------------
//
//	PCG buffer UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPCGBufUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('P', 'C', 'G', 'B'));
}

//---------------------------------------------------------------------------
//
//	BG/Sprite buffer
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBGSpBuf()
{
	CRendBufWnd *pWnd;

	// If already exists, close
	pWnd = (CRendBufWnd*)GetView()->SearchSWnd(MAKEID('B', 'G', 'S', 'P'));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// Create subwindow
	pWnd = new CRendBufWnd(5);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	BG/Sprite buffer UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBGSpBufUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('B', 'G', 'S', 'P'));
}

//---------------------------------------------------------------------------
//
//	Palette buffer
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPaletBuf()
{
	CPaletteWnd *pWnd;

	// If already exists, close
	pWnd = (CPaletteWnd*)GetView()->SearchSWnd(MAKEID('P', 'A', 'L', 'B'));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// Create subwindow
	pWnd = new CPaletteWnd(TRUE);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	Palette buffer UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPaletBufUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('P', 'A', 'L', 'B'));
}

//---------------------------------------------------------------------------
//
//	Mix buffer
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMixBuf()
{
	ON_SUB_WINDOW(MAKEID('M', 'I', 'X', 'B'), CMixBufWnd);
}

//---------------------------------------------------------------------------
//
//	Mix buffer UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMixBufUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('M', 'I', 'X', 'B'));
}

//---------------------------------------------------------------------------
//
//	Component
//
//---------------------------------------------------------------------------
void CFrmWnd::OnComponent()
{
	ON_SUB_WINDOW(MAKEID('C', 'O', 'M', 'P'), CComponentWnd);
}

//---------------------------------------------------------------------------
//
//	Component UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnComponentUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('C', 'O', 'M', 'P'));
}

//---------------------------------------------------------------------------
//
//	OS information
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOSInfo()
{
	ON_SUB_WINDOW(MAKEID('O', 'S', 'I', 'N'), COSInfoWnd);
}

//---------------------------------------------------------------------------
//
//	OS information UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOSInfoUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('O', 'S', 'I', 'N'));
}

//---------------------------------------------------------------------------
//
//	Sound
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSound()
{
	ON_SUB_WINDOW(MAKEID('S', 'N', 'D', ' '), CSoundWnd);
}

//---------------------------------------------------------------------------
//
//	Sound UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSoundUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'N', 'D', ' '));
}

//---------------------------------------------------------------------------
//
//	Input
//
//---------------------------------------------------------------------------
void CFrmWnd::OnInput()
{
	ON_SUB_WINDOW(MAKEID('I', 'N', 'P', ' '), CInputWnd);
}

//---------------------------------------------------------------------------
//
//	Input UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnInputUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('I', 'N', 'P', ' '));
}

//---------------------------------------------------------------------------
//
//	Ports
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPort()
{
	ON_SUB_WINDOW(MAKEID('P', 'O', 'R', 'T'), CPortWnd);
}

//---------------------------------------------------------------------------
//
//	Ports UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPortUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('P', 'O', 'R', 'T'));
}

//---------------------------------------------------------------------------
//
//	Bitmap
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBitmap()
{
	ON_SUB_WINDOW(MAKEID('B', 'M', 'A', 'P'), CBitmapWnd);
}

//---------------------------------------------------------------------------
//
//	Bitmap UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBitmapUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('B', 'M', 'A', 'P'));
}

//---------------------------------------------------------------------------
//
//	MIDI controller (Driver)
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMIDIDrv()
{
	ON_SUB_WINDOW(MAKEID('M', 'D', 'R', 'V'), CMIDIDrvWnd);
}

//---------------------------------------------------------------------------
//
//	MIDI controller UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMIDIDrvUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('M', 'D', 'R', 'V'));
}

//---------------------------------------------------------------------------
//
//	Window title
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCaption()
{
	// Toggle flag
	m_bCaption = !m_bCaption;

	// Show or hide caption
	ShowCaption();
}

//---------------------------------------------------------------------------
//
//	Window title UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCaptionUI(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bCaption);
}

//---------------------------------------------------------------------------
//
//	Menu bar
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMenu()
{
	// Toggle flag
	m_bMenuBar = !m_bMenuBar;

	// Show or hide menu
	ShowMenu();
}

//---------------------------------------------------------------------------
//
//	Menu bar UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMenuUI(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bMenuBar);
}

//---------------------------------------------------------------------------
//
//	Status bar
//
//---------------------------------------------------------------------------
void CFrmWnd::OnStatus()
{
	// Toggle flag
	m_bStatusBar = !m_bStatusBar;

	// Show or hide status
	ShowStatus();
}

//---------------------------------------------------------------------------
//
//	Status bar UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnStatusUI(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bStatusBar);
}

//---------------------------------------------------------------------------
//
//	Refresh
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRefresh()
{
	// Lock
	::LockVM();

	// Draw
	GetView()->Refresh();

	// Unlock
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Window scale
//
//---------------------------------------------------------------------------
void CFrmWnd::OnWindowScale(UINT uID)
{
	int nScaleIndex;

	if (uID < IDM_SCALE_100) {
		return;
	}
	nScaleIndex = (int)(uID - IDM_SCALE_100);
	if (nScaleIndex < 0) {
		nScaleIndex = 0;
	}
	if (nScaleIndex > 4) {
		nScaleIndex = 4;
	}

	// Lock VM
	::LockVM();

	GetView()->SetScaleIndex(nScaleIndex);
	GetConfig()->SetWindowScale(nScaleIndex);

	// Unlock VM
	::UnlockVM();

	// Keep the window size in sync when not fullscreen.
	// Do this outside the VM lock to avoid stalling audio while DX9 resets.
	ApplyWindowScale();
}

//---------------------------------------------------------------------------
//
//	Window scale UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnWindowScaleUI(CCmdUI *pCmdUI)
{
	if (!pCmdUI) {
		return;
	}

	const int nScaleIndex = GetView()->GetScaleIndex();
	const int nItemIndex = (int)(pCmdUI->m_nID - IDM_SCALE_100);
	pCmdUI->SetCheck(nScaleIndex == nItemIndex);
}

//---------------------------------------------------------------------------
//
//	Full screen event
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFullScreen()
{
	BOOL bEnable;
	BOOL bSound;
	BOOL bMouse;

	// Stop emulation and sound briefly for layout change
	bEnable = GetScheduler()->IsEnable();
	bMouse = GetInput()->GetMouseMode();
	if (bMouse) {
		GetInput()->SetMouseMode(FALSE);
	}
	GetScheduler()->Enable(FALSE);
	::LockVM();
	::UnlockVM();
	::LockVM();
	bSound = GetSound()->IsEnable();
	GetSound()->Enable(FALSE);
	::UnlockVM();

	if (m_bFullScreen) {
		// Exit full screen (Window mode)
		ExitBorderlessFullscreen();
		m_bFullScreen = FALSE;

		ShowCaption();
		ShowMenu();
		ShowStatus();

		HideTaskBar(FALSE, TRUE);
		InitPos(FALSE);
		RecalcLayout();
	}
	else {
		// Enter full screen (Borderless)
		EnterBorderlessFullscreen();
		m_bFullScreen = TRUE;

		ShowCaption();
		ShowMenu();

		HideTaskBar(TRUE, TRUE);
		InitPos(FALSE);
		RecalcStatusView();
	}

	// Restore states
	GetScheduler()->Enable(bEnable);
	GetSound()->Enable(bSound);
	GetInput()->SetMouseMode(bMouse);
	ResetCaption();
	ResetStatus();

	// Auto-capture mouse if necessary
	if (m_bAutoMouse && bEnable && !bMouse && m_bFullScreen) {
		OnMouseMode();
	}
}


//---------------------------------------------------------------------------
//
//	Full screen UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFullScreenUI(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bFullScreen);
}

//---------------------------------------------------------------------------
//
//	Execute
//
//---------------------------------------------------------------------------
void CFrmWnd::OnExec()
{
	// Lock
	::LockVM();

	// Enable scheduler
	GetScheduler()->Reset();
	GetScheduler()->Enable(TRUE);
	ResetCaption();

	// Unlock
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Execute UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnExecUI(CCmdUI *pCmdUI)
{
	// Enable if scheduler stopped and power on
	if ((!GetScheduler()->IsEnable()) && ::GetVM()->IsPower()) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Stop
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBreak()
{
	// Lock
	::LockVM();

	// Disable scheduler
	GetScheduler()->Enable(FALSE);

	// Unlock
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Stop UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBreakUI(CCmdUI *pCmdUI)
{
	// Enable if scheduler is running
	if (GetScheduler()->IsEnable()) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Trace
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTrace()
{
	// Lock
	::LockVM();

	// Trace
	::GetVM()->Trace();
	GetScheduler()->SyncDisasm();

	// Unlock
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Trace UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTraceUI(CCmdUI *pCmdUI)
{
	// Enable if scheduler stopped and power on
	if ((!GetScheduler()->IsEnable()) && ::GetVM()->IsPower()) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Mouse mode
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMouseMode()
{
	CRect rect;
	BOOL b;
	int cnt;
	CString string;
	LONG cx;
	LONG cy;

	// Get current mouse mode
	b = GetInput()->GetMouseMode();

	// During minimization and with scheduler off, only turn off works
	if (!b) {
		// Cannot turn on if minimized
		if (IsIconic()) {
			return;
		}
		// Cannot turn on if scheduler is off
		if (!GetScheduler()->IsEnable()) {
			return;
		}
		// Cannot turn on if not active
		if (!GetInput()->IsActive()) {
			return;
		}
	}

	if (b) {
		// To mouse disabled mode
		GetInput()->SetMouseMode(FALSE);

		// Undo clip (release mouse)
		ClipCursor(NULL);

		// Move cursor to center of screen
		cx = ::GetSystemMetrics(SM_CXSCREEN);
		cy = ::GetSystemMetrics(SM_CYSCREEN);
		SetCursorPos(cx >> 1, cy >> 1);

		// Show cursor
		cnt = ::ShowCursor(TRUE);
		while (cnt < 0) {
			cnt = ::ShowCursor(TRUE);
		}

		// Set info message
		::GetMsg(IDS_MOUSE_WIN, string);
	}
	else {
		// Mouse cursor window is View window + 16
		GetView()->GetWindowRect(&rect);
		rect.right = rect.left + 16;
		rect.bottom = rect.top + 16;
		ClipCursor(&rect);

		// Hide mouse cursor
		cnt = ::ShowCursor(FALSE);
		while (cnt >= 0) {
			cnt = ::ShowCursor(FALSE);
		}

		// To mouse enabled mode
		GetInput()->SetMouseMode(TRUE);

		// Set info message
		::GetMsg(IDS_MOUSE_X68K, string);
	}

	// Update info message
	SetInfo(string);
}

//---------------------------------------------------------------------------
//
//	Software keyboard
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSoftKey()
{
	ON_SUB_WINDOW(MAKEID('S', 'K', 'E', 'Y'), CSoftKeyWnd);
}

//---------------------------------------------------------------------------
//
//	Software keyboard UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSoftKeyUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'K', 'E', 'Y'));

	// If keyboard device does not exist, do nothing
	if (!m_pKeyboard) {
		return;
	}

	// If subwindow does not exist
	if (GetView()->SearchSWnd(MAKEID('S', 'K', 'E', 'Y')) == NULL) {
		// If keyboard not connected
		if (!m_pKeyboard->IsConnect()) {
			// Prohibit
			pCmdUI->Enable(FALSE);
			return;
		}
	}

	// Enabled
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	Time adjustment
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTimeAdj()
{
	RTC *pRTC;

	// Called from pRTC->Adjust()
	::LockVM();
	pRTC = (RTC*)::GetVM()->SearchDevice(MAKEID('R', 'T', 'C', ' '));
	ASSERT(pRTC);
	pRTC->Adjust(FALSE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	WAV capture
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSaveWav()
{
	TCHAR szPath[_MAX_PATH];
	CString strMsg;
	BOOL bResult;

	// Stop if saving WAV
	if (GetSound()->IsSaveWav()) {
		GetSound()->EndSaveWav();
		::GetMsg(IDS_WAVSTOP, strMsg);
		SetInfo(strMsg);
		return;
	}

	// File save dialog
	szPath[0] = _T('\0');
	if (!FileSaveDlg(this, szPath, _T("wav"), IDS_WAVOPEN)) {
		ResetCaption();
		return;
	}

	// Try WAV save
	::LockVM();
	bResult = GetSound()->StartSaveWav(szPath);
	::UnlockVM();

	// Result notification
	if (bResult) {
		::GetMsg(IDS_WAVSTART, strMsg);
		SetInfo(strMsg);
	}
	else {
		::GetMsg(IDS_CREATEERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
	}
}

//---------------------------------------------------------------------------
//
//	WAV capture UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSaveWavUI(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSound()->IsPlay());
	pCmdUI->SetCheck(GetSound()->IsSaveWav());
}

//---------------------------------------------------------------------------
//
//	trap#0
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTrap()
{
	CTrapDlg dlg(this);
	DWORD dwVector;
	MFP *pMFP;
	Memory *pMemory;
	DWORD dwAddr;
	DWORD dwCode;

	// Get device
	pMFP = (MFP*)::GetVM()->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(pMFP);
	pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(pMemory);

	// Lock
	::LockVM();

	// Get MFP interrupt vector
	dwVector = (pMFP->GetVR() & 0xf0) + 5;
	dwVector <<= 2;

	// Get current Timer-C vector address
	dwAddr = pMemory->ReadOnly(dwVector);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 1);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 2);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 3);

	// Invalid if not initialized (24 bits or more)
	if (dwAddr > 0xffffff) {
		::UnlockVM();
		return;
	}

	// Invalid if already 0x6800
	if (dwAddr == 0x6800) {
		::UnlockVM();
		return;
	}

	// Get code
	dlg.m_dwCode = pMemory->ReadOnly(0x6809);
	if (pMemory->ReadOnly(0x6808) == 0xff) {
		// Considered not initialized, start from 0
		dlg.m_dwCode = 1;
	}
	::UnlockVM();
	if (dlg.DoModal() != IDOK) {
		return;
	}
	dwCode = dlg.m_dwCode;

	// Write code
	::LockVM();
	pMemory->WriteWord(0x6800, 0x2f00);
	pMemory->WriteWord(0x6802, 0x2f00);
	pMemory->WriteWord(0x6804, 0x203c);
	pMemory->WriteWord(0x6806, dwCode >> 16);
	pMemory->WriteWord(0x6808, (WORD)dwCode);
	pMemory->WriteWord(0x680a, 0x4e40);
	pMemory->WriteWord(0x680c, 0x203c);
	pMemory->WriteWord(0x680e, dwAddr >> 16);
	pMemory->WriteWord(0x6810, (WORD)dwAddr);
	pMemory->WriteWord(0x6812, 0x21c0);
	pMemory->WriteWord(0x6814, dwVector);
	pMemory->WriteWord(0x6816, 0x2f40);
	pMemory->WriteWord(0x6818, 0x0004);
	pMemory->WriteWord(0x681a, 0x201f);
	pMemory->WriteWord(0x681c, 0x4e75);

	// Change Timer-C vector
	pMemory->WriteWord(dwVector, 0x0000);
	pMemory->WriteWord(dwVector + 2, 0x6800);

	// Unlock
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	trap#0 UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTrapUI(CCmdUI *pCmdUI)
{
	DWORD dwVector;
	MFP *pMFP;
	Memory *pMemory;
	DWORD dwAddr;

	// Get device
	pMFP = (MFP*)::GetVM()->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(pMFP);
	pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(pMemory);

	// Get MFP interrupt vector
	dwVector = (pMFP->GetVR() & 0xf0) + 5;
	dwVector <<= 2;

	// Get current Timer-C vector address
	dwAddr = pMemory->ReadOnly(dwVector);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 1);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 2);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 3);

	// Prohibit if not initialized (24 bits or more)
	if (dwAddr > 0xffffff) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Prohibit if already 0x6800
	if (dwAddr == 0x6800) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Get trap #0 vector
	dwAddr = pMemory->ReadOnly(0x0080);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(0x0081);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(0x0082);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(0x0083);

	// Prohibit if not initialized (24 bits or more)
	if (dwAddr > 0xffffff) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Enable
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	New floppy disk
//
//---------------------------------------------------------------------------
void CFrmWnd::OnNewFD()
{
	CFDIDlg dlg(this);
	FDIDisk *pDisk;
	FDIDisk::option_t opt;
	CString strMsg;
	BOOL bRun;
	Filepath path;

	// Execute dialog
	if (dlg.DoModal() != IDOK) {
		return;
	}

	// Path create
	path.SetPath(dlg.m_szFileName);

	// Create option
	opt.phyfmt = dlg.m_dwPhysical;
	opt.logfmt = dlg.m_bLogical;
	ASSERT(_tcslen(dlg.m_szDiskName) < 60);
	_tcscpy(opt.name, dlg.m_szDiskName);

	// Create image according to type
	switch (dlg.m_dwType) {
		case 0:
			pDisk = (FDIDisk*)new FDIDisk2HD(0, NULL);
			break;
		case 1:
			pDisk = (FDIDisk*)new FDIDiskDIM(0, NULL);
			break;
		case 2:
			pDisk = (FDIDisk*)new FDIDiskD68(0, NULL);
			break;
		case 3:
			pDisk = (FDIDisk*)new FDIDisk2DD(0, NULL);
			break;
		case 4:
			pDisk = (FDIDisk*)new FDIDisk2HQ(0, NULL);
			break;
		default:
			ASSERT(FALSE);
			return;
	}

	// Format (includes physical, logical and save)
	// Stop scheduler during format
	AfxGetApp()->BeginWaitCursor();
	bRun = GetScheduler()->IsEnable();
	GetScheduler()->Enable(FALSE);
	::LockVM();
	::UnlockVM();
	if (!pDisk->Create(path, &opt)) {
		AfxGetApp()->EndWaitCursor();
		// Delete
		delete pDisk;

		// Create error
		::GetMsg(IDS_CREATEERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();
		return;
	}
	AfxGetApp()->EndWaitCursor();

	// Delete
	delete pDisk;

	// Auto mount (optional)
	if (dlg.m_nDrive >= 0) {
		InitCmdSub(dlg.m_nDrive, dlg.m_szFileName);
	}

	// Status message, resume
	::GetMsg(IDS_NEWFD, strMsg);
	SetInfo(strMsg);
	GetScheduler()->Reset();
	GetScheduler()->Enable(bRun);
	ResetCaption();
}

//---------------------------------------------------------------------------
//
//	New large capacity disk
//
//---------------------------------------------------------------------------
void CFrmWnd::OnNewDisk(UINT uID)
{
	CDiskDlg dlg(this);
	CString strMsg;
	BOOL bRun;
	UINT nMsg;
	LPCTSTR lpszPath;
	Filepath path;

	// Receive parameters
	ASSERT(this);
	ASSERT((uID >= IDM_NEWSASI) && (uID <= IDM_NEWMO));
	uID -= IDM_NEWSASI;
	ASSERT(uID <= 2);

	// Set type
	dlg.m_nType = (int)uID;

	// Get scheduler state (stop is done inside dialog)
	bRun = GetScheduler()->IsEnable();

	// Execute dialog
	if (dlg.DoModal() != IDOK) {
		// Canceled
		return;
	}

	// If dialog canceled, or failed
	if (!dlg.IsSucceeded()) {
		if (dlg.IsCanceled()) {
			// Canceled
			::GetMsg(IDS_CANCEL, strMsg);
		}
		else {
			// Create error
			::GetMsg(IDS_CREATEERR, strMsg);
		}
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();
		return;
	}

	// Get information message
	switch (uID) {
		// SASI-HD
		case 0:
			nMsg = IDS_NEWSASI;
			break;

		// SCSI-HD
		case 1:
			nMsg = IDS_NEWSCSI;
			break;

		// SCSI-MO
		case 2:
			nMsg = IDS_NEWMO;
			break;

		// Others (should not occur)
		default:
			ASSERT(FALSE);
			nMsg = 0;
			break;
	}

	// Load message
	::GetMsg(nMsg, strMsg);

	// MO mount
	if (uID == 2) {
		// Set flag
		if (dlg.m_bMount) {
			// Enable MO
			if (m_pSASI->IsValid()) {
				// Get file name
				lpszPath = dlg.GetPath();

				// Open and MRU
				path.SetPath(lpszPath);
				if (m_pSASI->Open(path)) {
					GetConfig()->SetMRUFile(2, lpszPath);
				}
			}
		}
	}

	// Status message, resume
	SetInfo(strMsg);
	GetScheduler()->Reset();
	GetScheduler()->Enable(bRun);
	ResetCaption();
}

//---------------------------------------------------------------------------
//
//	Options
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOptions()
{
	Config config;
	CConfigSheet sheet(this);

	// Get configuration data
	GetConfig()->GetConfig(&config);

	// Execute property sheet
	sheet.m_pConfig = &config;
	if (sheet.DoModal() != IDOK) {
		return;
	}

	// Set configuration
	GetConfig()->SetConfig(&config);

	// Apply (under VM lock)
	::LockVM();
	ApplyCfg();
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();
}
 
//---------------------------------------------------------------------------
//
//	Show cascaded
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCascade()
{
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);
	::CascadeWindows(GetView()->m_hWnd, 0, NULL, 0, NULL);
}

//---------------------------------------------------------------------------
//
//	Show cascaded UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCascadeUI(CCmdUI *pCmdUI)
{
	CMenu *pMenu;
	CMenu *pSubMenu;
	CSubWnd *pWnd;
	CString string;
	CString temp;
	int i;
	int n;
	UINT uID;
	DWORD dwID;

	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);

	// Determine if in popup window mode
	if (IsPopupSWnd()) {
		// If popup, meaningless
		pCmdUI->Enable(FALSE);
	}
	else {
		// If child, TRUE if one or more subwindows
		if (GetView()->GetSubWndNum() >= 1) {
			pCmdUI->Enable(TRUE);
		}
		else {
			pCmdUI->Enable(FALSE);
		}
	}

	// Find Window menu
	if (m_bPopupMenu) {
		pMenu = m_PopupMenu.GetSubMenu(0);
	}
	else {
		pMenu = &m_Menu;
	}
	ASSERT(pMenu);
	n = pMenu->GetMenuItemCount();
	pSubMenu = NULL;
	for (i=0; i<n; i++) {
		pSubMenu = pMenu->GetSubMenu(i);
		ASSERT(pSubMenu);
		if (pSubMenu->GetMenuItemID(0) == IDM_CASCADE) {
			break;
		}
	}
	ASSERT(pSubMenu);
	ASSERT(i < n);

	// Delete 6 or more
	while (pSubMenu->GetMenuItemCount() > 6) {
		pSubMenu->DeleteMenu(6, MF_BYPOSITION);
	}

	// Enable if subwindows exist
	pWnd = GetView()->GetFirstSWnd();
	if (!pWnd) {
		return;
	}

	// Add separator
	pSubMenu->AppendMenu(MF_SEPARATOR, 0);

	// Add menu items
	uID = IDM_SWND_START;
	while (pWnd) {
		// Get title and window ID
		pWnd->GetWindowText(string);
		dwID = pWnd->GetID();
		temp.Format("%c%c%c%c - ",
			(dwID >> 24) & 0xff,
			(dwID >> 16) & 0xff,
			(dwID >> 8) & 0xff,
			dwID & 0xff);
		string = temp + string;

		// Add to menu
		pSubMenu->AppendMenu(MF_STRING, uID, string);

		// Next
		pWnd = pWnd->m_pNextWnd;
		uID++;
	}
}

//---------------------------------------------------------------------------
//
//	Tile
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTile()
{
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);
	::TileWindows(GetView()->m_hWnd, MDITILE_VERTICAL, NULL, 0, NULL);
}

//---------------------------------------------------------------------------
//
//	Tile UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTileUI(CCmdUI *pCmdUI)
{
	if (IsPopupSWnd()) {
		// If popup, meaningless
		pCmdUI->Enable(FALSE);
	}
	else {
		// If child, TRUE if one or more subwindows
		if (GetView()->GetSubWndNum() >= 1) {
			pCmdUI->Enable(TRUE);
		}
		else {
			pCmdUI->Enable(FALSE);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Minimize all
//
//---------------------------------------------------------------------------
void CFrmWnd::OnIconic()
{
	CSubWnd *pSubWnd;

	ASSERT(this);
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);

	// Get first subwindow
	pSubWnd = GetView()->GetFirstSWnd();

	// Loop
	while (pSubWnd) {
		pSubWnd->ShowWindow(SW_MINIMIZE);
		pSubWnd = pSubWnd->m_pNextWnd;
	}
}

//---------------------------------------------------------------------------
//
//	Minimize all UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnIconicUI(CCmdUI *pCmdUI)
{
	// TRUE if subwindows exist
	if (GetView()->GetSubWndNum() > 0) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Arrange icons
//
//---------------------------------------------------------------------------
void CFrmWnd::OnArrangeIcon()
{
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);
	::ArrangeIconicWindows(GetView()->m_hWnd);
}

//---------------------------------------------------------------------------
//
//	Arrange icons UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnArrangeIconUI(CCmdUI *pCmdUI)
{
	CSubWnd *pSubWnd;

	if (IsPopupSWnd()) {
		// If popup, meaningless
		pCmdUI->Enable(FALSE);
		return;
	}

	// Enabled if any subwindow is minimized
	pSubWnd = GetView()->GetFirstSWnd();
	while (pSubWnd) {
		// Is minimized?
		if (pSubWnd->IsIconic()) {
			pCmdUI->Enable(TRUE);
			return;
		}

		// Next subwindow
		pSubWnd = pSubWnd->m_pNextWnd;
	}

	// Disabled because none are minimized
	pCmdUI->Enable(FALSE);
}

//---------------------------------------------------------------------------
//
//	Hide all
//
//---------------------------------------------------------------------------
void CFrmWnd::OnHide()
{
	CSubWnd *pSubWnd;

	ASSERT(this);
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);

	// Get first subwindow
	pSubWnd = GetView()->GetFirstSWnd();

	// Loop
	while (pSubWnd) {
		pSubWnd->ShowWindow(SW_HIDE);
		pSubWnd = pSubWnd->m_pNextWnd;
	}
}

//---------------------------------------------------------------------------
//
//	Hide all UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnHideUI(CCmdUI *pCmdUI)
{
	// TRUE if subwindows exist
	if (GetView()->GetSubWndNum() > 0) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Restore all
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRestore()
{
	CSubWnd *pSubWnd;

	ASSERT(this);
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);

	// Get first subwindow
	pSubWnd = GetView()->GetFirstSWnd();

	// Loop
	while (pSubWnd) {
		pSubWnd->ShowWindow(SW_RESTORE);
		pSubWnd = pSubWnd->m_pNextWnd;
	}
}

//---------------------------------------------------------------------------
//
//	Restore all UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRestoreUI(CCmdUI *pCmdUI)
{
	// TRUE if subwindows exist
	if (GetView()->GetSubWndNum() > 0) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Specify window
//
//---------------------------------------------------------------------------
void CFrmWnd::OnWindow(UINT uID)
{
	CSubWnd *pSubWnd;
	int n;

	ASSERT(this);
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);

	// Identify subwindow
	n = (int)(uID - IDM_SWND_START);
	pSubWnd = GetView()->GetFirstSWnd();
	if (!pSubWnd) {
		return;
	}

	// Search loop
	while (n > 0) {
		if (!pSubWnd) {
			return;
		}

		n--;
		pSubWnd = pSubWnd->m_pNextWnd;
	}

	// Activate subwindow
	pSubWnd->ShowWindow(SW_RESTORE);
	pSubWnd->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

//---------------------------------------------------------------------------
//
//	Version information
//
//---------------------------------------------------------------------------
void CFrmWnd::OnAbout()
{
	CAboutDlg dlg(this);

	// Execute modal dialog
	dlg.DoModal();
}

#endif	// _WIN32
