//---------------------------------------------------------------------------
//
// X68000 Emulator "XM6"
//
// Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
// [ MFC frame window ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_frm_h)
#define mfc_frm_h

//---------------------------------------------------------------------------
//
// Window messages
//
//---------------------------------------------------------------------------
#define WM_KICK			WM_APP				// Emulator kick-start
#define WM_SHELLNOTIFY	(WM_USER+5)		// Shell notification handling

//===========================================================================
//
// Frame window
//
//===========================================================================
class CFrmWnd : public CFrameWnd
{
public:
	// Frame window
	CFrmWnd();
										// Initialization
	BOOL Init();
										// Get View

	// Get View
	CDrawView* FASTCALL GetView() const;
										// Get First Component
	CComponent* FASTCALL GetFirstComponent() const;
										// Get Scheduler
	CScheduler* FASTCALL GetScheduler() const;
										// Get Sound
	CSound* FASTCALL GetSound() const;
										// Get Input
	CInput* FASTCALL GetInput() const;
										// Get Port
	CPort* FASTCALL GetPort() const;
										// Get MIDI
	CMIDI* FASTCALL GetMIDI() const;
										// Get T Key
	CTKey* FASTCALL GetTKey() const;
										// Get Host
	CHost* FASTCALL GetHost() const;
										// Get Info
	CInfo* FASTCALL GetInfo() const;
	// Get Config
	CConfig* FASTCALL GetConfig() const;
										// Recalculate status view
	void FASTCALL ApplyWindowScale();
										// Recompute the frame size using the selected window scale

	// Status bar support
	void FASTCALL RecalcStatusView();
										// Get window class name

	// Subwindow support
	LPCTSTR FASTCALL GetWndClassName() const;
										// Check whether this is a popup subwindow
	BOOL FASTCALL IsPopupSWnd() const;
										// Initialize the command-line subwindow

	// Drag and drop support
	BOOL FASTCALL InitCmdSub(int nDrive, LPCTSTR lpszPath);
									// Command-line processing subwindow
	BOOL m_bFullScreen;
	BOOL m_bBorderless;
	BOOL m_bVSyncEnabled;
	WINDOWPLACEMENT m_wpPrev;
	DWORD m_dwPrevStyle;
	DWORD m_dwPrevExStyle;

	void EnterBorderlessFullscreen();
	void ExitBorderlessFullscreen();
	void OnRenderFast();
	void OnRenderFastUI(CCmdUI *pCmdUI);
	void OnYmfm();
	void OnYmfmUI(CCmdUI *pCmdUI);
	void OnToggleRenderer();
	void OnToggleVSync();
	void OnToggleOSD();
	void OnToggleShader();
	void OnToggleShaderUI(CCmdUI *pCmdUI);

	CString m_strXM6FileName;
	CString m_strXM6FilePath;
	CString m_strSaveStatePath;

	// Save the frame window state
	void SaveFrameWnd();
	// Save disk state
	void SaveDiskState();


protected:
	// Override
	BOOL PreCreateWindow(CREATESTRUCT& cs);
										// Get the message string
	void GetMessageString(UINT nID, CString& rMessage) const;
										// Create

	// WM messages
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Close
	afx_msg void OnClose();
										// Destroy
	afx_msg void OnDestroy();
										// Move
	afx_msg void OnMove(int x, int y);
										// Display Change
	afx_msg LRESULT OnDisplayChange(UINT uParam, LONG lParam);
										// Erase Bkgnd
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
										// Paint
	afx_msg void OnPaint();
										// Activate
	afx_msg void OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized);
										// Activate
#if _MFC_VER >= 0x700
	afx_msg void OnActivateApp(BOOL bActive, DWORD dwThreadID);
#else
	afx_msg void OnActivateApp(BOOL bActive, HTASK hTask);
