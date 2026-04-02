//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI(ytanaka@ipc-tokai.or.jp)
//	[ FDC(uPD72065) ]
//
//---------------------------------------------------------------------------

#if !defined(fdc_h)
#define fdc_h

#include "device.h"
#include "event.h"

//===========================================================================
//
//	FDC
//
//===========================================================================
class FDC : public MemDevice
{
public:
	// Phase definition
	enum fdcphase {
		idle,							// Idle phase
		command,						// Command phase
		execute,						// Execute phase (general)
		read,							// Execute phase (Read)
		write,							// Execute phase (Write)
		result							// Result phase
	};

	// Status register definition
	enum {
		sr_rqm = 0x80,					// Request For Master
		sr_dio = 0x40,					// Data Input / Output
		sr_ndm = 0x20,					// Non-DMA Mode
		sr_cb = 0x10,					// FDC Busy
		sr_d3b = 0x08,					// Drive3 Seek
		sr_d2b = 0x04,					// Drive2 Seek
		sr_d1b = 0x02,					// Drive1 Seek
		sr_d0b = 0x01					// Drive0 Seek
	};

	// Command definition
	enum fdccmd {
		read_data,						// READ DATA
		read_del_data,					// READ DELETED DATA
		read_id,						// READ ID
		write_id,						// WRITE ID
		write_data,						// WRITE DATA
		write_del_data,					// WRITE DELETED DATA
		read_diag,						// READ DIAGNOSTIC
		scan_eq,						// SCAN EQUAL
		scan_lo_eq,					// SCAN LOW OR EQUAL
		scan_hi_eq,					// SCAN HIGH OR EQUAL
		seek,							// SEEK
		recalibrate,					// RECALIBRATE
		sense_int_stat,					// SENSE INTERRUPT STATUS
		sense_dev_stat,					// SENSE DEVICE STATUS
		specify,						// SPECIFY
		set_stdby,						// SET STANDBY
		reset_stdby,					// RESET STANDBY
		fdc_reset,						// SOFTWARE RESET
		invalid,						// INVALID
		no_cmd							// (NO COMMAND)
	};

	// Structure definition
	typedef struct {
		fdcphase phase;					// Phase
		fdccmd cmd;					// Command

		int in_len;					// Input allocation length
		int in_cnt;					// Input count
		DWORD in_pkt[0x10];			// Input packet
		int out_len;					// Output allocation length
		int out_cnt;					// Output count
		DWORD out_pkt[0x10];			// Output packet

		DWORD dcr;					// Drive control register
		DWORD dsr;					// Drive select register
		DWORD sr;					// Status register
		DWORD dr;					// Data register
		DWORD st[4];					// ST0-ST3

		DWORD srt;					// SRT
		DWORD hut;					// HUT
		DWORD hlt;					// HLT
		DWORD hd;					// HD
		DWORD us;					// US
		DWORD cyl[4];					// Current cylinder
		DWORD chrn[4];					// Specified C,H,R,N

		DWORD eot;					// EOT
		DWORD gsl;					// GSL
		DWORD dtl;					// DTL
		DWORD sc; 					// SC
		DWORD gpl;					// GAP3
		DWORD d;					// Format data
		DWORD err;					// Error code
		BOOL seek;					// Seek interrupt request
		BOOL ndm;					// Non-DMA mode
		BOOL mfm;					// MFM mode
		BOOL mt;					// Multi-track
		BOOL sk;					// Skip DDAM
		BOOL tc;					// TC
		BOOL load;					// Head load

		int offset;					// Buffer offset
		int len;					// Remaining length
		BYTE buffer[0x4000];			// Data buffer

		BOOL fast;					// Fast mode
		BOOL dual;					// Dual drive
	} fdc_t;

public:
	// Basic constructor
	FDC(VM *p);
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

	// External device access
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
	BOOL FASTCALL Callback(Event *ev);
										// Event callback
	const fdc_t* FASTCALL GetWork() const;
										// Get structure address
	void FASTCALL CompleteSeek(int drive, BOOL result);
										// Seek complete
	void FASTCALL SetTC();
										// TC signal

private:
	void FASTCALL Idle();
										// Idle phase
	void FASTCALL Command(DWORD data);
										// Command phase
	void FASTCALL CommandRW(fdccmd cmd, DWORD data);
										// Command phase (R/W sub)
	void FASTCALL Execute();
										// Execute phase
	void FASTCALL ReadID();
										// Execute phase (Read ID)
	void FASTCALL ExecuteRW();
										// Execute phase (R/W sub)
	BYTE FASTCALL Read();
										// Execute phase (Read)
	void FASTCALL Write(DWORD data);
										// Execute phase (Write)
	void FASTCALL Compare(DWORD data);
										// Execute phase (Compare)
	void FASTCALL Result();
										// Result phase
	void FASTCALL ResultRW();
										// Result phase (R/W sub)
	void FASTCALL Interrupt(BOOL flag);
										// Interrupt
	void FASTCALL SoftReset();
										// Software reset
	void FASTCALL MakeST3();
										// Create ST3
	BOOL FASTCALL ReadData();
										// READ (DELETED) DATA command
	BOOL FASTCALL WriteData();
										// WRITE (DELETED) DATA command
	BOOL FASTCALL ReadDiag();
										// READ DIAGNOSTIC command
	BOOL FASTCALL WriteID();
										// WRITE ID command
	BOOL FASTCALL Scan();
										// SCAN family command
	void FASTCALL EventRW();
										// Event processing (R/W)
	void FASTCALL EventErr(DWORD hus);
										// Event processing (Error)
	void FASTCALL WriteBack();
										// Write back
	BOOL FASTCALL NextSector();
										// Multi-sector processing
	IOSC *iosc;
										// IOSC
	DMAC *dmac;
										// DMAC
	FDD *fdd;
										// FDD
	Event event;
										// Event
	fdc_t fdc;
										// FDC member data
};

#endif	// fdc_h
