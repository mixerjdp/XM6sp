//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Musashi Adapter - Compatibility layer replacing Starscream
//	[ musashi_adapter.cpp ]
//
//---------------------------------------------------------------------------

// Use the XM6-specific Musashi configuration
#define MUSASHI_CNF "m68kconf_xm6.h"

#include <windows.h>
#include <string.h>
#include <setjmp.h>

// Minimal type compatibility (from xm6.h)
#ifndef FASTCALL
#define FASTCALL __fastcall
#endif

#include "musashi_adapter.h"
#include "m68k.h"
#include "m68kcpu.h"
#include <stdio.h>

extern "C" void musashi_trace_hook(unsigned int pc)
{
/*	static FILE* trace_file = NULL;
	static int trace_count = 0;

	if (trace_count >= 1000) return;

	if (!trace_file) {
		trace_file = fopen("cpu_trace.log", "w");
		if (!trace_file) return;
	}

	unsigned int d0 = m68k_get_reg(NULL, M68K_REG_D0);
	unsigned int a0 = m68k_get_reg(NULL, M68K_REG_A0);
	unsigned int sp = m68k_get_reg(NULL, M68K_REG_SP);
	unsigned int sr = m68k_get_reg(NULL, M68K_REG_SR);

	fprintf(trace_file, "[TRACE %05d] PC=%08X SP=%08X SR=%04X D0=%08X A0=%08X\n", 
			trace_count, pc, sp, sr, d0, a0);
	fflush(trace_file);
	
	trace_count++;
	if (trace_count >= 1000) {
		fprintf(trace_file, "--- TRACE END ---\n");
		fclose(trace_file);
	}*/
}

//---------------------------------------------------------------------------
//
//	Global variables
//
//---------------------------------------------------------------------------

// Starscream-compatible context (synced with Musashi)
struct S68000CONTEXT s68000context;

// I/O cycle counter (used by Scheduler::Wait)
unsigned int s68000iocycle = (unsigned int)-1;

// Current function code (user/supervisor/program/data)
unsigned int musashi_current_fc = 0;

// Flag to bypass function code during CPU reset vector fetch
bool musashi_is_resetting = false;

static int GetHighestPendingLegacyIRQ(void)
{
	unsigned char mask = s68000context.interrupts[0];
	for (int level = 7; level >= 1; level--) {
		if (mask & (1u << level)) {
			return level;
		}
	}
	return 0;
}

// Flag to track if we're inside m68k_execute
static int musashi_executing = 0;

// Tracks cycles consumed by Scheduler::Wait() during execution.
// m68k_modify_timeslice(-N) reduces remaining_cycles, which inflates
// the return value of m68k_execute(). We must subtract these to get
// the real CPU instruction cycles (matching Starscream's odometer).
static int musashi_wait_cycles = 0;


//---------------------------------------------------------------------------
//
//	Internal: Sync Musashi state to s68000context
//
//---------------------------------------------------------------------------
static void SyncContextFromMusashi(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		s68000context.dreg[i] = m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_D0 + i));
		s68000context.areg[i] = m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_A0 + i));
	}

	s68000context.pc = m68k_get_reg(NULL, M68K_REG_PC);
	s68000context.sr = (unsigned short)m68k_get_reg(NULL, M68K_REG_SR);
	s68000context.asp = m68k_get_reg(NULL, M68K_REG_USP);

	// Odometer: cycles consumed so far in this run
	// Must subtract musashi_wait_cycles to avoid double-counting.
	// Wait() already adds to sch.cycle; if the odometer also includes
	// Wait cycles, GetPassedTime() = GetCycle() + sch.cycle double-counts.
	if (musashi_executing) {
		int raw = (int)m68k_cycles_run();
		int adjusted = raw - musashi_wait_cycles;
		if (adjusted < 0) adjusted = 0;
		s68000context.odometer = (unsigned)adjusted;
	}
}

//---------------------------------------------------------------------------
//
//	Internal: Sync s68000context changes to Musashi
//
//---------------------------------------------------------------------------
static void SyncContextToMusashi(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		m68k_set_reg((m68k_register_t)(M68K_REG_D0 + i), s68000context.dreg[i]);
		m68k_set_reg((m68k_register_t)(M68K_REG_A0 + i), s68000context.areg[i]);
	}

	m68k_set_reg(M68K_REG_PC, s68000context.pc);
	m68k_set_reg(M68K_REG_SR, (unsigned int)s68000context.sr);
	m68k_set_reg(M68K_REG_USP, s68000context.asp);
}

