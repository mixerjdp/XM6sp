//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 P.I. (ytanaka@ipc-tokai.or.jp)
//	[ Graphic VRAM ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "fileio.h"
#include "render.h"
#include "renderin.h"
#include "gvram.h"

//===========================================================================
//
//	Graphic VRAM handler
//
//===========================================================================
//#define GVRAM_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
GVRAMHandler::GVRAMHandler(Render *rend, BYTE *mem, CPU *p)
{
	ASSERT(rend);
	ASSERT(mem);
	ASSERT(p);

	render = rend;
	gvram = mem;
	cpu = p;
}

//===========================================================================
//
//	Graphic VRAM handler (1024x1024)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
GVRAM1024::GVRAM1024(Render *rend, BYTE *mem, CPU *cpu) : GVRAMHandler(rend, mem, cpu)
{
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM1024::ReadByte(DWORD addr)
{
	DWORD offset;

	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Reading an even byte always returns 0
	if ((addr & 1) == 0) {
		return 0x00;
	}

	// Common terms
	offset = addr & 0x3ff;

	// Split into upper/lower and left/right halves
	if (addr & 0x100000) {
		if (addr & 0x400) {
			// Page 3 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] >> 4);
		}
		else {
			// Page 2 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] & 0x0f);
		}
	}
	else {
		if (addr & 0x400) {
			// Page 1 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] >> 4);
		}
		else {
			// Page 0 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] & 0x0f);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM1024::ReadWord(DWORD addr)
{
	DWORD offset;

	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT((addr & 1) == 0);

	// Common terms
	offset = addr & 0x3ff;

	// Split into upper/lower and left/right halves
	if (addr & 0x100000) {
		if (addr & 0x400) {
			// Page 3 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] >> 4);
		}
		else {
			// Page 2 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] & 0x0f);
		}
	}
	else {
		if (addr & 0x400) {
			// Page 1 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] >> 4);
		}
		else {
			// Page 0 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] & 0x0f);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM1024::WriteByte(DWORD addr, DWORD data)
{
	DWORD offset;
	DWORD mem;

	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT(data < 0x100);

	// Even bytes cannot be written
	if ((addr & 1) == 0) {
		return;
	}

	// Common terms
	offset = addr & 0x3ff;

	// Split into upper/lower and left/right halves
	if (addr & 0x100000) {
		if (addr & 0x400) {
			// Page 3 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// To the upper nibble
			mem = (gvram[addr] & 0x0f);
			mem |= (data << 4);

			// Write
			if (gvram[addr] != mem) {
				gvram[addr] = (BYTE)mem;
				render->GrpMem(addr, 3);
			}
		}
		else {
			// Page 2 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// To the lower nibble
			mem = (gvram[addr] & 0xf0);
			mem |= (data & 0x0f);

			// Write
			if (gvram[addr] != mem) {
				gvram[addr] = (BYTE)mem;
				render->GrpMem(addr, 2);
			}
		}
	}
	else {
		if (addr & 0x400) {
			// Page 1 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// To the upper nibble
			mem = (gvram[addr ^ 1] & 0x0f);
			mem |= (data << 4);

			// Write
			if (gvram[addr ^ 1] != mem) {
				gvram[addr ^ 1] = (BYTE)mem;
				render->GrpMem(addr ^ 1, 1);
			}
		}
		else {
			// Page 0 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// To the lower nibble
			mem = (gvram[addr ^ 1] & 0xf0);
			mem |= (data & 0x0f);

			// Write
			if (gvram[addr ^ 1] != mem) {
				gvram[addr ^ 1] = (BYTE)mem;
				render->GrpMem(addr ^ 1, 0);
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM1024::WriteWord(DWORD addr, DWORD data)
{
	DWORD offset;
	DWORD mem;

	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT(data < 0x10000);

	// Common terms
	offset = addr & 0x3ff;

	// Split into upper/lower and left/right halves
	if (addr & 0x100000) {
		if (addr & 0x400) {
			// Page 3 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// To the upper nibble
			mem = (gvram[addr ^ 1] & 0x0f);
			data &= 0x0f;
			mem |= (data << 4);

			// Write
			if (gvram[addr ^ 1] != mem) {
				gvram[addr ^ 1] = (BYTE)mem;
				render->GrpMem(addr ^ 1, 3);
			}
		}
		else {
			// Page 2 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// To the lower nibble
			mem = (gvram[addr ^ 1] & 0xf0);
			mem |= (data & 0x0f);

			// Write
			if (gvram[addr ^ 1] != mem) {
				gvram[addr ^ 1] = (BYTE)mem;
				render->GrpMem(addr ^ 1, 2);
			}
		}
	}
	else {
		if (addr & 0x400) {
			// Page 1 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// To the upper nibble
			mem = (gvram[addr] & 0x0f);
			data &= 0x0f;
			mem |= (data << 4);

			// Write
			if (gvram[addr] != mem) {
				gvram[addr] = (BYTE)mem;
				render->GrpMem(addr, 1);
			}
		}
		else {
			// Page 0 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// To the lower nibble
			mem = (gvram[addr] & 0xf0);
			mem |= (data & 0x0f);

			// Write
			if (gvram[addr] != mem) {
				gvram[addr] = (BYTE)mem;
				render->GrpMem(addr, 0);
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	Read-only
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM1024::ReadOnly(DWORD addr) const
{
	DWORD offset;

	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Reading an even byte always returns 0
	if ((addr & 1) == 0) {
		return 0x00;
	}

	// Common terms
	offset = addr & 0x3ff;

	// Split into upper/lower and left/right halves
	if (addr & 0x100000) {
		if (addr & 0x400) {
			// Page 3 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] >> 4);
		}
		else {
			// Page 2 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] & 0x0f);
		}
	}
	else {
		if (addr & 0x400) {
			// Page 1 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] >> 4);
		}
		else {
			// Page 0 area
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] & 0x0f);
		}
	}
}

//===========================================================================
//
//	Graphic VRAM handler (16 colors)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
GVRAM16::GVRAM16(Render *rend, BYTE *mem, CPU *cpu) : GVRAMHandler(rend, mem, cpu)
{
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM16::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Odd addresses only
	if (addr & 1) {
		if (addr < 0x80000) {
			// Page 0: lower word byte bits b0-b3
			return (gvram[addr ^ 1] & 0x0f);
		}

		if (addr < 0x100000) {
			// Page 1: lower word byte bits b4-b7
			addr &= 0x7ffff;
			return (gvram[addr ^ 1] >> 4);
		}

		if (addr < 0x180000) {
			// Page 2: upper word byte bits b0-b3
			addr &= 0x7ffff;
			return (gvram[addr] & 0x0f);
		}

		// Page 3: upper word byte bits b4-b7
		addr &= 0x7ffff;
		return (gvram[addr] >> 4);
	}

	// Even addresses are always 0
	return 0;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM16::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT((addr & 1) == 0);

	if (addr < 0x80000) {
		// Page 0: lower word byte bits b0-b3
		return (gvram[addr] & 0x0f);
	}

	if (addr < 0x100000) {
		// Page 1: lower word byte bits b4-b7
		addr &= 0x7ffff;
		return (gvram[addr] >> 4);
	}

	if (addr < 0x180000) {
		// Page 2: upper word byte bits b0-b3
		addr &= 0x7ffff;
		return (gvram[addr ^ 1] & 0x0f);
	}

	// Page 3: upper word byte bits b4-b7
	addr &= 0x7ffff;
	return (gvram[addr ^ 1] >> 4);
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM16::WriteByte(DWORD addr, DWORD data)
{
	DWORD mem;

	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT(data < 0x100);

	// Odd addresses only
	if (addr & 1) {
		if (addr < 0x80000) {
			// Page 0: lower word byte bits b0-b3
			mem = (gvram[addr ^ 1] & 0xf0);
			mem |= (data & 0x0f);

			// Write
			if (gvram[addr ^ 1] != mem) {
				gvram[addr ^ 1] = (BYTE)mem;
				render->GrpMem(addr ^ 1, 0);
			}
			return;
		}

		if (addr < 0x100000) {
			// Page 1: lower word byte bits b4-b7
			addr &= 0x7ffff;
			mem = (gvram[addr ^ 1] & 0x0f);
			mem |= (data << 4);

			// Write
			if (gvram[addr ^ 1] != mem) {
				gvram[addr ^ 1] = (BYTE)mem;
				render->GrpMem(addr ^ 1, 1);
			}
			return;
		}

		if (addr < 0x180000) {
			// Page 2: upper word byte bits b0-b3
			addr &= 0x7ffff;
			mem = (gvram[addr] & 0xf0);
			mem |= (data & 0x0f);

			// Write
			if (gvram[addr] != mem) {
				gvram[addr] = (BYTE)mem;
				render->GrpMem(addr, 2);
			}
			return;
		}

		// Page 3: upper word byte bits b4-b7
		addr &= 0x7ffff;
		mem = (gvram[addr] & 0x0f);
		mem |= (data << 4);

		// Write
		if (gvram[addr] != mem) {
			gvram[addr] = (BYTE)mem;
			render->GrpMem(addr, 3);
		}
		return;
	}

	// Even addresses cannot be written
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM16::WriteWord(DWORD addr, DWORD data)
{
	DWORD mem;

	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	if (addr < 0x80000) {
		// Page 0: lower word byte bits b0-b3
		mem = (gvram[addr] & 0xf0);
		mem |= (data & 0x0f);

		// Write
		if (gvram[addr] != mem) {
			gvram[addr] = (BYTE)mem;
			render->GrpMem(addr, 0);
		}
		return;
	}
	if (addr < 0x100000) {
		// Page 1: lower word byte bits b4-b7
		addr &= 0x7ffff;
		mem = (gvram[addr] & 0x0f);
		data &= 0x0f;
		mem |= (data << 4);

		// Write
		if (gvram[addr] != mem) {
			gvram[addr] = (BYTE)mem;
			render->GrpMem(addr, 1);
		}
		return;
	}
	if (addr < 0x180000) {
		// Page 2: upper word byte bits b0-b3
		addr &= 0x7ffff;
		mem = (gvram[addr ^ 1] & 0xf0);
		mem |= (data & 0x0f);

		// Write
		if (gvram[addr ^ 1] != mem) {
			gvram[addr ^ 1] = (BYTE)mem;
			render->GrpMem(addr ^ 1, 2);
		}
		return;
	}

	// Page 3: upper word byte bits b4-b7
	addr &= 0x7ffff;
	mem = (gvram[addr ^ 1] & 0x0f);
	data &= 0x0f;
	mem |= (data << 4);

	// Write
	if (gvram[addr ^ 1] != mem) {
		gvram[addr ^ 1] = (BYTE)mem;
		render->GrpMem(addr ^ 1, 3);
	}
}

//---------------------------------------------------------------------------
//
//	Read-only
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM16::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Odd addresses only
	if (addr & 1) {
		if (addr < 0x80000) {
			// Page 0: lower word byte bits b0-b3
			return (gvram[addr ^ 1] & 0x0f);
		}

		if (addr < 0x100000) {
			// Page 1: lower word byte bits b4-b7
			addr &= 0x7ffff;
			return (gvram[addr ^ 1] >> 4);
		}

		if (addr < 0x180000) {
			// Page 2: upper word byte bits b0-b3
			addr &= 0x7ffff;
			return (gvram[addr] & 0x0f);
		}

		// Page 3: upper word byte bits b4-b7
		addr &= 0x7ffff;
		return (gvram[addr] >> 4);
	}

	// Even addresses are always 0
	return 0;
}

//===========================================================================
//
//	Graphic VRAM handler (256 colors)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
GVRAM256::GVRAM256(Render *rend, BYTE *mem, CPU *p) : GVRAMHandler(rend, mem, p)
{
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM256::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Page 0
	if (addr < 0x80000) {
		if (addr & 1) {
			// Lower word byte
			return gvram[addr ^ 1];
		}
		return 0;
	}

	// Page 1
	if (addr < 0x100000) {
		addr &= 0x7ffff;
		if (addr & 1) {
			// Upper word byte
			return gvram[addr];
		}
		return 0;
	}

	// Bus error
	cpu->BusErr(addr + 0xc00000, TRUE);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM256::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT((addr & 1) == 0);

	// Page 0
	if (addr < 0x80000) {
		// Lower word byte
		return gvram[addr];
	}

	// Page 1
	if (addr < 0x100000) {
		addr &= 0x7ffff;
		// Upper word byte
		return gvram[addr ^ 1];
	}

	// Bus error
	cpu->BusErr(addr + 0xc00000, TRUE);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM256::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT(data < 0x100);

	// Page 0
	if (addr < 0x80000) {
		if (addr & 1) {
			// Lower word byte
			if (gvram[addr ^ 1] != data) {
				gvram[addr ^ 1] = (BYTE)data;
				render->GrpMem(addr ^ 1, 0);
			}
		}
		return;
	}

	// Page 1 (block 2)
	if (addr < 0x100000) {
		addr &= 0x7ffff;
		if (addr & 1) {
			// Upper word byte
			if (gvram[addr] != data) {
				gvram[addr] = (BYTE)data;
				render->GrpMem(addr, 2);
			}
		}
		return;
	}

	// Bus error
	cpu->BusErr(addr + 0xc00000, FALSE);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM256::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	// Page 0
	if (addr < 0x80000) {
		// Lower word byte
		if (gvram[addr] != data) {
			gvram[addr] = (BYTE)data;
			render->GrpMem(addr, 0);
		}
		return;
	}

	// Page 1 (block 2)
	if (addr < 0x100000) {
		addr &= 0x7ffff;
		// Upper word byte
		if (gvram[addr ^ 1] != data) {
			gvram[addr ^ 1] = (BYTE)data;
			render->GrpMem(addr ^ 1, 2);
		}
		return;
	}

	// Bus error
	cpu->BusErr(addr + 0xc00000, FALSE);
}

//---------------------------------------------------------------------------
//
//	Read-only
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM256::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Page 0
	if (addr < 0x80000) {
		if (addr & 1) {
			// Lower word byte
			return gvram[addr ^ 1];
		}
		return 0;
	}

	// Page 1
	if (addr < 0x100000) {
		addr &= 0x7ffff;
		if (addr & 1) {
			// Upper word byte
			return gvram[addr];
		}
		return 0;
	}

	// Bus error
	return 0xff;
}

//===========================================================================
//
//	Graphic VRAM handler (invalid)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
GVRAMNDef::GVRAMNDef(Render *rend, BYTE *mem, CPU *p) : GVRAMHandler(rend, mem, p)
{
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAMNDef::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Invalid page
	if (addr & 0x80000) {
		return 0;
	}

	// Lower page
	addr &= 0x7ffff;
	if (addr & 1) {
		return gvram[addr ^ 1];
	}
	return 0;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAMNDef::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT((addr & 1) == 0);

	// Invalid page
	if (addr & 0x80000) {
		return 0;
	}

	// Lower page
	addr &= 0x7ffff;
	return gvram[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAMNDef::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT(data < 0x100);

	// Invalid page
	if (addr & 0x80000) {
		return;
	}

	// Lower page
	addr &= 0x7ffff;
	if (addr & 1) {
		if (gvram[addr ^ 1] != data) {
			gvram[addr ^ 1] = (BYTE)data;
			render->GrpMem(addr ^ 1, 0);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAMNDef::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	// Invalid page
	if (addr & 0x80000) {
		return;
	}

	// Lower page
	addr &= 0x7ffff;
	if (gvram[addr ^ 1] != data) {
		gvram[addr ^ 1] = (BYTE)data;
		render->GrpMem(addr ^ 1, 0);
	}
}

//---------------------------------------------------------------------------
//
//	Read-only
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAMNDef::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Invalid page
	if (addr & 0x80000) {
		return 0;
	}

	// Lower page
	addr &= 0x7ffff;
	if (addr & 1) {
		return gvram[addr ^ 1];
	}
	return 0;
}

//===========================================================================
//
//	Graphic VRAM handler (65536 colors)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
GVRAM64K::GVRAM64K(Render *rend, BYTE *mem, CPU *p) : GVRAMHandler(rend, mem, p)
{
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM64K::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	if (addr < 0x80000) {
		return gvram[addr ^ 1];
	}

	// Bus error
	cpu->BusErr(addr + 0xc00000, TRUE);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM64K::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT((addr & 1) == 0);

	if (addr < 0x80000) {
		return *(WORD*)(&gvram[addr]);
	}

	// Bus error
	cpu->BusErr(addr + 0xc00000, TRUE);
	return 0xffff;
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM64K::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT(data < 0x100);

	if (addr < 0x80000) {
		if (gvram[addr ^ 1] != data) {
			gvram[addr ^ 1] = (BYTE)data;
			render->GrpMem(addr ^ 1, 0);
		}
		return;
	}

	// Bus error
	cpu->BusErr(addr + 0xc00000, FALSE);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM64K::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	if (addr < 0x80000) {
		if (*(WORD*)(&gvram[addr]) != data) {
			*(WORD*)(&gvram[addr]) = (WORD)data;
			render->GrpMem(addr, 0);
		}
		return;
	}

	// Bus error
	cpu->BusErr(addr + 0xc00000, FALSE);
}

//---------------------------------------------------------------------------
//
//	Read-only
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM64K::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	if (addr < 0x80000) {
		return gvram[addr ^ 1];
	}

	// Bus error
	return 0xff;
}

//===========================================================================
//
//	Graphic VRAM
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
GVRAM::GVRAM(VM *p) : MemDevice(p)
{
	// Initialize the device ID
	dev.id = MAKEID('G', 'V', 'R', 'M');
	dev.desc = "Graphic VRAM";

	// Start and end addresses
	memdev.first = 0xc00000;
	memdev.last = 0xdfffff;

	// Work area
	gvram = NULL;
	render = NULL;

	// Handler
	handler = NULL;
	hand1024 = NULL;
	hand16 = NULL;
	hand256 = NULL;
	handNDef = NULL;
	hand64K = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL GVRAM::Init()
{
	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Allocate and clear memory
	try {
		gvram = new BYTE[ 0x80000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!gvram) {
		return FALSE;
	}
	memset(gvram, 0, 0x80000);

	// Get the renderer
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// Create the handlers
	hand1024 = new GVRAM1024(render, gvram, cpu);
	hand16 = new GVRAM16(render, gvram, cpu);
	hand256 = new GVRAM256(render, gvram, cpu);
	handNDef = new GVRAMNDef(render, gvram, cpu);
	hand64K = new GVRAM64K(render, gvram, cpu);

	// Initialize the data
	gvdata.mem = TRUE;
	gvdata.siz = 0;
	gvdata.col = 3;
	gvcount = 0;

	// Initialize the handlers (64K)
	gvdata.type = 4;
	handler = hand64K;

	// Fast-clear mask
	gvdata.mask[0] = 0xfff0;
	gvdata.mask[1] = 0xff0f;
	gvdata.mask[2] = 0xf0ff;
	gvdata.mask[3] = 0x0fff;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::Cleanup()
{
	ASSERT(this);

	// Release the handlers
	if (hand64K) {
		delete hand64K;
		hand64K = NULL;
	}
	if (handNDef) {
		delete handNDef;
		handNDef = NULL;
	}
	if (hand256) {
		delete hand256;
		hand256 = NULL;
	}
	if (hand16) {
		delete hand16;
		hand16 = NULL;
	}
	if (hand1024) {
		delete hand1024;
		hand1024 = NULL;
	}
	handler = NULL;

	// Release memory
	if (gvram) {
		delete[] gvram;
		gvram = NULL;
	}

	// Return to the base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::Reset()
{
	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "リセット");

	// No fast clear
	gvdata.plane[0] = FALSE;
	gvdata.plane[1] = FALSE;
	gvdata.plane[2] = FALSE;
	gvdata.plane[3] = FALSE;

	// Initialize the handlers (64K)
	gvdata.mem = TRUE;
	gvdata.siz = 0;
	gvdata.col = 3;
	gvdata.type = 4;
	handler = hand64K;

	// Clear the access count
	gvcount = 0;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL GVRAM::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	LOG0(Log::Normal, "セーブ");

	// Save memory
	if (!fio->Write(gvram, 0x80000)) {
		return FALSE;
	}

	// Save the size
	sz = sizeof(gvram_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save the payload
	if (!fio->Write(&gvdata, (int)sz)) {
		return FALSE;
	}

	// gvcount (added in version 2.04)
	if (!fio->Write(&gvcount, sizeof(gvcount))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL GVRAM::Load(Fileio *fio, int ver)
{
	size_t sz;
	int line;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);
	ASSERT_DIAG();

	LOG0(Log::Normal, "ロード");

	// Load memory
	if (!fio->Read(gvram, 0x80000)) {
		return FALSE;
	}

	// Load and verify the size
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(gvram_t)) {
		return FALSE;
	}

	// Load the payload
	if (!fio->Read(&gvdata, (int)sz)) {
		return FALSE;
	}

	// gvcount (added in version 2.04)
	gvcount = 0;
	if (ver >= 0x0204) {
		if (!fio->Read(&gvcount, sizeof(gvcount))) {
			return FALSE;
		}
	}

	// Notify the renderer
	for (line=0; line<0x200; line++) {
		render->GrpAll(line, 0);
		render->GrpAll(line, 1);
		render->GrpAll(line, 2);
		render->GrpAll(line, 3);
	}

	// Select the handler
	switch (gvdata.type) {
		case 0:
			ASSERT(hand1024);
			handler = hand1024;
			break;
		// 16-color type
		case 1:
			ASSERT(hand16);
			handler = hand16;
			break;
		// 256-color type
		case 2:
			ASSERT(hand256);
			handler = hand256;
			break;
		// Undefined type
		case 3:
			ASSERT(handNDef);
			handler = handNDef;
			break;
		// 64K-color type
		case 4:
			ASSERT(hand64K);
			handler = hand64K;
			break;
		// Other
		default:
			ASSERT(FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "設定適用");
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Diagnostics
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::AssertDiag() const
{
	// Base class
	MemDevice::AssertDiag();

	ASSERT(this);
	ASSERT(GetID() == MAKEID('G', 'V', 'R', 'M'));
	ASSERT(memdev.first == 0xc00000);
	ASSERT(memdev.last == 0xdfffff);
	ASSERT(gvram);
	ASSERT(hand1024);
	ASSERT(hand16);
	ASSERT(hand256);
	ASSERT(handNDef);
	ASSERT(hand64K);
	ASSERT(handler);
	ASSERT((gvdata.mem == TRUE) || (gvdata.mem == FALSE));
	ASSERT((gvdata.siz == 0) || (gvdata.siz == 1));
	ASSERT((gvdata.col >= 0) && (gvdata.col <= 3));
	ASSERT((gvdata.type >= 0) && (gvdata.type <= 4));
	ASSERT((gvcount == 0) || (gvcount == 1));
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// Wait (0.5 wait)
	scheduler->Wait(gvcount);
	gvcount ^= 1;

	// Leave it to the handler
	return handler->ReadByte(addr & 0x1fffff);
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT_DIAG();

	// Wait (0.5 wait)
	scheduler->Wait(gvcount);
	gvcount ^= 1;

	// Leave it to the handler
	return handler->ReadWord(addr & 0x1fffff);
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// Wait (0.5 wait)
	scheduler->Wait(gvcount);
	gvcount ^= 1;

	// Leave it to the handler
	handler->WriteByte(addr & 0x1fffff, data);
	if (render) {
		render->GVRAMWrite(addr & 0x1fffff, (BYTE)data);
	}
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);
	ASSERT_DIAG();

	// Wait (0.5 wait)
	scheduler->Wait(gvcount);
	gvcount ^= 1;

	// Leave it to the handler
	handler->WriteWord(addr & 0x1fffff, data);
	if (render) {
		render->GVRAMWrite(addr & 0x1fffff, (BYTE)((data >> 8) & 0xff));
		render->GVRAMWrite((addr + 1) & 0x1fffff, (BYTE)(data & 0xff));
	}
}

//---------------------------------------------------------------------------
//
//	Read-only
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// Leave it to the handler
	return handler->ReadOnly(addr & 0x1fffff);
}

//---------------------------------------------------------------------------
//
//	Get GVRAM
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL GVRAM::GetGVRAM() const
{
	ASSERT(this);
	ASSERT_DIAG();

	return gvram;
}

//---------------------------------------------------------------------------
//
//	Set the type
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::SetType(DWORD type)
{
	int next;
	int prev;
	BOOL mem;
	DWORD siz;

	ASSERT(this);
	ASSERT_DIAG();

	// Save the current value
	mem = gvdata.mem;
	siz = gvdata.siz;

	// Set the value
	if (type & 8) {
		gvdata.mem = TRUE;
	}
	else {
		gvdata.mem = FALSE;
	}
	if (type & 4) {
		gvdata.siz = 1;
	}
	else {
		gvdata.siz = 0;
	}
	gvdata.col = type & 3;

	// Store the current gvdata.type
	prev = gvdata.type;

	// Check the new type
	if (gvdata.mem) {
		next = 4;
	}
	else {
		if (gvdata.siz) {
			next = 0;
		}
		else {
			next = gvdata.col + 1;
		}
	}

	// Recreate it if it differs
	if (prev != next) {
		switch (next) {
			// 1024-color type
			case 0:
				ASSERT(hand1024);
				handler = hand1024;
				break;
			// 16-color type
			case 1:
				ASSERT(hand16);
				handler = hand16;
				break;
			// 256-color type
			case 2:
				ASSERT(hand256);
				handler = hand256;
				break;
			// Undefined type
			case 3:
				LOG0(Log::Warning, "グラフィックVRAM 未定義タイプ");
				ASSERT(handNDef);
				handler = handNDef;
				break;
			// 64K-color type
			case 4:
				ASSERT(hand64K);
				handler = hand64K;
				break;
			// Other
			default:
				ASSERT(FALSE);
		}
		gvdata.type = next;
	}

	// Notify the renderer if the memory type or actual screen size differs
	if ((gvdata.mem != mem) || (gvdata.siz != siz)) {
		render->SetVC();
	}
}

//---------------------------------------------------------------------------
//
//	Set fast clear
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::FastSet(DWORD mask)
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(GVRAM_LOG)
	LOG1(Log::Normal, "高速クリアプレーン指定 %02X", mask);
#endif	// GVRAM_LOG

	if (mask & 0x08) {
		gvdata.plane[3] = TRUE;
	}
	else {
		gvdata.plane[3] = FALSE;
	}

	if (mask & 0x04) {
		gvdata.plane[2] = TRUE;
	}
	else {
		gvdata.plane[2] = FALSE;
	}

	if (mask & 0x02) {
		gvdata.plane[1] = TRUE;
	}
	else {
		gvdata.plane[1] = FALSE;
	}

	if (mask & 0x01) {
		gvdata.plane[0] = TRUE;
	}
	else {
		gvdata.plane[0] = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Fast clear
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::FastClr(const CRTC::crtc_t *p)
{
	ASSERT(this);
	ASSERT_DIAG();

	if (gvdata.siz) {
		// 1024x1024
		if (p->hd >= 1) {
			// 1024x1024, 512 or 768
			FastClr768(p);
		}
		else {
			// 1024x1024, 256
			FastClr256(p);
		}
	}
	else {
		// 512x512
		FastClr512(p);
	}
}

//---------------------------------------------------------------------------
//
//	Fast clear 1024x1024 512/768
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::FastClr768(const CRTC::crtc_t *p)
{
	int y;
	int n;
	int j;
	int k;
	DWORD offset;
	WORD *q;

	ASSERT(this);
	ASSERT_DIAG();

#if defined(GVRAM_LOG)
	LOG0(Log::Normal, "高速クリア 1024x1024 (512/768幅)");
#endif	// GVRAM_LOG

	// Get the Y offset and line count n
	y = p->v_scan;
	n = 1;
	if ((p->v_mul == 2) && !(p->lowres)) {
		if (y & 1) {
			return;
		}
		y >>= 1;
	}
	if (p->v_mul == 0) {
		y <<= 1;
		n = 2;
	}

	// Line loop
	for (j=0; j<n; j++) {
		// Get the offset from the scroll registers
		offset = (y + p->grp_scrly[0]) & 0x3ff;

		// Split into upper and lower halves
		if (offset < 512) {
			// Build the pointer
			q = (WORD*)&gvram[offset << 10];

			// Clear the lower byte
			for (k=0; k<512; k++) {
				*q++ &= 0xff00;
			}

			// Raise the flag
			render->GrpAll(offset, 0);
			render->GrpAll(offset, 1);
		}
		else {
			// Build the pointer
			offset &= 0x1ff;
			q = (WORD*)&gvram[offset << 10];

			// Clear the upper byte
			for (k=0; k<512; k++) {
				*q++ &= 0x00ff;
			}

			// Raise the flag
			render->GrpAll(offset, 2);
			render->GrpAll(offset, 3);
		}

		// Advance to the next line
		y++;
	}
}

//---------------------------------------------------------------------------
//
//	Fast clear 1024x1024 256
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::FastClr256(const CRTC::crtc_t *p)
{
#if defined(GVRAM_LOG)
	LOG0(Log::Normal, "高速クリア 1024x1024 (256幅)");
#endif	// GVRAM_LOG

	// Temporary measure
	FastClr768(p);
}

//---------------------------------------------------------------------------
//
//	Fast clear 512x512
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::FastClr512(const CRTC::crtc_t *p)
{
	int y;
	int n;
	int i;
	int j;
	int k;
	int w[2];
	DWORD offset;
	WORD *q;

	ASSERT(this);
	ASSERT_DIAG();

#if defined(GVRAM_LOG)
	LOG1(Log::Normal, "高速クリア 512x512 Scan=%d", p->v_scan);
#endif	// GVRAM_LOG

	// Get the Y offset and line count n
	y = p->v_scan;
	n = 1;
	if ((p->v_mul == 2) && !(p->lowres)) {
		if (y & 1) {
			return;
		}
		y >>= 1;
	}
	if (p->v_mul == 0) {
		y <<= 1;
		n = 2;
	}

	// Plane loop
	for (i=0; i<4; i++) {
		if (!gvdata.plane[i]) {
			continue;
		}

		// Calculate the width
		w[0] = p->h_dots;
		w[1] = 0;
		if (((p->grp_scrlx[i] & 0x1ff) + w[0]) > 512) {
			w[1] = (p->grp_scrlx[i] & 0x1ff) + w[0] - 512;
			w[0] = 512 - (p->grp_scrlx[i] & 0x1ff);
		}

		// Line loop
		for (j=0; j<n; j++) {
			// Get the offset from the scroll registers
			offset = ((y + p->grp_scrly[i]) & 0x1ff) << 10;
			q = (WORD*)&gvram[offset + ((p->grp_scrlx[i] & 0x1ff) << 1)];

			// Clear (1)
			for (k=0; k<w[0]; k++) {
				*q++ &= gvdata.mask[i];
			}
			if (w[1] > 0) {
				// Clear (2)
				q = (WORD*)&gvram[offset];
				for (k=0; k<w[1]; k++) {
					*q++ &= gvdata.mask[i];
				}
			}

			// Raise the flag
			render->GrpAll(offset >> 10, i);

			// Advance to the next line
			y++;
		}

		// Move back one line
		y -= n;
	}
}

