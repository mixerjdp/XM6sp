//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[MFC application]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "schedule.h"
#include "memory.h"
#include "sasi.h"
#include "scsi.h"
#include "fdd.h"
#include "fdc.h"
#include "fdi.h"
#include "render.h"
#include "mfc_frm.h"
#include "mfc_draw.h"
#include "mfc_res.h"
#include "mfc_sub.h"
#include "mfc_cpu.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_snd.h"
#include "mfc_inp.h"
#include "mfc_port.h"
#include "mfc_midi.h"
#include "mfc_tkey.h"
#include "mfc_host.h"
#include "mfc_info.h"
#include "mfc_cfg.h"
#include "mfc_stat.h"
static void FASTCALL VMHostSyncLockCallback(void *user)
{
	(void)user;
	::LockVM();
}

static void FASTCALL VMHostSyncUnlockCallback(void *user)
{
	(void)user;
	::UnlockVM();
}

static void FASTCALL VMHostMessageCallback(const TCHAR* message, void *user)
{
	(void)user;
	if (message) {
		::OutputDebugString(message);
		::OutputDebugString(_T("\n"));
	}
}


//===========================================================================
//
//	Frame window

//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Shell constant definitions
//	Must be defined by the application, not by an include file.
//
//---------------------------------------------------------------------------
#define SHCNRF_InterruptLevel			0x0001
#define SHCNRF_ShellLevel				0x0002
#define SHCNRF_NewDelivery				0x8000

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CFrmWnd::CFrmWnd()
{
	// VM pointer and status codes
	::pVM = NULL;
	m_nStatus = -1;

	// Devices
	m_pFDD = NULL;
	m_pSASI = NULL;
	m_pSCSI = NULL;
	m_pScheduler = NULL;
	m_pKeyboard = NULL;
	m_pMouse = NULL;

	// Components
	m_pFirstComponent = NULL;
	m_pDrawView = NULL;
	m_pStatusView = NULL;
	m_pSch = NULL;
	m_pSound = NULL;
	m_pInput = NULL;
	m_pPort = NULL;
	m_pMIDI = NULL;
	m_pTKey = NULL;
	m_pHost = NULL;
	m_pInfo = NULL;
	m_pConfig = NULL;

	// Full-screen state
	m_bFullScreen = FALSE;
	m_bBorderless = FALSE;
	m_dwPrevStyle = 0;
	m_dwPrevExStyle = 0;
	memset(&m_wpPrev, 0, sizeof(m_wpPrev));
	m_hTaskBar = NULL;
	memset(&m_DevMode, 0, sizeof(m_DevMode));
	m_nWndLeft = 0;
	m_nWndTop = 0;
	m_bVSyncEnabled = TRUE;

	// Child window
	m_strWndClsName.Empty();

	// Status bar, menu bar, and caption
	m_bStatusBar = FALSE;
	m_bMenuBar = TRUE;
	m_bCaption = TRUE;

	// Shell notifications
	m_uNotifyId = NULL;

	// Settings
	m_bMouseMid = TRUE;
	m_bPopup = FALSE;
	m_bAutoMouse = TRUE;

	// Other state
	m_bExit = FALSE;
	m_bSaved = FALSE;
	m_nFDDStatus[0] = 0;
	m_nFDDStatus[1] = 0;
	m_dwExec = 0;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CFrmWnd, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_MESSAGE(WM_DISPLAYCHANGE, OnDisplayChange)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_MOVE()
	ON_WM_ACTIVATE()
	ON_WM_ACTIVATEAPP()
	ON_WM_ENTERMENULOOP()
	ON_WM_EXITMENULOOP()
	ON_WM_PARENTNOTIFY()
	ON_MESSAGE(WM_KICK, OnKick)
	ON_WM_DRAWITEM()
	ON_WM_CONTEXTMENU()
	ON_MESSAGE(WM_POWERBROADCAST, OnPowerBroadCast)
	ON_WM_SYSCOMMAND()
#if _MFC_VER >= 0x700
	ON_WM_COPYDATA()
#else
	ON_MESSAGE(WM_COPYDATA, OnCopyData)
#endif
	ON_WM_ENDSESSION()
	ON_MESSAGE(WM_SHELLNOTIFY, OnShellNotify)

	ON_COMMAND(ID_FILE_CARGAR40006, OnFastOpen)

	ON_COMMAND(IDM_OPEN, OnOpen)
	ON_UPDATE_COMMAND_UI(IDM_OPEN, OnOpenUI)
	ON_COMMAND(IDM_SAVE, OnSave)
	ON_UPDATE_COMMAND_UI(IDM_SAVE, OnSaveUI)
	ON_COMMAND(IDM_SAVEAS, OnSaveAs)
	ON_UPDATE_COMMAND_UI(IDM_SAVEAS, OnSaveAsUI)
	ON_COMMAND(IDM_RESET, OnReset)
	ON_UPDATE_COMMAND_UI(IDM_RESET, OnResetUI)
	ON_COMMAND(IDM_SAVECUSTOMCONFIG, OnScc)
	ON_UPDATE_COMMAND_UI(IDM_SAVECUSTOMCONFIG, OnSccUI)
	ON_COMMAND(IDM_SAVEGLOBALCONFIG, OnSgc)
	ON_UPDATE_COMMAND_UI(IDM_SAVEGLOBALCONFIG, OnSgcUI)
	ON_COMMAND(IDM_SAVEGLOBALCONFIGANDRESET, OnSgcr)
	ON_UPDATE_COMMAND_UI(IDM_SAVEGLOBALCONFIGANDRESET, OnSgcrUI)
	ON_COMMAND(IDM_INTERRUPT, OnInterrupt)
	ON_UPDATE_COMMAND_UI(IDM_INTERRUPT, OnInterruptUI)
	ON_COMMAND(IDM_POWER, OnPower)
	ON_UPDATE_COMMAND_UI(IDM_POWER, OnPowerUI)
	ON_COMMAND_RANGE(IDM_XM6_MRU0, IDM_XM6_MRU8, OnMRU)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_XM6_MRU0, IDM_XM6_MRU8, OnMRUUI)
	ON_COMMAND(IDM_EXIT, OnExit)

	ON_COMMAND_RANGE(IDM_D0OPEN, IDM_D1_MRU8, OnFD)
	ON_UPDATE_COMMAND_UI(IDM_D0OPEN, OnFDOpenUI)
	ON_UPDATE_COMMAND_UI(IDM_D1OPEN, OnFDOpenUI)
	ON_UPDATE_COMMAND_UI(IDM_D0EJECT, OnFDEjectUI)
	ON_UPDATE_COMMAND_UI(IDM_D1EJECT, OnFDEjectUI)
	ON_UPDATE_COMMAND_UI(IDM_D0WRITEP, OnFDWritePUI)
	ON_UPDATE_COMMAND_UI(IDM_D1WRITEP, OnFDWritePUI)
	ON_UPDATE_COMMAND_UI(IDM_D0FORCE, OnFDForceUI)
	ON_UPDATE_COMMAND_UI(IDM_D1FORCE, OnFDForceUI)
	ON_UPDATE_COMMAND_UI(IDM_D0INVALID, OnFDInvalidUI)
	ON_UPDATE_COMMAND_UI(IDM_D1INVALID, OnFDInvalidUI)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_D0_MEDIA0, IDM_D0_MEDIAF, OnFDMediaUI)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_D1_MEDIA0, IDM_D1_MEDIAF, OnFDMediaUI)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_D0_MRU0, IDM_D0_MRU8, OnFDMRUUI)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_D1_MRU0, IDM_D1_MRU8, OnFDMRUUI)

	ON_COMMAND(IDM_MOOPEN, OnMOOpen)
	ON_UPDATE_COMMAND_UI(IDM_MOOPEN, OnMOOpenUI)
	ON_COMMAND(IDM_MOEJECT, OnMOEject)
	ON_UPDATE_COMMAND_UI(IDM_MOEJECT, OnMOEjectUI)
	ON_COMMAND(IDM_MOWRITEP, OnMOWriteP)
	ON_UPDATE_COMMAND_UI(IDM_MOWRITEP, OnMOWritePUI)
	ON_COMMAND(IDM_MOFORCE, OnMOForce)
	ON_UPDATE_COMMAND_UI(IDM_MOFORCE, OnMOForceUI)
	ON_COMMAND_RANGE(IDM_MO_MRU0, IDM_MO_MRU8, OnMOMRU)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_MO_MRU0, IDM_MO_MRU8, OnMOMRUUI)

	ON_COMMAND(IDM_CDOPEN, OnCDOpen)
	ON_UPDATE_COMMAND_UI(IDM_CDOPEN, OnCDOpenUI)
	ON_COMMAND(IDM_CDEJECT, OnCDEject)
	ON_UPDATE_COMMAND_UI(IDM_CDEJECT, OnCDEjectUI)
	ON_COMMAND(IDM_CDFORCE, OnCDForce)
	ON_UPDATE_COMMAND_UI(IDM_CDFORCE, OnCDForceUI)
	ON_COMMAND_RANGE(IDM_CD_MRU0, IDM_CD_MRU8, OnCDMRU)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_CD_MRU0, IDM_CD_MRU8, OnCDMRUUI)

	ON_COMMAND(IDM_LOG, OnLog)
	ON_UPDATE_COMMAND_UI(IDM_LOG, OnLogUI)
	ON_COMMAND(IDM_SCHEDULER, OnScheduler)
	ON_UPDATE_COMMAND_UI(IDM_SCHEDULER, OnSchedulerUI)
	ON_COMMAND(IDM_DEVICE, OnDevice)
	ON_UPDATE_COMMAND_UI(IDM_DEVICE, OnDeviceUI)
	ON_COMMAND(IDM_CPUREG, OnCPUReg)
	ON_UPDATE_COMMAND_UI(IDM_CPUREG, OnCPURegUI)
	ON_COMMAND(IDM_INT, OnInt)
	ON_UPDATE_COMMAND_UI(IDM_INT, OnIntUI)
	ON_COMMAND(IDM_DISASM, OnDisasm)
	ON_UPDATE_COMMAND_UI(IDM_DISASM, OnDisasmUI)
	ON_COMMAND(IDM_MEMORY, OnMemory)
	ON_UPDATE_COMMAND_UI(IDM_MEMORY, OnMemoryUI)
	ON_COMMAND(IDM_BREAKP, OnBreakP)
	ON_UPDATE_COMMAND_UI(IDM_BREAKP, OnBreakPUI)
	ON_COMMAND(IDM_MFP, OnMFP)
	ON_UPDATE_COMMAND_UI(IDM_MFP, OnMFPUI)
	ON_COMMAND(IDM_DMAC, OnDMAC)
	ON_UPDATE_COMMAND_UI(IDM_DMAC, OnDMACUI)
	ON_COMMAND(IDM_CRTC, OnCRTC)
	ON_UPDATE_COMMAND_UI(IDM_CRTC, OnCRTCUI)
	ON_COMMAND(IDM_VC, OnVC)
	ON_UPDATE_COMMAND_UI(IDM_VC, OnVCUI)
	ON_COMMAND(IDM_RTC, OnRTC)
	ON_UPDATE_COMMAND_UI(IDM_RTC, OnRTCUI)
	ON_COMMAND(IDM_OPM, OnOPM)
	ON_UPDATE_COMMAND_UI(IDM_OPM, OnOPMUI)
	ON_COMMAND(IDM_KEYBOARD, OnKeyboard)
	ON_UPDATE_COMMAND_UI(IDM_KEYBOARD, OnKeyboardUI)
	ON_COMMAND(IDM_FDD, OnFDD)
	ON_UPDATE_COMMAND_UI(IDM_FDD, OnFDDUI)
	ON_COMMAND(IDM_FDC, OnFDC)
	ON_UPDATE_COMMAND_UI(IDM_FDC, OnFDCUI)
	ON_COMMAND(IDM_SCC, OnSCC)
	ON_UPDATE_COMMAND_UI(IDM_SCC, OnSCCUI)
	ON_COMMAND(IDM_CYNTHIA, OnCynthia)
	ON_UPDATE_COMMAND_UI(IDM_CYNTHIA, OnCynthiaUI)
	ON_COMMAND(IDM_SASI, OnSASI)
	ON_UPDATE_COMMAND_UI(IDM_SASI, OnSASIUI)
	ON_COMMAND(IDM_MIDI, OnMIDI)
	ON_UPDATE_COMMAND_UI(IDM_MIDI, OnMIDIUI)
	ON_COMMAND(IDM_SCSI, OnSCSI)
	ON_UPDATE_COMMAND_UI(IDM_SCSI, OnSCSIUI)
