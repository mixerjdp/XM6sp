//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ・ｽ・ｼ・ｰ・ｽ・ｼ・ｩ・ｽ・ｼ・ｽ(ytanaka@ipc-tokai.or.jp)
//	[ 郢ｧ・ｹ郢ｧ・ｱ郢ｧ・ｸ郢晢ｽ･郢晢ｽｼ郢晢ｽｩ ]
//
//---------------------------------------------------------------------------

#if !defined(scheduler_h)
#define scheduler_h

#include "device.h"
#include "musashi_adapter.h"

#define SCHEDULER_FASTWAIT

//===========================================================================
//
//	郢ｧ・ｹ郢ｧ・ｱ郢ｧ・ｸ郢晢ｽ･郢晢ｽｼ郢晢ｽｩ
//
//===========================================================================
class Scheduler : public Device
{
public:
	// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢昜ｺ･・ｮ螟ゑｽｾ・ｩ
	typedef struct{
		BOOL use;						// 闖ｴ・ｿ騾包ｽｨ郢晁ｼ釆帷ｹｧ・ｰ
		DWORD addr;						// 郢ｧ・｢郢晏ｳｨﾎ樒ｹｧ・ｹ
		BOOL enable;					// 隴帷甥譟醍ｹ晁ｼ釆帷ｹｧ・ｰ
		DWORD time;						// 陋帶㊧・ｭ・｢隴弱ｅ・ｽ・ｮ隴弱ｋ菫｣
		DWORD count;					// 陋帶㊧・ｭ・｢陜玲ｨ顔・
	} breakpoint_t;

	// 郢ｧ・ｹ郢ｧ・ｱ郢ｧ・ｸ郢晢ｽ･郢晢ｽｼ郢晢ｽｩ陞ｳ螟ゑｽｾ・ｩ
	typedef struct {
		// 隴弱ｋ菫｣
		DWORD total;					// 郢晏現・ｽ・ｼ郢ｧ・ｿ郢晢ｽｫ陞ｳ貅ｯ・｡譴ｧ蜃ｾ鬮｢・ｽ
		DWORD one;						// 1陜玲ｧｭ・ｽ・ｮ陞ｳ貅ｯ・｡譴ｧ蜃ｾ鬮｢・ｽ
		DWORD sound;					// 郢ｧ・ｵ郢ｧ・ｦ郢晢ｽｳ郢晉判蟲ｩ隴・ｽｰ隴弱ｋ菫｣

		// CPU
		int clock;						// CPU郢ｧ・ｯ郢晢ｽｭ郢晢ｿｽ郢ｧ・ｯ(0・ｽ・ｽ・ｽ5)
		DWORD speed;					// CPU鬨ｾ貅ｷ・ｺ・ｦ(clock邵ｺ・ｫ郢ｧ蛹ｻ・願ｱ趣ｽｺ邵ｺ・ｾ郢ｧ・ｽ)
		int cycle;						// CPU郢ｧ・ｵ郢ｧ・､郢ｧ・ｯ郢晢ｽｫ隰ｨ・ｰ
		DWORD time;						// CPU郢ｧ・ｵ郢ｧ・､郢ｧ・ｯ郢晢ｽｫ髫ｱ・ｿ隰ｨ・ｴ騾包ｽｨ隴弱ｋ菫｣

		// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢晢ｿｽ
		BOOL brk;						// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ邵ｺ蜉ｱ笳・
		BOOL check;						// 隴帷甥譟醍ｸｺ・ｪ郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢晏現竕郢ｧ・ｽ

		// 郢ｧ・､郢晏生ﾎｦ郢晢ｿｽ
		Event *first;					// 隴崢陋ｻ譏ｴ・ｽ・ｮ郢ｧ・､郢晏生ﾎｦ郢晢ｿｽ
		BOOL exec;						// 郢ｧ・､郢晏生ﾎｦ郢昜ｺ･・ｮ貅ｯ・｡蠕｡・ｸ・ｭ
	} scheduler_t;

