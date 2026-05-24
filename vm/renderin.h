//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001,2002 PI (ytanaka@ipc-tokai.or.jp)
//	[ Video (Inline functions) ]
//
//---------------------------------------------------------------------------

#if !defined(renderin_h)
#define renderin_h

#include "render.h"

//---------------------------------------------------------------------------
//
//	Palette set
//
//---------------------------------------------------------------------------
inline void FASTCALL Render::SetPalette(int index)
{
	// Perform check at VC access point
	render.palmod[index] = TRUE;
	render.palette = TRUE;
}

//---------------------------------------------------------------------------
//
//	Text VRAM change
//
//---------------------------------------------------------------------------
inline void FASTCALL Render::TextMem(DWORD addr)
{
	// Perform check at Text VRAM access point
	addr &= 0x1ffff;
	addr >>= 2;
	render.textflag[addr] = TRUE;
	addr >>= 5;
	render.textmod[addr] = TRUE;
}

//---------------------------------------------------------------------------
//
//	Graphic VRAM change
//
//---------------------------------------------------------------------------
inline void FASTCALL Render::GrpMem(DWORD addr, DWORD block)
{
	// Perform check at Graphic VRAM access point
	ASSERT(addr <= 0x7ffff);
	ASSERT(block <= 3);

	// 16dot flag (16dot unit, (512/16) x 512 x 4 = 0x10000)
	addr >>= 5;
	block <<= 14;
	render.grpflag[addr | block] = TRUE;
	// Line flag (line unit, 512x4 = 2048)
	addr >>= 5;
	block >>= 5;
	render.grpmod[addr | block] = TRUE;
}

//---------------------------------------------------------------------------
//
//	Graphic VRAM change (all)
//
//---------------------------------------------------------------------------
inline void FASTCALL Render::GrpAll(DWORD line, DWORD block)
{
	ASSERT(line <= 0x1ff);
	ASSERT(block <= 3);

	render.grppal[(block << 9) | line] = TRUE;
}

#endif	// renderin_h
