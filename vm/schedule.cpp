//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ Scheduler ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "schedule.h"
#include "vm.h"
#include "log.h"
#include "cpu.h"
#include "event.h"
#include "dmac.h"
#include "core_soft.h"
#include "config.h"
#include "fileio.h"

//===========================================================================
//
//	Scheduler
//
//===========================================================================
//#define SCHEDULER_LOG

//---------------------------------------------------------------------------
//
//	Event detection/update assembly
//
//---------------------------------------------------------------------------
#if defined(_MSC_VER) && defined(_M_IX86)
#define SCHEDULER_ASM
#endif	// _MSC_VER

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Scheduler::Scheduler(VM *p) : Device(p)
{
	int i;

	// Device ID initialization
	dev.id = MAKEID('S', 'C', 'H', 'E');
	dev.desc = "Scheduler";

	// Breakpoint initialization
	for (i=0; i<BreakMax; i++) {
		breakp[i].use = FALSE;
		breakp[i].addr = 0;
		breakp[i].enable = FALSE;
		breakp[i].time = 0;
		breakp[i].count = 0;
	}

	// Time
	sch.total = 0;
	sch.one = 0;
	sch.sound = 0;

	// CPU
	sch.clock = 0;
	sch.speed = 2200;
	sch.cycle = 0;
	sch.time = 0;

	// Breakpoint
	sch.brk = FALSE;
	sch.check = FALSE;

	// Event
	sch.first = NULL;
	sch.exec = FALSE;

	// Device
	cpu = NULL;
	dmac = NULL;

	// Others
	dma_active = FALSE;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL Scheduler::Init()
{
	ASSERT(this);

	// Base class
	if (!Device::Init()) {
		return FALSE;
	}

	// CPU search
	ASSERT(!cpu);
	cpu = (CPU*)vm->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(cpu);
	
	// DMAC search
	ASSERT(!dmac);
	dmac = (DMAC*)vm->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
ASSERT(dmac);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::Cleanup()
{
	ASSERT(this);
ASSERT_DIAG();

	// To base class
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::Reset()
{
ASSERT(this);
ASSERT_DIAG();

	LOG0(Log::Normal, "Reset");

	// Time reset (excluding sound)
	sch.total = 0;
	sch.one = 0;

	// CPU cycle reset
	sch.cycle = 0;
	sch.time = 0;

	// Not executing event
	sch.exec = FALSE;

	// No DMA processing
	dma_active = FALSE;

	// CPU speed setting must be between 0-5 (for INFO.RAM timer compatibility)
ASSERT((sch.clock >= 0) && (sch.clock <= 5));
	SetCPUSpeed(ClockTable[sch.clock]);
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Scheduler::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

ASSERT(this);
ASSERT(fio);
ASSERT_DIAG();

	LOG0(Log::Normal, "Save");

	// Breakpoint array size
	sz = sizeof(breakp);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Breakpoint data save
	if (!fio->Write(breakp, (int)sz)) {
		return FALSE;
	}

	// Scheduler structure size
	sz = sizeof(scheduler_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Scheduler data save
	if (!fio->Write(&sch, (int)sz)) {
		return FALSE;
	}

	// Cycle table save
	if (!fio->Write(CycleTable, sizeof(CycleTable))) {
		return FALSE;
	}

	// dma_active save (version 2.01)
	if (!fio->Write(&dma_active, sizeof(dma_active))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Scheduler::Load(Fileio *fio, int ver)
{
	size_t sz;
	Event *first;

ASSERT(this);
ASSERT(fio);
ASSERT(ver >= 0x200);
ASSERT_DIAG();

	LOG0(Log::Normal, "Load");

	// Event pointer temporary
	first = sch.first;

	// Breakpoint array size verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(breakp)) {
		return FALSE;
	}

	// Breakpoint data load
	if (!fio->Read(breakp, (int)sz)) {
		return FALSE;
	}

	// Scheduler structure size verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(scheduler_t)) {
		return FALSE;
	}

	// Scheduler data load
	if (!fio->Read(&sch, (int)sz)) {
		return FALSE;
	}

	// Cycle table load
	if (!fio->Read(CycleTable, sizeof(CycleTable))) {
		return FALSE;
	}

	// Event pointer restore
	sch.first = first;

	// Scheduler::Save always writes dma_active. Reading it always maintains
	// the symmetry of the savestate stream.
	if (!fio->Read(&dma_active, sizeof(dma_active))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::ApplyCfg(const Config *config)
{
ASSERT(this);
ASSERT(config);
ASSERT_DIAG();

	LOG0(Log::Normal, "Apply configuration");

	// Check system clock configuration
	if (sch.clock != config->system_clock) {
		// Configuration changed, so recalculate cycle table
		sch.clock = config->system_clock;
ASSERT((sch.clock >= 0) && (sch.clock <= 5));
		SetCPUSpeed(ClockTable[sch.clock]);
	}
}

#if defined(_DEBUG)
//---------------------------------------------------------------------------
//
//	Assertion
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::AssertDiag() const
{
ASSERT(this);
ASSERT(GetID() == MAKEID('S', 'C', 'H', 'E'));
ASSERT(cpu);
ASSERT(cpu->GetID() == MAKEID('C', 'P', 'U', ' '));
ASSERT(dmac);
ASSERT(dmac->GetID() == MAKEID('D', 'M', 'A', 'C'));
}
#endif	// _DEBUG

//---------------------------------------------------------------------------
//
//	Internal data reference
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::GetScheduler(scheduler_t *buffer) const
{
ASSERT(this);
ASSERT(buffer);
ASSERT_DIAG();

	// Copy internal data
	*buffer = sch;
}

//---------------------------------------------------------------------------
//
//	Execute
//
//---------------------------------------------------------------------------
DWORD FASTCALL Scheduler::Exec(DWORD hus)
{
	int cycle;
	DWORD result;
	DWORD dcycle;

ASSERT(this);
ASSERT(hus > 0);
ASSERT_DIAG();

	// When breakpoints are not being checked
	if (!sch.check) {
		// Find minimum event
#if defined(SCHEDULER_ASM)
		sch.one = GetMinEvent(hus);
#else
		sch.one = GetMinRemain(hus);
#endif	// SCHEDULER_ASM

		// sch.one + sch.time must be less than cycle count that can be executed
ASSERT((sch.one + sch.time) < 0x1000);
		cycle = CycleTable[sch.one + sch.time];
		if (cycle > sch.cycle) {

			// Can execute this cycle count
			cycle -= sch.cycle;
			if (!dma_active) {
				// Normal
				result = cpu->Exec(cycle);
			}
			else {
				// DMAC auto DMA active
				dcycle = dmac->AutoDMA(cycle);
				if (dcycle != 0) {
					// CPU execution with different timing
					result = cpu->Exec(dcycle);
				}
				else {
					// All DMA processing done
					result = cycle;
				}
			}

			// Normal end
			if (result < 0x80000000) {
				// Update sch.time, sch.cycle
				sch.cycle += result;
				sch.time += sch.one;

				// Execute events
				ExecEvent(sch.one);

				if (sch.time < 200) {
					return sch.one;
				}

				// Time sync
				while (sch.time >= 200) {
					if ((DWORD)sch.cycle < sch.speed) {
						break;
					}
					sch.time -= 200;
					sch.cycle -= sch.speed;
				}

				// Breakpoint check
				if (!sch.brk) {
					return sch.one;
				}

#if defined(SCHEDULER_LOG)
				LOG0(Log::Normal, "Break");
#endif	// SCHEDULER_LOG
				sch.brk = FALSE;
				return (DWORD)(sch.one | 0x80000000);
			}
			else {
				// Execution error
				result &= 0x7fffffff;

				if ((int)result > cycle) {
					// Update sch.time, sch.cycle
					sch.time += sch.one;
					sch.cycle += result;

					// Execute events
					ExecEvent(sch.one);

					while (sch.time >= 200) {
						if ((DWORD)sch.cycle < sch.speed) {
							break;
						}
						sch.time -= 200;
						sch.cycle -= sch.speed;
					}
					// Execution error, event finished
					return 0x80000000;
				}
				// Full execution before cpu error
				sch.cycle += result;
				// Execution error, event not finished
				return 0x80000000;
			}
		}
		else {

			// Cannot execute now, only advance time
			sch.time += sch.one;
			ExecEvent(sch.one);

			if (sch.time < 200) {
				return sch.one;
			}

			// Update sch.time
			while (sch.time >= 200) {
				if ((DWORD)sch.cycle < sch.speed) {
					break;
				}
				sch.time -= 200;
				sch.cycle -= sch.speed;
			}

			// Normal end, event finished
			return sch.one;
		}

	}

	// Trace
	for (;;) {
		result = Trace(hus);

		switch (result) {
			// Normal end, event finished
			case 0:
				return sch.one;

			// Normal possible, event finished
			case 1:
				if (sch.brk) {
#if defined(SCHEDULER_LOG)
					LOG0(Log::Normal, "Break");
#endif	// SCHEDULER_LOG
					sch.brk = FALSE;
					return 0x80000000;
				}
				if (IsBreak(cpu->GetPC()) != -1) {
					OnBreak(cpu->GetPC());
					return 0x80000000;
				}
				return sch.one;

			// Execution error, event not finished
			case 2:
				if (sch.brk) {
#if defined(SCHEDULER_LOG)
					LOG0(Log::Normal, "Break");
#endif	// SCHEDULER_LOG
					sch.brk = FALSE;
					return 0x80000000;
				}
				if (IsBreak(cpu->GetPC()) != -1) {
					OnBreak(cpu->GetPC());
					return 0x80000000;
				}
				break;

			// Execution error
			case 3:
				if (sch.brk) {
#if defined(SCHEDULER_LOG)
					LOG0(Log::Normal, "Break");
#endif	// SCHEDULER_LOG
					sch.brk = FALSE;
				}
				return 0x80000000;

			// Others
			default:
				ASSERT(FALSE);
				return sch.one;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Trace
//
//---------------------------------------------------------------------------
DWORD FASTCALL Scheduler::Trace(DWORD hus)
{
	int cycle;
	DWORD result;

ASSERT(this);
ASSERT(hus > 0);
ASSERT_DIAG();

	// Find minimum event
#if defined(SCHEDULER_ASM)
	sch.one = GetMinEvent(hus);
#else
	sch.one = GetMinRemain(hus);
#endif	// SCHEDULER_ASM

	// sch.one + sch.time must be less than cycle count that can be executed
ASSERT((sch.one + sch.time) < 0x1000);
	cycle = CycleTable[sch.one + sch.time];
	if (cycle <= sch.cycle) {
		// Cannot execute now, only advance time
		sch.time += sch.one;
		ExecEvent(sch.one);

		// Update sch.time
		while (sch.time >= 200) {
			sch.time -= 200;
			sch.cycle -= sch.speed;
		}
		// Normal end, event finished
		return 0;
	}

	// Available cycle count to execute
	cycle -= sch.cycle;

	// Execute 1 cycle and see
	if (!dma_active) {
		// Normal
		result = cpu->Exec(1);
	}
	else {
		// DMAC auto DMA active
		result = dmac->AutoDMA(1);
		if (result != 0) {
			result = cpu->Exec(result);
		}
		else {
			result = 1;
		}
	}
	if (result >= 0x80000000) {
		// Execution error
		return 3;
	}

	// result >= cycle means event can be executed
	if ((int)result >= cycle) {
		// Update sch.time, sch.cycle
		sch.cycle += result;
		sch.time += sch.one;

		// Execute events
		ExecEvent(sch.one);

		while (sch.time >= 200) {
			sch.time -= 200;
			sch.cycle -= sch.speed;
		}
		// Normal possible, event finished
		return 1;
	}

	// Not enough time, so events are still pending
	// Update sch.cycle
	sch.cycle += result;

	// Execution error, event not finished
	return 2;
}

//---------------------------------------------------------------------------
//
//	CPU speed setting
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::SetCPUSpeed(DWORD speed)
{
	int i;
	DWORD cycle;

ASSERT(this);
ASSERT(speed > 0);
ASSERT_DIAG();

	LOG2(Log::Detail, "CPU speed setting %d.%02dMHz", speed / 100, (speed % 100));

	// Set CPU speed
	sch.speed = speed;

	// Calculate cycle count for 0-2048us at 0.5us intervals
	for (i=0; i<0x1000; i++) {
		cycle = (DWORD)i;
		cycle *= speed;
		cycle /= 200;
		CycleTable[i] = cycle;
	}
}

//---------------------------------------------------------------------------
//
//	Get elapsed time
//
//---------------------------------------------------------------------------
DWORD FASTCALL Scheduler::GetPassedTime() const
{
	DWORD hus;

ASSERT(this);
ASSERT_DIAG();

	// Return 0 if executing event
	if (sch.exec) {
		return 0;
	}

	// Calculate time from execution cycle and cpu_cycle
	hus = cpu->GetCycle() + sch.cycle;
	hus *= 200;
	hus /= sch.speed;
	hus -= sch.time;

	// If larger than one, use smaller (minimum)
	if (sch.one < hus) {
		hus = sch.one;
	}

	// Return in hus units
	return hus;
}

//---------------------------------------------------------------------------
//
//	Breakpoint setting
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::SetBreak(DWORD addr, BOOL enable)
{
	int i;
	BOOL flag;

ASSERT(this);
ASSERT(addr <= 0xffffff);
ASSERT_DIAG();

#if defined(SCHEDULER_LOG)
	LOG2(Log::Normal, "Breakpoint setting $%06X enable=%d", addr, enable);
#endif	// SCHEDULER_LOG

	flag = FALSE;

	// Search existing
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].addr == addr) {
				// Only change flag
				breakp[i].enable = enable;
				flag = TRUE;
				break;
			}
		}
	}

	if (!flag) {
		// Empty slot
		for (i=0; i<BreakMax; i++) {
			if (!breakp[i].use) {
				// Set
				breakp[i].use = TRUE;
				breakp[i].addr = addr;
				breakp[i].enable = enable;
				breakp[i].time = 0;
				breakp[i].count = 0;
				break;
			}
		}
	}

	// Set valid flag
	flag = FALSE;
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].enable) {
				// Valid breakpoint exists
				flag = TRUE;
				break;
			}
		}
	}
	sch.check = flag;
}

//---------------------------------------------------------------------------
//
//	Breakpoint deletion
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::DelBreak(DWORD addr)
{
	int i;
	BOOL flag;

ASSERT(this);
ASSERT(addr <= 0xffffff);
ASSERT_DIAG();

#if defined(SCHEDULER_LOG)
	LOG1(Log::Normal, "Breakpoint deletion $%06X", addr);
#endif	// SCHEDULER_LOG

	// Search
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].addr == addr) {
				// Remove
				breakp[i].use = FALSE;
				break;
			}
		}
	}

	// Set valid flag
	flag = FALSE;
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].enable) {
				// Valid breakpoint exists
				flag = TRUE;
				break;
			}
		}
	}
	sch.check = flag;
}

