//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 š°å•½D(ytanaka@ipc-tokai.or.jp)
//	[ MFC Component ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_inp_h)
#define mfc_inp_h

//===========================================================================
//
//	Component
//
//===========================================================================
class CInput : public CComponent
{
public:
	// Joystick constants
	enum {
		JoyDeviceMax = 16,				// Maximum supported devices
		JoyDevices = 2,					// Maximum used devices
		JoyAxes = 9,					// Maximum axes
		JoyButtons = 12,				// Maximum buttons
		JoyRapids = 10,					// Auto-repeat speed levels
	};
	// Joystick configuration
	typedef struct _JOYCFG {
		int nDevice;					// Device number (1~), unused=0
		DWORD dwAxis[JoyAxes];			// Axis mapping (1~), unused=0
		BOOL bAxis[JoyAxes];			// Axis inversion
		DWORD dwButton[JoyButtons];		// Button mapping (1~), unused=0
		DWORD dwRapid[JoyButtons];		// Auto-repeat interval, no repeat=0
		DWORD dwCount[JoyButtons];		// Auto-repeat counter
	} JOYCFG, *LPJOYCFG;

	CFrmWnd* xWnd;
	int joyType[2];
public:
	// Basic procedures
	CInput(CFrmWnd *pWnd);
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

	// Save/Load
	BOOL FASTCALL Save(Fileio *pFio, int nVer);
										// Save
	BOOL FASTCALL Load(Fileio *pFio, int nVer);
										// Load

	// External API
	void FASTCALL Process(BOOL bRun);
										// Processing
	void FASTCALL Activate(BOOL bActivate);
										// Active notification
	BOOL FASTCALL IsActive() const		{ return m_bActive; }
										// Get active status
	void FASTCALL Menu(BOOL bMenu);
										// Menu notification
	BOOL FASTCALL IsMenu() const		{ return m_bMenu; }
										// Get menu status
	DWORD FASTCALL GetProcessCount() const	{ return m_dwProcessCount; }
										// Get process count
	DWORD FASTCALL GetAcquireCount(int nType) const;
										// Get acquire count

	// Keyboard
	void FASTCALL GetKeyBuf(BOOL *pKeyBuf) const;
										// Get key buffer
	void FASTCALL EnableKey(BOOL bEnable);
										// Keyboard enable/disable
	void FASTCALL SetDefaultKeyMap(DWORD *pKeyMap);
										// Set default key map
	int FASTCALL Key2DirectX(int nKey);
										// Key conversion
	int FASTCALL Key2X68k(int nDXKey);
										// Key conversion
	static LPCTSTR FASTCALL GetKeyName(int nKey);
										// Get key name
	static LPCTSTR FASTCALL GetKeyID(int nID);
										// Get key ID
	void FASTCALL GetKeyMap(DWORD *pKeyMap);
										// Get key map
	void FASTCALL SetKeyMap(const DWORD *pKeyMap);
										// Set key map
	BOOL FASTCALL IsKeyMapped(int nID) const;
										// Key map valid check

	// Mouse
	void FASTCALL SetMouseMode(BOOL bMode);
										// Set mouse mode
	BOOL FASTCALL GetMouseMode() const	{ return m_bMouseMode; }
										// Get mouse mode
	void FASTCALL GetMouseInfo(int *pPos, BOOL *pBtn) const;
										// Get mouse info

	// Joystick
	static BOOL CALLBACK EnumCb(LPDIDEVICEINSTANCE pDevInst, LPVOID pvRef);
										// Joystick callback
	void FASTCALL EnableJoy(BOOL bEnable);
										// Joystick enable/disable
	int FASTCALL GetJoyDevice(int nJoy) const;
										// Get joystick device
	LONG FASTCALL GetJoyAxis(int nJoy, int nAxis) const;
										// Get joystick axis
	DWORD FASTCALL GetJoyButton(int nJoy, int nButton) const;
										// Get joystick button
	BOOL FASTCALL GetJoyCaps(int nDevice, CString& strDesc, DIDEVCAPS *pCaps) const;
										// Get joystick caps
	void FASTCALL GetJoyCfg(int nJoy, LPJOYCFG lpJoyCfg) const;
										// Get joystick config
	void FASTCALL SetJoyCfg(int nJoy, const LPJOYCFG lpJoyCfg);
										// Set joystick config

private:
	// General
	LPDIRECTINPUT m_lpDI;
										// DirectInput
	BOOL m_bActive;
										// Active flag
	BOOL m_bMenu;
										// Menu flag
	CRTC *m_pCRTC;
										// CRTC
	DWORD m_dwDispCount;
										// CRTC display count
	DWORD m_dwProcessCount;
										// Process count

