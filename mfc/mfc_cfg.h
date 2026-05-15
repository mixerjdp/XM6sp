//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[MFC configuration]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_cfg_h)
#define mfc_cfg_h

#include "config.h"
#include "ppi.h"

//===========================================================================
//
//	Configuration
//
//===========================================================================
class CConfig : public CComponent
{
public:
	// Basic functions
	CConfig(CFrmWnd *pWnd);
										// Constructor
	BOOL FASTCALL Init();
	BOOL FASTCALL CustomInit(BOOL ArchivoDefault);
										// Initialization
	void FASTCALL Cleanup();
	void FASTCALL Cleanup2();
										// Cleanup

	// Configuration data (global)
	void FASTCALL GetConfig(Config *pConfigBuf) const;
										// Get configuration data
	void FASTCALL SetConfig(Config *pConfigBuf);
										// Set configuration data

	// Configuration data (individual)
	void FASTCALL SetWindowScale(int nScale);
										// Window scaling configuration
	void FASTCALL SetMIDIDevice(int nDevice, BOOL bIn);
										// MIDI device configuration

	// MRU
	void FASTCALL SetMRUFile(int nType, LPCTSTR pszFile);
										// MRU file configuration
	void FASTCALL GetMRUFile(int nType, int nIndex, LPTSTR pszFile) const;
										// Get MRU file
	int FASTCALL GetMRUNum(int nType) const;
										// Get MRU file count

	// Save / Load
	BOOL FASTCALL Save(Fileio *pFio, int nVer);
										// Save
	BOOL FASTCALL Load(Fileio *pFio, int nVer);
										// Load
	BOOL FASTCALL IsApply();
										// Apply check

private:
	// Configuration data
	typedef struct _INIKEY {
		void *pBuf;						// Pointer to the INI value buffer
		LPCTSTR pszSection;				// Section name
		LPCTSTR pszKey;					// Key name
		int nType;						// Type
		int nDef;						// Default value
		int nMin;						// Minimum value (for some types only)
		int nMax;						// Maximum value (for some types only)
	} INIKEY, *PINIKEY;

	// INI file
	TCHAR m_IniFile[FILEPATH_MAX];
										// INI file name

	// Configuration data
	void FASTCALL LoadConfig();
										// Load configuration data
	void FASTCALL SaveConfig() const;
										// Save configuration data
	static const INIKEY IniTable[];
										// INI configuration data table
	static Config m_Config;
										// Configuration data

	// Version compatibility
	void FASTCALL ResetSASI();
										// SASI reconfiguration
	void FASTCALL ResetCDROM();
										// CD-ROM reconfiguration
	static BOOL m_bCDROM;
										// CD-ROM enabled

	// MRU
	enum {
		MruTypes=5					// MRU type count
	};
	void FASTCALL ClearMRU(int nType);
										// Clear MRU
	void FASTCALL LoadMRU(int nType);
										// Load MRU
	void FASTCALL SaveMRU(int nType) const;
										// Save MRU
	TCHAR m_MRUFile[MruTypes][9][FILEPATH_MAX];
										// MRU files
	int m_MRUNum[MruTypes];
										// MRU count

	// Keys
	void FASTCALL LoadKey() const;
										// Load keys
	void FASTCALL SaveKey() const;
										// Save keys

	// TrueKey
	void FASTCALL LoadTKey() const;
										// Load TrueKey
	void FASTCALL SaveTKey() const;
										// Save TrueKey

	// Version compatibility
	BOOL FASTCALL Load200(Fileio *pFio);
										// Version 2.00 or 2.01
	BOOL FASTCALL Load202(Fileio *pFio);
										// Version 2.02 or 2.03

	// Load/Save
	BOOL m_bApply;
										// Load and ApplyCfg execution flag
};

//---------------------------------------------------------------------------
//
//	Class forward declaration
//
//---------------------------------------------------------------------------
class CConfigSheet;