//---------------------------------------------------------------------------
//
//	Breakpoint reference
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::GetBreak(int index, breakpoint_t *buf) const
{
ASSERT(this);
ASSERT((index >= 0) && (index < BreakMax));
ASSERT(buf);
ASSERT_DIAG();

	// Copy
	*buf = breakp[index];
}

//---------------------------------------------------------------------------
//
//	Breakpoint enable/disable
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::EnableBreak(int index, BOOL enable)
{
ASSERT(this);
ASSERT((index >= 0) && (index < BreakMax));
ASSERT(breakp[index].use);
ASSERT_DIAG();

	breakp[index].enable = enable;
}

//---------------------------------------------------------------------------
//
//	Breakpoint counter clear
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::ClearBreak(int index)
{
ASSERT(this);
ASSERT((index >= 0) && (index < BreakMax));
ASSERT(breakp[index].use);
ASSERT_DIAG();

	breakp[index].count = 0;
	breakp[index].time = 0;
}

//---------------------------------------------------------------------------
//
//	Breakpoint address change
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::AddrBreak(int index, DWORD addr)
{
ASSERT(this);
ASSERT((index >= 0) && (index < BreakMax));
ASSERT(addr <= 0xffffff);
ASSERT(breakp[index].use);
ASSERT_DIAG();

	breakp[index].addr = addr;
}