	// 陋溷玄辟夊楜螟ゑｽｾ・ｩ
	enum {
		BreakMax = 8					// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢晁ご・ｷ荵礼・
	};

public:
	// 陜難ｽｺ隴幢ｽｬ郢晁ｼ斐＜郢晢ｽｳ郢ｧ・ｯ郢ｧ・ｷ郢晢ｽｧ郢晢ｽｳ
	Scheduler(VM *p);
										// 郢ｧ・ｳ郢晢ｽｳ郢ｧ・ｹ郢晏現ﾎ帷ｹｧ・ｯ郢ｧ・ｿ
	BOOL FASTCALL Init();
										// 陋ｻ譎・ｄ陋ｹ・ｽ
	void FASTCALL Cleanup();
										// 郢ｧ・ｯ郢晢ｽｪ郢晢ｽｼ郢晢ｽｳ郢ｧ・｢郢晢ｿｽ郢晢ｿｽ
	void FASTCALL Reset();
										// 郢晢ｽｪ郢ｧ・ｻ郢晢ｿｽ郢晢ｿｽ
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// 郢ｧ・ｻ郢晢ｽｼ郢晢ｿｽ
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// 郢晢ｽｭ郢晢ｽｼ郢晢ｿｽ
	void FASTCALL ApplyCfg(const Config *config);
										// 髫ｪ・ｭ陞ｳ螟絶・騾包ｽｨ
#if defined(_DEBUG)
	void FASTCALL AssertDiag() const;
										// 髫ｪ・ｺ隴・ｽｭ
#endif	// _DEBUG

	// 陞溷､慚哂PI
	void FASTCALL GetScheduler(scheduler_t *buffer) const;
										// 陷・ｽ鬩幢ｽｨ郢晢ｿｽ郢晢ｽｼ郢ｧ・ｿ陷ｿ髢・ｾ・ｽ
	DWORD FASTCALL Exec(DWORD hus);
										// 陞ｳ貅ｯ・｡・ｽ
	DWORD FASTCALL Trace(DWORD hus);
										// 郢晏現ﾎ樒ｹ晢ｽｼ郢ｧ・ｹ
	void FASTCALL ExecEventsOnly(DWORD hus) { ExecEvent(hus); }
										// 郢ｧ・､郢晏生ﾎｦ郢昜ｺ･・ｮ貅ｯ・｡・ｽ(CPUなし)
	void FASTCALL Break()				{ sch.brk = TRUE; }
										// 陞ｳ貅ｯ・｡蠕｡・ｸ・ｭ雎・ｽ｢
#ifdef SCHEDULER_FASTWAIT
	void FASTCALL Wait(DWORD cycle)		{ sch.cycle += cycle; if (::s68000iocycle != (DWORD)-1) { ::s68000iocycle -= cycle; ::musashi_adjust_timeslice(-(int)cycle); } }
#else
	void FASTCALL Wait(DWORD cycle)		{ ::s68000wait(cycle); sch.cycle += cycle; }
#endif

	// 霑夲ｽｹ隹ｿ鬆第｡・抄・ｽ(DMAC陷ｷ莉｣・)
	int FASTCALL GetCPUCycle() const	{ return sch.cycle; }
										// 郢ｧ・ｦ郢ｧ・ｧ郢ｧ・､郢晏沺辟夊愾髢・ｾ・ｽ
	void FASTCALL SetCPUCycle(int cycle) { sch.cycle = cycle; }
										// 郢ｧ・ｦ郢ｧ・ｧ郢ｧ・､郢晏沺辟夐坎・ｭ陞ｳ・ｽ

	// 隴弱ｋ菫｣隲・ｽ陜｣・ｱ
	DWORD FASTCALL GetTotalTime() const	{ return (GetPassedTime() + sch.total); }
										// 郢晏現・ｽ・ｼ郢ｧ・ｿ郢晢ｽｫ陞ｳ貅ｯ・｡譴ｧ蜃ｾ鬮｢阮呻ｽ定愾髢・ｾ・ｽ
	DWORD FASTCALL GetOneTime() const	{ return sch.one; }
										// 陟包ｽｮ陝・､ｧ・ｮ貅ｯ・｡譴ｧ蜃ｾ鬮｢阮呻ｽ定愾髢・ｾ・ｽ
	DWORD FASTCALL GetPassedTime() const;
										// 驍ｨ遒≫с隴弱ｋ菫｣郢ｧ雋槫徐陟包ｿｽ
	DWORD FASTCALL GetCPUSpeed() const	{ return sch.speed; }
										// CPU鬨ｾ貅ｷ・ｺ・ｦ陷ｿ髢・ｾ・ｽ
	void FASTCALL SetCPUSpeed(DWORD speed);
										// CPU鬨ｾ貅ｷ・ｺ・ｦ髫ｪ・ｭ陞ｳ・ｽ
	DWORD FASTCALL GetSoundTime() const	{ return sch.sound; }
										// 郢ｧ・ｵ郢ｧ・ｦ郢晢ｽｳ郢晉判蜃ｾ鬮｢阮呻ｽ定愾髢・ｾ・ｽ
	void FASTCALL SetSoundTime(DWORD hus) { sch.sound = hus; }
										// 郢ｧ・ｵ郢ｧ・ｦ郢晢ｽｳ郢晉判蜃ｾ鬮｢阮呻ｽ帝坎・ｭ陞ｳ・ｽ

	// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢晢ｿｽ
	void FASTCALL SetBreak(DWORD addr, BOOL enable = TRUE);
										// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢晞メ・ｨ・ｭ陞ｳ・ｽ
	void FASTCALL DelBreak(DWORD addr);
										// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢昜ｺ･轤朱ｫｯ・､
	void FASTCALL GetBreak(int index, breakpoint_t *buf) const;
										// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢昜ｺ･蜿呵包ｿｽ
	void FASTCALL EnableBreak(int index, BOOL enable = TRUE);
										// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢晏沺諤剰怏・ｹ郢晢ｽｻ霎滂ｽ｡陷会ｽｹ
	void FASTCALL ClearBreak(int index);
										// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ陜玲ｨ顔・郢ｧ・ｯ郢晢ｽｪ郢ｧ・｢
	void FASTCALL AddrBreak(int index, DWORD addr);
										// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢ｧ・｢郢晏ｳｨﾎ樒ｹｧ・ｹ陞溽判蟲ｩ
	int FASTCALL IsBreak(DWORD addr, BOOL any = FALSE) const;
										// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢ｧ・｢郢晏ｳｨﾎ樒ｹｧ・ｹ郢昶・縺臥ｹ晢ｿｽ郢ｧ・ｯ

	// 郢ｧ・､郢晏生ﾎｦ郢晢ｿｽ
	void FASTCALL AddEvent(Event *event);
										// 郢ｧ・､郢晏生ﾎｦ郢晞メ・ｿ・ｽ陷会｣ｰ
	void FASTCALL DelEvent(Event *event);
										// 郢ｧ・､郢晏生ﾎｦ郢昜ｺ･轤朱ｫｯ・､
	BOOL FASTCALL HasEvent(Event *event) const;
										// 郢ｧ・､郢晏生ﾎｦ郢晏沺蝨定ｭ帛ｳｨ繝｡郢ｧ・ｧ郢晢ｿｽ郢ｧ・ｯ
	Event* FASTCALL GetFirstEvent()	const { return sch.first; }
										// 隴崢陋ｻ譏ｴ・ｽ・ｮ郢ｧ・､郢晏生ﾎｦ郢晏現・定愾髢・ｾ・ｽ
	int FASTCALL GetEventNum() const;
										// 郢ｧ・､郢晏生ﾎｦ郢晏現・ｽ・ｮ陋溷玄辟夂ｹｧ雋槫徐陟包ｿｽ

	// 陞溷､慚夊ｬｫ蝣ｺ・ｽ諛翫Ψ郢晢ｽｩ郢ｧ・ｰ
	BOOL dma_active;
										// DMAC郢ｧ・ｪ郢晢ｽｼ郢晏現ﾎ懃ｹｧ・ｯ郢ｧ・ｨ郢ｧ・ｹ郢晏沺諤剰怏・ｹ

private:
	DWORD FASTCALL GetMinRemain(DWORD hus);
										// 隴崢驕擾ｽｭ邵ｺ・ｮ郢ｧ・､郢晏生ﾎｦ郢晏現・定募干・・
	void FASTCALL ExecEvent(DWORD hus);
										// 郢ｧ・､郢晏生ﾎｦ郢昜ｺ･・ｮ貅ｯ・｡・ｽ
	void FASTCALL OnBreak(DWORD addr);
										// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢晉｣ｯ竊宣包ｽｨ

	// 陷・ｽ鬩幢ｽｨ郢晢ｿｽ郢晢ｽｼ郢ｧ・ｿ
	breakpoint_t breakp[BreakMax];
										// 郢晄じﾎ樒ｹ晢ｽｼ郢ｧ・ｯ郢晄亢縺・ｹ晢ｽｳ郢晢ｿｽ
	scheduler_t sch;
										// 郢ｧ・ｹ郢ｧ・ｱ郢ｧ・ｸ郢晢ｽ･郢晢ｽｼ郢晢ｽｩ

	// 郢晢ｿｽ郢晁・縺・ｹｧ・ｹ
	CPU *cpu;
										// CPU
	DMAC *dmac;
										// DMAC

	// 郢晢ｿｽ郢晢ｽｼ郢晄じﾎ・
	static const DWORD ClockTable[];
										// 郢ｧ・ｯ郢晢ｽｭ郢晢ｿｽ郢ｧ・ｯ郢晢ｿｽ郢晢ｽｼ郢晄じﾎ・
	static int CycleTable[0x1000];
										// 隴弱ｋ菫｣(hus)遶雁・縺礼ｹｧ・､郢ｧ・ｯ郢晢ｽｫ隰ｨ・ｰ
};

#endif	// scheduler_h
