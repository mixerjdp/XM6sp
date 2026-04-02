//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Copyright (C) 2001-2005 PI.(ytanaka@ipc-tokai.or.jp)
//	[MFC subwindow (CPU)]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_cpu_h)
#define mfc_cpu_h

#include "mfc_sub.h"

//===========================================================================
//
//	History dialog
//
//===========================================================================
class CHistoryDlg : public CDialog
{
public:
	CHistoryDlg(UINT nID, CWnd *pParentWnd);
										// Constructor
	BOOL OnInitDialog();
										// Dialog initialization
	void OnOK();
										// OK
	DWORD m_dwValue;
										// Edit value

protected:
	virtual UINT* GetNumPtr() = 0;
										// Get history count pointer
	virtual DWORD* GetDataPtr() = 0;
										// Get history data pointer
	UINT m_nBit;
										// Valid bits
	DWORD m_dwMask;
										// Mask (generated internally from the bit count)
};

//===========================================================================
//
//	Address input dialog
//
//===========================================================================
class CAddrDlg : public CHistoryDlg
{
public:
	CAddrDlg(CWnd *pParent = NULL);
										// Constructor
	static void SetupHisMenu(CMenu *pMenu);
										// Menu configuration
	static DWORD GetAddr(UINT nID);
										// Get menu result

protected:
	UINT* GetNumPtr();
										// Get history count pointer
	DWORD* GetDataPtr();
										// Get history data pointer
	static UINT m_Num;
										// History count
	static DWORD m_Data[10];
										// History data
};

//===========================================================================
//
//	Register input dialog
//
//===========================================================================
class CRegDlg : public CHistoryDlg
{
public:
	CRegDlg(CWnd *pParent = NULL);
										// Constructor
	BOOL OnInitDialog();
										// Dialog initialization
	void OnOK();
										// OK
	UINT m_nIndex;
										// Register index

protected:
	UINT* GetNumPtr();
										// Get history count pointer
	DWORD* GetDataPtr();
										// Get history data pointer
	static UINT m_Num;
										// History count
	static DWORD m_Data[10];
										// History data
};

//===========================================================================
//
//	Data input dialog
//
//===========================================================================
class CDataDlg : public CHistoryDlg
{
public:
	CDataDlg(CWnd *pParent = NULL);
										// Constructor
	BOOL OnInitDialog();
										// Dialog initialization
	UINT m_nSize;
										// size
	DWORD m_dwAddr;
										// Address

protected:
	UINT* GetNumPtr();
										// Get history count pointer
	DWORD* GetDataPtr();
										// Get history data pointer
	static UINT m_Num;
										// History count
	static DWORD m_Data[10];
										// History data
};

//===========================================================================
//
//	MPU register window
//
//===========================================================================
class CCPURegWnd : public CSubTextWnd
{
public:
	CCPURegWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration
	static void SetupRegMenu(CMenu *pMenu, CPU *pCPU, BOOL bSR);
										// Menu configuration
	static DWORD GetRegValue(CPU *pCPU, UINT uID);
										// Get register value

protected:
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
										// Context menu
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
										// Left double-click
	afx_msg void OnReg(UINT nID);
										// Register selection

private:
	CPU *m_pCPU;
										// CPU

	DECLARE_MESSAGE_MAP()
										// with message map
};

//===========================================================================
//
//	Interrupt window
//
//===========================================================================
class CIntWnd : public CSubTextWnd
{
public:
	CIntWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

protected:
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
										// Left double-click
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
										// Context menu

private:
	CPU* m_pCPU;
										// CPU

	DECLARE_MESSAGE_MAP()
										// with message map
};

//===========================================================================
//
//	Disassembly window
//
//===========================================================================
class CDisasmWnd : public CSubTextScrlWnd
{
public:
	CDisasmWnd(int index);
										// Constructor
	void FASTCALL Setup();
										// Configuration
	void FASTCALL SetAddr(DWORD dwAddr);
										// Address specification
	void FASTCALL SetPC(DWORD pc);
										// PC specification
	void FASTCALL Update();
										// Update from the message thread
	static void FASTCALL SetupBreakMenu(CMenu *pMenu, Scheduler *pScheduler);
										// Breakpoint menu configuration
	static DWORD FASTCALL GetBreak(UINT nID, Scheduler *pScheduler);
										// Get breakpoint

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpcs);
										// Window creation
	afx_msg void OnDestroy();
										// Window destruction
	afx_msg void OnSize(UINT nType, int cx, int cy);
										// Resize
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
										// Left click
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
										// Left double-click
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// scroll vertical
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
										// Context menu
	afx_msg void OnNewWin();
										// New window
	afx_msg void OnPC();
										// Move to PC
	afx_msg void OnSync();
										// Sync with PC
	afx_msg void OnAddr();
										// Address input
	afx_msg void OnReg(UINT nID);
										// Register
	afx_msg void OnStack(UINT nID);
										// Stack
	afx_msg void OnBreak(UINT nID);
										// Breakpoint
	afx_msg void OnHistory(UINT nID);
										// Address history
	afx_msg void OnCPUExcept(UINT nID);
										// CPU exception vector
	afx_msg void OnTrap(UINT nID);
										// trap vector
	afx_msg void OnMFP(UINT nID);
										// MFP vector
	afx_msg void OnSCC(UINT nID);
										// SCC vector
	afx_msg void OnDMAC(UINT uID);
										// DMAC vector
	afx_msg void OnIOSC(UINT uID);
										// IOSC vector

