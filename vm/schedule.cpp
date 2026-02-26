//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �ｼｰ�ｼｩ�ｼ�(ytanaka@ipc-tokai.or.jp)
//	[ 繧ｹ繧ｱ繧ｸ繝･繝ｼ繝ｩ ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "schedule.h"
#include "vm.h"
#include "log.h"
#include "cpu.h"
#include "event.h"
#include "dmac.h"
#include "core_asm.h"
#include "config.h"
#include "fileio.h"

//===========================================================================
//
//	繧ｹ繧ｱ繧ｸ繝･繝ｼ繝ｩ
//
//===========================================================================
//#define SCHEDULER_LOG

//---------------------------------------------------------------------------
//
//	繧､繝吶Φ繝域､懃ｴ｢繝ｻ譖ｴ譁ｰ繧偵い繧ｻ繝ｳ繝悶Λ蛹�
//
//---------------------------------------------------------------------------
#if defined(_MSC_VER) && defined(_M_IX86)
#define SCHEDULER_ASM
#endif	// _MSC_VER

//---------------------------------------------------------------------------
//
//	繧ｳ繝ｳ繧ｹ繝医Λ繧ｯ繧ｿ
//
//---------------------------------------------------------------------------
Scheduler::Scheduler(VM *p) : Device(p)
{
	int i;

	// 繝�繝舌う繧ｹID繧貞�晄悄蛹�
	dev.id = MAKEID('S', 'C', 'H', 'E');
	dev.desc = "Scheduler";

	// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝亥句挨
	for (i=0; i<BreakMax; i++) {
		breakp[i].use = FALSE;
		breakp[i].addr = 0;
		breakp[i].enable = FALSE;
		breakp[i].time = 0;
		breakp[i].count = 0;
	}

	// 譎る俣
	sch.total = 0;
	sch.one = 0;
	sch.sound = 0;

	// CPU
	sch.clock = 0;
	sch.speed = 2200;
	sch.cycle = 0;
	sch.time = 0;

	// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝�
	sch.brk = FALSE;
	sch.check = FALSE;

	// 繧､繝吶Φ繝�
	sch.first = NULL;
	sch.exec = FALSE;

	// 繝�繝舌う繧ｹ
	cpu = NULL;
	dmac = NULL;

	// 縺昴�ｮ莉�
	dma_active = FALSE;
}

