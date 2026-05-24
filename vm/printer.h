//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 PI(ytanaka@ipc-tokai.or.jp)
//	[ Printer ]
//
//---------------------------------------------------------------------------

#if !defined(printer_h)
#define printer_h

#include "device.h"

//===========================================================================
//
//	Printer
//
//===========================================================================
class Printer : public MemDevice
{
public:
	// Constants
	enum {
		BufMax = 0x1000				// Buffer size (2^n)
	};

	// Internal data structure
	typedef struct {
		BOOL connect;					// Connect
		BOOL strobe;					// Strobe
		BOOL ready;						// Ready
		BYTE data;						// Write data
		BYTE buf[BufMax];				// Buffer data
		DWORD read;						// Buffer read position
		DWORD write;					// Buffer write position
		DWORD num;						// Buffer count
	} printer_t;

public:
	// Basic functions
	Printer(VM *p);
										// Constructor
	BOOL FASTCALL Init();
										// Initialize
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

	// Virtual device
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										// Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read only

	// External API
	BOOL FASTCALL IsReady() const		{ return printer.ready; }
										// Get ready
	void FASTCALL HSync();
										// H-Sync notification
	void FASTCALL GetPrinter(printer_t *buffer) const;
										// Get printer data
	void FASTCALL Connect(BOOL flag);
										// Printer connect
	BOOL FASTCALL GetData(BYTE *ptr);
										// Get first data

private:
	IOSC *iosc;
										// IOSC
	Sync *sync;
										// Sync object
	printer_t printer;
										// Printer data
};

#endif	// printer_h
