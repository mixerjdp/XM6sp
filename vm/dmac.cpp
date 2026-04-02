//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ DMAC(HD63450) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "cpu.h"
#include "memory.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "fdc.h"
#include "fileio.h"
#include "dmac.h"
#include "x68sound_bridge.h"

//===========================================================================
//
//	DMAC
//
//===========================================================================
//#define DMAC_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
DMAC::DMAC(VM *p) : MemDevice(p)
{
	// Device ID setting
	dev.id = MAKEID('D', 'M', 'A', 'C');
	dev.desc = "DMAC (HD63450)";

	// Start address, end address
	memdev.first = 0xe84000;
	memdev.last = 0xe85fff;

	// Others
	memory = NULL;
	fdc = NULL;
	legacy_cnt_mode = FALSE;
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::Init()
{
	int ch;

	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get memory
	memory = (Memory*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(memory);

	// Get FDC
	fdc = (FDC*)vm->SearchDevice(MAKEID('F', 'D', 'C', ' '));
	ASSERT(fdc);

	// Initialize channel structures
	for (ch=0; ch<4; ch++) {
		memset(&dma[ch], 0, sizeof(dma[ch]));
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::Cleanup()
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
void FASTCALL DMAC::Reset()
{
	int ch;

	ASSERT(this);
	LOG0(Log::Normal, "Reset");

	// Global
	dmactrl.transfer = 0;
	dmactrl.load = 0;
	dmactrl.exec = FALSE;
	dmactrl.current_ch = 0;
	dmactrl.cpu_cycle = 0;
	dmactrl.vector = -1;

	// Reset channel structures
	for (ch=0; ch<4; ch++) {
		ResetDMA(ch);
	}
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;
	int i;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "Save");

	// Channel
	sz = sizeof(dma_t);
	for (i=0; i<4; i++) {
		if (!fio->Write(&sz, sizeof(sz))) {
			return FALSE;
		}
		if (!fio->Write(&dma[i], (int)sz)) {
			return FALSE;
		}
	}

	// Global
	sz = sizeof(dmactrl_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (!fio->Write(&dmactrl, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::Load(Fileio *fio, int /*ver*/)
{
	int i;
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "Load");

	// Channel
	for (i=0; i<4; i++) {
		// Read size and check
		if (!fio->Read(&sz, sizeof(sz))) {
			return FALSE;
		}
		if (sz != sizeof(dma_t)) {
			return FALSE;
		}

		// Read self
		if (!fio->Read(&dma[i], (int)sz)) {
			return FALSE;
		}
	}

	// Global
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(dmactrl_t)) {
		return FALSE;
	}

	if (!fio->Read(&dmactrl, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);
//	ASSERT(config);
	LOG0(Log::Normal, "Apply config");
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::ReadByte(DWORD addr)
{
	int ch;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Wait
	scheduler->Wait(7);

	// Assign to channel
	ch = (int)(addr >> 6);
	ch &= 3;
	addr &= 0x3f;

	// Execute in channel unit
	return ReadDMA(ch, addr);
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::ReadWord(DWORD addr)
{
	int ch;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	// Wait
	scheduler->Wait(7);

	// Assign to channel
	ch = (int)(addr >> 6);
	ch &= 3;
	addr &= 0x3f;

	// Execute in channel unit
	return ((ReadDMA(ch, addr) << 8) | ReadDMA(ch, addr + 1));
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::WriteByte(DWORD addr, DWORD data)
{
	int ch;
	bool mirror_to_x68sound;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);

	scheduler->Wait(7);

	ch = (int)(addr >> 6);
	ch &= 3;
	addr &= 0x3f;
	mirror_to_x68sound = (ch == 3) && !(dma[ch].act && (addr == 0x04 || addr == 0x05 || addr == 0x06 || addr == 0x29 || addr == 0x31 || (addr == 0x07 && data == 0x48)));

	WriteDMA(ch, addr, data);
	if (mirror_to_x68sound) {
		Xm6X68Sound::WriteDma(static_cast<unsigned char>(addr), static_cast<unsigned char>(data));
	}
}

void FASTCALL DMAC::WriteWord(DWORD addr, DWORD data)
{
	int ch;
	bool mirror_to_x68sound;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	scheduler->Wait(7);

	ch = (int)(addr >> 6);
	ch &= 3;
	addr &= 0x3f;

	WriteDMA(ch, addr, (BYTE)(data >> 8));
	mirror_to_x68sound = (ch == 3) && !(dma[ch].act && (addr == 0x04 || addr == 0x05 || addr == 0x06 || addr == 0x29 || addr == 0x31 || (addr == 0x07 && data == 0x48)));
	if (mirror_to_x68sound) {
		Xm6X68Sound::WriteDma(static_cast<unsigned char>(addr), static_cast<unsigned char>(data >> 8));
	}
	WriteDMA(ch, addr + 1, (BYTE)data);
	mirror_to_x68sound = (ch == 3) && !(dma[ch].act && (addr + 1 == 0x04 || addr + 1 == 0x05 || addr + 1 == 0x06 || addr + 1 == 0x29 || addr + 1 == 0x31 || (addr + 1 == 0x07 && (data & 0xff) == 0x48)));
	if (mirror_to_x68sound) {
		Xm6X68Sound::WriteDma(static_cast<unsigned char>(addr + 1), static_cast<unsigned char>(data));
	}
}
DWORD FASTCALL DMAC::ReadOnly(DWORD addr) const
{
	int ch;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Assign to channel
	ch = (int)(addr >> 6);
	ch &= 3;
	addr &= 0x3f;

	// Execute in channel unit
	return ReadDMA(ch, addr);
}

//---------------------------------------------------------------------------
//
//	DMA read
//	Internal register access
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::ReadDMA(int ch, DWORD addr) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(addr <= 0x3f);

	switch (addr) {
		// CSR
		case 0x00:
			return GetCSR(ch);

		// CER
		case 0x01:
			return dma[ch].ecode;

		// DCR
		case 0x04:
			return GetDCR(ch);

		// OCR
		case 0x05:
			return GetOCR(ch);

		// SCR
		case 0x06:
			return GetSCR(ch);

		// CCR
		case 0x07:
			return GetCCR(ch);

		// MTC
		case 0x0a:
			return (BYTE)(dma[ch].mtc >> 8);
		case 0x0b:
			return (BYTE)dma[ch].mtc;

		// MAR
		case 0x0c:
			return 0;
		case 0x0d:
			return (BYTE)(dma[ch].mar >> 16);
		case 0x0e:
			return (BYTE)(dma[ch].mar >> 8);
		case 0x0f:
			return (BYTE)dma[ch].mar;

		// DAR
		case 0x14:
			return 0;
		case 0x15:
			return (BYTE)(dma[ch].dar >> 16);
		case 0x16:
			return (BYTE)(dma[ch].dar >> 8);
		case 0x17:
			return (BYTE)dma[ch].dar;

		// BTC
		case 0x1a:
			return (BYTE)(dma[ch].btc >> 8);
		case 0x1b:
			return (BYTE)dma[ch].btc;

		// BAR
		case 0x1c:
			return 0;
		case 0x1d:
			return (BYTE)(dma[ch].bar >> 16);
		case 0x1e:
			return (BYTE)(dma[ch].bar >> 8);
		case 0x1f:
			return (BYTE)dma[ch].bar;

		// NIV
		case 0x25:
			return dma[ch].niv;

		// EIV
		case 0x27:
			return dma[ch].eiv;

		// MFC
		case 0x29:
			return dma[ch].mfc;

		// CPR
		case 0x2d:
			return dma[ch].cp;

		// DFC
		case 0x31:
			return dma[ch].dfc;

		// BFC
		case 0x39:
			return dma[ch].bfc;

		// GCR
		case 0x3f:
			if (ch == 3) {
				// Only channel 3 returns burst mode
				ASSERT(dma[ch].bt <= 3);
				ASSERT(dma[ch].br <= 3);

				data = dma[ch].bt;
				data <<= 2;
				data |= dma[ch].br;
				return data;
			}
			return 0xff;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	DMA write
//	Internal register access required
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::WriteDMA(int ch, DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(addr <= 0x3f);
	ASSERT(data < 0x100);

	switch (addr) {
		// CSR
		case 0x00:
			SetCSR(ch, data);
			return;

		// CER(Read Only)
		case 0x01:
			return;

		// DCR
		case 0x04:
			SetDCR(ch, data);
			return;

		// OCR
		case 0x05:
			SetOCR(ch, data);
			return;

		// SCR
		case 0x06:
			SetSCR(ch, data);
			return;

		// CCR
		case 0x07:
			SetCCR(ch, data);
			return;

		// MTC
		case 0x0a:
			dma[ch].mtc &= 0x00ff;
			dma[ch].mtc |= (data << 8);
			return;
		case 0x0b:
			dma[ch].mtc &= 0xff00;
			dma[ch].mtc |= data;
			return;

		// MAR
		case 0x0c:
			return;
		case 0x0d:
			dma[ch].mar &= 0x0000ffff;
			dma[ch].mar |= (data << 16);
			return;
		case 0x0e:
			dma[ch].mar &= 0x00ff00ff;
			dma[ch].mar |= (data << 8);
			return;
		case 0x0f:
			dma[ch].mar &= 0x00ffff00;
			dma[ch].mar |= data;
			return;

		// DAR
		case 0x14:
			return;
		case 0x15:
			dma[ch].dar &= 0x0000ffff;
			dma[ch].dar |= (data << 16);
			return;
		case 0x16:
			dma[ch].dar &= 0x00ff00ff;
			dma[ch].dar |= (data << 8);
			return;
		case 0x17:
			dma[ch].dar &= 0x00ffff00;
			dma[ch].dar |= data;
			return;

		// BTC
		case 0x1a:
			dma[ch].btc &= 0x00ff;
			dma[ch].btc |= (data << 8);
			return;
		case 0x1b:
			dma[ch].btc &= 0xff00;
			dma[ch].btc |= data;
			return;

		// BAR
		case 0x1c:
			return;
		case 0x1d:
			dma[ch].bar &= 0x0000ffff;
			dma[ch].bar |= (data << 16);
			return;
		case 0x1e:
			dma[ch].bar &= 0x00ff00ff;
			dma[ch].bar |= (data << 8);
			return;
		case 0x1f:
			dma[ch].bar &= 0x00ffff00;
			dma[ch].bar |= data;
			return;

		// NIV
		case 0x25:
			dma[ch].niv = data;
			return;

		// EIV
		case 0x27:
			dma[ch].eiv = data;
			return;

		// MFC
		case 0x29:
			dma[ch].mfc = data;
			return;

		// CPR
		case 0x2d:
			dma[ch].cp = data & 0x03;
			return;

		// DFC
		case 0x31:
			dma[ch].dfc = data;
			return;

		// BFC
		case 0x39:
			dma[ch].bfc = data;
			return;

		// GCR
		case 0x3f:
			if (ch == 3) {
				// Only channel 3
				SetGCR(data);
			}
			return;
	}
}

//---------------------------------------------------------------------------
//
//	DCR set
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetDCR(int ch, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(data < 0x100);

	// Timing error if ACT is already on
	if (dma[ch].act) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "Channel %d Timing error(SetDCR)", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x02);
		return;
	}

	// XRM
	dma[ch].xrm = data >> 6;

	// DTYP
	dma[ch].dtyp = (data >> 4) & 0x03;

	// DPS
	if (data & 0x08) {
		dma[ch].dps = TRUE;
	}
	else {
		dma[ch].dps = FALSE;
	}

	// PCL
	dma[ch].pcl = (data & 0x03);

	// Interrupt check
	Interrupt();
}

//---------------------------------------------------------------------------
//
//	DCR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetDCR(int ch) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(dma[ch].xrm <= 3);
	ASSERT(dma[ch].dtyp <= 3);
	ASSERT(dma[ch].pcl <= 3);

	// Create data
	data = dma[ch].xrm;
	data <<= 2;
	data |= dma[ch].dtyp;
	data <<= 1;
	if (dma[ch].dps) {
		data |= 0x01;
	}
	data <<= 3;
	data |= dma[ch].pcl;

	return data;
}

//---------------------------------------------------------------------------
//
//	OCR set
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetOCR(int ch, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(data < 0x100);

	// Timing error if ACT is already on
	if (dma[ch].act) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "Channel %d Timing error(SetOCR)", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x02);
		return;
	}

	// DIR
	if (data & 0x80) {
		dma[ch].dir = TRUE;
	}
	else {
		dma[ch].dir = FALSE;
	}

	// BTD
	if (data & 0x40) {
		dma[ch].btd = TRUE;
	}
	else {
		dma[ch].btd = FALSE;
	}

	// SIZE
	dma[ch].size = (data >> 4) & 0x03;

	// CHAIN
	dma[ch].chain = (data >> 2) & 0x03;

	// REQG
	dma[ch].reqg = (data & 0x03);
}

//---------------------------------------------------------------------------
//
//	OCR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetOCR(int ch) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(dma[ch].size <= 3);
	ASSERT(dma[ch].chain <= 3);
	ASSERT(dma[ch].reqg <= 3);

	// Create data
	data = 0;
	if (dma[ch].dir) {
		data |= 0x02;
	}
	if (dma[ch].btd) {
		data |= 0x01;
	}
	data <<= 2;
	data |= dma[ch].size;
	data <<= 2;
	data |= dma[ch].chain;
	data <<= 2;
	data |= dma[ch].reqg;

	return data;
}

//---------------------------------------------------------------------------
//
//	SCR set
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetSCR(int ch, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(data < 0x100);

	// Timing error if ACT is already on
	if (dma[ch].act) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "Channel %d Timing error(SetSCR)", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x02);
		return;
	}

	dma[ch].mac = (data >> 2) & 0x03;
	dma[ch].dac = (data & 0x03);
}

//---------------------------------------------------------------------------
//
//	SCR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetSCR(int ch) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(dma[ch].mac <= 3);
	ASSERT(dma[ch].dac <= 3);

	// Create data
	data = dma[ch].mac;
	data <<= 2;
	data |= dma[ch].dac;

	return data;
}

