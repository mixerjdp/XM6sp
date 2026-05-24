//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ Scheduler ]
//
//---------------------------------------------------------------------------

#if !defined(scheduler_h)
#define scheduler_h

#include "device.h"
#include "musashi_adapter.h"

#define SCHEDULER_FASTWAIT

//===========================================================================
//
//	Scheduler
//
//===========================================================================
class Scheduler : public Device
{
public:
	// Breakpoint structure
	typedef struct{
		BOOL use;						// In use flag
		DWORD addr;					// Breakpoint address
		BOOL enable;					// Enable flag
		DWORD time;					// Last hit time
		DWORD count;					// Hit count
	} breakpoint_t;

	// Scheduler internal structure
	typedef struct {
		// Time
		DWORD total;					// Total execution time (us)
		DWORD one;						// One event time (us)
		DWORD sound;					// Sound time (us)

		// CPU
		int clock;						// CPU clock (0-5)
		DWORD speed;					// CPU speed (clock frequency)
		int cycle;						// CPU cycle
		DWORD time;					// CPU execution time

		// Breakpoint
		BOOL brk;						// Break flag
		BOOL check;					// Breakpoint check flag

		// Event
		Event *first;					// First event
		BOOL exec;						// Event execution flag
	} scheduler_t;

	// Breakpoint maximum
	enum {
		BreakMax = 8					// Maximum breakpoint count
	};

public:
	// Constructor
	Scheduler(VM *p);
										// Initialization
	BOOL FASTCALL Init();
										// Cleanup
	void FASTCALL Cleanup();
										// Reset
	void FASTCALL Reset();
										// Save
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Load
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Apply configuration
	void FASTCALL ApplyCfg(const Config *config);
										// Assertion
#if defined(_DEBUG)
	void FASTCALL AssertDiag() const;
										// Assert
#endif	// _DEBUG

	// Scheduler API
	void FASTCALL GetScheduler(scheduler_t *buffer) const;
										// Execute
	DWORD FASTCALL Exec(DWORD hus);
										// Trace
	DWORD FASTCALL Trace(DWORD hus);
										// Execute events only
	void FASTCALL ExecEventsOnly(DWORD hus) { ExecEvent(hus); }
										// Break (without CPU)
	void FASTCALL Break()				{ sch.brk = TRUE; }
										// Wait for cycle
#ifdef SCHEDULER_FASTWAIT
	void FASTCALL Wait(DWORD cycle)		{ sch.cycle += cycle; if (::s68000iocycle != (DWORD)-1) { ::s68000iocycle -= cycle; ::musashi_adjust_timeslice(-(int)cycle); } }
#else
	void FASTCALL Wait(DWORD cycle)		{ ::s68000wait(cycle); sch.cycle += cycle; }
#endif

	// CPU cycle access
	int FASTCALL GetCPUCycle() const	{ return sch.cycle; }
										// Set CPU cycle
	void FASTCALL SetCPUCycle(int cycle) { sch.cycle = cycle; }
										// Set CPU time

	// Time access
	DWORD FASTCALL GetTotalTime() const	{ return (GetPassedTime() + sch.total); }
										// Get one event time
	DWORD FASTCALL GetOneTime() const	{ return sch.one; }
										// Get passed time
	DWORD FASTCALL GetPassedTime() const;
										// Get CPU speed
	DWORD FASTCALL GetCPUSpeed() const	{ return sch.speed; }
										// Set CPU speed
	void FASTCALL SetCPUSpeed(DWORD speed);
										// Get sound time
	DWORD FASTCALL GetSoundTime() const	{ return sch.sound; }
										// Set sound time
	void FASTCALL SetSoundTime(DWORD hus) { sch.sound = hus; }

	// Breakpoint functions
	void FASTCALL SetBreak(DWORD addr, BOOL enable = TRUE);
										// Delete breakpoint
	void FASTCALL DelBreak(DWORD addr);
										// Get breakpoint
	void FASTCALL GetBreak(int index, breakpoint_t *buf) const;
										// Enable/disable breakpoint
	void FASTCALL EnableBreak(int index, BOOL enable = TRUE);
										// Clear breakpoint
	void FASTCALL ClearBreak(int index);
										// Change breakpoint address
	void FASTCALL AddrBreak(int index, DWORD addr);
										// Check breakpoint
	int FASTCALL IsBreak(DWORD addr, BOOL any = FALSE) const;

	// Event functions
	void FASTCALL AddEvent(Event *event);
										// Delete event
	void FASTCALL DelEvent(Event *event);
										// Check event exists
	BOOL FASTCALL HasEvent(Event *event) const;
										// Get first event
	Event* FASTCALL GetFirstEvent()	const { return sch.first; }
										// Get event count
	int FASTCALL GetEventNum() const;
										// Get event count

	// DMA state
	BOOL dma_active;
										// DMAC auto DMA active flag

private:
	// Find minimum remaining event (non-ASM version)
	DWORD FASTCALL GetMinRemain(DWORD hus);
										// Execute events
	void FASTCALL ExecEvent(DWORD hus);
										// Breakpoint callback
	void FASTCALL OnBreak(DWORD addr);

	// Internal data
	breakpoint_t breakp[BreakMax];
										// Breakpoint array
	scheduler_t sch;
										// Scheduler structure

	// Device
	CPU *cpu;
										// CPU
	DMAC *dmac;
										// DMAC

	// Table
	static const DWORD ClockTable[];
										// Clock table (MHz)
	static int CycleTable[0x1000];
										// Cycle table (0-2048us)
};

#endif	// scheduler_h