//---------------------------------------------------------------------------
//
//	Breakpoint address check
//
//---------------------------------------------------------------------------
int FASTCALL Scheduler::IsBreak(DWORD addr, BOOL any) const
{
	int i;

ASSERT(this);
ASSERT(addr <= 0xffffff);
ASSERT_DIAG();

	// Check flag at top
	if (!sch.check) {
		return -1;
	}

	// Search
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].addr == addr) {
				// Valid/Invalid or enabled
				if (any || breakp[i].enable) {
					return i;
				}
			}
		}
	}

	// Breakpoint exists but not enabled
	return -1;
}

//---------------------------------------------------------------------------
//
//	Breakpoint address callback
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::OnBreak(DWORD addr)
{
	int i;

ASSERT(this);
ASSERT(addr <= 0xffffff);
ASSERT(sch.check);
ASSERT_DIAG();

	// Search
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].addr == addr) {
				break;
			}
		}
	}
ASSERT(i < BreakMax);

	// Time counter, count up
	breakp[i].time = GetTotalTime();
	breakp[i].count++;
}

//---------------------------------------------------------------------------
//
//	Event addition
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::AddEvent(Event *event)
{
	Event *p;

ASSERT(this);
ASSERT(event);
ASSERT_DIAG();

#if defined(SCHEDULER_LOG)
	LOG4(Log::Normal, "Event addition Device=%c%c%c%c",
					(char)(event->GetDevice()->GetID() >> 24),
					(char)(event->GetDevice()->GetID() >> 16),
					(char)(event->GetDevice()->GetID() >> 8),
					(char)(event->GetDevice()->GetID()));
	LOG1(Log::Normal, "Event addition %s", event->GetDesc());
#endif	// SCHEDULER_LOG

	// First event
	if (!sch.first) {
		// First event
		sch.first = event;
		event->SetNextEvent(NULL);

#if defined(SCHEDULER_ASM)
		// Notify
		NotifyEvent(sch.first);
#endif	// SCHEDULER_ASM
		return;
	}

	// Search last event
	p = sch.first;
	while (p->GetNextEvent()) {
		p = p->GetNextEvent();
	}

	// p is last event, so add to this
	p->SetNextEvent(event);
	event->SetNextEvent(NULL);

#if defined(SCHEDULER_ASM)
	// Notify
	NotifyEvent(sch.first);
#endif	// SCHEDULER_ASM
}

