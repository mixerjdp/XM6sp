//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 Takashi Tanaka (ytanaka@ipc-tokai.or.jp)
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
	// System data structure
	typedef struct {
		DWORD contrast;					// Contrast
		DWORD scope_3d;					// 3D shift
		DWORD image_unit;				// Image unit select
		DWORD power_count;				// Power save counter
		DWORD ver_count;				// Version control counter
	} sysport_t;

public:
	// Constructor
	SysPort(VM *p);
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

private:
	DWORD FASTCALL GetVR();
										// Version register read
	sysport_t sysport;
										// Structure
	Memory *memory;
										// Memory
	SRAM *sram;
										// SRAM
	Keyboard *keyboard;
										// Keyboard
	CRTC *crtc;
										// CRTC
	Render *render;
										// Render
};

#endif	// sysport_h