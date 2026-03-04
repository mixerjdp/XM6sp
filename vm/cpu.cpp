//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 窶嗤窶喇ﾂ．(ytanaka@ipc-tokai.or.jp)
//	[ CPU(MC68000) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "iosc.h"
#include "mfp.h"
#include "vm.h"
#include "schedule.h"
#include "log.h"
#include "memory.h"
#include "dmac.h"
#include "scc.h"
#include "midi.h"
#include "scsi.h"
#include "fileio.h"
#include "cpu.h"
#include "musashi/m68k.h"

extern bool musashi_is_resetting;

//---------------------------------------------------------------------------
//
//	ﾆ但ﾆ短ﾆ停愴置ﾆ停ｰﾆ坦ﾆ但窶堙・堙姑辰ﾆ停愴耽ﾆ稚ﾆ巽ﾂーﾆ湛
//
//---------------------------------------------------------------------------
#if defined(__cplusplus)
extern "C" {
#endif	// __cplusplus

//---------------------------------------------------------------------------
//
//	ﾆ湛ﾆ耽ﾆ弾ﾆ達ﾆ鍛ﾆ誰 ﾆ陳渉ーﾆ誰
//
//---------------------------------------------------------------------------
static CPU *cpu;

// Pending interrupt vectors by level (1..7). -1 means autovector.
static int g_pending_vector[8] = {0, -1, -1, -1, -1, -1, -1, -1};
static DWORD g_pending_mask = 0;

static int GetHighestPendingIRQ()
{
	for (int level = 7; level >= 1; level--) {
		if (g_pending_mask & (1u << level)) {
			return level;
		}
	}
	return 0;
}
										// CPU

//---------------------------------------------------------------------------
//
//	ﾅO窶｢窶昶凖ｨ窶ｹ`
//
//---------------------------------------------------------------------------
void m68k_pulse_bus_error(void);
										// PCﾆ稚ﾆ達ﾂーﾆ檀ﾆ弛ﾆ鍛ﾆ誰

										// ﾆ弛ﾆ湛ﾆ竪ﾆ停ｰﾂー

//---------------------------------------------------------------------------
//
//	RESET窶督ｽ窶氾淅地ﾆ停愴檀ﾆ停ｰ
//
//---------------------------------------------------------------------------
static void cpu_resethandler(void)
{
	cpu->ResetInst();
}

void musashi_reset_callback(void)
{
	cpu_resethandler();
}

//---------------------------------------------------------------------------
//
//	ﾅ窶樞堙ｨﾂ最ｾ窶堙戡CK
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

void s68000intack(void)
{
	int sr;

	sr = m68k_get_reg(NULL, M68K_REG_SR);
	sr >>= 8;
	sr &= 0x0007;

	cpu->IntAck(sr);
}

//---------------------------------------------------------------------------
//
//	ﾆ弛ﾆ湛ﾆ竪ﾆ停ｰﾂー窶ｹLﾋ弯
//
//---------------------------------------------------------------------------
void s68000buserrlog(DWORD addr, DWORD stat)
{
	cpu->BusErrLog(addr, stat);
}

//---------------------------------------------------------------------------
//
//	ﾆ但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停ｰﾂー窶ｹLﾋ弯
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
//	ﾆ坦ﾆ停愴湛ﾆ暖ﾆ停ｰﾆ誰ﾆ耽
//
//---------------------------------------------------------------------------
CPU::CPU(VM *p) : Device(p)
{
	// ﾆ断ﾆ弛ﾆ辰ﾆ湛ID窶堙ｰﾂ鞘ｰﾅﾃｺ窶ｰﾂｻ
	dev.id = MAKEID('C', 'P', 'U', ' ');
	dev.desc = "MPU (MC68000)";

	// ﾆ竹ﾆ辰ﾆ停愴耽ﾂ鞘ｰﾅﾃｺ窶ｰﾂｻ
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
//	ﾂ鞘ｰﾅﾃｺ窶ｰﾂｻ
//
//---------------------------------------------------------------------------
extern class Memory* g_musashi_memory;

BOOL FASTCALL CPU::Init()
{
	ASSERT(this);

	// ﾅﾃｮ窶怒ﾆ誰ﾆ停ｰﾆ湛
	if (!Device::Init()) {
		return FALSE;
	}

	// CPU窶ｹL窶ｰﾂｯ
	::cpu = this;

	// ﾆ陳・停堡椎ﾅｽﾃｦ窶慊ｾ
	memory = (Memory*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(memory);
	g_musashi_memory = memory;
	g_musashi_memory = memory; // Setup context for Musashi early

	// DMACﾅｽﾃｦ窶慊ｾ
	dmac = (DMAC*)vm->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	ASSERT(dmac);

	// MFPﾅｽﾃｦ窶慊ｾ
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(mfp);

	// IOSCﾅｽﾃｦ窶慊ｾ
	iosc = (IOSC*)vm->SearchDevice(MAKEID('I', 'O', 'S', 'C'));
	ASSERT(iosc);

	// SCCﾅｽﾃｦ窶慊ｾ
	scc = (SCC*)vm->SearchDevice(MAKEID('S', 'C', 'C', ' '));
	ASSERT(scc);

	// MIDIﾅｽﾃｦ窶慊ｾ
	midi = (MIDI*)vm->SearchDevice(MAKEID('M', 'I', 'D', 'I'));
	ASSERT(midi);

	// SCSIﾅｽﾃｦ窶慊ｾ
	scsi = (SCSI*)vm->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(scsi);

	// ﾆ湛ﾆ単ﾆ淡ﾆ停ｦﾂーﾆ停ｰﾅｽﾃｦ窶慊ｾ
	scheduler = (Scheduler*)vm->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(scheduler);

	// CPUﾆ坦ﾆ但窶堙姑淡ﾆ槌槌停愴致ﾆ弾ﾂーﾆ置ﾆ停ｹ窶堙ｰﾂ催ｬﾂ青ｬ
	::m68k_init();
	::m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	::m68k_set_int_ack_callback(musashi_int_ack);

	for (int i=1; i<=7; i++) {
		g_pending_vector[i] = -1;
	}
	g_pending_mask = 0;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ﾆ誰ﾆ椎ﾂーﾆ停愴但ﾆ鍛ﾆ致
//
//---------------------------------------------------------------------------
void FASTCALL CPU::Cleanup()
{
	ASSERT(this);

	// ﾅﾃｮ窶怒ﾆ誰ﾆ停ｰﾆ湛窶堙・
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	ﾆ椎ﾆ短ﾆ鍛ﾆ暖
//
//---------------------------------------------------------------------------
void FASTCALL CPU::Reset()
{
	int i;
	
	DWORD bit;

	ASSERT(this);
	LOG0(Log::Normal, "ﾆ椎ﾆ短ﾆ鍛ﾆ暖");

	// ﾆ竪ﾆ停ｰﾂーﾆ但ﾆ檀ﾆ椎槌湛ﾂ、ﾆ竪ﾆ停ｰﾂーﾅｽﾅｾﾅﾃ犯誰ﾆ椎ﾆ但
	sub.erraddr = 0;
	sub.errtime = 0;

	// ﾅ窶樞堙ｨﾂ最ｾ窶堙敞谷ﾆ脱ﾆ停愴暖ﾆ誰ﾆ椎ﾆ但
	for (i=0; i<8; i++) {
		sub.intreq[i] = 0;
		sub.intack[i] = 0;
	}

	// ﾆ陳・停堡椎ﾆ坦ﾆ停愴弾ﾆ鱈ﾆ湛ﾆ暖ﾂ催ｬﾂ青ｬ(ﾆ椎ﾆ短ﾆ鍛ﾆ暖ﾂ静ｪ窶廃)
	memory->MakeContext(TRUE);

	// ﾆ椎ﾆ短ﾆ鍛ﾆ暖
	musashi_is_resetting = true;
	::m68k_pulse_reset();
	musashi_is_resetting = false;
	::m68k_set_reset_instr_callback(cpu_resethandler);
	

	// ﾅ窶樞堙ｨﾂ最ｾ窶堙昶堙ｰ窶堋ｷ窶堙冷堙・ｽﾃｦ窶堙ｨﾂ湘≫堋ｷ
	::m68k_set_irq(0);
	for (i=1; i<=7; i++) {
		g_pending_vector[i] = -1;
	}
	g_pending_mask = 0;
	

	// ﾆ陳・停堡椎ﾆ坦ﾆ停愴弾ﾆ鱈ﾆ湛ﾆ暖ﾂ催ｬﾂ青ｬ(窶凖環湘ｭ)
	memory->MakeContext(FALSE);
}

//---------------------------------------------------------------------------
//
//	ﾆ短ﾂーﾆ置
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPU::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;
	cpu_t cpu;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "ﾆ短ﾂーﾆ置");

	// ﾆ坦ﾆ停愴弾ﾆ鱈ﾆ湛ﾆ暖ﾅｽﾃｦ窶慊ｾ
	GetCPU(&cpu);

	// ﾆ探ﾆ辰ﾆ炭窶堙ｰﾆ短ﾂーﾆ置
	sz = sizeof(cpu_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// ﾅｽﾃ窶佚娯堙ｰﾆ短ﾂーﾆ置
	if (!fio->Write(&cpu, (int)sz)) {
		return FALSE;
	}

	// ﾆ探ﾆ辰ﾆ炭窶堙ｰﾆ短ﾂーﾆ置(ﾆ探ﾆ置)
	sz = sizeof(cpusub_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// ﾅｽﾃ窶佚娯堙ｰﾆ短ﾂーﾆ置(ﾆ探ﾆ置)
	if (!fio->Write(&sub, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ﾆ陳債ーﾆ檀
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPU::Load(Fileio *fio, int /*ver*/)
{
	cpu_t cpu;
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "ﾆ陳債ーﾆ檀");

	// ﾆ探ﾆ辰ﾆ炭窶堙ｰﾆ陳債ーﾆ檀ﾂ、ﾂ湘・坂｡
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(cpu_t)) {
		return FALSE;
	}

	// ﾅｽﾃ窶佚娯堙ｰﾆ陳債ーﾆ檀
	if (!fio->Read(&cpu, (int)sz)) {
		return FALSE;
	}

	// 窶廳窶廃(ﾆ椎ﾆ短ﾆ鍛ﾆ暖窶堋ｵ窶堙・堋ｩ窶堙ｧﾂ行窶堋､)
	memory->MakeContext(TRUE);
	musashi_is_resetting = true;
	::m68k_pulse_reset();
	musashi_is_resetting = false;
	memory->MakeContext(FALSE);
	SetCPU(&cpu);

	// ﾆ探ﾆ辰ﾆ炭窶堙ｰﾆ陳債ーﾆ檀ﾂ、ﾂ湘・坂｡(ﾆ探ﾆ置)
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(cpusub_t)) {
		return FALSE;
	}

	// ﾅｽﾃ窶佚娯堙ｰﾆ陳債ーﾆ檀(ﾆ探ﾆ置)
	if (!fio->Read(&sub, (int)sz)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ﾂ静昶凖ｨ窶廳窶廃
//
//---------------------------------------------------------------------------
void FASTCALL CPU::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);

	LOG0(Log::Normal, "ﾂ静昶凖ｨ窶廳窶廃");
}

//---------------------------------------------------------------------------
//
//	CPUﾆ椎槌淡ﾆ湛ﾆ耽ﾅｽﾃｦ窶慊ｾ
//
//---------------------------------------------------------------------------
void FASTCALL CPU::GetCPU(cpu_t *buffer) const
{
	int i;

	ASSERT(this);
	ASSERT(buffer);

	// Dreg, Areg
	for (i=0; i<8; i++) {
		buffer->dreg[i] = ::m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_D0 + i));
		buffer->areg[i] = ::m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_A0 + i));
	}

	// ﾅ窶樞堙ｨﾂ最ｾ窶堙・
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

	// 窶堋ｻ窶堙娯伉ｼ
	buffer->sp = ::m68k_get_reg(NULL, M68K_REG_A7);
	buffer->pc = ::m68k_get_reg(NULL, M68K_REG_PC);
	buffer->sr = ::m68k_get_reg(NULL, M68K_REG_SR);
	buffer->odd = 0;
}

//---------------------------------------------------------------------------
//
//	CPUﾆ椎槌淡ﾆ湛ﾆ耽ﾂ静昶凖ｨ
//
//---------------------------------------------------------------------------
void FASTCALL CPU::SetCPU(const cpu_t *buffer)
{
	int i;
	

	ASSERT(this);
	ASSERT(buffer);

	// ﾆ坦ﾆ停愴弾ﾆ鱈ﾆ湛ﾆ暖ﾅｽﾃｦ窶慊ｾ
	::m68k_set_irq(0);

	// Dreg, Areg
	for (i=0; i<8; i++) {
		::m68k_set_reg((m68k_register_t)(M68K_REG_D0 + i), buffer->dreg[i]);
		::m68k_set_reg((m68k_register_t)(M68K_REG_A0 + i), buffer->areg[i]);
	}

	// ﾅ窶樞堙ｨﾂ最ｾ窶堙・
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

	::m68k_set_irq(GetHighestPendingIRQ());

	// 窶堋ｻ窶堙娯伉ｼ
	::m68k_set_reg(M68K_REG_A7, buffer->sp);
	::m68k_set_reg(M68K_REG_PC, buffer->pc);
	::m68k_set_reg(M68K_REG_SR, buffer->sr);
	

	// ﾆ坦ﾆ停愴弾ﾆ鱈ﾆ湛ﾆ暖ﾂ静昶凖ｨ
	
}

//---------------------------------------------------------------------------
//
//	ﾅ窶樞堙ｨﾂ最ｾ窶堙・
//
//---------------------------------------------------------------------------
BOOL FASTCALL CPU::Interrupt(int level, int vector)
{
	// INTERRUPT SWITCH窶堙俄堙ｦ窶堙ｩNMIﾅ窶樞堙ｨﾂ最ｾ窶堙昶堙哉遅ﾆ誰ﾆ耽-1
	ASSERT(this);
	ASSERT((level >= 1) && (level <= 7));
	ASSERT(vector >= -1);

	// Keep Starscream semantics: if this IRQ level is already pending,
	// reject duplicate requests and return FALSE.
	if (g_pending_mask & (1u << level)) {
		return FALSE;
	}

	// Record vector and raise highest pending level.
	g_pending_vector[level] = vector;
	g_pending_mask |= (1u << level);
	::m68k_set_irq(GetHighestPendingIRQ());
	::s68000releaseTimeslice();

	{
#if defined(CPU_LOG)
		LOG2(Log::Normal, "ﾅ窶樞堙ｨﾂ最ｾ窶堙昶牌窶ｹﾂ・ｽﾃｳ窶板・ﾆ椎槌遅ﾆ停ｹ%d ﾆ遅ﾆ誰ﾆ耽$%02X", level, vector);
#endif	// CPU_LOG
		sub.intreq[level]++;
		return TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	ﾅ窶樞堙ｨﾂ最ｾ窶堙戡CK
//
//---------------------------------------------------------------------------
void FASTCALL CPU::IntAck(int level)
{
	ASSERT(this);
	ASSERT((level >= 1) && (level <= 7));

#if defined(CPU_LOG)
	LOG1(Log::Normal, "ﾅ窶樞堙ｨﾂ最ｾ窶堙昶牌窶ｹﾂ、CK ﾆ椎槌遅ﾆ停ｹ%d", level);
#endif	// CPU_LOG

	// ﾆ谷ﾆ脱ﾆ停愴暖ﾆ但ﾆ鍛ﾆ致
	sub.intack[level]++;

	// ﾅ窶樞堙ｨﾂ最ｾ窶堙敞椎槌遅ﾆ停ｹ窶｢ﾃ・
	switch (level) {
		// IOSC,SCSI(窶愿窶伉)
		case 1:
			iosc->IntAck();
			scsi->IntAck(1);
			break;

		// MIDI,SCSI(ﾆ椎槌遅ﾆ停ｹ2)
		case 2:
			midi->IntAck(2);
			scsi->IntAck(2);
			break;

		// DMAC
		case 3:
			dmac->IntAck();
			break;

		// MIDI,SCSI(ﾆ椎槌遅ﾆ停ｹ4)
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

		// 窶堋ｻ窶堙娯伉ｼ
		default:
			break;
	}
}

//---------------------------------------------------------------------------
//
//	ﾅ窶樞堙ｨﾂ最ｾ窶堙敞鱈ﾆ槌槌停愴短ﾆ停ｹ
//
//---------------------------------------------------------------------------
void FASTCALL CPU::IntCancel(int level)
{
	ASSERT(this);
	ASSERT((level >= 1) && (level <= 7));

	if (g_pending_mask & (1u << level)) {
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
//	RESET窶督ｽ窶氾・
//
//---------------------------------------------------------------------------
void FASTCALL CPU::ResetInst()
{
	Device *device;

	ASSERT(this);
	LOG0(Log::Detail, "RESET");

	// ﾆ陳・停堡椎窶堙ｰﾅｽﾃｦ窶慊ｾ
	device = (Device*)vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(device);

	// ﾆ陳・停堡椎ﾆ断ﾆ弛ﾆ辰ﾆ湛窶堙俄佚寂堋ｵ窶堙・堋ｷ窶堙冷堙・椎ﾆ短ﾆ鍛ﾆ暖窶堙ｰ窶堋ｩ窶堋ｯ窶堙・堋ｨ窶堋ｭ
	// ﾂ青ｳﾅm窶堙俄堙債、CPU窶堙軍ESETﾂ信ﾂ坂窶堋ｪ窶堙・堋ｱ窶堙懌堙・彖窶堙ｭ窶堙≫堙・堋｢窶堙ｩ窶堋ｩ窶堙俄堙ｦ窶堙ｩ
	while (device) {
		device->Reset();
		device = device->GetNextDevice();
	}
}

//---------------------------------------------------------------------------
//
//	ﾆ弛ﾆ湛ﾆ竪ﾆ停ｰﾂー
//	ﾂ・ｦDMA窶彎窶倪披堙俄堙ｦ窶堙ｩﾆ弛ﾆ湛ﾆ竪ﾆ停ｰﾂー窶堙窶堋ｱ窶堋ｱ窶堙俄藩・堙ｩ
//	ﾂ・ｦCPUﾆ坦ﾆ但窶愿窶｢窶昶堙・弛ﾆ湛ﾆ竪ﾆ停ｰﾂー窶堙・敖ｻ窶凖ｨ窶堋ｵ窶堋ｽﾂ湘ｪﾂ坂｡窶堙債、窶堋ｱ窶堋ｱ窶堙ｰﾅ弛窶燃窶堋ｵ窶堙遺堋｢
//
//---------------------------------------------------------------------------
void FASTCALL CPU::BusErr(DWORD addr, BOOL read)
{
	DWORD pc;
	DWORD stat;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);

	// DMAC窶堙俄彎窶倪披吮窶堋ｩ窶｢ﾂｷ窶堋ｭﾂ。DMAC窶吮窶堙遺堙ｧDMAC窶堙俄戮窶堋ｹ窶堙ｩ
	if (dmac->IsDMA()) {
		dmac->BusErr(addr, read);
		return;
	}

	// ﾆ但ﾆ檀ﾆ椎槌湛窶堋ｪ窶楼窶ｰﾃｱ窶堙姑但ﾆ檀ﾆ椎槌湛+2窶堙・、窶堋ｩ窶堙でｽﾅｾﾅﾃ披堋ｪ窶慊ｯ窶堋ｶ窶堙遺堙ｧ窶督ｳﾅｽ窶ｹ窶堋ｷ窶堙ｩ(LONGﾆ但ﾆ誰ﾆ短ﾆ湛)
	if (addr == (sub.erraddr + 2)) {
		if (scheduler->GetTotalTime() == sub.errtime) {
			return;
		}
	}

	// ﾆ但ﾆ檀ﾆ椎槌湛窶堙・ｽﾅｾﾅﾃ披堙ｰﾂ更ﾂ新
	sub.erraddr = addr;
	sub.errtime = scheduler->GetTotalTime();

	// PCﾅｽﾃｦ窶慊ｾ(ﾅY窶懌凪督ｽ窶氾溪堙姑棚ﾆ馳ﾆ坦ﾂーﾆ檀窶堙架・岩冰窶堋ｷ窶堙ｩ)
	pc = GetPC();

	// 窶愿・堙敖出窶堋ｵ(Word)
	stat = memory->ReadOnly(pc);
	stat <<= 8;
	stat |= memory->ReadOnly(pc + 1);
	stat <<= 16;

	// ﾆ稚ﾆ叩ﾆ停愴誰ﾆ歎ﾆ停｡ﾆ停愴坦ﾂーﾆ檀ﾂ催ｬﾂ青ｬ(ﾂ湘ｭ窶堙家断ﾂーﾆ耽ﾆ但ﾆ誰ﾆ短ﾆ湛窶堙・堙昶堙遺堋ｷ)
	stat |= 0x09;
	if (::m68k_get_reg(NULL, M68K_REG_SR) & 0x2000) {
		stat |= 0x04;
	}
	if (read) {
		stat |= 0x10;
	}

	// ﾆ弛ﾆ湛ﾆ竪ﾆ停ｰﾂー窶敖ｭﾂ行
	::m68k_pulse_bus_error();
}

//---------------------------------------------------------------------------
//
//	ﾆ但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停ｰﾂー
//	ﾂ・ｦDMA窶彎窶倪披堙俄堙ｦ窶堙ｩﾆ但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停ｰﾂー窶堙窶堋ｱ窶堋ｱ窶堙俄藩・堙ｩ
//	ﾂ・ｦCPUﾆ坦ﾆ但窶愿窶｢窶昶堙・但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停ｰﾂー窶堙・敖ｻ窶凖ｨ窶堋ｵ窶堋ｽﾂ湘ｪﾂ坂｡窶堙債、窶堋ｱ窶堋ｱ窶堙ｰﾅ弛窶燃窶堋ｵ窶堙遺堋｢
//
//---------------------------------------------------------------------------
void FASTCALL CPU::AddrErr(DWORD addr, BOOL read)
{
	DWORD pc;
	DWORD stat;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT(addr & 1);

	// DMAC窶堙俄彎窶倪披吮窶堋ｩ窶｢ﾂｷ窶堋ｭﾂ。DMAC窶吮窶堙遺堙ｧDMAC窶堙俄戮窶堋ｹ窶堙ｩ
	if (dmac->IsDMA()) {
		dmac->AddrErr(addr, read);
		return;
	}

	// ﾆ但ﾆ檀ﾆ椎槌湛窶堋ｪ窶楼窶ｰﾃｱ窶堙姑但ﾆ檀ﾆ椎槌湛+2窶堙・、窶堋ｩ窶堙でｽﾅｾﾅﾃ披堋ｪ窶慊ｯ窶堋ｶ窶堙遺堙ｧ窶督ｳﾅｽ窶ｹ窶堋ｷ窶堙ｩ(LONGﾆ但ﾆ誰ﾆ短ﾆ湛)
	if (addr == (sub.erraddr + 2)) {
		if (scheduler->GetTotalTime() == sub.errtime) {
			return;
		}
	}

	// ﾆ但ﾆ檀ﾆ椎槌湛窶堙・ｽﾅｾﾅﾃ披堙ｰﾂ更ﾂ新
	sub.erraddr = addr;
	sub.errtime = scheduler->GetTotalTime();

	// PCﾅｽﾃｦ窶慊ｾ(ﾅY窶懌凪督ｽ窶氾溪堙姑棚ﾆ馳ﾆ坦ﾂーﾆ檀窶堙架・岩冰窶堋ｷ窶堙ｩ)
	pc = GetPC();

	// 窶愿・堙敖出窶堋ｵ(Word)
	stat = memory->ReadOnly(pc);
	stat <<= 8;
	stat |= memory->ReadOnly(pc + 1);
	stat <<= 16;

	// ﾆ稚ﾆ叩ﾆ停愴誰ﾆ歎ﾆ停｡ﾆ停愴坦ﾂーﾆ檀ﾂ催ｬﾂ青ｬ(ﾂ湘ｭ窶堙家断ﾂーﾆ耽ﾆ但ﾆ誰ﾆ短ﾆ湛窶堙・堙昶堙遺堋ｷ)
	stat |= 0x8009;
	if (::m68k_get_reg(NULL, M68K_REG_SR) & 0x2000) {
		stat |= 0x04;
	}
	if (read) {
		stat |= 0x10;
	}

	// ﾆ弛ﾆ湛ﾆ竪ﾆ停ｰﾂー窶敖ｭﾂ行(窶愿窶｢窶昶堙・但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停ｰﾂー窶堙問｢ﾂｪﾅﾃｲ)
	::m68k_pulse_bus_error();
}

//---------------------------------------------------------------------------
//
//	ﾆ弛ﾆ湛ﾆ竪ﾆ停ｰﾂー窶ｹLﾋ弯
//	ﾂ・ｦCPUﾆ坦ﾆ但窶愿窶｢窶昶堙・弛ﾆ湛ﾆ竪ﾆ停ｰﾂー窶堙・敖ｻ窶凖ｨ窶堋ｵ窶堋ｽﾂ湘ｪﾂ坂｡窶堙ﾂ、窶堋ｱ窶堋ｱ窶堙ｰ窶凖岩堙ｩ
//
//---------------------------------------------------------------------------
void FASTCALL CPU::BusErrLog(DWORD addr, DWORD stat)
{
	ASSERT(this);

	// 窶｢K窶堋ｸﾆ筑ﾆ湛ﾆ誰(24bit窶堙ｰ窶卍ｴ窶堋ｦ窶堙ｩﾂ湘ｪﾂ坂｡窶堋ｪ窶堋窶堙ｩ)
	addr &= 0xffffff;

	if (stat & 0x10) {
		LOG1(Log::Warning, "ﾆ弛ﾆ湛ﾆ竪ﾆ停ｰﾂー(窶愿・堙敖最ｾ窶堙・ $%06X", addr);
	}
	else {
		LOG1(Log::Warning, "ﾆ弛ﾆ湛ﾆ竪ﾆ停ｰﾂー(ﾂ鞘倪堋ｫﾂ最ｾ窶堙・ $%06X", addr);
	}
}

//---------------------------------------------------------------------------
//
//	ﾆ但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停ｰﾂー窶ｹLﾋ弯
//	ﾂ・ｦCPUﾆ坦ﾆ但窶愿窶｢窶昶堙・但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停ｰﾂー窶堙・敖ｻ窶凖ｨ窶堋ｵ窶堋ｽﾂ湘ｪﾂ坂｡窶堙ﾂ、窶堋ｱ窶堋ｱ窶堙ｰ窶凖岩堙ｩ
//
//---------------------------------------------------------------------------
void FASTCALL CPU::AddrErrLog(DWORD addr, DWORD stat)
{
	ASSERT(this);

	// 窶｢K窶堋ｸﾆ筑ﾆ湛ﾆ誰(24bit窶堙ｰ窶卍ｴ窶堋ｦ窶堙ｩﾂ湘ｪﾂ坂｡窶堋ｪ窶堋窶堙ｩ)
	addr &= 0xffffff;

	if (stat & 0x10) {
		LOG1(Log::Warning, "ﾆ但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停ｰﾂー(窶愿・堙敖最ｾ窶堙・ $%06X", addr);
	}
	else {
		LOG1(Log::Warning, "ﾆ但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停ｰﾂー(ﾂ鞘倪堋ｫﾂ最ｾ窶堙・ $%06X", addr);
	}
}
