//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 P.I. (ytanaka@ipc-tokai.or.jp)
//	[ Graphic VRAM ]
//
//---------------------------------------------------------------------------

#if !defined(gvram_h)
#define gvram_h

#include "device.h"
#include "crtc.h"

//===========================================================================
//
//	Graphic VRAM handler
//
//===========================================================================
class GVRAMHandler
{
public:
	GVRAMHandler(Render *rend, BYTE *mem, CPU *p);
										// Constructor
	virtual DWORD FASTCALL ReadByte(DWORD addr) = 0;
										// Byte read
	virtual DWORD FASTCALL ReadWord(DWORD addr) = 0;
										// Word read
	virtual void FASTCALL WriteByte(DWORD addr, DWORD data) = 0;
										// Byte write
	virtual void FASTCALL WriteWord(DWORD addr, DWORD data) = 0;
										// Word write
	virtual DWORD FASTCALL ReadOnly(DWORD addr) const = 0;
										// Read-only

protected:
	Render *render;
										// Renderer
	BYTE *gvram;
										// Graphic VRAM
	CPU *cpu;
										// CPU
};

//===========================================================================
//
//	Graphic VRAM handler (1024)
//
//===========================================================================
class GVRAM1024 : public GVRAMHandler
{
public:
	GVRAM1024(Render *render, BYTE *gvram, CPU *p);
										// Constructor
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										// Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read-only
};

//===========================================================================
//
//	Graphic VRAM handler (16 colors)
//
//===========================================================================
class GVRAM16 : public GVRAMHandler
{
public:
	GVRAM16(Render *render, BYTE *gvram, CPU *p);
										// Constructor
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										// Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read-only
};

//===========================================================================
//
//	Graphic VRAM handler (256 colors)
//
//===========================================================================
class GVRAM256 : public GVRAMHandler
{
public:
	GVRAM256(Render *render, BYTE *gvram, CPU *p);
										// Constructor
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										// Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read-only
};

//===========================================================================
//
//	Graphic VRAM handler (invalid)
//
//===========================================================================
class GVRAMNDef : public GVRAMHandler
{
public:
	GVRAMNDef(Render *render, BYTE *gvram, CPU *p);
										// Constructor
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										// Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read-only
};

//===========================================================================
//
//	Graphic VRAM handler (65536 colors)
//
//===========================================================================
class GVRAM64K : public GVRAMHandler
{
public:
	GVRAM64K(Render *render, BYTE *gvram, CPU *p);
										// Constructor
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										// Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read-only
};

//===========================================================================
//
//	Graphic VRAM
//
//===========================================================================
class GVRAM : public MemDevice
{
public:
	// Internal state definition
	typedef struct {
		BOOL mem;						// 512 KB simple-memory flag
		DWORD siz;						// 1024x1024 flag
		DWORD col;						// 16, 256, undefined, 65536
		int type;						// Handler type (0 to 4)
		DWORD mask[4];					// Fast-clear mask
		BOOL plane[4];					// Fast-clear plane
	} gvram_t;

public:
	// Basic functions
	GVRAM(VM *p);
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
										// Apply settings
#if !defined(NDEBUG)
	void FASTCALL AssertDiag() const;
										// Diagnostics
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
										// Read-only

	// External API
	void FASTCALL SetType(DWORD type);
										// Set the GVRAM type
	void FASTCALL FastSet(DWORD mask);
										// Set fast clear
	void FASTCALL FastClr(const CRTC::crtc_t *p);
										// Fast clear
	const BYTE* FASTCALL GetGVRAM() const;
										// Get GVRAM

private:
	void FASTCALL FastClr768(const CRTC::crtc_t *p);
										// Fast clear 1024x1024 512/768
	void FASTCALL FastClr256(const CRTC::crtc_t *p);
										// Fast clear 1024x1024 256
	void FASTCALL FastClr512(const CRTC::crtc_t *p);
										// Fast clear 512x512
	Render *render;
										// Renderer
	BYTE *gvram;
										// Graphic VRAM
	GVRAMHandler *handler;
										// Current memory handler
	GVRAM1024 *hand1024;
										// 1024 memory handler
	GVRAM16 *hand16;
										// 16-color memory handler
	GVRAM256 *hand256;
										// 256-color memory handler
	GVRAMNDef *handNDef;
										// Invalid memory handler
	GVRAM64K *hand64K;
										// 64K-color memory handler
	gvram_t gvdata;
										// Internal state
	DWORD gvcount;
										// GVRAM access count (version 2.04 or later)
};

#endif	// gvram_h