//===========================================================================
//
//	Property page configuration
//
//===========================================================================
class CConfigPage : public CPropertyPage
{
public:
	CConfigPage();
										// Constructor
	void FASTCALL Init(CConfigSheet *pSheet);
										// Initialization
	virtual BOOL OnInitDialog();
										// Dialog initialization
	virtual BOOL OnSetActive();
										// Active page
	DWORD FASTCALL GetID() const		{ return m_dwID; }
										// Get ID

protected:
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT nMsg);
										// Mouse cursor configuration
	Config *m_pConfig;
										// Configuration data
	DWORD m_dwID;
										// Page ID
	int m_nTemplate;
										// Template ID
	UINT m_uHelpID;
										// Help ID
	UINT m_uMsgID;
										// Help message ID
	CConfigSheet *m_pSheet;
										// Property sheet

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Basic page
//
//===========================================================================
class CBasicPage : public CConfigPage
{
public:
	CBasicPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK

protected:
	afx_msg void OnMPUFull();
										// MPU full speed

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Sound page
//
//===========================================================================
class CSoundPage : public CConfigPage
{
public:
	CSoundPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Vertical scroll
	afx_msg void OnSelChange();
										// Combo box change

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Control state change
	BOOL m_bEnableCtrl;
										// Control state
	static const UINT ControlTable[];
										// Control table

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Volume page
//
//===========================================================================
class CVolPage : public CConfigPage
{
public:
	CVolPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	void OnCancel();
										// Cancel

protected:
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Horizontal scroll
	afx_msg void OnFMCheck();
										// FM synthesizer check
	afx_msg void OnADPCMCheck();
										// ADPCM synthesizer check
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
#else
	afx_msg void OnTimer(UINT nTimerID);
#endif
										// Timer

private:
	CSound *m_pSound;
										// Sound
	OPMIF *m_pOPMIF;
										// OPM interface
	ADPCM *m_pADPCM;
										// ADPCM
	CMIDI *m_pMIDI;
										// MIDI
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
#else
	UINT m_nTimerID;
#endif
										// Timer ID
	int m_nMasterVol;
										// Master volume
	int m_nMasterOrg;
										// Original master volume
	int m_nMIDIVol;
										// MIDI volume
	int m_nMIDIOrg;
										// MIDI original volume

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Keyboard page
//
//===========================================================================
class CKbdPage : public CConfigPage
{
public:
	CKbdPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	void OnCancel();
										// Cancel

protected:
	afx_msg void OnEdit();
										// Edit
	afx_msg void OnDefault();
										// Default
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Column click
	afx_msg void OnRClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Column right click
	afx_msg void OnConnect();
										// Connection

private:
	void FASTCALL UpdateReport();
										// Report update
	void FASTCALL EnableControls(BOOL bEnable);
										// Control state change
	DWORD m_dwEdit[0x100];
										// Edit
	DWORD m_dwBackup[0x100];
										// Backup
	CInput *m_pInput;
										// CInput
	BOOL m_bEnableCtrl;
										// Control state
	static const UINT ControlTable[];
										// Control table

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Keyboard map editor dialog
//
//===========================================================================
class CKbdMapDlg : public CDialog
{
public:
	CKbdMapDlg(CWnd *pParent, DWORD *pMap);
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	void OnCancel();
										// Cancel

protected:
	afx_msg void OnPaint();
										// Dialog draw
	afx_msg LONG OnKickIdle(UINT uParam, LONG lParam);
										// Idle process
	afx_msg LONG OnApp(UINT uParam, LONG lParam);
										// Sub-window notification

private:
	void FASTCALL OnDraw(CDC *pDC);
										// Drawing subroutine
	CRect m_rectStat;
										// Status rectangle
	CString m_strStat;
										// Status message
	CString m_strGuide;
										// Guide message
	CWnd *m_pDispWnd;
										// CKeyDispWnd
	CInput *m_pInput;
										// CInput
	DWORD *m_pEditMap;
										// Edit map

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Key input dialog
//
//===========================================================================
class CKeyinDlg : public CDialog
{
public:
	CKeyinDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	UINT m_nTarget;
										// Target key
	UINT m_nKey;
										// Key assignment

protected:
	afx_msg void OnPaint();
										// Draw
	afx_msg UINT OnGetDlgCode();
										// Get dialog code
	afx_msg LONG OnKickIdle(UINT uParam, LONG lParam);
										// Idle process
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
										// Right click

private:
	void FASTCALL OnDraw(CDC *pDC);
										// Drawing subroutine
	CInput *m_pInput;
										// CInput
	BOOL m_bKey[0x100];
										// Key memory
	CRect m_GuideRect;
										// Guide rectangle
	CString m_GuideString;
										// Guide string
	CRect m_AssignRect;
										// Assignment rectangle
	CString m_AssignString;
										// Assignment string
	CRect m_KeyRect;
										// Key rectangle
	CString m_KeyString;
										// Key string

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Mouse page
//
//===========================================================================
class CMousePage : public CConfigPage
{
public:
	CMousePage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK

protected:
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Horizontal scroll
	afx_msg void OnPort();
										// Port selection

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Control state change
	BOOL m_bEnableCtrl;
										// Control state
	static const UINT ControlTable[];
										// Control table

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Joystick page
//
//===========================================================================
class CJoyPage : public CConfigPage
{
public:
	CJoyPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	void OnCancel();
										// Cancel

protected:
	BOOL OnCommand(WPARAM wParam, LPARAM lParam);
										// Command notification

private:
	void FASTCALL OnSelChg(CComboBox* pComboBox);
										// Combo box change
	void FASTCALL OnDetail(UINT nButton);
										// Details
	void FASTCALL OnSetting(UINT nButton);
										// Configuration
	CButton* GetCorButton(UINT nComboBox);
										// Get corresponding button
	CComboBox* GetCorCombo(UINT nButton);
										// Get corresponding combo box
	CInput *m_pInput;
										// CInput
	static UINT ControlTable[];
										// Control table
};

//===========================================================================
//
//	Joystick details dialog
//
//===========================================================================
class CJoyDetDlg : public CDialog
{
public:
	CJoyDetDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Initialization

