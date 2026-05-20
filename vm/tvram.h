//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 P.I. (ytanaka@ipc-tokai.or.jp)
//	[ Text VRAM ]
//
//---------------------------------------------------------------------------

#if !defined(tvram_h)
#define tvram_h

#include "device.h"

//===========================================================================
//
//	Text VRAM handler
//
//===========================================================================
class TVRAMHandler
{
public:
	TVRAMHandler(Render *rend, BYTE *mem);
										// Constructor
	virtual void FASTCALL WriteByte(DWORD addr, DWORD data) = 0;
										// Byte write
	virtual void FASTCALL WriteWord(DWORD addr, DWORD data) = 0;
										// Word write

	// Copy the TVRAM state
	DWORD multi;
										// Simultaneous access (bits 0-3)
	DWORD mask;
										// Access mask (1 means unchanged)
	DWORD rev;
										// Inverted access mask
	DWORD maskh;
										// Upper access-mask byte
	DWORD revh;
										// Upper access-mask inversion

protected:
	void FASTCALL NotifyPx68kTVRAMWrite(DWORD internal_addr, BYTE data);
	void FASTCALL NotifyPx68kTVRAMWord(DWORD addr, WORD data);
	Render *render;
										// Renderer
	BYTE *tvram;
										// Text VRAM
};

//===========================================================================
//
//	Text VRAM handler (normal)
//
//===========================================================================
class TVRAMNormal : public TVRAMHandler
{
public:
	TVRAMNormal(Render *rend, BYTE *mem);
										// Constructor
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
};

//===========================================================================
//
//	Text VRAM handler (mask)
//
//===========================================================================
class TVRAMMask : public TVRAMHandler
{
public:
	TVRAMMask(Render *rend, BYTE *mem);
										// Constructor
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
};

//===========================================================================
//
//	Text VRAM handler (multi)
//
//===========================================================================
class TVRAMMulti : public TVRAMHandler
{
public:
	TVRAMMulti(Render *rend, BYTE *mem);
										// Constructor
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
};

//===========================================================================
//
//	Text VRAM handler (mask + multi)
//
//===========================================================================
class TVRAMBoth : public TVRAMHandler
{
public:
	TVRAMBoth(Render *rend, BYTE *mem);
										// Constructor
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
};

//===========================================================================
//
//	Text VRAM
//
//===========================================================================
class TVRAM : public MemDevice
{
public:
	// Internal data definition
	typedef struct {
		DWORD multi;					// Simultaneous access (bits 0-3)
		DWORD mask;						// Access mask (1 means unchanged)
		DWORD rev;						// Inverted access mask
		DWORD maskh;					// Upper access-mask byte
		DWORD revh;						// Upper access-mask inversion
		DWORD src;						// Raster copy source raster
		DWORD dst;						// Raster copy destination raster
		DWORD plane;					// Raster copy target plane
	} tvram_t;

public:
	// Basic functions
	TVRAM(VM *p);
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
	const BYTE* FASTCALL GetTVRAM() const;
										// Get TVRAM
	void FASTCALL SetMulti(DWORD data);
										// Set simultaneous writes
	void FASTCALL SetMask(DWORD data);
										// Set the access mask
	void FASTCALL SetCopyRaster(DWORD src, DWORD dst, DWORD plane);
										// Select the raster to copy
	void FASTCALL RasterCopy();
										// Raster copy operation

private:
	void FASTCALL SelectHandler();
										// Select the handler
	TVRAMNormal *normal;
										// Handler (normal)
	TVRAMMask *mask;
										// Handler (mask)
	TVRAMMulti *multi;
										// Handler (multi)
	TVRAMBoth *both;
										// Handler (both)
	TVRAMHandler *handler;
										// Currently selected handler
	Render *render;
										// Renderer
	BYTE *tvram;
										// Text VRAM (512 KB)
	tvram_t tvdata;
										// Internal data
	DWORD tvcount;
										// TVRAM access count (version 2.04 or later)
};

#endif	// tvram_h
