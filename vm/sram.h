//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ SRAM ]
//
//---------------------------------------------------------------------------

#if !defined(sram_h)
#define sram_h

#include "device.h"
#include "filepath.h"

//===========================================================================
//
//	SRAM
//
//===========================================================================
class SRAM : public MemDevice
{
public:
	// Constructor
	SRAM(VM *p);
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

#if !defined(NDEBUG)
	void FASTCALL AssertDiag() const;
										// Assert
#endif	// NDEBUG

	// Memory device
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
	const BYTE* FASTCALL GetSRAM() const;
										// Get SRAM area
	int FASTCALL GetSize() const;
										// Get SRAM size
	void FASTCALL WriteEnable(BOOL enable);
										// Write enable
	void FASTCALL SetMemSw(DWORD offset, DWORD data);
										// Set memory switch
	DWORD FASTCALL GetMemSw(DWORD offset) const;
										// Get memory switch
	void FASTCALL UpdateBoot();
										// Boot counter update

private:
	Filepath sram_path;
										// SRAM file path
	int sram_size;
										// SRAM size (16,32,48,64)
	BYTE sram[0x10000];
										// SRAM (64KB)
	BOOL write_en;
										// Write enable flag
	BOOL mem_sync;
										// Common RAM size sync flag
	BOOL changed;
										// Changed flag
};

#endif	// sram_h