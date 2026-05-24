//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
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

	// TVRAM structure copy
	DWORD multi;
										// Multi access (bit0-bit3)
	DWORD mask;
										// Access mask (1 does not change)
	DWORD rev;
										// Access mask reverse
	DWORD maskh;
										// Access mask high byte
	DWORD revh;
										// Access mask high reverse

protected:
	Render *render;
										// Render
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
//	Text VRAM handler (mask+multi)
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
	// TVRAM data structure
	typedef struct {
		DWORD multi;					// Multi access (bit0-bit3)
		DWORD mask;						// Access mask (1 does not change)
		DWORD rev;						// Access mask reverse
		DWORD maskh;					// Access mask high byte
		DWORD revh;						// Access mask high reverse
		DWORD src;						// Raster copy source
		DWORD dst;						// Raster copy dest
		DWORD plane;					// Raster copy plane
	} tvram_t;

public:
	// Constructor
	TVRAM(VM *p);
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

	// External API
	const BYTE* FASTCALL GetTVRAM() const;
										// Get TVRAM
	void FASTCALL SetMulti(DWORD data);
										// Multi access setting
	void FASTCALL SetMask(DWORD data);
										// Access mask setting
	void FASTCALL SetCopyRaster(DWORD src, DWORD dst, DWORD plane);
										// Raster copy setting
	void FASTCALL RasterCopy();
										// Raster copy execute

private:
	void FASTCALL SelectHandler();
										// Handler select
	TVRAMNormal *normal;
										// Handler (normal)
	TVRAMMask *mask;
										// Handler (mask)
	TVRAMMulti *multi;
										// Handler (multi)
	TVRAMBoth *both;
										// Handler (both)
	TVRAMHandler *handler;
										// Handler (current select)
	Render *render;
										// Render
	BYTE *tvram;
										// Text VRAM (512KB)
	tvram_t tvdata;
										// VRAM data
	DWORD tvcount;
										// VRAM access count (version 2.04+)
};

#endif	// tvram_h