//---------------------------------------------------------------------------
//
//	蛻晄悄蛹�
//
//---------------------------------------------------------------------------
BOOL FASTCALL Scheduler::Init()
{
	ASSERT(this);

	// 蝓ｺ譛ｬ繧ｯ繝ｩ繧ｹ
	if (!Device::Init()) {
		return FALSE;
	}

	// CPU蜿門ｾ�
	ASSERT(!cpu);
	cpu = (CPU*)vm->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(cpu);

	// DMAC蜿門ｾ�
	ASSERT(!dmac);
	dmac = (DMAC*)vm->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	ASSERT(dmac);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	繧ｯ繝ｪ繝ｼ繝ｳ繧｢繝�繝�
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::Cleanup()
{
	ASSERT(this);
	ASSERT_DIAG();

	// 蝓ｺ譛ｬ繧ｯ繝ｩ繧ｹ縺ｸ
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	繝ｪ繧ｻ繝�繝�
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::Reset()
{
	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "繝ｪ繧ｻ繝�繝�");

	// 譎る俣繝ｪ繧ｻ繝�繝�(sound髯､縺�)
	sch.total = 0;
	sch.one = 0;

	// CPU繧ｵ繧､繧ｯ繝ｫ繝ｪ繧ｻ繝�繝�
	sch.cycle = 0;
	sch.time = 0;

	// 繧､繝吶Φ繝亥ｮ溯｡御ｸｭ縺ｧ縺ｪ縺�
	sch.exec = FALSE;

	// DMA螳溯｡後↑縺�
	dma_active = FALSE;

	// CPU騾溷ｺｦ險ｭ螳壹�ｯ豈主屓陦後≧(INFO.RAM蟇ｾ遲悶Ν繝ｼ繝√Φ縺ｮ縺溘ａ)
	ASSERT((sch.clock >= 0) && (sch.clock <= 5));
	SetCPUSpeed(ClockTable[sch.clock]);
}

//---------------------------------------------------------------------------
//
//	繧ｻ繝ｼ繝�
//
//---------------------------------------------------------------------------
BOOL FASTCALL Scheduler::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	LOG0(Log::Normal, "繧ｻ繝ｼ繝�");

	// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝医し繧､繧ｺ繧偵そ繝ｼ繝�
	sz = sizeof(breakp);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝亥ｮ滉ｽ薙ｒ繧ｻ繝ｼ繝�
	if (!fio->Write(breakp, (int)sz)) {
		return FALSE;
	}

	// 繧ｹ繧ｱ繧ｸ繝･繝ｼ繝ｩ繧ｵ繧､繧ｺ繧偵そ繝ｼ繝�
	sz = sizeof(scheduler_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// 繧ｹ繧ｱ繧ｸ繝･繝ｼ繝ｩ螳滉ｽ薙ｒ繧ｻ繝ｼ繝�
	if (!fio->Write(&sch, (int)sz)) {
		return FALSE;
	}

	// 繧ｵ繧､繧ｯ繝ｫ繝�繝ｼ繝悶Ν繧偵そ繝ｼ繝�
	if (!fio->Write(CycleTable, sizeof(CycleTable))) {
		return FALSE;
	}

	// dma_active繧偵そ繝ｼ繝�(version 2.01)
	if (!fio->Write(&dma_active, sizeof(dma_active))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	繝ｭ繝ｼ繝�
//
//---------------------------------------------------------------------------
BOOL FASTCALL Scheduler::Load(Fileio *fio, int ver)
{
	size_t sz;
	Event *first;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x200);
	ASSERT_DIAG();

	LOG0(Log::Normal, "繝ｭ繝ｼ繝�");

	// 繧､繝吶Φ繝医�昴う繝ｳ繧ｿ繧剃ｿ晄戟
	first = sch.first;

	// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝医し繧､繧ｺ繧偵Ο繝ｼ繝峨∫�ｧ蜷�
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(breakp)) {
		return FALSE;
	}

	// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝亥ｮ滉ｽ薙ｒ繝ｭ繝ｼ繝�
	if (!fio->Read(breakp, (int)sz)) {
		return FALSE;
	}

	// 繧ｹ繧ｱ繧ｸ繝･繝ｼ繝ｩ繧ｵ繧､繧ｺ繧偵Ο繝ｼ繝峨∫�ｧ蜷�
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(scheduler_t)) {
		return FALSE;
	}

	// 繧ｹ繧ｱ繧ｸ繝･繝ｼ繝ｩ螳滉ｽ薙ｒ繝ｭ繝ｼ繝�
	if (!fio->Read(&sch, (int)sz)) {
		return FALSE;
	}

	// 繧ｵ繧､繧ｯ繝ｫ繝�繝ｼ繝悶Ν繧偵Ο繝ｼ繝�
	if (!fio->Read(CycleTable, sizeof(CycleTable))) {
		return FALSE;
	}

	// 繧､繝吶Φ繝医�昴う繝ｳ繧ｿ繧貞ｾｩ蟶ｰ
	sch.first = first;

	// 繝舌�ｼ繧ｸ繝ｧ繝ｳ2.01莉･荳翫↑繧峨‥ma_active繧偵Ο繝ｼ繝�
	if (ver >= 0x0201) {
		if (!fio->Read(&dma_active, sizeof(dma_active))) {
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	險ｭ螳夐←逕ｨ
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	ASSERT_DIAG();

	LOG0(Log::Normal, "險ｭ螳夐←逕ｨ");

	// 繧ｷ繧ｹ繝�繝繧ｯ繝ｭ繝�繧ｯ險ｭ螳壹ｒ豈碑ｼ�
	if (sch.clock != config->system_clock) {
		// 險ｭ螳壹′逡ｰ縺ｪ縺｣縺ｦ縺�繧九�ｮ縺ｧ縲√し繧､繧ｯ繝ｫ繝�繝ｼ繝悶Ν蜀肴ｧ狗ｯ�
		sch.clock = config->system_clock;
		ASSERT((sch.clock >= 0) && (sch.clock <= 5));
		SetCPUSpeed(ClockTable[sch.clock]);
	}
}

#if defined(_DEBUG)
//---------------------------------------------------------------------------
//
//	險ｺ譁ｭ
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::AssertDiag() const
{
	ASSERT(this);
	ASSERT(GetID() == MAKEID('S', 'C', 'H', 'E'));
	ASSERT(cpu);
	ASSERT(cpu->GetID() == MAKEID('C', 'P', 'U', ' '));
	ASSERT(dmac);
	ASSERT(dmac->GetID() == MAKEID('D', 'M', 'A', 'C'));
}
#endif	// _DEBUG

//---------------------------------------------------------------------------
//
//	蜀�驛ｨ繝�繝ｼ繧ｿ蜿門ｾ�
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::GetScheduler(scheduler_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT_DIAG();

	// 蜀�驛ｨ繝�繝ｼ繧ｿ繧偵さ繝斐�ｼ
	*buffer = sch;
}

//---------------------------------------------------------------------------
//
//	螳溯｡�
//
//---------------------------------------------------------------------------
DWORD FASTCALL Scheduler::Exec(DWORD hus)
{
	int cycle;
	DWORD result;
	DWORD dcycle;

	ASSERT(this);
	ASSERT(hus > 0);
	ASSERT_DIAG();

	// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝育┌縺励�ｮ蝣ｴ蜷�
	if (!sch.check) {
		// 譛遏ｭ縺ｮ繧､繝吶Φ繝医ｒ謗｢縺�
#if defined(SCHEDULER_ASM)
		sch.one = GetMinEvent(hus);
#else
		sch.one = GetMinRemain(hus);
#endif	// SCHEDULER_ASM

		// sch.one + sch.time縺ｫ隕句粋縺�繧ｵ繧､繧ｯ繝ｫ縺縺第里縺ｫ螳溯｡後＠縺ｦ縺�繧九°
		ASSERT((sch.one + sch.time) < 0x1000);
		cycle = CycleTable[sch.one + sch.time];
		if (cycle > sch.cycle) {

			// 莉雁屓螳溯｡後〒縺阪ｋ繧ｵ繧､繧ｯ繝ｫ謨ｰ繧呈爾縺｣縺ｦ縲∝ｮ溯｡�
			cycle -= sch.cycle;
			if (!dma_active) {
				// 騾壼ｸｸ
				result = cpu->Exec(cycle);
			}
			else {
				// DMAC繧ｪ繝ｼ繝医Μ繧ｯ繧ｨ繧ｹ繝域怏蜉ｹ
				dcycle = dmac->AutoDMA(cycle);
				if (dcycle != 0) {
					// 縺｡繧�縺｣縺ｨ隱､蟾ｮ縺悟�ｺ繧具ｼ�
					result = cpu->Exec(dcycle);
				}
				else {
					// 縺吶∋縺ｦDMA縺ｧ豸郁ｲｻ
					result = cycle;
				}
			}

			// 豁｣蟶ｸ邨ゆｺ�縺�
			if (result < 0x80000000) {
				// sch.time, sch.cycle繧呈峩譁ｰ
				sch.cycle += result;
				sch.time += sch.one;

				// 譎る俣繧帝ｲ繧√ｋ
				ExecEvent(sch.one);

				if (sch.time < 200) {
					return sch.one;
				}

				// 譎る俣Sync
				while (sch.time >= 200) {
					if ((DWORD)sch.cycle < sch.speed) {
						break;
					}
					sch.time -= 200;
					sch.cycle -= sch.speed;
				}

				// 繝悶Ξ繝ｼ繧ｯ繝√ぉ繝�繧ｯ
				if (!sch.brk) {
					return sch.one;
				}

#if defined(SCHEDULER_LOG)
				LOG0(Log::Normal, "繝悶Ξ繝ｼ繧ｯ");
#endif	// SCHEDULER_LOG
				sch.brk = FALSE;
				return (DWORD)(sch.one | 0x80000000);
			}
			else {
				// 螳溯｡後お繝ｩ繝ｼ
				result &= 0x7fffffff;

				if ((int)result > cycle) {
					// sch.time縲《ch.cycle繧呈峩譁ｰ
					sch.time += sch.one;
					sch.cycle += result;

					// 繧､繝吶Φ繝亥ｮ溯｡�
					ExecEvent(sch.one);

					while (sch.time >= 200) {
						if ((DWORD)sch.cycle < sch.speed) {
							break;
						}
						sch.time -= 200;
						sch.cycle -= sch.speed;
					}
					// 螳溯｡後お繝ｩ繝ｼ縲√う繝吶Φ繝亥ｮ御ｺ�
					return 0x80000000;
				}
				// 蜈ｨ驛ｨ螳溯｡後☆繧句燕縺ｫcpu繧ｨ繝ｩ繝ｼ縺瑚ｵｷ縺阪◆
				sch.cycle += result;
				// 螳溯｡後お繝ｩ繝ｼ縲√う繝吶Φ繝域悴螳御ｺ�
				return 0x80000000;
			}
		}
		else {

			// 莉雁屓縺ｯ螳溯｡後〒縺阪↑縺�縲よ凾髢薙ｒ騾ｲ繧√ｋ縺ｮ縺ｿ
			sch.time += sch.one;
			ExecEvent(sch.one);

			if (sch.time < 200) {
				return sch.one;
			}

			// sch.time繧呈峩譁ｰ
			while (sch.time >= 200) {
				if ((DWORD)sch.cycle < sch.speed) {
					break;
				}
				sch.time -= 200;
				sch.cycle -= sch.speed;
			}

			// 螳溯｡悟多莉､縺ｪ縺励√う繝吶Φ繝亥ｮ御ｺ�
			return sch.one;
		}

	}

	// 繝ｫ繝ｼ繝�
	for (;;) {
		result = Trace(hus);

		switch (result) {
			// 螳溯｡悟多莉､縺ｪ縺励√う繝吶Φ繝亥ｮ御ｺ�
			case 0:
				return sch.one;

			// 螳溯｡悟庄縲√う繝吶Φ繝亥ｮ御ｺ�
			case 1:
				if (sch.brk) {
#if defined(SCHEDULER_LOG)
					LOG0(Log::Normal, "繝悶Ξ繝ｼ繧ｯ");
#endif	// SCHEDULER_LOG
					sch.brk = FALSE;
					return 0x80000000;
				}
				if (IsBreak(cpu->GetPC()) != -1) {
					OnBreak(cpu->GetPC());
					return 0x80000000;
				}
				return sch.one;

			// 螳溯｡後≠繧翫√う繝吶Φ繝域悴螳御ｺ�
			case 2:
				if (sch.brk) {
#if defined(SCHEDULER_LOG)
					LOG0(Log::Normal, "繝悶Ξ繝ｼ繧ｯ");
#endif	// SCHEDULER_LOG
					sch.brk = FALSE;
					return 0x80000000;
				}
				if (IsBreak(cpu->GetPC()) != -1) {
					OnBreak(cpu->GetPC());
					return 0x80000000;
				}
				break;

			// 螳溯｡後お繝ｩ繝ｼ
			case 3:
				if (sch.brk) {
#if defined(SCHEDULER_LOG)
					LOG0(Log::Normal, "繝悶Ξ繝ｼ繧ｯ");
#endif	// SCHEDULER_LOG
					sch.brk = FALSE;
				}
				return 0x80000000;

			// 縺昴ｌ莉･螟�
			default:
				ASSERT(FALSE);
				return sch.one;
		}
	}
}

//---------------------------------------------------------------------------
//
//	繝医Ξ繝ｼ繧ｹ
//
//---------------------------------------------------------------------------
DWORD FASTCALL Scheduler::Trace(DWORD hus)
{
	int cycle;
	DWORD result;

	ASSERT(this);
	ASSERT(hus > 0);
	ASSERT_DIAG();

	// 譛遏ｭ縺ｮ繧､繝吶Φ繝医ｒ謗｢縺�
#if defined(SCHEDULER_ASM)
	sch.one = GetMinEvent(hus);
#else
	sch.one = GetMinRemain(hus);
#endif	// SCHEDULER_ASM

	// sch.one + sch.time縺ｫ隕句粋縺�繧ｵ繧､繧ｯ繝ｫ縺縺第里縺ｫ螳溯｡後＠縺ｦ縺�繧九°
	ASSERT((sch.one + sch.time) < 0x1000);
	cycle = CycleTable[sch.one + sch.time];
	if (cycle <= sch.cycle) {
		// 莉雁屓縺ｯ螳溯｡後〒縺阪↑縺�縲よ凾髢薙□縺鷹ｲ繧√ｋ
		sch.time += sch.one;
		ExecEvent(sch.one);

		// sch.time繧呈峩譁ｰ
		while (sch.time >= 200) {
			sch.time -= 200;
			sch.cycle -= sch.speed;
		}
		// 螳溯｡悟多莉､縺ｪ縺励√う繝吶Φ繝亥ｮ御ｺ�
		return 0;
	}

	// 莉雁屓螳溯｡後〒縺阪ｋ繧ｵ繧､繧ｯ繝ｫ謨ｰ繧呈爾繧�
	cycle -= sch.cycle;

	// 1繧ｵ繧､繧ｯ繝ｫ縺縺台ｸ弱∴縺ｦ螳溯｡後＠縺ｦ縺ｿ繧�
	if (!dma_active) {
		// 騾壼ｸｸ
		result = cpu->Exec(1);
	}
	else {
		// DMAC繧ｪ繝ｼ繝医Μ繧ｯ繧ｨ繧ｹ繝域怏蜉ｹ
		result = dmac->AutoDMA(1);
		if (result != 0) {
			result = cpu->Exec(result);
		}
		else {
			result = 1;
		}
	}
	if (result >= 0x80000000) {
		// 螳溯｡後お繝ｩ繝ｼ
		return 3;
	}

	// result >= cycle縺ｪ繧峨√う繝吶Φ繝亥ｮ溯｡後〒縺阪ｋ
	if ((int)result >= cycle) {
		// sch.time, sch.cycle繧呈峩譁ｰ
		sch.cycle += result;
		sch.time += sch.one;

		// 譎る俣繧帝ｲ繧√ｋ
		ExecEvent(sch.one);

		while (sch.time >= 200) {
			sch.time -= 200;
			sch.cycle -= sch.speed;
		}
		// 螳溯｡悟庄縲√う繝吶Φ繝亥ｮ御ｺ�
		return 1;
	}

	// 縺ｾ縺雜ｳ繧翫※縺�縺ｪ縺�縺ｮ縺ｧ縲√う繝吶Φ繝医∪縺ｧ縺ｯ髢薙′縺ゅｋ
	// sch.cycle繧呈峩譁ｰ
	sch.cycle += result;

	// 螳溯｡後≠繧翫√う繝吶Φ繝域悴螳御ｺ�
	return 2;
}

//---------------------------------------------------------------------------
//
//	CPU騾溷ｺｦ繧定ｨｭ螳�
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::SetCPUSpeed(DWORD speed)
{
	int i;
	DWORD cycle;

	ASSERT(this);
	ASSERT(speed > 0);
	ASSERT_DIAG();

	LOG2(Log::Detail, "CPU騾溷ｺｦ險ｭ螳� %d.%02dMHz", speed / 100, (speed % 100));

	// CPU騾溷ｺｦ繧定ｨ俶�ｶ
	sch.speed = speed;

	// 0縲�2048us縺ｾ縺ｧ縲�0.5us蜊倅ｽ阪〒縺ｮ蟇ｾ蠢懊☆繧九し繧､繧ｯ繝ｫ謨ｰ繧定ｨ育ｮ�
	for (i=0; i<0x1000; i++) {
		cycle = (DWORD)i;
		cycle *= speed;
		cycle /= 200;
		CycleTable[i] = cycle;
	}
}

//---------------------------------------------------------------------------
//
//	邨碁℃譎る俣繧貞叙蠕�
//
//---------------------------------------------------------------------------
DWORD FASTCALL Scheduler::GetPassedTime() const
{
	DWORD hus;

	ASSERT(this);
	ASSERT_DIAG();

	// 繧､繝吶Φ繝亥ｮ溯｡御ｸｭ縺ｪ繧�0
	if (sch.exec) {
		return 0;
	}

	// 螳溯｡後し繧､繧ｯ繝ｫ謨ｰ縲…pu_cylcle縺九ｉ譎る俣繧堤ｮ怜�ｺ
	hus = cpu->GetCycle() + sch.cycle;
	hus *= 200;
	hus /= sch.speed;
	hus -= sch.time;

	// one繧医ｊ繧ょ､ｧ縺阪￠繧後�ｰ縲∝宛髯�
	if (sch.one < hus) {
		hus = sch.one;
	}

	// hus蜊倅ｽ阪〒霑斐☆
	return hus;
}

//---------------------------------------------------------------------------
//
//	繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝郁ｨｭ螳�
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::SetBreak(DWORD addr, BOOL enable)
{
	int i;
	BOOL flag;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT_DIAG();

#if defined(SCHEDULER_LOG)
	LOG2(Log::Normal, "繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝郁ｨｭ螳� $%06X enable=%d", addr, enable);
#endif	// SCHEDULER_LOG

	flag = FALSE;

	// 荳閾ｴ繝√ぉ繝�繧ｯ
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].addr == addr) {
				// 繝輔Λ繧ｰ螟画峩縺ｮ縺ｿ
				breakp[i].enable = enable;
				flag = TRUE;
				break;
			}
		}
	}

	if (!flag) {
		// 遨ｺ縺阪し繝ｼ繝�
		for (i=0; i<BreakMax; i++) {
			if (!breakp[i].use) {
				// 繧ｻ繝�繝�
				breakp[i].use = TRUE;
				breakp[i].addr = addr;
				breakp[i].enable = enable;
				breakp[i].time = 0;
				breakp[i].count = 0;
				break;
			}
		}
	}

	// 譛牙柑繝輔Λ繧ｰ繧定ｨｭ螳�
	flag = FALSE;
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].enable) {
				// 譛牙柑縺ｪ繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝医′蟄伜惠
				flag = TRUE;
				break;
			}
		}
	}
	sch.check = flag;
}

