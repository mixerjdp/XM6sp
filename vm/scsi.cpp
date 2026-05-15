//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI. (ytanaka@ipc-tokai.or.jp)
//	[ SCSI(MB89352) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "memory.h"
#include "sram.h"
#include "config.h"
#include "disk.h"
#include "fileio.h"
#include "filepath.h"
#include "scsi.h"

//===========================================================================
//
//	SCSI
//
//===========================================================================
//#define SCSI_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSI::SCSI(VM *p) : MemDevice(p)
{
	// Initialize device ID
	dev.id = MAKEID('S', 'C', 'S', 'I');
	dev.desc = "SCSI (MB89352)";

	// Start address, end address
	memdev.first = 0xea0000;
	memdev.last = 0xea1fff;

	// Initialize work
	memset(&scsi, 0, sizeof(scsi));

	// Devices
	memory = NULL;
	sram = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::Init()
{
	int i;

	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get memory
	ASSERT(!memory);
	memory = (Memory*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(memory);

	// Get SRAM
	ASSERT(!sram);
	sram = (SRAM*)vm->SearchDevice(MAKEID('S', 'R', 'A', 'M'));
	ASSERT(sram);

	// Initialize work
	scsi.bdid = 0x80;
	scsi.ilevel = 2;
	scsi.vector = -1;
	scsi.id = -1;
	for (i=0; i<sizeof(scsi.cmd); i++) {
		scsi.cmd[i] = 0;
	}

	// Initialize configuration
	scsi.memsw = TRUE;
	scsi.mo_first = FALSE;

	// Initialize events
	event.SetDevice(this);
	event.SetDesc("Selection");
	event.SetUser(0);
	event.SetTime(0);
	cdda.SetDevice(this);
	cdda.SetDesc("CD-DA 75fps");
	cdda.SetUser(1);
	cdda.SetTime(0);

	// Initialize disks
	for (i=0; i<DeviceMax; i++) {
		scsi.disk[i] = NULL;
	}
	for (i=0; i<HDMax; i++) {
		scsi.hd[i] = NULL;
	}
	scsi.mo = NULL;
	scsi.cdrom = NULL;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Cleanup()
{
	int i;

	ASSERT(this);

	// Delete HD
	for (i=0; i<HDMax; i++) {
		if (scsi.hd[i]) {
			delete scsi.hd[i];
			scsi.hd[i] = NULL;
		}
	}

	// Delete MO
	if (scsi.mo) {
		delete scsi.mo;
		scsi.mo = NULL;
	}

	// Delete CD
	if (scsi.cdrom) {
		delete scsi.cdrom;
		scsi.cdrom = NULL;
	}

	// To base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Reset()
{
	Memory::memtype type;
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "Reset");

	// SCSI type (query memory)
	type = memory->GetMemType();
	switch (type) {
		// SASI only
		case Memory::None:
		case Memory::SASI:
			scsi.type = 0;
			break;

		// External
		case Memory::SCSIExt:
			scsi.type = 1;
			break;

		// Other (internal)
		default:
			scsi.type = 2;
			break;
	}

	// Dynamically add/remove events
	if (scsi.type == 0) {
		// Remove events
		if (scheduler->HasEvent(&event)) {
			scheduler->DelEvent(&event);
			scheduler->DelEvent(&cdda);
		}
	}
	else {
		// Add events
		if (!scheduler->HasEvent(&event)) {
			scheduler->AddEvent(&event);
			scheduler->AddEvent(&cdda);
		}
	}

	// Write memory switch
	if (scsi.memsw) {
		// Only if SCSI exists
		if (scsi.type != 0) {
			// $ED006F: SCSI flag ('V' for SCSI enabled)
			if (sram->GetMemSw(0x6f) != 'V') {
				sram->SetMemSw(0x6f, 'V');

				// Initialize host ID to 7
				sram->SetMemSw(0x70, 0x07);
			}

			// $ED0070: Board type + host ID
			if (scsi.type == 1) {
				// External
				sram->SetMemSw(0x70, sram->GetMemSw(0x70) | 0x08);
			}
			else {
				// Internal
				sram->SetMemSw(0x70, sram->GetMemSw(0x70) & ~0x08);
			}

			// $ED0071: SASI emulation flag
			sram->SetMemSw(0x71, 0x00);
		}
	}

	// Rebuild disks
	Construct();

	// Reset disks
	for (i=0; i<HDMax; i++) {
		if (scsi.hd[i]) {
			scsi.hd[i]->Reset();
		}
	}
	if (scsi.mo) {
		scsi.mo->Reset();
	}
	if (scsi.cdrom) {
		scsi.cdrom->Reset();
	}

	// Clear command
	for (i=0; i<sizeof(scsi.cmd); i++) {
		scsi.cmd[i] = 0;
	}

	// Reset registers
	ResetReg();

	// Bus free phase
	BusFree();
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::Save(Fileio *fio, int ver)
{
	size_t sz;
	int i;
	Disk *disk;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Save");

	// Flush disks
	for (i=0; i<HDMax; i++) {
		if (scsi.hd[i]) {
			if (!scsi.hd[i]->Flush()) {
				return FALSE;
			}
		}
	}
	if (scsi.mo) {
		if (!scsi.mo->Flush()) {
			return FALSE;
		}
	}
	if (scsi.cdrom) {
		if (!scsi.cdrom->Flush()) {
			return FALSE;
		}
	}

	// Save size
	sz = sizeof(scsi_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save entity
	if (!fio->Write(&scsi, (int)sz)) {
		return FALSE;
	}

	// Save events
	if (!event.Save(fio, ver)) {
		return FALSE;
	}
	if (!cdda.Save(fio, ver)) {
		return FALSE;
	}

	// Save paths
	for (i=0; i<HDMax; i++) {
		if (!scsihd[i].Save(fio, ver)) {
			return FALSE;
		}
	}

	// Save disks
	for (i=0; i<HDMax; i++) {
		if (scsi.hd[i]) {
			if (!scsi.hd[i]->Save(fio, ver)) {
				return FALSE;
			}
		}
		else {
			// Save dummy for count alignment (version 2.04)
			disk = new SCSIHD(this);
			if (!disk->Save(fio, ver)) {
				delete disk;
				return FALSE;
			}
			delete disk;
		}
	}
	if (scsi.mo) {
		if (!scsi.mo->Save(fio, ver)) {
			return FALSE;
		}
	}
	if (scsi.cdrom) {
		if (!scsi.cdrom->Save(fio, ver)) {
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::Load(Fileio *fio, int ver)
{
	size_t sz;
	int i;
	Disk *disk;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);

	LOG0(Log::Normal, "Load");

	// Versions 2.01 and earlier do not support SCSI
	if (ver <= 0x201) {
		// No SCSI, drive count 0
		scsi.type = 0;
		scsi.scsi_drives = 0;

		// Remove events
		if (scheduler->HasEvent(&event)) {
			scheduler->DelEvent(&event);
			scheduler->DelEvent(&cdda);
		}
		return TRUE;
	}

	// Delete disks and initialize
	for (i=0; i<HDMax; i++) {
		if (scsi.hd[i]) {
			delete scsi.hd[i];
			scsi.hd[i] = NULL;
		}
	}
	if (scsi.mo) {
		delete scsi.mo;
		scsi.mo = NULL;
	}
	if (scsi.cdrom) {
		delete scsi.cdrom;
		scsi.cdrom = NULL;
	}
	scsi.type = 0;
	scsi.scsi_drives = 0;

	// Load size and compare
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(scsi_t)) {
		return FALSE;
	}

	// Load entity
	if (!fio->Read(&scsi, (int)sz)) {
		return FALSE;
	}

	// Clear disks (will be overwritten during load)
	for (i=0; i<HDMax; i++) {
		scsi.hd[i] = NULL;
	}
	scsi.mo = NULL;
	scsi.cdrom = NULL;

	// Load events
	if (!event.Load(fio, ver)) {
		return FALSE;
	}
	if (ver >= 0x0203) {
		// CD-DA event added in version 2.03
		if (!cdda.Load(fio, ver)) {
			return FALSE;
		}
	}

	// Load paths
	for (i=0; i<HDMax; i++) {
		if (!scsihd[i].Load(fio, ver)) {
			return FALSE;
		}
	}

	// Rebuild disks
	Construct();

	// Load disks (added in version 2.03)
	if (ver >= 0x0203) {
		for (i=0; i<HDMax; i++) {
			if (scsi.hd[i]) {
				if (!scsi.hd[i]->Load(fio, ver)) {
					return FALSE;
				}
			}
			else {
				// Load dummy for count alignment (version 2.04)
				if (ver >= 0x0204) {
					disk = new SCSIHD(this);
					if (!disk->Load(fio, ver)) {
						delete disk;
						return FALSE;
					}
					delete disk;
				}
			}
		}
		if (scsi.mo) {
			if (!scsi.mo->Load(fio, ver)) {
				return FALSE;
			}
		}
		if (scsi.cdrom) {
			if (!scsi.cdrom->Load(fio, ver)) {
				return FALSE;
			}
		}
	}

	// Dynamically add/remove events
	if (scsi.type == 0) {
		// Remove events
		if (scheduler->HasEvent(&event)) {
			scheduler->DelEvent(&event);
			scheduler->DelEvent(&cdda);
		}
	}
	else {
		// Add events
		if (!scheduler->HasEvent(&event)) {
			scheduler->AddEvent(&event);
			scheduler->AddEvent(&cdda);
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::ApplyCfg(const Config *config)
{
	int i;

	ASSERT(this);
	ASSERT(config);
	LOG0(Log::Normal, "Apply configuration");

	// SCSI drive count
	scsi.scsi_drives = config->scsi_drives;

	// Memory switch auto update
	scsi.memsw = config->scsi_sramsync;

	// MO priority flag
	scsi.mo_first = config->scsi_mofirst;

	// SCSI file names
	for (i=0; i<HDMax; i++) {
		scsihd[i].SetPath(config->scsi_file[i]);
	}

	// Rebuild disks
	Construct();
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Diagnostics
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::AssertDiag() const
{
	// Base class
	MemDevice::AssertDiag();

	ASSERT(this);
	ASSERT(GetID() == MAKEID('S', 'C', 'S', 'I'));
	ASSERT(memory);
	ASSERT(memory->GetID() == MAKEID('M', 'E', 'M', ' '));
	ASSERT(sram);
	ASSERT(sram->GetID() == MAKEID('S', 'R', 'A', 'M'));
	ASSERT((scsi.type == 0) || (scsi.type == 1) || (scsi.type == 2));
	ASSERT((scsi.vector == -1) || (scsi.vector == 0x6c) || (scsi.vector == 0xf6));
	ASSERT((scsi.id >= -1) && (scsi.id <= 7));
	ASSERT(scsi.bdid != 0);
	ASSERT(scsi.bdid < 0x100);
	ASSERT(scsi.ints < 0x100);
	ASSERT(scsi.mbc < 0x10);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSI::ReadByte(DWORD addr)
{
	const BYTE *rom;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);

	// For normal reads
	if (addr >= memdev.first) {
		// SASI only or internal, this space is not visible
		if (scsi.type != 1) {
			// Bus error
			cpu->BusErr(addr, TRUE);
			return 0xff;
		}
	}

	// Address mask
	addr &= 0x1fff;

	// ROM data
	if (addr >= 0x20) {
		// Wait
		scheduler->Wait(1);

		// Return ROM
		rom = memory->GetSCSI();
		return rom[addr];
	}

	// Register
	if ((addr & 1) == 0) {
		// Even address is not decoded
		return 0xff;
	}
	addr &= 0x1f;
	addr >>= 1;

	// Wait
	scheduler->Wait(1);

	// By register
	switch (addr) {
		// BDID
		case 0:
			return scsi.bdid;

		// SCTL
		case 1:
			return scsi.sctl;

		// SCMD
		case 2:
			return scsi.scmd;

		// Reserved
		case 3:
			return 0xff;

		// INTS
		case 4:
			return scsi.ints;

		// PSNS
		case 5:
			return GetPSNS();

		// SSTS
		case 6:
			return GetSSTS();

		// SERR
		case 7:
			return GetSERR();

		// PCTL
		case 8:
			return scsi.pctl;

		// MBC
		case 9:
			return scsi.mbc;

		// DREG
		case 10:
			return GetDREG();

		// TEMP
		case 11:
			return scsi.temp;

		// TCH
		case 12:
			return (BYTE)(scsi.tc >> 16);

		// TCM
		case 13:
			return (BYTE)(scsi.tc >> 8);

		// TCL
		case 14:
			return (BYTE)scsi.tc;

		// Reserved
		case 15:
			return 0xff;
	}

	// Normally should not reach here
	LOG1(Log::Warning, "Undefined register read R%d", addr);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSI::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr & 1) == 0);

	return (0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT(data < 0x100);

	// For normal reads
	if (addr >= memdev.first) {
		// SASI only or internal, this space is not visible
		if (scsi.type != 1) {
			// Bus error
			cpu->BusErr(addr, FALSE);
			return;
		}
	}

	// Address mask
	addr &= 0x1fff;

	// ROM data
	if (addr >= 0x20) {
		// Write to ROM
		return;
	}

	// Register
	if ((addr & 1) == 0) {
		// Even address is not decoded
		return;
	}
	addr &= 0x1f;
	addr >>= 1;

	// Wait
	scheduler->Wait(1);

	// By register
	switch (addr) {
		// BDID
		case 0:
			SetBDID(data);
			return;

		// SCTL
		case 1:
			SetSCTL(data);
			return;

		// SCMD
		case 2:
			SetSCMD(data);
			return;

		// Reserved
		case 3:
			break;

		// INTS
		case 4:
			SetINTS(data);
			return;

		// SDGC
		case 5:
			SetSDGC(data);
			return;

		// SSTS(Read Only)
		case 6:
			// SCSI driver v1.04 patch seems to write for timing delay
			return;

		// SERR(Read Only)
		case 7:
			break;

		// PCTL
		case 8:
			SetPCTL(data);
			return;

		// MBC(Read Only)
		case 9:
			break;

		// DREG
		case 10:
			SetDREG(data);
			return;

		// TEMP
		case 11:
			SetTEMP(data);
			return;

		// TCH
		case 12:
			SetTCH(data);
			return;

		// TCM
		case 13:
			SetTCM(data);
			return;

		// TCL
		case 14:
			SetTCL(data);
			return;

		// Reserved
		case 15:
			break;
	}

	// Normally should not reach here
	LOG2(Log::Warning, "Undefined register write R%d <- $%02X", addr, data);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSI::ReadOnly(DWORD addr) const
{
	const BYTE *rom;

	ASSERT(this);

	// For normal reads
	if (addr >= memdev.first) {
		// SASI only or internal, this space is not visible
		if (scsi.type != 1) {
			return 0xff;
		}
	}

	// Address mask
	addr &= 0x1fff;

	// ROM data
	if (addr >= 0x20) {
		// Return ROM
		rom = memory->GetSCSI();
		return rom[addr];
	}

	// Register
	if ((addr & 1) == 0) {
		// Even address is not decoded
		return 0xff;
	}
	addr &= 0x1f;
	addr >>= 1;

	// By register
	switch (addr) {
		// BDID
		case 0:
			return scsi.bdid;

		// SCTL
		case 1:
			return scsi.sctl;

		// SCMD
		case 2:
			return scsi.scmd;

		// Reserved
		case 3:
			break;

		// INTS
		case 4:
			return scsi.ints;

		// PSNS
		case 5:
			return GetPSNS();

		// SSTS
		case 6:
			return GetSSTS();

		// SERR
		case 7:
			return GetSERR();

		// PCTL
		case 8:
			return scsi.pctl;

		// MBC
		case 9:
			return scsi.mbc;

		// DREG
		case 10:
			return scsi.buffer[scsi.offset];

		// TEMP
		case 11:
			return scsi.temp;

		// TCH
		case 12:
			return (BYTE)(scsi.tc >> 16);

		// TCM
		case 13:
			return (BYTE)(scsi.tc >> 8);

		// TCL
		case 14:
			return (BYTE)scsi.tc;

		// Reserved
		case 15:
			break;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Get internal data
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::GetSCSI(scsi_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT_DIAG();

	// Copy internal work
	*buffer = scsi;
}

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::Callback(Event* ev)
{
	BOOL success;

	ASSERT(this);
	ASSERT(ev);
	ASSERT_DIAG();

	// CD-DA event specific processing
	if (ev->GetUser() == 1) {
		if (scsi.cdrom) {
			// Notify CD-ROM
			if (scsi.cdrom->NextFrame()) {
				// Normal speed
				ev->SetTime(26666);
			}
			else {
				// Correct once per 75 times
				ev->SetTime(26716);
			}
		}

		// Continue
		return TRUE;
	}

	// Selection phase only valid
	ASSERT(ev->GetUser() == 0);
	if (scsi.phase != selection) {
		// One-shot
		return FALSE;
	}

	// Create success flag
	success = FALSE;
	if (scsi.id != -1) {
		// ID has both initiator and target specified
		if (scsi.disk[scsi.id]) {
			// Valid disk is mounted
			success = TRUE;
		}
	}

	// Branch by success flag
	if (success) {
#if defined(SCSI_LOG)
		LOG1(Log::Normal, "Selection success ID=%d", scsi.id);
#endif	// SCSI_LOG

		// Decrement TC
		scsi.tc &= 0x00ffff00;
		scsi.tc -= 0x2800;

		// Complete (selection success)
		Interrupt(4, TRUE);

		// BSY=1 here
		scsi.bsy = TRUE;

		// ATN=1 means message out phase, otherwise command phase
		if (scsi.atn) {
			MsgOut();
		}
		else {
			Command();
		}
	}
	else {
#if defined(SCSI_LOG)
		LOG1(Log::Normal, "Selection failed TEMP=$%02X", scsi.temp);
#endif	// SCSI_LOG

		// Set TC=0 (timeout)
		scsi.tc = 0;

		// Timeout (selection failed)
		Interrupt(2, TRUE);

		// BSY=FALSE means bus free
		if (!scsi.bsy) {
			BusFree();
		}
	}

	// One-shot
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Reset registers
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::ResetReg()
{
	ASSERT(this);

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Reset registers");
#endif	// SCSI_LOG

	// Current ID
	scsi.id = -1;

	// Interrupt
	scsi.vector = -1;

	// Registers
	scsi.bdid = 0x80;
	scsi.sctl = 0x80;
	scsi.scmd = 0;
	scsi.ints = 0;
	scsi.sdgc = 0;
	scsi.pctl = 0;
	scsi.mbc = 0;
	scsi.temp = 0;
	scsi.tc = 0;

	// Stop transfer command
	scsi.trans = FALSE;

	// Phase (SPC intends bus free)
	scsi.phase = busfree;

	// Interrupt check
	IntCheck();
}

//---------------------------------------------------------------------------
//
//	Reset transfer
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::ResetCtrl()
{
	ASSERT(this);

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Reset transfer");
#endif	// SCSI_LOG

	// Stop transfer command
	scsi.trans = FALSE;
}

//---------------------------------------------------------------------------
//
//	Reset bus
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::ResetBus(BOOL reset)
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	if (reset) {
		LOG0(Log::Normal, "SCSI bus reset");
	}
	else {
		LOG0(Log::Normal, "SCSI bus reset release");
	}
#endif	// SCSI_LOG

	// Store
	scsi.rst = reset;

	// Signal lines (same as bus free)
	scsi.bsy = FALSE;
	scsi.sel = FALSE;
	scsi.atn = FALSE;
	scsi.msg = FALSE;
	scsi.cd = FALSE;
	scsi.io = FALSE;
	scsi.req = FALSE;
	scsi.ack = FALSE;

	// Stop transfer command
	scsi.trans = FALSE;

	// Phase (SPC intends bus free)
	scsi.phase = busfree;

	// Generate/cancel bus reset interrupt
	Interrupt(0, reset);
}

//---------------------------------------------------------------------------
//
//	Set BDID
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetBDID(DWORD data)
{
	DWORD bdid;

	ASSERT(this);
	ASSERT(data < 0x100);

	// Only 3 bits valid
	data &= 0x07;

#if defined(SCSI_LOG)
	LOG1(Log::Normal, "Set board ID ID%d", data);
#endif	// SCSI_LOG

	// Convert from number to bit
	bdid = (DWORD)(1 << data);

	// If different, need to rebuild disks
	if (scsi.bdid != bdid) {
		scsi.bdid = bdid;
		Construct();
	}
}

//---------------------------------------------------------------------------
//
//	Set SCTL
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetSCTL(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

#if defined(SCSI_LOG)
	LOG1(Log::Normal, "Set SCTL $%02X", data);
#endif	// SCSI_LOG

	scsi.sctl = data;

	// bit7: Reset & Disable
	if (scsi.sctl & 0x80) {
		// Reset registers
		ResetReg();
	}

	// bit6: Reset Control
	if (scsi.sctl & 0x40) {
		// Reset transfer
		ResetCtrl();
	}

	// Interrupt check
	IntCheck();
}

//---------------------------------------------------------------------------
//
//	Set SCMD
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetSCMD(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// bit4: RST Out
	if (data & 0x10) {
		if ((scsi.scmd & 0x10) == 0) {
			// RST 0->1
			if ((scsi.sctl & 0x80) == 0) {
				// SPC not in reset
				ResetBus(TRUE);
			}
		}
	}
	else {
		if (scsi.scmd & 0x10) {
			// RST 1->0
			if ((scsi.sctl & 0x80) == 0) {
				// SPC not in reset
				ResetBus(FALSE);
			}
		}
	}

	// Store command
	scsi.scmd = data;

	// bit7-5: Command
	switch (scsi.scmd >> 5) {
		// Bus Release
		case 0:
			BusRelease();
			break;

		// Select
		case 1:
			Select();
			break;

		// Reset ATN
		case 2:
			ResetATN();
			break;

		// Set ATN
		case 3:
			SetATN();
			break;

		// Transfer
		case 4:
			Transfer();
			break;

		// Transfer Pause
		case 5:
			TransPause();
			break;

		// Reset ACK/REQ
		case 6:
			ResetACKREQ();
			break;

		// Set ACK/REQ
		case 7:
			SetACKREQ();
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Set INTS
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetINTS(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG1(Log::Normal, "Clear interrupt $%02X", data);
#endif	// SCSI_LOG

	// Withdraw interrupt with bit set
	scsi.ints &= ~data;

	// Interrupt check
	IntCheck();
}

//---------------------------------------------------------------------------
//
//	Get PSNS
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSI::GetPSNS() const
{
	DWORD data;

	ASSERT(this);
	ASSERT_DIAG();

	// Initialize data
	data = 0;

	// bit7: REQ
	if (scsi.req) {
		data |= 0x80;
	}

	// bit6: ACK
	if (scsi.ack) {
		data |= 0x40;
	}

	// bit5: ATN
	if (scsi.atn) {
		data |= 0x20;
	}

	// bit4: SEL
	if (scsi.sel) {
		data |= 0x10;
	}

	// bit3: BSY
	if (scsi.bsy) {
		data |= 0x08;
	}

	// bit2: MSG
	if (scsi.msg) {
		data |= 0x04;
	}

	// bit1: C/D
	if (scsi.cd) {
		data |= 0x02;
	}

	// bit0: I/O
	if (scsi.io) {
		data |= 0x01;
	}

	return data;
}

//---------------------------------------------------------------------------
//
//	Set SDGC
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetSDGC(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

#if defined(SCSI_LOG)
	LOG1(Log::Normal, "Set SDGC $%02X", data);
#endif	// SCSI_LOG

	scsi.sdgc = data;
}

//---------------------------------------------------------------------------
//
//	Get SSTS
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSI::GetSSTS() const
{
	DWORD data;

	ASSERT(this);
	ASSERT_DIAG();

	// Initialize data
	data = 0;

	// bit6-7: Connected
	if (scsi.phase != busfree) {
		// Not bus free means connected as initiator
		data = 0x80;
	}

	// bit5: SPC BUSY
	if (scsi.bsy) {
		data |= 0x20;
	}

	// bit4: Transfer Progress
	if (scsi.trans) {
		data |= 0x10;
	}

	// bit3: Reset
	if (scsi.rst) {
		data |= 0x08;
	}

	// bit2: TC=0
	if (scsi.tc == 0) {
		data |= 0x04;
	}

	// bit0-1: FIFO count (00:1-7 bytes, 01:Empty, 10:8 bytes, 11:unused)
	if (scsi.trans) {
		// During transfer
		if (scsi.io) {
			// Only during Read, data may accumulate in FIFO
			if ((scsi.length > 0) && (scsi.tc > 0)) {
				if ((scsi.length >= 8) && (scsi.tc >= 8)) {
					// 8 or more bytes (TS-6BS1mkIII)
					data |= 0x02;
					return data;
				}
				else {
					// 7 or fewer bytes
					return data;
				}
			}
		}
	}

	// No data in FIFO
	data |= 0x01;
	return data;
}

//---------------------------------------------------------------------------
//
//	Get SERR
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSI::GetSERR() const
{
	ASSERT(this);
	ASSERT_DIAG();

	// During program transfer, if request signal is output
	if (scsi.sdgc & 0x20) {
		if (scsi.trans) {
			if (scsi.tc != 0) {
				// Indicate Xfer Out
				return 0x20;
			}
		}
	}

	// Otherwise 0
	return 0x00;
}

//---------------------------------------------------------------------------
//
//	Set PCTL
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetPCTL(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	scsi.pctl = data;

}

//---------------------------------------------------------------------------
//
//	Get DREG
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSI::GetDREG()
{
	DWORD data;

	ASSERT(this);
	ASSERT_DIAG();

	// If not transferring, return FF
	if (!scsi.trans) {
		return 0xff;
	}

	// If request is not asserted, return FF
	if (!scsi.req) {
		return 0xff;
	}

	// If TC is 0, return FF
	if (scsi.tc == 0) {
		return 0xff;
	}

	// Perform REQ-ACK handshake automatically
	Xfer(&data);
	XferNext();

	// MBC
	scsi.mbc = (scsi.mbc - 1) & 0x0f;

	// TC
	scsi.tc--;
	if (scsi.tc == 0) {
		// Transfer complete (if scsi.length != 0, SCSI bus is locked)
		TransComplete();
		return data;
	}

	// If in status phase, transfer complete or error
	if (scsi.phase == status) {
		// Transfer complete
		TransComplete();
		return data;
	}

	// Continue reading
	return data;
}

//---------------------------------------------------------------------------
//
//	Set DREG
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetDREG(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// If not transferring, do nothing
	if (!scsi.trans) {
		return;
	}

	// If request is not asserted, do nothing
	if (!scsi.req) {
		return;
	}

	// If TC is 0, do nothing
	if (scsi.tc == 0) {
		return;
	}

	// Perform REQ-ACK handshake automatically
	Xfer(&data);
	XferNext();

	// MBC
	scsi.mbc = (scsi.mbc - 1) & 0x0f;

	// TC
	scsi.tc--;
	if (scsi.tc == 0) {
		// Transfer complete (if scsi.length != 0, SCSI bus is locked)
		TransComplete();
		return;
	}

	// If in status phase, transfer complete or error
	if (scsi.phase == status) {
		// Transfer complete
		TransComplete();
		return;
	}
}

//---------------------------------------------------------------------------
//
//	Set TEMP
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetTEMP(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	scsi.temp = data;

	// In selection phase, setting 0x00 makes SEL=0
	if (scsi.phase == selection) {
		if (data == 0x00) {
			// SEL=0, BSY=0
			scsi.sel = FALSE;
			scsi.bsy = FALSE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Set TCH
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetTCH(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// Set to b23-b16
	scsi.tc &= 0x0000ffff;
	data <<= 16;
	scsi.tc |= data;

#if defined(SCSI_LOG)
	LOG1(Log::Normal, "Set TCH TC=$%06X", scsi.tc);
#endif	// SCSI_LOG
}

//---------------------------------------------------------------------------
//
//	Set TCM
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetTCM(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	// Set to b15-b8
	scsi.tc &= 0x00ff00ff;
	data <<= 8;
	scsi.tc |= data;

#if defined(SCSI_LOG)
	LOG1(Log::Normal, "Set TCM TC=$%06X", scsi.tc);
#endif	// SCSI_LOG
}

//---------------------------------------------------------------------------
//
//	Set TCL
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetTCL(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	// Set to b7-b0
	scsi.tc &= 0x00ffff00;
	scsi.tc |= data;

#if defined(SCSI_LOG)
	LOG1(Log::Normal, "Set TCL TC=$%06X", scsi.tc);
#endif	// SCSI_LOG

	// If SCMD is $20, continue selection
	if ((scsi.scmd & 0xe0) == 0x20) {
		SelectTime();
	}
}

//---------------------------------------------------------------------------
//
//	Bus release
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::BusRelease()
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Bus Release command");
#endif	// SCSI_LOG

	// Bus free
	BusFree();
}

//---------------------------------------------------------------------------
//
//	Select
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Select()
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Select command");
#endif	// SCSI_LOG

	// Not processed during SPC reset or SCSI bus reset
	if (scsi.sctl & 0x80) {
		return;
	}
	if (scsi.rst) {
		return;
	}

	// If SCTL bit4 is 1, arbitration phase runs first
	if (scsi.sctl & 0x10) {
		Arbitration();
	}

	// If PCTL bit0 is 1, reselection (target only allowed)
	if (scsi.pctl & 1) {
		LOG0(Log::Warning, "Reselection phase");
		BusFree();
		return;
	}

	// Selection phase
	Selection();
}

//---------------------------------------------------------------------------
//
//	Reset ATN
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::ResetATN()
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Reset ATN command");
#endif	// SCSI_LOG

	// Signal line operated by initiator
	scsi.atn = FALSE;
}

//---------------------------------------------------------------------------
//
//	Set ATN
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetATN()
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Set ATN command");
#endif	// SCSI_LOG

	// Signal line operated by initiator
	scsi.atn = TRUE;
}

//---------------------------------------------------------------------------
//
//	Transfer
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Transfer()
{
	ASSERT(this);
	ASSERT_DIAG();
	ASSERT(scsi.req);

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Transfer command");
#endif	// SCSI_LOG

	// Check if device side is ready
	if (scsi.length <= 0) {
		// Service Required (phase mismatch)
		Interrupt(3, TRUE);
		return;
	}

	// Check if phase matches

	// Check if controller side is ready
	if (scsi.tc == 0) {
		// Complete (transfer end)
		Interrupt(4, TRUE);
	}

	// Start transfer command
	scsi.trans = TRUE;
}

//---------------------------------------------------------------------------
//
//	Transfer pause
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::TransPause()
{
	ASSERT(this);
	ASSERT_DIAG();

	// Only target can issue
	LOG0(Log::Warning, "Transfer Pause command");
}

//---------------------------------------------------------------------------
//
//	Transfer complete
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::TransComplete()
{
	ASSERT(this);
	ASSERT(!scsi.ack);
	ASSERT(scsi.trans);
	ASSERT_DIAG();

	// Transfer complete
	scsi.trans = FALSE;

	// Transfer complete interrupt
	Interrupt(4, TRUE);
}

//---------------------------------------------------------------------------
//
//	Reset ACK/REQ
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::ResetACKREQ()
{
	ASSERT(this);
	ASSERT_DIAG();

	// Only meaningful if ACK is asserted
	if (!scsi.ack) {
		return;
	}

	// Phases where ACK is asserted are limited
	ASSERT((scsi.phase >= command) && (scsi.phase != execute));

	// Advance data transfer
	XferNext();
}

//---------------------------------------------------------------------------
//
//	Set ACK/REQ
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SetACKREQ()
{
	ASSERT(this);
	ASSERT_DIAG();

	// Only meaningful if REQ is asserted
	if (!scsi.req) {
		return;
	}

	// Phases where REQ is asserted are limited
	ASSERT((scsi.phase >= command) && (scsi.phase != execute));

	// Transfer data between TEMP register and SCSI data bus
	Xfer(&scsi.temp);
}

//---------------------------------------------------------------------------
//
//	Data transfer
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Xfer(DWORD *reg)
{
	ASSERT(this);
	ASSERT(reg);
	ASSERT_DIAG();

	// REQ must be asserted
	ASSERT(scsi.req);
	ASSERT(!scsi.ack);
	ASSERT((scsi.phase >= command) && (scsi.phase != execute));

	// Signal line operated by initiator
	scsi.ack = TRUE;

	// Put data on SCSI data bus
	switch (scsi.phase) {
		// Command phase
		case command:
#if defined(SCSI_LOG)
			LOG1(Log::Normal, "Command phase $%02X", *reg);
#endif	// SCSI_LOG
			// Reset length by first data (offset 0)
			scsi.cmd[scsi.offset] = *reg;
			if (scsi.offset == 0) {
				if (scsi.temp >= 0x20) {
					// 10-byte CDB
					scsi.length = 10;
				}
			}
			break;

		// Message in phase
		case msgin:
			*reg = scsi.message;
#if defined(SCSI_LOG)
			LOG1(Log::Normal, "Message in phase $%02X", *reg);
#endif	// SCSI_LOG
			break;

		// Message out phase
		case msgout:
			scsi.message = *reg;
#if defined(SCSI_LOG)
			LOG1(Log::Normal, "Message out phase $%02X", *reg);
#endif	// SCSI_LOG
			break;

		// Data in phase
		case datain:
			*reg = (DWORD)scsi.buffer[scsi.offset];
			break;

		// Data out phase
		case dataout:
			scsi.buffer[scsi.offset] = (BYTE)*reg;
			break;

		// Status phase
		case status:
			*reg = scsi.status;
#if defined(SCSI_LOG)
			LOG1(Log::Normal, "Status phase $%02X", *reg);
#endif	// SCSI_LOG
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}

	// Signal line operated by target
	scsi.req = FALSE;
}

//---------------------------------------------------------------------------
//
//	Continue data transfer
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::XferNext()
{
	BOOL result;

	ASSERT(this);
	ASSERT_DIAG();

	// ACK must be asserted
	ASSERT(!scsi.req);
	ASSERT(scsi.ack);
	ASSERT((scsi.phase >= command) && (scsi.phase != execute));

	// Offset and length
	ASSERT(scsi.length >= 1);
	scsi.offset++;
	scsi.length--;

	// Signal line operated by initiator
	scsi.ack = FALSE;

	// If length != 0, set req again
	if (scsi.length != 0) {
		scsi.req = TRUE;
		return;
	}

	// Block decrement, initialize result
	scsi.blocks--;
	result = TRUE;

	// Processing after data acceptance (by phase)
	switch (scsi.phase) {
		// Data in phase
		case datain:
			if (scsi.blocks != 0) {
				// Set next buffer (set offset, length)
				result = XferIn();
			}
			break;

		// Data out phase
		case dataout:
			if (scsi.blocks == 0) {
				// End with this buffer
				result = XferOut(FALSE);
			}
			else {
				// Continue to next buffer (set offset, length)
				result = XferOut(TRUE);
			}
			break;

		// Message out phase
		case msgout:
			if (!XferMsg(scsi.message)) {
				// If message out fails, immediately bus free
				BusFree();
				return;
			}
			// Clear message data for message in
			scsi.message = 0x00;
			break;
	}

	// If result FALSE, transition to status phase
	if (!result) {
		Error();
		return;
	}

	// If blocks != 0, set req again
	if (scsi.blocks != 0){
		ASSERT(scsi.length > 0);
		ASSERT(scsi.offset == 0);
		scsi.req = TRUE;
		return;
	}

	// Move to next phase
	switch (scsi.phase) {
		// Command phase
		case command:
			// Execute phase
			Execute();
			break;

		// Message in phase
		case msgin:
			// Bus free phase
			BusFree();
			break;

		// Message out phase
		case msgout:
			// Command phase
			Command();
			break;

		// Data in phase
		case datain:
			// Status phase
			Status();
			break;

		// Data out phase
		case dataout:
			// Status phase
			Status();
			break;

		// Status phase
		case status:
			// Message in phase
			MsgIn();
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Data transfer IN
//	Note: Must reset offset, length
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::XferIn()
{
	ASSERT(this);
	ASSERT(scsi.phase == datain);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// If disk is gone due to state load
	if (!scsi.disk[scsi.id]) {
		// Abort data in
		return FALSE;
	}

	// Only READ commands
	switch (scsi.cmd[0]) {
		// READ(6)
		case 0x08:
		// READ(10)
		case 0x28:
			// Read from disk
			scsi.length = scsi.disk[scsi.id]->Read(scsi.buffer, scsi.next);
			scsi.next++;

			// If error, go to status phase
			if (scsi.length <= 0) {
				// Abort data in
				return FALSE;
			}

			// If normal, set work
			scsi.offset = 0;
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			return FALSE;
	}

	// Buffer setup successful
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Data transfer OUT
//	Note: If cont=TRUE, must reset offset, length
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::XferOut(BOOL cont)
{
	ASSERT(this);
	ASSERT(scsi.phase == dataout);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// If disk is gone due to state load
	if (!scsi.disk[scsi.id]) {
		// Abort data out
		return FALSE;
	}

	// MODE SELECT or WRITE
	switch (scsi.cmd[0]) {
		// MODE SELECT
		case 0x15:
			if (!scsi.disk[scsi.id]->ModeSelect(scsi.buffer, scsi.offset)) {
				// MODE SELECT failed
				return FALSE;
			}
			break;

		// WRITE(6)
		case 0x0a:
		// WRITE(10)
		case 0x2a:
			// Perform write
			if (!scsi.disk[scsi.id]->Write(scsi.buffer, scsi.next - 1)) {
				// Write failed
				return FALSE;
			}

			// End here if next block not needed
			scsi.next++;
			if (!cont) {
				break;
			}

			// Check next block
			scsi.length = scsi.disk[scsi.id]->WriteCheck(scsi.next);
			if (scsi.length <= 0) {
				// Cannot write
				return FALSE;
			}

			// If normal, set work
			scsi.offset = 0;
			break;
	}

	// Buffer save successful
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Data transfer MSG
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::XferMsg(DWORD /*msg*/)
{
	ASSERT(this);
	ASSERT(scsi.phase == msgout);
	ASSERT_DIAG();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Bus free phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::BusFree()
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Bus free phase");
#endif	// SCSI_LOG

	// Set phase
	scsi.phase = busfree;

	// Signal lines
	scsi.bsy = FALSE;
	scsi.sel = FALSE;
	scsi.atn = FALSE;
	scsi.msg = FALSE;
	scsi.cd = FALSE;
	scsi.io = FALSE;
	scsi.req = FALSE;
	scsi.ack = FALSE;

	// Initialize status and message (GOOD)
	scsi.status = 0x00;
	scsi.message = 0x00;

	// Stop event
	event.SetTime(0);

	// Check PCTL Busfree INT Enable
	if (scsi.pctl & 0x8000) {
		// Disconnect interrupt
		Interrupt(5, TRUE);
	}
	else {
		// Interrupt off
		Interrupt(5, FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Arbitration phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Arbitration()
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Arbitration phase");
#endif	// SCSI_LOG

	// Set phase
	scsi.phase = arbitration;

	// Signal lines
	scsi.bsy = TRUE;
	scsi.sel = TRUE;
}

//---------------------------------------------------------------------------
//
//	Selection phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Selection()
{
	int i;
	DWORD temp;

	ASSERT(this);
	ASSERT_DIAG();

	// Set phase
	scsi.phase = selection;

	// Initialize target ID
	scsi.id = -1;

	// TEMP register needs both initiator and target bits
	temp = scsi.temp;
	if ((temp & scsi.bdid) == scsi.bdid) {
		// Remove initiator bit
		temp &= ~(scsi.bdid);

		// Find target
		for (i=0; i<8; i++) {
			if (temp == (DWORD)(1 << i)) {
				scsi.id = i;
				break;
			}
		}
	}

#if defined(SCSI_LOG)
	if (scsi.id != -1) {
		if (scsi.disk[scsi.id]) {
			LOG1(Log::Normal, "Selection phase ID=%d (device present)", scsi.id);
		}
		else {
			LOG1(Log::Normal, "Selection phase ID=%d (not connected)", scsi.id);
		}
	}
	else {
		LOG0(Log::Normal, "Selection phase (invalid)");
	}
#endif	// SCSI_LOG

	// Determine selection time
	SelectTime();
}

//---------------------------------------------------------------------------
//
//	Selection phase time setting
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SelectTime()
{
	DWORD tc;

	ASSERT(this);
	ASSERT((scsi.scmd & 0xe0) == 0x20);
	ASSERT_DIAG();

	tc = scsi.tc;
	if (tc == 0) {
		LOG0(Log::Warning, "Selection timeout is infinite");
		tc = 0x1000000;
	}
	tc &= 0x00ffff00;
	tc += 15;

	// Further consider TCL
	tc += (((scsi.tc & 0xff) + 7) >> 1);

	// Convert from 400ns units to 500ns units (us)
	tc = (tc * 4 / 5);

	// Phase
	scsi.phase = selection;

	// Signal lines
	scsi.sel = TRUE;

	// Set event time
	event.SetDesc("Selection");
	if (scsi.id != -1) {
		if (scsi.disk[scsi.id]) {
			// If specified ID is valid, succeed selection in 16us
			event.SetTime(32);
			return;
		}
	}

	// For timeout, set as per TC
	event.SetTime(tc);
}

//---------------------------------------------------------------------------
//
//	Command phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Command()
{
	ASSERT(this);
	ASSERT((scsi.id >= 0) && (scsi.id <= 7));
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Command phase");
#endif	// SCSI_LOG

	// If disk is gone due to state load
	if (!scsi.disk[scsi.id]) {
		// Abort command phase
		BusFree();
		return;
	}

	// Set phase
	scsi.phase = command;

	// Signal line operated by initiator
	scsi.atn = FALSE;

	// Signal lines operated by target
	scsi.msg = FALSE;
	scsi.cd = TRUE;
	scsi.io = FALSE;

	// Data transfer is 6 bytes x 1 block
	scsi.offset = 0;
	scsi.length = 6;
	scsi.blocks = 1;

	// Request command
	scsi.req = TRUE;
}

//---------------------------------------------------------------------------
//
//	Execute phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Execute()
{
	ASSERT(this);
	ASSERT((scsi.id >= 0) && (scsi.id <= 7));
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG1(Log::Normal, "Execute phase command $%02X", scsi.cmd[0]);
#endif	// SCSI_LOG

	// If disk is gone due to state load
	if (!scsi.disk[scsi.id]) {
		// Abort command phase
		Error();
		return;
	}

	// Set phase
	scsi.phase = execute;

	// Initialize for data transfer
	scsi.offset = 0;
	scsi.blocks = 1;

	// Process by command
	switch (scsi.cmd[0]) {
		// TEST UNIT READY
		case 0x00:
			TestUnitReady();
			return;

		// REZERO
		case 0x01:
			Rezero();
			return;

		// REQUEST SENSE
		case 0x03:
			RequestSense();
			return;

		// FORMAT UNIT
		case 0x04:
			Format();
			return;

		// REASSIGN BLOCKS
		case 0x07:
			Reassign();
			return;

		// READ(6)
		case 0x08:
			Read6();
			return;

		// WRITE(6)
		case 0x0a:
			Write6();
			return;

		// SEEK(6)
		case 0x0b:
			Seek6();
			return;

		// INQUIRY
		case 0x12:
			Inquiry();
			return;

		// MODE SELECT
		case 0x15:
			ModeSelect();
			return;

		// MODE SENSE
		case 0x1a:
			ModeSense();
			return;

		// START STOP UNIT
		case 0x1b:
			StartStop();
			return;

		// SEND DIAGNOSTIC
		case 0x1d:
			SendDiag();
			return;

		// PREVENT/ALLOW MEDIUM REMOVAL
		case 0x1e:
			Removal();
			return;

		// READ CAPACITY
		case 0x25:
			ReadCapacity();
			return;

		// READ(10)
		case 0x28:
			Read10();
			return;

		// WRITE(10)
		case 0x2a:
			Write10();
			return;

		// SEEK(10)
		case 0x2b:
			Seek10();
			return;

		// WRITE AND VERIFY
		case 0x2e:
			Write10();
			return;

		// VERIFY
		case 0x2f:
			Verify();
			return;

		// READ TOC
		case 0x43:
			ReadToc();
			return;

		// PLAY AUDIO(10)
		case 0x45:
			PlayAudio10();
			return;

		// PLAY AUDIO MSF
		case 0x47:
			PlayAudioMSF();
			return;

		// PLAY AUDIO TRACK
		case 0x48:
			PlayAudioTrack();
			return;
	}

	// Others not supported
	LOG1(Log::Warning, "Unsupported command $%02X", scsi.cmd[0]);
	scsi.disk[scsi.id]->InvalidCmd();

	// Error
	Error();
}

//---------------------------------------------------------------------------
//
//	Message in phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::MsgIn()
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Message in phase");
#endif	// SCSI_LOG

	// Set phase
	scsi.phase = msgin;

	// Signal lines operated by target
	scsi.msg = TRUE;
	scsi.cd = TRUE;
	scsi.io = TRUE;

	// Data transfer is 1 byte x 1 block
	scsi.offset = 0;
	scsi.length = 1;
	scsi.blocks = 1;

	// Request message
	scsi.req = TRUE;
}

//---------------------------------------------------------------------------
//
//	Message out phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::MsgOut()
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Message out phase");
#endif	// SCSI_LOG

	// Set phase
	scsi.phase = msgout;

	// Signal lines operated by target
	scsi.msg = TRUE;
	scsi.cd = TRUE;
	scsi.io = FALSE;

	// Data transfer is 1 byte x 1 block
	scsi.offset = 0;
	scsi.length = 1;
	scsi.blocks = 1;

	// Request message
	scsi.req = TRUE;
}

//---------------------------------------------------------------------------
//
//	Data in phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::DataIn()
{
	ASSERT(this);
	ASSERT(scsi.length >= 0);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Data in phase");
#endif	// SCSI_LOG

	// If length 0, go to status phase
	if (scsi.length == 0) {
		Status();
		return;
	}

	// Set phase
	scsi.phase = datain;

	// Signal lines operated by target
	scsi.msg = FALSE;
	scsi.cd = FALSE;
	scsi.io = TRUE;

	// length, blocks already set
	ASSERT(scsi.length > 0);
	ASSERT(scsi.blocks > 0);
	scsi.offset = 0;

	// Request data
	scsi.req = TRUE;
}

//---------------------------------------------------------------------------
//
//	Data out phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::DataOut()
{
	ASSERT(this);
	ASSERT(scsi.length >= 0);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Data out phase");
#endif	// SCSI_LOG

	// If length 0, go to status phase
	if (scsi.length == 0) {
		Status();
		return;
	}

	// Set phase
	scsi.phase = dataout;

	// Signal lines operated by target
	scsi.msg = FALSE;
	scsi.cd = FALSE;
	scsi.io = FALSE;

	// length, blocks already set
	ASSERT(scsi.length > 0);
	ASSERT(scsi.blocks > 0);
	scsi.offset = 0;

	// Request data
	scsi.req = TRUE;
}

//---------------------------------------------------------------------------
//
//	Status phase
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Status()
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Status phase");
#endif	// SCSI_LOG

	// Set phase
	scsi.phase = status;

	// Signal lines operated by target
	scsi.msg = FALSE;
	scsi.cd = TRUE;
	scsi.io = TRUE;

	// Data transfer is 1 byte x 1 block
	scsi.offset = 0;
	scsi.length = 1;
	scsi.blocks = 1;

	// Request status
	scsi.req = TRUE;
}

//---------------------------------------------------------------------------
//
//	Interrupt request
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Interrupt(int type, BOOL flag)
{
	DWORD ints;

	ASSERT(this);
	ASSERT((type >= 0) && (type <= 7));
	ASSERT_DIAG();

	// Backup INTS
	ints = scsi.ints;

	// Set INTS register
	if (flag) {
		// Interrupt request
		scsi.ints |= (1 << type);
	}
	else {
		// Cancel interrupt
		scsi.ints &= ~(1 << type);
	}

	// If changed, check interrupt
	if (ints != scsi.ints) {
		IntCheck();
	}
}

//---------------------------------------------------------------------------
//
//	Interrupt check
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::IntCheck()
{
	int v;
	int lev;

	ASSERT(this);
	ASSERT_DIAG();

	// Set interrupt level
	if (scsi.type == 1) {
		// External (level selectable from 2 or 4)
		lev = scsi.ilevel;
	}
	else {
		// Internal (level fixed at 1)
		lev = 1;
	}

	// Create request vector
	v = -1;
	if (scsi.sctl & 0x01) {
		// Interrupt enabled
		if (scsi.ints != 0) {
			// Interrupt request present. Create vector
			if (scsi.type == 1) {
				// External (vector is $F6)
				v = 0xf6;
			}
			else {
				// Internal (vector is $6C)
				v = 0x6c;
			}
		}
	}

	// If matching, OK
	if (scsi.vector == v) {
		return;
	}

	// If interrupt already requested, cancel
	if (scsi.vector >= 0) {
		cpu->IntCancel(lev);
	}

	// If cancel request, done
	if (v < 0) {
#if defined(SCSI_LOG)
		LOG2(Log::Normal, "Cancel interrupt level %d vector $%02X",
							lev, scsi.vector);
#endif	// SCSI_LOG
		scsi.vector = -1;
		return;
	}

	// Interrupt request
#if defined(SCSI_LOG)
	LOG3(Log::Normal, "Interrupt request level %d vector $%02X INTS=%02X",
						lev, v, scsi.ints);
#endif	// SCSI_LOG
	cpu->Interrupt(lev, v);
	scsi.vector = v;
}

//---------------------------------------------------------------------------
//
//	Interrupt ACK
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::IntAck(int level)
{
	ASSERT(this);
	ASSERT((level == 1) || (level == 2) || (level == 4));
	ASSERT_DIAG();

	// Interrupt from CPU incorrectly after reset, or other device
	if (scsi.vector < 0) {
		return;
	}

	// If interrupt level differs, other device
	if (scsi.type == 1) {
		// External (level selectable from 2 or 4)
		if (level != scsi.ilevel) {
			return;
		}
	}
	else {
		// Internal (level fixed at 1)
		if (level != 1) {
			return;
		}
	}

	// No interrupt
	scsi.vector = -1;

	// Interrupt check
	IntCheck();
}

//---------------------------------------------------------------------------
//
//	Get SCSI-ID
//
//---------------------------------------------------------------------------
int FASTCALL SCSI::GetSCSIID() const
{
	int id;
	DWORD bdid;

	ASSERT(this);
	ASSERT_DIAG();

	// If not present, fixed at 7
	if (scsi.type == 0) {
		return 7;
	}

	// Create from BDID
	ASSERT(scsi.bdid != 0);
	ASSERT(scsi.bdid < 0x100);

	// Initialize
	id = 0;
	bdid = scsi.bdid;

	// Convert from bit to number
	while ((bdid & 1) == 0) {
		bdid >>= 1;
		id++;
	}

	ASSERT((id >= 0) && (id <= 7));
	return id;
}

//---------------------------------------------------------------------------
//
//	BUSY check
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::IsBusy() const
{
	ASSERT(this);
	ASSERT_DIAG();

	// If BSY not set, FALSE
	if (!scsi.bsy) {
		return FALSE;
	}

	// If selection phase, FALSE
	if (scsi.phase == selection) {
		return FALSE;
	}

	// BUSY
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	BUSY check
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSI::GetBusyDevice() const
{
	ASSERT(this);
	ASSERT_DIAG();

	// If BSY not set, 0
	if (!scsi.bsy) {
		return 0;
	}

	// If selection phase, 0
	if (scsi.phase == selection) {
		return 0;
	}

	// Selection finished, so normally device exists
	ASSERT((scsi.id >= 0) && (scsi.id <= 7));
	ASSERT(scsi.disk[scsi.id]);

	// If no device, 0
	if (!scsi.disk[scsi.id]) {
		return 0;
	}

	return scsi.disk[scsi.id]->GetID();
}

//---------------------------------------------------------------------------
//
//	Common error
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Error()
{
	ASSERT(this);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "Error (to status phase)");
#endif	// SCSI_LOG

	// Set status and message (CHECK CONDITION)
	scsi.status = 0x02;
	scsi.message = 0x00;

	// Stop event
	event.SetTime(0);

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::TestUnitReady()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "TEST UNIT READY command");
#endif	// SCSI_LOG

	// Process command in drive
	status = scsi.disk[scsi.id]->TestUnitReady(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	REZERO UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Rezero()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "REZERO UNIT command");
#endif	// SCSI_LOG

	// Process command in drive
	status = scsi.disk[scsi.id]->Rezero(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::RequestSense()
{
	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "REQUEST SENSE command");
#endif	// SCSI_LOG

	// Process command in drive
	scsi.length = scsi.disk[scsi.id]->RequestSense(scsi.cmd, scsi.buffer);
	ASSERT(scsi.length > 0);

#if defined(SCSI_LOG)
	LOG1(Log::Normal, "Sense key $%02X", scsi.buffer[2]);
#endif

	// Data in phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	FORMAT UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Format()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "FORMAT UNIT command");
#endif	// SCSI_LOG

	// Process command in drive
	status = scsi.disk[scsi.id]->Format(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	REASSIGN BLOCKS
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Reassign()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "REASSIGN BLOCKS command");
#endif	// SCSI_LOG

	// Process command in drive
	status = scsi.disk[scsi.id]->Reassign(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	READ(6)
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Read6()
{
	DWORD record;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// Get record number and block count
	record = scsi.cmd[1] & 0x1f;
	record <<= 8;
	record |= scsi.cmd[2];
	record <<= 8;
	record |= scsi.cmd[3];
	scsi.blocks = scsi.cmd[4];
	if (scsi.blocks == 0) {
		scsi.blocks = 0x100;
	}

#if defined(SCSI_LOG)
	LOG2(Log::Normal, "READ(6) command record=%06X block=%d", record, scsi.blocks);
#endif	// SCSI_LOG

	// Process command in drive
	scsi.length = scsi.disk[scsi.id]->Read(scsi.buffer, record);
	if (scsi.length <= 0) {
		// Failed (error)
		Error();
		return;
	}

	// Set next block
	scsi.next = record + 1;

	// Data in phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	WRITE(6)
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Write6()
{
	DWORD record;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// Get record number and block count
	record = scsi.cmd[1] & 0x1f;
	record <<= 8;
	record |= scsi.cmd[2];
	record <<= 8;
	record |= scsi.cmd[3];
	scsi.blocks = scsi.cmd[4];
	if (scsi.blocks == 0) {
		scsi.blocks = 0x100;
	}

#if defined(SCSI_LOG)
	LOG2(Log::Normal, "WRITE(6) command record=%06X block=%d", record, scsi.blocks);
#endif	// SCSI_LOG

	// Process command in drive
	scsi.length = scsi.disk[scsi.id]->WriteCheck(record);
	if (scsi.length <= 0) {
		// Failed (error)
		Error();
		return;
	}

	// Set next block
	scsi.next = record + 1;

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	SEEK(6)
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Seek6()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "SEEK(6) command");
#endif	// SCSI_LOG

	// Process command in drive
	status = scsi.disk[scsi.id]->Seek(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Inquiry()
{
	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "INQUIRY command");
#endif	// SCSI_LOG

	// Process command in drive
	scsi.length = scsi.disk[scsi.id]->Inquiry(scsi.cmd, scsi.buffer);
	if (scsi.length <= 0) {
		// Failed (error)
		Error();
		return;
	}

	// Data in phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::ModeSelect()
{
	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "MODE SELECT command");
#endif	// SCSI_LOG

	// Process command in drive
	scsi.length = scsi.disk[scsi.id]->SelectCheck(scsi.cmd);
	if (scsi.length <= 0) {
		// Failed (error)
		Error();
		return;
	}

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	MODE SENSE
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::ModeSense()
{
	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "MODE SENSE command");
#endif	// SCSI_LOG

	// Process command in drive
	scsi.length = scsi.disk[scsi.id]->ModeSense(scsi.cmd, scsi.buffer);
	ASSERT(scsi.length >= 0);
	if (scsi.length == 0) {
		LOG1(Log::Warning, "Unsupported MODE SENSE page $%02X", scsi.cmd[2]);

		// Failed (error)
		Error();
		return;
	}

	// Data in phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	START STOP UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::StartStop()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "START STOP UNIT command");
#endif	// SCSI_LOG

	// Process command in drive
	status = scsi.disk[scsi.id]->StartStop(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	SEND DIAGNOSTIC
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::SendDiag()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "SEND DIAGNOSTIC command");
#endif	// SCSI_LOG

	// Process command in drive
	status = scsi.disk[scsi.id]->SendDiag(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	PREVENT/ALLOW MEDIUM REMOVAL
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Removal()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "PREVENT/ALLOW MEDIUM REMOVAL command");
#endif	// SCSI_LOG

	// Process command in drive
	status = scsi.disk[scsi.id]->Removal(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	READ CAPACITY
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::ReadCapacity()
{
	int length;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "READ CAPACITY command");
#endif	// SCSI_LOG

	// Process command in drive
	length = scsi.disk[scsi.id]->ReadCapacity(scsi.cmd, scsi.buffer);
	ASSERT(length >= 0);
	if (length <= 0) {
		Error();
		return;
	}

	// Set length
	scsi.length = length;

	// Data in phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	READ(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Read10()
{
	DWORD record;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// Get record number and block count
	record = scsi.cmd[2];
	record <<= 8;
	record |= scsi.cmd[3];
	record <<= 8;
	record |= scsi.cmd[4];
	record <<= 8;
	record |= scsi.cmd[5];
	scsi.blocks = scsi.cmd[7];
	scsi.blocks <<= 8;
	scsi.blocks |= scsi.cmd[8];

#if defined(SCSI_LOG)
	LOG2(Log::Normal, "READ(10) command record=%08X block=%d", record, scsi.blocks);
#endif	// SCSI_LOG

	// Block count 0 is not processed
	if (scsi.blocks == 0) {
		Status();
		return;
	}

	// Process command in drive
	scsi.length = scsi.disk[scsi.id]->Read(scsi.buffer, record);
	if (scsi.length <= 0) {
		// Failed (error)
		Error();
		return;
	}

	// Set next block
	scsi.next = record + 1;

	// Data in phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	WRITE(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Write10()
{
	DWORD record;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// Get record number and block count
	record = scsi.cmd[2];
	record <<= 8;
	record |= scsi.cmd[3];
	record <<= 8;
	record |= scsi.cmd[4];
	record <<= 8;
	record |= scsi.cmd[5];
	scsi.blocks = scsi.cmd[7];
	scsi.blocks <<= 8;
	scsi.blocks |= scsi.cmd[8];

#if defined(SCSI_LOG)
	LOG2(Log::Normal, "WRITE(10) command record=%08X block=%d", record, scsi.blocks);
#endif	// SCSI_LOG

	// Block count 0 is not processed
	if (scsi.blocks == 0) {
		Status();
		return;
	}

	// Process command in drive
	scsi.length = scsi.disk[scsi.id]->WriteCheck(record);
	if (scsi.length <= 0) {
		// Failed (error)
		Error();
		return;
	}

	// Set next block
	scsi.next = record + 1;

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	SEEK(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Seek10()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

#if defined(SCSI_LOG)
	LOG0(Log::Normal, "SEEK(10) command");
#endif	// SCSI_LOG

	// Process command in drive
	status = scsi.disk[scsi.id]->Seek(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	VERIFY
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Verify()
{
	BOOL status;
	DWORD record;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// Get record number and block count
	record = scsi.cmd[2];
	record <<= 8;
	record |= scsi.cmd[3];
	record <<= 8;
	record |= scsi.cmd[4];
	record <<= 8;
	record |= scsi.cmd[5];
	scsi.blocks = scsi.cmd[7];
	scsi.blocks <<= 8;
	scsi.blocks |= scsi.cmd[8];

#if defined(SCSI_LOG)
	LOG2(Log::Normal, "VERIFY command record=%08X block=%d", record, scsi.blocks);
#endif	// SCSI_LOG

	// Block count 0 is not processed
	if (scsi.blocks == 0) {
		Status();
		return;
	}

	// If BytChk=0
	if ((scsi.cmd[1] & 0x02) == 0) {
		// Process command in drive
		status = scsi.disk[scsi.id]->Seek(scsi.cmd);
		if (!status) {
			// Failed (error)
			Error();
			return;
		}

		// Status phase
		Status();
		return;
	}

	// Test read
	scsi.length = scsi.disk[scsi.id]->Read(scsi.buffer, record);
	if (scsi.length <= 0) {
		// Failed (error)
		Error();
		return;
	}

	// Set next block
	scsi.next = record + 1;

	// Data out phase
	DataOut();
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::ReadToc()
{
	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// Process command in drive
	scsi.length = scsi.disk[scsi.id]->ReadToc(scsi.cmd, scsi.buffer);
	if (scsi.length <= 0) {
		// Failed (error)
		Error();
		return;
	}

	// Data in phase
	DataIn();
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO(10)
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::PlayAudio10()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// Process command in drive
	status = scsi.disk[scsi.id]->PlayAudio(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Start CD-DA event (26666 x 74 + 26716 = 1 second total)
	if (cdda.GetTime() == 0) {
		cdda.SetTime(26666);
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO MSF
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::PlayAudioMSF()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// Process command in drive
	status = scsi.disk[scsi.id]->PlayAudioMSF(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Start CD-DA event (26666 x 74 + 26716 = 1 second total)
	if (cdda.GetTime() == 0) {
		cdda.SetTime(26666);
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO TRACK
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::PlayAudioTrack()
{
	BOOL status;

	ASSERT(this);
	ASSERT(scsi.disk[scsi.id]);
	ASSERT_DIAG();

	// Process command in drive
	status = scsi.disk[scsi.id]->PlayAudioTrack(scsi.cmd);
	if (!status) {
		// Failed (error)
		Error();
		return;
	}

	// Start CD-DA event (26666 x 74 + 26716 = 1 second total)
	if (cdda.GetTime() == 0) {
		cdda.SetTime(26666);
	}

	// Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	Rebuild disks
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Construct()
{
	int hd;
	BOOL mo;
	BOOL cd;
	int i;
	int init;
	int index;
	Filepath path;

	ASSERT(this);
	ASSERT_DIAG();

	// Clear first
	hd = 0;
	mo = FALSE;
	cd = FALSE;
	for (i=0; i<DeviceMax; i++) {
		scsi.disk[i] = NULL;
	}

	// If SCSI not present, delete disks and exit
	if (scsi.type == 0) {
		for (i=0; i<HDMax; i++) {
			if (scsi.hd[i]) {
				delete scsi.hd[i];
				scsi.hd[i] = NULL;
			}
			if (scsi.mo) {
				delete scsi.mo;
				scsi.mo = NULL;
			}
			if (scsi.cdrom) {
				delete scsi.cdrom;
				scsi.cdrom = NULL;
			}
		}
		return;
	}

	// Determine disk count
	switch (scsi.scsi_drives) {
		// 0 drives
		case 0:
			break;

		// 1 drive
		case 1:
			// MO priority or HD priority
			if (scsi.mo_first) {
				mo = TRUE;
			}
			else {
				hd = 1;
			}
			break;

		// 2 drives
		case 2:
			// 1 HD, 1 MO
			hd = 1;
			mo = TRUE;
			break;

		// 3 drives
		case 3:
			// 1 HD, 1 MO, 1 CD
			hd = 1;
			mo = TRUE;
			cd = TRUE;

		// 4 or more drives
		default:
			ASSERT(scsi.scsi_drives <= 7);
			hd = scsi.scsi_drives - 2;
			mo = TRUE;
			cd = TRUE;
			break;
	}

	// Create HD
	for (i=0; i<HDMax; i++) {
		// If count exceeded, delete
		if (i >= hd) {
			if (scsi.hd[i]) {
				delete scsi.hd[i];
				scsi.hd[i] = NULL;
			}
			continue;
		}

		// Check disk type
		if (scsi.hd[i]) {
			if (scsi.hd[i]->GetID() != MAKEID('S', 'C', 'H', 'D')) {
				delete scsi.hd[i];
				scsi.hd[i] = NULL;
			}
		}

		// If disk exists
		if (scsi.hd[i]) {
			// Get path, if match then OK
			scsi.hd[i]->GetPath(path);
			if (path.CmpPath(scsihd[i])) {
				// Path matches
				continue;
			}

			// Path differs, delete disk temporarily
			delete scsi.hd[i];
			scsi.hd[i] = NULL;
		}

		// Create SCSI hard disk and try to open
		scsi.hd[i] = new SCSIHD(this);
		if (!scsi.hd[i]->Open(scsihd[i])) {
			// Open failed. Set scsi.disk for this number to NULL
			delete scsi.hd[i];
			scsi.hd[i] = NULL;
		}
	}

	// Create MO
	if (mo) {
		// MO present
		if (scsi.mo) {
			if (scsi.mo->GetID() != MAKEID('S', 'C', 'M', 'O')) {
				delete scsi.mo;
				scsi.mo = NULL;
			}
		}
		if (!scsi.mo) {
			scsi.mo = new SCSIMO(this);
		}
	}
	else {
		// No MO
		if (scsi.mo) {
			delete scsi.mo;
			scsi.mo = NULL;
		}
	}

	// Create CD-ROM
	if (cd) {
		// CD-ROM present
		if (scsi.cdrom) {
			if (scsi.cdrom->GetID() != MAKEID('S', 'C', 'C', 'D')) {
				delete scsi.cdrom;
				scsi.cdrom = NULL;
			}
		}
		if (!scsi.cdrom) {
			scsi.cdrom = new SCSICD(this);
		}
	}
	else {
		// No CD-ROM
		if (scsi.cdrom) {
			delete scsi.cdrom;
			scsi.cdrom = NULL;
		}
	}

	// Arrange disks considering host/MO priority
	init = GetSCSIID();
	index = 0;
	for (i=0; i<DeviceMax; i++) {
		// Skip host (initiator)
		if (i == init) {
			continue;
		}

		// If MO priority, place MO first
		if (scsi.mo_first) {
			if (mo) {
				ASSERT(scsi.mo);
				scsi.disk[i] = scsi.mo;
				mo = FALSE;
				continue;
			}
		}

		// Place HD
		if (index < hd) {
			// NULL is OK
			scsi.disk[i] = scsi.hd[index];
			index++;
			continue;
		}

		// Place MO
		if (mo) {
			ASSERT(scsi.mo);
			scsi.disk[i] = scsi.mo;
			mo = FALSE;
			continue;
		}
	}

	// CD is fixed at ID6. If ID6 full, ID7
	if (cd) {
		ASSERT(scsi.cdrom);
		if (!scsi.disk[6]) {
			scsi.disk[6] = scsi.cdrom;
		}
		else {
			ASSERT(!scsi.disk[7]);
			scsi.disk[7] = scsi.cdrom;
		}
	}

#if defined(SCSI_LOG)
	for (i=0; i<DeviceMax; i++) {
		if (!scsi.disk[i]) {
			LOG1(Log::Detail, "ID=%d : Not connected or initiator", i);
			continue;
		}
		switch (scsi.disk[i]->GetID()) {
			case MAKEID('S', 'C', 'H', 'D'):
				LOG1(Log::Detail, "ID=%d : SCSI hard disk", i);
				break;
			case MAKEID('S', 'C', 'M', 'O'):
				LOG1(Log::Detail, "ID=%d : SCSI MO disk", i);
				break;
			case MAKEID('S', 'C', 'C', 'D'):
				LOG1(Log::Detail, "ID=%d : SCSI CD-ROM drive", i);
				break;
			default:
				LOG2(Log::Detail, "ID=%d : Undefined(%08X)", i, scsi.disk[i]->GetID());
				break;
		}
	}
#endif	// SCSI_LOG
}

//---------------------------------------------------------------------------
//
//	MO/CD open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::Open(const Filepath& path, BOOL mo)
{
	ASSERT(this);
	ASSERT_DIAG();

	// If not valid, error
	if (!IsValid(mo)) {
		return FALSE;
	}

	// Eject
	Eject(FALSE, mo);

	// If not not ready, error
	if (IsReady(mo)) {
		return FALSE;
	}

	// Leave to drive
	if (mo) {
		ASSERT(scsi.mo);
		if (scsi.mo->Open(path, TRUE)) {
			// Success (MO)
			return TRUE;
		}
	}
	else {
		ASSERT(scsi.cdrom);
		if (scsi.cdrom->Open(path, TRUE)) {
			// Success (CD)
			return TRUE;
		}
	}

	// Failed
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	MO/CD eject
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::Eject(BOOL force, BOOL mo)
{
	ASSERT(this);
	ASSERT_DIAG();

	// If not ready, do nothing
	if (!IsReady(mo)) {
		return;
	}

	// Notify drive
	if (mo) {
		ASSERT(scsi.mo);
		scsi.mo->Eject(force);
	}
	else {
		ASSERT(scsi.cdrom);
		scsi.cdrom->Eject(force);
	}
}

//---------------------------------------------------------------------------
//
//	MO write protect
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::WriteP(BOOL flag)
{
	ASSERT(this);
	ASSERT_DIAG();

	// If not ready, do nothing
	if (!IsReady()) {
		return;
	}

	// Try to set write protect
	ASSERT(scsi.mo);
	scsi.mo->WriteP(flag);
}

//---------------------------------------------------------------------------
//
//	Get MO write protect
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::IsWriteP() const
{
	ASSERT(this);
	ASSERT_DIAG();

	// If not ready, not write protected
	if (!IsReady()) {
		return FALSE;
	}

	// Ask drive
	ASSERT(scsi.mo);
	return scsi.mo->IsWriteP();
}

//---------------------------------------------------------------------------
//
//	MO read-only check
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::IsReadOnly() const
{
	ASSERT(this);
	ASSERT_DIAG();

	// If not ready, not read-only
	if (!IsReady()) {
		return FALSE;
	}

	// Ask drive
	ASSERT(scsi.mo);
	return scsi.mo->IsReadOnly();
}

//---------------------------------------------------------------------------
//
//	MO/CD locked check
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::IsLocked(BOOL mo) const
{
	ASSERT(this);
	ASSERT_DIAG();

	if (mo) {
		// Does drive exist (MO)
		if (!scsi.mo) {
			return FALSE;
		}

		// Ask drive (MO)
		return scsi.mo->IsLocked();
	}
	else {
		// Does drive exist (CD)
		if (!scsi.cdrom) {
			return FALSE;
		}

		// Ask drive (CD)
		return scsi.cdrom->IsLocked();
	}
}

//---------------------------------------------------------------------------
//
//	MO/CD ready check
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::IsReady(BOOL mo) const
{
	ASSERT(this);
	ASSERT_DIAG();

	if (mo) {
		// Does drive exist (MO)
		if (!scsi.mo) {
			return FALSE;
		}

		// Ask drive (MO)
		return scsi.mo->IsReady();
	}
	else {
		// Does drive exist (CD)
		if (!scsi.cdrom) {
			return FALSE;
		}

		// Ask drive (CD)
		return scsi.cdrom->IsReady();
	}
}

//---------------------------------------------------------------------------
//
//	MO/CD valid check
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSI::IsValid(BOOL mo) const
{
	ASSERT(this);
	ASSERT_DIAG();

	if (mo) {
		// Determine by pointer (MO)
		if (scsi.mo) {
			return TRUE;
		}
	}
	else {
		// Determine by pointer (CD)
		if (scsi.cdrom) {
			return TRUE;
		}
	}

	// No drive
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Get MO/CD path
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::GetPath(Filepath& path, BOOL mo) const
{
	ASSERT(this);
	ASSERT_DIAG();

	if (mo) {
		// MO
		if (scsi.mo) {
			// Is ready?
			if (scsi.mo->IsReady()) {
				// Get path
				scsi.mo->GetPath(path);
				return;
			}
		}
	}
	else {
		// CD
		if (scsi.cdrom) {
			// Is ready?
			if (scsi.cdrom->IsReady()) {
				// Get path
				scsi.cdrom->GetPath(path);
				return;
			}
		}
	}

	// Not a valid path, clear
	path.Clear();
}

//---------------------------------------------------------------------------
//
//	Get CD-DA buffer
//
//---------------------------------------------------------------------------
void FASTCALL SCSI::GetBuf(DWORD *buffer, int samples, DWORD rate)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT(samples >= 0);
	ASSERT(rate != 0);
	ASSERT((rate % 100) == 0);
	ASSERT_DIAG();

	// Interface is valid
	if (scsi.type != 0) {
		// Only if CD-ROM exists
		if (scsi.cdrom) {
			// Request from CD-ROM
			scsi.cdrom->GetBuf(buffer, samples, rate);
		}
	}
}
