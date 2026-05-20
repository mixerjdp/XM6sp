//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 PI (ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ Sprite (CYNTHIA) ]
//
//---------------------------------------------------------------------------

#if !defined(sprite_h)
#define sprite_h

#include "device.h"

//===========================================================================
//
//	Sprite
//
//===========================================================================
class Sprite : public MemDevice
{
public:
	// Work data definition
	typedef struct {
		BOOL connect;					// Access enable flag
		BOOL disp;						// Display (wait) flag
		BYTE *mem;						// Sprite memory
		BYTE *pcg;						// Sprite PCG area

		BOOL bg_on[2];					// BG display ON
		DWORD bg_area[2];				// BG data area
		DWORD bg_scrlx[2];				// BG scroll X
		DWORD bg_scrly[2];				// BG scroll Y
		BOOL bg_size;					// BG size

		DWORD h_total;					// Horizontal total count
		DWORD h_disp;					// Horizontal display count
		DWORD v_disp;					// Vertical display count
		BOOL lowres;					// 15kHz mode
		DWORD h_res;					// Horizontal resolution
		DWORD v_res;					// Vertical resolution
	} sprite_t;

public:
	// Basic functions
	Sprite(VM *p);
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
	void FASTCALL Connect(BOOL con)		{ spr.connect = con; }
										// Connect
	BOOL FASTCALL IsConnect() const		{ return spr.connect; }
										// Get connection status
	BOOL FASTCALL IsDisplay() const		{ return spr.disp; }
										// Get display status
	void FASTCALL GetSprite(sprite_t *buffer) const;
										// Get sprite data
	const BYTE* FASTCALL GetMem() const;
										// Get work area
	const BYTE* FASTCALL GetPCG() const;
										// Get PCG area
	void FASTCALL HSync();
										// H-Sync notification
private:
	void FASTCALL Control(DWORD addr, DWORD ctrl);
										// Control
	void FASTCALL NotifyRender();
	void FASTCALL NotifyPx68kBGWrite(DWORD addr, WORD data);
										// Notify renderer
	sprite_t spr;
										// Work data
	Render *render;
										// Renderer
	BYTE *sprite;
										// Sprite RAM (64KB)
	DWORD sphsync[128];
										// Sprite HSYNC schedule
	DWORD bghsync;
										// BG HSYNC schedule
};

#endif	// sprite_h
