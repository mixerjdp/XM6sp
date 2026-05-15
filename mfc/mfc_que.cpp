//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC Queue ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "mfc_que.h"

//===========================================================================
//
//	Queue
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CQueue::CQueue()
{
	// No pointer
	m_Queue.pBuf = NULL;

	// Size 0
	m_Queue.dwSize = 0;
	m_Queue.dwMask = 0;

	// Others
	Clear();
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
CQueue::~CQueue()
{
	// Cleanup
	if (m_Queue.pBuf) {
		delete[] m_Queue.pBuf;
		m_Queue.pBuf = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CQueue::Init(DWORD dwSize)
{
	ASSERT(this);
	ASSERT(!m_Queue.pBuf);
	ASSERT(m_Queue.dwSize == 0);
	ASSERT(dwSize > 0);

	// Allocate
	try {
		m_Queue.pBuf = new BYTE[dwSize];
	}
	catch (...) {
		m_Queue.pBuf = NULL;
		return FALSE;
	}
	if (!m_Queue.pBuf) {
		return FALSE;
	}

	// Size setting
	m_Queue.dwSize = dwSize;
	m_Queue.dwMask = m_Queue.dwSize - 1;

	// Clear
	Clear();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Clear
//	Internal use only
//
//---------------------------------------------------------------------------
void FASTCALL CQueue::Clear()
{
	ASSERT(this);

	m_Queue.dwRead = 0;
	m_Queue.dwWrite = 0;
	m_Queue.dwNum = 0;
	m_Queue.dwTotalRead = 0;
	m_Queue.dwTotalWrite = 0;
}

//---------------------------------------------------------------------------
//
//	Get all data from queue
//	Internal use only
//
//---------------------------------------------------------------------------
DWORD FASTCALL CQueue::Get(BYTE *pDest)
{
	DWORD dwLength;
	DWORD dwRest;

	ASSERT(this);
	ASSERT(m_Queue.dwSize > 0);
	ASSERT(m_Queue.pBuf);
	ASSERT((m_Queue.dwMask + 1) == m_Queue.dwSize);
	ASSERT(m_Queue.dwRead < m_Queue.dwSize);
	ASSERT(m_Queue.dwWrite < m_Queue.dwSize);
	ASSERT(m_Queue.dwNum <= m_Queue.dwSize);

	ASSERT(pDest);

	// If no data, return 0
	if (m_Queue.dwNum == 0) {
		return 0;
	}

	// Calculate number of bytes before and after overlap
	dwLength = m_Queue.dwNum;
	dwRest = 0;
	if ((dwLength + m_Queue.dwRead) > m_Queue.dwSize) {
		dwRest = (dwLength + m_Queue.dwRead) - m_Queue.dwSize;
		dwLength -= dwRest;
	}

	// Copy
	memcpy(pDest, &m_Queue.pBuf[m_Queue.dwRead], dwLength);
	m_Queue.dwRead += dwLength;
	m_Queue.dwRead &= m_Queue.dwMask;
	m_Queue.dwNum -= dwLength;

	// Overlap
	memcpy(&pDest[dwLength], m_Queue.pBuf, dwRest);
	m_Queue.dwRead += dwRest;
	m_Queue.dwRead &= m_Queue.dwMask;
	m_Queue.dwNum -= dwRest;

	// Increment total read
	m_Queue.dwTotalRead += dwLength;
	m_Queue.dwTotalRead += dwRest;

	// Return copy total size
	return (DWORD)(dwLength + dwRest);
}

//---------------------------------------------------------------------------
//
//	Get all data from queue (queue unchanged)
//	Internal use only
//
//---------------------------------------------------------------------------
DWORD FASTCALL CQueue::Copy(BYTE *pDest) const
{
	DWORD dwLength;
	DWORD dwRest;

	ASSERT(this);
	ASSERT(m_Queue.dwSize > 0);
	ASSERT(m_Queue.pBuf);
	ASSERT((m_Queue.dwMask + 1) == m_Queue.dwSize);
	ASSERT(m_Queue.dwRead < m_Queue.dwSize);
	ASSERT(m_Queue.dwWrite < m_Queue.dwSize);
	ASSERT(m_Queue.dwNum <= m_Queue.dwSize);

	ASSERT(pDest);

	// If no data, return 0
	if (m_Queue.dwNum == 0) {
		return 0;
	}

	// Calculate number of bytes before and after overlap
	dwLength = m_Queue.dwNum;
	dwRest = 0;
	if ((dwLength + m_Queue.dwRead) > m_Queue.dwSize) {
		dwRest = (dwLength + m_Queue.dwRead) - m_Queue.dwSize;
		dwLength -= dwRest;
	}

	// Copy
	memcpy(pDest, &m_Queue.pBuf[m_Queue.dwRead], dwLength);

	// Overlap
	memcpy(&pDest[dwLength], m_Queue.pBuf, dwRest);

	// Return copy total size
	return (DWORD)(dwLength + dwRest);
}

//---------------------------------------------------------------------------
//
//	Discard from queue
//	Internal use only. Copy->Discard returns same as Get if queue unchanged
//
//---------------------------------------------------------------------------
void FASTCALL CQueue::Discard(DWORD dwNum)
{
	ASSERT(this);
	ASSERT(m_Queue.dwSize > 0);
	ASSERT(m_Queue.pBuf);
	ASSERT((m_Queue.dwMask + 1) == m_Queue.dwSize);
	ASSERT(m_Queue.dwRead < m_Queue.dwSize);
	ASSERT(m_Queue.dwWrite < m_Queue.dwSize);
	ASSERT(m_Queue.dwNum <= m_Queue.dwSize);

	ASSERT(dwNum <= m_Queue.dwSize);
	ASSERT(dwNum <= m_Queue.dwNum);

	// Discard
	m_Queue.dwRead += dwNum;
	m_Queue.dwRead &= m_Queue.dwMask;

	// Count
	m_Queue.dwNum -= dwNum;

	// Total
	m_Queue.dwTotalRead += dwNum;
}

//---------------------------------------------------------------------------
//
//	Put back into queue
//	Internal use only. Get->Back returns same as Insert if unchanged
//
//---------------------------------------------------------------------------
void FASTCALL CQueue::Back(DWORD dwNum)
{
	DWORD dwRest;

	ASSERT(this);
	ASSERT(m_Queue.dwSize > 0);
	ASSERT(m_Queue.pBuf);
	ASSERT((m_Queue.dwMask + 1) == m_Queue.dwSize);
	ASSERT(m_Queue.dwRead < m_Queue.dwSize);
	ASSERT(m_Queue.dwWrite < m_Queue.dwSize);
	ASSERT(m_Queue.dwNum <= m_Queue.dwSize);

	ASSERT(dwNum <= m_Queue.dwSize);

	// Calculate dwRest
	dwRest = 0;
	if (m_Queue.dwRead < dwNum) {
		dwRest = dwNum - m_Queue.dwRead;
		dwNum = m_Queue.dwRead;
	}

	// Copy
	m_Queue.dwRead -= dwNum;
	m_Queue.dwRead &= m_Queue.dwMask;
	m_Queue.dwNum += dwNum;

	// Overlap
	m_Queue.dwRead -= dwRest;
	m_Queue.dwRead &= m_Queue.dwMask;
	m_Queue.dwNum += dwRest;

	// Total
	m_Queue.dwTotalRead -= dwNum;
	m_Queue.dwTotalRead -= dwRest;
}

//---------------------------------------------------------------------------
//
//	Get free space count
//	Internal use only
//
//---------------------------------------------------------------------------
DWORD FASTCALL CQueue::GetFree() const
{
	ASSERT(this);
	ASSERT(m_Queue.dwSize > 0);
	ASSERT(m_Queue.pBuf);
	ASSERT((m_Queue.dwMask + 1) == m_Queue.dwSize);
	ASSERT(m_Queue.dwRead < m_Queue.dwSize);
	ASSERT(m_Queue.dwWrite < m_Queue.dwSize);
	ASSERT(m_Queue.dwNum <= m_Queue.dwSize);

	return (DWORD)(m_Queue.dwSize - m_Queue.dwNum);
}

//---------------------------------------------------------------------------
//
//	Insert into queue
//	Internal use only
//
//---------------------------------------------------------------------------
BOOL FASTCALL CQueue::Insert(const BYTE *pSrc, DWORD dwLength)
{
	DWORD dwRest;

	ASSERT(this);
	ASSERT(m_Queue.dwSize > 0);
	ASSERT(m_Queue.pBuf);
	ASSERT((m_Queue.dwMask + 1) == m_Queue.dwSize);
	ASSERT(m_Queue.dwRead < m_Queue.dwSize);
	ASSERT(m_Queue.dwWrite < m_Queue.dwSize);
	ASSERT(m_Queue.dwNum <= m_Queue.dwSize);

	ASSERT(pSrc);

	// Calculate number of bytes before and after overlap
	dwRest = 0;
	if ((dwLength + m_Queue.dwWrite) > m_Queue.dwSize) {
		dwRest = (dwLength + m_Queue.dwWrite) - m_Queue.dwSize;
		dwLength -= dwRest;
	}

	// Copy
	memcpy(&m_Queue.pBuf[m_Queue.dwWrite], pSrc, dwLength);
	m_Queue.dwWrite += dwLength;
	m_Queue.dwWrite &= m_Queue.dwMask;
	m_Queue.dwNum += dwLength;

	// Overlap
	memcpy(m_Queue.pBuf, &pSrc[dwLength], dwRest);
	m_Queue.dwWrite += dwRest;
	m_Queue.dwWrite &= m_Queue.dwMask;
	m_Queue.dwNum += dwRest;

	// Increment total write
	m_Queue.dwTotalWrite += dwLength;
	m_Queue.dwTotalWrite += dwRest;

	// If read position has passed, return FALSE with correction
	if (m_Queue.dwNum > m_Queue.dwSize) {
		// Buffer overflow
		m_Queue.dwNum = m_Queue.dwSize;
		m_Queue.dwRead = m_Queue.dwWrite;
		return FALSE;
	}
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get queue
//	Internal use only
//
//---------------------------------------------------------------------------
void FASTCALL CQueue::GetQueue(QUEUEINFO *pInfo) const
{
	ASSERT(this);
	ASSERT(pInfo);

	*pInfo = m_Queue;
}

#endif	// _WIN32
