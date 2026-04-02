//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 š°å•½D(ytanaka@ipc-tokai.or.jp)
//	[ MFC Queue ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_que_h)
#define mfc_que_h

//===========================================================================
//
//	Queue
//
//===========================================================================
class CQueue : public CObject
{
public:
	// Internal data type
	typedef struct _QUQUEINFO {
		BYTE *pBuf;						// Buffer
		DWORD dwSize;					// Size
		DWORD dwMask;					// Mask (size-1)
		DWORD dwRead;					// Read pointer
		DWORD dwWrite;					// Write pointer
		DWORD dwNum;					// Count
		DWORD dwTotalRead;				// Total read
		DWORD dwTotalWrite;				// Total write
	} QUEUEINFO, *LPQUEUEINFO;

	// Basic procedures
	CQueue();
										// Constructor
	virtual ~CQueue();
										// Destructor
	BOOL FASTCALL Init(DWORD dwSize);
										// Initialization

	// API
	void FASTCALL Clear();
										// Clear
	BOOL FASTCALL IsEmpty()	const		{ return (BOOL)(m_Queue.dwNum == 0); }
										// Check if queue is empty
	DWORD FASTCALL Get(BYTE *pDest);
										// Get all data from queue
	DWORD FASTCALL Copy(BYTE *pDest) const;
										// Get all data from queue (queue unchanged)
	void FASTCALL Discard(DWORD dwNum);
										// Discard from queue
	void FASTCALL Back(DWORD dwNum);
										// Put back into queue
	DWORD FASTCALL GetFree() const;
										// Get free space count
	BOOL FASTCALL Insert(const BYTE *pSrc, DWORD dwLength);
										// Insert into queue
	void FASTCALL GetQueue(QUEUEINFO *pInfo) const;
										// Get queue

private:
	QUEUEINFO m_Queue;
										// Internal queue
};

#endif	// mfc_que_h
#endif	// _WIN32
