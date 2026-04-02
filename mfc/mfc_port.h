//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 š°å•½D(ytanaka@ipc-tokai.or.jp)
//	[ MFC Port ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_port_h)
#define mfc_port_h

#include "fileio.h"
#include "mfc_que.h"

//===========================================================================
//
//	Port
//
//===========================================================================
class CPort : public CComponent
{
public:
	// Basic procedures
	CPort(CFrmWnd *pWnd);
										// Constructor
	BOOL FASTCALL Init();
										// Initialization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL ApplyCfg(const Config *pConfig);
										// Apply configuration

	// External API
	void FASTCALL RunCOM();
										// COM execution
	BOOL FASTCALL GetCOMInfo(LPTSTR lpszDevFile, DWORD *dwLogFile) const;
										// Get COM info
	void FASTCALL GetTxQueue(CQueue::LPQUEUEINFO lpqi) const;
										// Get Tx queue
	void FASTCALL GetRxQueue(CQueue::LPQUEUEINFO lpqi) const;
										// Get Rx queue
	void FASTCALL RunLPT();
										// LPT execution
	BOOL FASTCALL GetLPTInfo(LPTSTR lpszDevFile, DWORD *dwLogFile) const;
										// Get LPT info
	void FASTCALL GetLPTQueue(CQueue::LPQUEUEINFO lpqi) const;
										// Get LPT queue

private:
	// Serial port
	BOOL FASTCALL OpenCOM();
										// COM open
	void FASTCALL AppendCOM();
										// COM file
	void FASTCALL CloseCOM();
										// COM close
	void FASTCALL AdjustCOM(DCB *pDCB);
										// Adjust parameters
	BOOL FASTCALL MatchCOM(DWORD dwSCC, DWORD dwBase);
										// Baudrate match
	void FASTCALL SignalCOM();
										// Signal handling
	void FASTCALL BufCOM();
										// Buffer handling
	void FASTCALL CtrlCOM();
										// Control
	void FASTCALL OnCTSDSR();
										// CTS, DSR change
	void FASTCALL OnErr();
										// Rx error
	void FASTCALL OnRx();
										// Rx subroutine
	void FASTCALL OnTx();
										// Tx subroutine
	UINT m_nCOM;
										// COM port (0 means not used)
	HANDLE m_hCOM;
										// File handle
	CWinThread *m_pCOM;
										// Thread
	BOOL m_bCOMReq;
										// Thread exit request
	BOOL m_bBreak;
										// X68000 break signal
	BOOL m_bRTS;
										// RTS (Tx)
	BOOL m_bDTR;
										// DTR (Tx)
	BOOL m_bCTS;
										// CTS (Rx)
	BOOL m_bDSR;
										// DSR (Rx)
	OVERLAPPED m_TxOver;
										// Overlapped
	CQueue m_TxQueue;
										// Tx queue
	BYTE m_TxBuf[0x1000];
										// Tx overlapped buffer
	BOOL m_bTxValid;
										// Tx valid flag
	OVERLAPPED m_RxOver;
										// Overlapped
	CQueue m_RxQueue;
										// Rx queue
	BYTE m_RxBuf[0x1000];
										// Rx overlapped buffer
	DWORD m_dwErr;
										// Rx error
	TCHAR m_RecvLog[_MAX_PATH];
										// Rx log
	Fileio m_RecvFile;
										// Rx log
	BOOL m_bForce;
										// Force 38400bps mode
	SCC *m_pSCC;
										// SCC

	// Parallel port
	BOOL FASTCALL OpenLPT();
										// LPT open
	void FASTCALL AppendLPT();
										// LPT file
	void FASTCALL CloseLPT();
										// LPT close
	void FASTCALL RecvLPT();
										// LPT receive
	void FASTCALL SendLPT();
										// LPT send

	UINT m_nLPT;
										// LPT port (0 means not used)
	HANDLE m_hLPT;
										// File handle
	CWinThread *m_pLPT;
										// Thread
	BOOL m_bLPTReq;
										// Thread exit request
	BOOL m_bLPTValid;
										// Tx valid flag
	OVERLAPPED m_LPTOver;
										// Overlapped
	CQueue m_LPTQueue;
										// Queue
	TCHAR m_SendLog[_MAX_PATH];
										// Tx log
	Fileio m_SendFile;
										// Tx log
	Printer *m_pPrinter;
										// Printer
};

#endif	// mfc_port_h
#endif	// _WIN32
