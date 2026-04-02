//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI(ytanaka@ipc-tokai.or.jp)
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
//	Graphic VRAMHandler
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
//	Graphic VRAMHandler(1024x1024)
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

	// Odd byte returns 0
	if ((addr & 1) == 0) {
		return 0x00;
	}

	// Odd address
	offset = addr & 0x3ff;

	// Interleaved bank arrangement
	if (addr & 0x100000) {
		if (addr & 0x400) {
			// Page3 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] >> 4);
		}
		else {
			// Page2 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] & 0x0f);
		}
	}
	else {
		if (addr & 0x400) {
			// Page1 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] >> 4);
		}
		else {
			// Page0 access
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

	// Odd address
	offset = addr & 0x3ff;

	// Interleaved bank arrangement
	if (addr & 0x100000) {
		if (addr & 0x400) {
			// Page3 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] >> 4);
		}
		else {
			// Page2 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] & 0x0f);
		}
	}
	else {
		if (addr & 0x400) {
			// Page1 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] >> 4);
		}
		else {
			// Page0 access
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

	// Odd byte is ignored
	if ((addr & 1) == 0) {
		return;
	}

	// Odd address
	offset = addr & 0x3ff;

	// Interleaved bank arrangement
	if (addr & 0x100000) {
		if (addr & 0x400) {
			// Page3 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// Upper nibble merge
			mem = (gvram[addr] & 0x0f);
			mem |= (data << 4);

			// Write
			if (gvram[addr] != mem) {
				gvram[addr] = (BYTE)mem;
				render->GrpMem(addr, 3);
			}
		}
		else {
			// Page2 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// Lower nibble merge
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
			// Page1 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// Upper nibble merge
			mem = (gvram[addr ^ 1] & 0x0f);
			mem |= (data << 4);

			// Write
			if (gvram[addr ^ 1] != mem) {
				gvram[addr ^ 1] = (BYTE)mem;
				render->GrpMem(addr ^ 1, 1);
			}
		}
		else {
			// Page0 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// Lower nibble merge
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

	// Odd address
	offset = addr & 0x3ff;

	// Interleaved bank arrangement
	if (addr & 0x100000) {
		if (addr & 0x400) {
			// Page3 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// Upper nibble merge
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
			// Page2 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// Lower nibble merge
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
			// Page1 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// Upper nibble merge
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
			// Page0 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;

			// Lower nibble merge
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
//	Read only 
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM1024::ReadOnly(DWORD addr) const
{
	DWORD offset;

	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Odd byte returns 0
	if ((addr & 1) == 0) {
		return 0x00;
	}

	// Odd address
	offset = addr & 0x3ff;

	// Interleaved bank arrangement
	if (addr & 0x100000) {
		if (addr & 0x400) {
			// Page3 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] >> 4);
		}
		else {
			// Page2 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr] & 0x0f);
		}
	}
	else {
		if (addr & 0x400) {
			// Page1 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] >> 4);
		}
		else {
			// Page0 access
			addr >>= 1;
			addr &= 0x7fc00;
			addr |= offset;
			return (gvram[addr ^ 1] & 0x0f);
		}
	}
}

//===========================================================================
//
//	Graphic VRAMHandler(16 colors)
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

	// Odd address 
	if (addr & 1) {
		if (addr < 0x80000) {
			// Page0:Lower nibble of word b0-b3
			return (gvram[addr ^ 1] & 0x0f);
		}

		if (addr < 0x100000) {
			// Page1:Lower nibble of word b4-b7
			addr &= 0x7ffff;
			return (gvram[addr ^ 1] >> 4);
		}

		if (addr < 0x180000) {
			// Page2:Upper nibble of word b0-b3
			addr &= 0x7ffff;
			return (gvram[addr] & 0x0f);
		}

		// Page3:Upper nibble of word b4-b7
		addr &= 0x7ffff;
		return (gvram[addr] >> 4);
	}

	// Odd address is always 0
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
		// Page0:Lower nibble of word b0-b3
		return (gvram[addr] & 0x0f);
	}

	if (addr < 0x100000) {
		// Page1:Lower nibble of word b4-b7
		addr &= 0x7ffff;
		return (gvram[addr] >> 4);
	}

	if (addr < 0x180000) {
		// Page2:Upper nibble of word b0-b3
		addr &= 0x7ffff;
		return (gvram[addr ^ 1] & 0x0f);
	}

	// Page3:Upper nibble of word b4-b7
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

	// Odd address 
	if (addr & 1) {
		if (addr < 0x80000) {
			// Page0:Lower nibble of word b0-b3
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
			// Page1:Lower nibble of word b4-b7
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
			// Page2:Upper nibble of word b0-b3
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

		// Page3:Upper nibble of word b4-b7
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

	// Odd byte is ignored
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
		// Page0:Lower nibble of word b0-b3
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
		// Page1:Lower nibble of word b4-b7
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
		// Page2:Upper nibble of word b0-b3
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

	// Page3:Upper nibble of word b4-b7
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
//	Read only 
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM16::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Odd address 
	if (addr & 1) {
		if (addr < 0x80000) {
			// Page0:Lower nibble of word b0-b3
			return (gvram[addr ^ 1] & 0x0f);
		}

		if (addr < 0x100000) {
			// Page1:Lower nibble of word b4-b7
			addr &= 0x7ffff;
			return (gvram[addr ^ 1] >> 4);
		}

		if (addr < 0x180000) {
			// Page2:Upper nibble of word b0-b3
			addr &= 0x7ffff;
			return (gvram[addr] & 0x0f);
		}

		// Page3:Upper nibble of word b4-b7
		addr &= 0x7ffff;
		return (gvram[addr] >> 4);
	}

	// Odd address is always 0
	return 0;
}

//===========================================================================
//
//	Graphic VRAMHandler(256 colors)
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

	// Page0
	if (addr < 0x80000) {
		if (addr & 1) {
			// Lower nibble of word
			return gvram[addr ^ 1];
		}
		return 0;
	}

	// Page1
	if (addr < 0x100000) {
		addr &= 0x7ffff;
		if (addr & 1) {
			// Upper nibble of word
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

	// Page0
	if (addr < 0x80000) {
		// Lower nibble of word
		return gvram[addr];
	}

	// Page1
	if (addr < 0x100000) {
		addr &= 0x7ffff;
		// Upper nibble of word
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

	// Page0
	if (addr < 0x80000) {
		if (addr & 1) {
			// Lower nibble of word
			if (gvram[addr ^ 1] != data) {
				gvram[addr ^ 1] = (BYTE)data;
				render->GrpMem(addr ^ 1, 0);
			}
		}
		return;
	}

	// Page1(Block2)
	if (addr < 0x100000) {
		addr &= 0x7ffff;
		if (addr & 1) {
			// Upper nibble of word
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

	// Page0
	if (addr < 0x80000) {
		// Lower nibble of word
		if (gvram[addr] != data) {
			gvram[addr] = (BYTE)data;
			render->GrpMem(addr, 0);
		}
		return;
	}

	// Page1(Block2)
	if (addr < 0x100000) {
		addr &= 0x7ffff;
		// Upper nibble of word
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
//	Read only 
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM256::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Page0
	if (addr < 0x80000) {
		if (addr & 1) {
			// Lower nibble of word
			return gvram[addr ^ 1];
		}
		return 0;
	}

	// Page1
	if (addr < 0x100000) {
		addr &= 0x7ffff;
		if (addr & 1) {
			// Upper nibble of word
			return gvram[addr];
		}
		return 0;
	}

	// Bus error
	return 0xff;
}

//===========================================================================
//
//	Graphic VRAMHandler(Undefined)
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

	// Undefined page
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

	// Undefined page
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

	// Undefined page
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

	// Undefined page
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
//	Read only 
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAMNDef::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT(addr <= 0x1fffff);

	// Undefined page
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
//	Graphic VRAMHandler(65536 colors)
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
//	Read only 
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
	// Device ID Initialize
	dev.id = MAKEID('G', 'V', 'R', 'M');
	dev.desc = "Graphic VRAM";

	// Start address, End address
	memdev.first = 0xc00000;
	memdev.last = 0xdfffff;

	// Video RAM
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

	// Internal allocate, Cleanup
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

	// Renderer get
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// Handler create
	hand1024 = new GVRAM1024(render, gvram, cpu);
	hand16 = new GVRAM16(render, gvram, cpu);
	hand256 = new GVRAM256(render, gvram, cpu);
	handNDef = new GVRAMNDef(render, gvram, cpu);
	hand64K = new GVRAM64K(render, gvram, cpu);

	// Memory Initialize
	gvdata.mem = TRUE;
	gvdata.siz = 0;
	gvdata.col = 3;
	gvcount = 0;

	// Handler default(64K)
	gvdata.type = 4;
	handler = hand64K;

	// Plane clear mask
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

	// Handler default
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

	// Internal default
	if (gvram) {
		delete[] gvram;
		gvram = NULL;
	}

	// Base class de
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

	LOG0(Log::Normal, "Reset");

	// Plane clear flag
	gvdata.plane[0] = FALSE;
	gvdata.plane[1] = FALSE;
	gvdata.plane[2] = FALSE;
	gvdata.plane[3] = FALSE;

	// Handler default(64K)
	gvdata.mem = TRUE;
	gvdata.siz = 0;
	gvdata.col = 3;
	gvdata.type = 4;
	handler = hand64K;

	// Access count 0
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

	LOG0(Log::Normal, "Save");

	// Internal Save
	if (!fio->Write(gvram, 0x80000)) {
		return FALSE;
	}

	// Size Save
	sz = sizeof(gvram_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Status Save
	if (!fio->Write(&gvdata, (int)sz)) {
		return FALSE;
	}

	// gvcount(version 2.04 or later)
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

	LOG0(Log::Normal, "Load");

	// Internal Load
	if (!fio->Read(gvram, 0x80000)) {
		return FALSE;
	}

	// Size Load, Verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(gvram_t)) {
		return FALSE;
	}

	// Status Load
	if (!fio->Read(&gvdata, (int)sz)) {
		return FALSE;
	}

	// gvcount(version 2.04 or later)
	gvcount = 0;
	if (ver >= 0x0204) {
		if (!fio->Read(&gvcount, sizeof(gvcount))) {
			return FALSE;
		}
	}

	// Renderer notify
	for (line=0; line<0x200; line++) {
		render->GrpAll(line, 0);
		render->GrpAll(line, 1);
		render->GrpAll(line, 2);
		render->GrpAll(line, 3);
	}

	// Handler select
	switch (gvdata.type) {
		case 0:
			ASSERT(hand1024);
			handler = hand1024;
			break;
		// 16 colorstype
		case 1:
			ASSERT(hand16);
			handler = hand16;
			break;
		// 256 colorstype
		case 2:
			ASSERT(hand256);
			handler = hand256;
			break;
		// Undefined type
		case 3:
			ASSERT(handNDef);
			handler = handNDef;
			break;
		// 64K colors type
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
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "Apply config");
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assert
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

	// Wait(0.5 cycle)
	scheduler->Wait(gvcount);
	gvcount ^= 1;

	// Handler delegate
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

	// Wait(0.5 cycle)
	scheduler->Wait(gvcount);
	gvcount ^= 1;

	// Handler delegate
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

	// Wait(0.5 cycle)
	scheduler->Wait(gvcount);
	gvcount ^= 1;

	// Handler delegate
	handler->WriteByte(addr & 0x1fffff, data);
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

	// Wait(0.5 cycle)
	scheduler->Wait(gvcount);
	gvcount ^= 1;

	// Handler delegate
	handler->WriteWord(addr & 0x1fffff, data);
}

//---------------------------------------------------------------------------
//
//	Read only 
//
//---------------------------------------------------------------------------
DWORD FASTCALL GVRAM::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// Handler delegate
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
//	Type set
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

	// Current value save
	mem = gvdata.mem;
	siz = gvdata.siz;

	// Set
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

	// Current gvdata.type keep
	prev = gvdata.type;

	// New type calc
	if (gvdata.mem) {
		next = 4;
	}
	else {
		if (gvdata.siz) {
			next = 0;
		}
		else {
			const bool use_fast_mapping =
				(render && (render->GetCompositorMode() == Render::compositor_fast));
			if (use_fast_mapping && (gvdata.col == 2)) {
				next = 2;
			}
			else {
				next = gvdata.col + 1;
			}
		}
	}

	// If changed, notifies the renderer
	if (prev != next) {
		switch (next) {
			// 1024 colorstype
			case 0:
				ASSERT(hand1024);
				handler = hand1024;
				break;
			// 16 colorstype
			case 1:
				ASSERT(hand16);
				handler = hand16;
				break;
			// 256 colorstype
			case 2:
				ASSERT(hand256);
				handler = hand256;
				break;
			// Undefined type
			case 3:
				LOG0(Log::Warning, "Graphic VRAM Undefined type");
				ASSERT(handNDef);
				handler = handNDef;
				break;
			// 64K colors type
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

	// Internal type differs from previous display size, notifies the renderer
	if ((gvdata.mem != mem) || (gvdata.siz != siz)) {
		render->SetVC();
	}
}

//---------------------------------------------------------------------------
//
//	Plane clear flag set
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::FastSet(DWORD mask)
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(GVRAM_LOG)
	LOG1(Log::Normal, "Plane clear flag set %02X", mask);
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
//	Plane clear
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::FastClr(const CRTC::crtc_t *p)
{
	ASSERT(this);
	ASSERT_DIAG();

	if (gvdata.siz) {
		// 1024x1024
		if (p->hd >= 1) {
			// 1024x1024,512 or 768
			FastClr768(p);
		}
		else {
			// 1024x1024,256
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
//	Plane clear 1024x1024 512/768
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
	LOG0(Log::Normal, "Plane clear 1024x1024 (512/768 width");
#endif	// GVRAM_LOG

	// Get offset y, scan line from
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

	// Scan line loop
	for (j=0; j<n; j++) {
		// Scroll register from offset get
		offset = (y + p->grp_scrly[0]) & 0x3ff;

		// Upper part, lower part combine
		if (offset < 512) {
			// Pointer create
			q = (WORD*)&gvram[offset << 10];

			// Lower byte clear
			for (k=0; k<512; k++) {
				*q++ &= 0xff00;
			}

			// FlagUp
			render->GrpAll(offset, 0);
			render->GrpAll(offset, 1);
		}
		else {
			// Pointer create
			offset &= 0x1ff;
			q = (WORD*)&gvram[offset << 10];

			// Upper byte clear
			for (k=0; k<512; k++) {
				*q++ &= 0x00ff;
			}

			// FlagUp
			render->GrpAll(offset, 2);
			render->GrpAll(offset, 3);
		}

		// Next scan line
		y++;
	}
}

//---------------------------------------------------------------------------
//
//	Plane clear 1024x1024 256
//
//---------------------------------------------------------------------------
void FASTCALL GVRAM::FastClr256(const CRTC::crtc_t *p)
{
#if defined(GVRAM_LOG)
	LOG0(Log::Normal, "Plane clear 1024x1024 (256 width");
#endif	// GVRAM_LOG

	// Dummy call
	FastClr768(p);
}

//---------------------------------------------------------------------------
//
//	Plane clear 512x512
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
	LOG1(Log::Normal, "Plane clear 512x512 Scan=%d", p->v_scan);
#endif	// GVRAM_LOG

	// Get offset y, scan line from
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

	// Plane scan loop
	for (i=0; i<4; i++) {
		if (!gvdata.plane[i]) {
			continue;
		}

		// Width calc
		w[0] = p->h_dots;
		w[1] = 0;
		if (((p->grp_scrlx[i] & 0x1ff) + w[0]) > 512) {
			w[1] = (p->grp_scrlx[i] & 0x1ff) + w[0] - 512;
			w[0] = 512 - (p->grp_scrlx[i] & 0x1ff);
		}

		// Scan line loop
		for (j=0; j<n; j++) {
			// Scroll register from offset get
			offset = ((y + p->grp_scrly[i]) & 0x1ff) << 10;
			q = (WORD*)&gvram[offset + ((p->grp_scrlx[i] & 0x1ff) << 1)];

			// Clear(1)
			for (k=0; k<w[0]; k++) {
				*q++ &= gvdata.mask[i];
			}
			if (w[1] > 0) {
				// Clear(2)
				q = (WORD*)&gvram[offset];
				for (k=0; k<w[1]; k++) {
					*q++ &= gvdata.mask[i];
				}
			}

			// FlagUp
			render->GrpAll(offset >> 10, i);

			// Next scan line
			y++;
		}

		// Scan line return
		y -= n;
	}
}

