//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
//	[ MFC Component ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "crtc.h"
#include "keyboard.h"
#include "mouse.h"
#include "ppi.h"
#include "fileio.h"
#include "mfc_frm.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_res.h"
#include "mfc_cfg.h"
#include "mfc_inp.h"

//===========================================================================
//
//	Component
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CInput::CInput(CFrmWnd *pWnd) : CComponent(pWnd)
{
	int i;
	int nAxis;
	int nButton;

	// Component identifier
	m_dwID = MAKEID('I', 'N', 'P', ' ');
	m_strDesc = _T("Input Manager");

	// General initialization
	m_lpDI = NULL;
	m_bActive = TRUE;
	m_bMenu = FALSE;
	m_pCRTC = NULL;
	m_dwDispCount = 0;
	m_dwProcessCount = 0;

	// Keyboard initialization
	m_pKeyboard = NULL;
	m_lpDIKey = NULL;
	m_dwKeyAcquire = 0;
	m_bKeyEnable = TRUE;

	// Keyboard buffer clear
	for (i=0; i<0x100; i++) {
		m_KeyBuf[i] = FALSE;
		m_KeyMap[i] = 0;
	}

	// Mouse initialization
	m_pMouse = NULL;
	m_lpDIMouse = NULL;
	m_dwMouseAcquire = 0;
	m_bMouseMode = FALSE;
	m_nMouseX = 0;
	m_nMouseY = 0;
	m_bMouseB[0] = FALSE;
	m_bMouseB[1] = FALSE;
	m_dwMouseMid = 0;
	m_bMouseMid = TRUE;

	// Joystick initialization
	m_pPPI = NULL;
	m_dwJoyDevs = 0;
	m_bJoyEnable = TRUE;
	for (i=0; i<JoyDevices; i++) {
		// Device
		m_lpDIJoy[i] = NULL;
		m_lpDIDev2[i] = NULL;

		// Configuration
		memset(&m_JoyCfg[i], 0, sizeof(JOYCFG));
		// nDevice settings:
		// Device number assigned +1 (0 means not assigned)
		m_JoyCfg[i].nDevice = i + 1;
		for (nAxis=0; nAxis<JoyAxes; nAxis++) {
			if (nAxis < 9) {
				// dwAxis settings:
				// Low word: assigned port (0x00000 or 0x10000)
				// High word: axis+1 (1~4, 0 means not assigned)
				m_JoyCfg[i].dwAxis[nAxis] = (DWORD)((i << 16) | (nAxis + 1));
			}
			m_JoyCfg[i].bAxis[nAxis] = FALSE;
		}
		for (nButton=0; nButton<JoyButtons; nButton++) {
			if (nButton < 8) {
				// dwButton settings:
				// Low word: assigned port (0x00000 or 0x10000)
				// High word: button+1 (1~8, 0 means not assigned)
				m_JoyCfg[i].dwButton[nButton] = (DWORD)((i << 16) | (nButton + 1));
			}
			// Auto-repeat, initialize counter
			m_JoyCfg[i].dwRapid[nButton] = 0;
			m_JoyCfg[i].dwCount[nButton] = 0;
		}

		// Axis range
		memset(m_lJoyAxisMin[i], 0, sizeof(m_lJoyAxisMin[i]));
		memset(m_lJoyAxisMax[i], 0, sizeof(m_lJoyAxisMax[i]));

		// Acquire counter
		m_dwJoyAcquire[i] = 0;

		// State data
		memset(&m_JoyState[i], 0, sizeof(DIJOYSTATE));
	}

	// Key map initialization (mutual exclusion with Config Manager)
	SetDefaultKeyMap(m_KeyMap);
	xWnd = pWnd;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInput::Init()
{
	ASSERT(this);

	// Base class
	if (!CComponent::Init()) {
		return FALSE;
	}

	// Get CRTC
	ASSERT(!m_pCRTC);
	m_pCRTC = (CRTC*)::GetVM()->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	ASSERT(m_pCRTC);

	// Get keyboard
	ASSERT(!m_pKeyboard);
	m_pKeyboard = (Keyboard*)::GetVM()->SearchDevice(MAKEID('K', 'E', 'Y', 'B'));
	ASSERT(m_pKeyboard);

	// Get mouse
	ASSERT(!m_pMouse);
	m_pMouse = (Mouse*)::GetVM()->SearchDevice(MAKEID('M', 'O', 'U', 'S'));
	ASSERT(m_pMouse);

	// Get PPI
	ASSERT(!m_pPPI);
	m_pPPI = (PPI*)::GetVM()->SearchDevice(MAKEID('P', 'P', 'I', ' '));
	ASSERT(m_pPPI);

	// Create DirectInput object
	if (FAILED(DirectInputCreate(AfxGetApp()->m_hInstance, DIRECTINPUT_VERSION,
							&m_lpDI, NULL))) {
		return FALSE;
	}

	// Keyboard
	if (!InitKey()) {
		return FALSE;
	}

	// Mouse
	if (!InitMouse()) {
		return FALSE;
	}

	// Joystick
	EnumJoy();
	InitJoy();

	// Get current Joytypes
	Config config;
	xWnd->GetConfig()->GetConfig(&config);
	CString sz;
	for (int contx = 0; contx < 2; contx++)	{
		joyType[contx] = config.joy_type[contx];
	}
	/*
	sz.Format(_T("JOY[0]: %d   \r\n"), joyType[0]);
	OutputDebugStringW(CT2W(sz));
	sz.Format(_T("JOY[1]: %d   \r\n"), joyType[1]);
	OutputDebugStringW(CT2W(sz));
	*/

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CInput::Cleanup()
{
	int i;

	ASSERT(this);
	ASSERT_VALID(this);

	// Mouse mode off
	SetMouseMode(FALSE);

	// Release joystick devices
	for (i=0; i<JoyDevices; i++) {
		if (m_lpDIDev2[i]) {
			m_lpDIDev2[i]->Release();
			m_lpDIDev2[i] = NULL;
		}

		if (m_lpDIJoy[i]) {
			m_lpDIJoy[i]->Unacquire();
			m_lpDIJoy[i]->Release();
			m_lpDIJoy[i] = NULL;
		}
	}

	// Release mouse device
	if (m_lpDIMouse) {
		m_lpDIMouse->Unacquire();
		m_lpDIMouse->Release();
		m_lpDIMouse = NULL;
	}

	// Release keyboard device
	if (m_lpDIKey) {
		m_lpDIKey->Unacquire();
		m_lpDIKey->Release();
		m_lpDIKey = NULL;
	}

	// Release DirectInput object
	if (m_lpDI) {
		m_lpDI->Release();
		m_lpDI = NULL;
	}

	// Base class
	CComponent::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL CInput::ApplyCfg(const Config* pConfig)
{
	BOOL bFlag;
	int i;
	int nButton;
	int nConfig;

	ASSERT(this);
	ASSERT(pConfig);
	ASSERT_VALID(this);

	// Mouse middle button
	m_bMouseMid = pConfig->mouse_mid;

	// Reset middle button count
	m_dwMouseMid = 5;

	// Joystick device (device change)
	bFlag = FALSE;
	for (i=0; i<JoyDevices; i++) {
		// If the used device No. has changed, reinitialize is needed
		if (pConfig->joy_dev[i] != m_JoyCfg[i].nDevice) {
			m_JoyCfg[i].nDevice = pConfig->joy_dev[i];
			bFlag = TRUE;
		}
	}
	if (bFlag) {
		// Reinitialize
		InitJoy();
	}

	// Joystick device (button)
	for (i=0; i<JoyDevices; i++) {
		for (nButton=0; nButton<JoyButtons; nButton++) {
			// Get configuration data (bit16:port bit15-8:axis bit7-0:button)
			if (i == 0) {
				nConfig = pConfig->joy_button0[nButton];
			}
			else {
				nConfig = pConfig->joy_button1[nButton];
			}

			// Clear
			m_JoyCfg[i].dwButton[nButton] = 0;
			m_JoyCfg[i].dwRapid[nButton] = 0;
			m_JoyCfg[i].dwCount[nButton] = 0;

			// Check if assigned
			if ((nConfig & 0xff) == 0) {
				continue;
			}

			// Check if button number exceeds limit
			if ((nConfig & 0xff) > PPI::ButtonMax) {
				continue;
			}

			// Assign button and set
			m_JoyCfg[i].dwButton[nButton] = (DWORD)(nConfig & 0xff00ff);

			// Auto-repeat setting
			m_JoyCfg[i].dwRapid[nButton] = (DWORD)((nConfig >> 8) & 0xff);
			if (m_JoyCfg[i].dwRapid[nButton] > JoyRapids) {
				// In case of out of range, set no repeat
				m_JoyCfg[i].dwRapid[nButton] = 0;
			}
		}
	}
}

#if defined(_DEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void CInput::AssertValid() const
{
	ASSERT(this);
	ASSERT(GetID() == MAKEID('I', 'N', 'P', ' '));
	ASSERT(m_pCRTC);
	ASSERT(m_pCRTC->GetID() == MAKEID('C', 'R', 'T', 'C'));
	ASSERT(m_pKeyboard);
	ASSERT(m_pKeyboard->GetID() == MAKEID('K', 'E', 'Y', 'B'));
	ASSERT(m_pMouse);
	ASSERT(m_pMouse->GetID() == MAKEID('M', 'O', 'U', 'S'));
	ASSERT(m_pPPI);
	ASSERT(m_pPPI->GetID() == MAKEID('P', 'P', 'I', ' '));
}
#endif	// _DEBUG

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInput::Save(Fileio *pFio, int /*nVer*/)
{
	BOOL bResult;

	ASSERT(this);
	ASSERT(pFio);
	ASSERT_VALID(this);

	// Lock VM and temporarily stop processing
	::LockVM();

	// Save main
	bResult = SaveMain(pFio);

	// Unlock VM
	::UnlockVM();

	// Return result
	return bResult;
}

//---------------------------------------------------------------------------
//
//	Save main
//
//---------------------------------------------------------------------------
BOOL CInput::SaveMain(Fileio *pFio)
{
	ASSERT(this);
	ASSERT(pFio);
	ASSERT_VALID(this);

	//
	//	version2.00
	//

	// General
	if (!pFio->Write(&m_dwProcessCount, sizeof(m_dwProcessCount))) {
		return FALSE;
	}
	if (!pFio->Write(&m_dwDispCount, sizeof(m_dwDispCount))) {
		return FALSE;
	}

	// Keyboard
	if (!pFio->Write(m_KeyBuf, sizeof(m_KeyBuf))) {
		return FALSE;
	}

	// Mouse
	if (!pFio->Write(&m_nMouseX, sizeof(m_nMouseX))) {
		return FALSE;
	}
	if (!pFio->Write(&m_nMouseY, sizeof(m_nMouseY))) {
		return FALSE;
	}
	if (!pFio->Write(&m_dwMouseMid, sizeof(m_dwMouseMid))) {
		return FALSE;
	}

	// Joystick
	if (!pFio->Write(m_JoyState, sizeof(m_JoyState))) {
		return FALSE;
	}

	//
	//	version2.01
	//

	// Keyboard
	if (!pFio->Write(m_KeyMap, sizeof(m_KeyMap))) {
		return FALSE;
	}

	// Mouse
	if (!pFio->Write(m_bMouseB, sizeof(m_bMouseB))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInput::Load(Fileio *pFio, int nVer)
{
	ASSERT(this);
	ASSERT(pFio);
	ASSERT(nVer >= 0x0200);
	ASSERT_VALID(this);

	// Lock VM and temporarily stop processing
	::LockVM();

	// Load (version2.00)
	if (!Load200(pFio)) {
		::UnlockVM();
		return FALSE;
	}

	if (nVer >= 0x0201) {
		// version2.01
		if (!Load201(pFio)) {
			::UnlockVM();
			return FALSE;
		}
	}

	// Unlock VM
	::UnlockVM();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load main (version2.00)
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInput::Load200(Fileio *pFio)
{
	ASSERT(this);
	ASSERT(pFio);
	ASSERT_VALID(this);

	// General
	if (!pFio->Read(&m_dwProcessCount, sizeof(m_dwProcessCount))) {
		return FALSE;
	}
	if (!pFio->Read(&m_dwDispCount, sizeof(m_dwDispCount))) {
		return FALSE;
	}

	// Keyboard
	if (!pFio->Read(m_KeyBuf, sizeof(m_KeyBuf))) {
		return FALSE;
	}

	// Mouse
	if (!pFio->Read(&m_nMouseX, sizeof(m_nMouseX))) {
		return FALSE;
	}
	if (!pFio->Read(&m_nMouseY, sizeof(m_nMouseY))) {
		return FALSE;
	}
	if (!pFio->Read(&m_dwMouseMid, sizeof(m_dwMouseMid))) {
		return FALSE;
	}

	// Joystick
	if (!pFio->Read(m_JoyState, sizeof(m_JoyState))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load main (version2.01)
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInput::Load201(Fileio *pFio)
{
	ASSERT(this);
	ASSERT(pFio);
	ASSERT_VALID(this);

	// Keyboard
	if (!pFio->Read(m_KeyMap, sizeof(m_KeyMap))) {
		return FALSE;
	}

	// Mouse
	if (!pFio->Read(m_bMouseB, sizeof(m_bMouseB))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Processing
//
//---------------------------------------------------------------------------
void FASTCALL CInput::Process(BOOL bRun)
{
	DWORD dwDispCount;

	ASSERT(this);
	ASSERT_VALID(this);

	// Do nothing if disabled
	if (!m_bEnable) {
		return;
	}

	// If bRun = FALSE, emulate frame stop (called every 10ms)
	if (!bRun) {
		// Increment process counter
		m_dwProcessCount++;

		// Dummy input
		InputKey(FALSE);
		InputMouse(FALSE);
		InputJoy(FALSE);
		return;
	}

	// Get CRTC display count and process every frame
	ASSERT(m_pCRTC);
	dwDispCount = m_pCRTC->GetDispCount();
	if (dwDispCount == m_dwDispCount) {
		return;
	}
	m_dwDispCount = dwDispCount;

	// Increment process counter
	m_dwProcessCount++;

	// If not active, or in menu mode, dummy input
	if (!m_bActive || m_bMenu) {
		// Dummy input
		InputKey(FALSE);
		InputMouse(FALSE);
		InputJoy(FALSE);
		MakeJoy(FALSE);
		return;
	}

	// OK, process input
	InputKey(TRUE);
	InputMouse(TRUE);
	InputJoy(TRUE);
	MakeJoy(m_bJoyEnable);
}

//---------------------------------------------------------------------------
//
//	Active notification
//
//---------------------------------------------------------------------------
void FASTCALL CInput::Activate(BOOL bActive)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Set active flag
	m_bActive = bActive;
}

//---------------------------------------------------------------------------
//
//	Menu notification
//
//---------------------------------------------------------------------------
void FASTCALL CInput::Menu(BOOL bMenu)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Set menu flag
	m_bMenu = bMenu;
}

//---------------------------------------------------------------------------
//
//	Get acquire count
//
//---------------------------------------------------------------------------
DWORD FASTCALL CInput::GetAcquireCount(int nType) const
{
	ASSERT(this);
	ASSERT(JoyDevices >= 2);
	ASSERT_VALID(this);

	switch (nType) {
		// 0: Keyboard
		case 0:
			return m_dwKeyAcquire;

		// 1: Mouse
		case 1:
			return m_dwMouseAcquire;

		// 2: Joystick A
		case 2:
			return m_dwJoyAcquire[0];

		// 3: Joystick B
		case 3:
			return m_dwJoyAcquire[1];

		// Others
		default:
			break;
	}

	// Normally, should not happen
	ASSERT(FALSE);
	return 0;
}

//===========================================================================
//
//	Keyboard
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Keyboard initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInput::InitKey()
{
	ASSERT(this);
	ASSERT(m_lpDI);
	ASSERT(!m_lpDIKey);
	ASSERT_VALID(this);

	// Create keyboard device
	if (FAILED(m_lpDI->CreateDevice(GUID_SysKeyboard, &m_lpDIKey, NULL))) {
		return FALSE;
	}

	// Set keyboard data format
	if (FAILED(m_lpDIKey->SetDataFormat(&c_dfDIKeyboard))) {
		return FALSE;
	}

	// Set cooperative level
	if (FAILED(m_lpDIKey->SetCooperativeLevel(m_pFrmWnd->m_hWnd,
						DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Keyboard input
//
//---------------------------------------------------------------------------
void FASTCALL CInput::InputKey(BOOL bEnable)
{
	HRESULT hr;
	BYTE buf[0x100];
	DWORD dwCode;
	int nKey;

	ASSERT(this);
	ASSERT(m_lpDIKey);
	ASSERT_VALID(this);

	// Self check
	if (!m_lpDIKey) {
		return;
	}

	// If OK, get device state, if not, turn all OFF
	if (bEnable || !m_bKeyEnable) {
		hr = m_lpDIKey->GetDeviceState(sizeof(buf), buf);
		if (hr != DI_OK) {
			// If error, try to acquire again
			m_lpDIKey->Acquire();
			m_dwKeyAcquire++;
			return;
		}
	}
	else {
		// Current keyboard input is ignored
		memset(buf, 0, sizeof(buf));
	}

	// In Win2000,WinXP, DIK_KANJI,DIK_KANA are blocked, so turn ON
	if (::IsWinNT()) {
		buf[DIK_KANJI] = 0;
		buf[DIK_KANA] = 0;
	}

	// If key enable is OFF, Win32 handles it
	if (!m_bKeyEnable) {
		// Record held keys in buffer
		for (nKey=0; nKey<0x100; nKey++) {
			if (buf[nKey] & 0x80) {
				m_KeyBuf[nKey] = TRUE;
			}
			else {
				m_KeyBuf[nKey] = FALSE;
			}
		}
		return;
	}

	// If ALT+key is not mapped, ignore
	if (buf[DIK_LMENU] & 0x80) {
		if (m_KeyMap[DIK_LMENU] == 0) {
			// Not mapped, so ignore
			return;
		}
	}
	if (buf[DIK_RMENU] & 0x80) {
		if (m_KeyMap[DIK_RMENU] == 0) {
			// Not mapped, so ignore
			return;
		}
	}

	// Merge shift keys
	if (buf[DIK_LSHIFT] | buf[DIK_RSHIFT] & 0x80) {
		buf[DIK_LSHIFT] |= 0x80;
		buf[DIK_RSHIFT] |= 0x80;
	}

	// Loop
	for (nKey=0; nKey<0x100; nKey++) {
		if (buf[nKey] & 0x80) {
			// Key is pressed, buffer is OFF
			if (m_KeyBuf[nKey] == FALSE) {
				// State transition (pressed)
				m_KeyBuf[nKey] = TRUE;
				// Check key translation map, and output valid codes
				dwCode = m_KeyMap[nKey];
				if (dwCode != 0) {
					m_pKeyboard->MakeKey(dwCode);
				}
			}
		}
		else {
			// Key is released, buffer is ON
			if (m_KeyBuf[nKey] == TRUE) {
				// State transition (released)
				m_KeyBuf[nKey] = FALSE;
				// Check key translation map, and output valid codes
				dwCode = m_KeyMap[nKey];
				if (dwCode != 0) {
					m_pKeyboard->BreakKey(dwCode);
				}
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	Set default key map
//
//---------------------------------------------------------------------------
void FASTCALL CInput::SetDefaultKeyMap(DWORD *pKeyMap)
{
	const DWORD *pTable;
	DWORD dwDX;
	DWORD dw68;

	// ASSERT_VALID is not done (called from constructor)
	ASSERT(this);
	ASSERT(pKeyMap);

	// Get table (Japanese 106 only)
	pTable = m_KeyMap106;

	// Clear all
	memset(pKeyMap, 0, sizeof(DWORD) * 0x100);

	// Loop (from X68k code ADIK code)
	dw68 = 0x0001;
	for (;;) {
		// Get DIK code
		dwDX = *pTable++;

		// End with 0xff
		if (dwDX == 0xff) {
			break;
		}

		// If not 0, assign this key to X68k code
		if (dwDX != 0) {
			pKeyMap[dwDX] = dw68;
			// If DIK is LSHIFT, also RSHIFT
			if (dwDX == DIK_LSHIFT) {
				pKeyMap[DIK_RSHIFT] = dw68;
			}
		}

		// Next
		dw68++;
	}
}

//---------------------------------------------------------------------------
//
//	Default map (for 106 keyboard)
//
//---------------------------------------------------------------------------
const DWORD CInput::m_KeyMap106[] = {
	DIK_ESCAPE,							// 01 [ESC]
	DIK_1,								// 02 [1]
	DIK_2,								// 03 [2]
	DIK_3,								// 04 [3]
	DIK_4,								// 05 [4]
	DIK_5,								// 06 [5]
	DIK_6,								// 07 [6]
	DIK_7,								// 08 [7]
	DIK_8,								// 09 [8]
	DIK_9,								// 0A [9]
	DIK_0,								// 0B [0]
	DIK_MINUS,							// 0C [-]
	DIK_CIRCUMFLEX,						// 0D [^]
	DIK_YEN,							// 0E [\]
	DIK_BACK,							// 0F [BS]

	DIK_TAB,							// 10 [TAB]
	DIK_Q,								// 11 [Q]
	DIK_W,								// 12 [W]
	DIK_E,								// 13 [E]
	DIK_R,								// 14 [R]
	DIK_T,								// 15 [T]
	DIK_Y,								// 16 [Y]
	DIK_U,								// 17 [U]
	DIK_I,								// 18 [I]
	DIK_O,								// 19 [O]
	DIK_P,								// 1A [P]
	DIK_AT,								// 1B [@]
	DIK_LBRACKET,						// 1C [[]
	DIK_RETURN,							// 1D [CR]

	DIK_A,								// 1E [A]
	DIK_S,								// 1F [S]
	DIK_D,								// 20 [D]
	DIK_F,								// 21 [F]
	DIK_G,								// 22 [G]
	DIK_H,								// 23 [H]
	DIK_J,								// 24 [J]
	DIK_K,								// 25 [K]
	DIK_L,								// 26 [L]
	DIK_SEMICOLON,						// 27 [;]
	DIK_COLON,							// 28 [:]
	DIK_RBRACKET,						// 29 []]

	DIK_Z,								// 2A [Z]
	DIK_X,								// 2B [X]
	DIK_C,								// 2C [C]
	DIK_V,								// 2D [V]
	DIK_B,								// 2E [B]
	DIK_N,								// 2F [N]
	DIK_M,								// 30 [M]
	DIK_COMMA,							// 31 [,]
	DIK_PERIOD,							// 32 [.]
	DIK_SLASH,							// 33 [/]
	DIK_BACKSLASH,						// 34 [_]

	DIK_SPACE,							// 35 [SPACE]

	DIK_HOME,							// 36 [HOME]
	DIK_DELETE,							// 37 [DEL]
	DIK_NEXT,							// 38 [ROLL UP]
	DIK_PRIOR,							// 39 [ROLL DOWN]
	DIK_END,							// 3A [UNDO]

	DIK_LEFT,							// 3B [LEFT]
	DIK_UP,								// 3C [UP]
	DIK_RIGHT,							// 3D [RIGHT]
	DIK_DOWN,							// 3E [DOWN]

	DIK_NUMLOCK,						// 3F [Tenkey CLR]
	DIK_DIVIDE,							// 40 [Tenkey /]
	DIK_MULTIPLY,						// 41 [Tenkey *]
	DIK_SUBTRACT,						// 42 [Tenkey -]
	DIK_NUMPAD7,						// 43 [Tenkey 7]
	DIK_NUMPAD8,						// 44 [Tenkey 8]
	DIK_NUMPAD9,						// 45 [Tenkey 9]
	DIK_ADD,							// 46 [Tenkey +]
	DIK_NUMPAD4,						// 47 [Tenkey 4]
	DIK_NUMPAD5,						// 48 [Tenkey 5]
	DIK_NUMPAD6,						// 49 [Tenkey 6]
	0,									// 4A [Tenkey =]
	DIK_NUMPAD1,						// 4B [Tenkey 1]
	DIK_NUMPAD2,						// 4C [Tenkey 2]
	DIK_NUMPAD3,						// 4D [Tenkey 3]
	DIK_NUMPADENTER,					// 4E [Tenkey CR]
	DIK_NUMPAD0,						// 4F [Tenkey 0]
	0,									// 50 [Tenkey ,]
	DIK_DECIMAL,						// 51 [Tenkey .]

	0,									// 52 [henkan]
	0,									// 53 [hiragana]
	0,									// 54 [HELP]

	0,									// 55 [XF1]
	0,									// 56 [XF2]
	0,									// 57 [XF3]
	0,									// 58 [XF4]
	0,									// 59 [XF5]

	0,									// 5A [NFER]
	0,									// 5B [XFER]
	0,									// 5C [COPY]
	DIK_CAPITAL,						// 5D [CAPS]

	DIK_INSERT,							// 5E [INS]
	0,									// 5F [STOP]
	0,									// 60 [ESC]

	DIK_PAUSE,							// 61 [BREAK]
	0,									// 62 [COPY]
	DIK_F1,								// 63 [F1]
	DIK_F2,								// 64 [F2]
	DIK_F3,								// 65 [F3]
	DIK_F4,								// 66 [F4]
	DIK_F5,								// 67 [F5]
	DIK_F6,								// 68 [F6]
	DIK_F7,								// 69 [F7]
	DIK_F8,								// 6A [F8]
	DIK_F9,								// 6B [F9]
	DIK_F11,							// 6C [F10]
	0,									// 6D (Reserved)
	0,									// 6E (Reserved)
	0,									// 6F (Reserved)
	DIK_LSHIFT,							// 70 [SHIFT]
	DIK_LCONTROL,						// 71 [CTRL]
	0,									// 72 [OPT1]
	0,									// 73 [OPT2]
	0xff
};

//---------------------------------------------------------------------------
//
//	Get key input buffer
//
//---------------------------------------------------------------------------
void FASTCALL CInput::GetKeyBuf(BOOL *pKeyBuf) const
{
	ASSERT(this);
	ASSERT(pKeyBuf);
	ASSERT_VALID(this);

	// Copy current buffer
	memcpy(pKeyBuf, m_KeyBuf, sizeof(m_KeyBuf));
}

//---------------------------------------------------------------------------
//
//	Keyboard enable/disable
//
//---------------------------------------------------------------------------
void FASTCALL CInput::EnableKey(BOOL bEnable)
{
	DWORD dwCode;
	int nKey;

	ASSERT(this);
	ASSERT_VALID(this);

	// Do nothing if same
	if (m_bKeyEnable == bEnable) {
		return;
	}

	// Change
	m_bKeyEnable = bEnable;

	// If TRUE, do nothing
	if (m_bKeyEnable) {
		return;
	}

	// If FALSE, break all currently held keys
	for (nKey=0; nKey<0x100; nKey++) {
		// Key is pressed, buffer is ON
		if (m_KeyBuf[nKey] == TRUE) {
			// Check key translation map, and output valid codes
			dwCode = m_KeyMap[nKey];
			if (dwCode != 0) {
				m_pKeyboard->BreakKey(dwCode);
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	Convert X68000 key code to DirectX key code
//
//---------------------------------------------------------------------------
int FASTCALL CInput::Key2DirectX(int nKey)
{
	int nIndex;

	ASSERT(this);
	ASSERT((nKey >= 0) && (nKey <= 0x73));

	for (nIndex=0; nIndex<0x100; nIndex++) {
		// OK if registered in key map
		if (m_KeyMap[nIndex] == (UINT)nKey) {
			return nIndex;
		}
	}

	// Not found
	return 0;
}

//---------------------------------------------------------------------------
//
//	Convert DirectX key code to X68000 key code
//
//---------------------------------------------------------------------------
int FASTCALL CInput::Key2X68k(int nDXKey)
{
	ASSERT(this);
	ASSERT((nDXKey >= 0) && (nDXKey < 0x100));
	ASSERT_VALID(this);

	// Return as-is from key map. 0 if not assigned
	return m_KeyMap[nDXKey];
}

//---------------------------------------------------------------------------
//
//	Get key name
//
//---------------------------------------------------------------------------
LPCTSTR FASTCALL CInput::GetKeyName(int nKey)
{
	ASSERT((nKey >= 0) && (nKey <= 0x73));

	return KeyNameTable[nKey];
}

//---------------------------------------------------------------------------
//
//	Key name table
//
//---------------------------------------------------------------------------
LPCTSTR CInput::KeyNameTable[] = {
	NULL,
	_T("ESC"),
	_T("1!\x22"),
	_T("3#\x23"),
	_T("4$\x24"),
	_T("5%\x25"),
	_T("6&\x26"),
	_T("7'\x27"),
	_T("8(\x28"),
	_T("9)\x29"),
	_T("0 \xa0"),
	_T("-=\x3d"),
	_T("^~\x5e"),
	_T("\\|\x5c"),
	_T("BS"),

	_T("TAB"),
	_T("Q\xa0"),
	_T("W\xa0"),
	_T("E\xa0"),
	_T("R\xa0"),
	_T("T\xa0"),
	_T("Y\xa0"),
	_T("U\xa0"),
	_T("I\xa0"),
	_T("O\xa0"),
	_T("P\xa0"),
	_T("@`\x40"),
	_T("[{\x5b"),
	_T("RETURN"),

	_T("A\xa0"),
	_T("S\xa0"),
	_T("D\xa0"),
	_T("F\xa0"),
	_T("G\xa0"),
	_T("H\xa0"),
	_T("J\xa0"),
	_T("K\xa0"),
	_T("L\xa0"),
	_T(";+\x3b"),
	_T(":*\x3a"),
	_T("]}\x5d"),

	_T("Z\xa0"),
	_T("X\xa0"),
	_T("C\xa0"),
	_T("V\xa0"),
	_T("B\xa0"),
	_T("N\xa0"),
	_T("M\xa0"),
	_T(",<\x2c"),
	_T(".>\x2e"),
	_T("/?\x2f"),
	_T("_\xa0"),
	_T("SPACE"),

	_T("HOME"),
	_T("DEL"),
	_T("ROLL UP"),
	_T("ROLL DOWN"),
	_T("UNDO"),
	_T("hen"),
	_T("hir"),
	_T("kan"),
	_T("zen"),

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
	_T("ENTER"),
	_T("0"),
	_T(","),
	_T("."),

	_T("henkan"),
	_T("hiragana"),
	_T("HELP"),

	_T("XF1"),
	_T("XF2"),
	_T("XF3"),
	_T("XF4"),
	_T("XF5"),

	_T("NFER"),
	_T("XFER"),
	_T("COPY"),
	_T("CAPS"),
	_T("INS"),
	_T("STOP"),
	_T("ESC"),

	_T("BREAK"),
	_T("COPY"),
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
	_T("OPT.2")
};

//---------------------------------------------------------------------------
//
//	Get key ID
//
//---------------------------------------------------------------------------
LPCSTR FASTCALL CInput::GetKeyID(int nID)
{
	ASSERT((nID >= 0) && (nID < 0x100));

	return KeyIDTable[nID];
}

//---------------------------------------------------------------------------
//
//	DirectX Key ID table
//
//---------------------------------------------------------------------------
LPCSTR CInput::KeyIDTable[] = {
	NULL,							// 0x00
	_T("DIK_ESCAPE"),				// 0x01
	_T("DIK_1"),					// 0x02
	_T("DIK_2"),					// 0x03
	_T("DIK_3"),					// 0x04
	_T("DIK_4"),					// 0x05
	_T("DIK_5"),					// 0x06
	_T("DIK_6"),					// 0x07
	_T("DIK_7"),					// 0x08
	_T("DIK_8"),					// 0x09
	_T("DIK_9"),					// 0x0A
	_T("DIK_0"),					// 0x0B
	_T("DIK_MINUS"),				// 0x0C
	_T("DIK_EQUALS"),				// 0x0D
	_T("DIK_BACK"),					// 0x0E
	_T("DIK_TAB"),					// 0x0F
	_T("DIK_Q"),					// 0x10
	_T("DIK_W"),					// 0x11
	_T("DIK_E"),					// 0x12
	_T("DIK_R"),					// 0x13
	_T("DIK_T"),					// 0x14
	_T("DIK_Y"),					// 0x15
	_T("DIK_U"),					// 0x16
	_T("DIK_I"),					// 0x17
	_T("DIK_O"),					// 0x18
	_T("DIK_P"),					// 0x19
	_T("DIK_LBRACKET"),				// 0x1A
	_T("DIK_RBRACKET"),				// 0x1B
	_T("DIK_RETURN"),				// 0x1C
	_T("DIK_LCONTROL"),				// 0x1D
	_T("DIK_A"),					// 0x1E
	_T("DIK_S"),					// 0x1F
	_T("DIK_D"),					// 0x20
	_T("DIK_F"),					// 0x21
	_T("DIK_G"),					// 0x22
	_T("DIK_H"),					// 0x23
	_T("DIK_J"),					// 0x24
	_T("DIK_K"),					// 0x25
	_T("DIK_L"),					// 0x26
	_T("DIK_SEMICOLON"),			// 0x27
	_T("DIK_APOSTROPHE"),			// 0x28
	_T("DIK_GRAVE"),				// 0x29
	_T("DIK_LSHIFT"),				// 0x2A
	_T("DIK_BACKSLASH"),			// 0x2B
	_T("DIK_Z"),					// 0x2C
	_T("DIK_X"),					// 0x2D
	_T("DIK_C"),					// 0x2E
	_T("DIK_V"),					// 0x2F
	_T("DIK_B"),					// 0x30
	_T("DIK_N"),					// 0x31
	_T("DIK_M"),					// 0x32
	_T("DIK_COMMA"),				// 0x33
	_T("DIK_PERIOD"),				// 0x34
	_T("DIK_SLASH"),				// 0x35
	_T("DIK_RSHIFT"),				// 0x36
	_T("DIK_MULTIPLY"),				// 0x37
	_T("DIK_LMENU"),				// 0x38
	_T("DIK_SPACE"),				// 0x39
	_T("DIK_CAPITAL"),				// 0x3A
	_T("DIK_F1"),					// 0x3B
	_T("DIK_F2"),					// 0x3C
	_T("DIK_F3"),					// 0x3D
	_T("DIK_F4"),					// 0x3E
	_T("DIK_F5"),					// 0x3F
	_T("DIK_F6"),					// 0x40
	_T("DIK_F7"),					// 0x41
	_T("DIK_F8"),					// 0x42
	_T("DIK_F9"),					// 0x43
	_T("DIK_F10"),					// 0x44
	_T("DIK_NUMLOCK"),				// 0x45
	_T("DIK_SCROLL"),				// 0x46
	_T("DIK_NUMPAD7"),				// 0x47
	_T("DIK_NUMPAD8"),				// 0x48
	_T("DIK_NUMPAD9"),				// 0x49
	_T("DIK_SUBTRACT"),				// 0x4A
	_T("DIK_NUMPAD4"),				// 0x4B
	_T("DIK_NUMPAD5"),				// 0x4C
	_T("DIK_NUMPAD6"),				// 0x4D
	_T("DIK_ADD"),					// 0x4E
	_T("DIK_NUMPAD1"),				// 0x4F
	_T("DIK_NUMPAD2"),				// 0x50
	_T("DIK_NUMPAD3"),				// 0x51
	_T("DIK_NUMPAD0"),				// 0x52
	_T("DIK_DECIMAL"),				// 0x53
	NULL,							// 0x54
	NULL,							// 0x55
	_T("DIK_OEM_102"),				// 0x56
	_T("DIK_F11"),					// 0x57
	_T("DIK_F12"),					// 0x58
	NULL,							// 0x59
	NULL,							// 0x5A
	NULL,							// 0x5B
	NULL,							// 0x5C
	NULL,							// 0x5D
	NULL,							// 0x5E
	NULL,							// 0x5F
	NULL,							// 0x60
	NULL,							// 0x61
	NULL,							// 0x62
	NULL,							// 0x63
	_T("DIK_F13"),					// 0x64
	_T("DIK_F14"),					// 0x65
	_T("DIK_F15"),					// 0x66
	NULL,							// 0x67
	NULL,							// 0x68
	NULL,							// 0x69
	NULL,							// 0x6A
	NULL,							// 0x6B
	NULL,							// 0x6C
	NULL,							// 0x6D
	NULL,							// 0x6E
	NULL,							// 0x6F
	_T("DIK_KANA"),					// 0x70
	NULL,							// 0x71
	NULL,							// 0x72
	_T("DIK_ABNT_C1"),				// 0x73
	NULL,							// 0x74
	NULL,							// 0x75
	NULL,							// 0x76
	NULL,							// 0x77
	NULL,							// 0x78
	_T("DIK_CONVERT"),				// 0x79
	NULL,							// 0x7A
	_T("DIK_NOCONVERT"),			// 0x7B
	NULL,							// 0x7C
	_T("DIK_YEN"),					// 0x7D
	_T("DIK_ABNT_C2"),				// 0x7E
	NULL,							// 0x7F
	NULL,							// 0x80
	NULL,							// 0x81
	NULL,							// 0x82
	NULL,							// 0x83
	NULL,							// 0x84
	NULL,							// 0x85
	NULL,							// 0x86
	NULL,							// 0x87
	NULL,							// 0x88
	NULL,							// 0x89
	NULL,							// 0x8A
	NULL,							// 0x8B
	NULL,							// 0x8C
	_T("DIK_NUMPADEQUALS"),			// 0x8D
	NULL,							// 0x8E
	NULL,							// 0x8F
	_T("DIK_PREVTRACK"),			// 0x90
	_T("DIK_AT"),					// 0x91
	_T("DIK_COLON"),				// 0x92
	_T("DIK_UNDERLINE"),			// 0x93
	_T("DIK_KANJI"),				// 0x94
	_T("DIK_STOP"),					// 0x95
	_T("DIK_AX"),					// 0x96
	_T("DIK_UNLABELED"),			// 0x97
	NULL,							// 0x98
	_T("DIK_NEXTTRACK"),			// 0x99
	NULL,							// 0x9A
	NULL,							// 0x9B
	_T("DIK_NUMPADENTER"),			// 0x9C
	_T("DIK_RCONTROL"),				// 0x9D
	NULL,							// 0x9E
	NULL,							// 0x9F
	_T("DIK_MUTE"),					// 0xA0
	_T("DIK_CALCULATOR"),			// 0xA1
	_T("DIK_PLAYPAUSE"),			// 0xA2
	NULL,							// 0xA3
	_T("DIK_MEDIASTOP"),			// 0xA4
	NULL,							// 0xA5
	NULL,							// 0xA6
	NULL,							// 0xA7
	NULL,							// 0xA8
	NULL,							// 0xA9
	NULL,							// 0xAA
	NULL,							// 0xAB
	NULL,							// 0xAC
	NULL,							// 0xAD
	_T("DIK_VOLUMEDOWN"),			// 0xAE
	NULL,							// 0xAF
	_T("DIK_VOLUMEUP"),				// 0xB0
	NULL,							// 0xB1
	_T("DIK_WEBHOME"),				// 0xB2
	_T("DIK_NUMPADCOMMA"),			// 0xB3
	NULL,							// 0xB4
	_T("DIK_DIVIDE"),				// 0xB5
	NULL,							// 0xB6
	_T("DIK_SYSRQ"),				// 0xB7
	_T("DIK_RMENU"),				// 0xB8
	NULL,							// 0xB9
	NULL,							// 0xBA
	NULL,							// 0xBB
	NULL,							// 0xBC
	NULL,							// 0xBD
	NULL,							// 0xBE
	NULL,							// 0xBF
	NULL,							// 0xC0
	NULL,							// 0xC1
	NULL,							// 0xC2
	NULL,							// 0xC3
	NULL,							// 0xC4
	_T("DIK_PAUSE"),				// 0xC5
	NULL,							// 0xC6
	_T("DIK_HOME"),					// 0xC7
	_T("DIK_UP"),					// 0xC8
	_T("DIK_PRIOR"),				// 0xC9
	NULL,							// 0xCA
	_T("DIK_LEFT"),					// 0xCB
	NULL,							// 0xCC
	_T("DIK_RIGHT"),				// 0xCD
	NULL,							// 0xCE
	_T("DIK_END"),					// 0xCF
	_T("DIK_DOWN"),					// 0xD0
	_T("DIK_NEXT"),					// 0xD1
	_T("DIK_INSERT"),				// 0xD2
	_T("DIK_DELETE"),				// 0xD3
	NULL,							// 0xD4
	NULL,							// 0xD5
	NULL,							// 0xD6
	NULL,							// 0xD7
	NULL,							// 0xD8
	NULL,							// 0xD9
	NULL,							// 0xDA
	_T("DIK_LWIN"),					// 0xDB
	_T("DIK_RWIN"),					// 0xDC
	_T("DIK_APPS"),					// 0xDD
	_T("DIK_POWER"),				// 0xDE
	_T("DIK_SLEEP"),				// 0xDF
	NULL,							// 0xE0
	NULL,							// 0xE1
	NULL,							// 0xE2
	_T("DIK_WAKE"),					// 0xE3
	NULL,							// 0xE4
	_T("DIK_WEBSEARCH"),			// 0xE5
	_T("DIK_WEBFAVORITES"),			// 0xE6
	_T("DIK_WEBREFRESH"),			// 0xE7
	_T("DIK_WEBSTOP"),				// 0xE8
	_T("DIK_WEBFORWARD"),			// 0xE9
	_T("DIK_WEBBACK"),				// 0xEA
	_T("DIK_MYCOMPUTER"),			// 0xEB
	_T("DIK_MAIL"),					// 0xEC
	_T("DIK_MEDIASELECT"),			// 0xED
	NULL,							// 0xEE
	NULL,							// 0xEF
	NULL,							// 0xF0
	NULL,							// 0xF1
	NULL,							// 0xF2
	NULL,							// 0xF3
	NULL,							// 0xF4
	NULL,							// 0xF5
	NULL,							// 0xF6
	NULL,							// 0xF7
	NULL,							// 0xF8
	NULL,							// 0xF9
	NULL,							// 0xFA
	NULL,							// 0xFB
	NULL,							// 0xFC
	NULL,							// 0xFD
	NULL,							// 0xFE
	NULL							// 0xFF
};

//---------------------------------------------------------------------------
//
//	Get key map
//
//---------------------------------------------------------------------------
void FASTCALL CInput::GetKeyMap(DWORD *pKeyMap)
{
	ASSERT(this);
	ASSERT(pKeyMap);
	ASSERT_VALID(this);

	// Copy current key map
	memcpy(pKeyMap, m_KeyMap, sizeof(m_KeyMap));
}

//---------------------------------------------------------------------------
//
//	Set key map
//
//---------------------------------------------------------------------------
void FASTCALL CInput::SetKeyMap(const DWORD *pKeyMap)
{
	// ASSERT_VALID is not done (called from Config)
	ASSERT(this);
	ASSERT(pKeyMap);

	// Set key map
	memcpy(m_KeyMap, pKeyMap, sizeof(m_KeyMap));
}

//---------------------------------------------------------------------------
//
//	Check if key is assigned
//	Called from config command handler, so all keys assigned in XM6 are checked
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInput::IsKeyMapped(int nID) const
{
	ASSERT(this);
	ASSERT((nID >= 0) && (nID < 0x100));
	ASSERT_VALID(this);

	// If assigned code is 0, not used (not mapped)
	if (m_KeyMap[nID] == 0) {
		return FALSE;
	}

	// Is mapped
	return TRUE;
}

//===========================================================================
//
//	Mouse
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Mouse initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInput::InitMouse()
{
	ASSERT(this);
	ASSERT(m_lpDI);
	ASSERT(!m_lpDIMouse);
	ASSERT_VALID(this);

	// Create mouse device
	if (FAILED(m_lpDI->CreateDevice(GUID_SysMouse, &m_lpDIMouse, NULL))) {
		return FALSE;
	}

	// Set mouse data format
	if (FAILED(m_lpDIMouse->SetDataFormat(&c_dfDIMouse))) {
		return FALSE;
	}

	// Set cooperative level (different for Win9x/WinNT, so separate)
	if (::IsWinNT()) {
		// WindowsNT
		if (FAILED(m_lpDIMouse->SetCooperativeLevel(m_pFrmWnd->m_hWnd,
						DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))) {
			return FALSE;
		}
	}
	else {
		// Windows9x
		if (FAILED(m_lpDIMouse->SetCooperativeLevel(m_pFrmWnd->m_hWnd,
						DISCL_FOREGROUND | DISCL_EXCLUSIVE))) {
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Mouse input
//
//---------------------------------------------------------------------------
void FASTCALL CInput::InputMouse(BOOL bEnable)
{
	HRESULT hr;
	DIMOUSESTATE dims;

	ASSERT(this);
	ASSERT(m_pFrmWnd);
	ASSERT(m_lpDIMouse);
	ASSERT(m_pMouse);
	ASSERT_VALID(this);

	// Not needed
	if (!bEnable) {
		// Turn mouse mode OFF
		if (m_bMouseMode) {
			m_pFrmWnd->PostMessage(WM_COMMAND, IDM_MOUSEMODE, 0);
		}

		// Notify button UP
		m_pMouse->SetMouse(m_nMouseX, m_nMouseY, FALSE, FALSE);
		return;
	}

	// If mouse mode ON
	if (!m_bMouseMode) {
		// Notify button UP
		m_pMouse->SetMouse(m_nMouseX, m_nMouseY, FALSE, FALSE);
		return;
	}

	// Get device state
	hr = m_lpDIMouse->GetDeviceState(sizeof(dims), &dims);
	if (hr != DI_OK) {
		// Acquire is required
		m_lpDIMouse->Acquire();
		m_dwMouseAcquire++;

		// Reset mouse
		m_pMouse->ResetMouse();
		return;
	}

	// Add delta data
	m_nMouseX += dims.lX;
	m_nMouseY += dims.lY;
	if (dims.rgbButtons[0] & 0x80) {
		m_bMouseB[0] = TRUE;
	}
	else {
		m_bMouseB[0] = FALSE;
	}
	if (dims.rgbButtons[1] & 0x80) {
		m_bMouseB[1] = TRUE;
	}
	else {
		m_bMouseB[1] = FALSE;
	}

	// Notify mouse device
	m_pMouse->SetMouse(m_nMouseX, m_nMouseY, m_bMouseB[0], m_bMouseB[1]);

	// If middle button function is disabled, exit
	if (!m_bMouseMid) {
		m_dwMouseMid = 5;
		return;
	}

	// Check middle button. After pressing and releasing, turn mouse mode off
	if (dims.rgbButtons[2] & 0x80) {
		// Pressed
		if (m_dwMouseMid < 4) {
			// Return to normal state
			m_dwMouseMid++;
			if (m_dwMouseMid == 4) {
				// State is established, hold
				m_dwMouseMid = 3;
			}
		}
	}
	else {
		// Released
		if ((m_dwMouseMid == 3) || (m_dwMouseMid == 4)) {
			// State was pressed, wait for next press after release
			m_dwMouseMid++;
			if (m_dwMouseMid == 5) {
				// More than 3 frames after press, now more than 2 frames after release
				m_pFrmWnd->PostMessage(WM_COMMAND, IDM_MOUSEMODE, 0);
				m_dwMouseMid++;
			}
		}
		else {
			// Was not pressed, not held. Reset
			m_dwMouseMid = 0;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Set mouse mode
//
//---------------------------------------------------------------------------
void FASTCALL CInput::SetMouseMode(BOOL bMode)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// If same as current mode
	if (m_bMouseMode != bMode) {
		// Set mode
		m_bMouseMode = bMode;

		// Unacquire regardless
		if (m_lpDIMouse) {
			m_lpDIMouse->Unacquire();
		}

		// Disable middle button count
		m_dwMouseMid = 5;
	}
}

//---------------------------------------------------------------------------
//
//	Get mouse info
//
//---------------------------------------------------------------------------
void FASTCALL CInput::GetMouseInfo(int *pPos, BOOL *pBtn) const
{
	ASSERT(this);
	ASSERT(pPos);
	ASSERT(pBtn);
	ASSERT_VALID(this);

	// Each 3 elements
	pPos[0] = m_nMouseX;
	pPos[1] = m_nMouseY;
	pPos[2] = (int)m_dwMouseMid;

	// Button
	pBtn[0] = m_bMouseB[0];
	pBtn[1] = m_bMouseB[1];
	pBtn[2] = m_bMouseMid;
}

//===========================================================================
//
//	Joystick
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Enumerate joysticks
//
//---------------------------------------------------------------------------
void FASTCALL CInput::EnumJoy()
{
	ASSERT(this);
	ASSERT(m_lpDI);

	// Clear joystick list
	m_dwJoyDevs = 0;

	// Start enumeration
	m_lpDI->EnumDevices(DIDEVTYPE_JOYSTICK, (LPDIENUMDEVICESCALLBACK)EnumCb,
							this, DIEDFL_ATTACHEDONLY);
}

//---------------------------------------------------------------------------
//
//	Joystick callback
//
//---------------------------------------------------------------------------
BOOL CALLBACK CInput::EnumCb(LPDIDEVICEINSTANCE pDevInst, LPVOID pvRef)
{
	CInput *pInput;

	ASSERT(pDevInst);
	ASSERT(pvRef);

	// Convert to CInput
	pInput = (CInput*)pvRef;

	// Call back
	return pInput->EnumDev(pDevInst);
}

//---------------------------------------------------------------------------
//
//	Add joystick
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInput::EnumDev(LPDIDEVICEINSTANCE pDevInst)
{
	LPDIRECTINPUTDEVICE pInputDev;

	ASSERT(this);
	ASSERT(pDevInst);
	ASSERT(m_lpDI);

	// Maximum check. Support only up to JoyDeviceMax devices
	if (m_dwJoyDevs >= JoyDeviceMax) {
		ASSERT(m_dwJoyDevs == JoyDeviceMax);
		return DIENUM_STOP;
	}

	// Store instance
	memcpy(&m_JoyDevInst[m_dwJoyDevs], pDevInst, sizeof(DIDEVICEINSTANCE));

	// Create device
	pInputDev = NULL;
	if (FAILED(m_lpDI->CreateDevice(pDevInst->guidInstance,
									&pInputDev,
									NULL))) {
		return DIENUM_CONTINUE;
	}
	ASSERT(pInputDev);

	// Set data format
	if (FAILED(pInputDev->SetDataFormat(&c_dfDIJoystick))) {
		// Release device
		pInputDev->Unacquire();
		pInputDev->Release();
		return DIENUM_CONTINUE;
	}

	// Get Caps
	memset(&m_JoyDevCaps[m_dwJoyDevs], 0, sizeof(DIDEVCAPS));
	m_JoyDevCaps[m_dwJoyDevs].dwSize = sizeof(DIDEVCAPS);
	if (FAILED(pInputDev->GetCapabilities(&m_JoyDevCaps[m_dwJoyDevs]))) {
		// Release device
		pInputDev->Unacquire();
		pInputDev->Release();
		return DIENUM_CONTINUE;
	}

	// Release device immediately
	pInputDev->Unacquire();
	pInputDev->Release();

	// Add and continue
	m_dwJoyDevs++;
	return DIENUM_CONTINUE;
}

//---------------------------------------------------------------------------
//
//	Initialize joystick
//	Called from ApplyCfg and Init. Only reinitialize if dwDevice is set
//
//---------------------------------------------------------------------------
void FASTCALL CInput::InitJoy()
{
	int i;
	int nDevice;
	int nAxis;
	BOOL bError[JoyDevices];
	DIPROPDWORD dpd;
	DIPROPRANGE dpr;

	ASSERT(this);
	ASSERT(m_lpDI);

	// Release all devices
	for (i=0; i<JoyDevices; i++) {
		if (m_lpDIDev2[i]) {
			m_lpDIDev2[i]->Release();
			m_lpDIDev2[i] = NULL;
		}

		if (m_lpDIJoy[i]) {
			m_lpDIJoy[i]->Unacquire();
			m_lpDIJoy[i]->Release();
			m_lpDIJoy[i] = NULL;
		}
	}

	// Clear state data
	for (i=0; i<JoyDevices; i++) {
		memset(&m_JoyState[i], 0, sizeof(DIJOYSTATE));
	}

	// Main loop
	for (i=0; i<JoyDevices; i++) {
		// Error flag OFF
		bError[i] = FALSE;

		// If not used, skip
		if (m_JoyCfg[i].nDevice == 0) {
			continue;
		}

		// Create device
		nDevice = m_JoyCfg[i].nDevice - 1;
		if (FAILED(m_lpDI->CreateDevice(m_JoyDevInst[nDevice].guidInstance,
										&m_lpDIJoy[i],
										NULL))) {
			continue;
		}

		// Error flag ON
		bError[i] = TRUE;

		// Set data format
		if (FAILED(m_lpDIJoy[i]->SetDataFormat(&c_dfDIJoystick))) {
			continue;
		}

		// Set cooperative level
		if (FAILED(m_lpDIJoy[i]->SetCooperativeLevel(m_pFrmWnd->m_hWnd,
							DISCL_BACKGROUND | DISCL_NONEXCLUSIVE))) {
			continue;
		}

		// Set axis mode (absolute)
		memset(&dpd, 0, sizeof(dpd));
		dpd.diph.dwSize = sizeof(DIPROPDWORD);
		dpd.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dpd.diph.dwHow = DIPH_DEVICE;
		dpd.dwData = DIPROPAXISMODE_ABS;
		if (FAILED(m_lpDIJoy[i]->SetProperty(DIPROP_AXISMODE, (LPDIPROPHEADER)&dpd))) {
			continue;
		}

		// Dead zone setting (no dead zone)
		memset(&dpd, 0, sizeof(dpd));
		dpd.diph.dwSize = sizeof(DIPROPDWORD);
		dpd.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dpd.diph.dwHow = DIPH_DEVICE;
		dpd.dwData = 0;
		if (FAILED(m_lpDIJoy[i]->SetProperty(DIPROP_DEADZONE, (LPDIPROPHEADER)&dpd))) {
			continue;
		}

		// Get axis range (get for all, but only valid ones are stored)
		for (nAxis=0; nAxis<JoyAxes; nAxis++) {
			// Get (error is OK)
			memset(&dpr, 0, sizeof(dpr));
			dpr.diph.dwSize = sizeof(DIPROPRANGE);
			dpr.diph.dwHeaderSize = sizeof(DIPROPHEADER);
			dpr.diph.dwHow = DIPH_BYOFFSET;
			dpr.diph.dwObj = JoyAxisOffsetTable[nAxis];
			m_lpDIJoy[i]->GetProperty(DIPROP_RANGE, (LPDIPROPHEADER)&dpr);

			// Store
			m_lJoyAxisMin[i][nAxis] = dpr.lMin;
			m_lJoyAxisMax[i][nAxis] = dpr.lMax;
		}

		// Get IDirectInputDevice2
		if (FAILED(m_lpDIJoy[i]->QueryInterface(IID_IDirectInputDevice2,
										(LPVOID*)&m_lpDIDev2[i]))) {
			// If cannot get IDirectInputDevice2
			m_lpDIDev2[i] = NULL;
		}

		// Error flag OFF (success)
		bError[i] = FALSE;
	}

	// For devices that had errors, release
	for (i=0; i<JoyDevices; i++) {
		if (bError[i]) {
			ASSERT(m_lpDIJoy[i]);
			m_lpDIJoy[i]->Unacquire();
			m_lpDIJoy[i]->Release();
			m_lpDIJoy[i] = NULL;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Joystick input
//
//---------------------------------------------------------------------------
void FASTCALL CInput::InputJoy(BOOL bEnable)
{
	int i;
	int nAxis;
	BYTE *pOffset;
	LONG *pAxis;
	LONG lRange;
	LONG lAxis;

	ASSERT(this);

	// If bEnable=FALSE and current m_bJoyEnable=TRUE (VM active), clear input data
	if (!bEnable && m_bJoyEnable) {
		for (i=0; i<JoyDevices; i++) {
			// Clear input data
			memset(&m_JoyState[i], 0, sizeof(DIJOYSTATE));
		}
		return;
	}

	// Device loop
	for (i=0; i<JoyDevices; i++) {
		// Skip if no device
		if (!m_lpDIJoy[i]) {
			continue;
		}

		// Polling
		if (m_lpDIDev2[i]) {
			if (FAILED(m_lpDIDev2[i]->Poll())) {
				// Acquire is required
				m_lpDIJoy[i]->Acquire();
				m_dwJoyAcquire[i]++;
				continue;
			}
		}

		// Get data
		if (FAILED(m_lpDIJoy[i]->GetDeviceState(sizeof(DIJOYSTATE),
												&m_JoyState[i]))) {
			// Acquire is required
			m_lpDIJoy[i]->Acquire();
			m_dwJoyAcquire[i]++;
			continue;
		}

		// Convert axis (-800 ~ 7FF)
		for (nAxis=0; nAxis<JoyAxes; nAxis++) {
			// Get pointer
			pOffset = (BYTE*)&m_JoyState[i];
			pOffset += JoyAxisOffsetTable[nAxis];
			pAxis = (LONG*)pOffset;

			// Skip if not valid
			if (m_lJoyAxisMin[i][nAxis] == m_lJoyAxisMax[i][nAxis]) {
				continue;
			}

			// Add -lMin (shift center to 0)
			lAxis = *pAxis - m_lJoyAxisMin[i][nAxis];

			// Get range, convert overflow to center
			lRange = m_lJoyAxisMax[i][nAxis] - m_lJoyAxisMin[i][nAxis] + 1;
			if (lRange >= 0x100000) {
				lRange >>= 12;
				lAxis >>= 12;
			}

			// Convert
			lAxis <<= 12;
			lAxis /= lRange;
			lAxis -= 0x800;
			*pAxis = lAxis;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Joystick synthesis
//
//---------------------------------------------------------------------------
void FASTCALL CInput::MakeJoy(BOOL bEnable)
{
	int i;
	int nAxis;
	int nButton;
	BYTE *pOffset;
	LONG *pAxis;
	LONG lAxis;
	PPI::joyinfo_t ji[PPI::PortMax];
	static bool jtk = FALSE;
	int dmy;

	ASSERT(this);
	ASSERT(m_pPPI);

	// Clear all
	memset(ji, 0, sizeof(ji));

	// Disabling would be a good option
	if (!bEnable) {
		for (i=0; i<PPI::PortMax; i++) {
			// Send data with no input
			m_pPPI->SetJoyInfo(i, &ji[i]);

			// Clear button sequence
			for (nButton=0; nButton<JoyButtons; nButton++) {
				m_JoyCfg[i].dwCount[nButton] = 0;
			}
		}
		return;
	}

	// Process input data
	for (i=0; i<JoyDevices; i++) {
		dmy = -1;

		// Axis
		for (nAxis = 0; nAxis < JoyAxes; nAxis++)
		{
			dmy++;// dmy = naxis

			// Get pointers
			pOffset = (BYTE*)&m_JoyState[i];
			pOffset += JoyAxisOffsetTable[nAxis];
			pAxis = (LONG*)pOffset;
			// Data acquisition
			lAxis = *pAxis;

			// Zero is ignored if repeated, exception is direction pad (POV)
			// Compare with first axis to check for null input for continue
			if ((lAxis == 0 && nAxis != 8) || (lAxis == 0 && ji[HIWORD(m_JoyCfg[i].dwAxis[0])].axis[0] == 0))
			{
				continue;
			}

			// Inversion
			if (m_JoyCfg[i].bAxis[nAxis]) {
				// 7FF to -800, -800 to 7FF
				lAxis = -1 - lAxis;
			}
			if (dmy < 4)
			{
				ji[HIWORD(m_JoyCfg[i].dwAxis[nAxis])].axis[dmy] = (DWORD)lAxis;
			}

			/*
			PAD to Axis

			Up = 0
			Up-Right = 4500
			Right = 9000
			Down-Right = 13500
			Down = 18000
			Down-Left = 22500
			Left = 27000
			Up-Left = 31500

			Axis 1 Up = -2048
			Up-Right = 1 -2048 / 0 2047
			Axis 0 Right = 2047
			Down-Right = 1 2047 / 0 2047
			Axis 1 Down = 2047
			Axis 0 Left = -2048

			*/


			if(nAxis==8)// If num Axis = 8 POV, direction pad movements are mapped to Axis 0 and 1 *-*
			{
				//CString sz;
				//sz.Format(_T("\nAxis: %d  lAxis: %d \n"), nAxis, lAxis);
				//OutputDebugStringW(CT2W(sz));

				if(lAxis==0)// Up
					ji[HIWORD(m_JoyCfg[i].dwAxis[1])].axis[1] = (DWORD)-2048;
				if(lAxis==4500)// Up-Right
				{
					ji[HIWORD(m_JoyCfg[i].dwAxis[1])].axis[1] = (DWORD)-2048;
					ji[HIWORD(m_JoyCfg[i].dwAxis[0])].axis[0] = (DWORD)2047;
				}
				if(lAxis==9000)// Right
					ji[HIWORD(m_JoyCfg[i].dwAxis[0])].axis[0] = (DWORD)2047;
				if(lAxis==13500)// Down-Right
				{
					ji[HIWORD(m_JoyCfg[i].dwAxis[0])].axis[0] = (DWORD)2047;
					ji[HIWORD(m_JoyCfg[i].dwAxis[1])].axis[1] = (DWORD)2047;
				}
				if(lAxis==18000)// Down
					ji[HIWORD(m_JoyCfg[i].dwAxis[1])].axis[1] = (DWORD)2047;
				if(lAxis==22500)// Down-Left
				{
					ji[HIWORD(m_JoyCfg[i].dwAxis[1])].axis[1] = (DWORD)2047;
					ji[HIWORD(m_JoyCfg[i].dwAxis[0])].axis[0] = (DWORD)-2048;
				}
				if(lAxis==27000)// Left
					ji[HIWORD(m_JoyCfg[i].dwAxis[0])].axis[0] = (DWORD)-2048;
				if(lAxis==31500)// Up-Left
				{
					ji[HIWORD(m_JoyCfg[i].dwAxis[0])].axis[0] = (DWORD)-2048;
					ji[HIWORD(m_JoyCfg[i].dwAxis[1])].axis[1] = (DWORD)-2048;
				}

				if (jtk)
				{
					// Deactivate special game keys mapped from joystick to keyboard *-*
					if(m_pKeyboard->keyboard.status[0x55]==TRUE)// XF1
					{
						m_pKeyboard->BreakKey(0x55);
						jtk = FALSE;
					}
					if(m_pKeyboard->keyboard.status[0x57]==TRUE)// XF3
					{
						m_pKeyboard->BreakKey(0x57);
						jtk = FALSE;
					}
					if(m_pKeyboard->keyboard.status[0x63]==TRUE)// F1
					{
						m_pKeyboard->BreakKey(0x63);
						jtk = FALSE;
					}
					if(m_pKeyboard->keyboard.status[0x65]==TRUE)// F3
					{
						m_pKeyboard->BreakKey(0x65);
						jtk = FALSE;
					}
					if(m_pKeyboard->keyboard.status[0x72]==TRUE)// OPT1
					{
						m_pKeyboard->BreakKey(0x72);
						jtk = FALSE;
					}
					if(m_pKeyboard->keyboard.status[0x73]==TRUE)// OPT2
					{
						m_pKeyboard->BreakKey(0x73);
						jtk = FALSE;
					}
				}
			}

		}


		// Joystick button press
		for (nButton = 0; nButton < JoyButtons; nButton++) {

			if(m_JoyState[i].rgbButtons[nButton]==0x80)// Joystick to keyboard mapping
			{
				switch(nButton)// Buttons 6 and 7 are assigned to OPT, OPT2, button 9 to START *-*
				{
					case 6:
						m_pKeyboard->MakeKey(0x72);//OPT1
						m_pKeyboard->MakeKey(0x57);//XF3
						jtk = TRUE;
						break;
					case 7:
						m_pKeyboard->MakeKey(0x73);//OPT2
						m_pKeyboard->MakeKey(0x55);//XF1
						jtk = TRUE;
						break;
					case 8:
						m_pKeyboard->MakeKey(0x65);// 8 = Select or Credit (F3)
						jtk = TRUE;
						break;
					case 9:
						m_pKeyboard->MakeKey(0x63);// 9 = Start Button (F1)
						jtk = TRUE;
						break;
				}
			}

			/*
			if (m_JoyState[i].rgbButtons[nButton] == 0x80)
			{
				CString sz;
				sz.Format(_T("\nButton: %d  Status:%d \n"), nButton, m_JoyState[i].rgbButtons[nButton]);
				OutputDebugStringW(CT2W(sz));
			}
			*/

			// Ignore if not assigned
			if (LOWORD(m_JoyCfg[i].dwButton[nButton]) == 0) {
				continue;
			}

			// OFF
			if ((m_JoyState[i].rgbButtons[nButton] & 0x80) == 0)
			{
				// Only clear continuous fire counter (button press info is cleared at the beginning)

				m_JoyCfg[i].dwCount[nButton] = 0;
				continue;
			}

			ASSERT(HIWORD(m_JoyCfg[i].dwButton[nButton]) >= 0);
			ASSERT(HIWORD(m_JoyCfg[i].dwButton[nButton]) < PPI::PortMax);
			ASSERT(LOWORD(m_JoyCfg[i].dwButton[nButton]) > 0);
			ASSERT(LOWORD(m_JoyCfg[i].dwButton[nButton]) <= PPI::ButtonMax);

			// No repeat
			if (m_JoyCfg[i].dwRapid[nButton] == 0) {
				// Store at destination position
				ji[HIWORD(m_JoyCfg[i].dwButton[nButton])].button[LOWORD(m_JoyCfg[i].dwButton[nButton]) - 1]
					= TRUE;
				continue;
			}

			// Has continuous fire
			if (m_JoyCfg[i].dwCount[nButton] == 0) {
				// ON and reload counter as if first time
				ji[HIWORD(m_JoyCfg[i].dwButton[nButton])].button[LOWORD(m_JoyCfg[i].dwButton[nButton]) - 1]
					= TRUE;
				m_JoyCfg[i].dwCount[nButton] = JoyRapidTable[m_JoyCfg[i].dwRapid[nButton]];
				continue;
			}

			// Countdown continuous fire; if 0, ON and reload counter
			m_JoyCfg[i].dwCount[nButton]--;
			if (m_JoyCfg[i].dwCount[nButton] == 0) {
				ji[HIWORD(m_JoyCfg[i].dwButton[nButton])].button[LOWORD(m_JoyCfg[i].dwButton[nButton]) - 1]
					= TRUE;
				m_JoyCfg[i].dwCount[nButton] = JoyRapidTable[m_JoyCfg[i].dwRapid[nButton]];
				continue;
			}

			// Separate ON/OFF in first and second half of counter
			if (m_JoyCfg[i].dwCount[nButton] > (JoyRapidTable[m_JoyCfg[i].dwRapid[nButton]] >> 1)) {
				ji[HIWORD(m_JoyCfg[i].dwButton[nButton])].button[LOWORD(m_JoyCfg[i].dwButton[nButton]) - 1]
					= TRUE;
			}
			else {
				ji[HIWORD(m_JoyCfg[i].dwButton[nButton])].button[LOWORD(m_JoyCfg[i].dwButton[nButton]) - 1]
					= FALSE;
			}
		}
	}

	// Combined with keyboard


	// Send to PPI
	for (i=0; i<PPI::PortMax; i++) {
		m_pPPI->SetJoyInfo(i, &ji[i]);
	}
}



//---------------------------------------------------------------------------
//
//	Joystick enable/disable
//
//---------------------------------------------------------------------------
void FASTCALL CInput::EnableJoy(BOOL bEnable)
{
	PPI::joyinfo_t ji;

	ASSERT(this);

	// Do nothing if same
	if (m_bJoyEnable == bEnable) {
		return;
	}

	// Change
	m_bJoyEnable = bEnable;

	// If changed to FALSE, send dummy data to PPI
	if (!bEnable) {
		memset(&ji, 0, sizeof(ji));
		m_pPPI->SetJoyInfo(0, &ji);
		m_pPPI->SetJoyInfo(1, &ji);
	}
}

//---------------------------------------------------------------------------
//
//	Get joystick device
//
//---------------------------------------------------------------------------
int FASTCALL CInput::GetJoyDevice(int nJoy) const
{
	ASSERT(this);
	ASSERT((nJoy >= 0) && (nJoy < JoyDevices));

	// 0 means not assigned
	if (m_JoyCfg[nJoy].nDevice == 0) {
		return 0;
	}

	// If device pointer is not available, treat as error and return -1
	if (!m_lpDIJoy[nJoy]) {
		return -1;
	}

	// Is available, so return device number
	return m_JoyCfg[nJoy].nDevice;
}

//---------------------------------------------------------------------------
//
//	Get joystick axis
//
//---------------------------------------------------------------------------
LONG FASTCALL CInput::GetJoyAxis(int nJoy, int nAxis) const
{
	BYTE *pOffset;
	LONG *pAxis;

	ASSERT(this);
	ASSERT((nJoy >= 0) && (nJoy < JoyDevices));
	ASSERT((nAxis >= 0) && (nAxis < JoyAxes));

	// 0 means not assigned
	if (m_JoyCfg[nJoy].nDevice == 0) {
		return 0x10000;
	}

	// If device pointer is not available, treat as error
	if (!m_lpDIJoy[nJoy]) {
		return 0x10000;
	}

	// If not available, return error
	/*if (m_lJoyAxisMin[nJoy][nAxis] == m_lJoyAxisMax[nJoy][nAxis]) {
		return 0x10000;
	}*/

	// Return value
	pOffset = (BYTE*)&m_JoyState[nJoy];
	pOffset += JoyAxisOffsetTable[nAxis];
	pAxis = (LONG*)pOffset;
	return *pAxis;
}

//---------------------------------------------------------------------------
//
//	Get joystick button
//
//---------------------------------------------------------------------------
DWORD FASTCALL CInput::GetJoyButton(int nJoy, int nButton) const
{
	ASSERT(this);
	ASSERT((nJoy >= 0) && (nJoy < JoyDevices));
	ASSERT((nButton >= 0) && (nButton < JoyButtons));

	// 0 means not assigned
	if (m_JoyCfg[nJoy].nDevice == 0) {
		return 0x10000;
	}

	// If device pointer is not available, treat as error
	if (!m_lpDIJoy[nJoy]) {
		return 0x10000;
	}

	// If button does not exist, return error
	if (nButton >= (int)m_JoyDevCaps[m_JoyCfg[nJoy].nDevice - 1].dwButtons) {
		return 0x10000;
	}

	// Return value
	return (DWORD)m_JoyState[nJoy].rgbButtons[nButton];
}

//---------------------------------------------------------------------------
//
//	Get joystick caps
//
//---------------------------------------------------------------------------
BOOL FASTCALL CInput::GetJoyCaps(int nDevice, CString& strDesc, DIDEVCAPS *pCaps) const
{
	ASSERT(this);
	ASSERT(nDevice >= 0);
	ASSERT(pCaps);

	// Compare with joystick device
	if (nDevice >= (int)m_dwJoyDevs) {
		// Device at specified index does not exist
		return FALSE;
	}

	// Set desc
	ASSERT(nDevice < JoyDeviceMax);
	strDesc = m_JoyDevInst[nDevice].tszInstanceName;

	// Copy caps
	*pCaps = m_JoyDevCaps[nDevice];

	// OK
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get joystick config
//
//---------------------------------------------------------------------------
void FASTCALL CInput::GetJoyCfg(int nJoy, LPJOYCFG lpJoyCfg) const
{
	ASSERT(this);
	ASSERT((nJoy >= 0) && (nJoy < JoyDevices));

	// Copy config
	*lpJoyCfg = m_JoyCfg[nJoy];
}

//---------------------------------------------------------------------------
//
//	Set joystick config
//
//---------------------------------------------------------------------------
void FASTCALL CInput::SetJoyCfg(int nJoy, const LPJOYCFG lpJoyCfg)
{
	ASSERT(this);
	ASSERT((nJoy >= 0) && (nJoy < JoyDevices));

	// Copy config
	m_JoyCfg[nJoy] = *lpJoyCfg;
}

//---------------------------------------------------------------------------
//
//	Joystick axis table
//
//---------------------------------------------------------------------------
const DWORD CInput::JoyAxisOffsetTable[JoyAxes] = {
	DIJOFS_X,
	DIJOFS_Y,
	DIJOFS_Z,
	DIJOFS_RX,
	DIJOFS_RY,
	DIJOFS_RZ,
	DIJOFS_SLIDER(0),
	DIJOFS_SLIDER(1),
	DIJOFS_POV(0)
};

//---------------------------------------------------------------------------
//
//	Joystick auto-repeat table
//	Values for when assuming 60 frames/sec based on auto-repeat rate
//
//---------------------------------------------------------------------------
const DWORD CInput::JoyRapidTable[JoyRapids + 1] = {
	0,									// (unused area)
	30,									// 2 times
	20,									// 3 times
	15,									// 4 times
	12,									// 5 times
	8,									// 7.5 times
	6,									// 10 times
	5,									// 12 times
	4,									// 15 times
	3,									// 20 times
	2									// 30 times
};

#endif	// _WIN32