//---------------------------------------------------------------------------
//
//	CCR set
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetCCR(int ch, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(data < 0x100);

	// INT
	if (data & 0x08) {
		dma[ch].intr = TRUE;
	}
	else {
		dma[ch].intr = FALSE;
	}

	// HLT
	if (data & 0x20) {
		dma[ch].hlt = TRUE;
	}
	else {
		dma[ch].hlt = FALSE;
	}

	// STR
	if (data & 0x80) {
		dma[ch].str = TRUE;
		StartDMA(ch);
	}

	// CNT
	if (data & 0x40) {
		dma[ch].cnt = TRUE;
		ContDMA(ch);
	}
	else if (!legacy_cnt_mode) {
		dma[ch].cnt = FALSE;
	}

	// SAB
	if (data & 0x10) {
		dma[ch].sab = TRUE;
		AbortDMA(ch);
	}
}

//---------------------------------------------------------------------------
//
//	CCR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetCCR(int ch) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// Return INT,HLT,STR,CNT status
	data = 0;
	if (dma[ch].intr) {
		data |= 0x08;
	}
	if (dma[ch].hlt) {
		data |= 0x20;
	}
	if (dma[ch].str) {
		data |= 0x80;
	}
	if (dma[ch].cnt) {
		data |= 0x40;
	}

	return data;
}

