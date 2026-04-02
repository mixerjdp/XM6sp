//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC Port ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "scc.h"
#include "printer.h"
#include "config.h"
#include "filepath.h"
#include "fileio.h"
#include "mfc_com.h"
#include "mfc_cfg.h"
#include "mfc_que.h"
#include "mfc_port.h"

//===========================================================================
//
//	Port
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	COM thread function
//
//---------------------------------------------------------------------------
static UINT COMThread(LPVOID pParam)
{
	CPort *pPort;

	// Receive parameter
	pPort = (CPort*)pParam;
	ASSERT(pPort);

	// Execute
	pPort->RunCOM();

	// Return code terminates thread
	return 0;
}

//---------------------------------------------------------------------------
//
//	LPT thread function
//
//---------------------------------------------------------------------------
static UINT LPTThread(LPVOID pParam)
{
	CPort *pPort;

	// Receive parameter
	pPort = (CPort*)pParam;
	ASSERT(pPort);

	// Execute
	pPort->RunLPT();

	// Return code terminates thread
	return 0;
}

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CPort::CPort(CFrmWnd *pWnd) : CComponent(pWnd)
{
	// Component identifier
	m_dwID = MAKEID('P', 'O', 'R', 'T');
	m_strDesc = _T("Port Handler");

	// Serial port
	m_nCOM = 0;
	m_hCOM = INVALID_HANDLE_VALUE;
	m_pCOM = NULL;
	m_bCOMReq = FALSE;
	m_bBreak = FALSE;
	m_bRTS = FALSE;
	m_bDTR = FALSE;
	m_bCTS = FALSE;
	m_bDSR = FALSE;
	m_bTxValid = FALSE;
	memset(&m_TxOver, 0, sizeof(m_TxOver));
	m_dwErr = 0;
	memset(&m_RxOver, 0, sizeof(m_RxOver));
	m_RecvLog[0] = _T('\0');
	m_bForce = FALSE;
	m_pSCC = NULL;

	// Parallel port
	m_nLPT = 0;
	m_hLPT = INVALID_HANDLE_VALUE;
	m_pLPT = NULL;
	m_bLPTValid = FALSE;
	m_bLPTReq = FALSE;
	memset(&m_LPTOver, 0, sizeof(m_LPTOver));
	m_SendLog[0] = _T('\0');
	m_pPrinter = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPort::Init()
{
	ASSERT(this);

	// Base class
	if (!CComponent::Init()) {
		return FALSE;
	}

	// Get SCC
	ASSERT(!m_pSCC);
	m_pSCC = (SCC*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'C', ' '));
	ASSERT(m_pSCC);

	// Serial queue initialization
	if (!m_TxQueue.Init(0x1000)) {
		return FALSE;
	}
	if (!m_RxQueue.Init(0x1000)) {
		return FALSE;
	}

	// Serial port
	OpenCOM();

	// Get printer
	ASSERT(!m_pPrinter);
	m_pPrinter = (Printer*)::GetVM()->SearchDevice(MAKEID('P', 'R', 'N', ' '));
	ASSERT(m_pPrinter);

	// Parallel queue initialization
	if (!m_LPTQueue.Init(0x1000)) {
		return FALSE;
	}

	// Parallel port
	OpenLPT();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CPort::Cleanup()
{
	ASSERT(this);

	// Parallel port
	CloseLPT();

	// Serial port
	CloseCOM();

	// Base class
	CComponent::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL CPort::ApplyCfg(const Config* pConfig)
{
	BOOL bCOM;
	BOOL bLPT;

	ASSERT(this);
	ASSERT(pConfig);

	// COM flag initialization
	bCOM = FALSE;

	// COM port number has changed
	if (pConfig->port_com != (int)m_nCOM) {
		m_nCOM = pConfig->port_com;
		bCOM = TRUE;

		// If TrueKey and port number are same, disable port, TrueKey has priority
		if (pConfig->port_com == pConfig->tkey_com) {
			m_nCOM = 0;
		}
	}

	// Receive log file has changed
	if (_tcscmp(pConfig->port_recvlog, m_RecvLog) != 0) {
		ASSERT(_tcslen(pConfig->port_recvlog) < _MAX_PATH);
		_tcscpy(m_RecvLog, pConfig->port_recvlog);
		bCOM = TRUE;
	}

	// Force flag has changed
	if (pConfig->port_384 != m_bForce) {
		m_bForce = pConfig->port_384;
		bCOM = TRUE;
	}

	// If change detected
	if (bCOM) {
		// For cleanup, temporarily release locked VM
		::UnlockVM();
		CloseCOM();
		OpenCOM();
		::LockVM();
	}

	// LPT flag initialization
	bLPT = FALSE;

	// LPT port number has changed
	if (pConfig->port_lpt != (int)m_nLPT) {
		m_nLPT = pConfig->port_lpt;
		bLPT = TRUE;
	}

	// Send log file has changed
	if (strcmp(pConfig->port_sendlog, m_SendLog) != 0) {
		ASSERT(strlen(pConfig->port_sendlog) < sizeof(m_SendLog));
		strcpy(m_SendLog, pConfig->port_sendlog);
		bLPT = TRUE;
	}

	// If change detected
	if (bLPT) {
		// For cleanup, temporarily release locked VM
		::UnlockVM();
		CloseLPT();
		OpenLPT();
		::LockVM();
	}
}

//===========================================================================
//
//	Serial port
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	COM open
//	Note VM is locked
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPort::OpenCOM()
{
	CString strFile;
	DCB dcb;

	ASSERT(this);

	// Port 0 means not assigned
	if (m_nCOM == 0) {
		m_hCOM = INVALID_HANDLE_VALUE;
		return TRUE;
	}

	// Create file name
	strFile.Format(_T("\\\\.\\COM%d"), m_nCOM);

	// Open
	m_hCOM = ::CreateFile(  strFile,
							GENERIC_READ | GENERIC_WRITE,
							0,
							NULL,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
							0);
	if (m_hCOM == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	// Buffer size
	::SetupComm(m_hCOM, 0x1000, 0x1000);

	// Clear send/receive
	m_TxQueue.Clear();
	m_RxQueue.Clear();
	::PurgeComm(m_hCOM, PURGE_TXCLEAR);
	::PurgeComm(m_hCOM, PURGE_RXCLEAR);

	// Set send/receive settings
	OnErr();
	OnCTSDSR();
	::GetCommState(m_hCOM, &dcb);
	::LockVM();
	AdjustCOM(&dcb);
	SignalCOM();
	::UnlockVM();
	CtrlCOM();

	// Create log file (append)
	AppendCOM();

	// Start thread
	m_bCOMReq = FALSE;
	m_pCOM = AfxBeginThread(COMThread, this);
	if (!m_pCOM) {
		CloseCOM();
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	COM log file
//
//---------------------------------------------------------------------------
void FASTCALL CPort::AppendCOM()
{
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	Filepath path;

	ASSERT(this);

	// If log file is not specified, return
	if (_tcslen(m_RecvLog) == 0) {
		return;
	}

	// Parse
	_tsplitpath(m_RecvLog, szDrive, szDir, szFile, szExt);

	// If no drive path, use relative path
	path.SetPath(m_RecvLog);
	if ((_tcslen(szDrive) == 0) && (szDir[0] != _T('\\'))) {
		path.SetBaseDir();
	}

	// Append open
	m_RecvFile.Open(path, Fileio::Append);
}

//---------------------------------------------------------------------------
//
//	COM close
//	Note VM is locked
//
//---------------------------------------------------------------------------
void FASTCALL CPort::CloseCOM()
{
	DWORD dwCompleted;

	ASSERT(this);

	// Stop thread
	if (m_pCOM) {
		// Set exit flag
		m_bCOMReq = TRUE;

		// Wait for exit (infinite)
		::WaitForSingleObject(m_pCOM->m_hThread, INFINITE);

		m_pCOM = NULL;
	}

	// Close handle
	if (m_hCOM != INVALID_HANDLE_VALUE) {
		// Buffer purge
		m_TxQueue.Clear();
		m_RxQueue.Clear();
		::PurgeComm(m_hCOM, PURGE_TXCLEAR);
		::PurgeComm(m_hCOM, PURGE_RXCLEAR);

		// Send
		if (m_bTxValid) {
			// If overlapped I/O is in progress
			if (!GetOverlappedResult(m_hCOM, &m_TxOver, &dwCompleted, FALSE)) {
				// Still pending, so cancel
				CancelIo(m_hCOM);
			}
			m_bTxValid = FALSE;
		}

		// If overlapped I/O is in progress
		if (!GetOverlappedResult(m_hCOM, &m_RxOver, &dwCompleted, FALSE)) {
			// Still pending, so cancel
			CancelIo(m_hCOM);
		}

		// Close
		::CloseHandle(m_hCOM);
		m_hCOM = INVALID_HANDLE_VALUE;
	}

	// Log file close
	m_RecvFile.Close();

	// To SCC, if send/wait is not complete
	m_pSCC->WaitTx(0, FALSE);
	m_pSCC->SetCTS(0, FALSE);
	m_pSCC->SetDCD(0, FALSE);
}

//---------------------------------------------------------------------------
//
//	COM execution
//
//---------------------------------------------------------------------------
void FASTCALL CPort::RunCOM()
{
	COMMTIMEOUTS cto;
	DCB dcb;

	ASSERT(this);
	ASSERT(m_hCOM);

	// Event mask
	::SetCommMask(m_hCOM, 0);

	// Timeout
	::GetCommTimeouts(m_hCOM, &cto);
	cto.ReadIntervalTimeout = 1;
	cto.ReadTotalTimeoutMultiplier = 0;
	cto.ReadTotalTimeoutConstant = 10000;
	cto.WriteTotalTimeoutMultiplier = 0;
	cto.WriteTotalTimeoutConstant = 10000;
	::SetCommTimeouts(m_hCOM, &cto);

	// Receive wait
	memset(&m_RxOver, 0, sizeof(m_RxOver));
	::ReadFile(m_hCOM, m_RxBuf, sizeof(m_RxBuf), NULL, &m_RxOver);

	// Not sending
	m_bTxValid = FALSE;
	memset(&m_TxOver, 0, sizeof(m_TxOver));

	// Until exit flag is set
	while (!m_bCOMReq) {
		// Sleep
		::Sleep(5);

		// CTS, DSR
		OnCTSDSR();

		// Error
		OnErr();

		// Receive
		OnRx();

		// Get Win32 status
		::GetCommState(m_hCOM, &dcb);

		// Lock VM
		::LockVM();

		// Adjust
		AdjustCOM(&dcb);
		SignalCOM();
		BufCOM();

		// Unlock VM
		::UnlockVM();

		// Send control
		CtrlCOM();

		// Send
		OnTx();
	}
}

//---------------------------------------------------------------------------
//
//	Adjust parameters
//
//---------------------------------------------------------------------------
void FASTCALL CPort::AdjustCOM(DCB *pDCB)
{
	DCB dcb;
	const SCC::scc_t *scc;
	const SCC::ch_t *p;
	int i;
	BOOL bFlag;
	DWORD dwValue;
	static DWORD dwTable[] = {
		CBR_110,
		CBR_300,
		CBR_600,
		CBR_1200,
		CBR_2400,
		CBR_4800,
		CBR_9600,
		CBR_14400,
		CBR_19200,
		31250,
		CBR_38400,
		CBR_57600,
		CBR_115200,
		0
	};

	ASSERT(this);
	ASSERT(m_pSCC);
	ASSERT(m_hCOM);
	ASSERT(pDCB);

	// Change flag clear
	bFlag = FALSE;

	// Get SCC data
	scc = m_pSCC->GetWork();
	p = &scc->ch[0];

	// Get Win32 data
	memcpy(&dcb, pDCB, sizeof(dcb));

	// Baudrate
	dwValue = CBR_300;
	for (i=0;; i++) {
		// End if table ends at 300bps
		if (dwTable[i] == 0) {
			break;
		}
		// Match with 5% margin
		if (MatchCOM(p->baudrate, dwTable[i])) {
			dwValue = dwTable[i];
			break;
		}
	}

	// Force 38400bps
	if (m_bForce) {
		dwValue = CBR_38400;
	}

	// Compare
	if (dcb.BaudRate != dwValue) {
		dcb.BaudRate = dwValue;
		bFlag = TRUE;
	}

	// Binary mode
	dcb.fBinary = 0;

	// Parity setting
	if (p->parity == 0) {
		dwValue = 0;
	}
	else {
		dwValue = 1;
	}
	if (dcb.fParity != dwValue) {
		dcb.fParity = dwValue;
	}

	// CTS filter
	if (dcb.fOutxCtsFlow != 0) {
		dcb.fOutxCtsFlow = 0;
		bFlag = TRUE;
	}

	// DSR filter
	if (dcb.fOutxDsrFlow != 0) {
		dcb.fOutxDsrFlow = 0;
		bFlag = TRUE;
	}

	// DTR control
	if (dcb.fDtrControl != DTR_CONTROL_ENABLE) {
		dcb.fDtrControl = DTR_CONTROL_ENABLE;
		bFlag = TRUE;
	}

	// DSR sensitivity
	if (dcb.fDsrSensitivity != 0) {
		dcb.fDsrSensitivity = 0;
		bFlag = TRUE;
	}

	// XON,XOFF
	if (dcb.fTXContinueOnXoff != 0) {
		dcb.fTXContinueOnXoff = 0;
		bFlag = TRUE;
	}

	// OutX
	if (dcb.fOutX != 0) {
		dcb.fOutX = 0;
		bFlag = TRUE;
	}

	// InX
	if (dcb.fInX != 0) {
		dcb.fInX = 0;
		bFlag = TRUE;
	}

	// ErrorChar
	if (dcb.fErrorChar != 0) {
		dcb.fErrorChar = 0;
		bFlag = TRUE;
	}

	// fRTSControl
	if (dcb.fRtsControl != RTS_CONTROL_ENABLE) {
		dcb.fRtsControl = RTS_CONTROL_ENABLE;
		bFlag = TRUE;
	}

	// fAbortOnError
	if (dcb.fAbortOnError != TRUE) {
		dcb.fAbortOnError = TRUE;
		bFlag = TRUE;
	}

	// Data bits
	if (dcb.ByteSize != (BYTE)p->rxbit) {
		if (p->rxbit == p->txbit) {
			if (p->rxbit >= 5) {
				dcb.ByteSize = (BYTE)p->rxbit;
				bFlag = TRUE;
			}
		}
	}

	// Parity
	switch (p->parity) {
		case 0:
			dwValue = NOPARITY;
			break;
		case 1:
			dwValue = ODDPARITY;
			break;
		case 2:
			dwValue = EVENPARITY;
			break;
		default:
			ASSERT(FALSE);
			break;
	}
	if (dcb.Parity != (BYTE)dwValue) {
		dcb.Parity = (BYTE)dwValue;
		bFlag = TRUE;
	}

	// Stop bits
	switch (p->stopbit) {
		case 1:
			dwValue = ONESTOPBIT;
			break;
		case 2:
			dwValue = ONE5STOPBITS;
			break;
		case 3:
			dwValue = TWOSTOPBITS;
			break;
		default:
			dwValue = ONESTOPBIT;
			break;
	}
	if (dcb.StopBits != (BYTE)dwValue) {
		dcb.StopBits = (BYTE)dwValue;
		bFlag = TRUE;
	}

	// If different
	if (bFlag) {
		// Set
		::SetCommState(m_hCOM, &dcb);

		// Clear buffer
		m_TxQueue.Clear();
		m_RxQueue.Clear();

		// Win32 purge
		::PurgeComm(m_hCOM, PURGE_TXCLEAR);
		::PurgeComm(m_hCOM, PURGE_RXCLEAR);
	}
}

//---------------------------------------------------------------------------
//
//	Baudrate match
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPort::MatchCOM(DWORD dwSCC, DWORD dwBase)
{
	DWORD dwOffset;

	ASSERT(this);

	// 5% offset against reference
	dwOffset = (dwBase * 5);
	dwOffset /= 100;

	// If within range, OK
	if ((dwBase - dwOffset) <= dwSCC) {
		if (dwSCC <= (dwBase + dwOffset)) {
			return TRUE;
		}
	}

	// Different
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Signal handling
//
//---------------------------------------------------------------------------
void FASTCALL CPort::SignalCOM()
{
	ASSERT(this);
	ASSERT(m_pSCC);

	// Notify VM (error)
	if (m_dwErr & CE_BREAK) {
		m_pSCC->SetBreak(0, TRUE);
	}
	else {
		m_pSCC->SetBreak(0, FALSE);
	}
	if (m_dwErr & CE_FRAME) {
		m_pSCC->FramingErr(0);
	}
	if (m_dwErr & CE_RXPARITY) {
		m_pSCC->ParityErr(0);
	}

	// Notify VM (receive signal)
	m_pSCC->SetCTS(0, m_bCTS);
	m_pSCC->SetDCD(0, m_bDSR);

	// Get from VM (receive signal)
	m_bBreak = m_pSCC->GetBreak(0);
	m_bRTS = m_pSCC->GetRTS(0);
	m_bDTR = m_pSCC->GetDTR(0);
}

//---------------------------------------------------------------------------
//
//	Buffer handling
//
//---------------------------------------------------------------------------
void FASTCALL CPort::BufCOM()
{
	DWORD i;
	BYTE RxBuf[0x1000];
	DWORD dwRx;
	BYTE TxData;

	ASSERT(this);
	ASSERT(m_pSCC);

	// Get all receive data
	dwRx = m_RxQueue.Get(RxBuf);

	// Send all valid data to SCC
	for (i=0; i<dwRx; i++) {
		m_pSCC->Send(0, RxBuf[i]);
	}

	// Based on SCC send buffer status, wait for send completion
	if (m_pSCC->IsTxFull(0)) {
		// Buffer is full, so continue waiting until send completes
		m_pSCC->WaitTx(0, TRUE);
	}
	else {
		// Send ok
		m_pSCC->WaitTx(0, FALSE);
	}

	// If there is send data, receive
	while (!m_pSCC->IsTxEmpty(0)) {
		// If buffer is full, break within range
		if (m_TxQueue.GetFree() == 0) {
			break;
		}

		TxData = (BYTE)m_pSCC->Receive(0);
		m_TxQueue.Insert(&TxData, 1);
	}
}

//---------------------------------------------------------------------------
//
//	Control
//
//---------------------------------------------------------------------------
void FASTCALL CPort::CtrlCOM()
{
	ASSERT(this);
	ASSERT(m_hCOM);

	// Break
	if (m_bBreak) {
		::EscapeCommFunction(m_hCOM, SETBREAK);
	}
	else {
		::EscapeCommFunction(m_hCOM, CLRBREAK);
	}

	// RTS
	if (m_bRTS) {
		::EscapeCommFunction(m_hCOM, SETRTS);
	}
	else {
		::EscapeCommFunction(m_hCOM, CLRRTS);
	}

	// DTR
	if (m_bDTR) {
		::EscapeCommFunction(m_hCOM, SETDTR);
	}
	else {
		::EscapeCommFunction(m_hCOM, CLRDTR);
	}
}

//---------------------------------------------------------------------------
//
//	CTS,DSR
//
//---------------------------------------------------------------------------
void FASTCALL CPort::OnCTSDSR()
{
	DWORD dwStat;

	ASSERT(this);
	ASSERT(m_hCOM);

	// Get status
	::GetCommModemStatus(m_hCOM, &dwStat);

	// CTS
	if (dwStat & MS_CTS_ON) {
		m_bCTS = TRUE;
	}
	else {
		m_bCTS = FALSE;
	}

	// DSR
	if (dwStat & MS_DSR_ON) {
		m_bDSR = TRUE;
	}
	else {
		m_bDSR = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Receive error
//
//---------------------------------------------------------------------------
void FASTCALL CPort::OnErr()
{
	ASSERT(this);
	ASSERT(m_hCOM);

	// Clear error and record
	::ClearCommError(m_hCOM, &m_dwErr, NULL);
}

//---------------------------------------------------------------------------
//
//	Receive subroutine
//
//---------------------------------------------------------------------------
void FASTCALL CPort::OnRx()
{
	DWORD dwReceived;

	ASSERT(this);
	ASSERT(m_hCOM);

	// If not receiving, exit
	dwReceived = 0;
	if (!GetOverlappedResult(m_hCOM, &m_RxOver, &dwReceived, FALSE)) {
		return;
	}

	// If receive data is valid
	if (dwReceived > 0) {
		// If receive log file is valid, save
		if (m_RecvFile.IsValid()) {
			m_RecvFile.Write(m_RxBuf, dwReceived);
		}

		// Add to queue
		m_RxQueue.Insert(m_RxBuf, dwReceived);
	}

	// Next receive
	memset(&m_RxOver, 0, sizeof(m_RxOver));
	::ReadFile(m_hCOM, m_RxBuf, sizeof(m_RxBuf), NULL, &m_RxOver);
}

//---------------------------------------------------------------------------
//
//	Send subroutine
//
//---------------------------------------------------------------------------
void FASTCALL CPort::OnTx()
{
	DWORD dwSize;
	DWORD dwSent;

	ASSERT(this);
	ASSERT(m_hCOM);

	// If previous send is not finished, return
	if (m_bTxValid) {
		if (!GetOverlappedResult(m_hCOM, &m_TxOver, &dwSent, FALSE)) {
			return;
		}

		// If send is complete, discard from send buffer
		if (dwSent > 0) {
			m_TxQueue.Discard(dwSent);
		}

		// No send now
		m_bTxValid = FALSE;
	}

	// If no send data, return
	if (m_TxQueue.IsEmpty()) {
		return;
	}

	// Get send queue (pointer is incremented)
	dwSize = m_TxQueue.Copy(m_TxBuf);

	// Send start
	memset(&m_TxOver, 0, sizeof(m_TxOver));
	m_bTxValid = TRUE;
	::WriteFile(m_hCOM, m_TxBuf, dwSize, NULL, &m_TxOver);
}

//---------------------------------------------------------------------------
//
//	Get COM info
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPort::GetCOMInfo(LPTSTR lpszDevFile, DWORD *dwLogFile) const
{
	ASSERT(this);
	ASSERT(lpszDevFile);
	ASSERT(dwLogFile);

	// File is serial, handle is invalid
	_tcscpy(lpszDevFile, _T("  (None)"));
	*dwLogFile = (DWORD)INVALID_HANDLE_VALUE;

	// If thread is not running, exit with FALSE at this point
	if (!m_pCOM) {
		return FALSE;
	}

	// Set device file name
	if (m_nCOM > 0) {
		_stprintf(lpszDevFile, _T("\\\\.\\COM%d"), m_nCOM);
	}

	// Set log file
	if (m_RecvFile.IsValid()) {
		*dwLogFile = (DWORD)m_RecvFile.GetHandle();
	}

	// Thread is running
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get Tx queue
//
//---------------------------------------------------------------------------
void FASTCALL CPort::GetTxQueue(CQueue::LPQUEUEINFO lpqi) const
{
	ASSERT(this);
	ASSERT(lpqi);

	m_TxQueue.GetQueue(lpqi);
}

//---------------------------------------------------------------------------
//
//	Get Rx queue
//
//---------------------------------------------------------------------------
void FASTCALL CPort::GetRxQueue(CQueue::LPQUEUEINFO lpqi) const
{
	ASSERT(this);
	ASSERT(lpqi);

	m_RxQueue.GetQueue(lpqi);
}

//===========================================================================
//
//	Parallel port
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	LPT open
//	Note VM is locked
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPort::OpenLPT()
{
	COMMTIMEOUTS cto;
	CString strFile;

	ASSERT(this);

	// Initialize handle
	m_hLPT = INVALID_HANDLE_VALUE;
	m_pLPT = NULL;
	m_bLPTValid = FALSE;

	// Create log file (append)
	AppendLPT();

	// If port is not 0, output to LPT device
	if (m_nLPT != 0) {
		// Create file name
		strFile.Format(_T("\\\\.\\LPT%d"), m_nLPT);

		// Open
		m_hLPT = ::CreateFile(  strFile,
								GENERIC_WRITE,
								0,
								NULL,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
								0);

		// If opened successfully, set
		if (m_hLPT != INVALID_HANDLE_VALUE) {
			// Buffer size
			::SetupComm(m_hLPT, 0x400, 0x1000);

			// Event mask
			::SetCommMask(m_hLPT, 0);

			// Timeout
			::GetCommTimeouts(m_hLPT, &cto);
			cto.WriteTotalTimeoutMultiplier = 1;
			cto.WriteTotalTimeoutConstant = 10;
			::SetCommTimeouts(m_hLPT, &cto);
		}
	}

	// Buffer initialization
	m_LPTQueue.Clear();

	// If log file or LPT device is available
	if (m_SendFile.IsValid() || (m_hLPT != INVALID_HANDLE_VALUE)) {
		// Start thread
		m_bLPTReq = FALSE;
		m_pLPT = AfxBeginThread(LPTThread, this);

		// If thread starts, notify printer
		if (m_pLPT) {
			m_pPrinter->Connect(TRUE);
			return TRUE;
		}
	}

	// Close port (eventually m_pPrinter->Connect(FALSE) is called)
	CloseLPT();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	LPT log file
//
//---------------------------------------------------------------------------
void FASTCALL CPort::AppendLPT()
{
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	Filepath path;

	ASSERT(this);

	// If log file is not specified, return
	if (_tcslen(m_SendLog) == 0) {
		return;
	}

	// Parse
	_tsplitpath(m_SendLog, szDrive, szDir, szFile, szExt);

	// If no drive path, use relative path
	path.SetPath(m_SendLog);
	if ((_tcslen(szDrive) == 0) && (szDir[0] != _T('\\'))) {
		path.SetBaseDir();
	}

	// Append open
	m_SendFile.Open(path, Fileio::Append);
}

//---------------------------------------------------------------------------
//
//	LPT close
//	Note VM is locked
//
//---------------------------------------------------------------------------
void FASTCALL CPort::CloseLPT()
{
	DWORD dwCompleted;

	ASSERT(this);

	// Stop thread
	if (m_pLPT) {
		// Set exit flag
		m_bLPTReq = TRUE;

		// Wait for exit (infinite)
		::WaitForSingleObject(m_pLPT->m_hThread, INFINITE);

		m_pLPT = NULL;
	}

	// Close handle
	if (m_hLPT != INVALID_HANDLE_VALUE) {
		// Send
		if (m_bLPTValid) {
			if (!GetOverlappedResult(m_hLPT, &m_LPTOver, &dwCompleted, FALSE)) {
				// Still pending
				CancelIo(m_hLPT);
			}
			// Not sending
			m_bLPTValid = FALSE;
		}

		// Close
		::CloseHandle(m_hLPT);

		// Reset
		m_hLPT = INVALID_HANDLE_VALUE;
		m_bLPTValid = FALSE;
		memset(&m_LPTOver, 0, sizeof(m_LPTOver));
	}

	// Log file close
	if (m_SendFile.IsValid()) {
		m_SendFile.Close();
	}

	// To printer, disconnect notification
	m_pPrinter->Connect(FALSE);
}

//---------------------------------------------------------------------------
//
//	LPT execution
//
//---------------------------------------------------------------------------
void FASTCALL CPort::RunLPT()
{
	ASSERT(this);

	// Until exit flag is set
	while (!m_bLPTReq) {
		// Sleep
		::Sleep(10);

		// Send buffer content and log output
		SendLPT();

		// Get printer data
		RecvLPT();
	}
}

//---------------------------------------------------------------------------
//
//	Printer data send
//
//---------------------------------------------------------------------------
void FASTCALL CPort::SendLPT()
{
	BYTE Buffer[0x1000];
	DWORD dwNum;
	DWORD dwWritten;

	ASSERT(this);

	// Send
	if (!m_bLPTValid) {
		// If overlapped buffer is empty, do nothing
		if (m_LPTQueue.IsEmpty()) {
			return;
		}

		// Copy data as batch and receive
		dwNum = m_LPTQueue.Copy(Buffer);
		ASSERT(dwNum > 0);
		ASSERT(dwNum <= sizeof(Buffer));

		// Buffer valid, LPT device not available, log only
		if (m_hLPT == INVALID_HANDLE_VALUE) {
			// Send log file must be valid
			ASSERT(m_SendFile.IsValid());

			// Write to log file
			m_SendFile.Write(Buffer, dwNum);

			// Discard here
			m_LPTQueue.Discard(dwNum);
			return;
		}

		// Buffer valid, LPT device available. Start write operation
		memset(&m_LPTOver, 0, sizeof(m_LPTOver));
		::WriteFile(m_hLPT, Buffer, dwNum, &dwWritten, &m_LPTOver);
		m_bLPTValid = TRUE;
		return;
	}

	// If device is not available, exit here
	if (m_hLPT == INVALID_HANDLE_VALUE) {
		return;
	}

	// If send is not complete
	if (!GetOverlappedResult(m_hLPT, &m_LPTOver, &dwWritten, FALSE)) {
		return;
	}

	// Send complete. Copy remaining data and discard (buffer discard)
	m_LPTQueue.Copy(Buffer);
	m_LPTQueue.Discard(dwWritten);

	// If log file is valid
	if (m_SendFile.IsValid()) {
		m_SendFile.Write(Buffer, dwWritten);
	}

	// Send flag off
	m_bLPTValid = FALSE;
}

//---------------------------------------------------------------------------
//
//	Printer data receive
//	Printer device performs handshaking
//
//---------------------------------------------------------------------------
void FASTCALL CPort::RecvLPT()
{
	BYTE Buffer[0x1000];
	DWORD dwNum;
	DWORD dwFree;

	ASSERT(this);

	// Get buffer free space
	dwFree = m_LPTQueue.GetFree();
	dwNum = 0;

	// Loop while there is free space in buffer
	while (dwFree > 0) {
		// Get data if available
		if (!m_pPrinter->GetData(&Buffer[dwNum])) {
			break;
		}

		// Increment data count
		dwFree--;
		dwNum++;
	}

	// Add to queue if there is new data
	if (dwNum > 0) {
		m_LPTQueue.Insert(Buffer, dwNum);
	}
}

//---------------------------------------------------------------------------
//
//	Get LPT info
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPort::GetLPTInfo(LPTSTR lpszDevFile, DWORD *dwLogFile) const
{
	ASSERT(this);
	ASSERT(lpszDevFile);
	ASSERT(dwLogFile);

	// File is parallel, handle is invalid
	_tcscpy(lpszDevFile, _T("  (None)"));
	*dwLogFile = (DWORD)INVALID_HANDLE_VALUE;

	// If thread is not running, exit with FALSE at this point
	if (!m_pLPT) {
		return FALSE;
	}

	// Set device file name
	if (m_nLPT > 0) {
		_stprintf(lpszDevFile, _T("\\\\.\\LPT%d"), m_nLPT);
	}

	// Set log file
	if (m_SendFile.IsValid()) {
		*dwLogFile = (DWORD)m_SendFile.GetHandle();
	}

	// Thread is running
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get LPT queue
//
//---------------------------------------------------------------------------
void FASTCALL CPort::GetLPTQueue(CQueue::LPQUEUEINFO lpqi) const
{
	ASSERT(this);
	ASSERT(lpqi);

	m_LPTQueue.GetQueue(lpqi);
}

#endif	// _WIN32