//---------------------------------------------------------------------------
//
//	繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝亥炎髯､
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::DelBreak(DWORD addr)
{
	int i;
	BOOL flag;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT_DIAG();

#if defined(SCHEDULER_LOG)
	LOG1(Log::Normal, "繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝亥炎髯､ $%06X", addr);
#endif	// SCHEDULER_LOG

	// 荳閾ｴ繝√ぉ繝�繧ｯ
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].addr == addr) {
				// 蜑企勁
				breakp[i].use = FALSE;
				break;
			}
		}
	}

	// 譛牙柑繝輔Λ繧ｰ繧定ｨｭ螳�
	flag = FALSE;
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].enable) {
				// 譛牙柑縺ｪ繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝医′蟄伜惠
				flag = TRUE;
				break;
			}
		}
	}
	sch.check = flag;
}

//---------------------------------------------------------------------------
//
//	繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝亥叙蠕�
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::GetBreak(int index, breakpoint_t *buf) const
{
	ASSERT(this);
	ASSERT((index >= 0) && (index < BreakMax));
	ASSERT(buf);
	ASSERT_DIAG();

	// 繧ｳ繝斐�ｼ
	*buf = breakp[index];
}

//---------------------------------------------------------------------------
//
//	繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝域怏蜉ｹ繝ｻ辟｡蜉ｹ
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::EnableBreak(int index, BOOL enable)
{
	ASSERT(this);
	ASSERT((index >= 0) && (index < BreakMax));
	ASSERT(breakp[index].use);
	ASSERT_DIAG();

	breakp[index].enable = enable;
}

