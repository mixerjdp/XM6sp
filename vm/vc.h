//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ Video Controller (CATHY & VIPS) ]
//
//---------------------------------------------------------------------------

#if !defined(vc_h)
#define vc_h

#include "device.h"

//===========================================================================
//
//	Video Controller
//
//===========================================================================
class VC : public MemDevice
{
public:
	// Work data definition
	typedef struct {
		DWORD vr1h;						// VR1(H) backup
		DWORD vr1l;						// VR1(L) backup
		DWORD vr2h;						// VR2(H) backup
		DWORD vr2l;						// VR2(L) backup
		BOOL siz;						// Screen size
		DWORD col;						// Color mode
		DWORD sp;						// Sprite priority
		DWORD tx;						// Text priority
		DWORD gr;						// Graphic priority (1024)
		DWORD gp[4];					// Graphic priority (512)
		BOOL ys;						// Ys emphasis
		BOOL ah;						// Text palette offset
		BOOL vht;						// Graphic video enable
		BOOL exon;						// External priority enable
		BOOL hp;						// Horizontal priority
		BOOL bp;						// Bottom bit clear flag
		BOOL gg;						// Graphic GG enable
		BOOL gt;						// Graphic text enable
		BOOL bcon;						// Shape enable
		BOOL son;						// Sprite ON
		BOOL ton;						// Text ON
		BOOL gon;						// Graphic ON (screen 1024)
		BOOL gs[4];						// Graphic ON (screen 512)
	} vc_t;

public:
	// Basic functions
	VC(VM *p);
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
	void FASTCALL HSync();
										// H-Sync notification
	void FASTCALL GetVC(vc_t *buffer);
										// Get work data
	const BYTE* FASTCALL GetPalette() const	{ return palette; }
										// Get palette RAM
	const vc_t* FASTCALL GetWorkAddr() const{ return &vc; }
										// Get work address
private:
	// Register access
	void FASTCALL SetVR0L(DWORD data);
										// Register 0(L) set
	DWORD FASTCALL GetVR0() const;
										// Get register 0
	void FASTCALL SetVR1H(DWORD data);
										// Register 1(H) set
	void FASTCALL SetVR1L(DWORD data);
										// Register 1(L) set
	DWORD FASTCALL GetVR1() const;
										// Get register 1
	void FASTCALL SetVR2H(DWORD data);
										// Register 2(H) set
	void FASTCALL SetVR2L(DWORD data);
										// Register 2(L) set
	DWORD FASTCALL GetVR2() const;
										// Get register 2

	// Data
	Sprite *sprite;
										// Sprite controller
	Render *render;
										// Renderer
	vc_t vc;
										// Work data
	BYTE palette[0x400];
										// Palette RAM
	BOOL vr1h;
										// Register 1(H) change flag
	BOOL vr2h;
										// Register 2(H) change flag
	int palette_wait;
										// Palette wait
};

#endif	// vc_h
