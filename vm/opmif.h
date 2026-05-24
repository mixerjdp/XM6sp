//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ OPM(YM2151) ]
//
//---------------------------------------------------------------------------

#if !defined(opmif_h)
#define opmif_h

#include "device.h"
#include "event.h"
namespace FM { class OPM; }
//===========================================================================
//
//	OPM
//
//===========================================================================
class OPMIF : public MemDevice
{
public:
	// Internal data structure
	typedef struct {
		DWORD reg[0x100];				// Register
		DWORD key[8];					// Key
		DWORD addr;						// Current address
		BOOL busy;						// BUSY flag
		BOOL enable[2];					// Timer enable
		BOOL action[2];					// Timer action
		BOOL interrupt[2];				// Timer interrupt
		DWORD time[2];					// Timer value
		BOOL started;					// Start flag
	} opm_t;

	// Buffer management structure
	typedef struct {
		DWORD max;						// Maximum
		DWORD num;						// Valid data count
		DWORD read;						// Read pointer
		DWORD write;					// Write pointer
		DWORD samples;					// Processed samples
		DWORD rate;						// Sampling rate
		DWORD under;					// Underflow count
		DWORD over;						// Overflow count
		BOOL sound;						// FM enable
	} opmbuf_t;

public:
	// Basic member function
	OPMIF(VM *p);
										// Constructor
	BOOL FASTCALL Init();
										// Initialize
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
										// Assert
#endif	// NDEBUG

	// External device
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
	void FASTCALL GetOPM(opm_t *buffer);
										// Get internal data
	BOOL FASTCALL Callback(Event *ev);
										// Event callback
	void FASTCALL Output(DWORD addr, DWORD data);
										// Register output
	void FASTCALL SetEngine(FM::OPM *p);
										// Engine specification
	void FASTCALL InitBuf(DWORD rate);
										// Initialize buffer
	DWORD FASTCALL ProcessBuf();
										// Process buffer
	void FASTCALL GetBuf(DWORD *buf, int samples);
										// Get buffer
	void FASTCALL GetBufInfo(opmbuf_t *buffer);
										// Get buffer info
	void FASTCALL EnableFM(BOOL flag)	{ bufinfo.sound = flag; }
										// FM sound enable
	void FASTCALL ClrStarted()			{ opm.started = FALSE; }
										// Clear start flag
	BOOL FASTCALL IsStarted() const		{ return opm.started; }
										// Get start flag

private:
	void FASTCALL CalcTimerA();
										// Timer-A calculation
	void FASTCALL CalcTimerB();
										// Timer-B calculation
	void FASTCALL CtrlTimer(DWORD data);
										// Timer control
	void FASTCALL CtrlCT(DWORD data);
										// CT control
	MFP *mfp;
										// MFP
	ADPCM *adpcm;
										// ADPCM
	FDD *fdd;
										// FDD
	opm_t opm;
										// OPM internal data
	opmbuf_t bufinfo;
										// Buffer info
	Event event[2];
										// Timer event
	FM::OPM *engine;
										// Sound engine
	enum {
		BufMax = 0x10000				// Buffer size
	};
	DWORD *opmbuf;
										// Sound buffer
};

#endif	// opmif_h