//---------------------------------------------------------------------------
//
//	繝悶Ξ繝ｼ繧ｯ蝗樊焚繧ｯ繝ｪ繧｢
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::ClearBreak(int index)
{
	ASSERT(this);
	ASSERT((index >= 0) && (index < BreakMax));
	ASSERT(breakp[index].use);
	ASSERT_DIAG();

	breakp[index].count = 0;
	breakp[index].time = 0;
}

//---------------------------------------------------------------------------
//
//	繝悶Ξ繝ｼ繧ｯ繧｢繝峨Ξ繧ｹ螟画峩
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::AddrBreak(int index, DWORD addr)
{
	ASSERT(this);
	ASSERT((index >= 0) && (index < BreakMax));
	ASSERT(addr <= 0xffffff);
	ASSERT(breakp[index].use);
	ASSERT_DIAG();

	breakp[index].addr = addr;
}

//---------------------------------------------------------------------------
//
//	繝悶Ξ繝ｼ繧ｯ繧｢繝峨Ξ繧ｹ繝√ぉ繝�繧ｯ
//
//---------------------------------------------------------------------------
int FASTCALL Scheduler::IsBreak(DWORD addr, BOOL any) const
{
	int i;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT_DIAG();

	// 譛蛻昴↓繝輔Λ繧ｰ繧定ｦ九ｋ
	if (!sch.check) {
		return -1;
	}

	// 荳閾ｴ繝√ぉ繝�繧ｯ
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].addr == addr) {
				// 譛牙柑繝ｻ辟｡蜉ｹ繧呈ｰ励↓縺励↑縺�縺九∵怏蜉ｹ
				if (any || breakp[i].enable) {
					return i;
				}
			}
		}
	}

	// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝医�ｯ縺ゅｋ縺後∽ｸ閾ｴ辟｡縺�
	return -1;
}

