//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 窶嗤窶喇ﾂ．(ytanaka@ipc-tokai.or.jp)
//	[ CPU(MC68000) ]
//
//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 窶嗤窶喇ﾂ．(ytanaka@ipc-tokai.or.jp)
//	[ CPU(MC68000) ]
//
//---------------------------------------------------------------------------

#if !defined(cpu_h)
#define cpu_h

#include "device.h"
#include "musashi_adapter.h"

//---------------------------------------------------------------------------
//
//	ﾅO窶｢窶昶凖ｨ窶ｹ`
//
//---------------------------------------------------------------------------
#if defined(__cplusplus)
extern "C" {
#endif	// __cplusplus


#if defined(__cplusplus)
}
#endif	// __cplusplus

//===========================================================================
//
//	CPU
//
//===========================================================================
class CPU : public Device
{
public:
	// 窶愿窶｢窶敞断ﾂーﾆ耽窶凖ｨ窶ｹ`
	typedef struct {
		DWORD dreg[8];					// ﾆ断ﾂーﾆ耽ﾆ椎槌淡ﾆ湛ﾆ耽
		DWORD areg[8];					// ﾆ但ﾆ檀ﾆ椎槌湛ﾆ椎槌淡ﾆ湛ﾆ耽
		DWORD sp;						// ﾆ湛ﾆ耽ﾆ鍛ﾆ誰窶能窶敕ｵ(USP or SSP)
		DWORD pc;						// ﾆ致ﾆ陳哉丹ﾆ停ｰﾆ停ぎﾆ谷ﾆ脱ﾆ停愴耽
		DWORD intr[8];					// ﾅ窶樞堙ｨﾂ最ｾ窶堙敖湘ｮ窶｢ﾃｱ
		DWORD sr;						// ﾆ湛ﾆ弾ﾂーﾆ耽ﾆ湛ﾆ椎槌淡ﾆ湛ﾆ耽
		DWORD intreq[8];				// ﾅ窶樞堙ｨﾂ最ｾ窶堙昶牌窶ｹﾂ≫ｰﾃｱﾂ絶・
		DWORD intack[8];
		DWORD odd;						// ﾅｽﾃﾂ行ﾆ谷ﾆ脱ﾆ停愴耽
	} cpu_t;

	typedef struct {
		DWORD erraddr;					// ﾆ竪ﾆ停ｰﾂーﾆ但ﾆ檀ﾆ椎槌湛
		DWORD errtime;					// ﾆ竪ﾆ停ｰﾂーﾅｽﾅｾ窶堙娯ｰﾂｼ窶・ﾅｽﾅｾﾅﾃ・
		DWORD intreq[8];				// ﾅ窶樞堙ｨﾂ最ｾ窶堙昶牌窶ｹﾂ≫ｰﾃｱﾂ絶・
		DWORD intack[8];
	} cpusub_t;

public:
	// ﾅﾃｮ窶怒ﾆ稚ﾆ叩ﾆ停愴誰ﾆ歎ﾆ停｡ﾆ停・
	CPU(VM *p);
										// ﾆ坦ﾆ停愴湛ﾆ暖ﾆ停ｰﾆ誰ﾆ耽
	BOOL FASTCALL Init();
										// ﾂ鞘ｰﾅﾃｺ窶ｰﾂｻ
	void FASTCALL Cleanup();
										// ﾆ誰ﾆ椎ﾂーﾆ停愴但ﾆ鍛ﾆ致
	void FASTCALL Reset();
										// ﾆ椎ﾆ短ﾆ鍛ﾆ暖
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// ﾆ短ﾂーﾆ置
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// ﾆ陳債ーﾆ檀
	void FASTCALL ApplyCfg(const Config *config);
										// ﾂ静昶凖ｨ窶廳窶廃

public:
	// ﾅO窶｢窶戡PI
	void FASTCALL GetCPU(cpu_t *buffer) const;
										// CPUﾆ椎槌淡ﾆ湛ﾆ耽ﾅｽﾃｦ窶慊ｾ
	void FASTCALL SetCPU(const cpu_t *buffer);
										// CPUﾆ椎槌淡ﾆ湛ﾆ耽ﾂ静昶凖ｨ
	DWORD FASTCALL Exec(int cycle) {
		DWORD result;

		if (::s68000exec(cycle) <= 0x80000000) {
			result = ::s68000context.odometer;
			::s68000context.odometer = 0;
			return result;
		}

		result = ::s68000context.odometer;
		result |= 0x80000000;
		::s68000context.odometer = 0;
		return result;
	}
										// ﾅｽﾃﾂ行
	BOOL FASTCALL Interrupt(int level, int vector);
										// ﾅ窶樞堙ｨﾂ最ｾ窶堙・
	void FASTCALL IntAck(int level);
										// ﾅ窶樞堙ｨﾂ最ｾ窶堙戡CK
	void FASTCALL IntCancel(int level);
										// ﾅ窶樞堙ｨﾂ最ｾ窶堙敞鱈ﾆ槌槌停愴短ﾆ停ｹ
	DWORD FASTCALL GetCycle() const		{ return ::s68000readOdometer(); }
										// ﾆ探ﾆ辰ﾆ誰ﾆ停ｹﾂ絶€敘ｽﾃｦ窶慊ｾ
	DWORD FASTCALL GetPC() const		{ return ::s68000readPC(); }
										// ﾆ致ﾆ陳哉丹ﾆ停ｰﾆ停ぎﾆ谷ﾆ脱ﾆ停愴耽ﾅｽﾃｦ窶慊ｾ
	void FASTCALL ResetInst();
										// RESET窶督ｽ窶氾・
	DWORD FASTCALL GetIOCycle()	const	{ return ::s68000getcounter(); }
										// I/Oﾆ探ﾆ辰ﾆ誰ﾆ停ｹﾅｽﾃｦ窶慊ｾ
	void FASTCALL SetIOCycle(DWORD c)	{ ::s68000setcounter(c); }
										// I/Oﾆ探ﾆ辰ﾆ誰ﾆ停ｹﾂ静昶凖ｨ
	void FASTCALL Release()				{ ::s68000releaseTimeslice(); }
										// CPUﾅｽﾃﾂ行窶堙ｰﾅ陳ｻ窶督ｽ窶氾溪€堙・€ｹﾂｭﾂ青ｧﾂ終窶板ｹ
	void FASTCALL BusErr(DWORD addr, BOOL read);
										// ﾆ弛ﾆ湛ﾆ竪ﾆ停€ｰﾂー
	void FASTCALL AddrErr(DWORD addr, BOOL read);
										// ﾆ但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停€ｰﾂー
	void FASTCALL BusErrLog(DWORD addr, DWORD stat);
										// ﾆ弛ﾆ湛ﾆ竪ﾆ停€ｰﾂー窶ｹLﾋ弯
	void FASTCALL AddrErrLog(DWORD addr, DWORD stat);
										// ﾆ但ﾆ檀ﾆ椎槌湛ﾆ竪ﾆ停ｰﾂー窶ｹLﾋ弯

private:
	cpusub_t sub;
										// 窶願窶｢窶敞断ﾂーﾆ耽
	Memory *memory;
										// ﾆ陳・停堡椎
	DMAC *dmac;
										// DMAC
	MFP *mfp;
										// MFP
	IOSC *iosc;
										// IOSC
	SCC *scc;
										// SCC
	MIDI *midi;
										// MIDI
	SCSI *scsi;
										// SCSI
	Scheduler *scheduler;
										// ﾆ湛ﾆ単ﾆ淡ﾆ停ｦﾂーﾆ停ｰ
};

#endif	// cpu_h
