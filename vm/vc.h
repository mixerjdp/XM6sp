//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
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
	// Video data structure
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
		BOOL ys;						// Ys output
		BOOL ah;						// Text palette disable
		BOOL vht;						// External sync
		BOOL exon;						// Sprite priority ext
		BOOL hp;						// Priority
		BOOL bp;						// Sprite priority bit flag
		BOOL gg;						// Graphic disable
		BOOL gt;						// Text disable
		BOOL bcon;						// VBLANK display
		BOOL son;						// Sprite ON
		BOOL ton;						// Text ON
		BOOL gon;						// Graphic ON (layer 1024)
		BOOL gs[4];					// Graphic ON (layer 512)
	} vc_t;

public:
	// Constructor
	VC(VM *p);
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
	void FASTCALL GetVC(vc_t *buffer);
										// Get video data
	const BYTE* FASTCALL GetPalette() const	{ return palette; }
										// Get palette RAM
	const vc_t* FASTCALL GetWorkAddr() const{ return &vc; }
										// Get work address

private:
	// Register access
	void FASTCALL SetVR0L(DWORD data);
										// Register 0 (L) set
	DWORD FASTCALL GetVR0() const;
										// Register 0 get
	void FASTCALL SetVR1H(DWORD data);
										// Register 1 (H) set
	void FASTCALL SetVR1L(DWORD data);
										// Register 1 (L) set
	DWORD FASTCALL GetVR1() const;
										// Register 1 get
	void FASTCALL SetVR2H(DWORD data);
										// Register 2 (H) set
	void FASTCALL SetVR2L(DWORD data);
										// Register 2 (L) set
	DWORD FASTCALL GetVR2() const;
										// Register 2 get

	// Data
	Render *render;
										// Render
	vc_t vc;
										// Video data
	BYTE palette[0x400];
										// Palette RAM
};

#endif	// vc_h