	// Save/Load
	BOOL FASTCALL SaveMain(Fileio *pFio);
										// Save main
	BOOL FASTCALL Load200(Fileio *pFio);
										// Load main (version2.00)
	BOOL FASTCALL Load201(Fileio *pFio);
										// Load main (version2.01)

	// Keyboard
	BOOL FASTCALL InitKey();
										// Keyboard initialization
	void FASTCALL InputKey(BOOL bEnable);
										// Keyboard input
	Keyboard *m_pKeyboard;
										// Keyboard
	LPDIRECTINPUTDEVICE m_lpDIKey;
										// Keyboard device
	DWORD m_dwKeyAcquire;
										// Keyboard acquire count
	BOOL m_bKeyEnable;
										// Keyboard enable flag
	BOOL m_KeyBuf[0x100];
										// Keyboard buffer
	DWORD m_KeyMap[0x100];
										// Key translation map
	static const DWORD m_KeyMap106[];
										// Default map (106)
	static LPCTSTR KeyNameTable[];
										// Key name table
	static LPCSTR KeyIDTable[];
										// DirectX key ID table

	// Mouse
	BOOL FASTCALL InitMouse();
										// Mouse initialization
	void FASTCALL InputMouse(BOOL bEnable);
										// Mouse input
	Mouse *m_pMouse;
										// Mouse
	LPDIRECTINPUTDEVICE m_lpDIMouse;
										// Mouse device
	DWORD m_dwMouseAcquire;
										// Mouse acquire count
	BOOL m_bMouseMode;
										// Mouse mode flag
	int m_nMouseX;
										// Mouse x coordinate
	int m_nMouseY;
										// Mouse y coordinate
	BOOL m_bMouseB[2];
										// Mouse left/right buttons
	DWORD m_dwMouseMid;
										// Mouse middle button count
	BOOL m_bMouseMid;
										// Mouse middle button enable flag

	// Joystick
	void FASTCALL EnumJoy();
										// Enumerate joysticks
	BOOL FASTCALL EnumDev(LPDIDEVICEINSTANCE pDevInst);
										// Add joystick
	void FASTCALL InitJoy();
										// Initialize joystick
	void FASTCALL InputJoy(BOOL bEnable);
										// Joystick input
	void FASTCALL MakeJoy(BOOL bEnable);
										// Synthesize joystick
	PPI *m_pPPI;
										// PPI
	BOOL m_bJoyEnable;
										// Joystick enable/disable
	LPDIRECTINPUTDEVICE m_lpDIJoy[JoyDevices];
										// Joystick device
	LPDIRECTINPUTDEVICE2 m_lpDIDev2[JoyDevices];
										// Extended polled device
	JOYCFG m_JoyCfg[JoyDevices];
										// Joystick configuration
	LONG m_lJoyAxisMin[JoyDevices][JoyAxes];
										// Joystick axis minimum
	LONG m_lJoyAxisMax[JoyDevices][JoyAxes];
										// Joystick axis maximum
	DWORD m_dwJoyAcquire[JoyDevices];
										// Joystick acquire count
	DIJOYSTATE m_JoyState[JoyDevices];
										// Joystick state
	DWORD m_dwJoyDevs;
										// Number of joystick devices
	DIDEVCAPS m_JoyDevCaps[JoyDeviceMax];
										// Joystick caps
	DIDEVICEINSTANCE m_JoyDevInst[JoyDeviceMax];
										// Joystick instance
	static const DWORD JoyAxisOffsetTable[JoyAxes];
										// Joystick axis offset table
	static const DWORD JoyRapidTable[JoyRapids + 1];
										// Joystick auto-repeat table
};

#endif	// mfc_inp_h
#endif	// _WIN32
