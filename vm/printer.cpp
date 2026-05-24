//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ Printer ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "cpu.h"
#include "log.h"
#include "iosc.h"
#include "sync.h"
#include "fileio.h"
#include "printer.h"

//===========================================================================
//
//	Printer
//
//===========================================================================
//#define PRINTER_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Printer::Printer(VM *p) : MemDevice(p)
{
	// Device ID setting
	dev.id = MAKEID('P', 'R', 'N', ' ');
	dev.desc = "Printer";

	// Start/End address
	memdev.first = 0xe8c000;
	memdev.last = 0xe8dfff;

	// Others
	iosc = NULL;
	sync = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL Printer::Init()
{
	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get IOSC
	iosc = (IOSC*)vm->SearchDevice(MAKEID('I', 'O', 'S', 'C'));
 ASSERT(iosc);

	// Create Sync
	sync = new Sync;

	// Not connected
	printer.connect = FALSE;

	// Clear buffer
	sync->Lock();
	printer.read = 0;
	printer.write = 0;
	printer.num = 0;
	sync->Unlock();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL Printer::Cleanup()
{
 ASSERT(this);

	// Delete Sync
	if (sync) {
		delete sync;
		sync = NULL;
	}

	// Base class cleanup
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Printer::Reset()
{
 ASSERT(this);

	LOG0(Log::Normal, "Reset");

	// Strobe (initial value)
	printer.strobe = FALSE;

	// Ready (initial value)
	if (printer.connect) {
		// If connected, READY
		printer.ready = TRUE;
	}
	else {
		// If not connected, BUSY
		printer.ready = FALSE;
	}

	// Interrupt clear
	iosc->IntPRT(FALSE);
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Printer::Save(Fileio *fio, int /* ver */)
{
	size_t sz;

 ASSERT(this);
 ASSERT(fio);
	LOG0(Log::Normal, "Save");

	// Size save
	sz = sizeof(printer_t);
	if (!fio->Write(&sz, (int)sizeof(sz))) {
		return FALSE;
	}

	// Data save
	if (!fio->Write(&printer, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Printer::Load(Fileio *fio, int /* ver */)
{
	size_t sz;

 ASSERT(this);
 ASSERT(fio);
	LOG0(Log::Normal, "Load");

	// Size load and check
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(printer_t)) {
		return FALSE;
	}

	// Data load
	if (!fio->Read(&printer, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL Printer::ApplyCfg(const Config* /*config*/)
{
 ASSERT(this);
	LOG0(Log::Normal, "Apply config");

	// Config change triggers Connect(), which changes READY
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL Printer::ReadByte(DWORD /*addr*/)
{
 ASSERT(this);

	// Returns 0xff (Write Only register)
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL Printer::ReadWord(DWORD /*addr*/)
{
 ASSERT(this);

	// Returns 0xff (Write Only register)
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL Printer::WriteByte(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));

 ASSERT(printer.num <= BufMax);
 ASSERT(printer.read < BufMax);
 ASSERT(printer.write < BufMax);

	// 4-byte unit loop (Note: WriteOnly port, invalid address)
	addr &= 0x03;

	// Decode
	switch (addr) {
		// $E8C001 printer data
		case 1:
			printer.data = (BYTE)data;
			break;

		// $E8C003 strobe
		case 3:
			// Strobe change from 0(TRUE) to 1(FALSE) triggers output
			if ((data & 1) == 0) {
#if defined(PRINTER_LOG)
				LOG0(Log::Normal, "Strobe 0(TRUE)");
#endif	// PRINTER_LOG
				printer.strobe = TRUE;
				break;
			}

			// If strobe is not TRUE, do nothing
			if (!printer.strobe) {
				break;
			}

			// Strobe becomes FALSE
			printer.strobe = FALSE;
#if defined(PRINTER_LOG)
			LOG0(Log::Normal, "Strobe 1(FALSE)");
#endif	// PRINTER_LOG

			// If not connected, do nothing
			if (!printer.connect) {
				break;
			}

			// Output data to buffer
#if defined(PRINTER_LOG)
			LOG1(Log::Normal, "Data send $%02X", printer.data);
#endif	// PRINTER_LOG

			sync->Lock();
			// Buffer write
			printer.buf[printer.write] = printer.data;
			printer.write = (printer.write + 1) & (BufMax - 1);
			printer.num++;

			// Buffer overflow warning, buffer is full
			if (printer.num > BufMax) {
				ASSERT(FALSE);
				LOG0(Log::Warning, "Printer buffer overflow");
				printer.num = BufMax;
			}
			sync->Unlock();

			// Set READY
			printer.ready = FALSE;

			// Interrupt clear
			iosc->IntPRT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL Printer::WriteWord(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT((addr & 1) == 0);

	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL Printer::ReadOnly(DWORD /*addr*/) const
{
 ASSERT(this);

	// Returns 0xff (Write Only register)
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	H-Sync notification
//
//---------------------------------------------------------------------------
void FASTCALL Printer::HSync()
{
 ASSERT(this);

	// If not ready (data transmission), do nothing
	if (printer.ready) {
		return;
	}

	// If not connected, exit
	if (!printer.connect) {
		// printer.ready is already FALSE
		return;
	}

	// Buffer full, try again next time
	if (printer.num == BufMax) {
#if defined(PRINTER_LOG)
	LOG0(Log::Normal, "Buffer full, wait");
#endif	// PRINTER_LOG
		return;
	}

#if defined(PRINTER_LOG)
	LOG0(Log::Normal, "Data transmission READY");
#endif	// PRINTER_LOG

	// Set ready
	printer.ready = TRUE;

	// Interrupt set
	iosc->IntPRT(TRUE);
}

//---------------------------------------------------------------------------
//
//	Get printer data
//
//---------------------------------------------------------------------------
void FASTCALL Printer::GetPrinter(printer_t *buffer) const
{
 ASSERT(this);
 ASSERT(buffer);

	// Copy structure
	*buffer = printer;
}

//---------------------------------------------------------------------------
//
//	Connect
//
//---------------------------------------------------------------------------
void FASTCALL Printer::Connect(BOOL flag)
{
 ASSERT(this);
 ASSERT(printer.num <= BufMax);
 ASSERT(printer.read < BufMax);
 ASSERT(printer.write < BufMax);

	// If already connected, do nothing
	if (printer.connect == flag) {
		return;
	}

	// Set
	printer.connect = flag;
#if defined(PRINTER_LOG)
	if (printer.connect) {
		LOG0(Log::Normal, "Printer connect");
	}
	else {
		LOG0(Log::Normal, "Printer disconnect");
	}
#endif	// PRINTER_LOG

	// If FALSE, clear ready
	if (!printer.connect) {
		printer.ready = FALSE;
		iosc->IntPRT(FALSE);
		return;
	}

	// If TRUE, clear buffer at next HSYNC to set ready
	sync->Lock();
	printer.read = 0;
	printer.write = 0;
	printer.num = 0;
	sync->Unlock();
}

//---------------------------------------------------------------------------
//
//	Get first data
//
//---------------------------------------------------------------------------
BOOL FASTCALL Printer::GetData(BYTE *ptr)
{
 ASSERT(this);
 ASSERT(ptr);
 ASSERT(printer.num <= BufMax);
 ASSERT(printer.read < BufMax);
 ASSERT(printer.write < BufMax);

	// Lock
	sync->Lock();

	// If no data, return FALSE
	if (printer.num == 0) {
		sync->Unlock();
		return FALSE;
	}

	// Get data
	*ptr = printer.buf[printer.read];

	// Increment
	printer.read = (printer.read + 1) & (BufMax - 1);
	printer.num--;

	// Unlock
	sync->Unlock();
	return TRUE;
}
