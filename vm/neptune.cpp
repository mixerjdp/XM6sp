//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ Neptune-X ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "neptune.h"

//===========================================================================
//
//	Neptune-X
//
//===========================================================================
//#define NEPTUNE_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Neptune::Neptune(VM *p) : MemDevice(p)
{
	// Device ID setting
	dev.id = MAKEID('N', 'E', 'P', 'X');
	dev.desc = "Neptune-X (DP8390)";

	// Start address, End address
	memdev.first = 0xece000;
	memdev.last = 0xecffff;
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL Neptune::Init()
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
void FASTCALL Neptune::Cleanup()
{
 ASSERT(this);

 // Base class
 MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Neptune::Reset()
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
BOOL FASTCALL Neptune::Save(Fileio* /*fio*/, int /*ver*/)
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
BOOL FASTCALL Neptune::Load(Fileio* /*fio*/, int /*ver*/)
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
void FASTCALL Neptune::ApplyCfg(const Config* /*config*/)
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
void FASTCALL Neptune::AssertDiag() const
{
	// Base class
	MemDevice::AssertDiag();

 ASSERT(this);
 ASSERT(GetID() == MAKEID('N', 'E', 'P', 'X'));
 ASSERT(memdev.first == 0xece000);
 ASSERT(memdev.last == 0xecffff);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL Neptune::ReadByte(DWORD addr)
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
DWORD FASTCALL Neptune::ReadWord(DWORD addr)
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
void FASTCALL Neptune::WriteByte(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 printf("%d", data);

 // Bus error
 cpu->BusErr(addr, FALSE);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL Neptune::WriteWord(DWORD addr, DWORD data)
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
DWORD FASTCALL Neptune::ReadOnly(DWORD addr) const
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT_DIAG();

 printf("%d", addr);

 return 0xff;
}