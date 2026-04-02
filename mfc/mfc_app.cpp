//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[MFC host application]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "mfc_frm.h"
#include "mfc_asm.h"
#include "mfc_com.h"
#include "mfc_info.h"
#include "mfc_res.h"
#include "mfc_app.h"
#include <stdlib.h>
#include <string.h>

//---------------------------------------------------------------------------
//
//	Application object
//
//---------------------------------------------------------------------------
CApp theApp;

//---------------------------------------------------------------------------
//
//	Function pointer declarations
//
//---------------------------------------------------------------------------
extern "C" {
typedef int (WINAPI *DRAWTEXTWIDE)(HDC, LPCWSTR, int, LPRECT, UINT);
}

//---------------------------------------------------------------------------
//
//	Global variables
//
//---------------------------------------------------------------------------
VM*pVM;								// Virtual Machine

//---------------------------------------------------------------------------
//
//	Static variables
//
//---------------------------------------------------------------------------
static CCriticalSection csect;        // Critical section used to serialize VM access
static BOOL bJapanese;                // Language mode flag: Japanese vs. non-Japanese
static BOOL bWinNT;                   // Windows NT/9x detection flag
static BOOL bSupport932;              // Shift-JIS (CP932) support flag
static BOOL bMMX;                     // MMX detection flag
static BOOL bCMOV;                    // CMOV detection flag
static LPSTR lpszInfoMsg;             // Information message buffer
static DRAWTEXTWIDE pDrawTextW;       // DrawTextW entry point

enum ui_language_mode_t {
	UI_LANG_AUTO = 0,
	UI_LANG_JAPANESE = 1,
	UI_LANG_ENGLISH = 2,
	UI_LANG_SPANISH = 3,
};

static ui_language_mode_t g_ui_language_mode = UI_LANG_AUTO;
static LANGID g_ui_langid = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

static ui_language_mode_t FASTCALL ParseUiLanguageMode(const char *value)
{
	if (!value || !value[0]) {
		return UI_LANG_AUTO;
	}

	if (_stricmp(value, "auto") == 0 || strcmp(value, "0") == 0) {
		return UI_LANG_AUTO;
	}
	if (_stricmp(value, "jp") == 0 || _stricmp(value, "ja") == 0 ||
		_stricmp(value, "japanese") == 0 || strcmp(value, "1") == 0) {
		return UI_LANG_JAPANESE;
	}
	if (_stricmp(value, "en") == 0 || _stricmp(value, "english") == 0 || strcmp(value, "2") == 0) {
		return UI_LANG_ENGLISH;
	}
	if (_stricmp(value, "es") == 0 || _stricmp(value, "spanish") == 0 || strcmp(value, "3") == 0) {
		return UI_LANG_SPANISH;
	}

	return UI_LANG_AUTO;
}

static LANGID FASTCALL SelectUiLangId(ui_language_mode_t mode)
{
	if (mode == UI_LANG_JAPANESE) {
		return MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
	}
	if (mode == UI_LANG_ENGLISH) {
		return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
	}
	if (mode == UI_LANG_SPANISH) {
		return MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_MODERN);
	}

	const WORD user_lang = PRIMARYLANGID(::GetUserDefaultLangID());
	if (user_lang == LANG_JAPANESE) {
		return MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
	}
	if (user_lang == LANG_SPANISH) {
		return MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_MODERN);
	}

	const WORD system_lang = PRIMARYLANGID(::GetSystemDefaultLangID());
	if (system_lang == LANG_JAPANESE) {
		return MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT);
	}
	if (system_lang == LANG_SPANISH) {
		return MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH_MODERN);
	}

	return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
}

