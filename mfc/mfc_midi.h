//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC MIDI ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_midi_h)
#define mfc_midi_h

#include "mfc_com.h"
#include "mfc_que.h"

//===========================================================================
//
//	MIDI
//
//===========================================================================
class CMIDI : public CComponent
{
public:
	// Information transmission type
	typedef struct _MIDIINFO {
		DWORD dwDevices;				// Number of devices
		DWORD dwDevice;					// Device number
		DWORD dwHandle;					// Handle (HMIDI*)
		CWinThread *pThread;			// Thread
		DWORD dwShort;					// Short message counter
		DWORD dwLong;					// System exclusive counter
		DWORD dwUnprepare;				// Header preparation counter
		DWORD dwBufNum;				// Buffer count
		DWORD dwBufRead;				// Buffer read position
		DWORD dwBufWrite;				// Buffer write position
	} MIDIINFO, *LPMIDIINFO;

public:
	// Basic procedures
	CMIDI(CFrmWnd *pWnd);
										// Constructor
	BOOL FASTCALL Init();
										// Initialization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL ApplyCfg(const Config *pConfig);
										// Apply configuration
#if !defined(NDEBUG)
	void AssertValid() const;
										// Assert
#endif	// NDEBUG

	// Callback procedures
	HMIDIOUT GetOutHandle() const		{ return m_hOut; }
										// Get Out device handle
	HMIDIIN GetInHandle() const			{ return m_hIn; }
										// Get In device handle

	// Devices
	DWORD FASTCALL GetOutDevs() const;
										// Get Out device count
	BOOL FASTCALL GetOutDevDesc(int nDevice, CString& strDesc) const;
										// Get Out device description
	DWORD FASTCALL GetInDevs() const;
										// Get IN device count
	BOOL FASTCALL GetInDevDesc(int nDevice, CString& strDesc) const;
										// Get IN device description

	// Delay
	void FASTCALL SetOutDelay(int nDelay);
										// Set Out delay
	void FASTCALL SetInDelay(int nDelay);
										// Set In delay

	// Volume
	int FASTCALL GetOutVolume();
										// Get Out volume
	void FASTCALL SetOutVolume(int nVolume);
										// Set Out volume

	// Info
	void FASTCALL GetOutInfo(LPMIDIINFO pInfo) const;
										// Get Out info
	void FASTCALL GetInInfo(LPMIDIINFO pInfo) const;
										// Get In info

private:
	MIDI *m_pMIDI;
										// MIDI
	Scheduler *m_pScheduler;
										// Scheduler
	CScheduler *m_pSch;
										// Scheduler

	// Out
	enum OutState {
		OutReady,						// Waiting for data
		OutEx,							// System exclusive message output
		OutShort,						// Short message output
	};
	void FASTCALL OpenOut(DWORD dwDevice);
										// Out open
	void FASTCALL CloseOut();
										// Out close
	void FASTCALL RunOut();
										// Out thread
	void FASTCALL CallbackOut(UINT wMsg, DWORD dwParam1, DWORD dwParam2);
										// Out callback
	void FASTCALL GetOutCaps();
										// Get Out device caps
	BOOL FASTCALL SendEx(const BYTE *pExData);
										// System exclusive send
	BOOL FASTCALL SendExWait();
										// System exclusive send wait
	void FASTCALL SendAllNoteOff();
										// All note off
	BOOL FASTCALL SendReset();
										// Reset send
	static UINT OutThread(LPVOID pParam);
										// Out thread
#if _MFC_VER >= 0x700
	static void CALLBACK OutProc(HMIDIOUT hOut, UINT wMsg, DWORD_PTR dwInstance,
		DWORD dwParam1, DWORD dwParam2);// Out callback
#else
	static void CALLBACK OutProc(HMIDIOUT hOut, UINT wMsg, DWORD dwInstance,
		DWORD dwParam1, DWORD dwParam2);// Out callback
#endif
	DWORD m_dwOutDevice;
										// Out device ID
	HMIDIOUT m_hOut;
										// Out device handle
	CWinThread *m_pOutThread;
										// Out thread
	BOOL m_bOutThread;
										// Out thread exit flag
	DWORD m_dwOutDevs;
										// Out device count
	LPMIDIOUTCAPS m_pOutCaps;
										// Out device caps
	DWORD m_dwOutDelay;
										// Out delay (ms)
	BOOL m_bSendEx;
										// System exclusive send flag
	BOOL m_bSendExHdr;
										// System exclusive header prepared flag
	BYTE m_ExBuf[0x2000];
										// System exclusive buffer
	MIDIHDR m_ExHdr;
										// System exclusive header
	int m_nOutReset;
										// Reset command type
	DWORD m_dwShortSend;
										// Short command send counter
	DWORD m_dwExSend;
										// System exclusive send fail counter
	DWORD m_dwUnSend;
										// Header preparation counter
	CCriticalSection m_OutSection;
										// Critical section
	static const BYTE ResetGM[];
										// GM reset command
	static const BYTE ResetGS[];
										// GS reset command
	static const BYTE ResetXG[];
										// XG reset command
	static const BYTE ResetLA[];
										// LA reset command

	// In
	enum InState {
		InNotUsed,						// Not used
		InReady,						// Waiting for data
		InDone							// Data ready
	};
	enum {
		InBufMax = 0x800				// Buffer size
	};
	void FASTCALL OpenIn(DWORD dwDevice);
										// In open
	void FASTCALL CloseIn();
										// In close
	void FASTCALL RunIn();
										// In thread
	void FASTCALL CallbackIn(UINT wMsg, DWORD dwParam1, DWORD dwParam2);
										// In callback
	void FASTCALL GetInCaps();
										// Get In device caps
	BOOL FASTCALL StartIn();
										// In start
	void FASTCALL StopIn();
										// In stop
	void FASTCALL ShortIn();
										// In short message
	void FASTCALL LongIn(int nHdr);
										// In long message
	static UINT InThread(LPVOID pParam);
										// In thread
#if _MFC_VER >= 0x700
	static void CALLBACK InProc(HMIDIIN hIn, UINT wMsg, DWORD_PTR dwInstance,
		DWORD dwParam1, DWORD dwParam2);// In callback
#else
	static void CALLBACK InProc(HMIDIIN hIn, UINT wMsg, DWORD dwInstance,
		DWORD dwParam1, DWORD dwParam2);// In callback
#endif
	DWORD m_dwInDevice;
										// In device ID
	HMIDIIN m_hIn;
										// In device handle
	CWinThread *m_pInThread;
										// In thread
	BOOL m_bInThread;
										// In thread exit flag
	DWORD m_dwInDevs;
										// In device count
	LPMIDIINCAPS m_pInCaps;
										// In device caps
	MIDIHDR m_InHdr[2];
										// System exclusive header
	BYTE m_InBuf[2][InBufMax];
										// System exclusive buffer
	InState m_InState[2];
										// System exclusive buffer state
	CQueue m_InQueue;
										// Short message input queue
	DWORD m_dwShortRecv;
										// Short command recv counter
	DWORD m_dwExRecv;
										// System exclusive recv counter
	DWORD m_dwUnRecv;
										// Header preparation counter
	CCriticalSection m_InSection;
										// Critical section
};

#endif	// mfc_midi_h
#endif	// _WIN32
