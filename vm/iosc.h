//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 PI(ytanaka@ipc-tokai.or.jp)
//	[ I/O Controller (IOSC-2) ]
//
//---------------------------------------------------------------------------

#if !defined(iosc_h)
#define iosc_h

#include "device.h"

//===========================================================================
//
//	I/O Controller
//
//===========================================================================
class IOSC : public MemDevice
{
public:
	// Internal data structure
	typedef struct {
		BOOL prt_int;					// Printer interrupt request
		BOOL prt_en;					// Printer interrupt enable
		BOOL fdd_int;					// FDD interrupt request
		BOOL fdd_en;					// FDD interrupt enable
		BOOL fdc_int;					// FDC interrupt request
		BOOL fdc_en;					// FDC interrupt enable
		BOOL hdc_int;					// HDD interrupt request
		BOOL hdc_en;					// HDD interrupt enable
		DWORD vbase;					// Vector base
		int vector;						// Current interrupt vector
	} iosc_t;

public:
	// Basic constructor
	IOSC(VM *p);
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
										// Apply config

	// Sub device
	DWORD FASTCALL ReadByte(DWORD addr);
										// Read byte
	DWORD FASTCALL ReadWord(DWORD addr);
										// Read word
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Write byte
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Write word
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read only

	// External API
	void FASTCALL GetIOSC(iosc_t *buffer) const;
										// Get IOSC
	DWORD FASTCALL GetVector() const	{ return iosc.vbase; }
										// Get vector base
	void FASTCALL IntAck();
										// Interrupt acknowledge
	void FASTCALL IntFDC(BOOL flag);
										// FDC interrupt
	void FASTCALL IntFDD(BOOL flag);
										// FDD interrupt
	void FASTCALL IntHDC(BOOL flag);
										// HDC interrupt
	void FASTCALL IntPRT(BOOL flag);
										// Printer interrupt

private:
	void FASTCALL IntChk();
										// Interrupt check
	iosc_t iosc;
										// Internal data
	Printer *printer;
										// Printer
};

#endif	// iosc_h