	CString m_strDesc;
										// Device name
	int m_nPort;
										// Port number (0 or 1)
	int m_nType;
										// Type (0-12)
};

//===========================================================================
//
//	Button configuration page
//
//===========================================================================
class CBtnSetPage : public CPropertyPage
{
public:
	CBtnSetPage();
										// Constructor
	void FASTCALL Init(CPropertySheet *pSheet);
										// Create
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	void OnCancel();
										// Cancel
	int m_nJoy;
										// Joystick number (0 or 1)
	int m_nType[PPI::PortMax];
										// Joystick type (0-12)

protected:
	afx_msg void OnPaint();
										// Draw
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
#else
	afx_msg void OnTimer(UINT nTimerID);
#endif
										// Timer
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Horizontal scroll
	BOOL OnCommand(WPARAM wParam, LPARAM lParam);
										// Command notification

private:
	enum CtrlType {
		BtnLabel,						// Label (button n)
		BtnCombo,						// Combo box
		BtnRapid,						// Rapid-fire slider
		BtnValue						// Rapid-fire value label
	};
	void FASTCALL OnDraw(CDC *pDC, BOOL *pButton, BOOL bForce);
										// Main draw
	void FASTCALL OnSlider(int nButton);
										// Slider modification
	void FASTCALL OnSelChg(int nButton);
										// Combo box change
	void FASTCALL GetButtonDesc(const char *pszDesc, CString &strDesc);
										// Get button description
	UINT FASTCALL GetControl(int nButton, CtrlType ctlType) const;
										// Get control ID
	CPropertySheet *m_pSheet;
										// Parent sheet
	CInput *m_pInput;
										// CInput
	CRect m_rectLabel[12];
										// Label position
	BOOL m_bButton[12];
										// Button state
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
#else
	UINT m_nTimerID;
#endif
										// Timer ID
	static const UINT ControlTable[];
										// Control table
	static const int RapidTable[];
										// Rapid-fire table

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Joystick property sheet
//
//===========================================================================
class CJoySheet : public CPropertySheet
{
public:
	CJoySheet(CWnd *pParent);
										// Constructor
	void FASTCALL SetParam(int nJoy, int nCombo, int nType[]);
										// Parameter configuration
	void FASTCALL InitSheet();
										// Page initialization
	int FASTCALL GetAxes() const;
										// Get axis count
	int FASTCALL GetButtons() const;
										// Get button count

private:
	CBtnSetPage m_BtnSet;
										// Button configuration page
	CInput *m_pInput;
										// CInput
	int m_nJoy;
										// Joystick number (0 or 1)
	int m_nCombo;
										// Combo box selection
	int m_nType[PPI::PortMax];
										// VM side type selection
	DIDEVCAPS m_DevCaps;
										// Device caps
};

//===========================================================================
//
//	SASI page
//
//===========================================================================
class CSASIPage : public CConfigPage
{
public:
	CSASIPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	BOOL OnSetActive();
										// Active page
	int FASTCALL GetDrives(const Config *pConfig) const;
										// Get SASI drive count

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Vertical scroll
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Column click

private:
	void FASTCALL UpdateList();
										// Update list control
	void FASTCALL CheckSASI(DWORD *pDisk);
										// SASI file check
	void FASTCALL EnableControls(BOOL bEnable, BOOL bDrive = TRUE);
										// Control state change
	SASI *m_pSASI;
										// SASI
	BOOL m_bInit;
										// Initialization flag
	int m_nDrives;
										// Drive count
	TCHAR m_szFile[16][FILEPATH_MAX];
										// SASI hard disk file name
	CString m_strError;
										// Error string

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	SxSI page
//
//===========================================================================
class CSxSIPage : public CConfigPage
{
public:
	CSxSIPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	BOOL OnSetActive();
										// Active page
	void OnOK();
										// OK
	int FASTCALL GetDrives(const Config *pConfig) const;
										// Get SxSI drive count

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Vertical scroll
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Column click
	afx_msg void OnCheck();
										// Check box

private:
	enum DevType {
		DevSASI,						// SASI hard disk
		DevSCSI,						// SCSI hard disk
		DevMO,							// MO SCSI unit
		DevInit,						// SCSI Initiator (Host)
		DevNone							// No device
	};
	void FASTCALL UpdateList();
										// Update list control
	void FASTCALL BuildMap();
										// Create device map
	int FASTCALL CheckSCSI(int nDrive);
										// Check SCSI device
	void FASTCALL EnableControls(BOOL bEnable, BOOL bDrive = TRUE);
										// Control state change
	BOOL m_bInit;
										// Initialization flag
	int m_nSASIDrives;
										// SASI drive count
	DevType m_DevMap[8];
										// Device map
	TCHAR m_szFile[6][FILEPATH_MAX];
										// SCSI hard disk file name
	CString m_strSASI;
										// SASI HD string
	CString m_strMO;
										// MO SCSI string
	CString m_strInit;
										// Initiator string
	CString m_strNone;
										// N/A string
	CString m_strError;
										// Device error string
	static const UINT ControlTable[];
										// Control table

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	SCSI page
//
//===========================================================================
class CSCSIPage : public CConfigPage
{
public:
	CSCSIPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	int FASTCALL GetInterface(const Config *pConfig) const;
										// Get interface type

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Vertical scroll
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Column click
	afx_msg void OnButton();
										// Browse button
	afx_msg void OnCheck();
										// Check box

private:
	enum DevType {
		DevSCSI,						// SCSI hard disk
		DevMO,							// MO SCSI unit
		DevCD,							// CD-ROM SCSI unit
		DevInit,						// SCSI Initiator (Host)
		DevNone							// No device
	};
	int FASTCALL GetIfCtrl() const;
										// Get interface (control type)
	BOOL FASTCALL CheckROM(int nType) const;
										// ROM check
	void FASTCALL UpdateList();
										// Update list control
	void FASTCALL BuildMap();
										// Create device map
	int FASTCALL CheckSCSI(int nDrive);
										// Check SCSI device
	void FASTCALL EnableControls(BOOL bEnable, BOOL bDrive = TRUE);
										// Control state change
	SCSI *m_pSCSI;
										// SCSI device
	BOOL m_bInit;
										// Initialization flag
	int m_nDrives;
										// Drive count
	BOOL m_bMOFirst;
										// MO first flag
	DevType m_DevMap[8];
										// Device map
	TCHAR m_szFile[5][FILEPATH_MAX];
										// SCSI hard disk file name
	CString m_strMO;
										// MO SCSI string
	CString m_strCD;
										// CD SCSI string
	CString m_strInit;
										// Initiator string
	CString m_strNone;
										// N/A string
	CString m_strError;
										// Device error string
	static const UINT ControlTable[];
										// Control table

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Port page
//
//===========================================================================
class CPortPage : public CConfigPage
{
public:
	CPortPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
};

//===========================================================================
//
//	MIDI page
//
//===========================================================================
class CMIDIPage : public CConfigPage
{
public:
	CMIDIPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	void OnCancel();
										// Cancel

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Vertical scroll
	afx_msg void OnBIDClick();
										// Board ID click

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Control state change
	CMIDI *m_pMIDI;
										// MIDI component
	BOOL m_bEnableCtrl;
										// Control state
	int m_nInDelay;
										// Input delay (ms)
	int m_nOutDelay;
										// Output delay (ms)
	static const UINT ControlTable[];
										// Control table

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Alteration page
//
//===========================================================================
class CAlterPage : public CConfigPage
{
public:
	CAlterPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	BOOL OnKillActive();
										// Page leave
	BOOL FASTCALL HasParity(const Config *pConfig) const;
										// SASI parity check

protected:
	void DoDataExchange(CDataExchange *pDX);
										// Data exchange

private:
	BOOL m_bInit;
										// Initialization
	BOOL m_bParity;
										// With parity
};

//===========================================================================
//
//	Resume page
//
//===========================================================================
class CResumePage : public CConfigPage
{
public:
	CResumePage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization

protected:
	void DoDataExchange(CDataExchange *pDX);
										// Data exchange
};

//===========================================================================
//
//	TrueKey input dialog
//
//===========================================================================
class CTKeyDlg : public CDialog
{
public:
	CTKeyDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK
	void OnCancel();
										// Cancel
	int m_nTarget;
										// Target key
	int m_nKey;
										// Key assignment

protected:
	afx_msg void OnPaint();
										// Draw
	afx_msg UINT OnGetDlgCode();
										// Get dialog code
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
										// Timer
#else
	afx_msg void OnTimer(UINT nTimerID);
										// Timer
#endif
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
										// Right click

private:
	void FASTCALL OnDraw(CDC *pDC);
										// Main draw
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
										// Timer ID
#else
	UINT m_nTimerID;
										// Timer ID
#endif
	BYTE m_KeyState[0x100];
										// VK key state
	CTKey *m_pTKey;
										// TrueKey
	CRect m_rectGuide;
										// Guide rectangle
	CString m_strGuide;
										// Guide string
	CRect m_rectAssign;
										// Assignment rectangle
	CString m_strAssign;
										// Assignment string
	CRect m_rectKey;
										// Key rectangle
	CString m_strKey;
										// Key string

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	TrueKey page
//
//===========================================================================
class CTKeyPage : public CConfigPage
{
public:
	CTKeyPage();
										// Constructor
	BOOL OnInitDialog();
										// Initialization
	void OnOK();
										// OK

protected:
	afx_msg void OnSelChange();
										// Combo box change
	afx_msg void OnClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Column click
	afx_msg void OnRClick(NMHDR *pNMHDR, LRESULT *pResult);
										// Column right click

private:
	void FASTCALL EnableControls(BOOL bEnable);
										// Control state change
	void FASTCALL UpdateReport();
										// Report update
	BOOL m_bEnableCtrl;
										// Control enable flag
	CInput *m_pInput;
										// CInput
	CTKey *m_pTKey;
										// TrueKey
	int m_nKey[0x73];
										// Edit conversion table
	static const UINT ControlTable[];
										// Control table

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//	Misc page
//
//===========================================================================
class CMiscPage : public CConfigPage
{
public:
	CMiscPage();
	BOOL OnInitDialog();
	void OnBuscarFolder();
	void OnOK();
	void DoDataExchange(CDataExchange *pDX);
										// Data exchange