//---------------------------------------------------------------------------
//
//	CSR set
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetCSR(int ch, DWORD data)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(data < 0x100);

	// Writing 1 to non-ACT,PCS clears the flag (allows clearing)
	if (data & 0x80) {
		dma[ch].coc = FALSE;
	}
	if (data & 0x40) {
		dma[ch].boc = FALSE;
	}
	if (data & 0x20) {
		dma[ch].ndt = FALSE;
	}
	if (data & 0x10) {
		dma[ch].err = FALSE;
	}
	if (data & 0x04) {
		dma[ch].dit = FALSE;
	}
	if (data & 0x02) {
		dma[ch].pct = FALSE;
	}

	// Interrupt processing
	Interrupt();
}

//---------------------------------------------------------------------------
//
//	CSR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetCSR(int ch) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// Create data
	data = 0;
	if (dma[ch].coc) {
		data |= 0x80;
	}
	if (dma[ch].boc) {
		data |= 0x40;
	}
	if (dma[ch].ndt) {
		data |= 0x20;
	}
	if (dma[ch].err) {
		data |= 0x10;
	}
	if (dma[ch].act) {
		data |= 0x08;
	}
	if (dma[ch].dit) {
		data |= 0x04;
	}
	if (dma[ch].pct) {
		data |= 0x02;
	}
	if (dma[ch].pcs) {
		data |= 0x01;
	}

	return data;
}

//---------------------------------------------------------------------------
//
//	GCR set
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::SetGCR(DWORD data)
{
	int ch;
	DWORD bt;
	DWORD br;

	ASSERT(this);
	ASSERT(data < 0x100);

	// Separate data
	bt = (data >> 2) & 0x03;
	br = data & 0x03;

	// Set all channels
	for (ch=0; ch<4; ch++) {
		dma[ch].bt = bt;
		dma[ch].br = br;
	}
}

