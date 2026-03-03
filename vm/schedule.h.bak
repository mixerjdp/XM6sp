//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �ｼｰ�ｼｩ�ｼ�(ytanaka@ipc-tokai.or.jp)
//	[ 繧ｹ繧ｱ繧ｸ繝･繝ｼ繝ｩ ]
//
//---------------------------------------------------------------------------

#if !defined(scheduler_h)
#define scheduler_h

#include "device.h"
#include "starcpu.h"

//---------------------------------------------------------------------------
//
//	鬮倬溘え繧ｧ繧､繝�(Starscream蟆ら畑)
//
//---------------------------------------------------------------------------
#define SCHEDULER_FASTWAIT
#if defined(SCHEDULER_FASTWAIT)
extern "C" {
extern DWORD s68000iocycle;
										// __io_cycle_counter(Starscream)
}
#endif	// SCHEDULER_FASTWAIT

//===========================================================================
//
//	繧ｹ繧ｱ繧ｸ繝･繝ｼ繝ｩ
//
//===========================================================================
class Scheduler : public Device
{
public:
	// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝亥ｮ夂ｾｩ
	typedef struct{
		BOOL use;						// 菴ｿ逕ｨ繝輔Λ繧ｰ
		DWORD addr;						// 繧｢繝峨Ξ繧ｹ
		BOOL enable;					// 譛牙柑繝輔Λ繧ｰ
		DWORD time;						// 蛛懈ｭ｢譎ゅ�ｮ譎る俣
		DWORD count;					// 蛛懈ｭ｢蝗樊焚
	} breakpoint_t;

	// 繧ｹ繧ｱ繧ｸ繝･繝ｼ繝ｩ螳夂ｾｩ
	typedef struct {
		// 譎る俣
		DWORD total;					// 繝医�ｼ繧ｿ繝ｫ螳溯｡梧凾髢�
		DWORD one;						// 1蝗槭�ｮ螳溯｡梧凾髢�
		DWORD sound;					// 繧ｵ繧ｦ繝ｳ繝画峩譁ｰ譎る俣

		// CPU
		int clock;						// CPU繧ｯ繝ｭ繝�繧ｯ(0�ｽ�5)
		DWORD speed;					// CPU騾溷ｺｦ(clock縺ｫ繧医ｊ豎ｺ縺ｾ繧�)
		int cycle;						// CPU繧ｵ繧､繧ｯ繝ｫ謨ｰ
		DWORD time;						// CPU繧ｵ繧､繧ｯ繝ｫ隱ｿ謨ｴ逕ｨ譎る俣

		// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝�
		BOOL brk;						// 繝悶Ξ繝ｼ繧ｯ縺励◆
		BOOL check;						// 譛牙柑縺ｪ繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝医≠繧�

		// 繧､繝吶Φ繝�
		Event *first;					// 譛蛻昴�ｮ繧､繝吶Φ繝�
		BOOL exec;						// 繧､繝吶Φ繝亥ｮ溯｡御ｸｭ
	} scheduler_t;

	// 蛟区焚螳夂ｾｩ
	enum {
		BreakMax = 8					// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝育ｷ乗焚
	};

public:
	// 蝓ｺ譛ｬ繝輔ぃ繝ｳ繧ｯ繧ｷ繝ｧ繝ｳ
	Scheduler(VM *p);
										// 繧ｳ繝ｳ繧ｹ繝医Λ繧ｯ繧ｿ
	BOOL FASTCALL Init();
										// 蛻晄悄蛹�
	void FASTCALL Cleanup();
										// 繧ｯ繝ｪ繝ｼ繝ｳ繧｢繝�繝�
	void FASTCALL Reset();
										// 繝ｪ繧ｻ繝�繝�
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// 繧ｻ繝ｼ繝�
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// 繝ｭ繝ｼ繝�
	void FASTCALL ApplyCfg(const Config *config);
										// 險ｭ螳夐←逕ｨ
#if defined(_DEBUG)
	void FASTCALL AssertDiag() const;
										// 險ｺ譁ｭ
#endif	// _DEBUG

	// 螟夜ΚAPI
	void FASTCALL GetScheduler(scheduler_t *buffer) const;
										// 蜀�驛ｨ繝�繝ｼ繧ｿ蜿門ｾ�
	DWORD FASTCALL Exec(DWORD hus);
										// 螳溯｡�
	DWORD FASTCALL Trace(DWORD hus);
										// 繝医Ξ繝ｼ繧ｹ
	void FASTCALL Break()				{ sch.brk = TRUE; }
										// 螳溯｡御ｸｭ豁｢
#ifdef SCHEDULER_FASTWAIT
	void FASTCALL Wait(DWORD cycle)		{ sch.cycle += cycle; if (::s68000iocycle != (DWORD)-1) ::s68000iocycle -= cycle; }
										// CPU繧ｦ繧ｧ繧､繝�(縺吶∋縺ｦ繧､繝ｳ繝ｩ繧､繝ｳ)
#else
	void FASTCALL Wait(DWORD cycle)		{ ::s68000wait(cycle); sch.cycle += cycle; }
										// CPU繧ｦ繧ｧ繧､繝�
#endif	// SCHEDULER_FASTWAIT

