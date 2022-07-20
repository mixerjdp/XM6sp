//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ スケジューラ ]
//
//---------------------------------------------------------------------------

#if !defined(scheduler_h)
#define scheduler_h

#include "device.h"
#include "starcpu.h"

//---------------------------------------------------------------------------
//
//	高速ウェイト(Starscream専用)
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
//	スケジューラ
//
//===========================================================================
class Scheduler : public Device
{
public:
	// ブレークポイント定義
	typedef struct{
		BOOL use;						// 使用フラグ
		DWORD addr;						// アドレス
		BOOL enable;					// 有効フラグ
		DWORD time;						// 停止時の時間
		DWORD count;					// 停止回数
	} breakpoint_t;

	// スケジューラ定義
	typedef struct {
		// 時間
		DWORD total;					// トータル実行時間
		DWORD one;						// 1回の実行時間
		DWORD sound;					// サウンド更新時間

		// CPU
		int clock;						// CPUクロック(0～5)
		DWORD speed;					// CPU速度(clockにより決まる)
		int cycle;						// CPUサイクル数
		DWORD time;						// CPUサイクル調整用時間

		// ブレークポイント
		BOOL brk;						// ブレークした
		BOOL check;						// 有効なブレークポイントあり

		// イベント
		Event *first;					// 最初のイベント
		BOOL exec;						// イベント実行中
	} scheduler_t;

	// 個数定義
	enum {
		BreakMax = 8					// ブレークポイント総数
	};

public:
	// 基本ファンクション
	Scheduler(VM *p);
										// コンストラクタ
	BOOL FASTCALL Init();
										// 初期化
	void FASTCALL Cleanup();
										// クリーンアップ
	void FASTCALL Reset();
										// リセット
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// セーブ
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// ロード
	void FASTCALL ApplyCfg(const Config *config);
										// 設定適用
#if defined(_DEBUG)
	void FASTCALL AssertDiag() const;
										// 診断
#endif	// _DEBUG

	// 外部API
	void FASTCALL GetScheduler(scheduler_t *buffer) const;
										// 内部データ取得
	DWORD FASTCALL Exec(DWORD hus);
										// 実行
	DWORD FASTCALL Trace(DWORD hus);
										// トレース
	void FASTCALL Break()				{ sch.brk = TRUE; }
										// 実行中止
#ifdef SCHEDULER_FASTWAIT
	void FASTCALL Wait(DWORD cycle)		{ sch.cycle += cycle; if (::s68000iocycle != (DWORD)-1) ::s68000iocycle -= cycle; }
										// CPUウェイト(すべてインライン)
#else
	void FASTCALL Wait(DWORD cycle)		{ ::s68000wait(cycle); sch.cycle += cycle; }
										// CPUウェイト
#endif	// SCHEDULER_FASTWAIT

	// 特殊操作(DMAC向け)
	int FASTCALL GetCPUCycle() const	{ return sch.cycle; }
										// ウェイト数取得
	void FASTCALL SetCPUCycle(int cycle) { sch.cycle = cycle; }
										// ウェイト数設定

	// 時間情報
	DWORD FASTCALL GetTotalTime() const	{ return (GetPassedTime() + sch.total); }
										// トータル実行時間を取得
	DWORD FASTCALL GetOneTime() const	{ return sch.one; }
										// 微少実行時間を取得
	DWORD FASTCALL GetPassedTime() const;
										// 経過時間を取得
	DWORD FASTCALL GetCPUSpeed() const	{ return sch.speed; }
										// CPU速度取得
	void FASTCALL SetCPUSpeed(DWORD speed);
										// CPU速度設定
	DWORD FASTCALL GetSoundTime() const	{ return sch.sound; }
										// サウンド時間を取得
	void FASTCALL SetSoundTime(DWORD hus) { sch.sound = hus; }
										// サウンド時間を設定

	// ブレークポイント
	void FASTCALL SetBreak(DWORD addr, BOOL enable = TRUE);
										// ブレークポイント設定
	void FASTCALL DelBreak(DWORD addr);
										// ブレークポイント削除
	void FASTCALL GetBreak(int index, breakpoint_t *buf) const;
										// ブレークポイント取得
	void FASTCALL EnableBreak(int index, BOOL enable = TRUE);
										// ブレークポイント有効・無効
	void FASTCALL ClearBreak(int index);
										// ブレーク回数クリア
	void FASTCALL AddrBreak(int index, DWORD addr);
										// ブレークアドレス変更
	int FASTCALL IsBreak(DWORD addr, BOOL any = FALSE) const;
										// ブレークアドレスチェック

	// イベント
	void FASTCALL AddEvent(Event *event);
										// イベント追加
	void FASTCALL DelEvent(Event *event);
										// イベント削除
	BOOL FASTCALL HasEvent(Event *event) const;
										// イベント所有チェック
	Event* FASTCALL GetFirstEvent()	const { return sch.first; }
										// 最初のイベントを取得
	int FASTCALL GetEventNum() const;
										// イベントの個数を取得

	// 外部操作フラグ
	BOOL dma_active;
										// DMACオートリクエスト有効

private:
	DWORD FASTCALL GetMinRemain(DWORD hus);
										// 最短のイベントを得る
	void FASTCALL ExecEvent(DWORD hus);
										// イベント実行
	void FASTCALL OnBreak(DWORD addr);
										// ブレークポイント適用

	// 内部データ
	breakpoint_t breakp[BreakMax];
										// ブレークポイント
	scheduler_t sch;
										// スケジューラ

	// デバイス
	CPU *cpu;
										// CPU
	DMAC *dmac;
										// DMAC

	// テーブル
	static const DWORD ClockTable[];
										// クロックテーブル
	static int CycleTable[0x1000];
										// 時間(hus)→サイクル数
};

#endif	// scheduler_h
