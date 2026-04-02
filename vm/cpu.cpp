//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ CPU(MC68000) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "iosc.h"
#include "mfp.h"
#include "vm.h"
#include "log.h"
#include "memory.h"
#include "dmac.h"
#include "scc.h"
#include "midi.h"
#include "scsi.h"
#include "fileio.h"
#include "cpu.h"
#include "m68k.h"

//---------------------------------------------------------------------------
//
//	Interface with assembler core
//
//---------------------------------------------------------------------------
#if defined(__cplusplus)
extern "C" {
#endif	// __cplusplus

//---------------------------------------------------------------------------
//
//	Static work
//
//---------------------------------------------------------------------------
static CPU *cpu;						// CPU

// Pending interrupt vectors by level (1..7). -1 means autovector.
static int g_pending_vector[8] = {0, -1, -1, -1, -1, -1, -1, -1};
static DWORD g_pending_mask = 0;

static int GetHighestPendingIRQ(void)
{
	for (int level = 7; level >= 1; level--) {
		if (g_pending_mask & (1u << level)) {
			return level;
		}
	}
	return 0;
}

//---------------------------------------------------------------------------
//
//	External definitions
//
//---------------------------------------------------------------------------
// Forward declarations removed - now in musashi_adapter.h
// DWORD s68000fbpc(void);
// void s68000buserr(DWORD addr, DWORD param);
// Bus error

//---------------------------------------------------------------------------
//
//	RESET instruction handler
//
//---------------------------------------------------------------------------
static void cpu_resethandler(void)
{
	cpu->ResetInst();
}

//---------------------------------------------------------------------------
//
//	Interrupt ACK
//
//---------------------------------------------------------------------------
int musashi_int_ack(int level)
{
	int vector;

	vector = g_pending_vector[level];
	g_pending_vector[level] = -1;
	g_pending_mask &= ~(1u << level);

	::m68k_set_irq(GetHighestPendingIRQ());
	cpu->IntAck(level);

	if (vector < 0) {
		return M68K_INT_ACK_AUTOVECTOR;
	}
	return vector;
}

int musashi_int_ack_callback(int int_level)
{
	return musashi_int_ack(int_level);
}

void s68000intack(int level)
{
	cpu->IntAck(level);
}

//---------------------------------------------------------------------------
//
//	Bus error logging
//
//---------------------------------------------------------------------------
void s68000buserrlog(DWORD addr, DWORD stat)
{
	cpu->BusErrLog(addr, stat);
}

//---------------------------------------------------------------------------
//
//	Address error logging
//
//---------------------------------------------------------------------------
void s68000addrerrlog(DWORD addr, DWORD stat)
{
	cpu->AddrErrLog(addr, stat);
}

#if defined(__cplusplus)
}
#endif	// __cplusplus