static void FASTCALL ApplyUiLanguageForResources(LANGID langid)
{
	HMODULE kernel = ::GetModuleHandle(_T("KERNEL32.DLL"));
	if (kernel) {
		typedef LANGID (WINAPI *set_thread_ui_lang_t)(LANGID);
		set_thread_ui_lang_t set_thread_ui_lang =
			(set_thread_ui_lang_t)::GetProcAddress(kernel, "SetThreadUILanguage");
		if (set_thread_ui_lang) {
			set_thread_ui_lang(langid);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Determine whether to use the Japanese UI
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsJapanese(void)
{
	return bJapanese;
}

//---------------------------------------------------------------------------
//
//	Detect Windows NT support
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsWinNT(void)
{
	return bWinNT;
}

//---------------------------------------------------------------------------
//
//	Detect CP932 support
//
//---------------------------------------------------------------------------
BOOL FASTCALL Support932(void)
{
	return bSupport932;
}

//---------------------------------------------------------------------------
//
//	Detect MMX support
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsMMX(void)
{
	return bMMX;
}

//---------------------------------------------------------------------------
//
//	Detect CMOV support
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsCMOV(void)
{
	return bCMOV;
}

//---------------------------------------------------------------------------
//	Load a message string
//
//---------------------------------------------------------------------------
void FASTCALL GetMsg(UINT uID, CString& string)
{
	if (uID == 0) {
		string.Empty();
		return;
	}


	if (IsJapanese()) {
		if (!string.LoadString(uID)) {
#if defined(_DEBUG)
			TRACE(_T("GetMsg: ID:%d\n"), uID);
#endif	// _DEBUG
			string.Empty();
		}
		return;
	}


	if (string.LoadString(uID + 5000)) {
		return;
	}


	if (!string.LoadString(uID)) {
#if defined(_DEBUG)
		TRACE(_T("GetMsg: ID:%d\n"), uID);
#endif	// _DEBUG
		string.Empty();
	}
}

//---------------------------------------------------------------------------
//
//	Get the virtual machine
//
//---------------------------------------------------------------------------
VM* FASTCALL GetVM(void)
{
	ASSERT(pVM);
	return pVM;
}

//---------------------------------------------------------------------------
//
//	Lock the virtual machine
//
//---------------------------------------------------------------------------
void FASTCALL LockVM(void)
{
	csect.Lock();
}

//---------------------------------------------------------------------------
//
//	Unlock the virtual machine
//
//---------------------------------------------------------------------------
void FASTCALL UnlockVM(void)
{
	csect.Unlock();
}

//---------------------------------------------------------------------------
//
//	Open file dialog
//	* lpszPath must be initialized before calling this function
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

	// Set up the dialog structure
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = pParent->m_hWnd;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = lpszPath;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrInitialDir = Filepath::GetDefaultDir();
	ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

	// Set up the file filter
	GetMsg(nFilterID, strFilter);
	_tcscpy(szFilter, (LPCTSTR)strFilter);
	nLen = (int)_tcslen(szFilter);
	for (i=0; i<nLen; i++) {
		if (szFilter[i] == _T('|')) {
			szFilter[i] = _T('\0');
		}
	}

	// Run the common dialog
	if (!GetOpenFileName(&ofn)) {
		return FALSE;
	}

	// Get the canonical file name (FindFirstFile returns only the name and extension)
	hFind = FindFirstFile(lpszPath, &wfd);
	FindClose(hFind);
	_tsplitpath(lpszPath, szDrive, szDir, NULL, NULL);
	_tcscpy(lpszPath, szDrive);
	_tcscat(lpszPath, szDir);
	_tcscat(lpszPath, wfd.cFileName);

	// Save the default directory
	Filepath::SetDefaultDir(lpszPath);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	File save dialog
//	* lpszPath must be initialized before calling this function
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

	// Set up the dialog structure
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = pParent->m_hWnd;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = lpszPath;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrInitialDir = Filepath::GetDefaultDir();
	ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt = lpszExt;

	// Set up the file filter
	GetMsg(nFilterID, strFilter);
	_tcscpy(szFilter, (LPCTSTR)strFilter);
	nLen = (int)_tcslen(szFilter);
	for (i=0; i<nLen; i++) {
		if (szFilter[i] == _T('|')) {
			szFilter[i] = _T('\0');
		}
	}

	// Run the common dialog
	if (!GetSaveFileName(&ofn)) {
		return FALSE;
	}

	// Save the default directory
	Filepath::SetDefaultDir(lpszPath);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Configure the information message buffer
//
//---------------------------------------------------------------------------
void FASTCALL SetInfoMsg(LPCTSTR lpszMsg, BOOL bRec)
{
	// Store the buffer address when requested
	if (bRec) {
		// Keep the message buffer pointer
		lpszInfoMsg = (LPSTR)lpszMsg;
		return;
	}

	// Ignore the request if no buffer address is available
	if (!lpszInfoMsg) {
		return;
	}

	// Check the buffer size
	if (_tcslen(lpszInfoMsg) < CInfo::InfoBufMax) {
		// Copy the provided string
		_tcscpy(lpszInfoMsg, lpszMsg);
	}
}

//---------------------------------------------------------------------------
//
//	Wide-text DrawText wrapper
//	* Does nothing on unsupported operating systems
//
//---------------------------------------------------------------------------
int FASTCALL DrawTextWide(HDC hDC, LPCWSTR lpString, int nCount, LPRECT lpRect, UINT uFormat)
{
	// Is it supported?
	if (!pDrawTextW) {
		// Do nothing
		return 1;
	}

	// Draw using wide characters
	return pDrawTextW(hDC, lpString, nCount, lpRect, uFormat);
}

//===========================================================================
//
//	Application class
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CApp::CApp() : CWinApp(_T("XM6"))
{
	m_hMutex = NULL;
	m_hUser32 = NULL;
}

// Remove double quotes
void CApp::RemoveDoubleQuotes(LPTSTR str) {
	LPTSTR source = str;
	LPTSTR destination = str;

	while (*source != _T('\0')) {
		if (*source != _T('"')) {
			*destination = *source;
			++destination;
		}
		++source;
	}

	*destination = _T('\0');
}

// Replace forward slashes with backslashes
void CApp::ReplaceForwardSlashWithBackslash(LPTSTR str) {
	while (*str != _T('\0')) {
		if (*str == _T('/')) {
			*str = _T('\\');
		}
		++str;
	}
}


//---------------------------------------------------------------------------
//
//	Process the command line
//
//---------------------------------------------------------------------------
BOOL CApp::InitInstance()
{
	CFrmWnd *pFrmWnd;

	// This section checks the initial command line for a possible HDF image.
	// Get the string from the command line ("Run" on PocketPC)
	ReplaceForwardSlashWithBackslash(m_lpCmdLine);
	RemoveDoubleQuotes(m_lpCmdLine);
	//int msgboxIDx = MessageBox(NULL, testString, "BBC", MB_OKCANCEL | MB_DEFBUTTON2);

	// Clear the default directory
	Filepath::ClearDefaultDir();

	// Environment check
	if (!CheckEnvironment()) {
		return FALSE;
	}

	// Check for a second instance
	if (!CheckMutex()) {
		// If there is a command line, pass it through
		if (m_lpCmdLine[0] != _T('\0')) {
			SendCmd();
		}
		return FALSE;
	}

	// Create the main window and assign it to m_pMainWnd immediately
	pFrmWnd = new CFrmWnd();
	m_pMainWnd = (CWnd*)pFrmWnd;

	pFrmWnd->m_strXM6FilePath = m_lpCmdLine;

	// Initialization
	if (!pFrmWnd->Init()) {
		return FALSE;
	}

	// Show the main window
	pFrmWnd->ShowWindow(m_nCmdShow);
	pFrmWnd->UpdateWindow();

	return TRUE;
}
//---------------------------------------------------------------------------
//
//	End of instance
//
//---------------------------------------------------------------------------
BOOL CApp::ExitInstance()
{
	// Delete the mutex
	if (m_hMutex) {
		::CloseHandle(m_hMutex);
		m_hMutex = NULL;
	}

	// Release USER32.DLL
	if (m_hUser32) {
		::FreeLibrary(m_hUser32);
		m_hUser32 = NULL;
	}

	// Base class
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Check mutex
//
//---------------------------------------------------------------------------
BOOL FASTCALL CApp::CheckMutex()
{
	HANDLE hMutex;

	ASSERT(this);

	// Create it regardless of whether it already exists
	hMutex = ::CreateMutex(NULL, TRUE, _T("XM6"));
	if (hMutex) {
		// Already running?
		if (::GetLastError() == ERROR_ALREADY_EXISTS) {
			return TRUE;
		}

		// OK
		m_hMutex = hMutex;
		return TRUE;
	}

	// Failed for some reason
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Host OS detection
//
//---------------------------------------------------------------------------
BOOL FASTCALL CApp::CheckEnvironment()
{
	OSVERSIONINFO ovi;
	CString strError;

	ASSERT(this);

	//
//	Host platform checks
	//

	// Read the UI language override from the environment variable.
	// XM6_UI_LANG values: auto|jp|en|es (or 0|1|2|3).
	const char *lang_override = getenv("XM6_UI_LANG");
	g_ui_language_mode = ParseUiLanguageMode(lang_override);
	g_ui_langid = SelectUiLangId(g_ui_language_mode);
	ApplyUiLanguageForResources(g_ui_langid);

	::bJapanese = (PRIMARYLANGID(g_ui_langid) == LANG_JAPANESE) ? TRUE : FALSE;

	// Detect Windows NT
	memset(&ovi, 0, sizeof(ovi));
	ovi.dwOSVersionInfoSize = sizeof(ovi);
	VERIFY(::GetVersionEx(&ovi));
	if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		::bWinNT = TRUE;
	}
	else {
		::bWinNT = FALSE;
	}

	// Detect code page 932 support (requires Unicode support)
	::bSupport932 = FALSE;
	::pDrawTextW = NULL;
	if (::bWinNT) {
		// Unicode-capable OS
		if (::IsValidCodePage(932)) {
			// Load USER32.DLL
			m_hUser32 = ::LoadLibrary(_T("USER32.DLL"));
			if (m_hUser32) {
				// Get the DrawTextW address
				pDrawTextW = (DRAWTEXTWIDE)::GetProcAddress(m_hUser32, _T("DrawTextW"));
				if (pDrawTextW) {
					// CP932 support is available, so DrawTextW can be used
					::bSupport932 = TRUE;
				}
			}
		}
	}

	//
//	CPU feature detection
	//

	// Detect CMOV
	::bCMOV = FALSE;
	if (::IsCMOVSupportPortable()) {
		::bCMOV = TRUE;
	}

	// Detect MMX support (Windows 98 and later)
	::bMMX = FALSE;
	if (ovi.dwMajorVersion >= 4) {
		// Windows 95 or Windows NT 4 or later
		if ((ovi.dwMajorVersion == 4) && (ovi.dwMinorVersion == 0)) {
			// Windows 95 or Windows NT 4
			::bMMX = FALSE;
		}
		else {
			// Depends on the processor
			::bMMX = ::IsMMXSupportPortable();
		}
	}

	// Since version 2.05, both CMOV and MMX are required
	if (!::bCMOV || !::bMMX) {
		::GetMsg(IDS_PROCESSOR ,strError);
		AfxMessageBox(strError, MB_ICONSTOP | MB_OK);
		return FALSE;
	}

	// All good
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Forward the command line to a running instance
//
//---------------------------------------------------------------------------
void FASTCALL CApp::SendCmd()
{
	HWND hWnd;
	COPYDATASTRUCT cds;

	ASSERT(this);

	// Window search
	hWnd = SearchXM6Wnd();
	if (!hWnd) {
		return;
	}

	// Send via WM_COPYDATA
	memset(&cds, 0, sizeof(cds));
	cds.dwData = WM_COPYDATA;
	cds.cbData = ((DWORD)_tcslen(m_lpCmdLine) + 1) * sizeof(TCHAR);
	cds.lpData = m_lpCmdLine;
	::SendMessage(hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
}

//---------------------------------------------------------------------------
//
//	XM6 window lookup
//
//---------------------------------------------------------------------------
HWND FASTCALL CApp::SearchXM6Wnd()
{
	HWND hWnd;

	// The window is NULL
	hWnd = NULL;

	// Search
	::EnumWindows(EnumXM6Proc, (LPARAM)&hWnd);

	// Store the result through the callback
	return hWnd;
}

//---------------------------------------------------------------------------
//
//	XM6 window enumeration callback
//
//---------------------------------------------------------------------------
BOOL CALLBACK CApp::EnumXM6Proc(HWND hWnd, LPARAM lParam)
{
	HWND *phWnd;
	LONG lUser;

	// Receive parameters
	phWnd = (HWND*)lParam;
	ASSERT(phWnd);
	ASSERT(*phWnd == NULL);

	// Get the window user data
	lUser = ::GetWindowLong(hWnd, GWL_USERDATA);

	// Verify the XM6 frame window
	if (lUser == (LONG)MAKEID('X', 'M', '6', ' ')) {
		// If it is identified as the XM6 frame window, stop searching
		*phWnd = hWnd;
		return FALSE;
	}

	// It is different, keep searching
	return TRUE;
}

#endif	// _WIN32