//---------------------------------------------------------------------------
//
//	DMA reset
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::ResetDMA(int ch)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// GCR reset
	dma[ch].bt = 0;
	dma[ch].br = 0;

	// DCR reset
	dma[ch].xrm = 0;
	dma[ch].dtyp = 0;
	dma[ch].dps = FALSE;
	dma[ch].pcl = 0;

	// OCR reset
	dma[ch].dir = FALSE;
	dma[ch].btd = FALSE;
	dma[ch].size = 0;
	dma[ch].chain = 0;
	dma[ch].reqg = 0;

	// SCR reset
	dma[ch].mac = 0;
	dma[ch].dac = 0;

	// CCR reset
	dma[ch].str = FALSE;
	dma[ch].cnt = FALSE;
	dma[ch].hlt = FALSE;
	dma[ch].sab = FALSE;
	dma[ch].intr = FALSE;

	// CSR reset
	dma[ch].coc = FALSE;
	dma[ch].boc = FALSE;
	dma[ch].ndt = FALSE;
	dma[ch].err = FALSE;
	dma[ch].act = FALSE;
	dma[ch].dit = FALSE;
	dma[ch].pct = FALSE;
	if (ch == 0) {
		// FDC is 'L'
		dma[ch].pcs = FALSE;
	}
	else {
		// Others are 'H'
		dma[ch].pcs = TRUE;
	}

	// CPR reset
	dma[ch].cp = 0;

	// CER reset
	dma[ch].ecode = 0;

	// Interrupt vector reset
	dma[ch].niv = 0x0f;
	dma[ch].eiv = 0x0f;

	// Address and counter increment reset (by data sheet)
	dma[ch].mar &= 0x00ffffff;
	dma[ch].dar &= 0x00ffffff;
	dma[ch].bar &= 0x00ffffff;
	dma[ch].mtc &= 0x0000ffff;
	dma[ch].btc &= 0x0000ffff;

	// Transfer type, counter reset
	dma[ch].type = 0;
	dma[ch].startcnt = 0;
	dma[ch].errorcnt = 0;
}

//---------------------------------------------------------------------------
//
//	DMA start
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::StartDMA(int ch)
{
	int c;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

#if defined(DMAC_LOG)
	LOG1(Log::Normal, "Channel %d Start", ch);
#endif	// DMAC_LOG

	// Timing error if ACT,COC,BOC,NDT,ERR are already on
	if (dma[ch].act || dma[ch].coc || dma[ch].boc || dma[ch].ndt || dma[ch].err) {
#if defined(DMAC_LOG)
		if (dma[ch].act) {
			LOG1(Log::Normal, "Channel %d Timing error (ACT)", ch);
		}
		if (dma[ch].coc) {
			LOG1(Log::Normal, "Channel %d Timing error (COC)", ch);
		}
		if (dma[ch].boc) {
			LOG1(Log::Normal, "Channel %d Timing error (BOC)", ch);
		}
		if (dma[ch].ndt) {
			LOG1(Log::Normal, "Channel %d Timing error (NDT)", ch);
		}
		if (dma[ch].err) {
			LOG1(Log::Normal, "Channel %d Timing error (ERR)", ch);
		}
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x02);
		return;
	}

	// If not chaining, MTC=0 causes transfer count error
	if (dma[ch].chain == 0) {
		if (dma[ch].mtc == 0) {
#if defined(DMAC_LOG)
			LOG1(Log::Normal, "Channel %d Transfer count error", ch);
#endif	// DMAC_LOG
			ErrorDMA(ch, 0x0d);
			return;
		}
	}

	// If burst chaining, BTC=0 causes base count error
	if (dma[ch].chain == 0x02) {
		if (dma[ch].btc == 0) {
#if defined(DMAC_LOG)
			LOG1(Log::Normal, "Channel %d Base count error", ch);
#endif	// DMAC_LOG
			ErrorDMA(ch, 0x0f);
			return;
		}
	}

	// Compatibility error check
	if ((dma[ch].xrm == 0x01) || (dma[ch].mac == 0x03) || (dma[ch].dac == 0x03)
			|| (dma[ch].chain == 0x01)) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "Channel %d Compatibility error", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x01);
		return;
	}

	// Create transfer type
	dma[ch].type = 0;
	if (dma[ch].dps) {
		dma[ch].type += 4;
	}
	dma[ch].type += dma[ch].size;

	// Clear flags
	dma[ch].str = FALSE;
	dma[ch].act = TRUE;
	dma[ch].cnt = FALSE;
	dma[ch].sab = FALSE;

	// Count up
	dma[ch].startcnt++;

	// If chaining or burst chaining, load first block
	if (dma[ch].chain != 0) {
		LoadDMA(ch);
		// If address error or bus error occurs, error flag is set
		if (dma[ch].err) {
			return;
		}
	}

	// Clear CPU cycle counter and execute
	dmactrl.cpu_cycle = 0;
	switch (dma[ch].reqg) {
		// External request start
		case 0:
		// External request maximum
		case 1:
			// Stop DMA while current pending DMA is running, stop CPU
			dmactrl.current_ch = ch;
			dmactrl.cpu_cycle = 0;
			dmactrl.exec = TRUE;
			scheduler->dma_active = TRUE;
			c = AutoDMA(cpu->GetIOCycle());
			if (c == 0) {
				cpu->Release();
			}
			break;

		// Priority rotation
		case 2:
			break;

		// External request priority rotation
		case 3:
			// Stop DMA while current pending DMA is running, stop CPU
			dmactrl.current_ch = ch;
			dmactrl.cpu_cycle = 0;
			dmactrl.exec = TRUE;
			scheduler->dma_active= TRUE;
			dma[ch].reqg = 1;
			c = AutoDMA(cpu->GetIOCycle());
			if (c == 0) {
				cpu->Release();
			}
			dma[ch].reqg = 3;
			break;

		default:
			ASSERT(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	DMA continue
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::ContDMA(int ch)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

#if defined(DMAC_LOG)
	LOG1(Log::Normal, "Channel %d Continue", ch);
#endif	// DMAC_LOG

	// Timing error if ACT is not on
	if (!dma[ch].act) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "Channel %d Timing error(Cont)", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x02);
		return;
	}
	// Compatibility error for chaining mode
	if (dma[ch].chain != 0) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "Channel %d Compatibility error", ch);
