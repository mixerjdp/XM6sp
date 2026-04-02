//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ MFC Scheduler ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_sch_h)
#define mfc_sch_h

//===========================================================================
//
//	Scheduler
//
//===========================================================================
class CScheduler : public CComponent
{
public:
	// Basic procedures
	CScheduler(CFrmWnd *pFrmWnd);
										// Constructor
	BOOL FASTCALL Init();
										// Initialization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL ApplyCfg(const Config *pConfig);
										// Apply configuration
#if defined(_DEBUG)
	void AssertValid() const;
										// Assert
#endif	// _DEBUG

	// Execution control
	void FASTCALL Reset();
										// Reset time
	void FASTCALL Run();
										// Execute
	void FASTCALL Stop();
										// Scheduler stop

	// Save/Load
	BOOL FASTCALL Save(Fileio *pFio, int nVer);
										// Save
	BOOL FASTCALL Load(Fileio *pFio, int nVer);
										// Load
	BOOL FASTCALL HasSavedEnable() const { return m_bSavedValid; }
										// Was Enable status saved
	BOOL FASTCALL GetSavedEnable() const { return m_bSavedEnable; }
										// Was Enable status set at save
	void FASTCALL SetSavedEnable(BOOL bEnable) { m_bSavedEnable = bEnable; }
										// Set status at save

	// Others
	void FASTCALL Menu(BOOL bMenu)		{ m_bMenu = bMenu; }
										// Menu notification
	void FASTCALL Activate(BOOL bAct)	{ m_bActivate = bAct; }
										// Active notification
	void FASTCALL SyncDisasm();
										// PC sync with disassembler
	int FASTCALL GetFrameRate();
										// Get frame rate
	void FASTCALL OnMainFramePresented();
										// Async confirmation of frame consumed by UI

private:
	static UINT ThreadFunc(LPVOID pParam);
										// Thread function
	unsigned __int64 FASTCALL GetTimeMicro();
	unsigned __int64 FASTCALL GetTimeMilli();
	DWORD FASTCALL GetTime()			{ return (DWORD)GetTimeMilli(); }
										// Get time
	void FASTCALL Lock()				{ ::LockVM(); }
										// Lock VM
	void FASTCALL Unlock()				{ ::UnlockVM(); }
										// Unlock VM
	void FASTCALL Refresh();
										// Screen refresh
	CPU *m_pCPU;
										// CPU
	Render *m_pRender;
										// Renderer
	CWinThread *m_pThread;
										// Thread pointer
	CSound *m_pSound;
										// Sound
	CInput *m_pInput;
										// Input
	BOOL m_bExitReq;
										// Thread exit request
	DWORD m_dwExecTime;
										// Timer count (execution)
	int m_nSubWndNum;
										// Number of subwindows
	int m_nSubWndDisp;
										// Display subwindow (-1:main screen)
	BOOL m_bMPUFull;
										// MPU fullspeed flag
	BOOL m_bVMFull;
										// VM fullspeed flag
	DWORD m_dwDrawCount;
										// Frame display count
	DWORD m_dwDrawPrev;
										// Frame display count (previous)
	DWORD m_dwDrawTime;
										// Frame display time
	DWORD m_dwDrawBackup;
										// Frame display count (backup)
	BOOL m_bMenu;
										// Menu flag
	BOOL m_bActivate;
										// Active flag
	BOOL m_bBackup;
										// Enable flag backup
	BOOL m_bSavedValid;
										// Enable status was saved
	BOOL m_bSavedEnable;
										// Enable status at save
	LARGE_INTEGER m_liFreq;
										// High performance counter frequency
};

#endif	// mfc_sch_h
#endif	// _WIN32
