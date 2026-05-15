//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//Copyright (C) 2001-2006  PI.(ytanaka@ipc-tokai.or.jp)
//	[ SASI ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "fileio.h"
#include "dmac.h"
#include "iosc.h"
#include "sram.h"
#include "config.h"
#include "disk.h"
#include "memory.h"
#include "scsi.h"
#include "sasi.h"

//===========================================================================
//
//	SASI
//
//===========================================================================
//#define SASI_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SASI::SASI(VM *p) : MemDevice(p)
{
	//Initialize device ID
	dev.id = MAKEID('S', 'A', 'S', 'I');
	dev.desc = "SASI (IOSC-2)";

	//Start address, end address
	memdev.first = 0xe96000;
	memdev.last = 0xe97fff;
}

//---------------------------------------------------------------------------
//
//Init
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASI::Init()
{
	int i;

	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	//Get DMAC
	dmac = (DMAC*)vm->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	ASSERT(dmac);

	//Get IOSC
	iosc = (IOSC*)vm->SearchDevice(MAKEID('I', 'O', 'S', 'C'));
	ASSERT(iosc);

	//Get SRAM
	sram = (SRAM*)vm->SearchDevice(MAKEID('S', 'R', 'A', 'M'));
	ASSERT(sram);

	//Get SCSI
	scsi = (SCSI*)vm->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(scsi);

	//Initialize internal data
	memset(&sasi, 0, sizeof(sasi));
	sxsicpu = FALSE;

	// Create disk
	for (i=0; i<SASIMax; i++) {
		sasi.disk[i] = new Disk(this);
	}

	// No current
	sasi.current = NULL;

	// Create event
	event.SetDevice(this);
	event.SetDesc("Data Transfer");
	event.SetUser(0);
	event.SetTime(0);
	scheduler->AddEvent(&event);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Cleanup()
{
	int i;

	ASSERT(this);

	// Delete disk
	for (i=0; i<SASIMax; i++) {
		if (sasi.disk[i]) {
			delete sasi.disk[i];
			sasi.disk[i] = NULL;
		}
	}

	// To base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Reset()
{
	Memory *memory;
	Memory::memtype type;
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "Reset");

	//SCSI type (Query memory)
	memory = (Memory*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(memory);
	type = memory->GetMemType();
	switch (type) {
		// SASI only
		case Memory::None:
		case Memory::SASI:
			sasi.scsi_type = 0;
			break;

		// External
		case Memory::SCSIExt:
			sasi.scsi_type = 1;
			break;

		//Other (Internal)
		default:
			sasi.scsi_type = 2;
			break;
	}

	//When using SCSI, exclusive with SxSI (SxSI forbidden)
	if (sasi.scsi_type != 0) {
		if (sasi.sxsi_drives != 0) {
			sasi.sxsi_drives = 0;
			Construct();
		}
	}

	//Memory switch write
	if (sasi.memsw) {
		// Set only when SASI exists
		if (sasi.scsi_type < 2) {
			//$ED005A: SASI disk count
			sram->SetMemSw(0x5a, sasi.sasi_drives);
		}
		else {
			//No SASI interface, so 0
			sram->SetMemSw(0x5a, 0x00);
		}

		// Clear only when SCSI does not exist
		if (sasi.scsi_type == 0) {
			//$ED006F: SCSI flag ('V' enables SCSI)
			sram->SetMemSw(0x6f, 0x00);
			//$ED0070: SCSI type (Internal/External) + Internal SCSI ID
			sram->SetMemSw(0x70, 0x07);
			//$ED0071: SASI emulation flag by SCSI
			sram->SetMemSw(0x71, 0x00);
		}
	}

	// EventReset
	// Reset attached disks/state so a VM reset behaves like a real bus reset.
	for (i=0; i<SASIMax; i++) {
		if (sasi.disk[i]) {
			sasi.disk[i]->Reset();
		}
	}

	for (i=0; i<(int)(sizeof(sasi.cmd) / sizeof(sasi.cmd[0])); i++) {
		sasi.cmd[i] = 0;
	}

	// Clear transfer state.
	sasi.blocks = 0;
	sasi.next = 0;
	sasi.offset = 0;
	sasi.length = 0;
	sasi.status = 0;
	sasi.message = 0;
	sasi.ctrl = 0;

	event.SetUser(0);
	event.SetTime(0);

	// BusReset
	BusFree();

	// No current device
	sasi.current = NULL;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASI::Save(Fileio *fio, int ver)
{
	size_t sz;
	int i;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Save");

	// Flush disk
	for (i=0; i<SASIMax; i++) {
		ASSERT(sasi.disk[i]);
		if (!sasi.disk[i]->Flush()) {
			return FALSE;
		}
	}

	// Size to Save
	sz = sizeof(sasi_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save entity
	if (!fio->Write(&sasi, (int)sz)) {
		return FALSE;
	}

	// Save event
	if (!event.Save(fio, ver)) {
		return FALSE;
	}

	// Save path
	for (i=0; i<SASIMax; i++) {
		if (!sasihd[i].Save(fio, ver)) {
			return FALSE;
		}
	}
	for (i=0; i<SCSIMax; i++) {
		if (!scsihd[i].Save(fio, ver)) {
			return FALSE;
		}
	}
	if (!scsimo.Save(fio, ver)) {
		return FALSE;
	}

	// version2.02 extension
	if (!fio->Write(&sxsicpu, sizeof(sxsicpu))) {
		return FALSE;
	}

	// version2.03 extension
	for (i=0; i<SASIMax; i++) {
		ASSERT(sasi.disk[i]);
		if (!sasi.disk[i]->Save(fio, ver)) {
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
BOOL FASTCALL SASI::Load(Fileio *fio, int ver)
{
	sasi_t buf;
	size_t sz;
	int i;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Load");

	// Load size, verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(sasi_t)) {
		return FALSE;
	}

	// Load entity to buffer
	if (!fio->Read(&buf, (int)sz)) {
		return FALSE;
	}

	//Initialize disk
	for (i=0; i<SASIMax; i++) {
		ASSERT(sasi.disk[i]);
		delete sasi.disk[i];
		sasi.disk[i] = new Disk(this);
	}

	// Move pointer
	for (i=0; i<SASIMax; i++) {
		buf.disk[i] = sasi.disk[i];
	}

	// Move
	sasi = buf;
	sasi.mo = NULL;
	sasi.current = NULL;

	// Load event
	if (!event.Load(fio, ver)) {
		return FALSE;
	}

	// Load path
	for (i=0; i<SASIMax; i++) {
		if (!sasihd[i].Load(fio, ver)) {
			return FALSE;
		}
	}
	for (i=0; i<SCSIMax; i++) {
		if (!scsihd[i].Load(fio, ver)) {
			return FALSE;
		}
	}
	if (!scsimo.Load(fio, ver)) {
		return FALSE;
	}

	// version2.02 extension
	sxsicpu = FALSE;
	if (ver >= 0x0202) {
		if (!fio->Read(&sxsicpu, sizeof(sxsicpu))) {
			return FALSE;
		}
	}

	// Recreate disk
	Construct();

	// version2.03 extension
	if (ver >= 0x0203) {
		for (i=0; i<SASIMax; i++) {
			ASSERT(sasi.disk[i]);
			if (!sasi.disk[i]->Load(fio, ver)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL SASI::ApplyCfg(const Config *config)
{
	int i;

	ASSERT(this);
	ASSERT(config);

	LOG0(Log::Normal, "Apply settings");

	//SASI drive count
	sasi.sasi_drives = config->sasi_drives;

	//Auto update memory switch
	sasi.memsw = config->sasi_sramsync;

	//SASI file name
	for (i=0; i<SASIMax; i++) {
		sasihd[i].SetPath(config->sasi_file[i]);
	}

	// Add parity circuit
	sasi.parity = config->sasi_parity;

	//SCSI drive count
	sasi.sxsi_drives = config->sxsi_drives;

	//SCSI file name
	for (i=0; i<SCSIMax; i++) {
		scsihd[i].SetPath(config->sxsi_file[i]);
	}

	// MO priority flag
	sasi.mo_first = config->sxsi_mofirst;

	// Rebuild disk
	Construct();
}

//---------------------------------------------------------------------------
//
//Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SASI::ReadByte(DWORD addr)
{
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	//Is SCSI internal
	if (sasi.scsi_type >= 2) {
		//Loop in 0x40 units
		addr &= 0x3f;

		//Area check
		if (addr >= 0x20) {
			//SCSI area
			return scsi->ReadByte(addr - 0x20);
		}
		if ((addr & 1) == 0) {
			//Even bytes are not decoded
			return 0xff;
		}
		addr &= 0x07;
		if (addr >= 4) {
			return 0xff;
		}
		return 0;
	}

	//Odd addresses only
	if (addr & 1) {
		//Loop in 8-byte units
		addr &= 0x07;

		//Wait
		scheduler->Wait(1);

		//Data register
		if (addr == 1) {
			return ReadData();
		}

		//Status register
		if (addr == 3) {
			data = 0;
			if (sasi.msg) {
				data |= 0x10;
			}
			if (sasi.cd) {
				data |= 0x08;
			}
			if (sasi.io) {
				data |= 0x04;
			}
			if (sasi.bsy) {
				data |= 0x02;
			}
			if (sasi.req) {
				data |= 0x01;
			}
			return data;
		}

		//Other registers are Write Only
		return 0xff;
	}

	//Even addresses are not decoded
	return 0xff;
}

//---------------------------------------------------------------------------
//
//Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SASI::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	return (0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//Byte write
//
//---------------------------------------------------------------------------
void FASTCALL SASI::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);

	//Is SCSI internal
	if (sasi.scsi_type >= 2) {
		//Loop in 0x40 units
		addr &= 0x3f;

		//Area check
		if (addr >= 0x20) {
			//SCSI area
			scsi->WriteByte(addr - 0x20, data);
		}
		//SASI area does nothing
		return;
	}

	//Odd addresses only
	if (addr & 1) {
		//Loop in 8-byte units
		addr &= 0x07;
		addr >>= 1;

		//Wait
		scheduler->Wait(1);

		switch (addr) {
			//Data write
			case 0:
				WriteData(data);
				return;

			//SEL with data write
			case 1:
				sasi.sel = FALSE;
				WriteData(data);
				return;

			// BusReset
			case 2:
#if defined(SASI_LOG)
				LOG0(Log::Normal, "BusReset");
#endif	// SASI_LOG
				BusFree();
				return;

			//SEL with data write
			case 3:
				sasi.sel = TRUE;
				WriteData(data);
				return;
		}

		//Never comes here
		ASSERT(FALSE);
		return;
	}

	//Even addresses are not decoded
}

//---------------------------------------------------------------------------
//
//Word write
//
//---------------------------------------------------------------------------
void FASTCALL SASI::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL SASI::ReadOnly(DWORD addr) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	//Is SCSI internal
	if (sasi.scsi_type >= 2) {
		//Loop in 0x40 units
		addr &= 0x3f;

		//Area check
		if (addr >= 0x20) {
			//SCSI area
			return scsi->ReadOnly(addr - 0x20);
		}
		if ((addr & 1) == 0) {
			//Even bytes are not decoded
			return 0xff;
		}
		addr &= 0x07;
		if (addr >= 4) {
			return 0xff;
		}
		return 0;
	}

	//Odd addresses only
	if (addr & 1) {
		//Loop in 8-byte units
		addr &= 0x07;
		addr >>= 1;

		switch (addr) {
			//Data register
			case 0:
				return 0;

			//Status register
			case 1:
				data = 0;
				if (sasi.msg) {
					data |= 0x10;
				}
				if (sasi.cd) {
					data |= 0x08;
				}
				if (sasi.io) {
					data |= 0x04;
				}
				if (sasi.bsy) {
					data |= 0x02;
				}
				if (sasi.req) {
					data |= 0x01;
				}
				return data;
		}

		//Other registers are Write Only
		return 0xff;
	}

	//Even addresses are not decoded
	return 0xff;
}

//---------------------------------------------------------------------------
//
//Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASI::Callback(Event *ev)
{
	int thres;

	ASSERT(this);
	ASSERT(ev);

	switch (ev->GetUser()) {
		//Normal data: Host<->Controller
		case 0:
			//Time adjustment
			if (ev->GetTime() != 32) {
				ev->SetTime(32);
			}

			//Request
			sasi.req = TRUE;
			dmac->ReqDMA(1);

			//DMA transfer may continue
			return TRUE;

		//Block data: Controller<->Host
		case 1:
			//Reset time
			if (ev->GetTime() != 48) {
				ev->SetTime(48);
			}

			//If DMA not active, set request only
			if (!dmac->IsAct(1)) {
				if ((sasi.phase == read) || (sasi.phase == write)) {
					sasi.req = TRUE;
				}

				//Continue event
				return TRUE;
			}

			//In one event, transfer 2/3 of excess CPU power
			thres = (int)scheduler->GetCPUSpeed();
			thres = (thres * 2) / 3;
			while ((sasi.phase == read) || (sasi.phase == write)) {
				//Cut off midway watching CPU power
				if (scheduler->GetCPUCycle() > thres) {
					break;
				}

				//Request setting
				sasi.req = TRUE;

				//If SxSI CPU flag set, stop here (CPU transfer)
				if (sxsicpu) {
					LOG0(Log::Warning, "SxSI CPU転送を検出");
					break;
				}

				//DMA request
				dmac->ReqDMA(1);
				if (sasi.req) {
#if defined(SASI_LOG)
					LOG0(Log::Normal, "DMA or CPU転送待ち");
#endif
					break;
				}
			}
			return TRUE;

		//Others are impossible
		default:
			ASSERT(FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//Get internal data
//
//---------------------------------------------------------------------------
void FASTCALL SASI::GetSASI(sasi_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	//Copy internal work
	*buffer = sasi;
}

//---------------------------------------------------------------------------
//
//	Rebuild disk
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Construct()
{
	int i;
	int index;
	int scmo;
	int schd;
	int map[SASIMax];
	Filepath path;
	Filepath mopath;
	BOOL moready;
	BOOL mowritep;
	BOOL moattn;
	SASIHD *sasitmp;
	SCSIHD *scsitmp;

	ASSERT(this);
	ASSERT((sasi.sasi_drives >= 0) && (sasi.sasi_drives <= SASIMax));
	ASSERT((sasi.sxsi_drives >= 0) && (sasi.sxsi_drives <= 7));

	//Save MO drive state, then disable
	moready = FALSE;
	moattn = FALSE;
	mowritep = FALSE;
	if (sasi.mo) {
		moready = sasi.mo->IsReady();
		if (moready) {
			sasi.mo->GetPath(mopath);
			mowritep = sasi.mo->IsWriteP();
			moattn = sasi.mo->IsAttn();
		}
	}
	sasi.mo = NULL;

	//Calculate valid SCSI-MO, SCSI-HD count
	i = (sasi.sasi_drives + 1) >> 1;
	schd = 7 - i;

	//Max SCSI drives from SASI limit
	if (schd > 0) {
		//Limited by configured SCSI drive count
		if (sasi.sxsi_drives < schd) {
			//Determine SCSI drive count
			schd = sasi.sxsi_drives;
		}
	}

	//Assign MO
	if (schd > 0) {
		//If at least 1 drive, always assign MO
		scmo = 1;
		schd--;
	}
	else {
		//No drives, so MO and HD both 0
		scmo = 0;
		schd = 0;
	}

	//Without parity, SxSI cannot be used
	if (!sasi.parity) {
		scmo = 0;
		schd = 0;
	}
#if defined(SASI_LOG)
	LOG3(Log::Normal, "再割り当て SASI-HD:%d台 SCSI-HD:%d台 SCSI-MO:%d台",
					sasi.sasi_drives, schd, scmo);
#endif	// SASI_LOG

	//Create map (0:NULL 1:SASI-HD 2:SCSI-HD 3:SCSI-MO)
	for (i=0; i<SASIMax; i++) {
		//All NULL
		map[i] = 0;
	}
	for (i=0; i<sasi.sasi_drives; i++) {
		//First SASI
		map[i] = 1;
	}
	if (scmo > 0) {
		//SCSI exists
		index = ((sasi.sasi_drives + 1) >> 1) << 1;
		if (sasi.mo_first) {
			//MO priority
			map[index] = 3;
			index += 2;

			//SCSI-HD loop
			for (i=0; i<schd; i++) {
				map[index] = 2;
				index += 2;
			}
		}
		else {
			//HD priority
			for (i=0; i<schd; i++) {
				map[index] = 2;
				index += 2;
			}

			//Finally MO
			map[index] = 3;
		}
		ASSERT(index <= 16);
	}

	//Clear SCSI hard disk sequence index
	index = 0;

	//Loop
	for (i=0; i<SASIMax; i++) {
		switch (map[i]) {
			//Null disk
			case 0:
				//Is null disk
				if (!sasi.disk[i]->IsNULL()) {
					//Replace with null disk
					delete sasi.disk[i];
					sasi.disk[i] = new Disk(this);
				}
				break;

			//SASI hard disk
			case 1:
				//Is SASI hard disk
				if (sasi.disk[i]->IsSASI()) {
					//Get path and verify match
					sasi.disk[i]->GetPath(path);
					if (path.CmpPath(sasihd[i])) {
						//Path matches
						break;
					}
				}

				//Create SASI hard disk and try open
				sasitmp = new SASIHD(this);
				if (sasitmp->Open(sasihd[i])) {
					//LUN setting
					sasitmp->SetLUN(i & 1);
					//Swap
					delete sasi.disk[i];
					sasi.disk[i] = sasitmp;
				}
				else {
					//Error
					delete sasitmp;
					delete sasi.disk[i];
					sasi.disk[i] = new Disk(this);
				}
				break;

			//SCSI hard disk
			case 2:
				//Is SCSI hard disk
				if (sasi.disk[i]->GetID() == MAKEID('S', 'C', 'H', 'D')) {
					//Get path and verify match
					sasi.disk[i]->GetPath(path);
					if (path.CmpPath(scsihd[index])) {
						//Path matches
						index++;
						break;
					}
				}

				//Create SCSI hard disk and try open
				scsitmp = new SCSIHD(this);
				if (scsitmp->Open(scsihd[index])) {
					//Swap
					delete sasi.disk[i];
					sasi.disk[i] = scsitmp;
				}
				else {
					//Error
					delete scsitmp;
					delete sasi.disk[i];
					sasi.disk[i] = new Disk(this);
				}
				index++;
				break;

			//SCSI MO disk
			case 3:
				//Is SCSI MO disk
				if (sasi.disk[i]->GetID() == MAKEID('S', 'C', 'M', 'O')) {
					//Remember
					sasi.mo = (SCSIMO*)sasi.disk[i];
				}
				else {
					//Create SCSI MO disk and remember
					delete sasi.disk[i];
					sasi.disk[i] = new SCSIMO(this);
					sasi.mo = (SCSIMO*)sasi.disk[i];
				}

				//If not inherited, reopen
				if (moready) {
					if (!sasi.mo->IsReady()) {
						if (sasi.mo->Open(mopath, moattn)) {
							sasi.mo->WriteP(mowritep);
						}
					}
				}
				break;

			//Other (impossible)
			default:
				ASSERT(FALSE);
				break;
		}
	}

	//If BUSY, update sasi.current
	if (sasi.bsy) {
		//Create drive
		ASSERT(sasi.ctrl < 8);
		index = sasi.ctrl << 1;
		if (sasi.cmd[1] & 0x20) {
			index++;
		}

		//Set current
		sasi.current = sasi.disk[index];
		ASSERT(sasi.current);
	}
}

//---------------------------------------------------------------------------
//
//Check HD BUSY
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASI::IsBusy() const
{
	ASSERT(this);

	// SASI
	if (sasi.bsy) {
		return TRUE;
	}

	// SCSI
	if (scsi->IsBusy()) {
		return TRUE;
	}

	//Both FALSE
	return FALSE;
}

//---------------------------------------------------------------------------
//
//Get BUSY device
//
//---------------------------------------------------------------------------
DWORD FASTCALL SASI::GetBusyDevice() const
{
	ASSERT(this);

	// SASI
	if (sasi.bsy) {
		//From end of selection phase to start of execution phase
		//Access target device is not determined
		if (!sasi.current) {
			return 0;
		}
		if (sasi.current->IsNULL()) {
			return 0;
		}

		return sasi.current->GetID();
	}

	// SCSI
	return scsi->GetBusyDevice();
}

//---------------------------------------------------------------------------
//
//Data read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SASI::ReadData()
{
	DWORD data;

	ASSERT(this);

	switch (sasi.phase) {
		//If status phase, pass status and go to message phase
		case status:
			data = sasi.status;
			sasi.req = FALSE;

			//Also serves as interrupt reset
			iosc->IntHDC(FALSE);
			Message();

			//Stop event
			event.SetTime(0);
			return data;

		//If message phase, pass message and go to bus free
		case message:
			data = sasi.message;
			sasi.req = FALSE;
			BusFree();

			//Stop event
			event.SetTime(0);
			return data;

		//If read phase, go to status phase after transfer
		case read:
			//For Non-DMA transfer, toggle SxSI CPU flag
			if (!dmac->IsDMA()) {
#if defined(SASI_LOG)
				LOG1(Log::Normal, "データINフェーズ CPU転送 オフセット=%d", sasi.offset);
#endif	// SASI_LOG
				sxsicpu = !sxsicpu;
			}

			data = (DWORD)sasi.buffer[sasi.offset];
			sasi.offset++;
			sasi.length--;
			sasi.req = FALSE;

			//Length check
			if (sasi.length == 0) {
				//Decrement blocks to transfer
				sasi.blocks--;

				//Is this the last
				if (sasi.blocks == 0) {
					//Stop event, status phase
					event.SetTime(0);
					Status();
					return data;
				}

				//Read next block
				sasi.length = sasi.current->Read(sasi.buffer, sasi.next);
				if (sasi.length <= 0) {
					//Error
					Error();
					return data;
				}

				//Next block ok, set work
				sasi.offset = 0;
				sasi.next++;
			}

			//Continue reading
			return data;
	}

	//Stop event
	event.SetTime(0);

	//Other operations not expected
	if (sasi.phase == busfree) {
#if defined(SASI_LOG)
		LOG0(Log::Normal, "バスフリー状態での読み込み。0を返す");
#endif	// SASI_LOG
		return 0;
	}

	LOG0(Log::Warning, "想定していないデータ読み込み");
	BusFree();
	return 0;
}

//---------------------------------------------------------------------------
//
//Data write
//
//---------------------------------------------------------------------------
void FASTCALL SASI::WriteData(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	switch (sasi.phase) {
		//If bus free phase, next allowed is selection phase
		case busfree:
			if (sasi.sel) {
				Selection(data);
			}
#if defined(SASI_LOG)
			else {
				LOG0(Log::Normal, "バスフリー状態、SEL無しでの書き込み。無視");
			}
#endif	// SASI_LOG
			event.SetTime(0);
			return;

		//If selection phase, release SEL and go to command phase
		case selection:
			if (!sasi.sel) {
				Command();
				return;
			}
			event.SetTime(0);
			break;

		//If command phase, receive 6/10 bytes
		case command:
			//Reset length from first data (offset 0)
			sasi.cmd[sasi.offset] = data;
			if (sasi.offset == 0) {
				if ((data >= 0x20) && (data <= 0x3f)) {
					//10-byte CDB
					sasi.length = 10;
				}
			}
			sasi.offset++;
			sasi.length--;
			sasi.req = FALSE;

			//Done?
			if (sasi.length == 0) {
				//Stop event and go to execution phase
				event.SetTime(0);
				Execute();
				return;
			}
			//Continue event
			return;

		//If write phase, go to status phase after transfer
		case write:
			//For Non-DMA transfer, toggle SxSI CPU flag
			if (!dmac->IsDMA()) {
#if defined(SASI_LOG)
				LOG1(Log::Normal, "データOUTフェーズ CPU転送 オフセット=%d", sasi.offset);
#endif	// SASI_LOG
				sxsicpu = !sxsicpu;
			}

			sasi.buffer[sasi.offset] = (BYTE)data;
			sasi.offset++;
			sasi.length--;
			sasi.req = FALSE;

			//Done?
			if (sasi.length > 0) {
				return;
			}

			//WRITE(6), WRITE(10), WRITE&VERIFY are separate
			switch (sasi.cmd[0]) {
				case 0x0a:
				case 0x2a:
				case 0x2e:
					break;
				//Non-WRITE DATA commands end here
				default:
					event.SetTime(0);
					Status();
					return;
			}

			//If WRITE command, write with current buffer
			if (!sasi.current->Write(sasi.buffer, sasi.next - 1)) {
				//Error
				Error();
				return;
			}

			//Decrement transfer block count
			sasi.blocks--;

			//Is last block
			if (sasi.blocks == 0) {
				//Stop event and go to status phase
				event.SetTime(0);
				Status();
				return;
			}

			//Prepare next block
			sasi.length = sasi.current->WriteCheck(sasi.next);
			if (sasi.length <= 0) {
				//Error
				Error();
				return;
			}

			//Transfer next block
			sasi.next++;
			sasi.offset = 0;

			//Continue event
			return;
	}

	//Stop event
	event.SetTime(0);

	//Other operations not expected
	LOG1(Log::Warning, "想定していないデータ書き込み $%02X", data);
	BusFree();
}

//---------------------------------------------------------------------------
//
//Bus free phase
//
//---------------------------------------------------------------------------
void FASTCALL SASI::BusFree()
{
	ASSERT(this);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "バスフリーフェーズ");
#endif	// SASI_LOG

	// BusReset
	sasi.sel = FALSE;
	sasi.msg = FALSE;
	sasi.cd = FALSE;
	sasi.io = FALSE;
	sasi.bsy = FALSE;
	sasi.req = FALSE;

	//Bus free phase
	sasi.phase = busfree;

	//Stop event
	event.SetTime(0);

	//Interrupt reset
	iosc->IntHDC(FALSE);

	//SxSI CPU transfer flag
	sxsicpu = FALSE;
}

//---------------------------------------------------------------------------
//
//Selection phase
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Selection(DWORD data)
{
	int c;
	int i;

	ASSERT(this);

	//Know selected controller
	c = 8;
	for (i=0; i<8; i++) {
		if (data & 1) {
			c = i;
			break;
		}
		data >>= 1;
	}

	//If no bit, bus free phase
	if (c >= 8) {
		BusFree();
		return;
	}

#if defined(SASI_LOG)
	LOG1(Log::Normal, "セレクションフェーズ コントローラ%d", c);
#endif	// SASI_LOG

	//If drive does not exist, bus free phase
	if (sasi.disk[(c << 1) + 0]->IsNULL()) {
		if (sasi.disk[(c << 1) + 1]->IsNULL()) {
			BusFree();
			return;
		}
	}

	//Raise BSY and selection phase
	sasi.ctrl = (DWORD)c;
	sasi.phase = selection;
	sasi.bsy = TRUE;
}

//---------------------------------------------------------------------------
//
//Command phase
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Command()
{
	ASSERT(this);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "コマンドフェーズ");
#endif	// SASI_LOG

	//Command phase
	sasi.phase = command;

	// I/O=0, C/D=1, MSG=0
	sasi.io = FALSE;
	sasi.cd = TRUE;
	sasi.sel = FALSE;
	sasi.msg = FALSE;

	//Remaining command length is 6 bytes (some commands are 10. Reset with WriteByte)
	sasi.offset = 0;
	sasi.length = 6;

	//Request command data
	event.SetUser(0);
	event.SetTime(32);
}

//---------------------------------------------------------------------------
//
//Execution phase
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Execute()
{
	int drive;

	ASSERT(this);

	//Execution phase
	sasi.phase = execute;

	//Create drive
	ASSERT(sasi.ctrl < 8);
	drive = sasi.ctrl << 1;
	if (sasi.cmd[1] & 0x20) {
		drive++;
	}

	//Set current
	sasi.current = sasi.disk[drive];
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG2(Log::Normal, "実行フェーズ コマンド$%02X ドライブ%d", sasi.cmd[0], drive);
#endif	// SASI_LOG

#if 0
	//Output error for attention (notify MO media change)
	if (sasi.current->IsAttn()) {
#if defined(SASI_LOG)
	LOG0(Log::Normal, "メディア交換通知のためのアテンションエラー");
#endif	// SASI_LOG

		//Clear attention
		sasi.current->ClrAttn();

		//Error (request sense)
		Error();
		return;
	}
#endif

	//Commands 0x12 to 0x2f are SCSI only
	if ((sasi.cmd[0] >= 0x12) && (sasi.cmd[0] <= 0x2f)) {
		ASSERT(sasi.current);
		if (sasi.current->IsSASI()) {
			//SASI only, unimplemented command error
			sasi.current->InvalidCmd();
			Error();
			return;
		}
	}

	//Command specific processing
	switch (sasi.cmd[0]) {
		// TEST UNIT READY
		case 0x00:
			TestUnitReady();
			return;

		// REZERO UNIT
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

		// FORMAT UNIT
		case 0x06:
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

		// ASSIGN(SASI only)
		case 0x0e:
			Assign();
			return;

		//INQUIRY (SCSI only)
		case 0x12:
			Inquiry();
			return;

		//MODE SENSE (SCSI only)
		case 0x1a:
			ModeSense();
			return;

		//START STOP UNIT (SCSI only)
		case 0x1b:
			StartStop();
			return;

		//PREVENT/ALLOW MEDIUM REMOVAL (SCSI only)
		case 0x1e:
			Removal();
			return;

		//READ CAPACITY (SCSI only)
		case 0x25:
			ReadCapacity();
			return;

		//READ(10) (SCSI only)
		case 0x28:
			Read10();
			return;

		//WRITE(10) (SCSI only)
		case 0x2a:
			Write10();
			return;

		//SEEK(10) (SCSI only)
		case 0x2b:
			Seek10();
			return;

		//WRITE and VERIFY (SCSI only)
		case 0x2e:
			Write10();
			return;

		//VERIFY (SCSI only)
		case 0x2f:
			Verify();
			return;

		// SPECIFY(SASI only)
		case 0xc2:
			Specify();
			return;
	}

	//Others not supported
	LOG1(Log::Warning, "未対応コマンド $%02X", sasi.cmd[0]);
	sasi.current->InvalidCmd();

	//Error
	Error();
}

//---------------------------------------------------------------------------
//
//Status phase
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Status()
{
	ASSERT(this);

#if defined(SASI_LOG)
	LOG1(Log::Normal, "ステータスフェーズ ステータス$%02X", sasi.status);
#endif	// SASI_LOG

	//Status phase
	sasi.phase = status;

	// I/O=1 C/D=1 MSG=0
	sasi.io = TRUE;
	sasi.cd = TRUE;
	ASSERT(!sasi.msg);

	//Request status data
	sasi.req = TRUE;

	//Interrupt request
	iosc->IntHDC(TRUE);
}

//---------------------------------------------------------------------------
//
//Message phase
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Message()
{
	ASSERT(this);

#if defined(SASI_LOG)
	LOG1(Log::Normal, "メッセージフェーズ メッセージ$%02X", sasi.message);
#endif	// SASI_LOG

	//Message phase
	sasi.phase = message;

	// I/O=1 C/D=1 MSG=1
	ASSERT(sasi.io);
	ASSERT(sasi.cd);
	sasi.msg = 1;

	//Request status data
	sasi.req = TRUE;
}

//---------------------------------------------------------------------------
//
//Common error
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Error()
{
	ASSERT(this);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "エラー(ステータスフェーズへ)");
#endif	// SASI_LOG

	//Set status and message (CHECK CONDITION)
	sasi.status = (sasi.current->GetLUN() << 5) | 0x02;
	sasi.message = 0x00;

	//Stop event
	event.SetTime(0);

	//Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
void FASTCALL SASI::TestUnitReady()
{
	BOOL status;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "TEST UNIT READYコマンド");
#endif	// SASI_LOG

	//Process command on drive
	status = sasi.current->TestUnitReady(sasi.cmd);
	if (!status) {
		//Failed (error)
		Error();
		return;
	}

	//Success
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	REZERO UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Rezero()
{
	BOOL status;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "REZEROコマンド");
#endif	// SASI_LOG

	//Process command on drive
	status = sasi.current->Rezero(sasi.cmd);
	if (!status) {
		//Failed (error)
		Error();
		return;
	}

	//Success
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
void FASTCALL SASI::RequestSense()
{
	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "REQUEST SENSEコマンド");
#endif	// SASI_LOG

	//Process command on drive
	sasi.length = sasi.current->RequestSense(sasi.cmd, sasi.buffer);
	if (sasi.length <= 0) {
		//Failed (error)
		Error();
		return;
	}

#if defined(SASI_LOG)
	LOG1(Log::Normal, "センスキー $%02X", sasi.buffer[2]);
#endif

	//Return data from drive. I/O=1, C/D=0
	sasi.offset = 0;
	sasi.blocks = 1;
	sasi.phase = read;
	sasi.io = TRUE;
	sasi.cd = FALSE;

	//Clear error status
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Set event, request data
	event.SetUser(0);
	event.SetTime(48);
}

//---------------------------------------------------------------------------
//
//	FORMAT UNIT
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Format()
{
	BOOL status;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "FORMAT UNITコマンド");
#endif	// SASI_LOG

	//Process command on drive
	status = sasi.current->Format(sasi.cmd);
	if (!status) {
		//Failed (error)
		Error();
		return;
	}

	//Success
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	REASSIGN BLOCKS
//SASI only (by spec, SCSI also possible)
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Reassign()
{
	BOOL status;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "REASSIGN BLOCKSコマンド");
#endif	// SASI_LOG

	//Error if not SASI (no defect list sent)
	if (!sasi.current->IsSASI()) {
		sasi.current->InvalidCmd();
		Error();
		return;
	}

	//Process command on drive
	status = sasi.current->Reassign(sasi.cmd);
	if (!status) {
		//Failed (error)
		Error();
		return;
	}

	//Success
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	READ(6)
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Read6()
{
	DWORD record;

	ASSERT(this);
	ASSERT(sasi.current);

	//Get record number and block count
	record = sasi.cmd[1] & 0x1f;
	record <<= 8;
	record |= sasi.cmd[2];
	record <<= 8;
	record |= sasi.cmd[3];
	sasi.blocks = sasi.cmd[4];
	if (sasi.blocks == 0) {
		sasi.blocks = 0x100;
	}

#if defined(SASI_LOG)
	LOG2(Log::Normal, "READ(6)コマンド レコード=%06X ブロック=%d", record, sasi.blocks);
#endif	// SASI_LOG

	//Process command on drive
	sasi.length = sasi.current->Read(sasi.buffer, record);
	if (sasi.length <= 0) {
		//Failed (error)
		Error();
		return;
	}

	//Set status to OK
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Next block ok, set work
	sasi.offset = 0;
	sasi.next = record + 1;

	//Read phase, I/O=1, C/D=0
	sasi.phase = read;
	sasi.io = TRUE;
	sasi.cd = FALSE;

	//Set event, request data
	event.SetUser(1);
	event.SetTime(48);
}

//---------------------------------------------------------------------------
//
//	WRITE(6)
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Write6()
{
	DWORD record;

	ASSERT(this);
	ASSERT(sasi.current);

	//Get record number and block count
	record = sasi.cmd[1] & 0x1f;
	record <<= 8;
	record |= sasi.cmd[2];
	record <<= 8;
	record |= sasi.cmd[3];
	sasi.blocks = sasi.cmd[4];
	if (sasi.blocks == 0) {
		sasi.blocks = 0x100;
	}

#if defined(SASI_LOG)
	LOG2(Log::Normal, "WRITE(6)コマンド レコード=%06X ブロック=%d", record, sasi.blocks);
#endif	// SASI_LOG

	//Process command on drive
	sasi.length = sasi.current->WriteCheck(record);
	if (sasi.length <= 0) {
		//Failed (error)
		Error();
		return;
	}

	//Set status to OK
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Transfer work
	sasi.next = record + 1;
	sasi.offset = 0;

	//Write phase, C/D=0
	sasi.phase = write;
	sasi.cd = FALSE;

	//Set event, request data write
	event.SetUser(1);
	event.SetTime(48);
}

//---------------------------------------------------------------------------
//
//	SEEK(6)
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Seek6()
{
	BOOL status;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "SEEK(6)コマンド");
#endif	// SASI_LOG

	//Process command on drive
	status = sasi.current->Seek(sasi.cmd);
	if (!status) {
		//Failed (error)
		Error();
		return;
	}

	//Success
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	ASSIGN
//SASI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Assign()
{
	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "ASSIGNコマンド");
#endif	// SASI_LOG

	//Error if not SASI (vendor unique command)
	if (!sasi.current->IsSASI()) {
		sasi.current->InvalidCmd();
		Error();
		return;
	}

	//Set status to OK
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Request 4 bytes of data
	sasi.phase = write;
	sasi.length = 4;
	sasi.offset = 0;

	//C/D is 0 (data write)
	sasi.cd = FALSE;

	//Set event, request data write
	event.SetUser(0);
	event.SetTime(48);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//SCSI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Inquiry()
{
	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "INQUIRYコマンド");
#endif	// SASI_LOG

	//Process command on drive
	sasi.length = sasi.current->Inquiry(sasi.cmd, sasi.buffer);
	if (sasi.length <= 0) {
		//Failed (error)
		Error();
		return;
	}

	//Set status to OK
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Return data from drive. I/O=1, C/D=0
	sasi.offset = 0;
	sasi.blocks = 1;
	sasi.phase = read;
	sasi.io = TRUE;
	sasi.cd = FALSE;

	//Set event, request data
	event.SetUser(0);
	event.SetTime(48);
}

//---------------------------------------------------------------------------
//
//	MODE SENSE
//SCSI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::ModeSense()
{
	int length;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "MODE SENSEコマンド");
#endif	// SASI_LOG

	//Process command on drive
	length = sasi.current->ModeSense(sasi.cmd, sasi.buffer);

	//Result evaluation
	if (length > 0) {
		//Set status to OK
		sasi.status = 0x00;
		sasi.message = 0x00;
		sasi.length = length;
	}
	else {
#if 1
		Error();
		return;
#else
		//Set status to error
		sasi.status = (sasi.current->GetLUN() << 5) | 0x02;
		sasi.message = 0;
		sasi.length = -length;
#endif
	}

	//Return data from drive. I/O=1, C/D=0
	sasi.offset = 0;
	sasi.blocks = 1;
	sasi.phase = read;
	sasi.io = TRUE;
	sasi.cd = FALSE;

	//Set event, request data
	event.SetUser(0);
	event.SetTime(48);
}

//---------------------------------------------------------------------------
//
//	START STOP UNIT
//SCSI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::StartStop()
{
	BOOL status;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "START STOP UNITコマンド");
#endif	// SASI_LOG

	//Process command on drive
	status = sasi.current->StartStop(sasi.cmd);
	if (!status) {
		//Failed (error)
		Error();
		return;
	}

	//Success
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	PREVENT/ALLOW MEDIUM REMOVAL
//SCSI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Removal()
{
	BOOL status;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "PREVENT/ALLOW MEDIUM REMOVALコマンド");
#endif	// SASI_LOG

	//Process command on drive
	status = sasi.current->Removal(sasi.cmd);
	if (!status) {
		//Failed (error)
		Error();
		return;
	}

	//Success
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	READ CAPACITY
//SCSI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::ReadCapacity()
{
	int length;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "READ CAPACITYコマンド");
#endif	// SASI_LOG

	//Process command on drive
	length = sasi.current->ReadCapacity(sasi.cmd, sasi.buffer);

	//Result evaluation
	if (length > 0) {
		//Set status to OK
		sasi.status = 0x00;
		sasi.message = 0x00;
		sasi.length = length;
	}
	else {
#if 1
		//Failed (error)
		Error();
		return;
#else
		//Set status to error
		sasi.status = (sasi.current->GetLUN() << 5) | 0x02;
		sasi.message = 0x00;
		sasi.length = -length;
#endif
	}

	//Return data from drive. I/O=1, C/D=0
	sasi.offset = 0;
	sasi.blocks = 1;
	sasi.phase = read;
	sasi.io = TRUE;
	sasi.cd = FALSE;

	//Set event, request data
	event.SetUser(0);
	event.SetTime(48);
}

//---------------------------------------------------------------------------
//
//	READ(10)
//SCSI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Read10()
{
	DWORD record;

	ASSERT(this);
	ASSERT(sasi.current);

	//Get record number and block count
	record = sasi.cmd[2];
	record <<= 8;
	record |= sasi.cmd[3];
	record <<= 8;
	record |= sasi.cmd[4];
	record <<= 8;
	record |= sasi.cmd[5];
	sasi.blocks = sasi.cmd[7];
	sasi.blocks <<= 8;
	sasi.blocks |= sasi.cmd[8];

#if defined(SASI_LOG)
	LOG2(Log::Normal, "READ(10)コマンド レコード=%08X ブロック=%d", record, sasi.blocks);
#endif	// SASI_LOG

	//Block count 0 not processed
	if (sasi.blocks == 0) {
		sasi.status = 0x00;
		sasi.message = 0x00;
		Status();
		return;
	}

	//Process command on drive
	sasi.length = sasi.current->Read(sasi.buffer, record);
	if (sasi.length <= 0) {
		//Failed (error)
		Error();
		return;
	}

	//Set status to OK
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Next block ok, set work
	sasi.offset = 0;
	sasi.next = record + 1;

	//Read phase, I/O=1, C/D=0
	sasi.phase = read;
	sasi.io = TRUE;
	sasi.cd = FALSE;

	//Set event, request data
	event.SetUser(1);
	event.SetTime(48);
}

//---------------------------------------------------------------------------
//
//	WRITE(10)
//SCSI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Write10()
{
	DWORD record;

	ASSERT(this);
	ASSERT(sasi.current);

	//Get record number and block count
	record = sasi.cmd[2];
	record <<= 8;
	record |= sasi.cmd[3];
	record <<= 8;
	record |= sasi.cmd[4];
	record <<= 8;
	record |= sasi.cmd[5];
	sasi.blocks = sasi.cmd[7];
	sasi.blocks <<= 8;
	sasi.blocks |= sasi.cmd[8];

#if defined(SASI_LOG)
	LOG2(Log::Normal, "WRITE(10)コマンド レコード=%08X ブロック=%d", record, sasi.blocks);
#endif	// SASI_LOG

	//Block count 0 not processed
	if (sasi.blocks == 0) {
		sasi.status = 0x00;
		sasi.message = 0x00;
		Status();
		return;
	}

	//Process command on drive
	sasi.length = sasi.current->WriteCheck(record);
	if (sasi.length <= 0) {
		//Failed (error)
		Error();
		return;
	}

	//Set status to OK
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Transfer work
	sasi.next = record + 1;
	sasi.offset = 0;

	//Write phase, C/D=0
	sasi.phase = write;
	sasi.cd = FALSE;

	//Set event, request data write
	event.SetUser(1);
	event.SetTime(48);
}

//---------------------------------------------------------------------------
//
//	SEEK(10)
//SCSI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Seek10()
{
	BOOL status;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "SEEK(10)コマンド");
#endif	// SASI_LOG

	//Process command (seek called since logical blocks not related)
	status = sasi.current->Seek(sasi.cmd);
	if (!status) {
		//Failed (error)
		Error();
		return;
	}

	//Success
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	VERIFY
//SCSI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Verify()
{
	BOOL status;

	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "VERIFYコマンド");
#endif	// SASI_LOG

	//Process command on drive
	status = sasi.current->Verify(sasi.cmd);
	if (!status) {
		//Failed (error)
		Error();
		return;
	}

	//Success
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Status phase
	Status();
}

//---------------------------------------------------------------------------
//
//	SPECIFY
//SASI only
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Specify()
{
	ASSERT(this);
	ASSERT(sasi.current);

#if defined(SASI_LOG)
	LOG0(Log::Normal, "SPECIFYコマンド");
#endif	// SASI_LOG

	//Error if not SASI (vendor unique command)
	if (!sasi.current->IsSASI()) {
		sasi.current->InvalidCmd();
		Error();
		return;
	}

	//Status OK
	sasi.status = 0x00;
	sasi.message = 0x00;

	//Request 10 bytes of data
	sasi.phase = write;
	sasi.length = 10;
	sasi.offset = 0;

	//C/D is 0 (data write)
	sasi.cd = FALSE;

	//Set event, request data write
	event.SetUser(0);
	event.SetTime(48);
}

//---------------------------------------------------------------------------
//
//MO open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASI::Open(const Filepath& path)
{
	ASSERT(this);

	//If SCSI enabled, let SCSI handle
	if (sasi.scsi_type != 0) {
		return scsi->Open(path);
	}

	//Error if not enabled
	if (!IsValid()) {
		return FALSE;
	}

	//Eject
	Eject(FALSE);

	//Error if not ready
	if (IsReady()) {
		return FALSE;
	}

	//Leave to drive
	ASSERT(sasi.mo);
	if (sasi.mo->Open(path, TRUE)) {
		//If successful, remember file path and write protect state
		sasi.writep = sasi.mo->IsWriteP();
		sasi.mo->GetPath(scsimo);
		return TRUE;
	}

	//Failed
	return FALSE;
}

//---------------------------------------------------------------------------
//
//MO Eject
//
//---------------------------------------------------------------------------
void FASTCALL SASI::Eject(BOOL force)
{
	ASSERT(this);

	//If SCSI enabled, let SCSI handle
	if (sasi.scsi_type != 0) {
		scsi->Eject(force);
		return;
	}

	//If not ready, do nothing
	if (!IsReady()) {
		return;
	}

	//Notify drive
	ASSERT(sasi.mo);
	sasi.mo->Eject(force);
}

//---------------------------------------------------------------------------
//
//MO write protect
//
//---------------------------------------------------------------------------
void FASTCALL SASI::WriteP(BOOL flag)
{
	ASSERT(this);

	//If SCSI enabled, let SCSI handle
	if (sasi.scsi_type != 0) {
		scsi->WriteP(flag);
		return;
	}

	//If not ready, do nothing
	if (!IsReady()) {
		return;
	}

	//Try setting write protect
	ASSERT(sasi.mo);
	sasi.mo->WriteP(flag);

	//Save current state
	sasi.writep = sasi.mo->IsWriteP();
}

//---------------------------------------------------------------------------
//
//Get MO write protect
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASI::IsWriteP() const
{
	ASSERT(this);

	//If SCSI enabled, let SCSI handle
	if (sasi.scsi_type != 0) {
		return scsi->IsWriteP();
	}

	//If not ready, not write protected
	if (!IsReady()) {
		return FALSE;
	}

	//Ask drive
	ASSERT(sasi.mo);
	return sasi.mo->IsWriteP();
}

//---------------------------------------------------------------------------
//
//Check MO Read Only
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASI::IsReadOnly() const
{
	ASSERT(this);

	//If SCSI enabled, let SCSI handle
	if (sasi.scsi_type != 0) {
		return scsi->IsReadOnly();
	}

	//If not ready, not read only
	if (!IsReady()) {
		return FALSE;
	}

	//Ask drive
	ASSERT(sasi.mo);
	return sasi.mo->IsReadOnly();
}

//---------------------------------------------------------------------------
//
//Check MO Locked
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASI::IsLocked() const
{
	ASSERT(this);

	//If SCSI enabled, let SCSI handle
	if (sasi.scsi_type != 0) {
		return scsi->IsLocked();
	}

	//Does drive exist
	if (!sasi.mo) {
		return FALSE;
	}

	//Ask drive
	return sasi.mo->IsLocked();
}

//---------------------------------------------------------------------------
//
//Check MO Ready
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASI::IsReady() const
{
	ASSERT(this);

	//If SCSI enabled, let SCSI handle
	if (sasi.scsi_type != 0) {
		return scsi->IsReady();
	}

	//Does drive exist
	if (!sasi.mo) {
		return FALSE;
	}

	//Ask drive
	return sasi.mo->IsReady();
}

//---------------------------------------------------------------------------
//
//Check MO valid
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASI::IsValid() const
{
	ASSERT(this);

	//If SCSI enabled, let SCSI handle
	if (sasi.scsi_type != 0) {
		return scsi->IsValid();
	}

	//Distinguish by pointer
	if (sasi.mo) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

//---------------------------------------------------------------------------
//
//Get MO path
//
//---------------------------------------------------------------------------
void FASTCALL SASI::GetPath(Filepath& path) const
{
	ASSERT(this);

	//If SCSI enabled, let SCSI handle
	if (sasi.scsi_type != 0) {
		scsi->GetPath(path);
		return;
	}

	if (sasi.mo) {
		if (sasi.mo->IsReady()) {
			//From MO disk
			sasi.mo->GetPath(path);
			return;
		}
	}

	//If not valid path, clear
	path.Clear();
}
