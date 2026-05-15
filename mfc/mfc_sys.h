//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC Sub-Window (System) ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_sys_h)
#define mfc_sys_h

#include "mfc_sub.h"
#include "log.h"

//===========================================================================
//
//	Log Window
//
//===========================================================================
class CLogWnd : public CSubListWnd
{
public:
	CLogWnd();
										// Constructor
	void FASTCALL Refresh();
										// Refresh display
	void FASTCALL Update();
										// Update message thread

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpcs);
										// Create window
	afx_msg void OnDblClick(NMHDR *pNotifyStruct, LRESULT *result);
										// Double click
	afx_msg void OnDrawItem(int nID, LPDRAWITEMSTRUCT lpdis);
										// Draw item
	void FASTCALL InitList();
										// Initialize list control

private:
	void FASTCALL TextSub(int nType, Log::logdata_t *pLogData, CString& string);
										// Format text
	Log *m_pLog;
										// Log
	CString m_strDetail;
										// Detail
	CString m_strNormal;
										// Normal
	CString m_strWarning;
										// Warning
	int m_nFocus;
										// Focus number

	DECLARE_MESSAGE_MAP()
										// Message map
};

//===========================================================================
//
//	Scheduler Window
//
//===========================================================================
class CSchedulerWnd : public CSubTextWnd
{
public:
	CSchedulerWnd();
										// Constructor
	void FASTCALL Setup();
										// Setup
	void FASTCALL Update();
										// Update message thread

private:
	Scheduler *m_pScheduler;
										// Scheduler
};

//===========================================================================
//
//	Device Window
//
//===========================================================================
class CDeviceWnd : public CSubTextWnd
{
public:
	CDeviceWnd();
										// Constructor
	void FASTCALL Setup();
										// Setup
};

#endif	// mfc_sys_h
#endif	// _WIN32
