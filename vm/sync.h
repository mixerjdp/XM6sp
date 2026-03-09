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

#if defined(_WIN32)

struct _RTL_CRITICAL_SECTION;
typedef struct _RTL_CRITICAL_SECTION CRITICAL_SECTION;

class Sync
{
public:
	Sync();
	virtual ~Sync();
	void FASTCALL Lock();
	void FASTCALL Unlock();

private:
	CRITICAL_SECTION *csect;
};

#endif	// _WIN32
#endif	// sync_h
