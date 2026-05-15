//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI(ytanaka@ipc-tokai.or.jp)
//	[ MIDI(YM3802) ]
//
//---------------------------------------------------------------------------

#if !defined(midi_h)
#define midi_h

#include "device.h"
#include "event.h"

//===========================================================================
//
//	MIDI
//
//===========================================================================
class MIDI : public MemDevice
{
public:
	// Constants
	enum {
		TransMax = 0x2000,				// Transmit buffer size
		RecvMax = 0x2000				// Receive buffer size
	};

	// MIDI byte data structure
	typedef struct {
		DWORD data;						// Data portion (8bit)
		DWORD vtime;					// Virtual time
	} mididata_t;

	// Internal data structure
	typedef struct {
		// Reset
		BOOL reset;						// Reset flag
		BOOL access;					// Access flag

		// Board data, interrupts
		DWORD bid;						// Board ID (0: no board)
		DWORD ilevel;					// Interrupt level
		int vector;						// Interrupt vector number

		// MCS registers (common)
		DWORD wdr;						// Write data register
		DWORD rgr;						// Register group register

		// MCS registers (interrupts)
		DWORD ivr;						// Interrupt vector register
		DWORD isr;						// Interrupt service register
		DWORD imr;						// Interrupt mask register
		DWORD ier;						// Interrupt enable register

		// MCS registers (message mode)
		DWORD dmr;						// Message mode register
		DWORD dcr;						// Message control register

		// MCS registers (receive)
		DWORD rrr;						// Receive rate register
		DWORD rmr;						// Receive mode register
		DWORD amr;						// Address match register
		DWORD adr;						// Address device register
		DWORD asr;						// Address status register
		DWORD rsr;						// Receive buffer status register
		DWORD rcr;						// Receive control register
		DWORD rcn;						// Receive counter

		// MCS registers (transmit)
		DWORD trr;						// Transmit rate register
		DWORD tmr;						// Transmit mode register
		BOOL tbs;						// Transmit BUSY register
		DWORD tcr;						// Transmit control register
		DWORD tcn;						// Transmit counter

		// MCS registers (FSK)
		DWORD fsr;						// FSK status register
		DWORD fcr;						// FSK control register

		// MCS registers (counters)
		DWORD ccr;						// Clock control register
		DWORD cdr;						// Clock data register
		DWORD ctr;						// Clock timer register
		DWORD srr;						// Serial clock counter register
		DWORD scr;						// Clock scaler register
		DWORD sct;						// Clock counter
		DWORD spr;						// Sample counter register
		DWORD str;						// Sample timer register
		DWORD gtr;						// General timer register
		DWORD mtr;						// MIDI clock timer register

		// MCS registers (GPIO)
		DWORD edr;						// Edge direction register
		DWORD eor;						// Edge output register
		DWORD eir;						// Edge input register

		// Normal buffer
		DWORD normbuf[16];				// Normal buffer
		DWORD normread;					// Normal buffer read
		DWORD normwrite;				// Normal buffer write
		DWORD normnum;					// Normal buffer count
		DWORD normtotal;				// Normal buffer total

		// Real-time transmit buffer
		DWORD rtbuf[4];					// Real-time transmit buffer
		DWORD rtread;					// Real-time transmit buffer read
		DWORD rtwrite;					// Real-time transmit buffer write
		DWORD rtnum;					// Real-time transmit buffer count
		DWORD rttotal;					// Real-time transmit buffer total

		// Standard buffer
		DWORD stdbuf[0x80];				// Standard buffer
		DWORD stdread;					// Standard buffer read
		DWORD stdwrite;					// Standard buffer write
		DWORD stdnum;					// Standard buffer count
		DWORD stdtotal;					// Standard buffer total

		// Real-time receive buffer
		DWORD rrbuf[4];					// Real-time receive buffer
		DWORD rrread;					// Real-time receive buffer read
		DWORD rrwrite;					// Real-time receive buffer write
		DWORD rrnum;					// Real-time receive buffer count
		DWORD rrtotal;					// Real-time receive buffer total

		// Transmit buffer (used to exchange with device)
		mididata_t *transbuf;			// Transmit buffer
		DWORD transread;				// Transmit buffer read
		DWORD transwrite;				// Transmit buffer write
		DWORD transnum;					// Transmit buffer count

		// Receive buffer (used to exchange with device)
		mididata_t *recvbuf;			// Receive buffer
		DWORD recvread;				// Receive buffer read
		DWORD recvwrite;				// Receive buffer write
		DWORD recvnum;					// Receive buffer count
	} midi_t;

public:
	// Basic operations
	MIDI(VM *p);
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

	// External I/O device
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
	BOOL FASTCALL IsActive() const;
										// MIDI active check
	BOOL FASTCALL Callback(Event *ev);
										// Event callback
	void FASTCALL IntAck(int level);
										// Interrupt ACK
	void FASTCALL GetMIDI(midi_t *buffer) const;
										// Get internal data
	DWORD FASTCALL GetExCount(int index) const;
										// Get ex counter

