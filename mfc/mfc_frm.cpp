//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ‚o‚hD(ytanaka@ipc-tokai.or.jp)
//	[ MFC Ventana del marco ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

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

//===========================================================================
//
//	Ventana del marco

//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Definiciones de las constantes de la cascara
//	Se requiere que sea definido por la aplicacion, no por un archivo de inclusion.
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
	// VM y codigos de estado
	::pVM = NULL;
	m_nStatus = -1;

	// Dispositivos
	m_pFDD = NULL;
	m_pSASI = NULL;
	m_pSCSI = NULL;
	m_pScheduler = NULL;
	m_pKeyboard = NULL;
	m_pMouse = NULL;

	// Componentes
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

	// Pantalla completa
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

	// Subventana
	m_strWndClsName.Empty();

	// Barra de estado, menu y subtitulos
	m_bStatusBar = FALSE;
	m_bMenuBar = TRUE;
	m_bCaption = TRUE;

	// Notificaciones de la cascara
	m_uNotifyId = NULL;

	// Configuracion
	m_bMouseMid = TRUE;
	m_bPopup = FALSE;
	m_bAutoMouse = TRUE;

	// Otras variables
	m_bExit = FALSE;
	m_bSaved = FALSE;
	m_nFDDStatus[0] = 0;
	m_nFDDStatus[1] = 0;
	m_dwExec = 0;	
}

