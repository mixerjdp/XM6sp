//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	[ Synchronization object ]
//
//---------------------------------------------------------------------------

#if !defined(sync_h)
#define sync_h

#include "os.h"
#include "xm6.h"

#if !defined(_WIN32)
#include <pthread.h>
#endif

class Sync
{
public:
	Sync();
	virtual ~Sync();
	void FASTCALL Lock();
	void FASTCALL Unlock();

private:
#if defined(_WIN32)
	CRITICAL_SECTION *csect;
#else
	pthread_mutex_t *mutex;
#endif
};

#endif	// sync_h
