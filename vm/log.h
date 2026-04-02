//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ Log ]
//
//---------------------------------------------------------------------------

#if !defined(log_h)
#define log_h

#include "xm6.h"
#include <stdarg.h>

//===========================================================================
//
//	Log
//
//===========================================================================
class Log
{
public:
	enum loglevel {
		Detail,							// Detail level
		Normal,							// Normal level
		Warning							// Warning level
	};
	typedef struct {
		DWORD number;					// Entry number (clear at reset)
		DWORD total;					// Entry number (total)
		DWORD time;						// Execution time
		DWORD id;						// Device ID
		DWORD pc;						// Program counter
		loglevel level;					// Level
		char *string;					// String data
	} logdata_t;

public:
	// Basic constructor
	Log();
										// Constructor
	BOOL FASTCALL Init(VM *vm);
										// Initialization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL Reset();
										// Reset
#if !defined(NDEBUG)
	void FASTCALL AssertDiag() const;
										// Assert
#endif	// NDEBUG

	// Output
	void Format(loglevel level, const Device *dev, char *format, ...);
										// Log output(...)
	void vFormat(loglevel level, const Device *dev, char *format, va_list args);
										// Log output(va)
	void FASTCALL AddString(DWORD id, loglevel level, char *string);
										// Log data add

	// Get
	int FASTCALL GetNum() const;
										// Get log entry count
	int FASTCALL GetMax() const;
										// Get max log count
	BOOL FASTCALL GetData(int index, logdata_t *ptr);
										// Get log data

private:
	enum {
		LogMax = 0x4000					// Max log entry (must be power of 2)
	};

private:
	void FASTCALL Clear();
										// Data clear
	int logtop;
										// Top pointer (cyclic)
	int lognum;
										// Log entry count
	int logcount;
										// Log count
	logdata_t *logdata[LogMax];
										// Log pointer
	Sync *sync;
										// Sync object
	CPU *cpu;
										// CPU
	Scheduler *scheduler;
										// Scheduler
};

//---------------------------------------------------------------------------
//
//	Log output macro
//
//---------------------------------------------------------------------------
#if !defined(NO_LOG)
class LogProxy {
public:
	// Constructor
	LogProxy(const Device* device, Log* log)
	{
		m_device = device;
		m_log = log;
	}

	// Log output
	void operator()(enum Log::loglevel level, char* format, ...) const
	{
		va_list args;
		va_start(args, format);
		m_log->vFormat(level, m_device, format, args);
		va_end(args);
	}
private:
	const Device* m_device;
	Log* m_log;
};
#define LOG						LogProxy(this, GetLog())
#define LOG0(l, s)			  	GetLog()->Format(l, this, s)
#define LOG1(l, s, a)		  	GetLog()->Format(l, this, s, a)
#define LOG2(l, s, a, b)	  	GetLog()->Format(l, this, s, a, b)
#define LOG3(l, s, a, b, c)   	GetLog()->Format(l, this, s, a, b, c)
#define LOG4(l, s, a, b, c, d)	GetLog()->Format(l, this, s, a, b, c, d)
#else
#define LOG						LOG_NONE
#define LOG0(l, s)				((void)0)
#define LOG1(l, s, a)			((void)0)
#define LOG2(l, s, a, b)		((void)0)
#define LOG3(l, s, a, b, c)	  	((void)0)
#define LOG4(l, s, a, b, c, d)	((void)0)
static inline LOG_NONE(enum Log::loglevel level, char *format, ...) {}
#endif	// !NO_LOG

#endif	// log_h