#endif
										// Enter Menu Loop
	afx_msg void OnEnterMenuLoop(BOOL bTrackPopup);
										// Exit Menu Loop
	afx_msg void OnExitMenuLoop(BOOL bTrackPopup);
										// Parent Notify
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
										// Kick
	afx_msg LONG OnKick(UINT uParam, LONG lParam);
										// Draw Item
	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDIS);
										// Context Menu
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint pos);
										// Power Broad Cast
	afx_msg LONG OnPowerBroadCast(UINT uParam, LONG lParam);
										// Sys Command
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
										// Sys Command
#if _MFC_VER >= 0x700
	afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
#else
	afx_msg LONG OnCopyData(UINT uParam, LONG lParam);
										// Copy Data
#endif
	afx_msg void OnEndSession(BOOL bEnding);
										// Shell Notify
	afx_msg LONG OnShellNotify(UINT uParam, LONG lParam);
										// Open

	// Command handling
	afx_msg void OnOpen();

	afx_msg void OnFastOpen();
										// Open UI
	afx_msg void OnOpenUI(CCmdUI *pCmdUI);
										// Save
	afx_msg void OnSave();
										// Save UI
	afx_msg void OnSaveUI(CCmdUI *pCmdUI);
										// Save as
	afx_msg void OnSaveAs();
										// Save as UI
	afx_msg void OnSaveAsUI(CCmdUI *pCmdUI);
										// Most recently used
	afx_msg void OnMRU(UINT uID);
										// MRU
	afx_msg void OnMRUUI(CCmdUI *pCmdUI);
										// MRU UI
	afx_msg void OnReset();
	afx_msg void OnResetNew();
										// Reset UI
	afx_msg void OnResetUI(CCmdUI *pCmdUI);
										// Custom configuration

	afx_msg void OnScc();	// Custom configuration

	afx_msg void OnSccUI(CCmdUI* pCmdUI);

	afx_msg void OnSgc();	// Global configuration

	afx_msg void OnSgcUI(CCmdUI* pCmdUI);

	afx_msg void OnSgcr();	// Global configuration and restart

	afx_msg void OnSgcrUI(CCmdUI* pCmdUI);

	afx_msg void OnInterrupt();
										// Interrupt UI
	afx_msg void OnInterruptUI(CCmdUI *pCmdUI);
										// Power toggle
	afx_msg void OnPower();
										// Power toggle UI
	afx_msg void OnPowerUI(CCmdUI *pCmdUI);
										// Exit
	afx_msg void OnExit();
										// Floppy disk command

	afx_msg void OnFD(UINT uID);
										// Floppy disk open UI
	afx_msg void OnFDOpenUI(CCmdUI *pCmdUI);
										// Floppy disk eject UI
	afx_msg void OnFDEjectUI(CCmdUI *pCmdUI);
										// Floppy disk write-protect UI
	afx_msg void OnFDWritePUI(CCmdUI *pCmdUI);
										// Floppy disk force-eject UI
	afx_msg void OnFDForceUI(CCmdUI *pCmdUI);
										// Floppy disk invalid-insert UI
	afx_msg void OnFDInvalidUI(CCmdUI *pCmdUI);
										// Floppy disk media UI
	afx_msg void OnFDMediaUI(CCmdUI *pCmdUI);
										// Floppy disk MRU UI
	afx_msg void OnFDMRUUI(CCmdUI *pCmdUI);
										// MO open

	afx_msg void OnMOOpen();
										// MO open UI
	afx_msg void OnMOOpenUI(CCmdUI *pCmdUI);
										// MO eject
	afx_msg void OnMOEject();
										// MO eject UI
	afx_msg void OnMOEjectUI(CCmdUI *pCmdUI);
										// MO write-protect
	afx_msg void OnMOWriteP();
										// MO write-protect UI
	afx_msg void OnMOWritePUI(CCmdUI *pCmdUI);
										// MO force-eject
	afx_msg void OnMOForce();
										// MO force-eject UI
	afx_msg void OnMOForceUI(CCmdUI *pCmdUI);
										// MO MRU
	afx_msg void OnMOMRU(UINT uID);
										// MOMRU
	afx_msg void OnMOMRUUI(CCmdUI *pCmdUI);
										// MOMRU UI

	afx_msg void OnCDOpen();
										// CD open UI
	afx_msg void OnCDOpenUI(CCmdUI *pCmdUI);
										// CD eject
	afx_msg void OnCDEject();
										// CD eject UI
	afx_msg void OnCDEjectUI(CCmdUI *pCmdUI);
										// CD force-eject
	afx_msg void OnCDForce();
										// CD force-eject UI
	afx_msg void OnCDForceUI(CCmdUI *pCmdUI);
										// CD MRU
	afx_msg void OnCDMRU(UINT nID);
										// CDMRU
	afx_msg void OnCDMRUUI(CCmdUI *pCmdUI);
										// CDMRU UI

	afx_msg void OnLog();
										// Log
	afx_msg void OnLogUI(CCmdUI *pCmdUI);
										// Scheduler
	afx_msg void OnScheduler();
										// Scheduler
	afx_msg void OnSchedulerUI(CCmdUI *pCmdUI);
										// Device
	afx_msg void OnDevice();
										// Device
	afx_msg void OnDeviceUI(CCmdUI *pCmdUI);
										// CPU registers
	afx_msg void OnCPUReg();
										// CPU register
	afx_msg void OnCPURegUI(CCmdUI *pCmdUI);
										// Interrupt
	afx_msg void OnInt();
										// Interrupt
	afx_msg void OnIntUI(CCmdUI *pCmdUI);
										// Disassembly
	afx_msg void OnDisasm();
										// Disassembly
	afx_msg void OnDisasmUI(CCmdUI *pCmdUI);
										// Memory
	afx_msg void OnMemory();
										// Memory
	afx_msg void OnMemoryUI(CCmdUI *pCmdUI);
										// Breakpoint
	afx_msg void OnBreakP();
										// Breakpoint
	afx_msg void OnBreakPUI(CCmdUI *pCmdUI);
										// MFP
	afx_msg void OnMFP();
										// MFP
	afx_msg void OnMFPUI(CCmdUI *pCmdUI);
										// MFP UI
	afx_msg void OnDMAC();
										// DMAC
	afx_msg void OnDMACUI(CCmdUI *pCmdUI);
										// DMAC UI
	afx_msg void OnCRTC();
										// CRTC
	afx_msg void OnCRTCUI(CCmdUI *pCmdUI);
										// CRTC UI
	afx_msg void OnVC();
										// VC
	afx_msg void OnVCUI(CCmdUI *pCmdUI);
										// VC UI
	afx_msg void OnRTC();
										// RTC
	afx_msg void OnRTCUI(CCmdUI *pCmdUI);
										// RTC UI
	afx_msg void OnOPM();
										// OPM
	afx_msg void OnOPMUI(CCmdUI *pCmdUI);
										// OPM UI
	afx_msg void OnKeyboard();
										// Keyboard
	afx_msg void OnKeyboardUI(CCmdUI *pCmdUI);
										// FDD
	afx_msg void OnFDD();
										// FDD
	afx_msg void OnFDDUI(CCmdUI *pCmdUI);
										// FDD UI
	afx_msg void OnFDC();
										// FDC
	afx_msg void OnFDCUI(CCmdUI *pCmdUI);
										// FDC UI
	afx_msg void OnSCC();
										// SCC
	afx_msg void OnSCCUI(CCmdUI *pCmdUI);
										// SCC UI
	afx_msg void OnCynthia();
										// CYNTHIA
	afx_msg void OnCynthiaUI(CCmdUI *pCmdUI);
										// CYNTHIA UI
	afx_msg void OnSASI();
										// SASI
	afx_msg void OnSASIUI(CCmdUI *pCmdUI);
										// SASI UI
	afx_msg void OnMIDI();
										// MIDI
	afx_msg void OnMIDIUI(CCmdUI *pCmdUI);
										// MIDI UI
	afx_msg void OnSCSI();
										// SCSI
	afx_msg void OnSCSIUI(CCmdUI *pCmdUI);
										// SCSI UI
	afx_msg void OnTVRAM();
										// Text view
	afx_msg void OnTVRAMUI(CCmdUI *pCmdUI);
										// Graphics view 1024x1024
	afx_msg void OnG1024();
										// Graphics view 1024x1024
	afx_msg void OnG1024UI(CCmdUI *pCmdUI);
										// Graphics view 16 colors
	afx_msg void OnG16(UINT uID);
										// Graphics view 16 colors
	afx_msg void OnG16UI(CCmdUI *pCmdUI);
										// Graphics view 256 colors
	afx_msg void OnG256(UINT uID);
										// Graphics view 256 colors
	afx_msg void OnG256UI(CCmdUI *pCmdUI);
										// Graphics view 65536 colors
	afx_msg void OnG64K();
										// Graphics view 65536 colors
	afx_msg void OnG64KUI(CCmdUI *pCmdUI);
										// PCG
	afx_msg void OnPCG();
										// PCG
	afx_msg void OnPCGUI(CCmdUI *pCmdUI);
										// PCG UI
	afx_msg void OnBG(UINT uID);
										// BG view
	afx_msg void OnBGUI(CCmdUI *pCmdUI);
										// Palette
	afx_msg void OnPalet();
										// Palette
	afx_msg void OnPaletUI(CCmdUI *pCmdUI);
										// Text buffer
	afx_msg void OnTextBuf();
										// Text buffer
	afx_msg void OnTextBufUI(CCmdUI *pCmdUI);
										// Graphics buffer
	afx_msg void OnGrpBuf(UINT uID);
										// Graphics buffer
	afx_msg void OnGrpBufUI(CCmdUI *pCmdUI);
										// PCG buffer
	afx_msg void OnPCGBuf();
										// PCG buffer
	afx_msg void OnPCGBufUI(CCmdUI *pCmdUI);
										// BG/sprite buffer
	afx_msg void OnBGSpBuf();
										// BG/sprite buffer
	afx_msg void OnBGSpBufUI(CCmdUI *pCmdUI);
										// Palette buffer
	afx_msg void OnPaletBuf();
										// Palette buffer
	afx_msg void OnPaletBufUI(CCmdUI *pCmdUI);
										// Mixed buffer
	afx_msg void OnMixBuf();
										// Mixed buffer
	afx_msg void OnMixBufUI(CCmdUI *pCmdUI);
										// Component
	afx_msg void OnComponent();
										// Component
	afx_msg void OnComponentUI(CCmdUI *pCmdUI);
										// OS information
	afx_msg void OnOSInfo();
										// OS information
	afx_msg void OnOSInfoUI(CCmdUI *pCmdUI);
										// Sound
	afx_msg void OnSound();
										// Sound
	afx_msg void OnSoundUI(CCmdUI *pCmdUI);
										// Input
	afx_msg void OnInput();
										// Input
	afx_msg void OnInputUI(CCmdUI *pCmdUI);
										// Port
	afx_msg void OnPort();
										// Port
	afx_msg void OnPortUI(CCmdUI *pCmdUI);
										// Bitmap
	afx_msg void OnBitmap();
										// Bitmap
	afx_msg void OnBitmapUI(CCmdUI *pCmdUI);
										// MIDI driver
	afx_msg void OnMIDIDrv();
										// MIDI driver
	afx_msg void OnMIDIDrvUI(CCmdUI *pCmdUI);
										// Caption
	afx_msg void OnCaption();
										// Caption
	afx_msg void OnCaptionUI(CCmdUI *pCmdUI);
										// Menu bar
	afx_msg void OnMenu();
										// Menu bar
	afx_msg void OnMenuUI(CCmdUI *pCmdUI);
										// Status bar
	afx_msg void OnStatus();
										// Status bar
	afx_msg void OnStatusUI(CCmdUI *pCmdUI);
										// Refresh
	afx_msg void OnRefresh();
										// Refresh
	afx_msg void OnWindowScale(UINT uID);
										// Window scale
	afx_msg void OnWindowScaleUI(CCmdUI *pCmdUI);
										// Window scale UI
										// Fullscreen
	afx_msg void OnFullScreen();
										// Fullscreen
	afx_msg void OnFullScreenUI(CCmdUI *pCmdUI);
										// Execute

	afx_msg void OnExec();
										// Run
	afx_msg void OnExecUI(CCmdUI *pCmdUI);
										// Break
	afx_msg void OnBreak();
										// Stop
	afx_msg void OnBreakUI(CCmdUI *pCmdUI);
										// Trace
	afx_msg void OnTrace();
										// Trace
	afx_msg void OnTraceUI(CCmdUI *pCmdUI);
										// Mouse mode

	afx_msg void OnMouseMode();
										// Mouse mode
	afx_msg void OnSoftKey();
										// Software keyboard
	afx_msg void OnSoftKeyUI(CCmdUI *pCmdUI);
										// Time adjustment
	afx_msg void OnTimeAdj();
										// Time adjustment
	afx_msg void OnTrap();
										// trap#0
	afx_msg void OnTrapUI(CCmdUI *pCmdUI);
										// trap#0 UI
	afx_msg void OnSaveWav();
										// WAV capture
	afx_msg void OnSaveWavUI(CCmdUI *pCmdUI);
										// New floppy disk
	afx_msg void OnNewFD();
										// New floppy disk
	afx_msg void OnNewDisk(UINT uID);
										// New disk image
	afx_msg void OnOptions();
										// Options

	afx_msg void OnCascade();
										// Cascade windows
	afx_msg void OnCascadeUI(CCmdUI *pCmdUI);
										// Tile windows
	afx_msg void OnTile();
										// Tile windows
	afx_msg void OnTileUI(CCmdUI *pCmdUI);
										// Minimize all
	afx_msg void OnIconic();
										// Minimize all
	afx_msg void OnIconicUI(CCmdUI *pCmdUI);
										// Arrange icons
	afx_msg void OnArrangeIcon();
										// Arrange icons
	afx_msg void OnArrangeIconUI(CCmdUI *pCmdUI);
										// Hide all
	afx_msg void OnHide();
										// Hide all
	afx_msg void OnHideUI(CCmdUI *pCmdUI);
										// Restore all
	afx_msg void OnRestore();
										// Restore all
	afx_msg void OnRestoreUI(CCmdUI *pCmdUI);
										// Window selection
	afx_msg void OnWindow(UINT uID);
										// Window selection
	afx_msg void OnAbout();
										// About and version information

