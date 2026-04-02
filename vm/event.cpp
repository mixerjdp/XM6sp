//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI(ytanaka@ipc-tokai.or.jp)
//	[ Event ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "device.h"
#include "schedule.h"
#include "fileio.h"
#include "event.h"

//===========================================================================
//
//	Event
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Event::Event()
{
	// Member variable initialization
	ev.device = NULL;
	ev.desc[0] = '\0';
	ev.user = 0;
	ev.time = 0;
	ev.remain = 0;
	ev.next = NULL;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
Event::~Event()
{
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assertion
//
//---------------------------------------------------------------------------
void FASTCALL Event::AssertDiag() const
{
	ASSERT(this);
	ASSERT(ev.remain <=(ev.time + 625));
	ASSERT(ev.device);
	ASSERT(ev.scheduler);
	ASSERT(ev.scheduler->GetID() == MAKEID('S', 'C', 'H', 'E'));
	ASSERT(ev.desc[0] != '\0');
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Event::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	// Save size
	sz = sizeof(event_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save this data
	if (!fio->Write(&ev, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Event::Load(Fileio *fio, int ver)
{
	size_t sz;
	event_t lev;

	ASSERT(this);
	ASSERT(fio);

	// Before version 2.01, old format
	if (ver <= 0x0201) {
		return Load201(fio);
	}

	// Load size and verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(event_t)) {
		return FALSE;
	}

	// Load this data to temporary area
	if (!fio->Read(&lev, (int)sz)) {
		return FALSE;
	}

	// Keep current device and scheduler pointers
	lev.device = ev.device;
	lev.scheduler = ev.scheduler;
	lev.next = ev.next;

	// Copy and finish
	ev = lev;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load (version 2.01)
//
//---------------------------------------------------------------------------
BOOL FASTCALL Event::Load201(Fileio *fio)
{
	size_t sz;
	event201_t ev201;

	ASSERT(this);
	ASSERT(fio);

	// Load size and verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(event201_t)) {
		return FALSE;
	}

	// Load this data to temporary area
	if (!fio->Read(&ev201, (int)sz)) {
		return FALSE;
	}

	// Copy (partial)
	strcpy(ev.desc, ev201.desc);
	ev.user = ev201.user;
	ev.time = ev201.time;
	ev.remain = ev201.remain;

	// If enable is off, set remain,time to 0
	if (!ev201.enable) {
		ev.time = 0;
		ev.remain = 0;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Device setting
//
//---------------------------------------------------------------------------
void FASTCALL Event::SetDevice(Device *p)
{
	VM *vm;

	ASSERT(this);
	ASSERT(!ev.device);
	ASSERT(p);

	// Get scheduler
	vm = p->GetVM();
	ASSERT(vm);
	ev.scheduler = (Scheduler*)vm->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(ev.scheduler);

	// Set device
	ev.device = p;
}

//---------------------------------------------------------------------------
//
//	Description setting
//
//---------------------------------------------------------------------------
void FASTCALL Event::SetDesc(const char *desc)
{
	ASSERT(this);
	ASSERT(desc);
	ASSERT(strlen(desc) < sizeof(ev.desc));

	strcpy(ev.desc, desc);
}

//---------------------------------------------------------------------------
//
//	Description get
//
//---------------------------------------------------------------------------
const char* FASTCALL Event::GetDesc() const
{
	ASSERT(this);
	ASSERT_DIAG();

	return ev.desc;
}

//---------------------------------------------------------------------------
//
//	Time setting
//
//---------------------------------------------------------------------------
void FASTCALL Event::SetTime(DWORD hus)
{
	ASSERT(this);
	ASSERT_DIAG();

	// Set time (0 stops event)
	ev.time = hus;

	// Set counter. Elapsed time increases remainder
	// Note: remainder will not decrease here (see Scheduler::GetPassedTime())
	ev.remain = ev.time;
	if (ev.remain > 0) {
		ev.remain += ev.scheduler->GetPassedTime();
	}
}

//---------------------------------------------------------------------------
//
//	Time execution
//
//---------------------------------------------------------------------------
void FASTCALL Event::Exec(DWORD hus)
{
	ASSERT(this);
	ASSERT_DIAG();

	// If time is not set, do nothing
	if (ev.time == 0) {
		ASSERT(ev.remain == 0);
		return;
	}

	// Execute when time arrives (over or equal is error)
	if (ev.remain <= hus) {
		// Reset time to load
		ev.remain = ev.time;

		// Callback execution
		ASSERT(ev.device);

		// If callback returns FALSE, stop
		if (!ev.device->Callback(this)) {
			ev.time = 0;
			ev.remain = 0;
		}
	}
	else {
		// Decrement
		ev.remain -= hus;
	}
}
