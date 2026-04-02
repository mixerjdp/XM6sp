//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ Memory ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "filepath.h"
#include "fileio.h"
#include "cpu.h"
#include "areaset.h"
#include "gvram.h"
#include "tvram.h"
#include "sram.h"
#include "config.h"
#include "core_asm.h"
#include "memory.h"

//---------------------------------------------------------------------------
//
//	Startup work
//
//---------------------------------------------------------------------------
static CPU *pCPU;
static Memory *pMemory;

//---------------------------------------------------------------------------
//
//	Bus error handler (memory only)
//
//---------------------------------------------------------------------------
extern "C" {

//---------------------------------------------------------------------------
//
//	Read bus error
//
//---------------------------------------------------------------------------
void ReadBusErr(DWORD addr)
{
	pCPU->BusErr(addr, TRUE);
}

//---------------------------------------------------------------------------
//
//	Write bus error
//
//---------------------------------------------------------------------------
void WriteBusErr(DWORD addr)
{
	pCPU->BusErr(addr, FALSE);
}
}

//===========================================================================
//
//	Memory
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Memory::Memory(VM *p) : MemDevice(p)
{
	// Device ID creation
	dev.id = MAKEID('M', 'E', 'M', ' ');
	dev.desc = "Memory Ctrl (OHM2)";

	// Start address, I/O address
	memdev.first = 0;
	memdev.last = 0xffffff;

	// RAM/ROM buffer
	mem.ram = NULL;
	mem.ipl = NULL;
	mem.cg = NULL;
	mem.scsi = NULL;

	// RAM is 2MB
	mem.size = 2;
	mem.config = 0;
	mem.length = 0;

	// Memory type not ready
	mem.type = None;
	mem.now = None;

	// Others
	areaset = NULL;
	sram = NULL;

	// Others
	memset(mem.table, 0, sizeof(mem.table));
	mem.memsw = TRUE;

	// Static work
	::pCPU = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::Init()
{
 ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Main memory
	mem.length = mem.size * 0x100000;
	try {
		mem.ram = new BYTE[ mem.length ];
	}
	catch (...) {
		return FALSE;
	}
	if (!mem.ram) {
		return FALSE;
	}

	// Clear main memory to zero
	memset(mem.ram, 0x00, mem.length);

	// IPL ROM
	try {
		mem.ipl = new BYTE[ 0x20000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!mem.ipl) {
		return FALSE;
	}

	// CG ROM
	try {
		mem.cg = new BYTE[ 0xc0000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!mem.cg) {
		return FALSE;
	}

	// SCSI ROM
	try {
		mem.scsi = new BYTE[ 0x20000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!mem.scsi) {
		return FALSE;
	}

	// SASI ROM is not required, so load anyway
	if (!LoadROM(SASI)) {
		// IPLROM.DAT, CGROM.DAT not found
		return FALSE;
	}

	// If other ROM exists, XV/Compact/030 compact is searched
	if (LoadROM(XVI)) {
		mem.now = XVI;
	}
	if (mem.type == None) {
		if (LoadROM(Compact)) {
			mem.now = Compact;
		}
	}
	if (mem.type == None) {
		if (LoadROM(X68030)) {
			mem.now = X68030;
		}
	}

	// If XVI,Compact,030 not exist, search SASI
	if (mem.type == None) {
		LoadROM(SASI);
		mem.now = SASI;
	}

	// Memory version
	// NOTE: Musashi doesn't use Starscream-style memory regions.
	// Memory access is handled via m68k_read/write_memory callbacks.
	// Region pointers are kept in s68000context as stubs for compilation.
	// ::s68000context.u_fetch = u_pgr;
	// ::s68000context.s_fetch = s_pgr;
	// ::s68000context.u_readbyte = u_rbr;
	// ::s68000context.s_readbyte = s_rbr;
	// ::s68000context.u_readword = u_rwr;
	// ::s68000context.s_readword = s_rwr;
	// ::s68000context.u_writebyte = u_wbr;
	// ::s68000context.s_writebyte = s_wbr;
	// ::s68000context.u_writeword = u_wwr;
	// ::s68000context.s_writeword = s_wwr;

	// 繧ｨ繝ｪ繧｢繧ｻ繝・ヨ蜿門ｾ・
	areaset = (AreaSet*)vm->SearchDevice(MAKEID('A', 'R', 'E', 'A'));
 ASSERT(areaset);

	// SRAM get
	sram = (SRAM*)vm->SearchDevice(MAKEID('S', 'R', 'A', 'M'));
 ASSERT(sram);

	// Static work
	::pCPU = cpu;
	::pMemory = this;

	// Initialize translation table
	InitTable();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ROM load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::LoadROM(memtype target)
{
	Filepath path;
	Fileio fio;
	int i;
	BYTE data;
	BYTE *ptr;
	BOOL scsi_req;
	int scsi_size;

 ASSERT(this);

	// All ROM areas clear
	memset(mem.ipl, 0xff, 0x20000);
	memset(mem.cg, 0xff, 0xc0000);
	memset(mem.scsi, 0xff, 0x20000);
	mem.type = None;

	// IPL
	switch (target) {
		case SASI:
		case SCSIInt:
		case SCSIExt:
			path.SysFile(Filepath::IPL);
			break;
		case XVI:
			path.SysFile(Filepath::IPLXVI);
			break;
		case Compact:
			path.SysFile(Filepath::IPLCompact);
			break;
		case X68030:
			path.SysFile(Filepath::IPL030);
			break;
		default:
		 ASSERT(FALSE);
			return FALSE;
	}
	if (!fio.Load(path, mem.ipl, 0x20000)) {
		return FALSE;
	}

	// IPL byte swap
	ptr = mem.ipl;
	for (i=0; i<0x10000; i++) {
		data = ptr[0];
		ptr[0] = ptr[1];
		ptr[1] = data;
		ptr += 2;
	}

	// CG
	path.SysFile(Filepath::CG);
	if (!fio.Load(path, mem.cg, 0xc0000)) {
		// If file not found, CGTMP
		path.SysFile(Filepath::CGTMP);
		if (!fio.Load(path, mem.cg, 0xc0000)) {
			return FALSE;
		}
	}

	// CG byte swap
	ptr = mem.cg;
	for (i=0; i<0x60000; i++) {
		data = ptr[0];
		ptr[0] = ptr[1];
		ptr[1] = data;
		ptr += 2;
	}

	// SCSI
	scsi_req = FALSE;
	switch (target) {
		// Internal
		case SCSIInt:
		case XVI:
		case Compact:
			path.SysFile(Filepath::SCSIInt);
			scsi_req = TRUE;
			break;
		case X68030:
			path.SysFile(Filepath::ROM030);
			scsi_req = TRUE;
			break;
		// External
		case SCSIExt:
			path.SysFile(Filepath::SCSIExt);
			scsi_req = TRUE;
			break;
		// SASI(ROM not required)
		case SASI:
			break;
		// Others
		default:
		 ASSERT(FALSE);
			break;
	}
	if (scsi_req) {
		// Only X68030 ROM30.DAT(0x20000 bytes), others 0x2000 bytes loaded
		if (target == X68030) {
			scsi_size = 0x20000;
		}
		else {
			scsi_size = 0x2000;
		}

		// Set mount point
		ptr = mem.scsi;

		// Load
		if (!fio.Load(path, mem.scsi, scsi_size)) {
			// SCSIExt is 0x1fe0 bytes, swap with WinX68k compatibility
			if (target != SCSIExt) {
				return FALSE;
			}

			// Load at 0x1fe0 bytes offset
			scsi_size = 0x1fe0;
			ptr = &mem.scsi[0x20];
			if (!fio.Load(path, &mem.scsi[0x0020], scsi_size)) {
				return FALSE;
			}
		}

		// SCSI byte swap
		for (i=0; i<scsi_size; i+=2) {
			data = ptr[0];
			ptr[0] = ptr[1];
			ptr[1] = data;
			ptr += 2;
		}
	}

	// Set to target
	mem.type = target;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Initialize translation table
//	: Specific to memory controller
//
//---------------------------------------------------------------------------
void FASTCALL Memory::InitTable()
{
#if defined(_WIN32)
#pragma pack(push, 1)
#endif	// _WIN32
	MemDevice* devarray[0x40];
#if defined(_WIN32)
#pragma pack(pop)
#endif	// _WIN32

	MemDevice *mdev;
	uintptr_t *table;
	int i;

 ASSERT(this);

	// Initialize mount point
	mdev = this;
	i = 0;

	// Traverse all devices, add to array
	while (mdev) {
		devarray[i] = mdev;

		// Next
		i++;
		mdev = (MemDevice*)mdev->GetNextDevice();
	}

	// Call decode, get translation table
	MemInitDecode(this, devarray);

	// Return decode table to internal array
	table = MemDecodeTable;
	for (i=0; i<0x180; i++) {
		mem.table[i] = reinterpret_cast<MemDevice*>(table[i]);
	}
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL Memory::Cleanup()
{
 ASSERT(this);

	// Memory release
	if (mem.ram) {
		delete[] mem.ram;
		mem.ram = NULL;
	}
	if (mem.ipl) {
		delete[] mem.ipl;
		mem.ipl = NULL;
	}
	if (mem.cg) {
		delete[] mem.cg;
		mem.cg = NULL;
	}
	if (mem.scsi) {
		delete[] mem.scsi;
		mem.scsi = NULL;
	}

	// To base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Memory::Reset()
{
	int size;

 ASSERT(this);
	LOG0(Log::Normal, "(message)");

	// If memory type changed
	if (mem.type != mem.now) {
		if (LoadROM(mem.type)) {
			// ROM exists, loaded
			mem.now = mem.type;
		}
		else {
			// ROM not exist, change to SASI
			LoadROM(SASI);
			mem.now = SASI;
			mem.type = SASI;
		}

		// Context created (CPU::Reset not called, so returns FALSE)
		MakeContext(FALSE);
	}

	// If memory size changed
	if (mem.size == ((mem.config + 1) * 2)) {
		// If changed, auto update memory switch
		if (mem.memsw) {
			// $ED0008 : Main RAM size
			size = mem.size << 4;
			sram->SetMemSw(0x08, 0x00);
			sram->SetMemSw(0x09, size);
			sram->SetMemSw(0x0a, 0x00);
			sram->SetMemSw(0x0b, 0x00);
		}
		return;
	}

	// Change
	mem.size = (mem.config + 1) * 2;

	// Reallocate
 ASSERT(mem.ram);
	delete[] mem.ram;
	mem.ram = NULL;
	mem.length = mem.size * 0x100000;
	try {
		mem.ram = new BYTE[ mem.length ];
	}
	catch (...) {
		// If memory allocation fail, fix to 2MB
		mem.config = 0;
		mem.size = 2;
		mem.length = mem.size * 0x100000;
		mem.ram = new BYTE[ mem.length ];
	}
	if (!mem.ram) {
		// If memory allocation fail, fix to 2MB
		mem.config = 0;
		mem.size = 2;
		mem.length = mem.size * 0x100000;
		mem.ram = new BYTE[ mem.length ];
	}

	// If memory allocated, clear
	if (mem.ram) {
		memset(mem.ram, 0x00, mem.length);

		// Context created (CPU::Reset not called, so returns FALSE)
		MakeContext(FALSE);
	}

	// Memory switch auto update
	if (mem.memsw) {
		// $ED0008 : Main RAM size
		size = mem.size << 4;
		sram->SetMemSw(0x08, 0x00);
		sram->SetMemSw(0x09, size);
		sram->SetMemSw(0x0a, 0x00);
		sram->SetMemSw(0x0b, 0x00);
	}
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::Save(Fileio *fio, int /*ver*/)
{
 ASSERT(this);


	// Write type
	if (!fio->Write(&mem.now, sizeof(mem.now))) {
		return FALSE;
	}

	// Write SCSI ROM (X68030 only)
	if (mem.now != X68030) {
		if (!fio->Write(mem.scsi, 0x2000)) {
			return FALSE;
		}
	}

	// Write mem.size
	if (!fio->Write(&mem.size, sizeof(mem.size))) {
		return FALSE;
	}

	// Write memory
	if (!fio->Write(mem.ram, mem.length)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::Load(Fileio *fio, int /*ver*/)
{
	int size;
	BOOL context;

 ASSERT(this);
	LOG0(Log::Normal, "Load");

	// Context not created
	context = FALSE;

	// Read type
	if (!fio->Read(&mem.type, sizeof(mem.type))) {
		return FALSE;
	}

	// If type different from current
	if (mem.type != mem.now) {
		// Read ROM
		if (!LoadROM(mem.type)) {
			// If ROM not exist at save time, load current
			LoadROM(mem.now);
			return FALSE;
		}

		// Success
		mem.now = mem.type;
		context = TRUE;
	}

	// Read SCSI ROM (X68030 only)
	if (mem.type != X68030) {
		if (!fio->Read(mem.scsi, 0x2000)) {
			return FALSE;
		}
	}

	// Read mem.size
	if (!fio->Read(&size, sizeof(size))) {
		return FALSE;
	}

	// If mem.size different
	if (mem.size != size) {
		// Change
		mem.size = size;

		// Reallocate
		delete[] mem.ram;
		mem.ram = NULL;
		mem.length = mem.size * 0x100000;
		try {
			mem.ram = new BYTE[ mem.length ];
		}
		catch (...) {
			mem.ram = NULL;
		}
		if (!mem.ram) {
			// If memory allocation fail, fix to 2MB
			mem.config = 0;
			mem.size = 2;
			mem.length = mem.size * 0x100000;
			mem.ram = new BYTE[ mem.length ];

			// Load error
			return FALSE;
		}

		// Context creation required
		context = TRUE;
	}

	// Read memory
	if (!fio->Read(mem.ram, mem.length)) {
		return FALSE;
	}

	// If required, create context
	if (context) {
		MakeContext(FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL Memory::ApplyCfg(const Config *config)
{
 ASSERT(this);
 ASSERT(config);
	LOG0(Log::Normal, "Apply config");

	// Memory type (ROM load after)
	mem.type = (memtype)config->mem_type;

	// RAM size (memory confirm after)
	mem.config = config->ram_size;
 ASSERT((mem.config >= 0) && (mem.config <= 5));

	// Memory switch auto update
	mem.memsw = config->ram_sramsync;
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL Memory::ReadByte(DWORD addr)
{
	DWORD index;

 ASSERT(this);
 ASSERT(addr <= 0xffffff);
 ASSERT(mem.now != None);

	// IPL ROM shadowed at 0x000000 during reset vector fetch ONLY.
	// In the original Starscream implementation, MakeContext(TRUE) set up
	// a temporary program region mapping 0x0000-0xFFFF to IPL ROM,
	// and MakeContext(FALSE) replaced it with normal RAM mapping.
	// We emulate this by only activating the shadow during m68k_pulse_reset().
	if (addr < 0x10000 && musashi_is_resetting) {
		return (DWORD)mem.ipl[(addr + 0x10000) ^ 1];
	}

	// Main RAM
	if (addr < mem.length) {
		return (DWORD)mem.ram[addr ^ 1];
	}

	// IPL
	if (addr >= 0xfe0000) {
		addr &= 0x1ffff;
		addr ^= 1;
		return (DWORD)mem.ipl[addr];
	}

	// IPL or SCSI internal
	if (addr >= 0xfc0000) {
		// IPL area
		if ((mem.now == SASI) || (mem.now == SCSIExt)) {
			// IPL area
			addr &= 0x1ffff;
			addr ^= 1;
			return (DWORD)mem.ipl[addr];
		}
		// SCSI internal area (address check)
		if (addr < 0xfc2000) {
			// SCSI internal
			addr &= 0x1fff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
		// X68030 IPL area
		if (mem.now == X68030) {
			// X68030 IPL area
			addr &= 0x1ffff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
		// SCSI internal ROM not exist
		return 0xff;
	}

	// CG
	if (addr >= 0xf00000) {
		addr &= 0xfffff;
		addr ^= 1;
		return (DWORD)mem.cg[addr];
	}

	// SCSI external
	if (mem.now == SCSIExt) {
		if ((addr >= 0xea0020) && (addr <= 0xea1fff)) {
			addr &= 0x1fff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
	}

	// Device access
	if (addr >= 0xc00000) {
		index = addr - 0xc00000;
		index >>= 13;
	 ASSERT(index < 0x180);
		if (mem.table[index] != (MemDevice*)this) {
			return mem.table[index]->ReadByte(addr);
		}
	}

	LOG1(Log::Warning, "(message)", addr);
	cpu->BusErr(addr, TRUE);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL Memory::ReadWord(DWORD addr)
{
	DWORD data;
	DWORD index;
	WORD *ptr;

 ASSERT(this);
 ASSERT(addr <= 0xffffff);
 ASSERT(mem.now != None);

	// If from CPU, address error occurs from CPU, if from DMA, check not needed
	if (addr & 1) {
		// Pass to CPU (CPU exception, not DMA)
		cpu->AddrErr(addr, TRUE);
		return 0xffff;
	}

	// IPL ROM shadowed at 0x000000 during reset vector fetch ONLY.
	if (addr < 0x10000 && musashi_is_resetting) {
		ptr = (WORD*)(&mem.ipl[addr + 0x10000]);
		data = (DWORD)*ptr;
		return data;
	}

	// Main RAM
	if (addr < mem.length) {
		ptr = (WORD*)(&mem.ram[addr]);
		data = (DWORD)*ptr;
		return data;
	}

	// IPL
	if (addr >= 0xfe0000) {
		addr &= 0x1ffff;
		ptr = (WORD*)(&mem.ipl[addr]);
		data = (DWORD)*ptr;
		return data;
	}

	// IPL or SCSI internal
	if (addr >= 0xfc0000) {
		// IPL area
		if ((mem.now == SASI) || (mem.now == SCSIExt)) {
			// IPL area
			addr &= 0x1ffff;
			ptr = (WORD*)(&mem.ipl[addr]);
			data = (DWORD)*ptr;
			return data;
		}
		// SCSI internal area (address check)
		if (addr < 0xfc2000) {
			// SCSI internal
			addr &= 0x1fff;
			ptr = (WORD*)(&mem.scsi[addr]);
			data = (DWORD)*ptr;
			return data;
		}
		// X68030 IPL area
		if (mem.now == X68030) {
			// X68030 IPL area
			addr &= 0x1ffff;
			ptr = (WORD*)(&mem.scsi[addr]);
			data = (DWORD)*ptr;
			return data;
		}
		// SCSI internal ROM not exist
		return 0xffff;
	}

	// CG
	if (addr >= 0xf00000) {
		addr &= 0xfffff;
		ptr = (WORD*)(&mem.cg[addr]);
		data = (DWORD)*ptr;
		return data;
	}

	// SCSI external
	if (mem.now == SCSIExt) {
		if ((addr >= 0xea0020) && (addr <= 0xea1fff)) {
			addr &= 0x1fff;
			ptr = (WORD*)(&mem.scsi[addr]);
			data = (DWORD)*ptr;
			return data;
		}
	}

	// Device access
	if (addr >= 0xc00000) {
		index = addr - 0xc00000;
		index >>= 13;
	 ASSERT(index < 0x180);
		if (mem.table[index] != (MemDevice*)this) {
			return mem.table[index]->ReadWord(addr);
		}
	}

	// Bus error
	LOG1(Log::Warning, "(message)", addr);
	cpu->BusErr(addr, TRUE);
	return 0xffff;
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL Memory::WriteByte(DWORD addr, DWORD data)
{
	DWORD index;

 ASSERT(this);
 ASSERT(addr <= 0xffffff);
 ASSERT(data < 0x100);
 ASSERT(mem.now != None);

	// Main RAM
	if (addr < mem.length) {
		mem.ram[addr ^ 1] = (BYTE)data;
		return;
	}

	// IPL,SCSI,CG
	if (addr >= 0xf00000) {
		return;
	}

	// Device access
	if (addr >= 0xc00000) {
		index = addr - 0xc00000;
		index >>= 13;
	 ASSERT(index < 0x180);
		if (mem.table[index] != (MemDevice*)this) {
			mem.table[index]->WriteByte(addr, data);
			return;
		}
	}

	// Bus error
	cpu->BusErr(addr, FALSE);
	LOG2(Log::Warning, "(message)", addr, data);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL Memory::WriteWord(DWORD addr, DWORD data)
{
	WORD *ptr;
	DWORD index;

 ASSERT(this);
 ASSERT(addr <= 0xffffff);
 ASSERT(data < 0x10000);
 ASSERT(mem.now != None);

	// If from CPU, address error occurs from CPU, if from DMA, check not needed
	if (addr & 1) {
		// Pass to CPU (CPU exception, not DMA)
		cpu->AddrErr(addr, FALSE);
		return;
	}

	// Main RAM
	if (addr < mem.length) {
		ptr = (WORD*)(&mem.ram[addr]);
		*ptr = (WORD)data;
		return;
	}

	// IPL,SCSI,CG
	if (addr >= 0xf00000) {
		return;
	}

	// Device access
	if (addr >= 0xc00000) {
		index = addr - 0xc00000;
		index >>= 13;
	 ASSERT(index < 0x180);
		if (mem.table[index] != (MemDevice*)this) {
			mem.table[index]->WriteWord(addr, data);
			return;
		}
	}

	// Bus error
	cpu->BusErr(addr, FALSE);
	LOG2(Log::Warning, "(message)", addr, data);
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL Memory::ReadOnly(DWORD addr) const
{
	DWORD index;

 ASSERT(this);
 ASSERT(addr <= 0xffffff);
 ASSERT(mem.now != None);

	// IPL ROM shadowed at 0x000000 during reset vector fetch ONLY.
	if (addr < 0x10000 && musashi_is_resetting) {
		return (DWORD)mem.ipl[(addr + 0x10000) ^ 1];
	}

	// Main RAM
	if (addr < mem.length) {
		return (DWORD)mem.ram[addr ^ 1];
	}

	// IPL
	if (addr >= 0xfe0000) {
		addr &= 0x1ffff;
		addr ^= 1;
		return (DWORD)mem.ipl[addr];
	}

	// IPL or SCSI internal
	if (addr >= 0xfc0000) {
		// IPL area
		if ((mem.now == SASI) || (mem.now == SCSIExt)) {
			// IPL area
			addr &= 0x1ffff;
			addr ^= 1;
			return (DWORD)mem.ipl[addr];
		}
		// SCSI internal area (address check)
		if (addr < 0xfc2000) {
			// SCSI internal
			addr &= 0x1fff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
		// X68030 IPL area
		if (mem.now == X68030) {
			// X68030 IPL area
			addr &= 0x1ffff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
		// SCSI internal ROM not exist
		return 0xff;
	}

	// CG
	if (addr >= 0xf00000) {
		addr &= 0xfffff;
		addr ^= 1;
		return (DWORD)mem.cg[addr];
	}

	// SCSI external
	if (mem.now == SCSIExt) {
		if ((addr >= 0xea0020) && (addr <= 0xea1fff)) {
			addr &= 0x1fff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
	}

	// Device access
	if (addr >= 0xc00000) {
		index = addr - 0xc00000;
		index >>= 13;
	 ASSERT(index < 0x180);
		if (mem.table[index] != (MemDevice*)this) {
			return mem.table[index]->ReadOnly(addr);
		}
	}

	// Ignored
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Context create
//
//---------------------------------------------------------------------------
void FASTCALL Memory::MakeContext(BOOL reset)
{
 ASSERT(this);

	// On reset, reset the AreaSet (called from CPU::Reset)
	if (reset) {
	 ASSERT(areaset);
		areaset->Reset();
		return;
	}

	// NOTE: Musashi uses m68k_read/write_memory callbacks instead of
	// Starscream memory region tables. Memory dispatch is handled by
	// Memory::ReadByte/ReadWord/WriteByte/WriteWord which already
	// implement the full address decoding logic.
	// The old Starscream region table construction code has been removed.

	// Force CPU to exit current instruction (was at the end of old MakeContext)
	cpu->Release();
}
void FASTCALL Memory::TerminateProgramRegion(int index, STARSCREAM_PROGRAMREGION *spr)
{
 ASSERT(this);
 ASSERT((index >= 0) && (index < 10));
 ASSERT(spr);

	spr[index].lowaddr = (DWORD)-1;
	spr[index].highaddr = (DWORD)-1;
	spr[index].offset = 0;
}

//---------------------------------------------------------------------------
//
//	Program region terminate
//
//---------------------------------------------------------------------------
void FASTCALL Memory::TerminateDataRegion(int index, STARSCREAM_DATAREGION *sdr)
{
 ASSERT(this);
 ASSERT((index >= 0) && (index < 10));
 ASSERT(sdr);

	sdr[index].lowaddr = (DWORD)-1;
	sdr[index].highaddr = (DWORD)-1;
	sdr[index].memorycall = NULL;
	sdr[index].userdata = NULL;
}

//---------------------------------------------------------------------------
//
//	IPL version check
//	: Check based on version 1.00(87/05/07)
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::CheckIPL() const
{
 ASSERT(this);
 ASSERT(mem.now != None);

	// Existence check
	if (!mem.ipl) {
		return FALSE;
	}

	// SASI type only
	if (mem.now != SASI) {
		return TRUE;
	}

	// Date(BCD) check
	if (mem.ipl[0x1000a] != 0x87) {
		return FALSE;
	}
	if (mem.ipl[0x1000c] != 0x07) {
		return FALSE;
	}
	if (mem.ipl[0x1000d] != 0x05) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	CG check
//	: 8x8 dot all sum,xor check
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::CheckCG() const
{
	BYTE add;
	BYTE eor;
	BYTE *ptr;
	int i;

 ASSERT(this);
 ASSERT(mem.now != None);

	// Existence check
	if (!mem.cg) {
		return FALSE;
	}

	// Initial setting
	add = 0;
	eor = 0;
	ptr = &mem.cg[0x3a800];

	// ADD, XOR loop
	for (i=0; i<0x1000; i++) {
		add = (BYTE)(add + *ptr);
		eor ^= *ptr;
		ptr++;
	}

	// Check(XVI compensation value)
	if ((add != 0xec) || (eor != 0x84)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	CG get
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL Memory::GetCG() const
{
 ASSERT(this);
 ASSERT(mem.cg);

	return mem.cg;
}

//---------------------------------------------------------------------------
//
//	SCSI get
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL Memory::GetSCSI() const
{
 ASSERT(this);
 ASSERT(mem.scsi);

	return mem.scsi;
}

//---------------------------------------------------------------------------
//
//	Musashi Memory Bridge Functions
//	Called from musashi_mem.cpp -> m68k_read/write_memory_*
//
//---------------------------------------------------------------------------
extern "C" {

DWORD FASTCALL MusashiReadByte(DWORD addr)
{
 ASSERT(pMemory);
	return pMemory->ReadByte(addr);
}

DWORD FASTCALL MusashiReadWord(DWORD addr)
{
 ASSERT(pMemory);
	return pMemory->ReadWord(addr);
}

void FASTCALL MusashiWriteByte(DWORD addr, DWORD data)
{
 ASSERT(pMemory);
	pMemory->WriteByte(addr, data);
}

void FASTCALL MusashiWriteWord(DWORD addr, DWORD data)
{
 ASSERT(pMemory);
	pMemory->WriteWord(addr, data);
}

} // extern "C"