private:
	// Initialization
	BOOL FASTCALL InitChild();
										// Child window initialization
	void FASTCALL InitPos(BOOL bStart = TRUE);
										// Position and rectangle initialization
	void FASTCALL InitShell();
										// Shell-link initialization
	BOOL FASTCALL InitVM();
										// VM initialization
	BOOL FASTCALL InitComponent();
										// Component initialization
	void FASTCALL InitVer();
										// Version initialization
	void FASTCALL InitCmd(LPCTSTR lpszCmd);
										// Command-line processing

	void FASTCALL ReadFile(LPCTSTR pszFileName, CString& str);
	CString FASTCALL ProcessM3u(CString str);

	void FASTCALL ApplyCfg();
										// Apply settings
	void FASTCALL SizeStatus();
										// Status bar size change
	void FASTCALL HideTaskBar(BOOL bHide, BOOL bFore);
										// Hide the taskbar
	BOOL RestoreFrameWnd(BOOL bFullScreen);
										// Restore the window
	void RestoreDiskState();
										// Disk/state restore
	int m_nStatus;
										// Status code
	static const DWORD SigTable[];
										// SRAM signature table


										// Disk/state save
	void FASTCALL CleanSub();
										// Cleanup
	BOOL m_bExit;
										// Exit flag
	BOOL m_bSaved;
										// Frame/disk-state save flag

	// Save/load
	BOOL FASTCALL SaveComponent(const Filepath& path, DWORD dwPos);
										// Save
	BOOL FASTCALL LoadComponent(const Filepath& path, DWORD dwPos);
										// Load

	// Command handler subwindow
	BOOL FASTCALL OnOpenSub(const Filepath& path);
										// Open subwindow
	BOOL FASTCALL OnOpenPrep(const Filepath& path, BOOL bWarning = TRUE);
										// Open check
	void FASTCALL OnSaveSub(const Filepath& path);
										// Save subwindow
	void FASTCALL OnFDOpen(int nDrive);
										// Floppy open
	void FASTCALL OnFDEject(int nDrive);
										// Floppy eject
	void FASTCALL OnFDWriteP(int nDrive);
										// Floppy write-protect save
	void FASTCALL OnFDForce(int nDrive);
										// Floppy force eject
	void FASTCALL OnFDInvalid(int nDrive);
										// Floppy invalid insert
	void FASTCALL OnFDMedia(int nDrive, int nMedia);
										// Floppy media
	void FASTCALL OnFDMRU(int nDrive, int nMRU);
										// Floppy MRU
	int m_nFDDStatus[2];
										// Floppy status

	// Device, view, and component
	FDD *m_pFDD;
										// FDD
	SASI *m_pSASI;
										// SASI
	SCSI *m_pSCSI;
										// SCSI
	Scheduler *m_pScheduler;
										// Scheduler
	Keyboard *m_pKeyboard;
										// Keyboard
	Mouse *m_pMouse;
										// Mouse
	CDrawView *m_pDrawView;
										// Draw view
	CComponent *m_pFirstComponent;
										// First component
	CScheduler *m_pSch;
										// Scheduler
	CSound *m_pSound;
										// Sound
	CInput *m_pInput;
										// Input
	CPort *m_pPort;
										// Port
	CMIDI *m_pMIDI;
										// MIDI
	CTKey *m_pTKey;
										// TrueKey
	CHost *m_pHost;
										// Host
	CInfo *m_pInfo;
										// Info
	CConfig *m_pConfig;
										// Configuration

	// Fullscreen

										// Fullscreen flag
	DEVMODE m_DevMode;
										// Screen-size memory
	HWND m_hTaskBar;
										// Taskbar
	int m_nWndLeft;
										// Window mode X
	int m_nWndTop;
										// Window mode Y

	// Subwindow
	CString m_strWndClsName;
										// Window class name

	// Status view
	void FASTCALL CreateStatusView();
										// Status view creation
	void FASTCALL DestroyStatusView();
										// Status view destruction
	CStatusView *m_pStatusView;
										// Status view

	// Status bar
	void FASTCALL ShowStatus();
										// Status bar display
	void FASTCALL ResetStatus();
										// Status bar reset
	CStatusBar m_StatusBar;
										// Status bar
	BOOL m_bStatusBar;
										// Status bar display flag

	// Menu
	void FASTCALL ShowMenu();
										// Menu bar display
	CMenu m_Menu;
										// Main menu
	BOOL m_bMenuBar;
										// Menu bar display flag
	CMenu m_PopupMenu;
										// Popup menu
	BOOL m_bPopupMenu;
										// Popup menu execution

	// Caption
	void FASTCALL ShowCaption();
										// Caption display
	void FASTCALL ResetCaption();
										// Caption reset
	BOOL m_bCaption;
										// Caption display flag

	// Info
	void FASTCALL SetInfo(CString& strInfo);
										// Info string set

	// Shell-link
	ULONG m_uNotifyId;
										// Shell notification ID
	SHChangeNotifyEntry m_fsne[1];
										// Shell notification entry

	// State file
	void FASTCALL UpdateExec();
										// Update (execute)
	DWORD m_dwExec;
										// Save-after-execute counter

	// Configuration
	BOOL m_bMouseMid;
										// Mouse middle button enabled
	BOOL m_bPopup;
										// Popup mode
	BOOL m_bAutoMouse;
										// Automatic mouse mode

	DECLARE_MESSAGE_MAP()
										// Message map
};

#endif	// mfc_frm_h
#endif	// _WIN32
