//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ Log ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "sync.h"
#include "log.h"
#include "device.h"
#include "vm.h"
#include "cpu.h"
#include "schedule.h"

//===========================================================================
//
//	Log
//
//===========================================================================
//#define LOG_WIN32

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Log::Log()
{
	// Management ring clear
	logtop = 0;
	lognum = 0;
	memset(logdata, 0, sizeof(logdata));
	logcount = 1;

	// No device
	cpu = NULL;
	scheduler = NULL;
	sync = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL Log::Init(VM *vm)
{
 ASSERT(this);
 ASSERT(vm);

	// Get CPU
 ASSERT(!cpu);
	cpu = (CPU *)vm->SearchDevice(MAKEID('C', 'P', 'U', ' '));
 ASSERT(cpu);

	// Get Scheduler
 ASSERT(!scheduler);
	scheduler = (Scheduler *)vm->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
 ASSERT(scheduler);

	// Create sync object
 ASSERT(!sync);
	sync = new Sync;
 ASSERT(sync);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL Log::Cleanup()
{
 ASSERT(this);

	// Clear all data (if initialized)
	if (sync) {
		Clear();
	}

	// Delete sync object
	if (sync) {
		delete sync;
		sync = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Log::Reset()
{
 ASSERT(this);
 ASSERT_DIAG();

	// Clear all data
	Clear();
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void FASTCALL Log::AssertDiag() const
{
 ASSERT(this);
 ASSERT(cpu);
 ASSERT(cpu->GetID() == MAKEID('C', 'P', 'U', ' '));
 ASSERT(scheduler);
 ASSERT(scheduler->GetID() == MAKEID('S', 'C', 'H', 'E'));
 ASSERT(sync);
 ASSERT((logtop >= 0) && (logtop < LogMax));
 ASSERT((lognum >= 0) && (lognum <= LogMax));
 ASSERT(logcount >= 1);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Clear
//
//---------------------------------------------------------------------------
void FASTCALL Log::Clear()
{
	int i;
	int index;

 ASSERT(this);

	// Lock
	sync->Lock();

	index = logtop;
	for (i=0; i<lognum; i++) {
		// If data exists
		if (logdata[index]) {
			// Delete string pointer
			delete[] logdata[index]->string;

			// Delete data body
			delete logdata[index];

			// NULL set
			logdata[index] = NULL;
		}

		// Move index
		index++;
		index &= (LogMax - 1);
	}

	// Management ring clear
	logtop = 0;
	lognum = 0;
	logcount = 1;

	// Unlock
	sync->Unlock();
}

//---------------------------------------------------------------------------
//
//	Format(...)
//
//---------------------------------------------------------------------------
void Log::Format(loglevel level, const Device *device, char *format, ...)
{
	char buffer[0x200];
	va_list args;
	va_start(args, format);

 ASSERT(this);
 ASSERT(device);
 ASSERT_DIAG();

	// Format
	vsprintf(buffer, format, args);

	// Variable argument end
	va_end(args);

	// Add message
	AddString(device->GetID(), level, buffer);
}

//---------------------------------------------------------------------------
//
//	Format(va)
//
//---------------------------------------------------------------------------
void Log::vFormat(loglevel level, const Device *device, char *format, va_list args)
{
	char buffer[0x200];

 ASSERT(this);
 ASSERT(device);
 ASSERT_DIAG();

	// Format
	vsprintf(buffer, format, args);

	// Add message
	AddString(device->GetID(), level, buffer);
}

//---------------------------------------------------------------------------
//
//	Add data
//
//---------------------------------------------------------------------------
void FASTCALL Log::AddString(DWORD id, loglevel level, char *string)
{
	int index;

 ASSERT(this);
 ASSERT(string);
 ASSERT_DIAG();

	// Lock
	sync->Lock();

	// Find add position
	if (lognum < LogMax) {
	 ASSERT(logtop == 0);
		index = lognum;
	 ASSERT(logdata[index] == NULL);
		lognum++;
	}
	else {
	 ASSERT(lognum == LogMax);
		index = logtop;
		logtop++;
		logtop &= (LogMax - 1);
	}

	// If already exists, release
	if (logdata[index]) {
		delete[] logdata[index]->string;
		delete logdata[index];
	}

	// Allocate structure
	logdata[index] = new logdata_t;
	logdata[index]->number = logcount++;
	logdata[index]->time = 0;
	logdata[index]->time = scheduler->GetTotalTime();
	logdata[index]->pc = cpu->GetPC();
	logdata[index]->id = id;
	logdata[index]->level = level;

	// String copy
	logdata[index]->string = new char[strlen(string) + 1];
	strcpy(logdata[index]->string, string);

#if defined(LOG_WIN32)
	// Output as Win32 message
	OutputDebugString(string);
	OutputDebugString(_T("\n"));
#endif	// LOG_WIN32

	// Unlock
	sync->Unlock();

	// If log count overflow, clear (every 16M or so)
	if (logcount >= 0x60000000) {
		Clear();
	}
}

//---------------------------------------------------------------------------
//
//	Get data count
//
//---------------------------------------------------------------------------
int FASTCALL Log::GetNum() const
{
 ASSERT(this);
 ASSERT_DIAG();

	return lognum;
}

//---------------------------------------------------------------------------
//
//	Get max log count
//
//---------------------------------------------------------------------------
int FASTCALL Log::GetMax() const
{
 ASSERT(this);
 ASSERT_DIAG();

	return LogMax;
}

//---------------------------------------------------------------------------
//
//	Get data
//
//---------------------------------------------------------------------------
BOOL FASTCALL Log::GetData(int index, logdata_t *ptr)
{
	char *string;

 ASSERT(this);
 ASSERT(index >= 0);
 ASSERT(ptr);
 ASSERT_DIAG();

	// Lock
	sync->Lock();

	// Current check
	if (index >= lognum) {
		sync->Unlock();
		return FALSE;
	}

	// Index calculate
	index += logtop;
	index &= (LogMax - 1);

	// Copy(exclude string pointer)
 ASSERT(logdata[index]);
	*ptr = *logdata[index];

	// Copy(string pointer)
	string = ptr->string;
	ptr->string = new char[strlen(string) + 1];
	strcpy(ptr->string, string);

	// Unlock
	sync->Unlock();
	return TRUE;
}
