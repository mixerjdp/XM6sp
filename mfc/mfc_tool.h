//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 PI.(ytanaka@ipc-tokai.or.jp)
//	[ MFC Tools ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined (mfc_tool_h)
#define mfc_tool_h

//===========================================================================
//
//	Floppy disk image creation dialog
//
//===========================================================================
class CFDIDlg : public CDialog
{
public:
	CFDIDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Dialog initialization
	void OnOK();
										// Dialog OK
	void OnCancel();
										// Dialog cancel

	TCHAR m_szFileName[_MAX_PATH];
										// Filename
	TCHAR m_szDiskName[60];
										// Disk name
	DWORD m_dwType;
										// File type (2HD,DIM,D68)
	DWORD m_dwPhysical;
										// Physical format
	BOOL m_bLogical;
										// Logical format
	int m_nDrive;
										// Drive (-1 means no mount)

protected:
	afx_msg void OnBrowse();
										// File selection
	afx_msg void OnPhysical();
										// Physical format radio

private:
	void FASTCALL MaskName();
										// Disk name mask
	void FASTCALL SetPhysical();
										// Physical format set
	void FASTCALL GetPhysical();
										// Physical format get
	void FASTCALL MaskPhysical();
										// Physical format mask
	void FASTCALL SetLogical();
										// Logical format set
	void FASTCALL GetLogical();
										// Logical format get
	void FASTCALL MaskLogical();
										// Logical format mask
	static const DWORD IDTable[16];
										// Physical format ID table
	BOOL FASTCALL GetFile();
										// Get filename

	DECLARE_MESSAGE_MAP()
										// Message map
};

//===========================================================================
//
//	Hard disk image creation dialog
//
//===========================================================================
class CDiskDlg : public CDialog
{
public:
	CDiskDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Dialog initialization
	void OnOK();
										// Dialog OK
	BOOL FASTCALL IsSucceeded() const	{ return m_bSucceed; }
										// Creation succeeded flag
	BOOL FASTCALL IsCanceled() const	{ return m_bCancel; }
										// Canceled flag
	LPCTSTR FASTCALL GetPath() const	{ return m_szPath; }
										// Get path
	int m_nType;
										// Type (0:SASI-HD 1:SCSI-HD 2:SCSI-MO)
	BOOL m_bMount;
										// Mount flag (SCSI-MO only)

protected:
	afx_msg void OnBrowse();
										// File selection
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
										// Vertical scroll
	afx_msg void OnMOSize();
										// MO size change

private:
	void FASTCALL CtrlSASI();
										// SASI-HD control
	void FASTCALL CtrlSCSI();
										// SCSI-HD control
	void FASTCALL CtrlMO();
										// SCSI-MO control
	BOOL FASTCALL GetFile();
										// File selection
	void FASTCALL CreateSASI();
										// SASI-HD create
	void FASTCALL CreateSCSI();
										// SCSI-HD create
	void FASTCALL CreateMO();
										// SCSI-MO create

	TCHAR m_szPath[_MAX_PATH];
										// Path
	BOOL m_bSucceed;
										// Creation succeeded flag
	BOOL m_bCancel;
										// Canceled flag
	UINT m_nSize;
										// Disk size (MB)
	UINT m_nFormat;
										// Logical format
	static const UINT SASITable[];
										// SASI-HD ID table
	static const UINT SCSITable[];
										// SCSI-HD ID table
	static const UINT MOTable[];
										// SCSI-MO ID table

	DECLARE_MESSAGE_MAP()
										// Message map
};

//===========================================================================
//
//	Disk image creation dialog
//
//===========================================================================
class CDiskMakeDlg : public CDialog
{
public:
	// Basic methods
	CDiskMakeDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Dialog initialization
	void OnOK();
										// Dialog OK
	void OnCancel();
										// Dialog cancel

	// API
	BOOL FASTCALL IsSucceeded() const	{ return m_bSucceed; }
										// Creation succeeded flag
	BOOL FASTCALL IsCanceled() const	{ return m_bCancel; }
										// Canceled flag
	static UINT ThreadFunc(LPVOID lpParam);
										// Thread function
	void FASTCALL Run();
										// Thread execution

	// Parameters
	DWORD m_dwSize;
										// Disk size
	TCHAR m_szPath[_MAX_PATH];
										// Path
	int m_nFormat;
										// Format type

protected:
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
#else
	afx_msg void OnTimer(UINT nTimerID);
#endif
										// Timer
	afx_msg void OnDestroy();
										// Dialog destruction
	virtual BOOL FASTCALL Format();
										// Format
	DWORD m_dwCurrent;
										// Creation progress

private:
	CWinThread *m_pThread;
										// Creation thread
	BOOL m_bThread;
										// Thread termination flag
	CCriticalSection m_CSection;
										// Critical section
	BYTE *m_pBuffer;
										// Buffer
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
#else
	UINT m_nTimerID;
#endif
										// Timer ID
	DWORD m_dwParcent;
										// Progress percent
	BOOL m_bSucceed;
										// Success flag
	BOOL m_bCancel;
										// Cancel flag

	DECLARE_MESSAGE_MAP()
										// Message map
};

//===========================================================================
//
//	SASI disk image creation dialog
//
//===========================================================================
class CSASIMakeDlg : public CDiskMakeDlg
{
public:
	CSASIMakeDlg(CWnd *pParent);
										// Constructor

protected:
	BOOL FASTCALL Format();
										// Format

private:
	static const BYTE MENU[];
										// Menu
	static const BYTE IPL[];
										// IPL
};

//===========================================================================
//
//	SCSI disk image creation dialog
//
//===========================================================================
class CSCSIMakeDlg : public CDiskMakeDlg
{
public:
	CSCSIMakeDlg(CWnd *pParent);
										// Constructor
};

//===========================================================================
//
//	MO disk image creation dialog
//
//===========================================================================
class CMOMakeDlg : public CDiskMakeDlg
{
public:
	CMOMakeDlg(CWnd *pParent);
										// Constructor

protected:
	BOOL FASTCALL Format();
										// Format

private:
	int m_nMedia;
										// Media type (0:128MB...3:640MB)
	BOOL FASTCALL FormatIBM();
										// IBM format
	void FASTCALL MakeBPB(BYTE *pBPB);
										// IBM format BPB create
	void FASTCALL MakeSerial(BYTE *pSerial);
										// Serial number create
	BOOL FASTCALL FormatSHARP();
										// SHARP format
	static const BYTE PartTable128[];
										// Partition table (128MB)
	static const BYTE PartTable230[];
										// Partition table (230MB)
	static const BYTE PartTable540[];
										// Partition table (540MB)
	static const BYTE PartTop128[];
										// Partition top (128MB)
	static const BYTE PartTop230[];
										// Partition top (230MB)
	static const BYTE PartTop540[];
										// Partition top (540MB)
	static const BYTE SCSIMENU[];
										// SCSI partition menu
	static const BYTE SCHDISK[];
										// SCSI disk header
	static const BYTE SCSIIOCS[];
										// SCSIIOCS tool header
	static const BYTE IPL[];
										// Human68k IPL
};

//===========================================================================
//
//	trap #0 dialog
//
//===========================================================================
class CTrapDlg : public CDialog
{
public:
	CTrapDlg(CWnd *pParent);
										// Constructor
	BOOL OnInitDialog();
										// Dialog initialization
	void OnOK();
										// Dialog OK
	DWORD m_dwCode;
										// Code value

protected:
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Vertical scroll

	DECLARE_MESSAGE_MAP()
										// Message map
};

#endif	// mfc_tool_h
#endif	// _WIN32
