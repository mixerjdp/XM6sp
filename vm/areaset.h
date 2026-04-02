//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2003 ÇoÇhÅD(ytanaka@ipc-tokai.or.jp)
// [ Area Set ]
//
//---------------------------------------------------------------------------

#if !defined(areaset_h)
#define areaset_h

#include "device.h"

//===========================================================================
//
// Area Set
//
//===========================================================================
class AreaSet : public MemDevice
{
public:
// Basic functions
	AreaSet(VM *p);
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
// Apply settings

// Memory device
	DWORD FASTCALL ReadByte(DWORD addr);
// Byte read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
// Byte write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
// Read only

// Public API
	DWORD FASTCALL GetArea() const;
// Get configuration value

private:
	Memory *memory;
// Memory
	DWORD area;
// Area set register
};

#endif	// areaset_h
