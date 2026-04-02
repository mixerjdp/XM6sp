//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 PI(ytanaka@ipc-tokai.or.jp)
//	[ MFP(MC68901) ]
//
//---------------------------------------------------------------------------

#if !defined(mfp_h)
#define mfp_h

#include "device.h"
#include "event.h"

//===========================================================================
//
//	MFP
//
//===========================================================================
class MFP : public MemDevice
{
public:
	// Internal data structure
	typedef struct {
		// Interrupt
		BOOL ier[0x10];					// Interrupt enable register
		BOOL ipr[0x10];					// Interrupt pending register
		BOOL isr[0x10];					// Interrupt in-service register
		BOOL imr[0x10];					// Interrupt mask register
		BOOL ireq[0x10];				// Interrupt request register
		DWORD vr;						// Vector register
		int iidx;						// Interrupt index

		// Timer
		DWORD tcr[4];					// Timer control register
		DWORD tdr[4];					// Timer data register
		DWORD tir[4];					// Timer counter(timer register)
		DWORD tbr[2];					// Timer backup register
		DWORD sram;						// si, info.ram speed flag
		DWORD tecnt;					// Event count mode counter

		// GPIP
		DWORD gpdr;						// GPIP data register
		DWORD aer;						// Active edge register
		DWORD ddr;						// Data direction register
		DWORD ber;						// Backup edge register

		// USART
		DWORD scr;						// SYNC register
		DWORD ucr;						// USART control register
		DWORD rsr;						// Receiver status register
		DWORD tsr;						// Transmitter status register
		DWORD rur;						// Receiver data register
		DWORD tur;						// Transmitter data register
		DWORD buffer[0x10];				// USART FIFO buffer
		int datacount;					// USART valid data count
		int readpoint;					// USART MFP read pointer
		int writepoint;					// USART keyboard write pointer
	} mfp_t;

public:
	// Basic constructor
	MFP(VM *p);
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

	// Sub device
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
	void FASTCALL GetMFP(mfp_t *buffer) const;
										// Get MFP
	BOOL FASTCALL Callback(Event *ev);
										// Event callback
	void FASTCALL IntAck();
										// Interrupt acknowledge
	void FASTCALL EventCount(int channel, int value);
										// Event count
	void FASTCALL SetGPIP(int num, int value);
										// GPIP set
	void FASTCALL KeyData(DWORD data);
										// Key data set
	DWORD FASTCALL GetVR() const;
										// Vector register get

private:
	// Interrupt controller
	void FASTCALL Interrupt(int level, BOOL enable);
										// Interrupt
	void FASTCALL IntCheck();
										// Interrupt priority check
	void FASTCALL SetIER(int offset, DWORD data);
										// IER set
	DWORD FASTCALL GetIER(int offset) const;
										// IER get
	void FASTCALL SetIPR(int offset, DWORD data);
										// IPR set
	DWORD FASTCALL GetIPR(int offset) const;
										// IPR get
	void FASTCALL SetISR(int offset, DWORD data);
										// ISR set
	DWORD FASTCALL GetISR(int offset) const;
										// ISR get
	void FASTCALL SetIMR(int offset, DWORD data);
										// IMR set
	DWORD FASTCALL GetIMR(int offset) const;
										// IMR get
	void FASTCALL SetVR(DWORD data);
										// VR set
	static const char* IntDesc[0x10];
										// Interrupt cause table

	// Timer
	void FASTCALL SetTCR(int channel, DWORD data);
										// TCR set
	DWORD FASTCALL GetTCR(int channel) const;
										// TCR get
	void FASTCALL SetTDR(int channel, DWORD data);
										// TDR set
	DWORD FASTCALL GetTIR(int channel) const;
										// TIR get
	void FASTCALL Proceed(int channel);
										// Timer proceed
	Event timer[4];
										// Timer event
	static const int TimerInt[4];
										// Timer interrupt table
	static const DWORD TimerHus[8];
										// Timer time table

	// GPIP
	void FASTCALL SetGPDR(DWORD data);
										// GPDR set
	void FASTCALL IntGPIP();
										// GPIP interrupt
	static const int GPIPInt[8];
										// GPIP interrupt table

	// USART
	void FASTCALL SetRSR(DWORD data);
										// RSR set
	void FASTCALL Receive();
										// USART data receive
	void FASTCALL SetTSR(DWORD data);
										// TSR set
	void FASTCALL Transmit(DWORD data);
										// USART data transmit
	void FASTCALL USART();
										// USART
	Event usart;
										// USART event
	Sync *sync;
										// USART Sync
	Keyboard *keyboard;
										// Keyboard
	mfp_t mfp;
										// Internal data
};

#endif	// mfp_h
