//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[ ADPCM (MSM6258V) ]
//
//---------------------------------------------------------------------------

#if !defined(adpcm_h)
#define adpcm_h

#include "device.h"
#include "event.h"

//===========================================================================
//
//	ADPCM
//
//===========================================================================
class ADPCM : public MemDevice
{
public:
	// Sample data definition
	typedef struct {
		DWORD panpot;					// Panpot
		BOOL play;						// Play mode
		BOOL rec;						// Record mode
		BOOL active;					// Active flag
		BOOL started;					// Play started, data flag received
		DWORD clock;					// Master clock (4 or 8)
		DWORD ratio;					// Clock ratio (0 or 1 or 2)
		DWORD speed;					// Playback speed (128,192,256,384,512)
		DWORD data;						// Sample data (4bit * 2sample)

		int offset;						// Current offset (0-48)
		int sample;						// Sample data
		int out;						// Output data
		int vol;						// Volume

		BOOL enable;					// Enable flag
		BOOL sound;						// ADPCM output enable flag
		DWORD readpoint;				// Buffer read point
		DWORD writepoint;				// Buffer write point
		DWORD number;					// Buffer valid data count
		int wait;						// Internal wait
		DWORD sync_cnt;					// Internal counter
		DWORD sync_rate;				// Internal rate (882,960,etc...)
		DWORD sync_step;				// Internal step (linear interpolation)
		BOOL interp;					// Interpolation flag
	} adpcm_t;

	typedef struct {
		DWORD start_events;
		DWORD stop_events;
		DWORD req_total;
		DWORD req_ok;
		DWORD req_fail;
		DWORD decode_calls;
		DWORD underrun_head_events;
		DWORD underrun_interp_events;
		DWORD underrun_linear_events;
		DWORD silence_fill_events;
		DWORD stale_nonzero_events;
		DWORD max_buffer_samples;
		DWORD last_data;
	} adpcm_diag_t;

public:
	// Basic functions
	ADPCM(VM *p);
										// Constructor
	BOOL FASTCALL Init();
										// Initialization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
	void FASTCALL ApplyCfg(const Config *config);
										// Apply config
#if !defined(NDEBUG)
	void FASTCALL AssertDiag() const;
										// Assertion
#endif	// NDEBUG

	// Memory device
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										// Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read only

	// External API
	void FASTCALL GetADPCM(adpcm_t *buffer);
										// Get ADPCM data
	BOOL FASTCALL Callback(Event *ev);
										// Event callback
	void FASTCALL SetClock(DWORD clk);
										// Set master clock
	void FASTCALL SetRatio(DWORD ratio);
										// Set clock ratio
	void FASTCALL SetPanpot(DWORD pan);
										// Set panpot
	void FASTCALL Enable(BOOL enable);
										// Internal enable
	void FASTCALL InitBuf(DWORD rate);
										// Buffer initialization
	void FASTCALL GetBuf(DWORD *buffer, int samples);
										// Get buffer
	void FASTCALL Wait(int num);
										// Wait
	void FASTCALL EnableADPCM(BOOL flag) { adpcm.sound = flag; }
										// Play enable
	void FASTCALL SetVolume(int volume);
										// Volume setting
	void FASTCALL SetArianshuuLoopFix(BOOL enabled);
										// Arianshuu: prevents infinite hold on ADPCM drain
	void FASTCALL GetDiag(adpcm_diag_t *buffer) const;
										// Internal ADPCM telemetry
	BOOL FASTCALL IsArianshuuLoopFixEnabled() const { return quirk_arianshuu_loop_fix; }
										// Arianshuu quirk state
	void FASTCALL ClrStarted()			{ adpcm.started = FALSE; }
										// Clear started flag
	BOOL FASTCALL IsStarted() const		{ return adpcm.started; }
										// Get started flag

private:
	enum {
		BufMax = 0x10000				// Buffer size
	};
	void FASTCALL MakeTable();
										// Make table
	void FASTCALL CalcSpeed();
										// Recalculate speed
	void FASTCALL Start(int type);
										// Record/play start
	void FASTCALL Stop();
										// Record/play stop
	void FASTCALL Decode(int data, int num, BOOL valid);
										// 4bit decode
	Event event;
										// Timer event
	adpcm_t adpcm;
										// Internal data
	DMAC *dmac;
										// DMAC
	DWORD *adpcmbuf;
										// Internal buffer
	int DiffTable[49 * 16];
										// Diff table
	static const int NextTable[16];
										// Next table
	static const int OffsetTable[58];
										// Offset table
	BOOL quirk_arianshuu_loop_fix;
	int quirk_stuck_l;
	int quirk_stuck_r;
	adpcm_diag_t diag;
};

#endif	// adpcm_h