private:
	DWORD FASTCALL GetPrevAddr(DWORD dwAddr);
										// Get previous address
	void FASTCALL SetupContext(CMenu *pMenu);
										// Context menu configuration
	void FASTCALL SetupVector(CMenu *pMenu, UINT index, DWORD vector, int num);
										// Interrupt vector configuration
	void FASTCALL SetupAddress(CMenu *pMenu, UINT index, DWORD addr);
										// Address configuration
	void FASTCALL OnVector(UINT vector);
										// Vector specification
	CPU *m_pCPU;
										// CPU
	Scheduler *m_pScheduler;
										// Scheduler (Scheduler)
	MFP *m_pMFP;
										// MFP
	Memory *m_pMemory;
										// memory
	SCC * m_pSCC;
										// SCC
	DMAC *m_pDMAC;
										// DMAC
	IOSC *m_pIOSC;
										// IOSC
	BOOL m_bSync;
										// PC sync flag
	DWORD m_dwPC;
										// PC
	DWORD m_dwAddr;
										// Display start address
	DWORD m_dwSetAddr;
										// Set address
	DWORD *m_pAddrBuf;
										// Address buffer
	CString m_Caption;
										// Caption string
	CString m_CaptionSet;
										// Caption configuration string

	DECLARE_MESSAGE_MAP()
										// with message map
};

//===========================================================================
//
//	Memory window
//
//===========================================================================
class CMemoryWnd : public CSubTextScrlWnd
{
public:
	CMemoryWnd(int nWnd);
										// Constructor
	void FASTCALL Setup();
										// Configuration
	void FASTCALL SetAddr(DWORD dwAddr);
										// Address specification
	void FASTCALL SetUnit(int nUnit);
										// Display unit specification
	void FASTCALL Update();
										// Update from the message thread
	static void SetupStackMenu(CMenu *pMenu, Memory *pMemory, CPU *pCPU);
										// Stack menu configuration
	static DWORD GetStackAddr(UINT nID, Memory *pMemory, CPU *pCPU);
										// Get stack

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Window creation
	afx_msg void OnPaint();
										// Drawing
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
										// Left double-click
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
										// Context menu
	afx_msg void OnAddr();
										// Address input
	afx_msg void OnNewWin();
										// New window
	afx_msg void OnUnit(UINT uID);
										// Display unit specification
	afx_msg void OnRange(UINT uID);
										// Address range specification
	afx_msg void OnReg(UINT uID);
										// Specify a register value
	afx_msg void OnArea(UINT uID);
										// Area specification
	afx_msg void OnHistory(UINT uID);
										// Address history
	afx_msg void OnStack(UINT uID);
										// Stack
	void FASTCALL SetupScrlV();
										// Prepare vertical scrolling

private:
	void FASTCALL SetupContext(CMenu *pMenu);
										// Context menu configuration
	Memory *m_pMemory;
										// memory
	CPU *m_pCPU;
										// CPU
	DWORD m_dwAddr;
										// Display start address
	CString m_strCaptionReq;
										// Caption string (request)
	CString m_strCaptionSet;
										// Caption string (configuration)
	CCriticalSection m_CSection;
										// Seccion critica
	UINT m_nUnit;
										// Display size 0/1/2=Byte/Word/Long

	DECLARE_MESSAGE_MAP()
										// with message map
};

//===========================================================================
//
//	Breakpoint window
//
//===========================================================================
class CBreakPWnd : public CSubTextWnd
{
public:
	CBreakPWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

protected:
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
										// Left double-click
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
										// Context menu
	afx_msg void OnEnable();
										// Enable/disable
	afx_msg void OnClear();
										// Clear count
	afx_msg void OnDel();
										// Delete
	afx_msg void OnAddr();
										// Address specification
	afx_msg void OnAll();
										// Delete all
	afx_msg void OnHistory(UINT nID);
										// Address history

private:
	void FASTCALL SetupContext(CMenu *pMenu);
										// Context menu configuration
	void FASTCALL SetAddr(DWORD dwAddr);
										// Set address
	Scheduler* m_pScheduler;
										// Scheduler (Scheduler)
	CPoint m_Point;
										// Context menu item

	DECLARE_MESSAGE_MAP()
										// with message map
};

#endif	// mfc_cpu_h
#endif	// _WIN32
