//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	[ Synchronization object ]
//
//---------------------------------------------------------------------------

#include <windows.h>
#include "os.h"
#include "xm6.h"
#include "sync.h"

#if defined(_WIN32)

Sync::Sync()
{
	csect = new CRITICAL_SECTION;
	::InitializeCriticalSection(csect);
}

Sync::~Sync()
{
	ASSERT(csect);
	::DeleteCriticalSection(csect);
	delete csect;
	csect = NULL;
}

void FASTCALL Sync::Lock()
{
	ASSERT(csect);
	::EnterCriticalSection(csect);
}

void FASTCALL Sync::Unlock()
{
	ASSERT(csect);
	::LeaveCriticalSection(csect);
}

#endif	// _WIN32