	// Transmit (MIDI OUT)
	DWORD FASTCALL GetTransNum() const;
										// Get transmit buffer count
	const mididata_t* FASTCALL GetTransData(DWORD proceed);
										// Get transmit buffer data
	void FASTCALL DelTransData(DWORD number);
										// Delete transmit buffer
	void FASTCALL ClrTransData();
										// Clear transmit buffer

	// Receive (MIDI IN)
	void FASTCALL SetRecvData(const BYTE *ptr, DWORD length);
										// Set receive data
	void FASTCALL SetRecvDelay(int delay);
										// Set receive delay

	// Reset
	BOOL FASTCALL IsReset() const		{ return midi.reset; }
										// Get reset flag
	void FASTCALL ClrReset()			{ midi.reset = FALSE; }
										// Clear reset flag

private:
	void FASTCALL Receive();
										// Receive callback
	void FASTCALL Transmit();
										// Transmit callback
	void FASTCALL Clock();
										// MIDI clock output
	void FASTCALL General();
										// General timer callback

	void FASTCALL InsertTrans(DWORD data);
										// Insert to transmit buffer
	void FASTCALL InsertRecv(DWORD data);
										// Insert to receive buffer
	void FASTCALL InsertNorm(DWORD data);
										// Insert to normal buffer
	void FASTCALL InsertRT(DWORD data);
										// Insert to real-time transmit buffer
	void FASTCALL InsertStd(DWORD data);
										// Insert to standard buffer
	void FASTCALL InsertRR(DWORD data);
										// Insert to real-time receive buffer

	void FASTCALL ResetReg();
										// Reset registers
	DWORD FASTCALL ReadReg(DWORD reg);
										// Register read
	void FASTCALL WriteReg(DWORD reg, DWORD data);
										// Register write
	DWORD FASTCALL ReadRegRO(DWORD reg) const;
										// Register read (ReadOnly)

	void FASTCALL SetICR(DWORD data);
										// ICR set
	void FASTCALL SetIOR(DWORD data);
										// IOR set
	void FASTCALL SetIMR(DWORD data);
										// IMR set
	void FASTCALL SetIER(DWORD data);
										// IER set
	void FASTCALL SetDMR(DWORD data);
										// DMR set
	void FASTCALL SetDCR(DWORD data);
										// DCR set
	DWORD FASTCALL GetDSR() const;
										// DSR get
	void FASTCALL SetDNR(DWORD data);
										// DNR set
	void FASTCALL SetRRR(DWORD data);
										// RRR set
	void FASTCALL SetRMR(DWORD data);
										// RMR set
	void FASTCALL SetAMR(DWORD data);
										// AMR set
	void FASTCALL SetADR(DWORD data);
										// ADR set
	DWORD FASTCALL GetRSR() const;
										// RSR get
	void FASTCALL SetRCR(DWORD data);
										// RCR set
	DWORD FASTCALL GetRDR();
										// RDR get (updated)
	DWORD FASTCALL GetRDRRO() const;
										// RDR get (Read Only)
	void FASTCALL SetTRR(DWORD data);
										// TRR set
	void FASTCALL SetTMR(DWORD data);
										// TMR set
	DWORD FASTCALL GetTSR() const;
										// TSR get
	void FASTCALL SetTCR(DWORD data);
										// TCR set
	void FASTCALL SetTDR(DWORD data);
										// TDR set
	DWORD FASTCALL GetFSR() const;
										// FSR get
	void FASTCALL SetFCR(DWORD data);
										// FCR set
	void FASTCALL SetCCR(DWORD data);
										// CCR set
	void FASTCALL SetCDR(DWORD data);
										// CDR set
	DWORD FASTCALL GetSRR() const;
										// SRR get
	void FASTCALL SetSCR(DWORD data);
										// SCR set
	void FASTCALL SetSPR(DWORD data, BOOL high);
										// SPR set
	void FASTCALL SetGTR(DWORD data, BOOL high);
										// GTR set
	void FASTCALL SetMTR(DWORD data, BOOL high);
										// MTR set
	void FASTCALL SetEDR(DWORD data);
										// EDR set
	void FASTCALL SetEOR(DWORD data);
										// EOR set
	DWORD FASTCALL GetEIR() const;
										// EIR get

	void FASTCALL CheckRR();
										// Real-time message receive buffer check
	void FASTCALL Interrupt(int type, BOOL flag);
										// Interrupt request
	void FASTCALL IntCheck();
										// Interrupt check
	Event event[3];
										// Events
	midi_t midi;
										// Internal data
	Sync *sync;
										// Data sync
	DWORD recvdelay;
										// Receive delay (hus)
	DWORD ex_cnt[4];
										// Ex counters
};

#endif	// midi_h