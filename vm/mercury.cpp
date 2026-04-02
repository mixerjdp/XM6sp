//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ Mercury-Unit ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "mercury.h"

//===========================================================================
//
//	Mercury-Unit
//
//===========================================================================
//#define MERCURY_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Mercury::Mercury(VM *p) : MemDevice(p)
{
	// Device ID creation
	dev.id = MAKEID('M', 'E', 'R', 'C');
	dev.desc = "Mercury-Unit";

	// Start address, I/O address
	memdev.first = 0xecc000;
	memdev.last = 0xecdfff;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL Mercury::Init()
{
 ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL Mercury::Cleanup()
{
 ASSERT(this);

	// Base class cleanup
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Mercury::Reset()
{
 ASSERT(this);
 ASSERT_DIAG();

	LOG0(Log::Normal, "Reset");
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Mercury::Save(Fileio* /*fio*/, int /*ver*/)
{
 ASSERT(this);
 ASSERT_DIAG();

	LOG0(Log::Normal, "Save");

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Mercury::Load(Fileio* /*fio*/, int /*ver*/)
{
 ASSERT(this);
 ASSERT_DIAG();

	LOG0(Log::Normal, "Load");

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL Mercury::ApplyCfg(const Config* /*config*/)
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
void FASTCALL Mercury::AssertDiag() const
{
	// Base class
	MemDevice::AssertDiag();

 ASSERT(this);
 ASSERT(GetID() == MAKEID('M', 'E', 'R', 'C'));
 ASSERT(memdev.first == 0xecc000);
 ASSERT(memdev.last == 0xecdfff);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL Mercury::ReadByte(DWORD addr)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT_DIAG();

	// Bus error
	cpu->BusErr(addr, TRUE);

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL Mercury::ReadWord(DWORD addr)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT((addr & 1) == 0);
 ASSERT_DIAG();

	return (0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL Mercury::WriteByte(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT(data < 0x100);
 ASSERT_DIAG();

	printf("%d",data);

	// Bus error
	cpu->BusErr(addr, FALSE);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL Mercury::WriteWord(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT((addr & 1) == 0);
 ASSERT(data < 0x10000);
 ASSERT_DIAG();

	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL Mercury::ReadOnly(DWORD addr) const
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT_DIAG();
	printf("%d", addr);

	return 0xff;
}