//---------------------------------------------------------------------------
//
//	Mapa de mensajes
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
	ON_COMMAND(IDM_REFRESH, OnRefresh)
	ON_COMMAND(IDM_STRETCH, OnStretch)
	ON_UPDATE_COMMAND_UI(IDM_STRETCH, OnStretchUI)
	ON_COMMAND(IDM_FULLSCREEN, OnFullScreen)
	ON_UPDATE_COMMAND_UI(IDM_FULLSCREEN, OnFullScreenUI)
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
//	Inicializacion
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::Init()
{
	// Creacion de ventanas

	if (!Create(NULL, _T("XM6"),
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
			WS_BORDER | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
			rectDefault, NULL, NULL, 0, NULL)) {
		return FALSE;
	}

	// El resto de la inicializacion se deja en manos de OnCreate.
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Preparando la creacion de una ventana
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	// Clase basica
	if (!CFrameWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// Eliminacion del borde del cliente
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Creacion de ventanas
//
//---------------------------------------------------------------------------
int CFrmWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	LONG lUser;
	CMenu *pSysMenu;
	UINT nCount;
	CString string;

	// Clase basica
	if (CFrameWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Especificacion de los datos del usuario
	lUser = (LONG)MAKEID('X', 'M', '6', ' ');
	::SetWindowLong(m_hWnd, GWL_USERDATA, lUser);

	// Acelerador, Icon, IMM
	LoadAccelTable(MAKEINTRESOURCE(IDR_ACCELERATOR));
	SetIcon(AfxGetApp()->LoadIcon(IDI_APPICON), TRUE);
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Menu (ventana)
	if (::IsJapanese()) {
		// “ú–{Œêƒƒjƒ…[
		m_Menu.LoadMenu(IDR_MENU);
		m_PopupMenu.LoadMenu(IDR_MENUPOPUP);
	}
	else {
		// Menu en ingles
		m_Menu.LoadMenu(IDR_US_MENU);
		m_PopupMenu.LoadMenu(IDR_US_MENUPOPUP);
	}
	SetMenu(&m_Menu);
	m_bMenuBar = TRUE;
	m_bPopupMenu = FALSE;

	// Menu (sistema)
	::GetMsg(IDS_STDWIN, string);
	pSysMenu = GetSystemMenu(FALSE);
	ASSERT(pSysMenu);
	nCount = pSysMenu->GetMenuItemCount();

	// Insertar "Posicion estandar de la ventana
	pSysMenu->InsertMenu(nCount - 2, MF_BYPOSITION | MF_STRING, IDM_STDWIN, string);
	pSysMenu->InsertMenu(nCount - 2, MF_BYPOSITION | MF_SEPARATOR);

	// Inicializacion de la ventana infantil
	if (!InitChild()) {
		return -1;
	}

	// Posicion de la ventana e inicializacion del rectangulo
	InitPos();

	// Inicializacion de las notificaciones de Shell
	InitShell();

	// Inicializacion de la maquina virtual
	if (!InitVM()) {
		// Error de inicializacion de la maquina virtual
		m_nStatus = 1;
		PostMessage(WM_KICK, 0, 0);
		return 0;
	}

	// Transmision de versiones desde los recursos de versiones a las maquinas virtuales
	InitVer();

	// Almacenamiento de dispositivos
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

	// Creacion e inicializacion de componentes
	if (!InitComponent()) {
		// Error de inicializacion del componente
		m_nStatus = 2;
		PostMessage(WM_KICK, 0, 0);
		return 0;
	}

	// Aplicar la configuracion (similar a OnOption, con la VM bloqueada)
	::LockVM();
	ApplyCfg();
	::UnlockVM();

	// Restablecer
	::GetVM()->Reset();

	// !Reanudar la posicion de la ventana (m_nStatus ! = 0 se anota)
	ASSERT(m_nStatus != 0);
	RestoreFrameWnd(FALSE);

	// Publica tu mensaje y ya esta.
	m_nStatus = 0;
	PostMessage(WM_KICK, 0, 0);
	return 0;
}

//---------------------------------------------------------------------------
//
//	Inicializacion de la ventana infantil
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

	// Ver creacion
	m_pDrawView = new CDrawView;
	if (!m_pDrawView->Init(this)) {
		return FALSE;
	}

	// Inicializacion del trabajo de la barra de estado
	ResetStatus();

	// Creacion de la barra de estado
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

	// Obtener la metrica del texto
	hDC = ::GetDC(m_hWnd);
	hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	hDefFont = (HFONT)::SelectObject(hDC, hFont);
	ASSERT(hDefFont);
	::GetTextMetrics(hDC, &tm);
	::SelectObject(hDC, hDefFont);
	::ReleaseDC(m_hWnd, hDC);

	// Bucle de ajuste de tamano
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

	// Redistribucion
	RecalcLayout();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Inicializacion de la posicion y del rectangulo
//	Si bStart=FALSE, restaura la posicion cuando bFullScreen=FALSE.
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitPos(BOOL bStart)
{
	int cx;
	int cy;
	CRect rect;
	CRect rectStatus;
	CRect rectWnd;

	ASSERT(this);

	// Obtener el tamano de la pantalla y el rectangulo de la ventana
	cx = ::GetSystemMetrics(SM_CXSCREEN);
	cy = ::GetSystemMetrics(SM_CYSCREEN);
	GetWindowRect(&rectWnd);

	// 800x600 e inferiores se extienden a tamano de pantalla completa
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


	/* ACA SE ESTABLECE TAMAÑO DE VENTANA PRINCIPAL Y DE PANTALLA COMPLETA */
	// 824x560 (DDP2) se reconoce como el tamano maximo no entrelazado
	rect.left = 0;
	rect.top = 0;

	/* Si es pantalla completa  utilizaremos resolucion total */
	if (m_bFullScreen) 
	{
		rect.right = cx;
		rect.bottom = cy;
	}
	else /* Si es ventana en 1024 es suficiente */
	{
		rect.right = 1024;
		rect.bottom = 768;
	}
	::AdjustWindowRectEx(&rect, GetView()->GetStyle(), FALSE, GetView()->GetExStyle());
	m_StatusBar.GetWindowRect(&rectStatus);
	rect.bottom += rectStatus.Height();
	::AdjustWindowRectEx(&rect, GetStyle(), TRUE, GetExStyle());

	// Parece que rect.left y rect.bottom son negativos (despues de esto, right y bottom indican cx y cy)
	rect.right -= rect.left;
	rect.left = 0;
	rect.bottom -= rect.top;
	rect.top = 0;

	// Centrado, si hay espacio
	if (rect.right < cx) {
		rect.left = (cx - rect.right) / 2;
	}
	if (rect.bottom < cy) {
		rect.top = (cy - rect.bottom) / 2;
	}

	// Dividir por bStart (inicio inicial o cambio de ventana a pantalla completa)
	if (bStart) {
		// Guardar la posicion de la ventana una vez (despues de esto hay otra oportunidad de RestoreFrameWnd)
		m_nWndLeft = rect.left;
		m_nWndTop = rect.top;
	}
	else {
		// Posicion correcta solo en modo ventana
		if (!m_bFullScreen) {
			if ((rect.left == 0) && (rect.top == 0)) {
				// Si recibe un mensaje WM_DISPLAYCHANGE y la ventana se reduce
				m_nWndLeft = rect.left;
				m_nWndTop = rect.top;
			}
			else {
				// Aparte de eso (incluyendo la transicion de estado de pantalla completa a ventana)
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
//	Inicializacion de la cooperacion de Shell
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitShell()
{
	int nSources;

	// Establecer factores de notificacion
	if (::IsWinNT()) {
		// Windows2000/XP: anadir la bandera para utilizar la memoria compartida
		nSources = SHCNRF_InterruptLevel | SHCNRF_ShellLevel | SHCNRF_NewDelivery;
	}
	else {
		// Windows9x: la memoria compartida no se utiliza
		nSources = SHCNRF_InterruptLevel | SHCNRF_ShellLevel;
	}

	// Inicializacion de una entrada
	m_fsne[0].pidl = NULL;
	m_fsne[0].fRecursive = FALSE;

	// ƒVƒFƒ‹’Ê’mƒƒbƒZ[ƒW‚ð“o˜^
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
//	Inicializacion de la maquina virtual
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::InitVM()
{
	::pVM = new VM;
	if (!::GetVM()->Init()) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Inicializacion de los componentes
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

	// Construcciones (hay que tener en cuenta el orden. Primera configuracion, ultimo programador)
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

	// Inicializacion
	pComponent = m_pFirstComponent;
	bSuccess = TRUE;

	// Bucle
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
//	Inicializacion de la version
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

	// Obtenga la ruta
	::GetModuleFileName(NULL, szPath, _MAX_PATH);

	// Leer la informacion de la version
	dwLength = GetFileVersionInfoSize(szPath, &dwHandle);
	if (dwLength == 0) {
		return;
	}

	pVerInfo = new BYTE[ dwLength ];
	if (::GetFileVersionInfo(szPath, dwHandle, dwLength, pVerInfo) == 0) {
		return;
	}

	// Recuperacion de la informacion de la version
	if (::VerQueryValue(pVerInfo, _T("\\"), (LPVOID*)&pFileInfo, &uLength) == 0) {
		delete[] pVerInfo;
		return;
	}

	// Separacion de versiones y notificacion a las maquinas virtuales
	dwMajor = (DWORD)HIWORD(pFileInfo->dwProductVersionMS);
	dwMinor = (DWORD)(LOWORD(pFileInfo->dwProductVersionMS) * 16
					+ HIWORD(pFileInfo->dwProductVersionLS));
	::GetVM()->SetVersion(dwMajor, dwMinor);

	// I—¹
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
      e->ReportError(); // see what's going wrong
   }
   END_CATCH_ALL
}



//Este codigo construye una cadena que contiene las rutas completas de los archivos
CString CFrmWnd::ProcesarM3u(CString str)
{
	CString nuevaRuta = RutaCompletaArchivoXM6;
	PathRemoveFileSpecA(nuevaRuta.GetBuffer());
	nuevaRuta.ReleaseBuffer();

	int curPos = 0;
	CString resToken = str.Tokenize(_T("\r\n"), curPos);
	CString cadTot;

	while (!resToken.IsEmpty())
	{
		cadTot += "\"" + nuevaRuta + "\\" + resToken + "\"  ";
		resToken = str.Tokenize(_T("\r\n"), curPos);
	}

	return cadTot;
}



//---------------------------------------------------------------------------
//
//	Procesamiento de la linea de comandos
//	Comun a la linea de comandos y a WM_DATA
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitCmd(LPCTSTR lpszCmd)
{
	ASSERT(this);
	ASSERT(lpszCmd);

	CString sz;
	sz.Format(_T("%s"), lpszCmd);
	CString fileName = sz.Mid(sz.ReverseFind('\\') + 1);

	RutaCompletaArchivoXM6 = lpszCmd;
	NombreArchivoXM6 = fileName;

	/* AQUÍ SE INICIALIZA CUALQUIER PARÁMETRO DE LA LÍNEA DE COMANDO */

	// Proceso de obtener la extensión del archivo
	CString str = RutaCompletaArchivoXM6;
	CString extensionArchivo = "";

	int curPos = 0;
	CString resToken = str.Tokenize(_T("."), curPos); // Obtiene la extensión de la ruta completa del archivo
	while (!resToken.IsEmpty())
	{
		extensionArchivo = resToken;
		resToken = str.Tokenize(_T("."), curPos);
	}

	/* Si es un archivo M3U, lo analiza y carga */
	if (extensionArchivo.MakeUpper() == "M3U")
	{
		CString contenidoM3u, cont2;
		ReadFile(lpszCmd, contenidoM3u);
		cont2 = ProcesarM3u(contenidoM3u).Trim();
		strcpy((char*)lpszCmd, cont2);
	}

	// lpszCmd es el comando completo con comillas incluidas
	// Inicialización de punteros y banderas
	BOOL bReset = FALSE;

	// Crear un objeto CString a partir de lpszCmd
	CString cmdString(lpszCmd);

	// Dividir la cadena en partes
	curPos = 0;
	CString part = cmdString.Tokenize(_T(" "), curPos);
	int i = 0;
	// Procesar cada parte
	while (!part.IsEmpty())
	{
		// Si la parte comienza y termina con una comilla doble, eliminar las comillas
		if (part[0] == _T('\"') && part[part.GetLength() - 1] == _T('\"'))
		{
			part = part.Mid(1, part.GetLength() - 2);
		}

		// Convertir la parte a LPCTSTR
		LPCTSTR szPath = (LPCTSTR)part;

		// Intentar abrir
		bReset = InitCmdSub(i, szPath);

		// Obtener la siguiente parte
		part = cmdString.Tokenize(_T(" "), curPos);
		i++;
	}

	// En caso de reinicio, solicitarlo
	if (bReset)
	{
		OnReset();
	}
}



//---------------------------------------------------------------------------
//
//	Procesamiento de la linea de comandos Sub
//	Comun para la linea de comandos, WM_DATA y arrastrar y soltar
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

	// Inicializacion de pFDI
	pFDI = NULL;

	// Comprobacion de archivo abierto
	path.SetPath(lpszPath);
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}
	dwSize = fio.GetFileSize();
	fio.Close();

	// Obtener ruta completa
	::GetFullPathName(lpszPath, _MAX_PATH, szPath, &lpszFile);
	path.SetPath(szPath);

	// Bloqueo VM
	::LockVM();

	// 128MO or 230MO or 540MO or 640MO
	if ((dwSize == 0x797f400) || (dwSize == 0xd9eea00) ||
		(dwSize == 0x1fc8b800) || (dwSize == 0x25e28000)) {
		// Intentar asignar una MO
		nDrive = 2;

		if (!m_pSASI->Open(path)) {
			// Fallo de asignacion de MO
			GetScheduler()->Reset();
			ResetCaption();
			::UnlockVM();
			return FALSE;
		}
	}
	else {
		if (dwSize >= 0x200000) {
			// Intento de asignar una VM
			nDrive = 4;

			// Pretratamiento abierto
			if (!OnOpenPrep(path, FALSE)) {
				// Archivos que faltan o versiones incorrectas, etc.
				GetScheduler()->Reset();
				ResetCaption();
				::UnlockVM();
				return FALSE;
			}

			// Ejecucion de la carga (dejelo en manos de OnOpenSub)
			::UnlockVM();
			if (OnOpenSub(path)) {
				Filepath::SetDefaultDir(szPath);
			}
			// No hay reinicio
			return FALSE;
		}
		else {
			// Intento de asignar un FD
			/* ACA SE INICIALIZA IMAGEN DISKETTE DESDE LINEA DE COMANDOS */ 

			if (!m_pFDD->Open(nDrive, path)) {
				// Fallo de asignacion de FD
				GetScheduler()->Reset();
				ResetCaption();
				::UnlockVM();
				return FALSE;
			}
			pFDI = m_pFDD->GetFDI(nDrive);
		}
	}

	// Restablecimiento y desbloqueo de la maquina virtual
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// El exito. Guardar directorio y anadir MRU
	Filepath::SetDefaultDir(szPath);
	GetConfig()->SetMRUFile(nDrive, szPath);

	// Aviso de mala imagen para los disquetes
	if (pFDI) {
		if (pFDI->GetID() == MAKEID('B', 'A', 'D', ' ')) {
			::GetMsg(IDS_BADFDI_WARNING, strMsg);
			//MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		}

		// Se reinicia solo cuando se asigna el disquete
		return TRUE;
	}

	// I—¹
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Guardar componente
//	El programador esta parado, pero CSound y CInput estan funcionando.
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

	// Creacion de informacion sobre la version
	::GetVM()->GetVersion(dwMajor, dwMinor);
	nVer = (int)((dwMajor << 8) | dwMinor);

	// Abrir y buscar archivos
	if (!fio.Open(path, Fileio::Append)) {
		return FALSE;
	}
	if (!fio.Seek(dwPos)) {
		fio.Close();
		return FALSE;
	}

	// Guardar la informacion del componente principal
	dwID = MAKEID('M', 'A', 'I', 'N');
	if (!fio.Write(&dwID, sizeof(dwID))) {
		fio.Close();
		return FALSE;
	}

	// Bucle de componentes
	pComponent = m_pFirstComponent;
	while (pComponent) {
		// ID‚ð•Û‘¶
		dwID = pComponent->GetID();
		if (!fio.Write(&dwID, sizeof(dwID))) {
			fio.Close();
			return FALSE;
		}

		// Componente especifico
		if (!pComponent->Save(&fio, nVer)) {
			fio.Close();
			return FALSE;
		}

		// ŽŸ‚Ö
		pComponent = pComponent->GetNextComponent();
	}

	// Escritura de terminacion
	dwID = MAKEID('E', 'N', 'D', ' ');
	if (!fio.Write(&dwID, sizeof(dwID))) {
		fio.Close();
		return FALSE;
	}

	// Finalizar
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Componentes de carga
//	El programador esta parado, pero CSound y CInput estan funcionando.
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

	// Archivo abierto
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Lectura de cabecera
	if (!fio.Read(cHeader, sizeof(cHeader))) {
		fio.Close();
		return FALSE;
	}

	// Comprobacion de cabeceras, lectura de la informacion de la version
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

	// buscar
	if (!fio.Seek(dwPos)) {
		fio.Close();
		return FALSE;
	}

	// lectura de los componentes principales
	if (!fio.Read(&dwID, sizeof(dwID))) {
		fio.Close();
		return FALSE;
	}
	if (dwID != MAKEID('M', 'A', 'I', 'N')) {
		fio.Close();
		return FALSE;
	}

	// bucle de componentes
	for (;;) {
		// Lectura de identificacion
		if (!fio.Read(&dwID, sizeof(dwID))) {
			fio.Close();
			return FALSE;
		}

		// control de salida
		if (dwID == MAKEID('E', 'N', 'D', ' ')) {
			break;
		}

		// Buscar componente
		pComponent = m_pFirstComponent->SearchComponent(dwID);
		if (!pComponent) {
			// Los componentes estaban presentes en el momento de la grabacion, pero ahora faltan
			fio.Close();
			return FALSE;
		}

		// componente especifico
		if (!pComponent->Load(&fio, nVer)) {
			fio.Close();
			return FALSE;
		}
	}

	//  cerrar
	fio.Close();

	// Aplicar la configuracion (se hace con la VM bloqueada).
	if (GetConfig()->IsApply()) {
		::LockVM();
		ApplyCfg();
		::UnlockVM();
	}

	// redistribucion de la ventana
	GetView()->Invalidate(FALSE);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Aplicar ajustes
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ApplyCfg()
{
	Config config;
	CComponent *pComponent;

	// Adquisicion de la configuracion
	GetConfig()->GetConfig(&config);

	// Aplicar a VM primero
	::GetVM()->ApplyCfg(&config);

	// A continuacion, aplique al componente
	pComponent = m_pFirstComponent;
	while (pComponent) {
		pComponent->ApplyCfg(&config);
		pComponent = pComponent->GetNextComponent();
	}

	// A continuacion, aplique para ver
	GetView()->ApplyCfg(&config);

	// Ventana enmarcada (emergente)
	if (config.popup_swnd != m_bPopup) {
		// Borrar todas las subventanas.
		GetView()->ClrSWnd();

		// cambiar
		m_bPopup = config.popup_swnd;
	}

	

	// Ventana del marco (raton)
	m_bMouseMid = config.mouse_mid;
	m_bAutoMouse = config.auto_mouse;

	// Aplicar configuracion de VSync
	m_bVSyncEnabled = config.render_vsync;
	GetView()->SetVSync(m_bVSyncEnabled);

	// Aplicar configuracion de renderizador (si es diferente)
	GetView()->ApplyRendererConfig(config.render_mode);

	// Para evitar Ruta SaveStates incorrecta *-*
	if (RutaSaveStates.GetLength() == 0)
		RutaSaveStates = config.ruta_savestate;
	//int msgboxID = MessageBox(RutaSaveStates,"rutasave",  2 );	
	if (config.mouse_port == 0) {
		// Modo raton desactivado si no hay conexion con el raton.
		if (GetInput()->GetMouseMode()) {
			OnMouseMode(); 
		}
	}
}

//---------------------------------------------------------------------------
//
//	Patadas
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

	// Tratamiento de errores en primer lugar
	switch (m_nStatus) {
		// VMƒGƒ‰[
		case 1:
			::GetMsg(IDS_INIT_VMERR, strMsg);
			MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
			PostMessage(WM_CLOSE, 0, 0);
			return 0;

		// Error en los componentes
		case 2:
			::GetMsg(IDS_INIT_COMERR, strMsg);
			MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
			PostMessage(WM_CLOSE, 0, 0);
			return 0;
	}
	// Si es normal
	ASSERT(m_nStatus == 0);

	// Comprobacion de la ROM
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

	// Obtener la configuracion (para el ajuste de power_off)
	GetConfig()->GetConfig(&config);
	if (config.power_off) {
		// “dŒ¹OFF‚Å‹N“®
		::GetVM()->SetPower(FALSE);
		::GetVM()->PowerSW(FALSE);
	}

	// Preparacion de la subventana
	m_strWndClsName = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS);

	// Activa el componente. Sin embargo, el Programador es configurable.
	GetView()->Enable(TRUE);
	pComponent = m_pFirstComponent;
	while (pComponent) {
		// ƒXƒPƒWƒ…[ƒ‰‚©
		if (pComponent->GetID() == MAKEID('S', 'C', 'H', 'E')) {
			if (config.power_off) {
				// “dŒ¹OFF‚Å‹N“®
				pComponent->Enable(FALSE);
				pComponent = pComponent->GetNextComponent();
				continue;
			}
		}

		// ƒCƒl[ƒuƒ‹
		pComponent->Enable(TRUE);
		pComponent = pComponent->GetNextComponent();
	}

	// ƒŠƒZƒbƒg(ƒXƒe[ƒ^ƒXƒo[‚Ì‚½‚ß)
	if (!config.power_off) {
		OnReset();
	}

	// ƒRƒ}ƒ“ƒhƒ‰ƒCƒ“ˆ—
	lpszCmd = AfxGetApp()->m_lpCmdLine;
	lpszCommand = A2T(lpszCmd);
	if (_tcslen(lpszCommand) > 0) {
		InitCmd(lpszCommand);
	}

	// Å‘å‰»Žw’è‚Å‚ ‚ê‚ÎA–ß‚µ‚½Œã‚ÉAƒtƒ‹ƒXƒNƒŠ[ƒ“
	bFullScreen = FALSE;
	if (IsZoomed()) {
		ShowWindow(SW_RESTORE);
		bFullScreen = TRUE;
	}

	// ƒEƒCƒ“ƒhƒEˆÊ’u‚ðƒŒƒWƒ…[ƒ€
	bFullScreen = RestoreFrameWnd(bFullScreen);
	if (bFullScreen) {
		// Å‘å‰»Žw’è‚©A‘O‰ñŽÀsŽž‚Éƒtƒ‹ƒXƒNƒŠ[ƒ“
		PostMessage(WM_COMMAND, IDM_FULLSCREEN);
	}

	// ƒfƒBƒXƒNEƒXƒe[ƒg‚ðƒŒƒWƒ…[ƒ€
	RestoreDiskState();

	// –³ŒÀƒ‹[ƒv
	dwTick20 = ::GetTickCount();
	dwTick40 = dwTick20;
	dwTick80 = dwTick20;
	while (!m_bExit) {
		// ƒƒbƒZ[ƒWƒ`ƒFƒbƒN•ƒ|ƒ“ƒv
		if (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			if (!AfxGetApp()->PumpMessage()) {
				::PostQuitMessage(0);
				return 0;
			}
			// continue‚·‚é‚±‚Æ‚ÅAWM_DESTROY’¼Œã‚Ìm_bExitƒ`ƒFƒbƒN‚ð•ÛØ
			continue;
		}

		// ƒXƒŠ[ƒv
		if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			Sleep(1);
			if (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
				continue;
			}

			// Info‚ð–ˆ‰ñŽæ‚è’¼‚·
			pInfo = GetInfo();
			if (!pInfo) {
				continue;
			}

			// XVƒJƒEƒ“ƒ^Up
			dwNow = ::GetTickCount();

			// ƒXƒe[ƒ^ƒXEŽÀs‚Í20ms
			if ((dwNow - dwTick20) >= 20) {
				dwTick20 = dwNow;
				pInfo->UpdateStatus();
				UpdateExec();
			}

			if ((dwNow - dwTick40) >= 40) {
				dwTick40 = dwNow;
				// ƒrƒ…[‚Í40ms
				GetView()->Update();
			}

			if ((dwNow - dwTick80) >= 80) {
				dwTick80 = dwNow;
				// ƒLƒƒƒvƒVƒ‡ƒ“Aî•ñ‚Í80ms
				pInfo->UpdateCaption();
				pInfo->UpdateInfo();
			}
		}
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	Obtener el nombre de la clase de la ventana
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
//	Pop-up.
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::IsPopupSWnd() const
{
	ASSERT(this);
	return m_bPopup;
}

//---------------------------------------------------------------------------
//
//	Cierre de ventanas
//
//---------------------------------------------------------------------------
void CFrmWnd::OnClose()
{
	CString strFormat;
	CString strText;
	Filepath path;	

	ASSERT(this);
	ASSERT(!m_bSaved);


	//int msgboxID = MessageBox("cerrar", "Ventana a cerrar", 2);

/*ACA  SE DESACTIVA DIALOGO CONFIRMACION GUARDADO*/
	// Si hay un archivo de estado valido, pide que se guarde
	::LockVM();
	::GetVM()->GetPath(path);
	::UnlockVM();

	// Si hay un archivo de estado valido y
	if (!path.IsClear()) {
		// Si tiene un historial de funcionamiento de mas de 20ms en el lado de Windows
	/*	if (m_dwExec >= 2) {
			// Confirmacion
			::GetMsg(IDS_SAVECLOSE, strFormat);
			strText.Format(strFormat, path.GetFileExt());
			nResult = MessageBox(strText, NULL, MB_ICONQUESTION | MB_YESNOCANCEL);

			// Depende de los resultados de la confirmacion
			switch (nResult) {
				// YES
				case IDYES:
					// Guardar
					OnSaveSub(path);
					break;

				// NO
				case IDNO:
					// Via libre (sin estado)
					::GetVM()->Clear();
					break;

				// ƒLƒƒƒ“ƒZƒ‹
				case IDCANCEL:
					// Fingire que no estaba cerrado.
					return;
			}
		}*/
	}

	// Si ya has inicializado
	if ((m_nStatus == 0) && !m_bSaved) {
		// Guardar el estado de la ventana y el estado del disco
		SaveFrameWnd();
		SaveDiskState();
		m_bSaved = TRUE;
	}

	// Desactivar en pantalla completa
	if (m_bFullScreen) {
		ASSERT(m_nStatus == 0);
		OnFullScreen();
	}

	// Si ya has inicializado
	if (m_nStatus == 0) {
		// Liberacion del raton
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}

	OutputDebugString("\n\nSe ejecutó OnClose...\n\n");
	// Šî–{ƒNƒ‰ƒX
	CFrameWnd::OnClose();
}

//---------------------------------------------------------------------------
//
//	Eliminacion de ventanas
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDestroy()
{
	ASSERT(this);

	// Si ya has inicializado
	if ((m_nStatus == 0) && !m_bSaved) 
	{
		// Guardar el estado de la ventana y el estado del disco
		SaveFrameWnd();
		SaveDiskState();
		m_bSaved = TRUE;
	}

	// Desactivar en pantalla completa
	if (m_bFullScreen) {
		ASSERT(m_nStatus == 0);
		OnFullScreen();
	}

	// Limpieza (comun con WM_ENDSESSION)
	CleanSub();


	OutputDebugString("\n\nSe ejecutó OnDestroy...\n\n");

	// Šî–{ƒNƒ‰ƒX‚Ö
	CFrameWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	Fin de la sesion
//
//---------------------------------------------------------------------------
void CFrmWnd::OnEndSession(BOOL bEnding)
{
	ASSERT(this);

	// Al salir, limpiar
	if (bEnding) {
		// Si ya has inicializado
		if (m_nStatus == 0) {
			// Guardar el estado de la ventana y el estado del disco
			if (!m_bSaved) {
				SaveFrameWnd();
				SaveDiskState();
				m_bSaved = TRUE;
			}

			// Limpieza
			CleanSub();
		}
	}


	OutputDebugString("\n\nSe ejecutó OnEndSession...\n\n");

	// Šî–{ƒNƒ‰ƒX
	CFrameWnd::OnEndSession(bEnding);
}

//---------------------------------------------------------------------------
//
//	Limpieza comun
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::CleanSub()
{
	CComponent *pComponent;
	CComponent *pNext;
	int i;

	// I—¹ƒtƒ‰ƒO‚ðã‚°‚é
	m_bExit = TRUE;

	// ƒRƒ“ƒ|[ƒlƒ“ƒg‚ðŽ~‚ß‚é
	GetView()->Enable(FALSE);
	pComponent = m_pFirstComponent;
	while (pComponent) {
		pComponent->Enable(FALSE);
		pComponent = pComponent->GetNextComponent();
	}

	// ƒ}ƒEƒX‰ðœ
	if (m_nStatus == 0) {
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}

	// ƒXƒPƒWƒ…[ƒ‰‚ªŽÀs‚ð‚â‚ß‚é‚Ü‚Å‘Ò‚Â
	for (i=0; i<8; i++) {
		::LockVM();
		::UnlockVM();
	}

	// ƒXƒPƒWƒ…[ƒ‰‚ð’âŽ~(CScheduler)
	if (m_nStatus == 0) {
		GetScheduler()->Stop();
	}

	// ƒRƒ“ƒ|[ƒlƒ“ƒg‚ðíœ
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

	// ‰¼‘zƒ}ƒVƒ“‚ðíœ
	if (::pVM) {
		::LockVM();
		::GetVM()->Cleanup();
		delete ::pVM;
		::pVM = NULL;
		::UnlockVM();
	}

	// ƒVƒFƒ‹’Ê’m‚ðíœ
	if (m_uNotifyId) {
		 VERIFY(::SHChangeNotifyDeregister(m_uNotifyId));
		 m_uNotifyId = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Guardar el estado de la ventana
//
//---------------------------------------------------------------------------
void CFrmWnd::SaveFrameWnd()
{
	CRect rectWnd;
	Config config;

	ASSERT(this);
	ASSERT_VALID(this);

	// Adquisicion de la configuracion
	GetConfig()->GetConfig(&config);

	// Leyenda, menu y barra de estado
	config.caption = m_bCaption;
	config.menu_bar = m_bMenuBar;
	config.status_bar = m_bStatusBar;

	// Rectangulo de la ventana
	if (m_bFullScreen) {
		// En pantalla completa, se conserva la posicion de la ventana
		config.window_left = m_nWndLeft;
		config.window_top = m_nWndTop;
	}
	else {
		// En una ventana, se guarda la posicion actual
		GetWindowRect(&rectWnd);
		config.window_left = rectWnd.left;
		config.window_top = rectWnd.top;
	}

	// Pantalla completa
	config.window_full = m_bFullScreen;

	// Cambiar la configuracion
	GetConfig()->SetConfig(&config);
}

//---------------------------------------------------------------------------
//
//	Guardar el estado del disco
//
//---------------------------------------------------------------------------
void CFrmWnd::SaveDiskState()
{
	int nDrive;
	Filepath path;
	Config config;

	ASSERT(this);
	ASSERT_VALID(this);

	// ƒƒbƒN
	::LockVM();

	// Ý’èŽæ“¾
	GetConfig()->GetConfig(&config);

	// ƒtƒƒbƒs[ƒfƒBƒXƒN
	for (nDrive=0; nDrive<2; nDrive++) {
		// ƒŒƒfƒB
		config.resume_fdi[nDrive] = m_pFDD->IsReady(nDrive, FALSE);

		// ƒŒƒfƒB‚Å‚È‚¯‚ê‚ÎAŽŸ‚Ö
		if (!config.resume_fdi[nDrive]) {
			continue;
		}

		// ƒƒfƒBƒA
		config.resume_fdm[nDrive]  = m_pFDD->GetMedia(nDrive);

		// ƒ‰ƒCƒgƒvƒƒeƒNƒg
		config.resume_fdw[nDrive] = m_pFDD->IsWriteP(nDrive);
	}

	// MOƒfƒBƒXƒN
	config.resume_mos = m_pSASI->IsReady();
	if (config.resume_mos) {
		config.resume_mow = m_pSASI->IsWriteP();
	}

	// CD-ROM
	config.resume_iso = m_pSCSI->IsReady(FALSE);

	// ƒXƒe[ƒg
	::GetVM()->GetPath(path);
	config.resume_xm6 = !path.IsClear();

	// ƒfƒtƒHƒ‹ƒgƒfƒBƒŒƒNƒgƒŠ
	_tcscpy(config.resume_path, Filepath::GetDefaultDir());

	// Ý’è•ÏX
	GetConfig()->SetConfig(&config);

	// ƒAƒ“ƒƒbƒN
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Restaurar el estado de la ventana
//	OnCreate y OnKick son llamados dos veces.
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

	// Adquisicion de la configuracion
	GetConfig()->GetConfig(&config);

	// Se ejecuta en el estado por defecto si no se especifica la restauracion de la posicion de la ventana
	if (!config.resume_screen) {
		return bFullScreen;
	}

	// Leyenda
	m_bCaption = config.caption;
	ShowCaption();

	// Menu
	m_bMenuBar = config.menu_bar;
	ShowMenu();

	// Barra de estado
	m_bStatusBar = config.status_bar;
	ShowStatus();

	// Obtener el tamano y el origen de la pantalla virtual
	nWidth = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
	nHeight = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
	nLeft = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
	nTop = ::GetSystemMetrics(SM_YVIRTUALSCREEN);

	// Obtener el rectangulo de la ventana
	GetWindowRect(&rectWnd);

	// Mueva la posicion de la ventana si esta a su alcance. Compruebe primero
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

	// Mover la posicion de la ventana
	if (bValid) {
		SetWindowPos(&wndTop, config.window_left, config.window_top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

		// Cambiar el area de trabajo al mismo tiempo
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




	// Esto en cuanto a las maquinas virtuales no inicializadas.
	if (m_nStatus != 0) {
		return FALSE;
	}


	


	// Pantalla completa.
	if (bFullScreen || config.window_full) {
		// Maximizar el inicio o la ultima vez que estaba a pantalla completa.
		return TRUE;
	}
	else {
		// No maximizado, y previamente en vista normal
		return FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Restaurar el estado del disco
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

	// Ý’èŽæ“¾
	GetConfig()->GetConfig(&config);

	// ƒXƒe[ƒg‚ªŽw’è‚³‚ê‚Ä‚¢‚ê‚ÎA‚±‚ê‚ðæ‚És‚¤
	if (config.resume_state) {
		// ƒXƒe[ƒg‚ª‚ ‚Á‚½
		if (config.resume_xm6) {
			// ƒpƒXŽæ“¾
			GetConfig()->GetMRUFile(4, 0, szMRU);
			path.SetPath(szMRU);

			// ƒI[ƒvƒ“‘Oˆ—
			if (OnOpenPrep(path)) {
				// ƒI[ƒvƒ“ƒTƒu
				if (OnOpenSub(path)) {
					// ¬Œ÷‚È‚Ì‚ÅAƒfƒtƒHƒ‹ƒgƒfƒBƒŒƒNƒgƒŠ‚¾‚¯ˆ—
					if (config.resume_dir) {
						Filepath::SetDefaultDir(config.resume_path);
					}

					// ‚±‚êˆÈ~‚Íˆ—‚µ‚È‚¢(FD, MO, CD‚ÌƒAƒNƒZƒX’†‚ÉƒZ[ƒu‚µ‚½ê‡)
					return;
				}
			}
		}
	}

	// ƒtƒƒbƒs[ƒfƒBƒXƒN
	if (config.resume_fd) {
		for (nDrive=0; nDrive<2; nDrive++) {
			// ƒfƒBƒXƒN‘}“ü‚³‚ê‚Ä‚¢‚½‚©
			if (!config.resume_fdi[nDrive]) {
				// ƒfƒBƒXƒN‘}“ü‚³‚ê‚Ä‚¢‚È‚¢BƒXƒLƒbƒv
				continue;
			}

			// ƒfƒBƒXƒN‘}“ü
			GetConfig()->GetMRUFile(nDrive, 0, szMRU);
			ASSERT(szMRU[0] != _T('\0'));
			path.SetPath(szMRU);

			// VMƒƒbƒN‚ðs‚¢AƒfƒBƒXƒNŠ„‚è“–‚Ä‚ðŽŽ‚Ý‚é
			::LockVM();
			bResult = m_pFDD->Open(nDrive, path, config.resume_fdm[nDrive]);
			::UnlockVM();

			// Š„‚è“–‚Ä‚Å‚«‚È‚¯‚ê‚ÎƒXƒLƒbƒv
			if (!bResult) {
				continue;
			}

			// ‘‚«ž‚Ý‹ÖŽ~
			if (config.resume_fdw[nDrive]) {
				::LockVM();
				m_pFDD->WriteP(nDrive, TRUE);
				::UnlockVM();
			}
		}
	}

	// MOƒfƒBƒXƒN
	if (config.resume_mo) {
		// ƒfƒBƒXƒN‘}“ü‚³‚ê‚Ä‚¢‚½‚©
		if (config.resume_mos) {
			// ƒfƒBƒXƒN‘}“ü
			GetConfig()->GetMRUFile(2, 0, szMRU);
			ASSERT(szMRU[0] != _T('\0'));
			path.SetPath(szMRU);

			// VMƒƒbƒN‚ðs‚¢AƒfƒBƒXƒNŠ„‚è“–‚Ä‚ðŽŽ‚Ý‚é
			::LockVM();
			bResult = m_pSASI->Open(path);
			::UnlockVM();

			// Š„‚è“–‚Ä‚Å‚«‚ê‚Î
			if (bResult) {
				// ‘‚«ž‚Ý‹ÖŽ~
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
		// ƒfƒBƒXƒN‘}“ü‚³‚ê‚Ä‚¢‚½‚©
		if (config.resume_iso) {
			// ƒfƒBƒXƒN‘}“ü
			GetConfig()->GetMRUFile(3, 0, szMRU);
			ASSERT(szMRU[0] != _T('\0'));
			path.SetPath(szMRU);

			// VMƒƒbƒN‚ðs‚¢AƒfƒBƒXƒNŠ„‚è“–‚Ä‚ðŽŽ‚Ý‚é
			::LockVM();
			m_pSCSI->Open(path, FALSE);
			::UnlockVM();
		}
	}

	// ƒfƒtƒHƒ‹ƒgƒfƒBƒŒƒNƒgƒŠ
	if (config.resume_dir) {
		Filepath::SetDefaultDir(config.resume_path);
	}
}

//---------------------------------------------------------------------------
//
//	Cambio de pantalla
//
//---------------------------------------------------------------------------
LRESULT CFrmWnd::OnDisplayChange(UINT uParam, LONG lParam)
{
	LRESULT lResult;
	uParam = 0;
	lParam = 0;
	// Clase basica
	lResult = 0; //CFrameWnd::OnDisplayChange(0, uParam, lParam);

	// La minimizacion no hace nada.
	if (IsIconic()) {
		return lResult;
	}

	// Ajuste de la posicion
	InitPos(FALSE);

	return lResult;
}

//---------------------------------------------------------------------------
//
//	Renderizacion del fondo de la ventana
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::OnEraseBkgnd(CDC * /* pDC */)
{
	// Suprimir el dibujo de fondo
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Representacion de ventanas
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPaint()
{
	PAINTSTRUCT ps;

	// Hazlo siempre con un candado
	::LockVM();

	BeginPaint(&ps);

	// Restablecer el titulo y la barra de estado si VM esta activado
	if (m_nStatus == 0) {
		ResetCaption();
		ResetStatus();
	}

	EndPaint(&ps);

	// Desbloquear
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Movimiento de las ventanas
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMove(int x, int y)
{
	CRect rect;

	// ‰Šú‰»Ï‚Ý‚È‚ç
	if (m_nStatus == 0) {
		// ƒ}ƒEƒXƒ‚[ƒhƒ`ƒFƒbƒN
		if (GetInput()->GetMouseMode()) {
			// ƒNƒŠƒbƒv”ÍˆÍ‚ð•ÏX
			ClipCursor(NULL);
			GetWindowRect(&rect);
			SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
			ClipCursor(&rect);
		}
	}

	// Šî–{ƒNƒ‰ƒX
	CFrameWnd::OnMove(x, y);
}

//---------------------------------------------------------------------------
//
//	Activar
//
//---------------------------------------------------------------------------
void CFrmWnd::OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized)
{
	CInput *pInput;
	CScheduler *pScheduler;

	// ‰Šú‰»Ï‚Ý‚È‚ç
	if (m_nStatus == 0) {
		// ƒCƒ“ƒvƒbƒgAƒXƒPƒWƒ…[ƒ‰‚Ö’Ê’m
		pInput = GetInput();
		pScheduler = GetScheduler();
		if (pInput && pScheduler) {
			// WA_INACTIVE‚©Å¬‰»‚È‚çAƒfƒBƒZ[ƒuƒ‹
			if ((nState == WA_INACTIVE) || bMinimized) {
				// “ü—ÍŽó‚¯•t‚¯‚È‚¢A’á‘¬ŽÀs
				pInput->Activate(FALSE);
				pScheduler->Activate(FALSE);

				// ƒ}ƒEƒXƒ‚[ƒhOFF(POPUPƒEƒBƒ“ƒhƒE‘Îô)
				if (pInput->GetMouseMode()) {
					OnMouseMode();
				}
			}
			else {
				// “ü—ÍŽó‚¯•t‚¯‚éA’ÊíŽÀs
				pInput->Activate(TRUE);
				pScheduler->Activate(TRUE);
			}
		}
	}

	// Šî–{ƒNƒ‰ƒX‚Ö
	CFrameWnd::OnActivate(nState, pWnd, bMinimized);
}

//---------------------------------------------------------------------------
//
//	Aplicacion de activacion
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
void CFrmWnd::OnActivateApp(BOOL bActive, DWORD dwThreadID)
#else
void CFrmWnd::OnActivateApp(BOOL bActive, HTASK hTask)
#endif
{
	// Si ya has inicializado
	if (m_nStatus == 0) {
		// ƒtƒ‹ƒXƒNƒŠ[ƒ“ê—p
		if (m_bFullScreen) {
			if (bActive) {
				// Estoy a punto de ser activo
				HideTaskBar(TRUE, TRUE);
				RecalcStatusView();
			}
			else {
				// Fuera de la actividad
				HideTaskBar(FALSE, FALSE);
			}
		}
	}

	// Clase basica
#if _MFC_VER >= 0x700
	CFrameWnd::OnActivateApp(bActive, dwThreadID);
#else
	CFrameWnd::OnActivateApp(bActive, hTask);
#endif
}

//---------------------------------------------------------------------------
//
//	Comienza el bucle del menu
//
//---------------------------------------------------------------------------
void CFrmWnd::OnEnterMenuLoop(BOOL bTrackPopup)
{
	CInput *pInput;
	CScheduler *pScheduler;

	// Restablecimiento de los subtitulos
	ResetCaption();

	::LockVM();

	// Notificar entradas
	pInput = GetInput();
	if (pInput) {
		pInput->Menu(TRUE);
	}

	// ƒ}ƒEƒXƒ‚[ƒhFALSE(ƒ}ƒEƒX‚Åƒƒjƒ…[‚ª‘€ì‚Å‚«‚é‚æ‚¤‚É)
	if (pInput->GetMouseMode()) {
		OnMouseMode();
	}

	// ƒXƒPƒWƒ…[ƒ‰‚Ö’Ê’m
	pScheduler = GetScheduler();
	if (pScheduler) {
		pScheduler->Menu(TRUE);
	}

	::UnlockVM();

	// Šî–{ƒNƒ‰ƒX‚Ö
	CFrameWnd::OnEnterMenuLoop(bTrackPopup);
}

//---------------------------------------------------------------------------
//
//	Fin del bucle del menu
//
//---------------------------------------------------------------------------
void CFrmWnd::OnExitMenuLoop(BOOL bTrackPopup)
{
	CInput *pInput;
	CScheduler *pScheduler;

	::LockVM();

	// ƒCƒ“ƒvƒbƒg‚Ö’Ê’m
	pInput = GetInput();
	if (pInput) {
		pInput->Menu(FALSE);
	}

	// ƒXƒPƒWƒ…[ƒ‰‚Ö’Ê’m
	pScheduler = GetScheduler();
	if (pScheduler) {
		pScheduler->Menu(FALSE);
	}

	::UnlockVM();

	// ƒLƒƒƒvƒVƒ‡ƒ“ƒŠƒZƒbƒg
	ResetCaption();

	// Šî–{ƒNƒ‰ƒX‚Ö
	CFrameWnd::OnExitMenuLoop(bTrackPopup);
}

//---------------------------------------------------------------------------
//
//	Notificacion de la ventana de los padres
//
//---------------------------------------------------------------------------
void CFrmWnd::OnParentNotify(UINT message, LPARAM lParam)
{
	CInput *pInput;

	// CInput‚ðŽæ“¾A’Ê’m
	if ((message == WM_MBUTTONDOWN) && (m_nStatus == 0)) {
		// ƒCƒ“ƒvƒbƒg‚ðŽæ“¾
		pInput = GetInput();
		if (pInput) {
			// ƒ}ƒEƒX–³Œø‚È‚ç—LŒø‚É‚·‚éB‹t‚Í‚µ‚È‚¢
			if (!pInput->GetMouseMode()) {
				// Ý’è‚Å"’†ƒ{ƒ^ƒ“‹ÖŽ~"‚É‚³‚ê‚Ä‚¢‚È‚¢‚±‚Æ‚ªðŒ
				if (m_bMouseMid) {
					OnMouseMode();
				}
			}
		}
	}

	// Šî–{ƒNƒ‰ƒX‚Ö
	CFrameWnd::OnParentNotify(message, lParam);
}

//---------------------------------------------------------------------------
//
//	Menu contextual
//
//---------------------------------------------------------------------------
void CFrmWnd::OnContextMenu(CWnd * /*pWnd*/, CPoint pos)
{
	CMenu *pMenu;
	SHORT sF10;
	SHORT sShift;

	// ƒL[ƒ{[ƒh‚©‚ç‚Ì“ü—Í‚Ì‚Æ‚«
	if ((pos.x == -1) && (pos.y == -1)) {
		// ƒXƒPƒWƒ…[ƒ‰ƒ`ƒFƒbƒNA“ü—Íƒ`ƒFƒbƒN
		if (GetScheduler()->IsEnable()) {
			if (GetInput()->IsActive() && !GetInput()->IsMenu()) {
				// DIK_APPS‚ªƒ}ƒbƒv‚³‚ê‚Ä‚¢‚é‚©
				if (GetInput()->IsKeyMapped(DIK_APPS)) {
					// SHIFT+F10‚ª‰Ÿ‚³‚ê‚Ä‚¢‚é‚©
					sF10 = ::GetAsyncKeyState(VK_F10);
					sShift = ::GetAsyncKeyState(VK_SHIFT);
					if (((sF10 & 0x8000) == 0) || ((sShift & 0x8000) == 0)) {
						// VK_APPS‚ª‰Ÿ‚³‚ê‚½‚½‚ß‚Æ”»’è
						return;
					}
				}
			}
		}

		// ƒ}ƒEƒXƒ‚[ƒh‚Å‚ ‚ê‚ÎA‰ðœ(ƒL[ƒ{[ƒh‚©‚ç‚Ìƒƒjƒ…[‹N“®)
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}
	else {
		// ƒ}ƒEƒXƒ‚[ƒh‚Å‚ ‚ê‚ÎA–³Ž‹(ƒ}ƒEƒX‚©‚ç‚Ìƒƒjƒ…[‹N“®)
		if (GetInput()->GetMouseMode()) {
			return;
		}
	}

	// ƒ|ƒbƒvƒAƒbƒvƒƒjƒ…[
	m_bPopupMenu = TRUE;
	pMenu = m_PopupMenu.GetSubMenu(0);
	pMenu->TrackPopupMenu(TPM_CENTERALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							pos.x, pos.y, this, 0);
	m_bPopupMenu = FALSE;
}

//---------------------------------------------------------------------------
//
//	Notificacion de cambio de potencia
//
//---------------------------------------------------------------------------
LONG CFrmWnd::OnPowerBroadCast(UINT /*uParam*/, LONG /*lParam*/)
{
	// ‰Šú‰»Ï‚Ý‚È‚ç
	if (m_nStatus == 0) {
		// VMƒƒbƒNAŽžŠÔÄÝ’è
		::LockVM();
		timeEndPeriod(1);
		timeBeginPeriod(1);
		::UnlockVM();
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Comandos del sistema
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSysCommand(UINT nID, LPARAM lParam)
{
	// •W€ƒEƒBƒ“ƒhƒEˆÊ’u‚ðƒTƒ|[ƒg
	if ((nID & 0xfff0) == IDM_STDWIN) {
		InitPos(TRUE);
		return;
	}

	// Å‘å‰»‚Íƒtƒ‹ƒXƒNƒŠ[ƒ“
	if ((nID & 0xfff0) == SC_MAXIMIZE) {
		if (!m_bFullScreen) {
			PostMessage(WM_COMMAND, IDM_FULLSCREEN);
		}
		return;
	}

	// Šî–{ƒNƒ‰ƒX
	CFrameWnd::OnSysCommand(nID, lParam);
}

//---------------------------------------------------------------------------
//
//	Transferencia de datos
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
afx_msg BOOL CFrmWnd::OnCopyData(CWnd* /*pWnd*/, COPYDATASTRUCT* pCopyDataStruct)
#else
LONG CFrmWnd::OnCopyData(UINT /*uParam*/, LONG pCopyDataStruct)
#endif
{
	PCOPYDATASTRUCT pCDS;

	// ƒpƒ‰ƒ[ƒ^Žó‚¯Žæ‚è
	pCDS = (PCOPYDATASTRUCT)pCopyDataStruct;

	// ƒRƒ}ƒ“ƒhƒ‰ƒCƒ“ˆ—‚Ö
	InitCmd((LPSTR)pCDS->lpData);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Notificaciones de la cascara
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

	// Windows NT‚©
	if (::IsWinNT()) {
		// Windows2000/XP‚Ìê‡ASHChangeNotification_Lock‚ÅƒƒbƒN‚·‚é
		hMemoryMap = (HANDLE)uParam;
		dwProcessId = (DWORD)lParam;
		hLock = ::SHChangeNotification_Lock(hMemoryMap, dwProcessId, &pidls, &nEvent);
		if (hLock == NULL) {
			return 0;
		}
	}
	else {
		// Windows9x‚Ìê‡Apidls‚ÆnEvent‚ÍuParam,lParam‚©‚ç’¼Ú“¾‚é
		pidls = (LPITEMIDLIST*)uParam;
		nEvent = lParam;
		hLock = NULL;
	}

	// ŽÀs’†‚ÅACHost‚ª‚ ‚ê‚ÎA’Ê’m
	if (m_nStatus == 0) {
		pHost = GetHost();

#if 1
		// Windrv‚ª‚Ü‚¾•sˆÀ’è‚Ì‚½‚ßAŽÀÛ‚ÉEnable‚É‚³‚ê‚Ä‚¢‚È‚¢ê‡‚Í‰½‚à‚µ‚È‚¢(version2.04)
		{
			Config config;
			GetConfig()->GetConfig(&config);
			if ((config.windrv_enable <= 0) || (config.windrv_enable > 3)) {
				pHost = NULL;
			}
		}
#endif

		if (pHost) {
			// ƒpƒXŽæ“¾
			::SHGetPathFromIDList(pidls[0], szPath);

			// ’Ê’m
			pHost->ShellNotify(nEvent, szPath);
		}
	}

	// NT‚Ìê‡ASHCnangeNotifcation_Unlock‚ÅƒAƒ“ƒƒbƒN‚·‚é
	if (::IsWinNT()) {
		ASSERT(hLock);
		::SHChangeNotification_Unlock(hLock);
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	Actualizacion (ejecucion)
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::UpdateExec()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Si el programador esta activado, aumenta el contador de ejecucion (se borra al guardar)
	if (GetScheduler()->IsEnable()) {
		m_dwExec++;
		if (m_dwExec == 0) {
			m_dwExec--;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Cadena de mensajes proporcionada
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

	// Indicador FALSE
	bValid = FALSE;

	// Haga primero las cadenas de menu (considere el entorno ingles + MRU)
	if ((nID >= IDM_OPEN) && (nID <= IDM_ABOUT)) {
		// ‰pŒêŠÂ‹«‚©
		if (!::IsJapanese()) {
			// +5000‚ÅŽŽ‚·
			if (rMessage.LoadString(nID + 5000)) {
				bValid = TRUE;
			}
		}
	}

	// Excepcion de cadena de menu (IDM_STDWIN)
	if (nID == IDM_STDWIN) {
		// ‰pŒêŠÂ‹«‚©
		if (!::IsJapanese()) {
			// +5000‚ÅŽŽ‚·
			if (rMessage.LoadString(nID + 5000)) {
				bValid = TRUE;
			}
		}
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

	// ƒfƒBƒXƒN–¼0
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

	// ƒfƒBƒXƒN–¼1
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

	// ‚±‚±‚Ü‚Å‚ÅŠm’è‚µ‚Ä‚¢‚È‚¯‚ê‚ÎAŠî–{ƒNƒ‰ƒX
	if (!bValid) {
		CFrameWnd::GetMessageString(nID, rMessage);
	}

	// Œ‹‰Ê‚ðInfo‚Ö’ñ‹Ÿ(“à•”•ÛŽ—p)
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->SetMessageString(rMessage);
	}

	// Œ‹‰Ê‚ðƒXƒe[ƒ^ƒXƒrƒ…[‚Ö’ñ‹Ÿ
	if (m_pStatusView) {
		m_pStatusView->SetMenuString(rMessage);
	}
}

//---------------------------------------------------------------------------
//
//	Ocultar la barra de tareas
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::HideTaskBar(BOOL bHide, BOOL bFore)
{
	if (bHide) {
		// "í‚É‘O–Ê"
		m_hTaskBar = ::FindWindow(_T("Shell_TrayWnd"), NULL);
		if (m_hTaskBar) {
			::ShowWindow(m_hTaskBar, SW_HIDE);
		}
		ModifyStyleEx(0, WS_EX_TOPMOST, 0);
	}
	else {
		// "’Êí"
		ModifyStyleEx(WS_EX_TOPMOST, 0, 0);
		if (m_hTaskBar) {
			::ShowWindow(m_hTaskBar, SW_SHOWNA);
		}
	}

	// ‘O–ÊƒIƒvƒVƒ‡ƒ“‚ª‚ ‚ê‚Î
	if (bFore) {
		SetForegroundWindow();
	}
}

//---------------------------------------------------------------------------
//
//	Visualizacion de la barra de estado
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ShowStatus()
{
	ASSERT(this);

	// •K—v‚È‚çVM‚ðƒƒbƒN
	if (m_nStatus == 0) {
		::LockVM();
	}

	// ƒtƒ‹ƒXƒNƒŠ[ƒ“‚©
	if (m_bFullScreen) {
		// ƒXƒe[ƒ^ƒXƒo[‚Íí‚É”ñ•\Ž¦
		ShowControlBar(&m_StatusBar, FALSE, FALSE);

		// ƒXƒe[ƒ^ƒXƒo[•\Ž¦‚©
		if (m_bStatusBar) {
			// ƒXƒe[ƒ^ƒXƒrƒ…[‚ª‘¶Ý‚µ‚È‚¯‚ê‚Î
			if (!m_pStatusView) {
				// ì¬‚µ‚Ä
				CreateStatusView();

				// Ä”z’u
				if (m_bStatusBar) {
					RecalcStatusView();
				}
			}
		}
		else {
			// ƒXƒe[ƒ^ƒXƒrƒ…[‚ª‘¶Ý‚µ‚Ä‚¢‚ê‚Î
			if (m_pStatusView) {
				// íœ‚µ‚Ä
				DestroyStatusView();

				// Ä”z’u
				RecalcStatusView();
			}
		}

		// •K—v‚ª‚ ‚ê‚ÎƒAƒ“ƒƒbƒN
		if (m_nStatus == 0) {
			::UnlockVM();
		}
		return;
	}

	// ƒXƒe[ƒ^ƒXƒrƒ…[‚Íƒtƒ‹ƒXƒNƒŠ[ƒ“ê—p‚È‚Ì‚ÅAíœ
	if (m_pStatusView) {
		DestroyStatusView();
		RecalcLayout();
	}

	// ƒEƒBƒ“ƒhƒE‚È‚Ì‚ÅAShowControlBar‚Å§Œä
	ShowControlBar(&m_StatusBar, m_bStatusBar, FALSE);

	// •K—v‚ª‚ ‚ê‚ÎƒAƒ“ƒƒbƒN
	if (m_nStatus == 0) {
		::UnlockVM();
	}
}

//---------------------------------------------------------------------------
//
//	Crear vista de estado (en pantalla completa)
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::CreateStatusView()
{
	CInfo *pInfo;

	ASSERT(!m_pStatusView);

	if (m_bStatusBar) {
		// ƒXƒe[ƒ^ƒXƒrƒ…[ì¬(Ä”z’u‚Ís‚í‚È‚¢)
		m_pStatusView = new CStatusView;
		if (m_pStatusView->Init(this)) {
			// ì¬¬Œ÷
			pInfo = GetInfo();
			if (pInfo) {
				// Info‚ª‘¶Ý‚·‚é‚Ì‚ÅAƒXƒe[ƒ^ƒXƒrƒ…[ì¬‚ð’Ê’m
				pInfo->SetStatusView(m_pStatusView);
			}
		}
		else {
			// ì¬Ž¸”s
			m_bStatusBar = FALSE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Salir de la vista de estado (en pantalla completa)
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::DestroyStatusView()
{
	CInfo *pInfo;

	// —LŒø‚ÈƒXƒe[ƒ^ƒXƒrƒ…[‚ª‘¶Ý‚·‚éê‡‚Ì‚Ý
	if (m_pStatusView) {
		// InfoŽæ“¾
		pInfo = GetInfo();
		if (pInfo) {
			// Info‚ª‘¶Ý‚·‚é‚Ì‚ÅAƒXƒe[ƒ^ƒXƒrƒ…[íœ‚ð’Ê’m
			pInfo->SetStatusView(NULL);
		}

		// ƒXƒe[ƒ^ƒXƒrƒ…[íœ(Ä”z’u‚Ís‚í‚È‚¢)
		m_pStatusView->DestroyWindow();
		m_pStatusView = NULL;
	}
}

//---------------------------------------------------------------------------
//¿Qué hace RecalcStatusView ?
//Es responsable de recalcular y ajustar la posición / tamaño de dos componentes críticos en la ventana principal :
//Vista de Dibujo(m_pDrawView) : Donde se renderiza la pantalla emulada.
//Vista de Estado(m_pStatusView) : Una barra de estado personalizada que reemplaza a la barra estándar de Windows en pantalla completa.
//---------------------------------------------------------------------------
void CFrmWnd::RecalcStatusView()
{
	CRect rectClient;
	GetClientRect(&rectClient);  // Área cliente actual

	const int clientWidth = rectClient.Width();
	const int clientHeight = rectClient.Height();

	// Flags comunes para SetWindowPos
	const UINT swpFlags = SWP_NOZORDER | SWP_NOACTIVATE;

	if (m_pStatusView && m_pStatusView->GetSafeHwnd())
	{
		CRect rectStatus;
		m_pStatusView->GetWindowRect(&rectStatus);
		const int statusHeight = rectStatus.Height();

		// Calcular altura para vista de dibujo
		const int drawHeight = clientHeight - statusHeight;

		// Solo redimensionar si hubieron cambios
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

		// Actualizar vista de estado solo si es necesario
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
		// Modo normal sin vista de estado personalizada
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

	// Forzar actualización visual solo si hay cambios
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
//	Restablecimiento de la barra de estado
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ResetStatus()
{
	CInfo *pInfo;

	// Info‚ª‚ ‚ê‚ÎƒŠƒZƒbƒg
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->ResetStatus();
	}
}

//---------------------------------------------------------------------------
//
//	Sorteo del propietario
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDrawItem(int nID, LPDRAWITEMSTRUCT lpDIS)
{
	int nPane;
	HDC hDC;
	CRect rectDraw;
	CInfo *pInfo;

	// ƒEƒBƒ“ƒhƒEƒnƒ“ƒhƒ‹‚Ìƒ`ƒFƒbƒN
	if (lpDIS->hwndItem != m_StatusBar.m_hWnd) {
		CFrameWnd::OnDrawItem(nID, lpDIS);
		return;
	}

	// Ží•ÊADCA‹éŒ`‚ðŽæ“¾
	nPane = lpDIS->itemID;
	if (nPane == 0) {
		return;
	}
	nPane--;
	hDC = lpDIS->hDC;
	rectDraw = &lpDIS->rcItem;

	// Info‚Ìƒ`ƒFƒbƒN
	pInfo = GetInfo();
	if (!pInfo) {
		// •‚Å“h‚è‚Â‚Ô‚·
		::SetBkColor(hDC, RGB(0, 0, 0));
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rectDraw, NULL, 0, NULL);
		return;
	}

	// Info‚ÉŽwŽ¦
	pInfo->DrawStatus(nPane, hDC, rectDraw);
}

//---------------------------------------------------------------------------
//
//	Visualizacion de la barra de menus
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ShowMenu()
{
	HMENU hMenu;

	ASSERT(this);

	// •K—v‚Å‚ ‚ê‚ÎVM‚ðƒƒbƒN
	if (m_nStatus == 0) {
		::LockVM();
	}

	// Œ»Ý‚Ìƒƒjƒ…[‚ðŽæ“¾
	hMenu = ::GetMenu(m_hWnd);

	// ƒƒjƒ…[‚ª•s•K—v‚Èê‡
	if (m_bFullScreen || !m_bMenuBar) {
		// ƒƒjƒ…[‚ª‘¶Ý‚·‚é‚©
		if (hMenu != NULL) {
			// ƒƒjƒ…[‚ðÁ‹Ž
			SetMenu(NULL);
		}
		if (m_nStatus == 0) {
			::UnlockVM();
		}
		return;
	}

	// ƒƒjƒ…[‚ª•K—v‚Èê‡
	if (hMenu != NULL) {
		// ƒZƒbƒg‚µ‚½‚¢ƒƒjƒ…[‚Æ“¯‚¶‚©
		if (m_Menu.GetSafeHmenu() == hMenu) {
			// •ÏX‚Ì•K—v‚Í‚È‚¢
			if (m_nStatus == 0) {
				::UnlockVM();
			}
			return;
		}
	}

	// ƒƒjƒ…[‚ðƒZƒbƒg
	SetMenu(&m_Menu);

	// •K—v‚È‚çVM‚ðƒAƒ“ƒƒbƒN
	if (m_nStatus == 0) {
		::UnlockVM();
	}
}

//---------------------------------------------------------------------------
//
//	Pantalla de subtitulos
//
//---------------------------------------------------------------------------
void CFrmWnd::ShowCaption()
{
	const DWORD dwCaptionStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	const BOOL bShouldShowCaption = !m_bFullScreen && m_bCaption;

	// 1. Verificar si ya está en el estado deseado
	DWORD dwCurrentStyle = GetStyle();
	if (bShouldShowCaption == ((dwCurrentStyle & dwCaptionStyle) == dwCaptionStyle)) {
		return; // No hacer cambios si ya está en el estado correcto
	}

	// 2. Bloquear VM solo si es necesario
	const BOOL bVMLockNeeded = (m_nStatus == 0);
	if (bVMLockNeeded) {
		::LockVM();
	}

	// 3. Modificar estilos de manera eficiente
	ModifyStyle(
		bShouldShowCaption ? 0 : dwCaptionStyle,  // Estilos a remover
		bShouldShowCaption ? dwCaptionStyle : 0,  // Estilos a añadir
		SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED
	);

	// 4. Actualización condicional de la interfaz
	if (bShouldShowCaption && m_bMenuBar) {
		DrawMenuBar(); // Redibujar barra de menú si es visible
	}

	// 5. Desbloquear VM si fue bloqueada
	if (bVMLockNeeded) {
		::UnlockVM();
	}
}

//---------------------------------------------------------------------------
//
//	Restablecimiento de los subtitulos
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ResetCaption()
{
	CInfo *pInfo;

	// Info‚ª‚ ‚ê‚ÎƒŠƒZƒbƒg
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->ResetCaption();
	}
}

//---------------------------------------------------------------------------
//
//	Configuracion de la informacion
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::SetInfo(CString& strInfo)
{
	CInfo *pInfo;

	// Info‚ª‚ ‚ê‚ÎÝ’è
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->SetInfo(strInfo);
	}
}

//---------------------------------------------------------------------------
//
//	Obtener la vista del dibujo
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
//	Obtener el primer componente
//
//---------------------------------------------------------------------------
CComponent* FASTCALL CFrmWnd::GetFirstComponent() const
{
	ASSERT(this);
	return m_pFirstComponent;
}

//---------------------------------------------------------------------------
//
//	Adquisicion del programador
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
//	Adquisicion de sonido
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
//	Adquisicion de entradas
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
//	Adquisicion de puertos
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
//	Adquisicion de MIDI
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
//	Obtener TrueKey
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
//	Obtenga un anfitrion
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
//	Obtener informacion
//
//---------------------------------------------------------------------------
CInfo* FASTCALL CFrmWnd::GetInfo() const
{
	ASSERT(this);

	// Info‚ª‘¶Ý‚µ‚È‚¯‚ê‚ÎNULL
	if (!m_pInfo) {
		return NULL;
	}

	// ’âŽ~’†‚È‚çNULL
	if (!m_pInfo->IsEnable()) {
		return NULL;
	}

	// “®ì’†BInfo‚ð•Ô‚·
	return m_pInfo;
}

//---------------------------------------------------------------------------
//
//	Obtenga la configuracion
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
//	Entrar a modo Borderless Fullscreen
//
//---------------------------------------------------------------------------
void CFrmWnd::EnterBorderlessFullscreen()
{
	if (m_bBorderless) return;

	// Guardar estado actual
	m_dwPrevStyle = GetWindowLong(m_hWnd, GWL_STYLE);
	m_dwPrevExStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
	GetWindowPlacement(&m_wpPrev);

	// Obtener el monitor actual
	HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO mi = { sizeof(mi) };
	if (GetMonitorInfo(hMonitor, &mi)) {
		// Quitar bordes y estilos
		SetWindowLong(m_hWnd, GWL_STYLE, m_dwPrevStyle & ~(WS_CAPTION | WS_THICKFRAME));
		SetWindowLong(m_hWnd, GWL_EXSTYLE, m_dwPrevExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

		// Expandir rect absoluto de la pantalla
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
//	Salir de modo Borderless Fullscreen
//
//---------------------------------------------------------------------------
void CFrmWnd::ExitBorderlessFullscreen()
{
	if (!m_bBorderless) return;

	// Restaurar estilos y rects
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