//	ON_COMMAND(IDM_TVRAM, OnTVRAM)
//	ON_UPDATE_COMMAND_UI(IDM_TVRAM, OnTVRAMUI)
	ON_COMMAND(IDM_G1024, OnG1024)
	ON_UPDATE_COMMAND_UI(IDM_G1024, OnG1024UI)
	ON_COMMAND_RANGE(IDM_G16P0, IDM_G16P3, OnG16)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_G16P0, IDM_G16P3, OnG16UI)
	ON_COMMAND_RANGE(IDM_G256P0, IDM_G256P1, OnG256)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_G256P0, IDM_G256P1, OnG256UI)
	ON_COMMAND(IDM_G64K, OnG64K)
	ON_UPDATE_COMMAND_UI(IDM_G64K, OnG64KUI)
	ON_COMMAND(IDM_PCG, OnPCG)
	ON_UPDATE_COMMAND_UI(IDM_PCG, OnPCGUI)
	ON_COMMAND_RANGE(IDM_BG0, IDM_BG1, OnBG)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_BG0, IDM_BG1, OnBGUI)
	ON_COMMAND(IDM_PALET, OnPalet)
	ON_UPDATE_COMMAND_UI(IDM_PALET, OnPaletUI)
	ON_COMMAND(IDM_REND_TEXT, OnTextBuf)
	ON_UPDATE_COMMAND_UI(IDM_REND_TEXT, OnTextBufUI)
	ON_COMMAND_RANGE(IDM_REND_GP0, IDM_REND_GP3, OnGrpBuf)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_REND_GP0, IDM_REND_GP3, OnGrpBufUI)
	ON_COMMAND(IDM_REND_PCG, OnPCGBuf)
	ON_UPDATE_COMMAND_UI(IDM_REND_PCG, OnPCGBufUI)
	ON_COMMAND(IDM_REND_BGSPRITE, OnBGSpBuf)
	ON_UPDATE_COMMAND_UI(IDM_REND_BGSPRITE, OnBGSpBufUI)
	ON_COMMAND(IDM_REND_PALET, OnPaletBuf)
	ON_UPDATE_COMMAND_UI(IDM_REND_PALET, OnPaletBufUI)
	ON_COMMAND(IDM_REND_MIX, OnMixBuf)
	ON_UPDATE_COMMAND_UI(IDM_REND_MIX, OnMixBufUI)
	ON_COMMAND(IDM_COMPONENT, OnComponent)
	ON_UPDATE_COMMAND_UI(IDM_COMPONENT, OnComponentUI)
	ON_COMMAND(IDM_OSINFO, OnOSInfo)
	ON_UPDATE_COMMAND_UI(IDM_OSINFO, OnOSInfoUI)
	ON_COMMAND(IDM_SOUND, OnSound)
	ON_UPDATE_COMMAND_UI(IDM_SOUND, OnSoundUI)
	ON_COMMAND(IDM_INPUT, OnInput)
	ON_UPDATE_COMMAND_UI(IDM_INPUT, OnInputUI)
	ON_COMMAND(IDM_PORT, OnPort)
	ON_UPDATE_COMMAND_UI(IDM_PORT, OnPortUI)
	ON_COMMAND(IDM_BITMAP, OnBitmap)
	ON_UPDATE_COMMAND_UI(IDM_BITMAP, OnBitmapUI)
	ON_COMMAND(IDM_MIDIDRV, OnMIDIDrv)
	ON_UPDATE_COMMAND_UI(IDM_MIDIDRV, OnMIDIDrvUI)
	ON_COMMAND(IDM_CAPTION, OnCaption)
	ON_UPDATE_COMMAND_UI(IDM_CAPTION, OnCaptionUI)
	ON_COMMAND(IDM_MENU, OnMenu)
	ON_UPDATE_COMMAND_UI(IDM_MENU, OnMenuUI)
	ON_COMMAND(IDM_STATUS, OnStatus)
	ON_UPDATE_COMMAND_UI(IDM_STATUS, OnStatusUI)
	ON_COMMAND_RANGE(IDM_SCALE_100, IDM_SCALE_300, OnWindowScale)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_SCALE_100, IDM_SCALE_300, OnWindowScaleUI)
	ON_COMMAND(IDM_REFRESH, OnRefresh)
	ON_COMMAND(IDM_FULLSCREEN, OnFullScreen)
	ON_UPDATE_COMMAND_UI(IDM_FULLSCREEN, OnFullScreenUI)
	ON_COMMAND(IDM_RENDER_FAST, OnRenderFast)
	ON_UPDATE_COMMAND_UI(IDM_RENDER_FAST, OnRenderFastUI)
	ON_COMMAND(IDM_YMFM, OnYmfm)
	ON_UPDATE_COMMAND_UI(IDM_YMFM, OnYmfmUI)
	ON_COMMAND(IDM_TOGGLE_RENDERER, OnToggleRenderer)
	ON_COMMAND(IDM_TOGGLE_OSD, OnToggleOSD)
	ON_COMMAND(IDM_TOGGLE_VSYNC, OnToggleVSync)
	ON_COMMAND(IDM_TOGGLE_SHADER, OnToggleShader)
	ON_UPDATE_COMMAND_UI(IDM_TOGGLE_SHADER, OnToggleShaderUI)

	ON_COMMAND(IDM_EXEC, OnExec)
	ON_UPDATE_COMMAND_UI(IDM_EXEC, OnExecUI)
	ON_COMMAND(IDM_BREAK, OnBreak)
	ON_UPDATE_COMMAND_UI(IDM_BREAK, OnBreakUI)
	ON_COMMAND(IDM_TRACE, OnTrace)
	ON_UPDATE_COMMAND_UI(IDM_TRACE, OnTraceUI)

	ON_COMMAND(IDM_MOUSEMODE, OnMouseMode)
	ON_COMMAND(IDM_SOFTKEY, OnSoftKey)
	ON_UPDATE_COMMAND_UI(IDM_SOFTKEY, OnSoftKeyUI)
	ON_COMMAND(IDM_TIMEADJ, OnTimeAdj)
	ON_COMMAND(IDM_TRAP0, OnTrap)
	ON_UPDATE_COMMAND_UI(IDM_TRAP0, OnTrapUI)
	ON_COMMAND(IDM_SAVEWAV, OnSaveWav)
	ON_UPDATE_COMMAND_UI(IDM_SAVEWAV, OnSaveWavUI)
	ON_COMMAND(IDM_NEWFD, OnNewFD)
	ON_COMMAND_RANGE(IDM_NEWSASI, IDM_NEWMO, OnNewDisk)
	ON_COMMAND(IDM_OPTIONS, OnOptions)

	ON_COMMAND(IDM_CASCADE, OnCascade)
	ON_UPDATE_COMMAND_UI(IDM_CASCADE, OnCascadeUI)
	ON_COMMAND(IDM_TILE, OnTile)
	ON_UPDATE_COMMAND_UI(IDM_TILE, OnTileUI)
	ON_COMMAND(IDM_ICONIC, OnIconic)
	ON_UPDATE_COMMAND_UI(IDM_ICONIC, OnIconicUI)
	ON_COMMAND(IDM_ARRANGEICON, OnArrangeIcon)
	ON_UPDATE_COMMAND_UI(IDM_ARRANGEICON, OnArrangeIconUI)
	ON_COMMAND(IDM_HIDE, OnHide)
	ON_UPDATE_COMMAND_UI(IDM_HIDE, OnHideUI)
	ON_COMMAND(IDM_RESTORE, OnRestore)
	ON_UPDATE_COMMAND_UI(IDM_RESTORE, OnRestoreUI)
	ON_COMMAND_RANGE(IDM_SWND_START, IDM_SWND_END, OnWindow)

	ON_COMMAND(IDM_ABOUT, OnAbout)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::Init()
{
	// Create the main frame window

	if (!Create(NULL, _T("XM6"),
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
			WS_BORDER | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
			rectDefault, NULL, NULL, 0, NULL)) {
		return FALSE;
	}

	// Leave the rest of initialization to OnCreate.
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window creation preprocessing
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	// Base class processing
	if (!CFrameWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// Remove client-edge border
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CFrmWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	LONG lUser;
	CMenu *pSysMenu;
	UINT nCount;
	CString string;

	// Base class processing
	if (CFrameWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Set custom user data marker
	lUser = (LONG)MAKEID('X', 'M', '6', ' ');
	::SetWindowLong(m_hWnd, GWL_USERDATA, lUser);

	// Accelerator, icon, and IME context
	LoadAccelTable(MAKEINTRESOURCE(IDR_ACCELERATOR));
	SetIcon(AfxGetApp()->LoadIcon(IDI_APPICON), TRUE);
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Window menus
	if (::IsJapanese()) {
		// Japanese menu resources
		m_Menu.LoadMenu(IDR_MENU);
		m_PopupMenu.LoadMenu(IDR_MENUPOPUP);
	}
	else {
		// English menu resources
		m_Menu.LoadMenu(IDR_US_MENU);
		m_PopupMenu.LoadMenu(IDR_US_MENUPOPUP);
	}
	SetMenu(&m_Menu);
	m_bMenuBar = TRUE;
	m_bPopupMenu = FALSE;

	// System menu
	::GetMsg(IDS_STDWIN, string);
	pSysMenu = GetSystemMenu(FALSE);
	ASSERT(pSysMenu);
	nCount = pSysMenu->GetMenuItemCount();

	// Insert "Standard window position"
	pSysMenu->InsertMenu(nCount - 2, MF_BYPOSITION | MF_STRING, IDM_STDWIN, string);
	pSysMenu->InsertMenu(nCount - 2, MF_BYPOSITION | MF_SEPARATOR);

	// Initialize child views
	if (!InitChild()) {
		return -1;
	}

	// Initialize window position and rectangle
	InitPos();

	// Initialize shell notifications
	InitShell();

	// Initialize virtual machine
	if (!InitVM()) {
		// VM initialization failed
		m_nStatus = 1;
		PostMessage(WM_KICK, 0, 0);
		return 0;
	}

	// Pass executable version info to the VM
	InitVer();

	// Cache device pointers
	m_pFDD = (FDD*)::GetVM()->SearchDevice(MAKEID('F', 'D', 'D', ' '));
	ASSERT(m_pFDD);
	m_pSASI = (SASI*)::GetVM()->SearchDevice(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(m_pSASI);
	m_pSCSI = (SCSI*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(m_pSCSI);
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);
	m_pKeyboard = (Keyboard*)::GetVM()->SearchDevice(MAKEID('K', 'E', 'Y', 'B'));
	ASSERT(m_pKeyboard);
	m_pMouse = (Mouse*)::GetVM()->SearchDevice(MAKEID('M', 'O', 'U', 'S'));
	ASSERT(m_pMouse);

	// Create and initialize components
	if (!InitComponent()) {
		// Component initialization failed
		m_nStatus = 2;
		PostMessage(WM_KICK, 0, 0);
		return 0;
	}

	// Apply settings (like OnOption) while VM is locked
	::LockVM();
	ApplyCfg();
	::UnlockVM();

	// Reset VM
	::GetVM()->Reset();

	// Restore frame state while startup status is non-zero
	ASSERT(m_nStatus != 0);
	RestoreFrameWnd(FALSE);

	// Post kick message and continue startup
	m_nStatus = 0;
	PostMessage(WM_KICK, 0, 0);
	return 0;
}

//---------------------------------------------------------------------------
//
//	Child view initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::InitChild()
{
	HDC hDC;
	HFONT hFont;
	HFONT hDefFont;
	TEXTMETRIC tm;
	int i;
	int nWidth;
	UINT uIndicator[6];

	// Restore shader setting before initializing the view
	Config config;
	memset(&config, 0, sizeof(config));
	if (m_pConfig) {
		m_pConfig->GetConfig(&config);
	}
	else {
		// Early startup path: config component is not created yet.
		config.render_shader = FALSE;
	}

	// Create view with the initial shader state
	m_pDrawView = new CDrawView;
	if (!m_pDrawView->Init(this, config.render_shader)) {
		return FALSE;
	}

	// Initialize status bar contents
	ResetStatus();

	// Create status bar
	if (!m_StatusBar.Create(this, WS_CHILD | WS_VISIBLE | CBRS_BOTTOM,
							AFX_IDW_STATUS_BAR)) {
		return FALSE;
	}
	m_bStatusBar = TRUE;
	uIndicator[0] = ID_SEPARATOR;
	for (i=1; i<6; i++) {
		uIndicator[i] = (UINT)i;
	}
	m_StatusBar.SetIndicators(uIndicator, 6);

	// Get text metrics
	hDC = ::GetDC(m_hWnd);
	hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	hDefFont = (HFONT)::SelectObject(hDC, hFont);
	ASSERT(hDefFont);
	::GetTextMetrics(hDC, &tm);
	::SelectObject(hDC, hDefFont);
	::ReleaseDC(m_hWnd, hDC);

	// Pane width setup loop
	m_StatusBar.SetPaneInfo(0, 0, SBPS_NOBORDERS | SBPS_STRETCH, 0);
	nWidth = 0;
	for (i=1; i<6; i++) {
		switch (i) {
			// FD0, FD1
			case 1:
			case 2:
				nWidth = tm.tmAveCharWidth * 32;
				break;

			// HD BUSY
			case 3:
				nWidth = tm.tmAveCharWidth * 10;
				break;

			// TIMER
			case 4:
				nWidth = tm.tmAveCharWidth * 9;
				break;

			// POWER
			case 5:
				nWidth = tm.tmAveCharWidth * 9;
				break;
		}
		m_StatusBar.SetPaneInfo(i, i, SBPS_NORMAL | SBPS_OWNERDRAW, nWidth);
	}

	// Recalculate layout
	RecalcLayout();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Initialize position and window rectangle
//	If bStart is FALSE, restore position only in windowed mode.
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitPos(BOOL bStart)
{
	int cx;
	int cy;
	CRect rect;
	CRect rectStatus;
	CRect rectWnd;
	Config config;
	int scaleIndex;
	int scalePercent;

	ASSERT(this);

	// Get screen size and current window rectangle
	cx = ::GetSystemMetrics(SM_CXSCREEN);
	cy = ::GetSystemMetrics(SM_CYSCREEN);
	GetWindowRect(&rectWnd);
	memset(&config, 0, sizeof(config));
	if (m_pConfig) {
		m_pConfig->GetConfig(&config);
	}
	else {
		// Early startup path before component creation.
		config.window_scale = 0;
	}

	scaleIndex = config.window_scale;
	if (scaleIndex < 0) {
		scaleIndex = 0;
	}
	if (scaleIndex > 4) {
		scaleIndex = 4;
	}
	scalePercent = 100 + (scaleIndex * 50);

	// On 800x600 or smaller, force full-screen-sized window
	if ((cx <= 800) || (cy <= 600)) {
		if ((rectWnd.left != 0) || (rectWnd.top != 0)) {
			SetWindowPos(&wndTop, 0, 0, cx, cy, SWP_NOZORDER);
			return;
		}
		if ((rectWnd.Width() != cx) || (rectWnd.Height() != cy)) {
			SetWindowPos(&wndTop, 0, 0, cx, cy, SWP_NOZORDER);
			return;
		}
		return;
	}


	/* Set main-window size and fullscreen size here */
	// 768x512 is treated as the 1.0x base size.
	rect.left = 0;
	rect.top = 0;

	/* In full-screen mode, use full monitor resolution */
	if (m_bFullScreen)
	{
		rect.right = cx;
		rect.bottom = cy;
	}
	else /* In windowed mode, 1024x768 is used */
	{
		rect.right = (768 * scalePercent) / 100;
		rect.bottom = (512 * scalePercent) / 100;
	}
	::AdjustWindowRectEx(&rect, GetView()->GetStyle(), FALSE, GetView()->GetExStyle());
	m_StatusBar.GetWindowRect(&rectStatus);
	rect.bottom += rectStatus.Height();
	::AdjustWindowRectEx(&rect, GetStyle(), TRUE, GetExStyle());

	// rect can become offset; normalize to width/height from origin
	rect.right -= rect.left;
	rect.left = 0;
	rect.bottom -= rect.top;
	rect.top = 0;

	// Center window when there is room
	if (rect.right < cx) {
		rect.left = (cx - rect.right) / 2;
	}
	if (rect.bottom < cy) {
		rect.top = (cy - rect.bottom) / 2;
	}

	// Branch by startup vs. runtime fullscreen/window transition
	if (bStart) {
		// Save initial window position once (RestoreFrameWnd may run later)
		m_nWndLeft = rect.left;
		m_nWndTop = rect.top;
	}
	else {
		// Keep corrected coordinates only in windowed mode
		if (!m_bFullScreen) {
			if ((rect.left == 0) && (rect.top == 0)) {
				// If WM_DISPLAYCHANGE shrank the window, store new position
				m_nWndLeft = rect.left;
				m_nWndTop = rect.top;
			}
			else {
				// Otherwise (including fullscreen->window), restore saved position
				rect.left = m_nWndLeft;
				rect.top = m_nWndTop;
			}
		}
	}


	if ((rect.left != rectWnd.left) || (rect.top != rectWnd.top)) {
		SetWindowPos(&wndTop, rect.left, rect.top, rect.right, rect.bottom, 0);
		return;
	}
	if ((rect.right != rectWnd.Width()) || (rect.bottom != rectWnd.Height())) {
		SetWindowPos(&wndTop, rect.left, rect.top, rect.right, rect.bottom, 0);
		return;
	}
}

//---------------------------------------------------------------------------
//
//	Recompute frame layout for the selected window scale
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ApplyWindowScale()
{
	InitPos(FALSE);
}

//---------------------------------------------------------------------------
//
//	Shell integration initialization
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitShell()
{
	int nSources;

	// Build shell notification flags
	if (::IsWinNT()) {
		// Windows 2000/XP: include shared-memory delivery flag
		nSources = SHCNRF_InterruptLevel | SHCNRF_ShellLevel | SHCNRF_NewDelivery;
	}
	else {
		// Windows 9x: shared-memory delivery is not used
		nSources = SHCNRF_InterruptLevel | SHCNRF_ShellLevel;
	}

	// Initialize one registration entry
	m_fsne[0].pidl = NULL;
	m_fsne[0].fRecursive = FALSE;

	// Register for shell media-change notifications
	m_uNotifyId = ::SHChangeNotifyRegister(m_hWnd,
							nSources,
							SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED | SHCNE_DRIVEADD | SHCNE_DRIVEREMOVED,
							WM_SHELLNOTIFY,
							sizeof(m_fsne)/sizeof(m_fsne[0]),
							m_fsne);
	ASSERT(m_uNotifyId);
}

//---------------------------------------------------------------------------
//
//	Virtual machine initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::InitVM()
{
	host_services_t host_services;
	host_services.lock_vm = VMHostSyncLockCallback;
	host_services.unlock_vm = VMHostSyncUnlockCallback;
	host_services.message = VMHostMessageCallback;
	host_services.user = NULL;

	::pVM = new VM;
	::GetVM()->SetHostServices(&host_services);
	if (!::GetVM()->Init()) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Component initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::InitComponent()
{
	BOOL bSuccess;
	CComponent *pComponent;

	ASSERT(!m_pFirstComponent);
	ASSERT(!m_pSch);
	ASSERT(!m_pSound);
	ASSERT(!m_pInput);
	ASSERT(!m_pPort);
	ASSERT(!m_pMIDI);
	ASSERT(!m_pTKey);
	ASSERT(!m_pHost);
	ASSERT(!m_pInfo);
	ASSERT(!m_pConfig);

	// Construct in order (config first, scheduler last)
	m_pConfig = new CConfig(this);
	m_pFirstComponent = m_pConfig;
	m_pSound = new CSound(this);
	m_pFirstComponent->AddComponent(m_pSound);
	m_pInput = new CInput(this);
	m_pFirstComponent->AddComponent(m_pInput);
	m_pPort = new CPort(this);
	m_pFirstComponent->AddComponent(m_pPort);
	m_pMIDI = new CMIDI(this);
	m_pFirstComponent->AddComponent(m_pMIDI);
	m_pTKey = new CTKey(this);
	m_pFirstComponent->AddComponent(m_pTKey);
	m_pHost = new CHost(this);
	m_pFirstComponent->AddComponent(m_pHost);
	m_pInfo = new CInfo(this, &m_StatusBar);
	m_pFirstComponent->AddComponent(m_pInfo);
	m_pSch = new CScheduler(this);
	m_pFirstComponent->AddComponent(m_pSch);

	// Initialize all components
	pComponent = m_pFirstComponent;
	bSuccess = TRUE;

	// Iterate component list
	while (pComponent) {
		if (!pComponent->Init()) {
			bSuccess = FALSE;
		}
		pComponent = pComponent->GetNextComponent();
	}

	return bSuccess;
}

//---------------------------------------------------------------------------
//
//	Version initialization
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitVer()
{
	TCHAR szPath[_MAX_PATH];
	DWORD dwHandle;
	DWORD dwLength;
	BYTE *pVerInfo;
	VS_FIXEDFILEINFO *pFileInfo;
	UINT uLength;
	DWORD dwMajor;
	DWORD dwMinor;

	ASSERT(this);

	// Get executable path
	::GetModuleFileName(NULL, szPath, _MAX_PATH);

	// Read version info block
	dwLength = GetFileVersionInfoSize(szPath, &dwHandle);
	if (dwLength == 0) {
		return;
	}

	pVerInfo = new BYTE[ dwLength ];
	if (::GetFileVersionInfo(szPath, dwHandle, dwLength, pVerInfo) == 0) {
		return;
	}

	// Query fixed file version info
	if (::VerQueryValue(pVerInfo, _T("\\"), (LPVOID*)&pFileInfo, &uLength) == 0) {
		delete[] pVerInfo;
		return;
	}

	// Build version numbers and pass them to the VM
	dwMajor = (DWORD)HIWORD(pFileInfo->dwProductVersionMS);
	dwMinor = (DWORD)(LOWORD(pFileInfo->dwProductVersionMS) * 16
					+ HIWORD(pFileInfo->dwProductVersionLS));
	::GetVM()->SetVersion(dwMajor, dwMinor);

	// Done
	delete[] pVerInfo;
}



void FASTCALL CFrmWnd::ReadFile(LPCTSTR pszFileName, CString& str)
{
   TRY
   {
      CFile file(pszFileName, CFile::modeRead);
	  UINT dwLength = (UINT)file.GetLength();
      file.Read(str.GetBuffer(dwLength), dwLength);
      str.ReleaseBuffer();
   }
   CATCH_ALL(e)
   {
      str.Empty();
e->ReportError();// see what's going wrong
   }
   END_CATCH_ALL
}



CString CFrmWnd::ProcessM3u(CString str)
{
	CString newPath = m_strXM6FilePath;
	PathRemoveFileSpecA(newPath.GetBuffer());
	newPath.ReleaseBuffer();

	int curPos = 0;
	CString resToken = str.Tokenize(_T("\r\n"), curPos);
	CString fullString;

	while (!resToken.IsEmpty())
	{
		fullString += "\"" + newPath + "\\" + resToken + "\"  ";
		resToken = str.Tokenize(_T("\r\n"), curPos);
	}

	return fullString;
}



//---------------------------------------------------------------------------
//
//	Command-line processing
//	Shared by startup command line and WM_COPYDATA
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitCmd(LPCTSTR lpszCmd)
{
	ASSERT(this);
	ASSERT(lpszCmd);

	CString sz;
	sz.Format(_T("%s"), lpszCmd);
	CString fileName = sz.Mid(sz.ReverseFind('\\') + 1);

	m_strXM6FilePath = lpszCmd;
	m_strXM6FileName = fileName;

	/* Parse and apply command-line parameters here */

	CString str = m_strXM6FilePath;
	CString extensionArchivo = "";

	int curPos = 0;
	CString resToken = str.Tokenize(_T("."), curPos);	// Tokenize full path by dot to get extension
	while (!resToken.IsEmpty())
	{
		extensionArchivo = resToken;
		resToken = str.Tokenize(_T("."), curPos);
	}

	/* If this is an M3U file, parse and expand its entries */
	if (extensionArchivo.MakeUpper() == "M3U")
	{
		CString m3uContent, cont2;
		ReadFile(lpszCmd, m3uContent);
		cont2 = ProcessM3u(m3uContent).Trim();
		strcpy((char*)lpszCmd, cont2);
	}

	// lpszCmd contains the full command line including quotes
	// Initialize parsing flags
	BOOL bReset = FALSE;

	// Create CString from command line
	CString cmdString(lpszCmd);

	// Split command line into parts
	curPos = 0;
	CString part = cmdString.Tokenize(_T(" "), curPos);
	int i = 0;
	// Process each token
	while (!part.IsEmpty())
	{
		// Strip surrounding quotes from each token
		if (part[0] == _T('\"') && part[part.GetLength() - 1] == _T('\"'))
		{
			part = part.Mid(1, part.GetLength() - 2);
		}

		// Convert token to LPCTSTR
		LPCTSTR szPath = (LPCTSTR)part;

		// Try to open token as media/state input
		bReset = InitCmdSub(i, szPath);

		// Read next token
		part = cmdString.Tokenize(_T(" "), curPos);
		i++;
	}

	// Trigger reset if requested by command processing
	if (bReset)
	{
		OnReset();
	}
}



//---------------------------------------------------------------------------
//
//	Command-line sub processing
//	Shared by command line, WM_COPYDATA, and drag-and-drop
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::InitCmdSub(int nDrive, LPCTSTR lpszPath)
{
	Filepath path;
	Fileio fio;
	LPTSTR lpszFile;
	DWORD dwSize;
	TCHAR szPath[_MAX_PATH];
	FDI *pFDI;
	CString strMsg;

	ASSERT(this);
	ASSERT((nDrive == 0) || (nDrive == 1));
	ASSERT(lpszPath);

	// Initialize FDI pointer
	pFDI = NULL;

	// Check whether the file can be opened
	path.SetPath(lpszPath);
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}
	dwSize = fio.GetFileSize();
	fio.Close();

	// Resolve absolute path
	::GetFullPathName(lpszPath, _MAX_PATH, szPath, &lpszFile);
	path.SetPath(szPath);

	// Lock VM
	::LockVM();

	// 128MO or 230MO or 540MO or 640MO
	if ((dwSize == 0x797f400) || (dwSize == 0xd9eea00) ||
		(dwSize == 0x1fc8b800) || (dwSize == 0x25e28000)) {
		// Try to mount as MO media
		nDrive = 2;

		if (!m_pSASI->Open(path)) {
			// MO mount failed
			GetScheduler()->Reset();
			ResetCaption();
			::UnlockVM();
			return FALSE;
		}
	}
	else {
		if (dwSize >= 0x200000) {
			// Try to load as VM state file
			nDrive = 4;

			// Pre-open validation
			if (!OnOpenPrep(path, FALSE)) {
				// Missing files, version mismatch, etc.
				GetScheduler()->Reset();
				ResetCaption();
				::UnlockVM();
				return FALSE;
			}

			// Execute load (handled by OnOpenSub)
			::UnlockVM();
			if (OnOpenSub(path)) {
				Filepath::SetDefaultDir(szPath);
			}
			// No reset required for this path
			return FALSE;
		}
		else {
			// Try to mount as floppy disk image
			/* Initialize floppy image from command line */

			if (!m_pFDD->Open(nDrive, path)) {
				// Floppy mount failed
				GetScheduler()->Reset();
				ResetCaption();
				::UnlockVM();
				return FALSE;
			}
			pFDI = m_pFDD->GetFDI(nDrive);
		}
	}

	// Reset scheduler state and unlock VM
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// Success: store default directory and add MRU entry
	Filepath::SetDefaultDir(szPath);
	GetConfig()->SetMRUFile(nDrive, szPath);

	// Warn on invalid floppy image
	if (pFDI) {
		if (pFDI->GetID() == MAKEID('B', 'A', 'D', ' ')) {
			::GetMsg(IDS_BADFDI_WARNING, strMsg);
			//MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		}

		// Reset is required only when a floppy is assigned
		return TRUE;
	}

	// Done
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Save components
//	Scheduler is stopped, but CSound and CInput keep running.
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::SaveComponent(const Filepath& path, DWORD dwPos)
{
	Fileio fio;
	DWORD dwID;
	CComponent *pComponent;
	DWORD dwMajor;
	DWORD dwMinor;
	int nVer;

	ASSERT(this);
	ASSERT(dwPos > 0);

	// Build version information value
	::GetVM()->GetVersion(dwMajor, dwMinor);
	nVer = (int)((dwMajor << 8) | dwMinor);

	// Open file and seek to target position
	if (!fio.Open(path, Fileio::Append)) {
		return FALSE;
	}
	if (!fio.Seek(dwPos)) {
		fio.Close();
		return FALSE;
	}

	// Write main component marker
	dwID = MAKEID('M', 'A', 'I', 'N');
	if (!fio.Write(&dwID, sizeof(dwID))) {
		fio.Close();
		return FALSE;
	}

	// Component loop
	pComponent = m_pFirstComponent;
	while (pComponent) {
		// Save component ID
		dwID = pComponent->GetID();
		if (!fio.Write(&dwID, sizeof(dwID))) {
			fio.Close();
			return FALSE;
		}

		// Save component-specific data
		if (!pComponent->Save(&fio, nVer)) {
			fio.Close();
			return FALSE;
		}

		// Next component
		pComponent = pComponent->GetNextComponent();
	}

	// Write end marker
	dwID = MAKEID('E', 'N', 'D', ' ');
	if (!fio.Write(&dwID, sizeof(dwID))) {
		fio.Close();
		return FALSE;
	}

	// Finish
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load components
//	Scheduler is stopped, but CSound and CInput keep running.
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::LoadComponent(const Filepath& path, DWORD dwPos)
{
	Fileio fio;
	DWORD dwID;
	CComponent *pComponent;
	char cHeader[0x10];
	int nVer;

	ASSERT(this);
	ASSERT(dwPos > 0);

	// Open file
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Read header
	if (!fio.Read(cHeader, sizeof(cHeader))) {
		fio.Close();
		return FALSE;
	}

	// Validate header and read version information
	cHeader[0x0a] = '\0';
	nVer = ::strtoul(&cHeader[0x09], NULL, 16);
	nVer <<= 8;
	cHeader[0x0d] = '\0';
	nVer |= ::strtoul(&cHeader[0x0b], NULL, 16);
	cHeader[0x09] = '\0';
	if (strcmp(cHeader, "XM6 DATA ") != 0) {
		fio.Close();
		return FALSE;
	}

	// Seek to component section
	if (!fio.Seek(dwPos)) {
		fio.Close();
		return FALSE;
	}

	// Read main component marker
	if (!fio.Read(&dwID, sizeof(dwID))) {
		fio.Close();
		return FALSE;
	}
	if (dwID != MAKEID('M', 'A', 'I', 'N')) {
		fio.Close();
		return FALSE;
	}

	// Component loop
	for (;;) {
		// Read component ID
		if (!fio.Read(&dwID, sizeof(dwID))) {
			fio.Close();
			return FALSE;
		}

		// End marker check
		if (dwID == MAKEID('E', 'N', 'D', ' ')) {
			break;
		}

		// Find component instance
		pComponent = m_pFirstComponent->SearchComponent(dwID);
		if (!pComponent) {
			// Component existed at save time but is missing now
			fio.Close();
			return FALSE;
		}

		// Load component-specific data
		if (!pComponent->Load(&fio, nVer)) {
			fio.Close();
			return FALSE;
		}
	}

	// Close file
	fio.Close();

	// Apply settings (with VM lock)
	if (GetConfig()->IsApply()) {
		::LockVM();
		ApplyCfg();
		::UnlockVM();
	}

	// Refresh window content
	GetView()->Invalidate(FALSE);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ApplyCfg()
{
	Config config;
	CComponent *pComponent;

	// Retrieve current configuration
	GetConfig()->GetConfig(&config);

	// Apply to VM first
	::GetVM()->ApplyCfg(&config);

	// Then apply to each component
	pComponent = m_pFirstComponent;
	while (pComponent) {
		pComponent->ApplyCfg(&config);
		pComponent = pComponent->GetNextComponent();
	}

	// Then apply to the draw view
	GetView()->ApplyCfg(&config);

	// Sync the top-level window size with the saved scale after the config is applied.
	// InitPos() runs once before the config component exists, so the initial frame size
	// can otherwise stay at the 1.0x fallback even when the INI says otherwise.
	ApplyWindowScale();

	// Popup subwindow mode
	if (config.popup_swnd != m_bPopup) {
		// Clear all subwindows
		GetView()->ClrSWnd();

		// Update flag
		m_bPopup = config.popup_swnd;
	}



	// Frame window mouse options
	m_bMouseMid = config.mouse_mid;
	m_bAutoMouse = config.auto_mouse;

	// Apply VSync setting
	m_bVSyncEnabled = config.render_vsync;
	GetView()->SetVSync(m_bVSyncEnabled);

	// Apply renderer mode setting
	GetView()->ApplyRendererConfig(config.render_mode);

	// Keep SaveState path initialized once
	if (m_strSaveStatePath.GetLength() == 0)
		m_strSaveStatePath = config.ruta_savestate;
	//int msgboxID = MessageBox(m_strSaveStatePath,"rutasave",  2 );
	if (config.mouse_port == 0) {
		// Disable mouse-capture mode when no mouse is connected.
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}
}

//---------------------------------------------------------------------------
//
//	Kick handler
//
//---------------------------------------------------------------------------
LONG CFrmWnd::OnKick(UINT /*uParam*/, LONG /*lParam*/)
{
	CComponent *pComponent;
	CInfo *pInfo;
	Config config;
	CString strMsg;
	MSG msg;
	Memory *pMemory;
	DWORD dwTick20;
	DWORD dwTick40;
	DWORD dwTick80;
	DWORD dwNow;
	LPSTR lpszCmd;
	LPCTSTR lpszCommand;
	BOOL bFullScreen;

	// Handle startup errors first
	switch (m_nStatus) {
		// VM initialization error
		case 1:
			::GetMsg(IDS_INIT_VMERR, strMsg);
			MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
			PostMessage(WM_CLOSE, 0, 0);
			return 0;

		// Component initialization error
		case 2:
			::GetMsg(IDS_INIT_COMERR, strMsg);
			MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
			PostMessage(WM_CLOSE, 0, 0);
			return 0;
	}
	// Normal startup path
	ASSERT(m_nStatus == 0);

	// Check required ROM data
	pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(pMemory);
	if (!pMemory->CheckIPL()) {
		::GetMsg(IDS_INIT_IPLERR, strMsg);
		if (MessageBox(strMsg, NULL, MB_ICONSTOP | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
			PostMessage(WM_CLOSE, 0, 0);
			return 0;
		}
	}
	if (!pMemory->CheckCG()) {
		::GetMsg(IDS_INIT_CGERR, strMsg);
		if (MessageBox(strMsg, NULL, MB_ICONSTOP | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
			PostMessage(WM_CLOSE, 0, 0);
			return 0;
		}
	}

	// Read config (power_off startup option)
	GetConfig()->GetConfig(&config);
	if (config.power_off) {
		// Start with power switch OFF
		::GetVM()->SetPower(FALSE);
		::GetVM()->PowerSW(FALSE);
	}

	// PLAN G: Hold VM lock across initial bootstrap
	::LockVM();

	// Register child-window class
	m_strWndClsName = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS);

	// Enable components; scheduler is enabled later.
	// PLAN G: atomic startup sequence
	GetView()->Enable(TRUE);
	pComponent = m_pFirstComponent;
	while (pComponent) {
		if (pComponent->GetID() == MAKEID('S', 'C', 'H', 'E')) {
			pComponent = pComponent->GetNextComponent();
			continue;
		}
		pComponent->Enable(TRUE);
		pComponent = pComponent->GetNextComponent();
	}

	if (!config.power_off) {
		OnReset();
	}

	RestoreDiskState();

	lpszCmd = AfxGetApp()->m_lpCmdLine;
	lpszCommand = A2T(lpszCmd);
	if (_tcslen(lpszCommand) > 0) {
		InitCmd(lpszCommand);
	}

	bFullScreen = FALSE;
	if (IsZoomed()) {
		ShowWindow(SW_RESTORE);
		bFullScreen = TRUE;
	}

	bFullScreen = RestoreFrameWnd(bFullScreen);
	if (bFullScreen) {
		PostMessage(WM_COMMAND, IDM_FULLSCREEN);
	}

	pComponent = m_pFirstComponent;
	while (pComponent) {
		if (pComponent->GetID() == MAKEID('S', 'C', 'H', 'E')) {
			if (!config.power_off) {
				pComponent->Enable(TRUE);
			}
			break;
		}
		pComponent = pComponent->GetNextComponent();
	}

	::UnlockVM();
	// Main loop
	DWORD dwStartTick = ::GetTickCount();
	BOOL bAutoResetDone = FALSE;
	dwTick20 = ::GetTickCount();
	dwTick40 = dwTick20;
	dwTick80 = dwTick20;
	while (!m_bExit) {
		// Check and pump pending window messages
		if (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			if (!AfxGetApp()->PumpMessage()) {
				::PostQuitMessage(0);
				return 0;
			}
			// Continue to guarantee m_bExit is checked after WM_DESTROY
			continue;
		}

		// Sleep briefly when idle
		if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			Sleep(1);
			if (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
				continue;
			}

			// Refresh info pointer each idle tick
			pInfo = GetInfo();
			if (!pInfo) {
				continue;
			}

			// Update periodic timers
			dwNow = ::GetTickCount();


			if ((dwNow - dwTick20) >= 20) {
				dwTick20 = dwNow;
				pInfo->UpdateStatus();
				UpdateExec();

				// HACK: auto-reset to work around cold-boot bug
				if (!bAutoResetDone && (dwNow - dwStartTick) >= 90) {
					bAutoResetDone = TRUE;
					OnReset();
				}
			}

			if ((dwNow - dwTick40) >= 40) {
				dwTick40 = dwNow;
				// Update render view every 40ms
				GetView()->Update();
			}

			if ((dwNow - dwTick80) >= 80) {
				dwTick80 = dwNow;
				// Update caption/info every 80ms
				pInfo->UpdateCaption();
				pInfo->UpdateInfo();
			}
		}
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	Get window class name
//
//---------------------------------------------------------------------------
LPCTSTR FASTCALL CFrmWnd::GetWndClassName() const
{
	ASSERT(this);
	ASSERT(m_strWndClsName.GetLength() != 0);

	return (LPCTSTR)m_strWndClsName;
}

//---------------------------------------------------------------------------
//
//	Popup mode flag
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::IsPopupSWnd() const
{
	ASSERT(this);
	return m_bPopup;
}

//---------------------------------------------------------------------------
//
//	Window close
//
//---------------------------------------------------------------------------
void CFrmWnd::OnClose()
{
	CString strFormat;
	CString strText;
	Filepath path;

	ASSERT(this);
	ASSERT(!m_bSaved);


	//int msgboxID = MessageBox("close", "Window to close", 2);

/* Save-confirmation dialog is intentionally disabled here */
	// If a valid state file exists, this is where save prompting would run
	::LockVM();
	::GetVM()->GetPath(path);
	::UnlockVM();

	// If a valid state file exists
	if (!path.IsClear()) {
		// If runtime activity history is at least 20ms on Windows side
	/*	if (m_dwExec >= 2) {
			// Confirmation dialog
			::GetMsg(IDS_SAVECLOSE, strFormat);
			strText.Format(strFormat, path.GetFileExt());
			nResult = MessageBox(strText, NULL, MB_ICONQUESTION | MB_YESNOCANCEL);

			// Handle confirmation result
			switch (nResult) {
				// YES
				case IDYES:
					// Save
					OnSaveSub(path);
					break;

				// NO
				case IDNO:
					// Continue without state
					::GetVM()->Clear();
					break;

				// CANCEL
				case IDCANCEL:
					// Abort close and keep running
					return;
			}
		}*/
	}

	// If initialization already completed
	if ((m_nStatus == 0) && !m_bSaved) {
		// Save frame and disk resume state
		SaveFrameWnd();
		SaveDiskState();
		m_bSaved = TRUE;
	}

	// Exit fullscreen first
	if (m_bFullScreen) {
		ASSERT(m_nStatus == 0);
		OnFullScreen();
	}

	// If initialization already completed
	if (m_nStatus == 0) {
		// Release mouse capture mode
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}

	OutputDebugString("\n\nOnClose executed...\n\n");
	// Base class handling
	CFrameWnd::OnClose();
}

//---------------------------------------------------------------------------
//
//	Window ofstroy
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDestroy()
{
	ASSERT(this);

	// If initialization already completed
	if ((m_nStatus == 0) && !m_bSaved)
	{
		// Save frame and disk resume state
		SaveFrameWnd();
		SaveDiskState();
		m_bSaved = TRUE;
	}

	// Exit fullscreen first
	if (m_bFullScreen) {
		ASSERT(m_nStatus == 0);
		OnFullScreen();
	}

	// Shared cleanup path (also used by WM_ENDSESSION)
	CleanSub();


	OutputDebugString("\n\nOnDestroy executed...\n\n");

	// Base class handling
	CFrameWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	End session
//
//---------------------------------------------------------------------------
void CFrmWnd::OnEndSession(BOOL bEnding)
{
	ASSERT(this);

	// Cleanup during system logoff/shutdown
	if (bEnding) {
		// If initialization already completed
		if (m_nStatus == 0) {
			// Save frame and disk resume state
			if (!m_bSaved) {
				SaveFrameWnd();
				SaveDiskState();
				m_bSaved = TRUE;
			}

			// Cleanup
			CleanSub();
		}
	}


	OutputDebugString("\n\nOnEndSession executed...\n\n");

	// Base class handling
	CFrameWnd::OnEndSession(bEnding);
}

//---------------------------------------------------------------------------
//
//	Shared cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::CleanSub()
{
	CComponent *pComponent;
	CComponent *pNext;
	int i;

	// Set exit flag
	m_bExit = TRUE;

	// Disable view and all components
	GetView()->Enable(FALSE);
	pComponent = m_pFirstComponent;
	while (pComponent) {
		pComponent->Enable(FALSE);
		pComponent = pComponent->GetNextComponent();
	}

	// Release mouse capture mode
	if (m_nStatus == 0) {
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}

	// Wait for scheduler execution to settle
	for (i=0; i<8; i++) {
		::LockVM();
		::UnlockVM();
	}

	// Stop scheduler (CScheduler)
	if (m_nStatus == 0) {
		GetScheduler()->Stop();
	}

	// Cleanup and delete all components
	pComponent = m_pFirstComponent;
	while (pComponent) {
		pComponent->Cleanup();
		pComponent = pComponent->GetNextComponent();
	}
	pComponent = m_pFirstComponent;
	while (pComponent) {
		pNext = pComponent->GetNextComponent();
		delete pComponent;
		pComponent = pNext;
	}

	// Cleanup and destroy VM instance
	if (::pVM) {
		::LockVM();
		::GetVM()->Cleanup();
		delete ::pVM;
		::pVM = NULL;
		::UnlockVM();
	}

	// Unregister shell notifications
	if (m_uNotifyId) {
		 VERIFY(::SHChangeNotifyDeregister(m_uNotifyId));
		 m_uNotifyId = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Save frame window state
//
//---------------------------------------------------------------------------
void CFrmWnd::SaveFrameWnd()
{
	CRect rectWnd;
	Config config;

	ASSERT(this);
	ASSERT_VALID(this);

	// Get config snapshot
	GetConfig()->GetConfig(&config);

	// Caption/menu/status visibility
	config.caption = m_bCaption;
	config.menu_bar = m_bMenuBar;
	config.status_bar = m_bStatusBar;

	// Window position
	if (m_bFullScreen) {
		// In fullscreen, keep last windowed position
		config.window_left = m_nWndLeft;
		config.window_top = m_nWndTop;
	}
	else {
		// In windowed mode, save current position
		GetWindowRect(&rectWnd);
		config.window_left = rectWnd.left;
		config.window_top = rectWnd.top;
	}

	// Fullscreen flag
	config.window_full = m_bFullScreen;

	// Shader state
	if (m_pDrawView) {
		config.render_shader = m_pDrawView->IsShaderEnabled();
	}

	// Store updated config
	GetConfig()->SetConfig(&config);
}

//---------------------------------------------------------------------------
//
//	Save disk/media state
//
//---------------------------------------------------------------------------
void CFrmWnd::SaveDiskState()
{
	int nDrive;
	Filepath path;
	Config config;

	ASSERT(this);
	ASSERT_VALID(this);

	// Lock VM
	::LockVM();

	// Get config snapshot
	GetConfig()->GetConfig(&config);

	// Floppy disk state
	for (nDrive=0; nDrive<2; nDrive++) {
		// Ready state
		config.resume_fdi[nDrive] = m_pFDD->IsReady(nDrive, FALSE);

		// If not ready, skip remaining fields
		if (!config.resume_fdi[nDrive]) {
			continue;
		}

		// Current media type
		config.resume_fdm[nDrive]  = m_pFDD->GetMedia(nDrive);

		// Write-protect state
		config.resume_fdw[nDrive] = m_pFDD->IsWriteP(nDrive);
	}

	// MO disk state
	config.resume_mos = m_pSASI->IsReady();
	if (config.resume_mos) {
		config.resume_mow = m_pSASI->IsWriteP();
	}

	// CD-ROM
	config.resume_iso = m_pSCSI->IsReady(FALSE);

	// Save-state file availability
	::GetVM()->GetPath(path);
	config.resume_xm6 = !path.IsClear();

	// Default directory
	_tcscpy(config.resume_path, Filepath::GetDefaultDir());

	// Store updated config
	GetConfig()->SetConfig(&config);

	// Unlock VM
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Restore frame window state
//	OnCreate and OnKick invoke this twice.
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::RestoreFrameWnd(BOOL bFullScreen)
{
	int nWidth;
	int nHeight;
	int nLeft;
	int nTop;
	CRect rectWnd;
	BOOL bValid;
	Config config;

	ASSERT(this);

	// Get config snapshot
	GetConfig()->GetConfig(&config);

	// Keep default placement if window-position restore is disabled
	if (!config.resume_screen) {
		return bFullScreen;
	}

	// Caption
	m_bCaption = config.caption;
	ShowCaption();

	// Menu bar
	m_bMenuBar = config.menu_bar;
	ShowMenu();

	// Status bar
	m_bStatusBar = config.status_bar;
	ShowStatus();

	// Get virtual-screen size and origin
	nWidth = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
	nHeight = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
	nLeft = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
	nTop = ::GetSystemMetrics(SM_YVIRTUALSCREEN);

	// Get current window rectangle
	GetWindowRect(&rectWnd);

	// Restore window position only when it is still within visible bounds
	bValid = TRUE;
	if (config.window_left < nLeft) {
		if (config.window_left < nLeft - rectWnd.Width()) {
			bValid = FALSE;
		}
	}
	else {
		if (config.window_left >= (nLeft + nWidth)) {
			bValid = FALSE;
		}
	}
	if (config.window_top < nTop) {
		if (config.window_top < nTop - rectWnd.Height()) {
			bValid = FALSE;
		}
	}
	else {
		if (config.window_top >= (nTop + nHeight)) {
			bValid = FALSE;
		}
	}

	// Apply restored window position
	if (bValid) {
		SetWindowPos(&wndTop, config.window_left, config.window_top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

		// Update saved work-area position at the same time
		m_nWndLeft = config.window_left;
		m_nWndTop = config.window_top;
	}



	/*char cadena[20],cadena2[20];
    sprintf(cadena, "%d", nHeight);
	sprintf(cadena2, "%d", nWidth);
	 int msgboxID = MessageBox(
       cadena,"Height",
        2 );
	 int msgboxID2 = MessageBox(
       cadena2,"Width",
        2 );
		*/




	// Stop here when VM is not fully initialized.
	if (m_nStatus != 0) {
		return FALSE;
	}





	// Fullscreen restore behavior.
	if (bFullScreen || config.window_full) {
		// Start maximized if requested now or in previous session.
		return TRUE;
	}
	else {
		// Stay in normal windowed mode.
		return FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Restore disk/media state
//
//---------------------------------------------------------------------------
void CFrmWnd::RestoreDiskState()
{
	int nDrive;
	TCHAR szMRU[_MAX_PATH];
	BOOL bResult;
	Filepath path;
	Config config;

	ASSERT(this);

	// Get config snapshot
	GetConfig()->GetConfig(&config);

	// If resume-from-state is enabled, try it first
	if (config.resume_state) {
		// A previous state file exists
		if (config.resume_xm6) {
			// Get saved path
			GetConfig()->GetMRUFile(4, 0, szMRU);
			path.SetPath(szMRU);

			// Pre-open validation
			if (OnOpenPrep(path)) {
				// Open state via subroutine
				if (OnOpenSub(path)) {
					// On success, restore only default directory if configured
					if (config.resume_dir) {
						Filepath::SetDefaultDir(config.resume_path);
					}

					// Stop here after state load (state already contains disk/media state)
					return;
				}
			}
		}
	}

	// Floppy disk resume
	if (config.resume_fd) {
		for (nDrive=0; nDrive<2; nDrive++) {
			// Was media inserted at save time?
			if (!config.resume_fdi[nDrive]) {
				// No inserted media recorded; skip
				continue;
			}

			// Reinsert media
			GetConfig()->GetMRUFile(nDrive, 0, szMRU);
			ASSERT(szMRU[0] != _T('\0'));
			path.SetPath(szMRU);

			// Lock VM and attempt media mount
			::LockVM();
			bResult = m_pFDD->Open(nDrive, path, config.resume_fdm[nDrive]);
			::UnlockVM();

			// If mount fails, skip this drive
			if (!bResult) {
				continue;
			}

			// Restore write-protect flag
			if (config.resume_fdw[nDrive]) {
				::LockVM();
				m_pFDD->WriteP(nDrive, TRUE);
				::UnlockVM();
			}
		}
	}

	// MO resume
	if (config.resume_mo) {
		// Was media inserted at save time?
		if (config.resume_mos) {
			// Reinsert media
			GetConfig()->GetMRUFile(2, 0, szMRU);
			ASSERT(szMRU[0] != _T('\0'));
			path.SetPath(szMRU);

			// Lock VM and attempt media mount
			::LockVM();
			bResult = m_pSASI->Open(path);
			::UnlockVM();

			// If mount succeeds
			if (bResult) {
				// Restore write-protect flag
				if (config.resume_mow) {
					::LockVM();
					m_pSASI->WriteP(TRUE);
					::UnlockVM();
				}
			}
		}
	}

	// CD-ROM
	if (config.resume_cd) {
		// Was media inserted at save time?
		if (config.resume_iso) {
			// Reinsert media
			GetConfig()->GetMRUFile(3, 0, szMRU);
			ASSERT(szMRU[0] != _T('\0'));
			path.SetPath(szMRU);

			// Lock VM and attempt media mount
			::LockVM();
			m_pSCSI->Open(path, FALSE);
			::UnlockVM();
		}
	}

	// Restore default directory
	if (config.resume_dir) {
		Filepath::SetDefaultDir(config.resume_path);
	}
}

//---------------------------------------------------------------------------
//
//	Display change
//
//---------------------------------------------------------------------------
LRESULT CFrmWnd::OnDisplayChange(UINT uParam, LONG lParam)
{
	LRESULT lResult;
	uParam = 0;
	lParam = 0;
	// Base class behavior
	lResult=0;//CFrameWnd::OnDisplayChange(0, uParam, lParam);

	// Ignore while minimized
	if (IsIconic()) {
		return lResult;
	}

	// Recalculate position
	InitPos(FALSE);

	return lResult;
}

//---------------------------------------------------------------------------
//
//	Window background erase
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::OnEraseBkgnd(CDC * /* pDC */)
{
	// Suppress background erase to reduce flicker
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window paint
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPaint()
{
	PAINTSTRUCT ps;

	// Always paint while VM is locked
	::LockVM();

	BeginPaint(&ps);

	// Refresh caption and status when VM is active
	if (m_nStatus == 0) {
		ResetCaption();
		ResetStatus();
	}

	EndPaint(&ps);

	// Unlock VM
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Window move
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMove(int x, int y)
{
	CRect rect;

	// If initialization already completed
	if (m_nStatus == 0) {
		// Check mouse-capture mode
		if (GetInput()->GetMouseMode()) {
			// Rebuild cursor clip rectangle around moved window
			ClipCursor(NULL);
			GetWindowRect(&rect);
			SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
			ClipCursor(&rect);
		}
	}

	// Base class handling
	CFrameWnd::OnMove(x, y);
}

//---------------------------------------------------------------------------
//
//	Activation
//
//---------------------------------------------------------------------------
void CFrmWnd::OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized)
{
	CInput *pInput;
	CScheduler *pScheduler;

	// If initialization already completed
	if (m_nStatus == 0) {
		// Notify input and scheduler about activation change
		pInput = GetInput();
		pScheduler = GetScheduler();
		if (pInput && pScheduler) {
			// On inactive/minimized, deactivate input and slow scheduler
			if ((nState == WA_INACTIVE) || bMinimized) {
				// Stop accepting input and run at inactive speed
				pInput->Activate(FALSE);
				pScheduler->Activate(FALSE);

				// Force mouse mode off (popup-window safety)
				if (pInput->GetMouseMode()) {
					OnMouseMode();
				}
			}
			else {
				// Resume input and normal execution speed
				pInput->Activate(TRUE);
				pScheduler->Activate(TRUE);
			}
		}
	}

	// Base class handling
	CFrameWnd::OnActivate(nState, pWnd, bMinimized);
}

//---------------------------------------------------------------------------
//
//	Application activation
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
void CFrmWnd::OnActivateApp(BOOL bActive, DWORD dwThreadID)
#else
void CFrmWnd::OnActivateApp(BOOL bActive, HTASK hTask)
#endif
{
	// If initialization already completed
	if (m_nStatus == 0) {
		// Full-screen-specific handling
		if (m_bFullScreen) {
			if (bActive) {
				// Becoming active
				HideTaskBar(TRUE, TRUE);
				RecalcStatusView();
			}
			else {
				// Becoming inactive
				HideTaskBar(FALSE, FALSE);
			}
		}
	}

	// Base class handling
#if _MFC_VER >= 0x700
	CFrameWnd::OnActivateApp(bActive, dwThreadID);
#else
	CFrameWnd::OnActivateApp(bActive, hTask);
#endif
}

//---------------------------------------------------------------------------
//
//	Enter menu loop
//
//---------------------------------------------------------------------------
void CFrmWnd::OnEnterMenuLoop(BOOL bTrackPopup)
{
	CInput *pInput;
	CScheduler *pScheduler;

	// Refresh caption before entering menu loop
	ResetCaption();

	::LockVM();

	// Notify input component
	pInput = GetInput();
	if (pInput) {
		pInput->Menu(TRUE);
	}

	// Disable mouse mode so menu can be operated normally
	if (pInput->GetMouseMode()) {
		OnMouseMode();
	}

	// Notify scheduler
	pScheduler = GetScheduler();
	if (pScheduler) {
		pScheduler->Menu(TRUE);
	}

	::UnlockVM();

	// Base class handling
	CFrameWnd::OnEnterMenuLoop(bTrackPopup);
}

//---------------------------------------------------------------------------
//
//	Exit menu loop
//
//---------------------------------------------------------------------------
void CFrmWnd::OnExitMenuLoop(BOOL bTrackPopup)
{
	CInput *pInput;
	CScheduler *pScheduler;

	::LockVM();

	// Notify input component
	pInput = GetInput();
	if (pInput) {
		pInput->Menu(FALSE);
	}

	// Notify scheduler
	pScheduler = GetScheduler();
	if (pScheduler) {
		pScheduler->Menu(FALSE);
	}

	::UnlockVM();

	// Refresh caption after leaving menu loop
	ResetCaption();

	// Base class handling
	CFrameWnd::OnExitMenuLoop(bTrackPopup);
}

//---------------------------------------------------------------------------
//
//	Parent window notification
//
//---------------------------------------------------------------------------
void CFrmWnd::OnParentNotify(UINT message, LPARAM lParam)
{
	CInput *pInput;

	// Forward middle-button events to CInput
	if ((message == WM_MBUTTONDOWN) && (m_nStatus == 0)) {
		// Get input component
		pInput = GetInput();
		if (pInput) {
			// Only enable mouse mode if it is currently disabled
			if (!pInput->GetMouseMode()) {
				// Only when middle-button mouse-mode toggle is enabled
				if (m_bMouseMid) {
					OnMouseMode();
				}
			}
		}
	}

	// Base class handling
	CFrameWnd::OnParentNotify(message, lParam);
}

//---------------------------------------------------------------------------
//
//	Context menu
//
//---------------------------------------------------------------------------
void CFrmWnd::OnContextMenu(CWnd * /*pWnd*/, CPoint pos)
{
	CMenu *pMenu;
	SHORT sF10;
	SHORT sShift;

	// Handle keyboard-triggered context menu invocation
	if ((pos.x == -1) && (pos.y == -1)) {
		// Check scheduler and input state
		if (GetScheduler()->IsEnable()) {
			if (GetInput()->IsActive() && !GetInput()->IsMenu()) {
				// If DIK_APPS is mapped
				if (GetInput()->IsKeyMapped(DIK_APPS)) {
					// Check whether SHIFT+F10 is currently pressed
					sF10 = ::GetAsyncKeyState(VK_F10);
					sShift = ::GetAsyncKeyState(VK_SHIFT);
					if (((sF10 & 0x8000) == 0) || ((sShift & 0x8000) == 0)) {
						// Ignore VK_APPS-generated invocation
						return;
					}
				}
			}
		}

		// If mouse mode is active, release it for keyboard menu invocation
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}
	else {
		// Ignore mouse-triggered context menu while mouse mode is active
		if (GetInput()->GetMouseMode()) {
			return;
		}
	}

	// Show popup menu
	m_bPopupMenu = TRUE;
	pMenu = m_PopupMenu.GetSubMenu(0);
	pMenu->TrackPopupMenu(TPM_CENTERALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							pos.x, pos.y, this, 0);
	m_bPopupMenu = FALSE;
}

//---------------------------------------------------------------------------
//
//	Power change notification
//
//---------------------------------------------------------------------------
LONG CFrmWnd::OnPowerBroadCast(UINT /*uParam*/, LONG /*lParam*/)
{
	// If initialization already completed
	if (m_nStatus == 0) {
		// Lock VM and refresh timer resolution settings
		::LockVM();
		timeEndPeriod(1);
		timeBeginPeriod(1);
		::UnlockVM();
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	System commands
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSysCommand(UINT nID, LPARAM lParam)
{
	// Handle "standard window position" command
	if ((nID & 0xfff0) == IDM_STDWIN) {
		InitPos(TRUE);
		return;
	}

	// Redirect maximize to fullscreen toggle
	if ((nID & 0xfff0) == SC_MAXIMIZE) {
		if (!m_bFullScreen) {
			PostMessage(WM_COMMAND, IDM_FULLSCREEN);
		}
		return;
	}

	// Base class handling
	CFrameWnd::OnSysCommand(nID, lParam);
}

//---------------------------------------------------------------------------
//
//	Data transfer
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
afx_msg BOOL CFrmWnd::OnCopyData(CWnd* /*pWnd*/, COPYDATASTRUCT* pCopyDataStruct)
#else
LONG CFrmWnd::OnCopyData(UINT /*uParam*/, LONG pCopyDataStruct)
#endif
{
	PCOPYDATASTRUCT pCDS;

	// Get received COPYDATA parameters
	pCDS = (PCOPYDATASTRUCT)pCopyDataStruct;

	// Process forwarded command line
	InitCmd((LPSTR)pCDS->lpData);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Shell notifications
//
//---------------------------------------------------------------------------
LRESULT CFrmWnd::OnShellNotify(UINT uParam, LONG lParam)
{
	HANDLE hMemoryMap;
	DWORD dwProcessId;
	LPITEMIDLIST *pidls;
	HANDLE hLock;
	LONG nEvent;
	TCHAR szPath[_MAX_PATH];
	CHost *pHost;

	// Branch by Windows platform family
	if (::IsWinNT()) {
		// Windows 2000/XP: lock shell notification payload
		hMemoryMap = (HANDLE)uParam;
		dwProcessId = (DWORD)lParam;
		hLock = ::SHChangeNotification_Lock(hMemoryMap, dwProcessId, &pidls, &nEvent);
		if (hLock == NULL) {
			return 0;
		}
	}
	else {
		// Windows 9x: PIDLs/event arrive directly in uParam/lParam
		pidls = (LPITEMIDLIST*)uParam;
		nEvent = lParam;
		hLock = NULL;
	}

	// While running, forward notifications to CHost when available
	if (m_nStatus == 0) {
		pHost = GetHost();

#if 1
		// If WinDRV is not effectively enabled, suppress host notification (v2.04)
		{
			Config config;
			GetConfig()->GetConfig(&config);
			if ((config.windrv_enable <= 0) || (config.windrv_enable > 3)) {
				pHost = NULL;
			}
		}
#endif

		if (pHost) {
			// Resolve path from PIDL
			::SHGetPathFromIDList(pidls[0], szPath);

			// Notify host component
			pHost->ShellNotify(nEvent, szPath);
		}
	}

	// On NT-class systems, unlock shell notification payload
	if (::IsWinNT()) {
		ASSERT(hLock);
		::SHChangeNotification_Unlock(hLock);
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	Execution counter update
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::UpdateExec()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// While scheduler runs, increment execution counter (cleared on save)
	if (GetScheduler()->IsEnable()) {
		m_dwExec++;
		if (m_dwExec == 0) {
			m_dwExec--;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Provide message string
//
//---------------------------------------------------------------------------
void CFrmWnd::GetMessageString(UINT nID, CString& rMessage) const
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];
	TCHAR szName[60];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	int nMRU;
	int nDisk;
	BOOL bValid;
	CInfo *pInfo;

	// Start as unresolved
	bValid = FALSE;

	// Resolve menu strings first (including English resources + MRU)
	if ((nID >= IDM_OPEN) && (nID <= IDM_ABOUT)) {
		// English environment?
		if (!::IsJapanese()) {
			// Resolve through safe message loader (handles +5000 fallback safely).
			::GetMsg(nID, rMessage);
			if (!rMessage.IsEmpty()) {
				bValid = TRUE;
			}
		}
	}

	// Special-case menu string (IDM_STDWIN)
	if (nID == IDM_STDWIN) {
		// English environment?
		if (!::IsJapanese()) {
			// Resolve through safe message loader (handles +5000 fallback safely).
			::GetMsg(nID, rMessage);
			if (!rMessage.IsEmpty()) {
				bValid = TRUE;
			}
		}
	}

	// YMFM runtime toggle
	if (nID == IDM_YMFM) {
		rMessage = _T("YMFM runtime audio backend");
		bValid = TRUE;
	}

	// MRU0
	if ((nID >= IDM_D0_MRU0) && (nID <= IDM_D0_MRU8)) {
		nMRU = nID - IDM_D0_MRU0;
		ASSERT((nMRU >= 0) && (nMRU <= 8));
		GetConfig()->GetMRUFile(0, nMRU, szPath);
		szPath[60] = _T('\0');
		rMessage = szPath;
		bValid = TRUE;
	}

	// MRU1
	if ((nID >= IDM_D1_MRU0) && (nID <= IDM_D1_MRU8)) {
		nMRU = nID - IDM_D1_MRU0;
		ASSERT((nMRU >= 0) && (nMRU <= 8));
		GetConfig()->GetMRUFile(1, nMRU, szPath);
		szPath[60] = _T('\0');
		rMessage = szPath;
		bValid = TRUE;
	}

	// MRU2
	if ((nID >= IDM_MO_MRU0) && (nID <= IDM_MO_MRU8)) {
		nMRU = nID - IDM_MO_MRU0;
		ASSERT((nMRU >= 0) && (nMRU <= 8));
		GetConfig()->GetMRUFile(2, nMRU, szPath);
		szPath[60] = _T('\0');
		rMessage = szPath;
		bValid = TRUE;
	}

	// MRU3
	if ((nID >= IDM_CD_MRU0) && (nID <= IDM_CD_MRU8)) {
		nMRU = nID - IDM_CD_MRU0;
		ASSERT((nMRU >= 0) && (nMRU <= 8));
		GetConfig()->GetMRUFile(3, nMRU, szPath);
		szPath[60] = _T('\0');
		rMessage = szPath;
		bValid = TRUE;
	}

	// MRU4
	if ((nID >= IDM_XM6_MRU0) && (nID <= IDM_XM6_MRU8)) {
		nMRU = nID - IDM_XM6_MRU0;
		ASSERT((nMRU >= 0) && (nMRU <= 8));
		GetConfig()->GetMRUFile(4, nMRU, szPath);
		szPath[60] = _T('\0');
		rMessage = szPath;
		bValid = TRUE;
	}

	// Disk label entry for drive 0
	if ((nID >= IDM_D0_MEDIA0) && (nID <= IDM_D0_MEDIAF)) {
		nDisk = nID - IDM_D0_MEDIA0;
		ASSERT((nDisk >= 0) && (nDisk <= 15));
		::LockVM();
		m_pFDD->GetName(0, szName, nDisk);
		m_pFDD->GetPath(0, path);
		::UnlockVM();
		_tsplitpath(path.GetPath(), szDrive, szDir, szFile, szExt);
		rMessage = szName;
		rMessage += _T(" (");
		rMessage += szFile;
		rMessage += szExt;
		rMessage += _T(")");
		bValid = TRUE;
	}

	// Disk label entry for drive 1
	if ((nID >= IDM_D1_MEDIA0) && (nID <= IDM_D1_MEDIAF)) {
		nDisk = nID - IDM_D1_MEDIA0;
		ASSERT((nDisk >= 0) && (nDisk <= 15));
		::LockVM();
		m_pFDD->GetName(1, szName, nDisk);
		m_pFDD->GetPath(1, path);
		::UnlockVM();
		_tsplitpath(path.GetPath(), szDrive, szDir, szFile, szExt);
		rMessage = szName;
		rMessage += _T(" (");
		rMessage += szFile;
		rMessage += szExt;
		rMessage += _T(")");
		bValid = TRUE;
	}

	// If nothing matched, defer to base class
	if (!bValid) {
		CFrameWnd::GetMessageString(nID, rMessage);
	}

	// Pass message to Info (internal retention)
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->SetMessageString(rMessage);
	}

	// Pass message to status view
	if (m_pStatusView) {
		m_pStatusView->SetMenuString(rMessage);
	}
}

//---------------------------------------------------------------------------
//
//	Hide/show taskbar
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::HideTaskBar(BOOL bHide, BOOL bFore)
{
	if (bHide) {
		// Enter always-on-top presentation
		m_hTaskBar = ::FindWindow(_T("Shell_TrayWnd"), NULL);
		if (m_hTaskBar) {
			::ShowWindow(m_hTaskBar, SW_HIDE);
		}
		ModifyStyleEx(0, WS_EX_TOPMOST, 0);
	}
	else {
		// Return to normal z-order
		ModifyStyleEx(WS_EX_TOPMOST, 0, 0);
		if (m_hTaskBar) {
			::ShowWindow(m_hTaskBar, SW_SHOWNA);
		}
	}

	// Optionally force foreground activation
	if (bFore) {
		SetForegroundWindow();
	}
}

//---------------------------------------------------------------------------
//
//	Status bar visibility
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ShowStatus()
{
	ASSERT(this);

	// Lock VM when required
	if (m_nStatus == 0) {
		::LockVM();
	}

	// Fullscreen path
	if (m_bFullScreen) {
		// Always hide the standard status bar in fullscreen
		ShowControlBar(&m_StatusBar, FALSE, FALSE);

		// If status display is enabled
		if (m_bStatusBar) {
			// Create fullscreen status view if missing
			if (!m_pStatusView) {
				// Create it
				CreateStatusView();

				// Recalculate placement
				if (m_bStatusBar) {
					RecalcStatusView();
				}
			}
		}
		else {
			// Destroy fullscreen status view when disabled
			if (m_pStatusView) {
				// Destroy it
				DestroyStatusView();

				// Recalculate placement
				RecalcStatusView();
			}
		}

		// Unlock VM when needed
		if (m_nStatus == 0) {
			::UnlockVM();
		}
		return;
	}

	// Status view is fullscreen-only; destroy it in windowed mode
	if (m_pStatusView) {
		DestroyStatusView();
		RecalcLayout();
	}

	// In windowed mode, control visibility through ShowControlBar
	ShowControlBar(&m_StatusBar, m_bStatusBar, FALSE);

	// Unlock VM when needed
	if (m_nStatus == 0) {
		::UnlockVM();
	}
}

//---------------------------------------------------------------------------
//
//	Create status view (fullscreen)
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::CreateStatusView()
{
	CInfo *pInfo;

	ASSERT(!m_pStatusView);

	if (m_bStatusBar) {
		// Create status view (layout update handled elsewhere)
		m_pStatusView = new CStatusView;
		if (m_pStatusView->Init(this)) {
			// Creation succeeded
			pInfo = GetInfo();
			if (pInfo) {
				// Inform Info that status view is now available
				pInfo->SetStatusView(m_pStatusView);
			}
		}
		else {
			// Creation failed
			m_bStatusBar = FALSE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Destroy status view (fullscreen)
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::DestroyStatusView()
{
	CInfo *pInfo;

	// Only when a valid status view exists
	if (m_pStatusView) {
		// Get Info component
		pInfo = GetInfo();
		if (pInfo) {
			// Inform Info that status view is being removed
			pInfo->SetStatusView(NULL);
		}

		// Destroy status view (layout update handled elsewhere)
		m_pStatusView->DestroyWindow();
		m_pStatusView = NULL;
	}
}

//---------------------------------------------------------------------------
// Recalculates and adjusts positions/sizes for two key child views:
// Draw view (`m_pDrawView`), where the emulated screen is rendered.
// Status view (`m_pStatusView`), the custom fullscreen status bar.
//---------------------------------------------------------------------------
void CFrmWnd::RecalcStatusView()
{
	CRect rectClient;
	GetClientRect(&rectClient);// Current client area

	const int clientWidth = rectClient.Width();
	const int clientHeight = rectClient.Height();

	// Common flags for SetWindowPos
	const UINT swpFlags = SWP_NOZORDER | SWP_NOACTIVATE;

	if (m_pStatusView && m_pStatusView->GetSafeHwnd())
	{
		CRect rectStatus;
		m_pStatusView->GetWindowRect(&rectStatus);
		const int statusHeight = rectStatus.Height();

		// Compute draw-view height
		const int drawHeight = clientHeight - statusHeight;

		// Resize only when dimensions changed
		if (m_pDrawView->GetSafeHwnd())
		{
			CRect currentDrawRect;
			m_pDrawView->GetWindowRect(&currentDrawRect);

			if (currentDrawRect.Height() != drawHeight ||
				currentDrawRect.Width() != clientWidth)
			{
				m_pDrawView->SetWindowPos(
					nullptr,
					0, 0,
					clientWidth, drawHeight,
					swpFlags
				);
			}
		}

		// Move/resize status view only when needed
		if (m_pStatusView->GetSafeHwnd())
		{
			CRect currentStatusRect;
			m_pStatusView->GetWindowRect(&currentStatusRect);

			if (currentStatusRect.top != drawHeight ||
				currentStatusRect.Height() != statusHeight ||
				currentStatusRect.Width() != clientWidth)
			{
				m_pStatusView->SetWindowPos(
					nullptr,
					0, drawHeight,
					clientWidth, statusHeight,
					swpFlags
				);
			}
		}
	}
	else
	{
		// Normal mode without custom status view
		if (m_pDrawView->GetSafeHwnd())
		{
			m_pDrawView->SetWindowPos(
				nullptr,
				0, 0,
				clientWidth, clientHeight,
				swpFlags
			);
		}
	}

	// Force redraw only when client rect is valid
	if (!rectClient.IsRectEmpty())
	{
		m_pDrawView->InvalidateRect(nullptr, FALSE);
		if (m_pStatusView) {
			m_pStatusView->InvalidateRect(nullptr, FALSE);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Reset status bar contents
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ResetStatus()
{
	CInfo *pInfo;

	// Reset via Info component when available
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->ResetStatus();
	}
}

//---------------------------------------------------------------------------
//
//	Owner-draw handling
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDrawItem(int nID, LPDRAWITEMSTRUCT lpDIS)
{
	int nPane;
	HDC hDC;
	CRect rectDraw;
	CInfo *pInfo;

	// Ensure this draw request is for the status bar
	if (lpDIS->hwndItem != m_StatusBar.m_hWnd) {
		CFrameWnd::OnDrawItem(nID, lpDIS);
		return;
	}

	// Get pane index, DC, and draw rectangle
	nPane = lpDIS->itemID;
	if (nPane == 0) {
		return;
	}
	nPane--;
	hDC = lpDIS->hDC;
	rectDraw = &lpDIS->rcItem;

	// Check Info component
	pInfo = GetInfo();
	if (!pInfo) {
		// Fill black as fallback
		::SetBkColor(hDC, RGB(0, 0, 0));
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rectDraw, NULL, 0, NULL);
		return;
	}

	// Delegate drawing to Info
	pInfo->DrawStatus(nPane, hDC, rectDraw);
}

//---------------------------------------------------------------------------
//
//	Menu bar visibility
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ShowMenu()
{
	HMENU hMenu;

	ASSERT(this);

	// Lock VM when required
	if (m_nStatus == 0) {
		::LockVM();
	}

	// Get currently attached menu
	hMenu = ::GetMenu(m_hWnd);

	// Case: menu should be hidden
	if (m_bFullScreen || !m_bMenuBar) {
		// If a menu is attached
		if (hMenu != NULL) {
			// Detach menu
			SetMenu(NULL);
		}
		if (m_nStatus == 0) {
			::UnlockVM();
		}
		return;
	}

	// Case: menu should be visible
	if (hMenu != NULL) {
		// If desired menu is already set
		if (m_Menu.GetSafeHmenu() == hMenu) {
			// No change needed
			if (m_nStatus == 0) {
				::UnlockVM();
			}
			return;
		}
	}

	// Attach standard menu
	SetMenu(&m_Menu);

	// Unlock VM when required
	if (m_nStatus == 0) {
		::UnlockVM();
	}
}

//---------------------------------------------------------------------------
//
//	Caption visibility
//
//---------------------------------------------------------------------------
void CFrmWnd::ShowCaption()
{
	const DWORD dwCaptionStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	const BOOL bShouldShowCaption = !m_bFullScreen && m_bCaption;

	// 1) Skip if current style already matches target
	DWORD dwCurrentStyle = GetStyle();
	if (bShouldShowCaption == ((dwCurrentStyle & dwCaptionStyle) == dwCaptionStyle)) {
		return;// No style update needed
	}

	// 2) Lock VM only when needed
	const BOOL bVMLockNeeded = (m_nStatus == 0);
	if (bVMLockNeeded) {
		::LockVM();
	}

	// 3) Update window styles
	ModifyStyle(
		bShouldShowCaption?0:dwCaptionStyle,// Styles to remove
		bShouldShowCaption?dwCaptionStyle:0,// Styles to add
		SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED
	);

	// 4) Redraw menu bar when needed
	if (bShouldShowCaption && m_bMenuBar) {
		DrawMenuBar();// Redraw menu bar if visible
	}

	// 5) Unlock VM if it was locked
	if (bVMLockNeeded) {
		::UnlockVM();
	}
}

//---------------------------------------------------------------------------
//
//	Reset caption text
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ResetCaption()
{
	CInfo *pInfo;

	// Reset through Info when available
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->ResetCaption();
	}
}

//---------------------------------------------------------------------------
//
//	Set info text
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::SetInfo(CString& strInfo)
{
	CInfo *pInfo;

	// Update through Info when available
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->SetInfo(strInfo);
	}
}

//---------------------------------------------------------------------------
//
//	Get draw view
//
//---------------------------------------------------------------------------
CDrawView* FASTCALL CFrmWnd::GetView() const
{
	ASSERT(this);
	ASSERT(m_pDrawView);
	ASSERT(m_pDrawView->m_hWnd);
	return m_pDrawView;
}

//---------------------------------------------------------------------------
//
//	Get first component
//
//---------------------------------------------------------------------------
CComponent* FASTCALL CFrmWnd::GetFirstComponent() const
{
	ASSERT(this);
	return m_pFirstComponent;
}

//---------------------------------------------------------------------------
//
//	Get scheduler
//
//---------------------------------------------------------------------------
CScheduler* FASTCALL CFrmWnd::GetScheduler() const
{
	ASSERT(this);
	ASSERT(m_pSch);
	return m_pSch;
}

//---------------------------------------------------------------------------
//
//	Get sound component
//
//---------------------------------------------------------------------------
CSound* FASTCALL CFrmWnd::GetSound() const
{
	ASSERT(this);
	ASSERT(m_pSound);
	return m_pSound;
}

//---------------------------------------------------------------------------
//
//	Get input component
//
//---------------------------------------------------------------------------
CInput* FASTCALL CFrmWnd::GetInput() const
{
	ASSERT(this);
	ASSERT(m_pInput);
	return m_pInput;
}

//---------------------------------------------------------------------------
//
//	Get port component
//
//---------------------------------------------------------------------------
CPort* FASTCALL CFrmWnd::GetPort() const
{
	ASSERT(this);
	ASSERT(m_pPort);
	return m_pPort;
}

//---------------------------------------------------------------------------
//
//	Get MIDI component
//
//---------------------------------------------------------------------------
CMIDI* FASTCALL CFrmWnd::GetMIDI() const
{
	ASSERT(this);
	ASSERT(m_pMIDI);
	return m_pMIDI;
}

//---------------------------------------------------------------------------
//
//	Get TrueKey component
//
//---------------------------------------------------------------------------
CTKey* FASTCALL CFrmWnd::GetTKey() const
{
	ASSERT(this);
	ASSERT(m_pTKey);
	return m_pTKey;
}

//---------------------------------------------------------------------------
//
//	Get host component
//
//---------------------------------------------------------------------------
CHost* FASTCALL CFrmWnd::GetHost() const
{
	ASSERT(this);
	ASSERT(m_pHost);
	return m_pHost;
}

//---------------------------------------------------------------------------
//
//	Get info component
//
//---------------------------------------------------------------------------
CInfo* FASTCALL CFrmWnd::GetInfo() const
{
	ASSERT(this);

	// Return NULL when Info does not exist
	if (!m_pInfo) {
		return NULL;
	}

	// Return NULL when Info is disabled
	if (!m_pInfo->IsEnable()) {
		return NULL;
	}

	// Info is active; return pointer
	return m_pInfo;
}

//---------------------------------------------------------------------------
//
//	Get config component
//
//---------------------------------------------------------------------------
CConfig* FASTCALL CFrmWnd::GetConfig() const
{
	ASSERT(this);
	ASSERT(m_pConfig);
	return m_pConfig;
}

//---------------------------------------------------------------------------
//
//	Toggle Renderer (DX9/GDI)
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRenderFast()
{
	Render *pRender;
	int nMode;
	CString info;
	Config config;

	pRender = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	if (!pRender) {
		return;
	}

	nMode = (pRender->GetCompositorMode() == Render::compositor_fast) ?
		Render::compositor_original : Render::compositor_fast;
	if (!pRender->SetCompositorMode(nMode)) {
		return;
	}

	pRender->Complete();
	if (m_pDrawView) {
		m_pDrawView->Refresh();
	}

	GetConfig()->GetConfig(&config);
	config.alt_raster = (nMode == Render::compositor_fast) ? TRUE : FALSE;
	GetConfig()->SetConfig(&config);

	::LockVM();
	::GetVM()->ApplyCfg(&config);
	::UnlockVM();

	info.Format(_T("Render Fast: %s"),
		(nMode == Render::compositor_fast) ? _T("ON") : _T("OFF"));
	SetInfo(info);
}

void CFrmWnd::OnRenderFastUI(CCmdUI *pCmdUI)
{
	Render *pRender;

	if (!pCmdUI) {
		return;
	}

	pRender = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	if (!pRender) {
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck(0);
		return;
	}

	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck((pRender->GetCompositorMode() == Render::compositor_fast) ? 1 : 0);
}

void CFrmWnd::OnYmfm()
{
	CSound *pSound;

	pSound = GetSound();
	if (!pSound) {
		return;
	}

	pSound->SetYmfm(!pSound->IsYmfm());

	::LockVM();
	ApplyCfg();
	::UnlockVM();

	CString info;
	info.Format(_T("YMFM: %s"), pSound->IsYmfm() ? _T("ON") : _T("OFF"));
	SetInfo(info);
}

void CFrmWnd::OnYmfmUI(CCmdUI *pCmdUI)
{
	CSound *pSound;

	if (!pCmdUI) {
		return;
	}

	pSound = GetSound();
	if (!pSound) {
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck(0);
		return;
	}

	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck(pSound->IsYmfm() ? 1 : 0);
}

void CFrmWnd::OnToggleRenderer()
{
	if (m_pDrawView) {
		m_pDrawView->ToggleRenderer();
	}
}

//---------------------------------------------------------------------------
//
//	Toggle VSync
//
//---------------------------------------------------------------------------
void CFrmWnd::OnToggleVSync()
{
	m_bVSyncEnabled = !m_bVSyncEnabled;

	if (m_pDrawView) {
		m_pDrawView->SetVSync(m_bVSyncEnabled);
		m_pDrawView->ShowRenderStatusOSD(m_bVSyncEnabled);
	}

	CString info;
	info.Format(_T("VSync: %s"), m_bVSyncEnabled ? _T("ON") : _T("OFF"));
	SetInfo(info);
}

//---------------------------------------------------------------------------
//
//	Enter borderless fullscreen mode
//
//---------------------------------------------------------------------------
void CFrmWnd::EnterBorderlessFullscreen()
{
	if (m_bBorderless) return;

	// Save current window state
	m_dwPrevStyle = GetWindowLong(m_hWnd, GWL_STYLE);
	m_dwPrevExStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
	GetWindowPlacement(&m_wpPrev);

	// Get current monitor
	HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO mi = { sizeof(mi) };
	if (GetMonitorInfo(hMonitor, &mi)) {
		// Remove border-related styles
		SetWindowLong(m_hWnd, GWL_STYLE, m_dwPrevStyle & ~(WS_CAPTION | WS_THICKFRAME));
		SetWindowLong(m_hWnd, GWL_EXSTYLE, m_dwPrevExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

		// Expand to full monitor bounds
		SetWindowPos(&wndTop,
			mi.rcMonitor.left, mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

		m_bBorderless = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Exit borderless fullscreen mode
//
//---------------------------------------------------------------------------
void CFrmWnd::ExitBorderlessFullscreen()
{
	if (!m_bBorderless) return;

	// Restore saved styles and placement
	SetWindowLong(m_hWnd, GWL_STYLE, m_dwPrevStyle);
	SetWindowLong(m_hWnd, GWL_EXSTYLE, m_dwPrevExStyle);

	SetWindowPlacement(&m_wpPrev);
	SetWindowPos(NULL, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

	m_bBorderless = FALSE;
}

#endif	// _WIN32

void CFrmWnd::OnToggleOSD()
{
	if (m_pDrawView) {
		m_pDrawView->m_bShowOSD = !m_pDrawView->m_bShowOSD;
		m_pDrawView->Invalidate();
	}
}

void CFrmWnd::OnToggleShader()
{
	if (m_pDrawView && m_pDrawView->IsDX9Active()) {
		m_pDrawView->ToggleShader();

		Config config;
		GetConfig()->GetConfig(&config);
		config.render_shader = m_pDrawView->IsShaderEnabled();
		GetConfig()->SetConfig(&config);
	}
}

void CFrmWnd::OnToggleShaderUI(CCmdUI *pCmdUI)
{
	if (pCmdUI) {
		if (m_pDrawView && m_pDrawView->IsDX9Active()) {
			pCmdUI->SetCheck(m_pDrawView->IsShaderEnabled() ? 1 : 0);
			pCmdUI->Enable(TRUE);
		}
		else {
			pCmdUI->Enable(FALSE);
		}
	}
}
