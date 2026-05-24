//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 Takashi Tanaka (ytanaka@ipc-tokai.or.jp)
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
	// Sprite data structure
	typedef struct {
		BOOL connect;					// Access enable flag
		BOOL disp;						// Display (wait) flag
		BYTE *mem;						// Sprite memory area
		BYTE *pcg;						// Sprite PCG area

		BOOL bg_on[2];					// BG display ON
		DWORD bg_area[2];				// BG data area
		DWORD bg_scrlx[2];				// BG scroll X
		DWORD bg_scrly[2];				// BG scroll Y
		BOOL bg_size;					// BG size

		DWORD h_total;					// Horizontal total
		DWORD h_disp;					// Horizontal display
		DWORD v_disp;					// Vertical display
		BOOL lowres;					// 15kHz mode
		DWORD h_res;					// Horizontal resolution
		DWORD v_res;					// Vertical resolution
	} sprite_t;

public:
	// Constructor
	Sprite(VM *p);
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
	void FASTCALL ApplyCfg(const Config* config);


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
										// Connect status get
	BOOL FASTCALL IsDisplay() const		{ return spr.disp; }
										// Display status get
	void FASTCALL GetSprite(sprite_t *buffer) const;
										// Sprite data get
	const BYTE* FASTCALL GetMem() const;
										// BG area get
	const BYTE* FASTCALL GetPCG() const;
										// PCG area get

private:
	void FASTCALL Control(DWORD addr, DWORD ctrl);
										// Control
	sprite_t spr;
										// Sprite data
	Render *render;
										// Render
	BYTE *sprite;
										// Sprite RAM (64KB)
};

#endif	// sprite_h