//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ‚o‚hD(ytanaka@ipc-tokai.or.jp)
//	[ MFC ƒAƒvƒŠƒP[ƒVƒ‡ƒ“ ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "mfc_frm.h"
#include "mfc_asm.h"
#include "mfc_com.h"
#include "mfc_info.h"
#include "mfc_res.h"
#include "mfc_app.h"

//---------------------------------------------------------------------------
//
//	ƒAƒvƒŠƒP[ƒVƒ‡ƒ“ ƒCƒ“ƒXƒ^ƒ“ƒX
//
//---------------------------------------------------------------------------
CApp theApp;

//---------------------------------------------------------------------------
//
//	ŠÖ”ƒ|ƒCƒ“ƒ^’è‹`
//
//---------------------------------------------------------------------------
extern "C" {
typedef int (WINAPI *DRAWTEXTWIDE)(HDC, LPCWSTR, int, LPRECT, UINT);
}

//---------------------------------------------------------------------------
//
//	ƒOƒ[ƒoƒ‹ ƒ[ƒN
//
//---------------------------------------------------------------------------
VM *pVM;								// Virtual Machine

//---------------------------------------------------------------------------
//
//	ƒXƒ^ƒeƒBƒbƒN ƒ[ƒN
//
//---------------------------------------------------------------------------
static CCriticalSection csect;			// VMƒƒbƒN—pƒNƒŠƒeƒBƒJƒ‹ƒZƒNƒVƒ‡ƒ“
static BOOL bJapanese;					// “ú–{ŒêE‰pŒê”»•Êƒtƒ‰ƒO
static BOOL bWinNT;						// WindowsNTEWindows9x”»•Êƒtƒ‰ƒO
static BOOL bSupport932;				// CP932(SHIFT-JIS)ƒTƒ|[ƒgƒtƒ‰ƒO
static BOOL bMMX;						// MMX”»•Êƒtƒ‰ƒO
static BOOL bCMOV;						// CMOV”»•Êƒtƒ‰ƒO
static LPSTR lpszInfoMsg;				// î•ñƒƒbƒZ[ƒWƒoƒbƒtƒ@
static DRAWTEXTWIDE pDrawTextW;			// DrawTextW

//---------------------------------------------------------------------------
//
//	“ú–{ŒêŠÂ‹«‚Ì”»’è
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsJapanese(void)
{
	return bJapanese;
}

//---------------------------------------------------------------------------
//
//	WindowsNT‚Ì”»’è
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsWinNT(void)
{
	return bWinNT;
}

//---------------------------------------------------------------------------
//
//	CP932ƒTƒ|[ƒg‚Ì”»’è
//
//---------------------------------------------------------------------------
BOOL FASTCALL Support932(void)
{
	return bSupport932;
}

//---------------------------------------------------------------------------
//
//	MMX‚Ì”»’è
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsMMX(void)
{
	return bMMX;
}

//---------------------------------------------------------------------------
//
//	CMOV‚Ì”»’è
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsCMOV(void)
{
	return bCMOV;
}

//---------------------------------------------------------------------------
//
//	ƒƒbƒZ[ƒWæ“¾
//
//---------------------------------------------------------------------------
void FASTCALL GetMsg(UINT uID, CString& string)
{
	// uID=0‚Å”ò‚ñ‚Å‚­‚éê‡‚ª‚ ‚é
	if (uID == 0) {
		string.Empty();
		return;
	}

	// “ú–{Œê‚©
	if (IsJapanese()) {
		if (!string.LoadString(uID)) {
#if defined(_DEBUG)
			TRACE(_T("GetMsg:•¶š—ñƒ[ƒh‚É¸”s ID:%d\n"), uID);
#endif	// _DEBUG
			string.Empty();
		}
		return;
	}

	// ‰pŒêB+5000‚Å‚·
	if (string.LoadString(uID + 5000)) {
		return;
	}

	// +0‚Å‚à‚¤ˆê“x
	if (!string.LoadString(uID)) {
#if defined(_DEBUG)
		TRACE(_T("GetMsg:•¶š—ñƒ[ƒh‚É¸”s ID:%d\n"), uID);
#endif	// _DEBUG
		string.Empty();
	}
}