//---------------------------------------------------------------------------
//
//	繝悶Ξ繝ｼ繧ｯ繧｢繝峨Ξ繧ｹ驕ｩ逕ｨ
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::OnBreak(DWORD addr)
{
	int i;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT(sch.check);
	ASSERT_DIAG();

	// 荳閾ｴ繝√ぉ繝�繧ｯ
	for (i=0; i<BreakMax; i++) {
		if (breakp[i].use) {
			if (breakp[i].addr == addr) {
				break;
			}
		}
	}
	ASSERT(i < BreakMax);

	// 譎る俣繧ｻ繝�繝医√き繧ｦ繝ｳ繝医い繝�繝�
	breakp[i].time = GetTotalTime();
	breakp[i].count++;
}

//---------------------------------------------------------------------------
//
//	繧､繝吶Φ繝郁ｿｽ蜉
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::AddEvent(Event *event)
{
	Event *p;

	ASSERT(this);
	ASSERT(event);
	ASSERT_DIAG();

#if defined(SCHEDULER_LOG)
	LOG4(Log::Normal, "繧､繝吶Φ繝郁ｿｽ蜉 Device=%c%c%c%c",
					(char)(event->GetDevice()->GetID() >> 24),
					(char)(event->GetDevice()->GetID() >> 16),
					(char)(event->GetDevice()->GetID() >> 8),
					(char)(event->GetDevice()->GetID()));
	LOG1(Log::Normal, "繧､繝吶Φ繝郁ｿｽ蜉 %s", event->GetDesc());
#endif	// SCHEDULER_LOG

	// 譛蛻昴�ｮ繧､繝吶Φ繝医°
	if (!sch.first) {
		// 譛蛻昴�ｮ繧､繝吶Φ繝�
		sch.first = event;
		event->SetNextEvent(NULL);

#if defined(SCHEDULER_ASM)
		// 騾夂衍
		NotifyEvent(sch.first);
#endif	// SCHEDULER_ASM
		return;
	}

	// 譛蠕後�ｮ繧､繝吶Φ繝医ｒ謗｢縺�
	p = sch.first;
	while (p->GetNextEvent()) {
		p = p->GetNextEvent();
	}

	// p縺梧怙蠕後�ｮ繧､繝吶Φ繝医↑縺ｮ縺ｧ縲√％繧後↓霑ｽ蜉
	p->SetNextEvent(event);
	event->SetNextEvent(NULL);

#if defined(SCHEDULER_ASM)
	// 騾夂衍
	NotifyEvent(sch.first);
#endif	// SCHEDULER_ASM
}

