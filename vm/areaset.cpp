//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// [ Area Set ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "cpu.h"
#include "log.h"
#include "fileio.h"
#include "memory.h"
#include "areaset.h"

//===========================================================================
//
// Area Set
//
//===========================================================================
//#define AREASET_LOG

//---------------------------------------------------------------------------
//
// Constructor
//
//---------------------------------------------------------------------------
AreaSet::AreaSet(VM *p) : MemDevice(p)
{
// Initialize the device ID
	dev.id = MAKEID('A', 'R', 'E', 'A');
	dev.desc = "Area Set";

// Start and end addresses
	memdev.first = 0xe86000;
	memdev.last = 0xe87fff;

// Object
	memory = NULL;
}

//---------------------------------------------------------------------------
//
// Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL AreaSet::Init()
{
	ASSERT(this);

// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

// Acquire memory
	memory = (Memory*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(memory);

	return TRUE;
}

//---------------------------------------------------------------------------
//
// Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL AreaSet::Cleanup()
{
	ASSERT(this);

// Back to base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
// Reset
// Called from Memory::MakeContext, not through the normal sequence.
//
//---------------------------------------------------------------------------
void FASTCALL AreaSet::Reset()
{
	ASSERT(this);
	LOG0(Log::Normal, "リセット");

#if defined(AREASET_LOG)
	LOG0(Log::Normal, "エリアセット設定 $00");
#endif	// AREASET_LOG

// Initialize area selection
	area = 0;
}

//---------------------------------------------------------------------------
//
// Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL AreaSet::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

	ASSERT(this);
	LOG0(Log::Normal, "セーブ");

// Save size
	sz = sizeof(area);
	if (!fio->Write(&sz, (int)sizeof(sz))) {
		return FALSE;
	}

// Save area information
	if (!fio->Write(&area, (int)sizeof(area))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
// Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL AreaSet::Load(Fileio *fio, int /*ver*/)
{
	size_t sz;

	ASSERT(this);
	LOG0(Log::Normal, "ロード");

// Load size
	if (!fio->Read(&sz, (int)sizeof(sz))) {
		return FALSE;
	}

// Compare size
	if (sz != sizeof(area)) {
		return FALSE;
	}

// Load area information
	if (!fio->Read(&area, (int)sizeof(area))) {
		return FALSE;
	}

// Apply
	memory->MakeContext(FALSE);

	return TRUE;
}

//---------------------------------------------------------------------------
//
// Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL AreaSet::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);
	LOG0(Log::Normal, "設定適用");
}

//---------------------------------------------------------------------------
//
// Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL AreaSet::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

// Always bus error
	cpu->BusErr(addr, TRUE);
	return 0xff;
}

//---------------------------------------------------------------------------
//
// Byte write
//
//---------------------------------------------------------------------------
void FASTCALL AreaSet::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

// Map every other byte
	addr &= 1;

// Odd addresses are area set
	if (addr & 1) {
		LOG1(Log::Detail, "エリアセット設定 $%02X", data);

// Store data
		area = data;

// Rebuild memory map
		memory->MakeContext(FALSE);
		return;
	}

// Even addresses are not decoded
}

//---------------------------------------------------------------------------
//
// Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL AreaSet::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

// EVEN returns 0xff; ODD returns the configured value
	if (addr & 1) {
		return area;
	}
	return 0xff;
}

//---------------------------------------------------------------------------
//
// Get area set
//
//---------------------------------------------------------------------------
DWORD FASTCALL AreaSet::GetArea() const
{
	ASSERT(this);
	return area;
}
