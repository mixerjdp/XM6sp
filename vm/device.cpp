//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
//	[ Device ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "device.h"

//===========================================================================
//
//	Device
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Device::Device(VM *p)
{
	// Link structure
	dev.next = NULL;
	dev.id = 0;
	dev.desc = NULL;

	// Link to VM and add device
	vm = p;
	vm->AddDevice(this);
	log = &(vm->log);
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
Device::~Device()
{
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL Device::Init()
{
	ASSERT(this);

	// Do nothing, leave memory and file handling undone
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL Device::Cleanup()
{
	ASSERT(this);

	// Delete from VM
	ASSERT(vm);
	vm->DelDevice(this);

	// Delete myself
	delete this;
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Device::Reset()
{
	ASSERT(this);
	ASSERT_DIAG();
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Device::Save(Fileio* /*fio*/, int /*ver*/)
{
	ASSERT(this);
	ASSERT_DIAG();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Device::Load(Fileio* /*fio*/, int /*ver*/)
{
	ASSERT(this);
	ASSERT_DIAG();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL Device::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);
	ASSERT_DIAG();
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void FASTCALL Device::AssertDiag() const
{
	ASSERT(this);
	ASSERT(dev.id != 0);
	ASSERT(dev.desc);
	ASSERT(vm);
	ASSERT(log);
	ASSERT(log == &(vm->log));
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL Device::Callback(Event* /*ev*/)
{
	ASSERT(this);
	ASSERT_DIAG();

	return TRUE;
}

//===========================================================================
//
//	Memory mapped device
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
MemDevice::MemDevice(VM *p) : Device(p)
{
	// Set dummy address
	memdev.first = 0;
	memdev.last = 0;

	// Initialize pointers
	cpu = NULL;
	scheduler = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL MemDevice::Init()
{
	ASSERT(this);
	ASSERT((memdev.first != 0) || (memdev.last != 0));
	ASSERT(memdev.first <= 0xffffff);
	ASSERT(memdev.last <= 0xffffff);
	ASSERT(memdev.first <= memdev.last);

	// Run base class device initialization
	if (!Device::Init()) {
		return FALSE;
	}

	// Get CPU and scheduler
	cpu = (CPU*)vm->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(cpu);
	scheduler = (Scheduler*)vm->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(scheduler);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL MemDevice::ReadByte(DWORD /*addr*/)
{
	ASSERT(this);
	ASSERT_DIAG();

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL MemDevice::ReadWord(DWORD addr)
{
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT_DIAG();

	// Low then high byte read
	data = ReadByte(addr);
	data <<= 8;
	data |= ReadByte(addr + 1);

	return data;
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL MemDevice::WriteByte(DWORD /*addr*/, DWORD /*data*/)
{
	ASSERT(this);
	ASSERT_DIAG();
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL MemDevice::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT_DIAG();

	// Low then high byte write
	WriteByte(addr, (BYTE)(data >> 8));
	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL MemDevice::ReadOnly(DWORD /*addr*/) const
{
	ASSERT(this);
	ASSERT_DIAG();

	return 0xff;
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void FASTCALL MemDevice::AssertDiag() const
{
	// Base class
	Device::AssertDiag();

	ASSERT(this);
	ASSERT((memdev.first != 0) || (memdev.last != 0));
	ASSERT(memdev.first <= 0xffffff);
	ASSERT(memdev.last <= 0xffffff);
	ASSERT(memdev.first <= memdev.last);
	ASSERT(cpu);
	ASSERT(cpu->GetID() == MAKEID('C', 'P', 'U', ' '));
	ASSERT(scheduler);
	ASSERT(scheduler->GetID() == MAKEID('S', 'C', 'H', 'E'));
}
#endif	// NDEBUG