#endif	// DMAC_LOG
		ErrorDMA(ch, 0x01);
	}
}

//---------------------------------------------------------------------------
//
//	DMA software abort
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::AbortDMA(int ch)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// Error processing does not occur if not active (Marianne.pan)
	if (!dma[ch].act) {
		// Clear COC (original flag)
		dma[ch].coc = FALSE;
		return;
	}

#if defined(DMAC_LOG)
	LOG1(Log::Normal, "Channel %d Software abort", ch);
#endif	// DMAC_LOG

	// Set transfer end, not active
	dma[ch].coc = TRUE;
	dma[ch].act = FALSE;

	// Error caused by software abort
	ErrorDMA(ch, 0x11);
}

//---------------------------------------------------------------------------
//
//	DMA block load
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::LoadDMA(int ch)
{
	DWORD base;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(dmactrl.load == 0);

	// Load mode (stops on address error in ReadWord and bus error)
	dmactrl.load = (ch + 1);

	if (dma[ch].bar & 1) {
		// BAR address error
		AddrErr(dma[ch].bar, TRUE);

		dmactrl.load = 0;
		return;
	}

	// MAR read
	dma[ch].bar &= 0xfffffe;
	dma[ch].mar = (memory->ReadWord(dma[ch].bar) & 0x00ff);
	dma[ch].bar += 2;
	dma[ch].mar <<= 16;
	dma[ch].bar &= 0xfffffe;
	dma[ch].mar |= (memory->ReadWord(dma[ch].bar) & 0xffff);
	dma[ch].bar += 2;

	// MTC read
	dma[ch].bar &= 0xfffffe;
	dma[ch].mtc = (memory->ReadWord(dma[ch].bar) & 0xffff);
	dma[ch].bar += 2;

	if (dma[ch].err) {
		// MAR,MTC read error
		dmactrl.load = 0;
		return;
	}

	// Burst chaining is not done here
	if (dma[ch].chain == 0x02) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "Channel %d Burst chaining block", ch);
#endif	// DMAC_LOG
		dma[ch].btc--;
		dmactrl.load = 0;
		return;
	}

	// Chain chaining (loads next chain address into BAR here)
#if defined(DMAC_LOG)
	LOG1(Log::Normal, "Channel %d Chain chaining block", ch);
#endif	// DMAC_LOG
	dma[ch].bar &= 0xfffffe;
	base = (memory->ReadWord(dma[ch].bar) & 0x00ff);
	dma[ch].bar += 2;
	base <<= 16;
	dma[ch].bar &= 0xfffffe;
	base |= (memory->ReadWord(dma[ch].bar) & 0xffff);
	dma[ch].bar = base;

	// Load complete
	dmactrl.load = 0;
}

//---------------------------------------------------------------------------
//
//	DMA error
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::ErrorDMA(int ch, DWORD code)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT((code >= 0x01) && (code <= 17));

#if defined(DMAC_LOG)
	LOG2(Log::Normal, "Channel %d Error occurred $%02X", ch, code);
#endif	// DMAC_LOG

	// ACT turns off (ADPCM)
	dma[ch].act = FALSE;

	// Error code set
	dma[ch].ecode = code;

	// Error flag set
	dma[ch].err = TRUE;

	// Count up
	dma[ch].errorcnt++;

	// Interrupt processing
	Interrupt();
}

//---------------------------------------------------------------------------
//
//	DMA interrupt
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::Interrupt()
{
	DWORD cp;
	int ch;

	ASSERT(this);

	// DMA at same priority level lowest (data sheet)
	for (cp=0; cp<=3; cp++) {
		for (ch=0; ch<=3; ch++) {
			// CP check
			if (cp != dma[ch].cp) {
				continue;
			}

			// Interrupt controller check
			if (!dma[ch].intr) {
				continue;
			}

			// Output at EIV if ERR
			if (dma[ch].err) {
				if (dmactrl.vector != (int)dma[ch].eiv) {
					// If pending interrupt exists, cancel subsequent
					if (dmactrl.vector >= 0) {
						cpu->IntCancel(3);
					}
#if defined(DMAC_LOG)
					LOG1(Log::Normal, "Channel %d Error interrupt", ch);
#endif	// DMAC_LOG
					cpu->Interrupt(3, (BYTE)dma[ch].eiv);
					dmactrl.vector = (int)dma[ch].eiv;
				}
				return;
			}

			// Output at NIV if COC,BOC,NDT(about interrupt level)
			if (dma[ch].coc || dma[ch].boc || dma[ch].ndt) {
				if (dmactrl.vector != (int)dma[ch].niv) {
					// If pending interrupt exists, cancel subsequent
					if (dmactrl.vector >= 0) {
						cpu->IntCancel(3);
					}
#if defined(DMAC_LOG)
					LOG1(Log::Normal, "Channel %d Normal interrupt", ch);
#endif	// DMAC_LOG
					cpu->Interrupt(3, (BYTE)dma[ch].niv);
					dmactrl.vector = (int)dma[ch].niv;
				}
				return;
			}

			if ((dma[ch].pcl == 0x01) && dma[ch].pct) {
				if (dmactrl.vector != (int)dma[ch].niv) {
					// If pending interrupt exists, cancel subsequent
					if (dmactrl.vector >= 0) {
						cpu->IntCancel(3);
					}
#if defined(DMAC_LOG)
					LOG1(Log::Normal, "Channel %d PCL interrupt", ch);
#endif	// DMAC_LOG
					cpu->Interrupt(3, (BYTE)dma[ch].niv);
					dmactrl.vector = (int)dma[ch].niv;
				}
				return;
			}
		}
	}

	// No pending interrupt
	if (dmactrl.vector >= 0) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "Interrupt canceled vector$%02X", dmactrl.vector);