//===========================================================================
//
//	CPU
//
//===========================================================================
//#define CPU_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CPU::CPU(VM *p) : Device(p)
{
	// Initialize device ID
	dev.id = MAKEID('C', 'P', 'U', ' ');
	dev.desc = "MPU (MC68000)";

	// Initialize pointers
	memory = NULL;
	dmac = NULL;
	mfp = NULL;
	iosc = NULL;
	scc = NULL;
	midi = NULL;
	scsi = NULL;
	scheduler = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPU::Init()
{
	ASSERT(this);

	// Base class
	if (!Device::Init()) {
		return FALSE;
	}

	// CPU storage
	::cpu = this;

	// Get memory
	memory = (Memory*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(memory);

	// Get DMAC
	dmac = (DMAC*)vm->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	ASSERT(dmac);

	// Get MFP
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(mfp);

	// Get IOSC
	iosc = (IOSC*)vm->SearchDevice(MAKEID('I', 'O', 'S', 'C'));
	ASSERT(iosc);

	// Get SCC
	scc = (SCC*)vm->SearchDevice(MAKEID('S', 'C', 'C', ' '));
	ASSERT(scc);

	// Get MIDI
	midi = (MIDI*)vm->SearchDevice(MAKEID('M', 'I', 'D', 'I'));
	ASSERT(midi);

	// Get SCSI
	scsi = (SCSI*)vm->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(scsi);

	// Get scheduler
	scheduler = (Scheduler*)vm->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(scheduler);

	// Create CPU core jump table
	::s68000init();
	for (int i=1; i<=7; i++) {
		g_pending_vector[i] = -1;
	}
	g_pending_mask = 0;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CPU::Cleanup()
{
	ASSERT(this);

	// To base class
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL CPU::Reset()
{
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "Reset");

	// Clear error address and error time
	sub.erraddr = 0;
	sub.errtime = 0;

	// Clear interrupt count
	for (i=0; i<8; i++) {
		sub.intreq[i] = 0;
		sub.intack[i] = 0;
	}

	// Create memory context (reset only)
	memory->MakeContext(TRUE);

	// Reset
	::s68000reset();
	::s68000context.resethandler = cpu_resethandler;
	::s68000context.odometer = 0;

	::m68k_set_irq(0);
	for (i=1; i<=7; i++) {
		g_pending_vector[i] = -1;
	}
	g_pending_mask = 0;

	// Create memory context (normal)
	memory->MakeContext(FALSE);
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPU::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;
	cpu_t cpu;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Save");

	// Get context
	GetCPU(&cpu);

	// Save size
	sz = sizeof(cpu_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save entity
	if (!fio->Write(&cpu, (int)sz)) {
		return FALSE;
	}

	// Save size (sub)
	sz = sizeof(cpusub_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save entity (sub)
	if (!fio->Write(&sub, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPU::Load(Fileio *fio, int /*ver*/)
{
	cpu_t cpu;
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Load");

	// Load size and verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(cpu_t)) {
		return FALSE;
	}

	// Load entity
	if (!fio->Read(&cpu, (int)sz)) {
		return FALSE;
	}

	// Apply (after reset)
	memory->MakeContext(TRUE);
	::s68000reset();
	memory->MakeContext(FALSE);
	SetCPU(&cpu);

	// Load size and verify (sub)
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(cpusub_t)) {
		return FALSE;
	}

	// Load entity (sub)
	if (!fio->Read(&sub, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL CPU::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);

	LOG0(Log::Normal, "Apply configuration");
}

//---------------------------------------------------------------------------
//
//	Get CPU registers
//
//---------------------------------------------------------------------------
void FASTCALL CPU::GetCPU(cpu_t *buffer) const
{
	int i;

	ASSERT(this);
	ASSERT(buffer);

	// Dreg, Areg
	for (i=0; i<8; i++) {
		buffer->dreg[i] = ::s68000context.dreg[i];
		buffer->areg[i] = ::s68000context.areg[i];
	}

	// Interrupts
	for (i=0; i<8; i++) {
		if (i == 0) {
			buffer->intr[i] = g_pending_mask;
		}
		else {
			buffer->intr[i] = (g_pending_vector[i] < 0) ? 0xFF : (DWORD)(g_pending_vector[i] & 0xFF);
		}
		buffer->intreq[i] = sub.intreq[i];
		buffer->intack[i] = sub.intack[i];
	}

	// Others
	buffer->sp = ::s68000context.asp;
	buffer->pc = ::s68000context.pc;
	buffer->sr = (DWORD)::s68000context.sr;
	buffer->odd = ::s68000context.odometer;
}

//---------------------------------------------------------------------------
//
//	Set CPU registers
//
//---------------------------------------------------------------------------
void FASTCALL CPU::SetCPU(const cpu_t *buffer)
{
	int i;
	S68000CONTEXT context;

	ASSERT(this);
	ASSERT(buffer);

	// Get context
	::s68000GetContext(&context);

	// Dreg, Areg
	for (i=0; i<8; i++) {
		context.dreg[i] = buffer->dreg[i];
		context.areg[i] = buffer->areg[i];
	}

	// Interrupts
	for (i=0; i<8; i++) {
		if (i == 0) {
			g_pending_mask = buffer->intr[i];
		}
		else {
			DWORD v = buffer->intr[i] & 0xFF;
			g_pending_vector[i] = (v == 0xFF) ? -1 : (int)v;
		}
		sub.intreq[i] = buffer->intreq[i];
		sub.intack[i] = buffer->intack[i];
	}

	// Others
	context.asp = buffer->sp;
	context.pc = buffer->pc;
	context.sr = (WORD)buffer->sr;
	context.odometer = buffer->odd;

	// Set context
	::s68000SetContext(&context);
	::m68k_set_irq(GetHighestPendingIRQ());
}

//---------------------------------------------------------------------------
//
//	Interrupt
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPU::Interrupt(int level, int vector)
{
	ASSERT(this);
	ASSERT((level >= 1) && (level <= 7));
	ASSERT(vector >= -1);

	if (g_pending_mask & (1u << level)) {
		return FALSE;
	}

	g_pending_vector[level] = vector;
	g_pending_mask |= (1u << level);
	::m68k_set_irq(GetHighestPendingIRQ());
	::s68000releaseTimeslice();

#if defined(CPU_LOG)
	LOG2(Log::Normal, "IRQ request accepted level%d vector$%02X", level, vector);
#endif	// CPU_LOG
	sub.intreq[level]++;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Interrupt ACK
//
//---------------------------------------------------------------------------
void FASTCALL CPU::IntAck(int level)
{
	ASSERT(this);
	ASSERT((level >= 1) && (level <= 7));

#if defined(CPU_LOG)
	LOG1(Log::Normal, "Interrupt request ACK level %d", level);
#endif	// CPU_LOG

	// Increment counter
	sub.intack[level]++;

	// Per interrupt level
	switch (level) {
		// IOSC, SCSI (internal)
		case 1:
			iosc->IntAck();
			scsi->IntAck(1);
			break;

		// MIDI, SCSI (level 2)
		case 2:
			midi->IntAck(2);
			scsi->IntAck(2);
			break;

		// DMAC
		case 3:
			dmac->IntAck();
			break;

		// MIDI, SCSI (level 4)
		case 4:
			midi->IntAck(4);
			scsi->IntAck(4);
			break;

		// SCC
		case 5:
			scc->IntAck();
			break;

		// MFP
		case 6:
			mfp->IntAck();
			break;

		// Others
		default:
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Cancel interrupt
//
//---------------------------------------------------------------------------
void FASTCALL CPU::IntCancel(int level)
{
	ASSERT(this);
	ASSERT((level >= 1) && (level <= 7));

	if (g_pending_mask & (1u << level)) {
#if defined(CPU_LOG)
		LOG1(Log::Normal, "Interrupt cancel level%d", level);
#endif	// CPU_LOG

		g_pending_mask &= ~(1u << level);
		g_pending_vector[level] = -1;
		if (sub.intreq[level] > 0) {
			sub.intreq[level]--;
		}
	}

	::m68k_set_irq(GetHighestPendingIRQ());
}

//---------------------------------------------------------------------------
//
//	RESET instruction
//
//---------------------------------------------------------------------------
void FASTCALL CPU::ResetInst()
{
	Device *device;

	ASSERT(this);
	LOG0(Log::Detail, "RESET instruction");

	// Get memory
	device = (Device*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(device);

	// Reset all devices. Depends on how far the CPU's RESET signal propagates
	while (device) {
		device->Reset();
		device = device->GetNextDevice();
	}
}

//---------------------------------------------------------------------------
//
//	Bus error
//	* Bus errors from DMA transfers also come here
//	* Bus errors detected internally by the CPU core do not pass through here
//
//---------------------------------------------------------------------------
void FASTCALL CPU::BusErr(DWORD addr, BOOL read)
{
	DWORD pc;
	DWORD stat;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);

	// Ask DMAC if transferring. If in DMAC, delegate to DMAC
	if (dmac->IsDMA()) {
		dmac->BusErr(addr, read);
		return;
	}

	// Ignore if address is previous address+2 and time is same (LONG access)
	if (addr == (sub.erraddr + 2)) {
		if (scheduler->GetTotalTime() == sub.errtime) {
			return;
		}
	}

	// Update address and time
	sub.erraddr = addr;
	sub.errtime = scheduler->GetTotalTime();

	// Get PC (at the opcode of the relevant instruction)
	pc = GetPC();

	// Read (Word)
	stat = memory->ReadOnly(pc);
	stat <<= 8;
	stat |= memory->ReadOnly(pc + 1);
	stat <<= 16;

	// Create function code (always treated as data access)
	stat |= 0x09;
	if (::s68000context.sr & 0x2000) {
		stat |= 0x04;
	}
	if (read) {
		stat |= 0x10;
	}

	// Issue bus error
	::s68000buserr(addr, stat);
}

//---------------------------------------------------------------------------
//
//	Address error
//	* Address errors from DMA transfers also come here
//	* Address errors detected internally by the CPU core do not pass through here
//
//---------------------------------------------------------------------------
void FASTCALL CPU::AddrErr(DWORD addr, BOOL read)
{
	DWORD pc;
	DWORD stat;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT(addr & 1);

	// Ask DMAC if transferring. If in DMAC, delegate to DMAC
	if (dmac->IsDMA()) {
		dmac->AddrErr(addr, read);
		return;
	}

	// Ignore if address is previous address+2 and time is same (LONG access)
	if (addr == (sub.erraddr + 2)) {
		if (scheduler->GetTotalTime() == sub.errtime) {
			return;
		}
	}

	// Update address and time
	sub.erraddr = addr;
	sub.errtime = scheduler->GetTotalTime();

	// Get PC (at the opcode of the relevant instruction)
	pc = GetPC();

	// Read (Word)
	stat = memory->ReadOnly(pc);
	stat <<= 8;
	stat |= memory->ReadOnly(pc + 1);
	stat <<= 16;

	// Create function code (always treated as data access)
	stat |= 0x8009;
	if (::s68000context.sr & 0x2000) {
		stat |= 0x04;
	}
	if (read) {
		stat |= 0x10;
	}

	// Issue bus error (branches to address error internally)
	::s68000buserr(addr, stat);
}

//---------------------------------------------------------------------------
//
//	Bus error logging
//	* Also passes through when bus error is detected by CPU core
//
//---------------------------------------------------------------------------
void FASTCALL CPU::BusErrLog(DWORD addr, DWORD stat)
{
	ASSERT(this);

	// Always mask (may exceed 24bit)
	addr &= 0xffffff;

	if (stat & 0x10) {
		LOG1(Log::Warning, "Bus error (read) $%06X", addr);
	}
	else {
		LOG1(Log::Warning, "Bus error (write) $%06X", addr);
	}
}

//---------------------------------------------------------------------------
//
//	Address error logging
//	* Also passes through when address error is detected by CPU core
//
//---------------------------------------------------------------------------
void FASTCALL CPU::AddrErrLog(DWORD addr, DWORD stat)
{
	ASSERT(this);

	// Always mask (may exceed 24bit)
	addr &= 0xffffff;

	if (stat & 0x10) {
		LOG1(Log::Warning, "Address error (read) $%06X", addr);
	}
	else {
		LOG1(Log::Warning, "Address error (write) $%06X", addr);
	}
}