//---------------------------------------------------------------------------
//
//	繧､繝吶Φ繝亥炎髯､
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::DelEvent(Event *event)
{
	Event *p;
	Event *prev;

	ASSERT(this);
	ASSERT(event);
	ASSERT_DIAG();

#if defined(SCHEDULER_LOG)
	LOG4(Log::Normal, "繧､繝吶Φ繝亥炎髯､ Device=%c%c%c%c",
					(char)(event->GetDevice()->GetID() >> 24),
					(char)(event->GetDevice()->GetID() >> 16),
					(char)(event->GetDevice()->GetID() >> 8),
					(char)(event->GetDevice()->GetID()));
	LOG1(Log::Normal, "繧､繝吶Φ繝亥炎髯､ %s", event->GetDesc());
#endif	// SCHEDULER_LOG

	// 譛蛻昴�ｮ繧､繝吶Φ繝医°
	if (sch.first == event) {
		// 譛蛻昴�ｮ繧､繝吶Φ繝医Ｏext繧呈怙蛻昴�ｮ繧､繝吶Φ繝医↓蜑ｲ繧雁ｽ薙※繧�
		sch.first = event->GetNextEvent();
		event->SetNextEvent(NULL);

#if defined(SCHEDULER_ASM)
		// 騾夂衍
		NotifyEvent(sch.first);
#endif	// SCHEDULER_ASM
		return;
	}

	// 縺薙�ｮ繧､繝吶Φ繝医′荳閾ｴ縺吶ｋ縺ｾ縺ｧ讀懃ｴ｢
	p = sch.first;
	prev = p;
	while (p) {
		// 荳閾ｴ繝√ぉ繝�繧ｯ
		if (p == event) {
			prev->SetNextEvent(event->GetNextEvent());
			event->SetNextEvent(NULL);

#if defined(SCHEDULER_ASM)
			// 騾夂衍
			NotifyEvent(sch.first);
#endif	// SCHEDULER_ASM
			return;
		}

		// 谺｡縺ｸ
		prev = p;
		p = p->GetNextEvent();
	}

	// 縺吶∋縺ｦ縺ｮ繧､繝吶Φ繝医′荳閾ｴ縺励↑縺�(縺ゅｊ蠕励↑縺�)
	ASSERT(FALSE);
}

