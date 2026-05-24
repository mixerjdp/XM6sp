//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ System Port ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "sram.h"
#include "keyboard.h"
#include "cpu.h"
#include "crtc.h"
#include "render.h"
#include "memory.h"
#include "schedule.h"
#include "fileio.h"
#include "sysport.h"

//===========================================================================
//
//	System Port
//
//===========================================================================
//#define SYSPORT_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SysPort::SysPort(VM *p) : MemDevice(p)
{
	// Device ID initialization
	dev.id = MAKEID('S', 'Y', 'S', 'P');
	dev.desc = "System (MESSIAH)";

	// Start address, end address
	memdev.first = 0xe8e000;
	memdev.last = 0xe8ffff;

	// All clear
	memset(&sysport, 0, sizeof(sysport));

	// Objects
	memory = NULL;
	sram = NULL;
	keyboard = NULL;
	crtc = NULL;
	render = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL SysPort::Init()
{
ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Memory get
ASSERT(!memory);
	memory = (Memory*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
ASSERT(memory);

	// SRAM get
ASSERT(!sram);
	sram = (SRAM*)vm->SearchDevice(MAKEID('S', 'R', 'A', 'M'));
ASSERT(sram);

	// Keyboard get
ASSERT(!keyboard);
	keyboard = (Keyboard*)vm->SearchDevice(MAKEID('K', 'E', 'Y', 'B'));
ASSERT(keyboard);

	// CRTC get
ASSERT(!crtc);
	crtc = (CRTC*)vm->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
ASSERT(crtc);

	// Render get
ASSERT(!render);
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
ASSERT(render);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL SysPort::Cleanup()
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
void FASTCALL SysPort::Reset()
{
ASSERT(this);
ASSERT_DIAG();

	LOG0(Log::Normal, "Reset");

	// Setting value reset
	sysport.contrast = 0;
	sysport.scope_3d = 0;
	sysport.image_unit = 0;
	sysport.power_count = 0;
	sysport.ver_count = 0;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL SysPort::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

ASSERT(this);
ASSERT(fio);
ASSERT_DIAG();

	LOG0(Log::Normal, "Save");

	// Size save
	sz = sizeof(sysport_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Data save
	if (!fio->Write(&sysport, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SysPort::Load(Fileio *fio, int /*ver*/)
{
	size_t sz;

ASSERT(this);
ASSERT(fio);
ASSERT_DIAG();

	LOG0(Log::Normal, "Load");

	// Size load, compare
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(sysport_t)) {
		return FALSE;
	}

	// Data load
	if (!fio->Read(&sysport, (int)sz)) {
		return FALSE;
	}

	// Render notify
	render->SetContrast(sysport.contrast);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL SysPort::ApplyCfg(const Config* /*config*/)
{
ASSERT(this);
ASSERT_DIAG();

	LOG0(Log::Normal, "Apply configuration");
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void FASTCALL SysPort::AssertDiag() const
{
	// Base class
	MemDevice::AssertDiag();

ASSERT(this);
ASSERT(GetID() == MAKEID('S', 'Y', 'S', 'P'));
ASSERT(memdev.first == 0xe8e000);
ASSERT(memdev.last == 0xe8ffff);
ASSERT(memory);
ASSERT(memory->GetID() == MAKEID('M', 'E', 'M', ' '));
ASSERT(sram);
ASSERT(sram->GetID() == MAKEID('S', 'R', 'A', 'M'));
ASSERT(keyboard);
ASSERT(keyboard->GetID() == MAKEID('K', 'E', 'Y', 'B'));
ASSERT(crtc);
ASSERT(crtc->GetID() == MAKEID('C', 'R', 'T', 'C'));
ASSERT(sysport.contrast <= 0x0f);
ASSERT(sysport.scope_3d <= 0x03);
ASSERT(sysport.image_unit <= 0x1f);
ASSERT(sysport.power_count <= 0x03);
ASSERT(sysport.ver_count <= 0x03);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SysPort::ReadByte(DWORD addr)
{
	DWORD data;

ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT_DIAG();

	// 4bit only decode
	addr &= 0x0f;

	// Even byte only decode
	if ((addr & 1) == 0) {
		return 0xff;
	}
	addr >>= 1;

	// Wait
	scheduler->Wait(1);

	switch (addr) {
		// Contrast
		case 0:
			data = 0xf0;
			data |= sysport.contrast;
			return data;

		// Baud rate receive and 3D shift
		case 1:
			data = 0xf8;
			data |= sysport.scope_3d;
			return data;

		// Image unit select
		case 2:
			return 0xff;

		// Keyboard connect ENMI reset and HRL
		case 3:
			data = 0xf0;
			if (keyboard->IsConnect()) {
				data |= 0x08;
			}
			if (crtc->GetHRL()) {
				data |= 0x02;
			}
			return data;

		// ROM/DRAM wait (X68030)
		case 4:
			return 0xff;

		// MPU mode selection
		case 5:
			switch (memory->GetMemType()) {
				// SASI/SCSI
				case Memory::SASI:
				case Memory::SCSIInt:
				case Memory::SCSIExt:
					// SASI port so return 0xff
					return 0xff;

				// XVI/Compact
				case Memory::XVI:
				case Memory::Compact:
					// Real machine, so 4bit assign to clock
					// 1111:10MHz
					// 1110:16MHz
					// 1101:20MHz
					// 1100:25MHz
					// 1011:33MHz
					// 1010:40MHz
					// 1001:50MHz
					return 0xfe;

				// X68030
				case Memory::X68030:
					// Real, so 4bit assign to MPU mode
					// 1111:68000
					// 1110:68020
					// 1101:68030
					// 1100:68040
					return 0xdc;

				// Others (invalid)
				default:
					ASSERT(FALSE);
					break;
			}
			return 0xff;

		// SRAM write enable
		case 6:
			// Version register (XM6 original)
			if (sysport.ver_count != 0) {
				return GetVR();
			}
			return 0xff;

		// POWER monitor
		case 7:
			return 0xff;
	}

	// Normal, other does not exist
ASSERT(FALSE);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SysPort::ReadWord(DWORD addr)
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
void FASTCALL SysPort::WriteByte(DWORD addr, DWORD data)
{
ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT(data < 0x100);
ASSERT_DIAG();

	// 4bit only decode
	addr &= 0x0f;

	// Even byte only decode
	if ((addr & 1) == 0) {
		return;
	}
	addr >>= 1;

	// Wait
	scheduler->Wait(1);

	switch (addr) {
		// Contrast
		case 0:
			data &= 0x0f;
			if (sysport.contrast != data) {
#if defined(SYSPORT_LOG)
				LOG1(Log::Normal, "Contrast setting %d", data);
#endif	// SYSPORT_LOG

				// Changed setting
				sysport.contrast = data;
				render->SetContrast(data);
			}
			return;

		// Baud rate and 3D shift
		case 1:
			data &= 3;
			if (sysport.scope_3d != data) {
#if defined(SYSPORT_LOG)
				LOG1(Log::Normal, "3D shift setting %d", data);
#endif	// SYSPORT_LOG

				// Changed setting
				sysport.scope_3d = data;
			}
			return;

		// Keyboard connect ENMI reset and HRL
		case 3:
			if (data & 0x08) {
#if defined(SYSPORT_LOG)
				LOG0(Log::Normal, "Keyboard connect");
#endif	// SYSPORT_LOG
				keyboard->SendWait(FALSE);
			}
			else {
#if defined(SYSPORT_LOG)
				LOG0(Log::Normal, "Keyboard disconnect");
#endif	// SYSPORT_LOG
				keyboard->SendWait(TRUE);
			}
			if (data & 0x02) {
#if defined(SYSPORT_LOG)
				LOG0(Log::Normal, "HRL=1");
#endif	// SYSPORT_LOG
				crtc->SetHRL(TRUE);
			}
			else {
#if defined(SYSPORT_LOG)
				LOG0(Log::Normal, "HRL=0");
#endif	// SYSPORT_LOG
				crtc->SetHRL(FALSE);
			}
			return;

		// Image unit select
		case 4:
#if defined(SYSPORT_LOG)
			LOG1(Log::Normal, "Image unit select $%02X", data & 0x1f);
#endif	// SYSPORT_LOG
			sysport.image_unit = data & 0x1f;
			return;

		// ROM/DRAM wait
		case 5:
			return;

		// SRAM write enable
		case 6:
			// SRAM write enable
			if (data == 0x31) {
				sram->WriteEnable(TRUE);
			}
			else {
				sram->WriteEnable(FALSE);
			}
			// Version register (XM6 original)
			if (data == 'X') {
#if defined(SYSPORT_LOG)
				LOG0(Log::Normal, "XM6 version read start");
#endif	// SYSPORT_LOG
				sysport.ver_count = 1;
			}
			else {
				sysport.ver_count = 0;
			}
			return;

		// Power save (00, 0F, 0F sequence to finish)
		case 7:
			data &= 0x0f;
			switch (sysport.power_count) {
				// Access none
				case 0:
					if (data == 0x00) {
						sysport.power_count++;
					}
					else {
						sysport.power_count = 0;
					}
					break;

				// 00 received
				case 1:
					if (data == 0x0f) {
						sysport.power_count++;
					}
					else {
						sysport.power_count = 0;
					}
					break;

				// 00,0F received
				case 2:
					if (data == 0x0f) {
						sysport.power_count++;
						LOG0(Log::Normal, "Power OFF");
						vm->SetPower(FALSE);
					}
					else {
						sysport.power_count = 0;
					}
					break;

				// 00,0F,0F
				default:
					break;
			}
			return;

		default:
			break;
	}

	// Normal, other does not exist
ASSERT(FALSE);
	return;
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL SysPort::WriteWord(DWORD addr, DWORD data)
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
DWORD FASTCALL SysPort::ReadOnly(DWORD addr) const
{
	DWORD data;

ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT_DIAG();

	// 4bit only decode
	addr &= 0x0f;

	// Even byte only decode
	if ((addr & 1) == 0) {
		return 0xff;
	}

	switch (addr) {
		// Contrast
		case 0:
			data = 0xf0;
			data |= sysport.contrast;
			return data;

		// Baud rate receive and 3D shift
		case 1:
			data = 0xf8;
			data |= sysport.scope_3d;
			return data;

		// Image unit select
		case 2:
			return 0xff;

		// Keyboard connect ENMI reset and HRL
		case 3:
			data = 0xf0;
			if (keyboard->IsConnect()) {
				data |= 0x08;
			}
			if (crtc->GetHRL()) {
				data |= 0x02;
			}
			return data;

		// ROM/DRAM wait (X68030)
		case 4:
			return 0xff;

		// MPU mode selection
		case 5:
			switch (memory->GetMemType()) {
				// SASI/SCSI
				case Memory::SASI:
				case Memory::SCSIInt:
				case Memory::SCSIExt:
					// SASI port so return 0xff
					return 0xff;

				// XVI/Compact
				case Memory::XVI:
				case Memory::Compact:
					// Real machine, so 4bit assign to clock
					// 1111:10MHz
					// 1110:16MHz
					// 1101:20MHz
					// 1100:25MHz
					// 1011:33MHz
					// 1010:40MHz
					// 1001:50MHz
					return 0xfe;

				// X68030
				case Memory::X68030:
					// Real, so 4bit assign to MPU mode
					// 1111:68000
					// 1110:68020
					// 1101:68030
					// 1100:68040
					return 0xdc;

				// Others (invalid)
				default:
					ASSERT(FALSE);
					break;
			}
			return 0xff;

		// SRAM write enable
		case 6:
			return 0xff;

		// POWER monitor
		case 7:
			return 0xff;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Version register read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SysPort::GetVR()
{
	DWORD major;
	DWORD minor;

ASSERT(this);
ASSERT_DIAG();

	switch (sysport.ver_count) {
		// 'X' write enable
		case 1:
			sysport.ver_count++;
			return '6';

		// Major version
		case 2:
			vm->GetVersion(major, minor);
			sysport.ver_count++;
			return major;

		// Minor version
		case 3:
			vm->GetVersion(major, minor);
			sysport.ver_count = 0;
			return minor;

		// Others (invalid)
		default:
			ASSERT(FALSE);
			break;
	}

	return 0xff;
}