#endif	// DMAC_LOG

		cpu->IntCancel(3);
		dmactrl.vector = -1;
	}
}

//---------------------------------------------------------------------------
//
//	Interrupt ACK
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::IntAck()
{
	ASSERT(this);

	// If vector is negative, the interrupt was not acknowledged and passed through
	if (dmactrl.vector < 0) {
		LOG0(Log::Warning, "Unrequested interrupt");
		return;
	}

#if defined(DMAC_LOG)
	LOG1(Log::Normal, "Interrupt acknowledged vector$%02X", dmactrl.vector);
#endif	// DMAC_LOG

	// Clear vector
	dmactrl.vector = -1;
}

//---------------------------------------------------------------------------
//
//	Get DMA
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::GetDMA(int ch, dma_t *buffer) const
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(buffer);

	// Copy channel structure
	*buffer = dma[ch];
}

//---------------------------------------------------------------------------
//
//	Get DMA control
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::GetDMACtrl(dmactrl_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	// Copy structure
	*buffer = dmactrl;
}

//---------------------------------------------------------------------------
//
//	DMA transfer query
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::IsDMA() const
{
	ASSERT(this);

	// If both transfer flag (per channel) and load flag are 0
	if ((dmactrl.transfer == 0) && (dmactrl.load == 0)) {
		return FALSE;
	}

	// Something is happening
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	DMA transfer possible
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::IsAct(int ch) const
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// If not ACT, or ERR, or HLT, cannot transfer
	if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
		return FALSE;
	}

	// Can transfer
	return TRUE;
}

void FASTCALL DMAC::SetLegacyCntMode(BOOL enabled)
{
	ASSERT(this);
	legacy_cnt_mode = enabled ? TRUE : FALSE;
}

BOOL FASTCALL DMAC::IsLegacyCntMode() const
{
	ASSERT(this);
	return legacy_cnt_mode;
}

//---------------------------------------------------------------------------
//
//	DMA bus error
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::BusErr(DWORD addr, BOOL read)
{
	ASSERT(this);
	ASSERT(addr <= 0xffffff);

	if (read) {
		LOG1(Log::Warning, "DMA bus error(read) $%06X", addr);
	}
	else {
		LOG1(Log::Warning, "DMA bus error(write) $%06X", addr);
	}

	// Load error
	if (dmactrl.load != 0) {
		// Ignores the boundary of current/device address
		ASSERT((dmactrl.load >= 1) && (dmactrl.load <= 4));
		ErrorDMA(dmactrl.load - 1, 0x08);
		return;
	}

	// Ignores the boundary of current/device address
	ASSERT((dmactrl.transfer >= 1) && (dmactrl.transfer <= 4));
	ErrorDMA(dmactrl.transfer - 1, 0x08);
}

//---------------------------------------------------------------------------
//
//	DMA address error
//
//---------------------------------------------------------------------------
void FASTCALL DMAC::AddrErr(DWORD addr, BOOL read)
{
	ASSERT(this);
	ASSERT(addr <= 0xffffff);

	if (read) {
		LOG1(Log::Warning, "DMA address error(read) $%06X", addr);
	}
	else {
		LOG1(Log::Warning, "DMA address error(write) $%06X", addr);
	}

	// Load error
	if (dmactrl.load != 0) {
		// Ignores the boundary of current/device address
		ASSERT((dmactrl.load >= 1) && (dmactrl.load <= 4));
		ErrorDMA(dmactrl.load - 1, 0x0c);
		return;
	}

	// Ignores the boundary of current/device address
	ASSERT((dmactrl.transfer >= 1) && (dmactrl.transfer <= 4));
	ErrorDMA(dmactrl.transfer - 1, 0x0c);
}

//---------------------------------------------------------------------------
//
//	Get vector
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::GetVector(int type) const
{
	ASSERT(this);
	ASSERT((type >= 0) && (type < 8));

	// Normal/error vector output
	if (type & 1) {
		// Error
		return dma[type >> 1].eiv;
	}
	else {
		// Normal
		return dma[type >> 1].niv;
	}
}

//---------------------------------------------------------------------------
//
//	DMA external request
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::ReqDMA(int ch)
{
	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));

	// If not ACT, or ERR, or HLT, cannot accept
	if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "Channel %d External request failed", ch);
#endif	// DMAC_LOG
		return FALSE;
	}

	// DMA transfer
	TransDMA(ch);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Auto request
