//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ CPU(MC68000) ]
//
//---------------------------------------------------------------------------

#if !defined(cpu_h)
#define cpu_h

#include "device.h"
#include "musashi_adapter.h"

// s68000getcounter/</minimax:tool_call> etcounter are declared in musashi_adapter.h


//===========================================================================
//
//	CPU
//
//===========================================================================
class CPU : public Device
{
public:
	// Internal data definition
	typedef struct {
		DWORD dreg[8];					// Data registers
		DWORD areg[8];					// Address registers
		DWORD sp;						// Stack pointer (USP or SSP)
		DWORD pc;						// Program counter
		DWORD intr[8];					// Interrupt information
		DWORD sr;						// Status register
		DWORD intreq[8];				// Interrupt request count
		DWORD intack[8];				// Interrupt acknowledge count
		DWORD odd;						// Execution counter
	} cpu_t;

	typedef struct {
		DWORD erraddr;					// Error address
		DWORD errtime;					// Virtual time at error
		DWORD intreq[8];				// Interrupt request count
		DWORD intack[8];				// Interrupt acknowledge count
	} cpusub_t;

public:
	// Basic functions
	CPU(VM *p);
										// Constructor
	BOOL FASTCALL Init();
										// Initialization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
	void FASTCALL ApplyCfg(const Config *config);
										// Apply configuration

public:
	// External API
	void FASTCALL GetCPU(cpu_t *buffer) const;
										// Get CPU registers
	void FASTCALL SetCPU(const cpu_t *buffer);
										// Set CPU registers
	DWORD FASTCALL Exec(int cycle) {
		DWORD result;

		if (::s68000exec(cycle) <= 0x80000000) {
			result = ::s68000context.odometer;
			::s68000context.odometer = 0;
			return result;
		}

		result = ::s68000context.odometer;
		result |= 0x80000000;
		::s68000context.odometer = 0;
		return result;
	}
										// Execution
	BOOL FASTCALL Interrupt(int level, int vector);
										// Interrupt
	void FASTCALL IntAck(int level);
										// Interrupt ACK
	void FASTCALL IntCancel(int level);
										// Cancel interrupt
	DWORD FASTCALL GetCycle() const		{ return ::s68000readOdometer(); }
										// Get cycle count
	DWORD FASTCALL GetPC() const		{ return ::s68000readPC(); }
										// Get program counter
	void FASTCALL ResetInst();
										// RESET instruction
	DWORD FASTCALL GetIOCycle()	const	{ return ::s68000getcounter(); }
										// Get I/O cycle
	void FASTCALL SetIOCycle(DWORD c)	{ ::s68000setcounter(c); }
										// Set I/O cycle
	void FASTCALL Release()				{ ::s68000releaseTimeslice(); }
										// Force CPU execution termination at current instruction
	void FASTCALL BusErr(DWORD addr, BOOL read);
										// Bus error
	void FASTCALL AddrErr(DWORD addr, BOOL read);
										// Address error
	void FASTCALL BusErrLog(DWORD addr, DWORD stat);
										// Bus error logging
	void FASTCALL AddrErrLog(DWORD addr, DWORD stat);
										// Address error logging

private:
	cpusub_t sub;
										// Internal data
	Memory *memory;
										// Memory
	DMAC *dmac;
										// DMAC
	MFP *mfp;
										// MFP
	IOSC *iosc;
										// IOSC
	SCC *scc;
										// SCC
	MIDI *midi;
										// MIDI
	SCSI *scsi;
										// SCSI
	Scheduler *scheduler;
										// Scheduler
};

#endif	// cpu_h
