//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ System Port ]
//
//---------------------------------------------------------------------------

#if !defined(sysport_h)
#define sysport_h

#include "device.h"

//===========================================================================
//
//	System Port
//
//===========================================================================
class SysPort : public MemDevice
{
public:
	// System data type definition
	typedef struct {
		DWORD contrast;					// Contrast
		DWORD scope_3d;					// 3D scope mode
		DWORD image_unit;				// Image unit number
		DWORD power_count;				// Power switch counter
		DWORD ver_count;				// Version management counter
	} sysport_t;

public:
	// Basic functions
	SysPort(VM *p);
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
#if !defined(NDEBUG)
	void FASTCALL AssertDiag() const;
										// Diagnostic
#endif	// NDEBUG

	// I/O device
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

private:
	DWORD FASTCALL GetVR();
										// Version register readout
	sysport_t sysport;
										// Work memory
	Memory *memory;
										// Memory
	SRAM *sram;
										// Static RAM
	Keyboard *keyboard;
										// Keyboard
	CRTC *crtc;
										// CRTC
	Render *render;
										// Render
};

#endif	// sysport_h