//---------------------------------------------------------------------------
//
//	‰¼‘zƒ}ƒVƒ“‚ğæ“¾
//
//---------------------------------------------------------------------------
VM* FASTCALL GetVM(void)
{
	ASSERT(pVM);
	return pVM;
}

//---------------------------------------------------------------------------
//
//	‰¼‘zƒ}ƒVƒ“‚ğƒƒbƒN
//
//---------------------------------------------------------------------------
void FASTCALL LockVM(void)
{
	csect.Lock();
}

//---------------------------------------------------------------------------
//
//	‰¼‘zƒ}ƒVƒ“‚ğƒAƒ“ƒƒbƒN
//
//---------------------------------------------------------------------------
void FASTCALL UnlockVM(void)
{
	csect.Unlock();
}

//---------------------------------------------------------------------------
//
//	ƒtƒ@ƒCƒ‹ƒI[ƒvƒ“ƒ_ƒCƒAƒƒO
//	¦lpszPath‚Í•K‚¸‰Šú‰»‚µ‚ÄŒÄ‚Ño‚·‚±‚Æ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FileOpenDlg(CWnd *pParent, LPSTR lpszPath, UINT nFilterID)
{
	OPENFILENAME ofn;
	TCHAR szFilter[0x200];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	CString strFilter;
	int i;
	int nLen;
	WIN32_FIND_DATA wfd;
	HANDLE hFind;

	ASSERT(pParent);
	ASSERT(lpszPath);
	ASSERT(nFilterID);

	// \‘¢‘Ì‚ğİ’è
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = pParent->m_hWnd;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = lpszPath;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrInitialDir = Filepath::GetDefaultDir();
	ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

	// ƒtƒBƒ‹ƒ^‚ğİ’è
	GetMsg(nFilterID, strFilter);
	_tcscpy(szFilter, (LPCTSTR)strFilter);
	nLen = (int)_tcslen(szFilter);
	for (i=0; i<nLen; i++) {
		if (szFilter[i] == _T('|')) {
			szFilter[i] = _T('\0');
		}
	}

	// ƒRƒ‚ƒ“ƒ_ƒCƒAƒƒOÀs
	if (!GetOpenFileName(&ofn)) {
		return FALSE;
	}

	// ³®‚Èƒtƒ@ƒCƒ‹–¼‚ğ“¾‚é(FindFirstFile‚Å“¾‚ç‚ê‚é‚Ì‚Íƒtƒ@ƒCƒ‹–¼+Šg’£q‚Ì‚İ)
	hFind = FindFirstFile(lpszPath, &wfd);
	FindClose(hFind);
	_tsplitpath(lpszPath, szDrive, szDir, NULL, NULL);
	_tcscpy(lpszPath, szDrive);
	_tcscat(lpszPath, szDir);
	_tcscat(lpszPath, wfd.cFileName);

	// ƒfƒtƒHƒ‹ƒgƒfƒBƒŒƒNƒgƒŠ‚ğ•Û‘¶
	Filepath::SetDefaultDir(lpszPath);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒtƒ@ƒCƒ‹ƒZ[ƒuƒ_ƒCƒAƒƒO
//	¦lpszPath‚Í•K‚¸‰Šú‰»‚µ‚ÄŒÄ‚Ño‚·‚±‚ÆBlpszExt‚Íæ“ª3•¶š‚Ì‚İ—LŒø
//
//---------------------------------------------------------------------------
BOOL FASTCALL FileSaveDlg(CWnd *pParent, LPSTR lpszPath, LPCTSTR lpszExt, UINT nFilterID)
{
	OPENFILENAME ofn;
	TCHAR szFilter[0x200];
	CString strFilter;
	int i;
	int nLen;

	ASSERT(pParent);
	ASSERT(lpszPath);
	ASSERT(nFilterID);

	// \‘¢‘Ì‚ğİ’è
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = pParent->m_hWnd;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = lpszPath;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrInitialDir = Filepath::GetDefaultDir();
	ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt = lpszExt;

	// ƒtƒBƒ‹ƒ^‚ğİ’è
	GetMsg(nFilterID, strFilter);
	_tcscpy(szFilter, (LPCTSTR)strFilter);
	nLen = (int)_tcslen(szFilter);
	for (i=0; i<nLen; i++) {
		if (szFilter[i] == _T('|')) {
			szFilter[i] = _T('\0');
		}
	}

	// ƒRƒ‚ƒ“ƒ_ƒCƒAƒƒOÀs
	if (!GetSaveFileName(&ofn)) {
		return FALSE;
	}

	// ƒfƒtƒHƒ‹ƒgƒfƒBƒŒƒNƒgƒŠ‚ğ•Û‘¶
	Filepath::SetDefaultDir(lpszPath);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	î•ñƒƒbƒZ[ƒWİ’è
//
//---------------------------------------------------------------------------
void FASTCALL SetInfoMsg(LPCTSTR lpszMsg, BOOL bRec)
{
	// ‹L‰¯ƒtƒ‰ƒO‚ğŒ©‚é
	if (bRec) {
		// ƒoƒbƒtƒ@ƒAƒhƒŒƒX‚ğ‹L‰¯
		lpszInfoMsg = (LPSTR)lpszMsg;
		return;
	}

	// ƒoƒbƒtƒ@ƒAƒhƒŒƒX‚ª—^‚¦‚ç‚ê‚Ä‚¢‚È‚¯‚ê‚ÎA–³‹
	if (!lpszInfoMsg) {
		return;
	}

	// ƒoƒbƒtƒ@ƒTƒCƒYƒ`ƒFƒbƒN
	if (_tcslen(lpszInfoMsg) < CInfo::InfoBufMax) {
		// —^‚¦‚ç‚ê‚½•¶š—ñ‚ğƒRƒs[
		_tcscpy(lpszInfoMsg, lpszMsg);
	}
}

//---------------------------------------------------------------------------
//
//	DrawTextW
//	¦”ñƒTƒ|[ƒgOS‚Å‚ÍA‰½‚à‚µ‚È‚¢
//
//---------------------------------------------------------------------------
int FASTCALL DrawTextWide(HDC hDC, LPCWSTR lpString, int nCount, LPRECT lpRect, UINT uFormat)
{
	// ƒTƒ|[ƒg‚³‚ê‚Ä‚¢‚é‚©
	if (!pDrawTextW) {
		// ‰½‚à‚µ‚È‚¢
		return 1;
	}

	// ƒƒCƒh•¶š‚ÅDraw
	return pDrawTextW(hDC, lpString, nCount, lpRect, uFormat);
}

//===========================================================================
//
//	ƒAƒvƒŠƒP[ƒVƒ‡ƒ“
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
CApp::CApp() : CWinApp(_T("XM6"))
{
	m_hMutex = NULL;
	m_hUser32 = NULL;
}

//---------------------------------------------------------------------------
//
//	Iniciar linea de comandos
//
//---------------------------------------------------------------------------
BOOL CApp::InitInstance()
{
	CFrmWnd *pFrmWnd;

	// En este apartado se verifica la linea de comando inicial para cargar un posible HDF *-*
	// Get the string from the command line ('Run' in PocketPC)
	CString testString(m_lpCmdLine);

	//int msgboxIDx = MessageBox(NULL, testString, "BBC", MB_OKCANCEL | MB_DEFBUTTON2);


	// Borrar el directorio por defecto
	Filepath::ClearDefaultDir();

	// Evaluacion de ambiente
	if (!CheckEnvironment()) {
		return FALSE;
	}

	// Comprobacion de la doble activacion
	if (!CheckMutex()) {
		// Si hay una linea de comandos, pase
		if (m_lpCmdLine[0] != _T('\0')) {
			SendCmd();
		}
		return FALSE;
	}

	// Crear ventana principal (asignar a m_pMainWnd inmediatamente)
	pFrmWnd = new CFrmWnd();
	m_pMainWnd = (CWnd*)pFrmWnd;


	pFrmWnd->RutaCompletaArchivoXM6 = m_lpCmdLine;
	//MessageBox(NULL, pFrmWnd->RutaCompletaArchivoXM6, "BBC", MB_OKCANCEL | MB_DEFBUTTON2);
	

	// Inicializacion
	if (!pFrmWnd->Init()) {
		return FALSE;
	}
		
	// Mostrar
	pFrmWnd->ShowWindow(m_nCmdShow);
	pFrmWnd->UpdateWindow();

	return TRUE;
}
//---------------------------------------------------------------------------
//
//	ƒCƒ“ƒXƒ^ƒ“ƒXI—¹
//
//---------------------------------------------------------------------------
BOOL CApp::ExitInstance()
{
	// Mutexíœ
	if (m_hMutex) {
		::CloseHandle(m_hMutex);
		m_hMutex = NULL;
	}

	// USER32.DLL‰ğ•ú
	if (m_hUser32) {
		::FreeLibrary(m_hUser32);
		m_hUser32 = NULL;
	}

	// Šî–{ƒNƒ‰ƒX
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Mutexƒ`ƒFƒbƒN
//
//---------------------------------------------------------------------------
BOOL FASTCALL CApp::CheckMutex()
{
	HANDLE hMutex;

	ASSERT(this);

	// —L–³‚É‚©‚©‚í‚ç‚¸Aì¬
	hMutex = ::CreateMutex(NULL, TRUE, _T("XM6"));
	if (hMutex) {
		// Šù‚É‹N“®H
		if (::GetLastError() == ERROR_ALREADY_EXISTS) {
			return TRUE;
		}

		// OK
		m_hMutex = hMutex;
		return TRUE;
	}

	// ‚È‚º‚©¸”s
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ŠÂ‹«‚Ì”»’è
//
//---------------------------------------------------------------------------
BOOL FASTCALL CApp::CheckEnvironment()
{
	OSVERSIONINFO ovi;
	CString strError;

	ASSERT(this);

	//
	//	OS‚Ì”»’è
	//

	// “ú–{ŒêŠÂ‹«‚Ì”»’è
	::bJapanese = FALSE;
	if (PRIMARYLANGID(GetSystemDefaultLangID()) == LANG_JAPANESE) {
		if (PRIMARYLANGID(GetUserDefaultLangID()) == LANG_JAPANESE) {
			// ƒVƒXƒeƒ€ƒfƒtƒHƒ‹ƒgEƒ†[ƒUƒfƒtƒHƒ‹ƒg‚Ì‘o•û‚Å”»’è
			::bJapanese = TRUE;
		}
	}

	// WindowsNT‚Ì”»’è
	memset(&ovi, 0, sizeof(ovi));
	ovi.dwOSVersionInfoSize = sizeof(ovi);
	VERIFY(::GetVersionEx(&ovi));
	if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		::bWinNT = TRUE;
	}
	else {
		::bWinNT = FALSE;
	}

	// ƒR[ƒhƒy[ƒW932ƒTƒ|[ƒg‚Ì”»’è(UNICODEƒTƒ|[ƒg‚ª‘O’ñ)
	::bSupport932 = FALSE;
	::pDrawTextW = NULL;
	if (::bWinNT) {
		// UNICODEƒTƒ|[ƒgOS
		if (::IsValidCodePage(932)) {
			// USER32.DLL‚ğƒ[ƒh
			m_hUser32 = ::LoadLibrary(_T("USER32.DLL"));
			if (m_hUser32) {
				// DrawTextW‚ÌƒAƒhƒŒƒX‚ğ“¾‚é
				pDrawTextW = (DRAWTEXTWIDE)::GetProcAddress(m_hUser32, _T("DrawTextW"));
				if (pDrawTextW) {
					// CP932‚Ö‚Ì•ÏŠ·‚Æ•¦‚ª‰Â”
					::bSupport932 = TRUE;
				}
			}
		}
	}

	//
	//	ƒvƒƒZƒbƒT‚Ì”»’è
	//

	// CMOV‚Ì”»’è
	::bCMOV = FALSE;
	if (::IsCMOVSupport()) {
		::bCMOV = TRUE;
	}

	// MMX‚Ì”»’è(Windows98ˆÈ~‚Ì‚İ)
	::bMMX = FALSE;
	if (ovi.dwMajorVersion >= 4) {
		// Windows95 or WindowsNT4 ˆÈ~
		if ((ovi.dwMajorVersion == 4) && (ovi.dwMinorVersion == 0)) {
			// Windows95 or WindowsNT4
			::bMMX = FALSE;
		}
		else {
			// ƒvƒƒZƒbƒT‚É‚æ‚é
			::bMMX = ::IsMMXSupport();
		}
	}

	// version2.05‚©‚çACMOV,MMX‚Æ‚à•K{
	if (!::bCMOV || !::bMMX) {
		::GetMsg(IDS_PROCESSOR ,strError);
		AfxMessageBox(strError, MB_ICONSTOP | MB_OK);
		return FALSE;
	}

	// ‚·‚×‚ÄOK
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒRƒ}ƒ“ƒhƒ‰ƒCƒ“‘—M
//
//---------------------------------------------------------------------------
void FASTCALL CApp::SendCmd()
{
	HWND hWnd;
	COPYDATASTRUCT cds;

	ASSERT(this);

	// ƒEƒBƒ“ƒhƒEŒŸõ
	hWnd = SearchXM6Wnd();
	if (!hWnd) {
		return;
	}

	// WM_COPYDATA‚Å‘—M
	memset(&cds, 0, sizeof(cds));
	cds.dwData = WM_COPYDATA;
	cds.cbData = ((DWORD)_tcslen(m_lpCmdLine) + 1) * sizeof(TCHAR);
	cds.lpData = m_lpCmdLine;
	::SendMessage(hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
}

//---------------------------------------------------------------------------
//
//	XM6ƒEƒBƒ“ƒhƒEŒŸõ
//
//---------------------------------------------------------------------------
HWND FASTCALL CApp::SearchXM6Wnd()
{
	HWND hWnd;

	// ƒEƒBƒ“ƒhƒE‚ÍNULL
	hWnd = NULL;

	// ŒŸõ
	::EnumWindows(EnumXM6Proc, (LPARAM)&hWnd);

	// ƒR[ƒ‹ƒoƒbƒNŠÖ”‚ÅŒ‹‰Ê‚ğ“ü‚ê‚é
	return hWnd;
}

//---------------------------------------------------------------------------
//
//	XM6ƒEƒBƒ“ƒhƒEŒŸõƒR[ƒ‹ƒoƒbƒN
//
//---------------------------------------------------------------------------
BOOL CALLBACK CApp::EnumXM6Proc(HWND hWnd, LPARAM lParam)
{
	HWND *phWnd;
	LONG lUser;

	// ƒpƒ‰ƒ[ƒ^ó‚¯æ‚è
	phWnd = (HWND*)lParam;
	ASSERT(phWnd);
	ASSERT(*phWnd == NULL);

	// ŠY“–ƒEƒBƒ“ƒhƒE‚Ìƒ†[ƒUƒf[ƒ^‚ğ“¾‚é
	lUser = ::GetWindowLong(hWnd, GWL_USERDATA);

	// XM6ƒ`ƒFƒbƒN‚ğs‚¤
	if (lUser == (LONG)MAKEID('X', 'M', '6', ' ')) {
		// XM6ƒtƒŒ[ƒ€ƒEƒBƒ“ƒhƒE‚Æ”»’è‚µA‘Å‚¿Ø‚é
		*phWnd = hWnd;
		return FALSE;
	}

	// ˆá‚Á‚Ä‚¢‚é‚Ì‚Å‘±‚¯‚é
	return TRUE;
}

#endif	// _WIN32
