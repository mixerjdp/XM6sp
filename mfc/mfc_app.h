//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[MFC host application]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_app_h)
#define mfc_app_h

//===========================================================================
//
//	Application class
//
//===========================================================================
class CApp : public CWinApp
{
public:
	CApp();
									// Constructor
	BOOL InitInstance();
									// Initialize the application instance
	BOOL ExitInstance();
									// Terminate the application instance

private:
	void ReplaceForwardSlashWithBackslash(LPTSTR str);
	void RemoveDoubleQuotes(LPTSTR str);

	BOOL FASTCALL CheckMutex();
									// Check mutex
	BOOL FASTCALL CheckEnvironment();
									// Detect the host environment
	void FASTCALL SendCmd();
									// Send a command to the running instance
	HWND FASTCALL SearchXM6Wnd();
										// XM6 window lookup
	static BOOL CALLBACK EnumXM6Proc(HWND hWnd, LPARAM lParam);
									// Window search callback
	HANDLE m_hMutex;
									// Mutex object handle
	HMODULE m_hUser32;
									// USER32.DLL handle
};

#endif	// mfc_app_h
#endif	// _WIN32