//---------------------------------------------------------------------------
//
//	s68000init - Initialize CPU core
//
//---------------------------------------------------------------------------
int s68000init(void)
{
	// Initialize Musashi
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);

	// Clear the context
	memset(&s68000context, 0, sizeof(s68000context));

	return 0;
}

//---------------------------------------------------------------------------
//
//	s68000reset - Reset CPU
//
//---------------------------------------------------------------------------
unsigned s68000reset(void)
{
	musashi_is_resetting = true;
	m68k_pulse_reset();
	musashi_is_resetting = false;
	SyncContextFromMusashi();
	s68000context.odometer = 0;
	return 0;
}

//---------------------------------------------------------------------------
//
//	s68000exec - Execute n cycles
//	Returns: odometer value, bit 31 set if exception occurred
//
//---------------------------------------------------------------------------
unsigned s68000exec(int n)
{
	int cycles_used;

	// Track that we're executing
	musashi_executing = 1;
	musashi_wait_cycles = 0;

	// Set up I/O cycle counter for Scheduler::Wait
	// CRITICAL: Must be set to 'n' (not -1) so that Scheduler::Wait()
	// can subtract device wait cycles during execution.
	s68000iocycle = (unsigned int)n;

	// Execute
	cycles_used = m68k_execute(n);

	// Subtract Wait() cycles from the result.
	// m68k_execute returns (initial - remaining). When Wait() calls
	// m68k_modify_timeslice(-N), remaining decreases, inflating the
	// return. We must subtract these to match Starscream's odometer
	// which only counted real CPU instruction cycles.
	cycles_used -= musashi_wait_cycles;
	if (cycles_used < 0) cycles_used = 0;

	// Sync state back to context
	musashi_executing = 0;
	SyncContextFromMusashi();
	s68000context.odometer = (unsigned)cycles_used;

	return (unsigned)cycles_used;
}

//---------------------------------------------------------------------------
//
//	s68000interrupt - Request an interrupt
//	Returns 0 if accepted, non-zero if rejected
//
//---------------------------------------------------------------------------
int s68000interrupt(int level, int vector)
{
	if (level < 1 || level > 7) {
		return 2; // bad input (Starscream contract)
	}
	if (vector > 255 || vector < -2) {
		return 2; // bad input (Starscream contract)
	}

	{
		unsigned char bit = (unsigned char)(1u << level);
		if (s68000context.interrupts[0] & bit) {
			return 1; // duplicate level request rejected
		}

		s68000context.interrupts[0] |= bit;
		if (vector == -2) {
			s68000context.interrupts[level] = 0x18; // spurious
		}
		else if (vector < 0) {
			s68000context.interrupts[level] = (unsigned char)(0x18 + level); // autovector
		}
		else {
			s68000context.interrupts[level] = (unsigned char)(vector & 0xFF);
		}
	}

	m68k_set_irq(GetHighestPendingLegacyIRQ());
	return 0;
}

//---------------------------------------------------------------------------
//
//	s68000flushInterrupts - Process pending interrupts
//
//---------------------------------------------------------------------------
void s68000flushInterrupts(void)
{
	// Musashi handles this automatically during execution
	// No explicit flush needed
}

//---------------------------------------------------------------------------
//
//	s68000GetContext / s68000SetContext
//
//---------------------------------------------------------------------------
int s68000GetContextSize(void)
{
	return sizeof(struct S68000CONTEXT);
}

void s68000GetContext(void *context)
{
	SyncContextFromMusashi();
	memcpy(context, &s68000context, sizeof(struct S68000CONTEXT));
}

void s68000SetContext(void *context)
{
	memcpy(&s68000context, context, sizeof(struct S68000CONTEXT));
	SyncContextToMusashi();
}

//---------------------------------------------------------------------------
//
//	Odometer functions
//
//---------------------------------------------------------------------------
unsigned s68000readOdometer(void)
{
	if (musashi_executing) {
		// Subtract Wait() cycles to avoid double-counting in GetPassedTime()
		int raw = (int)m68k_cycles_run();
		int adjusted = raw - musashi_wait_cycles;
		if (adjusted < 0) adjusted = 0;
		return (unsigned)adjusted;
	}
	return s68000context.odometer;
}