//
//---------------------------------------------------------------------------
DWORD FASTCALL DMAC::AutoDMA(DWORD cycle)
{
	int i;
	int ch;
	int mul;
	int remain;
	int used;
	DWORD backup;
	BOOL flag;

	ASSERT(this);

	// Parameter save
	remain = (int)cycle;

	// Cannot auto request if execute flag is not on
	if (!dmactrl.exec) {
		return cycle;
	}

	// Clear execute flag
	flag = FALSE;

	// Process maximum priority auto request channel
	for (i=0; i<4; i++) {
		// Round robin channel
		ch = (dmactrl.current_ch + i) & 3;

		// ACT, ERR, HLT check
		if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
			continue;
		}

		// Not maximum priority auto request
		if (dma[ch].reqg != 1) {
			continue;
		}

		// Accumulate, if less than 10 cycles, not needed
		dmactrl.cpu_cycle += cycle;
		if (dmactrl.cpu_cycle < 10) {
			// CPU cannot execute, for DMA use
			return 0;
		}

		// If less than 2 cycles, flag UP
		cycle = 0;
		flag = TRUE;

		// Save multiplier cycle (elapsed cycle calculation) and reset
		backup = scheduler->GetCPUCycle();
		scheduler->SetCPUCycle(0);

		// Execute until cpu_cycle becomes minus. Temporarily, multiplier applies
		while (scheduler->GetCPUCycle() < dmactrl.cpu_cycle) {
			// ACT, ERR, HLT check
			if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
				break;
			}

			// Use scheulder->GetCPUCycle() to get cycle count from DMAC
			TransDMA(ch);
		}

		// Subtract cycle count and restore
		dmactrl.cpu_cycle -= scheduler->GetCPUCycle();
		remain -= scheduler->GetCPUCycle();
		scheduler->SetCPUCycle(backup);

		// Round robin channel
		dmactrl.current_ch = (dmactrl.current_ch + 1) & 3;

		// If all round robin is exhausted, exit
		if (dmactrl.cpu_cycle <= 0) {
			// CPU cannot execute
			// If all channels exhausted, clear flag in AutoDMA
			return 0;
		}
	}

	// If no maximum priority auto request, rotate and continue
	// Burst priority auto request channel
	for (i=0; i<4; i++) {
		// Round robin channel
		ch = (dmactrl.current_ch + i) & 3;

		// ACT, ERR, HLT check
		if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
			continue;
		}

		// Not burst priority auto request (handled by other code)
		ASSERT(dma[ch].reqg != 1);

		// Not burst priority auto request
		if (dma[ch].reqg != 0) {
			continue;
		}

		// Accumulate, if less than 10 cycles, not needed
		dmactrl.cpu_cycle += cycle;
		if (dmactrl.cpu_cycle < 10) {
			// CPU cannot execute, for DMA use
			return 0;
		}

		// If less than 2 cycles, flag UP
		cycle = 0;
		flag = TRUE;

		// Save multiplier cycle (elapsed cycle calculation) and reset
		backup = scheduler->GetCPUCycle();
		scheduler->SetCPUCycle(0);

		// Calculate burst multiplication
		mul = (dma[ch].bt + 1);

		// If cpu_cycle multiplied by burst multiplier exceeds
		while ((scheduler->GetCPUCycle() << mul) < dmactrl.cpu_cycle) {
			// ACT, ERR, HLT check
			if (!dma[ch].act || dma[ch].err || dma[ch].hlt) {
				break;
			}

			// Transfer
			TransDMA(ch);
		}

		// Save used cycle count (to use later)
		used = scheduler->GetCPUCycle();
		scheduler->SetCPUCycle(backup);

		// Round robin channel
		dmactrl.current_ch = (dmactrl.current_ch + 1) & 3;

		// End if less than burst
		if (dmactrl.cpu_cycle < (used << mul)) {
			// Exceeded specified burst multiplier. Return remainder to CPU
			dmactrl.cpu_cycle -= used;
			if (used < remain) {
				// Excess transfer exists
				return (remain - used);
			}
			else {
				// None, use all. CPU gets 0
				return 0;
			}
		}

		// Still within burst use, continue
		remain -= used;
	}

	if (!flag) {
		// DMA is not used. Stop dmactrl.exec
		dmactrl.exec = FALSE;
		scheduler->dma_active = FALSE;
	}

	return cycle;
}

