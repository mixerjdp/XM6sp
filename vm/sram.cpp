//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ SRAM ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "filepath.h"
#include "fileio.h"
#include "cpu.h"
#include "schedule.h"
#include "memory.h"
#include "config.h"
#include "sram.h"

#include "sram_default.h"

//===========================================================================
//
//	SRAM
//
//===========================================================================
//#define SRAM_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SRAM::SRAM(VM *p) : MemDevice(p)
{
	// Device ID initialization
	dev.id = MAKEID('S', 'R', 'A', 'M');
	dev.desc = "Static RAM";

	// Start address, end address
	memdev.first = 0xed0000;
	memdev.last = 0xedffff;

	// Others initialization
	sram_size = 16;
	write_en = FALSE;
	mem_sync = TRUE;
	changed = FALSE;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL SRAM::Init()
{
	Fileio fio;
	int i;
	BYTE data;

ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Initialization

ASSERT(this);

	// Init
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// 
	memset(sram, 0xff, sizeof(sram));

	// Path create
	sram_path.SysFile(Filepath::SRAM);

#if defined(XM6_FORCE_DEFAULT_SRAM)
	// libretro: Apply default only on first session boot to initialize SRAM.DAT
	// On subsequent resets, try to load previous values to maintain persistence
	static bool session_initialized = false;
	if (!session_initialized) {
		memcpy(sram, kDefaultSramFile, sizeof(kDefaultSramFile));
		fio.Save(sram_path, (void *)kDefaultSramFile, sizeof(kDefaultSramFile));
		session_initialized = true;
		LOG0(Log::Normal, "SRAM: Initial session initialization (Template applied)");
	} else {
		// On reset, try to restore saved settings
		if (!fio.Load(sram_path, sram, sizeof(sram))) {
			memcpy(sram, kDefaultSramFile, sizeof(kDefaultSramFile));
			LOG0(Log::Warning, "SRAM: SRAM.DAT not found during reset, using defaults");
		} else {
			LOG0(Log::Normal, "SRAM: Persistence restored after reset");
		}
	}
#else
	// MFC: Use SRAM.DAT if exists, only initialize with default if not exists
	if (!fio.Load(sram_path, sram, sizeof(sram))) {
		memcpy(sram, kDefaultSramFile, sizeof(kDefaultSramFile));
		fio.Save(sram_path, (void *)kDefaultSramFile, sizeof(kDefaultSramFile));
	}
#endif

	// Endian convert
	for (i=0; i<sizeof(sram); i+=2) {
		data = sram[i];
		sram[i] = sram[i + 1];
		sram[i + 1] = data;
	}

	// Not changed
ASSERT(!changed);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::Cleanup()
{
	Fileio fio;
	int i;
	BYTE data;

ASSERT(this);

	// If changed
	if (changed) {
		// Endian convert
		for (i=0; i<sizeof(sram); i+=2) {
			data = sram[i];
			sram[i] = sram[i + 1];
			sram[i + 1] = data;
		}

		// Save
		fio.Save(sram_path, sram, sram_size << 10);
	}

	// Base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::Reset()
{
ASSERT(this);
ASSERT_DIAG();

	LOG0(Log::Normal, "Reset");

	// Write disable
	write_en = FALSE;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL SRAM::Save(Fileio *fio, int /*ver*/)
{
ASSERT(this);
ASSERT(fio);
ASSERT_DIAG();

	LOG0(Log::Normal, "Save");

	// SRAM size
	if (!fio->Write(&sram_size, sizeof(sram_size))) {
		return FALSE;
	}

	// SRAM data (up to 64KB)
	if (!fio->Write(&sram, sizeof(sram))) {
		return FALSE;
	}

	// Write enable flag
	if (!fio->Write(&write_en, sizeof(write_en))) {
		return FALSE;
	}

	// Memory sync flag
	if (!fio->Write(&mem_sync, sizeof(mem_sync))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SRAM::Load(Fileio *fio, int /*ver*/)
{
	BYTE *buf;

ASSERT(this);
ASSERT(fio);
ASSERT_DIAG();

	LOG0(Log::Normal, "Load");

	// Buffer allocation
	try {
		buf = new BYTE[sizeof(sram)];
	}
	catch (...) {
		buf = NULL;
	}
	if (!buf) {
		return FALSE;
	}

	// SRAM size
	if (!fio->Read(&sram_size, sizeof(sram_size))) {
		delete[] buf;
		return FALSE;
	}

	// SRAM data (up to 64KB)
	if (!fio->Read(buf, sizeof(sram))) {
		delete[] buf;
		return FALSE;
	}

	// Compare and copy
	if (memcmp(sram, buf, sizeof(sram)) != 0) {
		memcpy(sram, buf, sizeof(sram));
		changed = TRUE;
	}

	// Delete buffer
	delete[] buf;
	buf = NULL;

	// Write enable flag
	if (!fio->Read(&write_en, sizeof(write_en))) {
		return FALSE;
	}

	// Memory sync flag
	if (!fio->Read(&mem_sync, sizeof(mem_sync))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::ApplyCfg(const Config *config)
{
ASSERT(this);
ASSERT(config);
ASSERT_DIAG();

	LOG0(Log::Normal, "Apply configuration");

	// SRAM size
	if (config->sram_64k) {
		sram_size = 64;
#if defined(SRAM_LOG)
		LOG0(Log::Detail, "SRAM size 64KB");
#endif	// SRAM_LOG
	}
	else {
		sram_size = 16;
	}

	// RAM sync setting change
	mem_sync = config->ram_sramsync;
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::AssertDiag() const
{
	// Base class
	MemDevice::AssertDiag();

ASSERT(this);
ASSERT(GetID() == MAKEID('S', 'R', 'A', 'M'));
ASSERT(memdev.first == 0xed0000);
ASSERT(memdev.last == 0xedffff);
ASSERT((sram_size == 16) || (sram_size == 32) || (sram_size == 48) || (sram_size == 64));
ASSERT((write_en == TRUE) || (write_en == FALSE));
ASSERT((mem_sync == TRUE) || (mem_sync == FALSE));
ASSERT((changed == TRUE) || (changed == FALSE));
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SRAM::ReadByte(DWORD addr)
{
	DWORD size;

ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT_DIAG();

	// Offset calculate
	addr -= memdev.first;
	size = (DWORD)(sram_size << 10);

	// Size check
	if (size <= addr) {
		// Bus error
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xff;
	}

	// Wait
	scheduler->Wait(1);

	// Read (with endian convert)
	return (DWORD)sram[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SRAM::ReadWord(DWORD addr)
{
	DWORD size;

ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT((addr & 1) == 0);
ASSERT_DIAG();

	// Offset calculate
	addr -= memdev.first;
	size = (DWORD)(sram_size << 10);

	// Size check
	if (size <= addr) {
		// Bus error
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xff;
	}

	// Wait
	scheduler->Wait(1);

	// Read (with endian)
	return (DWORD)(*(WORD *)&sram[addr]);
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::WriteByte(DWORD addr, DWORD data)
{
	DWORD size;

ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT(data < 0x100);
ASSERT_DIAG();

	// Offset calculate
	addr -= memdev.first;
	size = (DWORD)(sram_size << 10);

	// Size check
	if (size <= addr) {
		// Bus error
		cpu->BusErr(memdev.first + addr, FALSE);
		return;
	}

	// Write enable check
	if (!write_en) {
		LOG1(Log::Warning, "Write disable $%06X", memdev.first + addr);
		return;
	}

	// Wait
	scheduler->Wait(1);

	// If address $09 is $00 or $10 written, update memory switch can be skipped
	// (Since Memory::Reset is called after reset, writing to this is ignored)
	if ((addr == 0x09) && (data == 0x10)) {
		if (cpu->GetPC() == 0xff03a8) {
			if (mem_sync) {
				LOG2(Log::Warning, "Memory switch change skip $%06X <- $%02X", memdev.first + addr, data);
				return;
			}
		}
	}

	// Write (with endian convert)
	if (addr < 0x100) {
		LOG2(Log::Detail, "Memory switch change $%06X <- $%02X", memdev.first + addr, data);
	}
	if (sram[addr ^ 1] != (BYTE)data) {
		sram[addr ^ 1] = (BYTE)data;
		changed = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::WriteWord(DWORD addr, DWORD data)
{
	DWORD size;

ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT((addr & 1) == 0);
ASSERT(data < 0x10000);
ASSERT_DIAG();

	// Offset calculate
	addr -= memdev.first;
	size = (DWORD)(sram_size << 10);

	// Size check
	if (size <= addr) {
		// Bus error
		cpu->BusErr(memdev.first + addr, FALSE);
		return;
	}

	// Write enable check
	if (!write_en) {
		LOG1(Log::Warning, "Write disable $%06X", memdev.first + addr);
		return;
	}

	// Wait
	scheduler->Wait(1);

	// Write (with endian)
	if (addr < 0x100) {
		LOG2(Log::Detail, "Memory switch change $%06X <- $%04X", memdev.first + addr, data);
	}
	if (*(WORD *)&sram[addr] != (WORD)data) {
		*(WORD *)&sram[addr] = (WORD)data;
		changed = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL SRAM::ReadOnly(DWORD addr) const
{
	DWORD size;

ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT_DIAG();

	// Offset calculate
	addr -= memdev.first;
	size = (DWORD)(sram_size << 10);

	// Size check
	if (size <= addr) {
		return 0xff;
	}

	// Read (with endian convert)
	return (DWORD)sram[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	Get SRAM address
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL SRAM::GetSRAM() const
{
ASSERT(this);
ASSERT_DIAG();

	return sram;
}

//---------------------------------------------------------------------------
//
//	Get SRAM size
//
//---------------------------------------------------------------------------
int FASTCALL SRAM::GetSize() const
{
ASSERT(this);
ASSERT_DIAG();

	return sram_size;
}

//---------------------------------------------------------------------------
//
//	Write enable
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::WriteEnable(BOOL enable)
{
ASSERT(this);
ASSERT_DIAG();

	write_en = enable;

	if (write_en) {
		LOG0(Log::Detail, "SRAM write enable");
	}
	else {
		LOG0(Log::Detail, "SRAM write disable");
	}
}

//---------------------------------------------------------------------------
//
//	Memory switch set
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::SetMemSw(DWORD offset, DWORD data)
{
ASSERT(this);
ASSERT(offset < 0x100);
ASSERT(data < 0x100);
ASSERT_DIAG();

	LOG2(Log::Detail, "Memory switch set $%06X <- $%02X", memdev.first + offset, data);
	if (sram[offset ^ 1] != (BYTE)data) {
		sram[offset ^ 1] = (BYTE)data;
		changed = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Memory switch get
//
//---------------------------------------------------------------------------
DWORD FASTCALL SRAM::GetMemSw(DWORD offset) const
{
ASSERT(this);
ASSERT(offset < 0x100);
ASSERT_DIAG();

	return (DWORD)sram[offset ^ 1];
}

//---------------------------------------------------------------------------
//
//	Boot counter update
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::UpdateBoot()
{
	WORD *ptr;

ASSERT(this);
ASSERT_DIAG();

	// Mark changed
	changed = TRUE;

	// Pointer set ($ED0044)
	ptr = (WORD *)&sram[0x0044];

	// Increment counter (Low)
	if (ptr[1] != 0xffff) {
		ptr[1] = ptr[1] + 1;
		return;
	}

	// Increment counter (High)
	ptr[1] = 0x0000;
	ptr[0] = ptr[0] + 1;
}