//---------------------------------------------------------------------------
//
//	繧､繝吶Φ繝域園譛峨メ繧ｧ繝�繧ｯ
//
//---------------------------------------------------------------------------
BOOL FASTCALL Scheduler::HasEvent(Event *event) const
{
	Event *p;

	ASSERT(this);
	ASSERT(event);
	ASSERT_DIAG();

	// 蛻晄悄蛹�
	p = sch.first;

	// 蜈ｨ縺ｦ縺ｮ繧､繝吶Φ繝医ｒ縺ｾ繧上ｋ
	while (p) {
		// 荳閾ｴ繝√ぉ繝�繧ｯ
		if (p == event) {
			return TRUE;
		}

		// 谺｡縺ｸ
		p = p->GetNextEvent();
	}

	// 縺薙�ｮ繧､繝吶Φ繝医�ｯ繝√ぉ繧､繝ｳ縺ｫ蜷ｫ縺ｾ繧後※縺�縺ｪ縺�
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	繧､繝吶Φ繝医�ｮ蛟区焚繧貞叙蠕�
//
//---------------------------------------------------------------------------
int FASTCALL Scheduler::GetEventNum() const
{
	int num;
	Event *p;

	ASSERT(this);
	ASSERT_DIAG();

	// 蛻晄悄蛹�
	num = 0;
	p = sch.first;

	// 蜈ｨ縺ｦ縺ｮ繧､繝吶Φ繝医ｒ縺ｾ繧上ｋ
	while (p) {
		num++;

		// 谺｡縺ｸ
		p = p->GetNextEvent();
	}

	// 繧､繝吶Φ繝医�ｮ蛟区焚繧定ｿ斐☆
	return num;
}

//---------------------------------------------------------------------------
//
//	譛遏ｭ縺ｮ繧､繝吶Φ繝医ｒ謗｢縺�
//	窶ｻ蛻･騾斐い繧ｻ繝ｳ繝悶Λ迚医ｒ逕ｨ諢�
//
//---------------------------------------------------------------------------
DWORD FASTCALL Scheduler::GetMinRemain(DWORD hus)
{
	Event *p;
	DWORD minimum;
	DWORD remain;

	ASSERT(this);
	ASSERT(hus > 0);
	ASSERT_DIAG();

	// 繧､繝吶Φ繝医�昴う繝ｳ繧ｿ蛻晄悄蛹�
	p = sch.first;

	// 蛻晄悄蛹�
	minimum = hus;

	// 繝ｫ繝ｼ繝�
	while (p) {
		// 谿九ｊ譎る俣蜿門ｾ�
		remain = p->GetRemain();

		// 譛牙柑縺�
		if (remain == 0) {
			// 谺｡縺ｸ
			p = p->GetNextEvent();
			continue;
		}

		// 譛蟆上メ繧ｧ繝�繧ｯ
		if (remain >= minimum) {
			p = p->GetNextEvent();
			continue;
		}

		// 譛蟆�
		minimum = remain;
		p = p->GetNextEvent();
	}

	return minimum;
}

//---------------------------------------------------------------------------
//
//	繧､繝吶Φ繝亥ｮ溯｡�
//	窶ｻ蛻･騾斐い繧ｻ繝ｳ繝悶Λ迚医ｒ逕ｨ諢�
//
//---------------------------------------------------------------------------
void FASTCALL Scheduler::ExecEvent(DWORD hus)
{
#if !defined(SCHEDULER_ASM)
	Event *p;
#endif	// !SCHEDULER_ASM

	ASSERT(this);
	ASSERT(hus >= 0);
	ASSERT_DIAG();

	// 繧､繝吶Φ繝亥ｮ溯｡碁幕蟋�
	sch.exec = TRUE;

	// 繝医�ｼ繧ｿ繝ｫ譎る俣蠅怜刈縲√し繧ｦ繝ｳ繝画凾髢灘｢怜刈
	sch.total += hus;
	sch.sound += hus;

#if defined(SCHEDULER_ASM)
	SubExecEvent(hus);
	sch.exec = FALSE;
#else

	// 繧､繝吶Φ繝医�昴う繝ｳ繧ｿ蛻晄悄蛹�
	p = sch.first;

	// 繧､繝吶Φ繝医ｒ蝗槭▲縺ｦ縲∝ｮ溯｡�
	while (p) {
		p->Exec(hus);
		p = p->GetNextEvent();
	}

	// 繧､繝吶Φ繝亥ｮ溯｡檎ｵゆｺ�
	sch.exec = FALSE;
#endif
}

//---------------------------------------------------------------------------
//
//	繧ｯ繝ｭ繝�繧ｯ繝�繝ｼ繝悶Ν
//
//---------------------------------------------------------------------------
const DWORD Scheduler::ClockTable[] = {
	979,			// 10MHz
	1171,			// 12MHz
	1460,			// 15MHz
	1556,			// 16MHz
	1689,			// 17.4MHz
	2200			// 22MHz
};

//---------------------------------------------------------------------------
//
//	繧ｵ繧､繧ｯ繝ｫ繝�繝ｼ繝悶Ν
//
//---------------------------------------------------------------------------
int Scheduler::CycleTable[0x1000];
