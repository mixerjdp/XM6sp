//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ÇoÇhÅD(ytanaka@ipc-tokai.or.jp)
//	[ Graphic VRAM ]
//
//---------------------------------------------------------------------------

#if !defined(gvram_h)
#define gvram_h

#include "device.h"
#include "crtc.h"

//===========================================================================
//
//	Graphic VRAMHandler
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
										// Read only

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
//	Graphic VRAMHandler(1024)
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
										// Read only
};

//===========================================================================
//
//	Graphic VRAMHandler(16 colors)
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
										// Read only
};

//===========================================================================
//
//	Graphic VRAMHandler(256 colors)
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
										// Read only
};

//===========================================================================
//
//	Graphic VRAMHandler(Undefined)
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
										// Read only
};

//===========================================================================
//
//	Graphic VRAMHandler(65536 colors)
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
										// Read only
};

//===========================================================================
//
//	Graphic VRAM
//
//===========================================================================
class GVRAM : public MemDevice
{
public:
	// Structure
	typedef struct {
		BOOL mem;						// 512KB single bank flag
		DWORD siz;						// 1024x1024 flag
		DWORD col;						// 16, 256, Undefined, 65536
		int type;						// Handler type (0-4)
		DWORD mask[4];					// Plane clear mask
		BOOL plane[4];					// Plane clear flag
	} gvram_t;

public:
	// Basic procedures
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
										// Apply config
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

	// Public API
	void FASTCALL SetType(DWORD type);
										// GVRAM type set
	void FASTCALL FastSet(DWORD mask);
										// Plane clear flag set
	void FASTCALL FastClr(const CRTC::crtc_t *p);
										// Plane clear
	const BYTE* FASTCALL GetGVRAM() const;
										// Get GVRAM

private:
	void FASTCALL FastClr768(const CRTC::crtc_t *p);
										// Plane clear 1024x1024 512/768
	void FASTCALL FastClr256(const CRTC::crtc_t *p);
										// Plane clear 1024x1024 256
	void FASTCALL FastClr512(const CRTC::crtc_t *p);
										// Plane clear 512x512
	Render *render;
										// Renderer
	BYTE *gvram;
										// Graphic VRAM
	GVRAMHandler *handler;
										// Internal Handler(default)
	GVRAM1024 *hand1024;
										// Internal Handler(1024)
	GVRAM16 *hand16;
										// Internal Handler(16 colors)
	GVRAM256 *hand256;
										// Internal Handler(256 colors)
	GVRAMNDef *handNDef;
										// Internal Handler(Undefined)
	GVRAM64K *hand64K;
										// Internal Handler(64K colors)
	gvram_t gvdata;
										// State
	DWORD gvcount;
										// GVRAM access count (version 2.04 or later)
};

#endif	// gvram_h
