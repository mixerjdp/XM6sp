//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	[ Synchronization object ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)
#include <windows.h>
#endif
#include "os.h"
#include "xm6.h"
#include "sync.h"

Sync::Sync()
{
#if defined(_WIN32)
	csect = new CRITICAL_SECTION;
	::InitializeCriticalSection(csect);
#else
	mutex = new pthread_mutex_t;
	pthread_mutex_init(mutex, NULL);
#endif
}

Sync::~Sync()
{
#if defined(_WIN32)
	ASSERT(csect);
	::DeleteCriticalSection(csect);
	delete csect;
	csect = NULL;
#else
	ASSERT(mutex);
	pthread_mutex_destroy(mutex);
	delete mutex;
	mutex = NULL;
#endif
}

void FASTCALL Sync::Lock()
{
#if defined(_WIN32)
	ASSERT(csect);
	::EnterCriticalSection(csect);
#else
	ASSERT(mutex);
	pthread_mutex_lock(mutex);
#endif
}

void FASTCALL Sync::Unlock()
{
#if defined(_WIN32)
	ASSERT(csect);
	::LeaveCriticalSection(csect);
#else
	ASSERT(mutex);
	pthread_mutex_unlock(mutex);
#endif
}