//---------------------------------------------------------------------------
//
//	Event deletion
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::DelEvent(Event *event)
{
	Event *p;
	Event *prev;

ASSERT(this);
ASSERT(event);
ASSERT_DIAG();

#if defined(SCHEDULER_LOG)
	LOG4(Log::Normal, "Event deletion Device=%c%c%c%c",
					(char)(event->GetDevice()->GetID() >> 24),
					(char)(event->GetDevice()->GetID() >> 16),
					(char)(event->GetDevice()->GetID() >> 8),
					(char)(event->GetDevice()->GetID()));
	LOG1(Log::Normal, "Event deletion %s", event->GetDesc());
#endif	// SCHEDULER_LOG

	// First event
	if (sch.first == event) {
		// First event next becomes first, disconnect from event
		sch.first = event->GetNextEvent();
		event->SetNextEvent(NULL);

#if defined(SCHEDULER_ASM)
		// Notify
		NotifyEvent(sch.first);
#endif	// SCHEDULER_ASM
		return;
	}

	// Search this event in list
	p = sch.first;
	prev = p;
	while (p) {
		// Search
		if (p == event) {
			prev->SetNextEvent(event->GetNextEvent());
			event->SetNextEvent(NULL);

#if defined(SCHEDULER_ASM)
			// Notify
			NotifyEvent(sch.first);
#endif	// SCHEDULER_ASM
			return;
		}

		// Next
		prev = p;
		p = p->GetNextEvent();
	}

	// All events not found (should not happen)
ASSERT(FALSE);
}