unsigned s68000tripOdometer(void)
{
	unsigned odo = s68000readOdometer();
	s68000context.odometer = 0;
	return odo;
}

unsigned s68000controlOdometer(int n)
{
	unsigned odo = s68000readOdometer();
	if (n >= 0) {
		s68000context.odometer = (unsigned)n;
	}
	return odo;
}

//---------------------------------------------------------------------------
//
//	s68000releaseTimeslice - End execution of current timeslice
//
//---------------------------------------------------------------------------
void s68000releaseTimeslice(void)
{
	if (musashi_executing) {
		m68k_end_timeslice();
	}
}

//---------------------------------------------------------------------------
//
//	s68000readPC - Read current program counter
//
//---------------------------------------------------------------------------
unsigned s68000readPC(void)
{
	return m68k_get_reg(NULL, M68K_REG_PC);
}

//---------------------------------------------------------------------------
//
//	s68000fetch - Read opcode at address
//
//---------------------------------------------------------------------------
int s68000fetch(unsigned address)
{
	// Read a word from the given address
	return (int)m68k_read_memory_16(address);
}

//---------------------------------------------------------------------------
//
//	s68000wait - Wait/consume cycles
//
//---------------------------------------------------------------------------
unsigned s68000wait(unsigned cycle)
{
	if (musashi_executing) {
		m68k_modify_timeslice(-(int)cycle);
		musashi_wait_cycles += (int)cycle;
	}
	return 0;
}

//---------------------------------------------------------------------------
//
//	musashi_adjust_timeslice - Wrapper for Scheduler::Wait fast path
//	Called from schedule.h inline to propagate cycle consumption to Musashi
//
//---------------------------------------------------------------------------
void musashi_adjust_timeslice(int cycles)
{
	if (musashi_executing) {
		m68k_modify_timeslice(cycles);
		// Track how many cycles Wait() consumed so s68000exec
		// can subtract them from the return value
		if (cycles < 0) {
			musashi_wait_cycles += (-cycles);
		}
	}
}

//---------------------------------------------------------------------------
//
//	I/O cycle counter (used by Scheduler fast wait)
//
//---------------------------------------------------------------------------
unsigned int s68000getcounter(void)
{
	if (musashi_executing) {
		return (unsigned int)m68k_cycles_remaining();
	}
	return s68000iocycle;
}

void s68000setcounter(unsigned int c)
{
	if (musashi_executing) {
		// Adjust the remaining cycles in Musashi
		int current = m68k_cycles_remaining();
		int diff = (int)c - current;
		if (diff != 0) {
			m68k_modify_timeslice(diff);
		}
	}
	s68000iocycle = c;
}

//---------------------------------------------------------------------------
//
//	s68000buserr - Trigger a bus error exception
//	addr: faulting address
//	param: status word (upper 16 = opcode, lower 16 = FC + R/W flags)
//
//---------------------------------------------------------------------------
void s68000buserr(unsigned int addr, unsigned int param)
{
	// Set up Musashi's internal bus error state
	// m68ki_aerr_address, m68ki_aerr_write_mode, m68ki_aerr_fc
	// are extern variables in m68kcpu.h
	extern unsigned int m68ki_aerr_address;
	extern unsigned int m68ki_aerr_write_mode;
	extern unsigned int m68ki_aerr_fc;

	m68ki_aerr_address = addr;

	// Decode the param: bit4=R/W (1=read), lower 3 bits = FC
	m68ki_aerr_fc = param & 0x07;
	m68ki_aerr_write_mode = (param & 0x10) ? 0 : 0x10; // Musashi: 0x10 = write

	// Trigger bus error in Musashi (uses longjmp internally)
	m68k_pulse_bus_error();
}

//---------------------------------------------------------------------------
//
//	Musashi Callbacks
//
//---------------------------------------------------------------------------

// RESET instruction callback
void musashi_reset_callback(void)
{
	if (s68000context.resethandler) {
		s68000context.resethandler();
	}
}

// Function code callback - tracks user/supervisor mode for memory access
void musashi_fc_callback(unsigned int new_fc)
{
	musashi_current_fc = new_fc;
}