	// 迚ｹ谿頑桃菴�(DMAC蜷代￠)
	int FASTCALL GetCPUCycle() const	{ return sch.cycle; }
										// 繧ｦ繧ｧ繧､繝域焚蜿門ｾ�
	void FASTCALL SetCPUCycle(int cycle) { sch.cycle = cycle; }
										// 繧ｦ繧ｧ繧､繝域焚險ｭ螳�

	// 譎る俣諠�蝣ｱ
	DWORD FASTCALL GetTotalTime() const	{ return (GetPassedTime() + sch.total); }
										// 繝医�ｼ繧ｿ繝ｫ螳溯｡梧凾髢薙ｒ蜿門ｾ�
	DWORD FASTCALL GetOneTime() const	{ return sch.one; }
										// 蠕ｮ蟆大ｮ溯｡梧凾髢薙ｒ蜿門ｾ�
	DWORD FASTCALL GetPassedTime() const;
										// 邨碁℃譎る俣繧貞叙蠕�
	DWORD FASTCALL GetCPUSpeed() const	{ return sch.speed; }
										// CPU騾溷ｺｦ蜿門ｾ�
	void FASTCALL SetCPUSpeed(DWORD speed);
										// CPU騾溷ｺｦ險ｭ螳�
	DWORD FASTCALL GetSoundTime() const	{ return sch.sound; }
										// 繧ｵ繧ｦ繝ｳ繝画凾髢薙ｒ蜿門ｾ�
	void FASTCALL SetSoundTime(DWORD hus) { sch.sound = hus; }
										// 繧ｵ繧ｦ繝ｳ繝画凾髢薙ｒ險ｭ螳�

	// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝�
	void FASTCALL SetBreak(DWORD addr, BOOL enable = TRUE);
										// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝郁ｨｭ螳�
	void FASTCALL DelBreak(DWORD addr);
										// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝亥炎髯､
	void FASTCALL GetBreak(int index, breakpoint_t *buf) const;
										// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝亥叙蠕�
	void FASTCALL EnableBreak(int index, BOOL enable = TRUE);
										// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝域怏蜉ｹ繝ｻ辟｡蜉ｹ
	void FASTCALL ClearBreak(int index);
										// 繝悶Ξ繝ｼ繧ｯ蝗樊焚繧ｯ繝ｪ繧｢
	void FASTCALL AddrBreak(int index, DWORD addr);
										// 繝悶Ξ繝ｼ繧ｯ繧｢繝峨Ξ繧ｹ螟画峩
	int FASTCALL IsBreak(DWORD addr, BOOL any = FALSE) const;
										// 繝悶Ξ繝ｼ繧ｯ繧｢繝峨Ξ繧ｹ繝√ぉ繝�繧ｯ

	// 繧､繝吶Φ繝�
	void FASTCALL AddEvent(Event *event);
										// 繧､繝吶Φ繝郁ｿｽ蜉
	void FASTCALL DelEvent(Event *event);
										// 繧､繝吶Φ繝亥炎髯､
	BOOL FASTCALL HasEvent(Event *event) const;
										// 繧､繝吶Φ繝域園譛峨メ繧ｧ繝�繧ｯ
	Event* FASTCALL GetFirstEvent()	const { return sch.first; }
										// 譛蛻昴�ｮ繧､繝吶Φ繝医ｒ蜿門ｾ�
	int FASTCALL GetEventNum() const;
										// 繧､繝吶Φ繝医�ｮ蛟区焚繧貞叙蠕�

	// 螟夜Κ謫堺ｽ懊ヵ繝ｩ繧ｰ
	BOOL dma_active;
										// DMAC繧ｪ繝ｼ繝医Μ繧ｯ繧ｨ繧ｹ繝域怏蜉ｹ

private:
	DWORD FASTCALL GetMinRemain(DWORD hus);
										// 譛遏ｭ縺ｮ繧､繝吶Φ繝医ｒ蠕励ｋ
	void FASTCALL ExecEvent(DWORD hus);
										// 繧､繝吶Φ繝亥ｮ溯｡�
	void FASTCALL OnBreak(DWORD addr);
										// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝磯←逕ｨ

	// 蜀�驛ｨ繝�繝ｼ繧ｿ
	breakpoint_t breakp[BreakMax];
										// 繝悶Ξ繝ｼ繧ｯ繝昴う繝ｳ繝�
	scheduler_t sch;
										// 繧ｹ繧ｱ繧ｸ繝･繝ｼ繝ｩ

	// 繝�繝舌う繧ｹ
	CPU *cpu;
										// CPU
	DMAC *dmac;
										// DMAC

	// 繝�繝ｼ繝悶Ν
	static const DWORD ClockTable[];
										// 繧ｯ繝ｭ繝�繧ｯ繝�繝ｼ繝悶Ν
	static int CycleTable[0x1000];
										// 譎る俣(hus)竊偵し繧､繧ｯ繝ｫ謨ｰ
};

#endif	// scheduler_h