//---------------------------------------------------------------------------
//
//	DMA1 transfer
//
//---------------------------------------------------------------------------
BOOL FASTCALL DMAC::TransDMA(int ch)
{
	DWORD data;

	ASSERT(this);
	ASSERT((ch >= 0) && (ch <= 3));
	ASSERT(dmactrl.transfer == 0);

	// Transfer flag ON
	dmactrl.transfer = ch + 1;

	// Transfer according to type and direction
	switch (dma[ch].type) {
		// 8bit, Pack byte, 8bit port
		case 0:
		// 8bit, Unpack byte, 8bit port
		case 3:
		// 8bit, Unpack byte, 16bit port
		case 7:
			// SCSI disk default mode (dskbench.x)
			if (dma[ch].dir) {
				memory->WriteByte(dma[ch].mar, (BYTE)(memory->ReadByte(dma[ch].dar)));
				scheduler->Wait(11);
			}
			else {
				memory->WriteByte(dma[ch].dar, (BYTE)(memory->ReadByte(dma[ch].mar)));
				scheduler->Wait(11);
			}
			break;

		// 8bit, Pack byte, 16bit port(Unpack exception:Paradise!)
		// Wait12:Paradise!,Wait?:Moon Fighter
		case 4:
			if (dma[ch].dir) {
				memory->WriteByte(dma[ch].mar, (BYTE)(memory->ReadByte(dma[ch].dar)));
				scheduler->Wait(10);
			}
			else {
				memory->WriteByte(dma[ch].dar, (BYTE)(memory->ReadByte(dma[ch].mar)));
				scheduler->Wait(10);
			}
			break;

		// 8bit, Word
		case 1:
			if (dma[ch].dir) {
				data = (BYTE)(memory->ReadByte(dma[ch].dar));
				data <<= 8;
				data |= (BYTE)(memory->ReadByte(dma[ch].dar + 2));
				memory->WriteWord(dma[ch].mar, data);
				scheduler->Wait(19);
			}
			else {
				data = memory->ReadWord(dma[ch].mar);
				memory->WriteByte(dma[ch].dar, (BYTE)(data >> 8));
				memory->WriteByte(dma[ch].dar + 2, (BYTE)data);
				scheduler->Wait(19);
			}
			break;

		// 8bit, Longword
		case 2:
			if (dma[ch].dir) {
				data = (BYTE)(memory->ReadByte(dma[ch].dar));
				data <<= 8;
				data |= (BYTE)(memory->ReadByte(dma[ch].dar + 2));
				data <<= 8;
				data |= (BYTE)(memory->ReadByte(dma[ch].dar + 4));
				data <<= 8;
				data |= (BYTE)(memory->ReadByte(dma[ch].dar + 6));
				memory->WriteWord(dma[ch].mar, (WORD)(data >> 16));
				memory->WriteWord(dma[ch].mar + 2, (WORD)data);
				scheduler->Wait(38);
			}
			else {
				data = memory->ReadWord(dma[ch].mar);
				data <<= 16;
				data |= (WORD)(memory->ReadWord(dma[ch].mar + 2));
				memory->WriteByte(dma[ch].dar, (BYTE)(data >> 24));
				memory->WriteByte(dma[ch].dar + 2, (BYTE)(data >> 16));
				memory->WriteByte(dma[ch].dar + 4, (BYTE)(data >> 8));
				memory->WriteByte(dma[ch].dar + 6, (BYTE)data);
				scheduler->Wait(38);
			}
			break;

		// 16bit, Word
		case 5:
			// FM synth continues after DMA transfer interrupt sometimes (Out Burst II)
			if (dma[ch].dir) {
				data = memory->ReadWord(dma[ch].dar);
				memory->WriteWord(dma[ch].mar, (WORD)data);
				scheduler->Wait(10);
			}
			else {
				data = memory->ReadWord(dma[ch].mar);
				memory->WriteWord(dma[ch].dar, (WORD)data);
				scheduler->Wait(10);
			}
			break;

		// 16bit, Longword
		case 6:
			if (dma[ch].dir) {
				data = memory->ReadWord(dma[ch].dar);
				data <<= 16;
				data |= (WORD)(memory->ReadWord(dma[ch].dar + 2));
				memory->WriteWord(dma[ch].mar, (WORD)(data >> 16));
				memory->WriteWord(dma[ch].mar + 2, (WORD)data);
				scheduler->Wait(20);
			}
			else {
				data = memory->ReadWord(dma[ch].mar);
				data <<= 16;
				data |= (WORD)memory->ReadWord(dma[ch].mar + 2);
				memory->WriteWord(dma[ch].dar, (WORD)(data >> 16));
				memory->WriteWord(dma[ch].dar + 2, (WORD)data);
				scheduler->Wait(20);
			}
			break;

		// Others
		default:
			ASSERT(FALSE);
	}

	// Transfer flag OFF
	dmactrl.transfer = 0;

	// Transfer error check (bus error and address error)
	if (dma[ch].err) {
		// Address update is not done (by data sheet)
		return FALSE;
	}

	// Address update(multiple of 12: Racing Champ)
	dma[ch].mar += MemDiffTable[ dma[ch].type ][ dma[ch].mac ];
	dma[ch].mar &= 0xffffff;
	dma[ch].dar += DevDiffTable[ dma[ch].type ][ dma[ch].dac ];
	dma[ch].dar &= 0xffffff;

	// Decrement transfer count
	dma[ch].mtc--;
	if (dma[ch].mtc > 0) {
		// If last block, set DONE and TC signal to FDC (DCII)
		if ((ch == 0) && (dma[ch].mtc == 1)) {
			fdc->SetTC();
		}
		return TRUE;
	}

	// Continue processing
	if (dma[ch].cnt) {
#if defined(DMAC_LOG)
		LOG1(Log::Normal, "Channel %d Continue block", ch);
#endif	// DMAC_LOG

		// BOC set
		dma[ch].boc = TRUE;
		Interrupt();

		if (legacy_cnt_mode) {
			// Legacy XM6 behavior: keep CNT latched and reload BAR/BTC every block.
			dma[ch].mar = dma[ch].bar;
			dma[ch].mfc = dma[ch].bfc;
			dma[ch].mtc = dma[ch].btc;
			return TRUE;
		}

		// HD63450/PX68k behavior: CNT is one-shot continuous transfer.
		if ((dma[ch].bar != 0) && (dma[ch].btc != 0)) {
			dma[ch].mar = dma[ch].bar;
			dma[ch].mfc = dma[ch].bfc;
			dma[ch].mtc = dma[ch].btc;
			dma[ch].bar = 0;
			dma[ch].btc = 0;
			dma[ch].cnt = FALSE;
			return TRUE;
		}

		dma[ch].cnt = FALSE;
	}

	// Burst chaining processing
	if (dma[ch].chain == 0x02) {
		if (dma[ch].btc > 0) {
			// Load next block
			LoadDMA(ch);
			return TRUE;
		}
	}

	// Chain chaining processing
	if (dma[ch].chain == 0x03) {
		if (dma[ch].bar != 0) {
			// Load next block
			LoadDMA(ch);
			return TRUE;
		}
	}

	// DMA end
#if defined(DMAC_LOG)
	LOG1(Log::Normal, "Channel %d DMA end", ch);
#endif	// DMAC_LOG

	// Flag set, interrupt
	dma[ch].act = FALSE;
	dma[ch].coc = TRUE;
	Interrupt();

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Memory address difference table
//
//---------------------------------------------------------------------------
const int DMAC::MemDiffTable[8][4] = {
	{ 0, 1, -1, 0},		// 8bit, byte
	{ 0, 2, -2, 0},		// 8bit, word
	{ 0, 4, -4, 0},		// 8bit, longword
	{ 0, 1, -1, 0},		// 8bit, pack byte
	{ 0, 1, -1, 0},		// 16bit, byte
	{ 0, 2, -2, 0},		// 16bit, word
	{ 0, 4, -4, 0},		// 16bit, longword
	{ 0, 1, -1, 0}		// 16bit, pack byte
};

//---------------------------------------------------------------------------
//
//	Device address difference table
//
//---------------------------------------------------------------------------
const int DMAC::DevDiffTable[8][4] = {
	{ 0, 2, -2, 0},		// 8bit, byte
	{ 0, 4, -4, 0},		// 8bit, word
	{ 0, 8, -8, 0},		// 8bit, longword
	{ 0, 2, -2, 0},		// 8bit, pack byte
	{ 0, 1, -1, 0},		// 16bit, byte
	{ 0, 2, -2, 0},		// 16bit, word
	{ 0, 4, -4, 0},		// 16bit, longword
	{ 0, 1, -1, 0}		// 16bit, pack byte
};