	int m_nRendererDefault;

DECLARE_MESSAGE_MAP()
};

//===========================================================================
//
//	Configuration property sheet
//
//===========================================================================
class CConfigSheet : public CPropertySheet
{
public:
	CConfigSheet(CWnd *pParent);
										// Constructor
	Config *m_pConfig;
										// Configuration data
	CConfigPage* FASTCALL SearchPage(DWORD dwID) const;
										// Page search

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Window create
	afx_msg void OnDestroy();
										// Window delete
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
#else
	afx_msg void OnTimer(UINT nTimerID);
#endif
										// Timer

private:
	CFrmWnd *m_pFrmWnd;
										// Frame window
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
#else
	UINT m_nTimerID;
#endif
										// Timer ID

	CBasicPage m_Basic;
										// Basic
	CSoundPage m_Sound;
										// Sound
	CVolPage m_Vol;
										// Volume
	CKbdPage m_Kbd;
										// Keyboard
	CMousePage m_Mouse;
										// Mouse
	CJoyPage m_Joy;
										// Joystick
	CSASIPage m_SASI;
										// SASI
	CSxSIPage m_SxSI;
										// SxSI
	CSCSIPage m_SCSI;
										// SCSI
	CPortPage m_Port;
										// Ports
	CMIDIPage m_MIDI;
										// MIDI
	CAlterPage m_Alter;
										// Alteration
	CResumePage m_Resume;
										// Resume
	CTKeyPage m_TKey;
										// TrueKey
	CMiscPage m_Misc;
										// Misc

	DECLARE_MESSAGE_MAP()
										// With message map
};

#endif	// mfc_cfg_h
#endif	// _WIN32