//---------------------------------------------------------------------------
//
//	Event existence check
//
//---------------------------------------------------------------------------
BOOL FASTCALL Scheduler::HasEvent(Event *event) const
{
	Event *p;

ASSERT(this);
ASSERT(event);
ASSERT_DIAG();

	// Initialize
	p = sch.first;

	// Check all events
	while (p) {
		// Search
		if (p == event) {
			return TRUE;
		}

		// Next
		p = p->GetNextEvent();
	}

	// This event is not in the list
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Get event count
//
//---------------------------------------------------------------------------
int FASTCALL Scheduler::GetEventNum() const
{
	int num;
	Event *p;

ASSERT(this);
ASSERT_DIAG();

	// Initialize
	num = 0;
	p = sch.first;

	// Check all events
	while (p) {
		num++;

		// Next
		p = p->GetNextEvent();
	}

	// Return event count
	return num;
}

//---------------------------------------------------------------------------
//
//	Find minimum event
//	NOTE: Use for non-assembly version
//
//---------------------------------------------------------------------------
DWORD FASTCALL Scheduler::GetMinRemain(DWORD hus)
{
	Event *p;
	DWORD minimum;
	DWORD remain;

ASSERT(this);
ASSERT(hus > 0);
ASSERT_DIAG();

	// Event pointer initialization
	p = sch.first;

	// Initialize
	minimum = hus;

	// Loop
	while (p) {
		// Remaining time get
		remain = p->GetRemain();

		// Valid?
		if (remain == 0) {
			// Next
			p = p->GetNextEvent();
			continue;
		}

		// Minimum check
		if (remain >= minimum) {
			p = p->GetNextEvent();
			continue;
		}

		// Minimum
		minimum = remain;
		p = p->GetNextEvent();
	}

	return minimum;
}

//---------------------------------------------------------------------------
//
//	Execute events
//	NOTE: Use for non-assembly version
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::ExecEvent(DWORD hus)
{
#if !defined(SCHEDULER_ASM)
	Event *p;
#endif	// !SCHEDULER_ASM

ASSERT(this);
ASSERT(hus >= 0);
ASSERT_DIAG();

	// Event execution start
	sch.exec = TRUE;

	// Virtual time increment, sound time increment
	sch.total += hus;
	sch.sound += hus;

#if defined(SCHEDULER_ASM)
	SubExecEvent(hus);
	sch.exec = FALSE;
#else

	// Event pointer initialization
	p = sch.first;

	// Execute events
	while (p) {
		p->Exec(hus);
		p = p->GetNextEvent();
	}

	// Event execution end
	sch.exec = FALSE;
#endif
}

//---------------------------------------------------------------------------
//
//	Clock table
//
//---------------------------------------------------------------------------
const DWORD Scheduler::ClockTable[] = {
	979,			// 10MHz
	1171,			// 12MHz
	1460,			// 15MHz
	1556,			// 16MHz
	1689,			// 17.4MHz
	2200			// 22MHz
};

//---------------------------------------------------------------------------
//
//	Cycle table
//
//---------------------------------------------------------------------------
int Scheduler::CycleTable[0x1000];
