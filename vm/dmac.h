//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ DMAC(HD63450) ]
//
//---------------------------------------------------------------------------

#if !defined(dmac_h)
#define dmac_h

#include "device.h"

//===========================================================================
//
//	DMAC
//
//===========================================================================
class DMAC : public MemDevice
{
public:
	// Member data definition (per channel)
	typedef struct {
		// Basic parameters
		DWORD xrm;						// Transfer mode
		DWORD dtyp;						// Device type
		BOOL dps;						// Port size (TRUE=16bit)
		DWORD pcl;						// PCL counter
		BOOL dir;						// Direction (TRUE=DAR increment)
		BOOL btd;						// Done block transfer
		DWORD size;						// Transfer size
		DWORD chain;					// Chaining mode
		DWORD reqg;						// REQ signal mode
		DWORD mac;						// Memory address update mode
		DWORD dac;						// Device address update mode

		// Status flags
		BOOL str;						// Start flag
		BOOL cnt;						// Counter flag
		BOOL hlt;						// HALT flag
		BOOL sab;						// Software abort flag
		BOOL intr;						// Interrupt possible flag
		BOOL coc;						// Channel operation complete flag
		BOOL boc;						// Block operation complete flag
		BOOL ndt;						// Normal end flag
		BOOL err;						// Error flag
		BOOL act;						// Active flag
		BOOL dit;						// Done interrupt flag
		BOOL pct;						// PCL negative edge detect flag
		BOOL pcs;						// PCL level (TRUE=High level)
		DWORD ecode;					// Error code

		// Address and counter registers
		DWORD mar;						// Memory address register
		DWORD dar;						// Device address register
		DWORD bar;						// Base address register
		DWORD mtc;						// Memory transfer count register
		DWORD btc;						// Base transfer count register
		DWORD mfc;						// Memory function code
		DWORD dfc;						// Device function code
		DWORD bfc;						// Base function code
		DWORD niv;						// Normal interrupt vector
		DWORD eiv;						// Error interrupt vector

		// Context save
		DWORD cp;						// Current priority
		DWORD bt;						// Burst transfer mode
		DWORD br;						// Burst request mode
		int type;						// Transfer type

		// Debug counters
		DWORD startcnt;					// Start counter
		DWORD errorcnt;					// Error counter
	} dma_t;

	// Member data definition (global)
	typedef struct {
		int transfer;					// Transfer flag (per channel)
		int load;						// Chain load flag (per channel)
		BOOL exec;						// Execute request active flag
		int current_ch;					// Current channel
		int cpu_cycle;					// CPU cycle counter
		int vector;						// Interrupt pending vector
	} dmactrl_t;

public:
	// Basic constructor
	DMAC(VM *p);
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
	void FASTCALL GetDMA(int ch, dma_t *buffer) const;
										// Get DMA
	void FASTCALL GetDMACtrl(dmactrl_t *buffer) const;
										// Get DMA control
	BOOL FASTCALL ReqDMA(int ch);
										// DMA request
	DWORD FASTCALL AutoDMA(DWORD cycle);
										// DMA auto request
	BOOL FASTCALL IsDMA() const;
										// DMA transfer query
	void FASTCALL BusErr(DWORD addr, BOOL read);
										// Bus error
	void FASTCALL AddrErr(DWORD addr, BOOL read);
										// Address error
	DWORD FASTCALL GetVector(int type) const;
										// Get vector
	void FASTCALL IntAck();
										// Interrupt ACK
	BOOL FASTCALL IsAct(int ch) const;
										// DMA transfer possible query
	void FASTCALL SetLegacyCntMode(BOOL enabled);
	BOOL FASTCALL IsLegacyCntMode() const;

private:
	// Channel register access
	DWORD FASTCALL ReadDMA(int ch, DWORD addr) const;
										// DMA read
	void FASTCALL WriteDMA(int ch, DWORD addr, DWORD data);
										// DMA write
	void FASTCALL SetDCR(int ch, DWORD data);
										// DCR set
	DWORD FASTCALL GetDCR(int ch) const;
										// DCR get
	void FASTCALL SetOCR(int ch, DWORD data);
										// OCR set
	DWORD FASTCALL GetOCR(int ch) const;
										// OCR get
	void FASTCALL SetSCR(int ch, DWORD data);
										// SCR set
	DWORD FASTCALL GetSCR(int ch) const;
										// SCR get
	void FASTCALL SetCCR(int ch, DWORD data);
										// CCR set
	DWORD FASTCALL GetCCR(int ch) const;
										// CCR get
	void FASTCALL SetCSR(int ch, DWORD data);
										// CSR set
	DWORD FASTCALL GetCSR(int ch) const;
										// CSR get
	void FASTCALL SetGCR(DWORD data);
										// GCR set

	// Channel operation
	void FASTCALL ResetDMA(int ch);
										// DMA reset
	void FASTCALL StartDMA(int ch);
										// DMA start
	void FASTCALL ContDMA(int ch);
										// DMA continue
	void FASTCALL AbortDMA(int ch);
										// DMA software abort
	void FASTCALL LoadDMA(int ch);
										// DMA block load
	void FASTCALL ErrorDMA(int ch, DWORD code);
										// Error
	void FASTCALL Interrupt();
										// Interrupt
	BOOL FASTCALL TransDMA(int ch);
										// DMA1 transfer

	// Table and device
	static const int MemDiffTable[8][4];
										// Memory difference table
	static const int DevDiffTable[8][4];
										// Device difference table
	Memory *memory;
										// Memory
	FDC *fdc;
										// FDC
	dma_t dma[4];
										// Structure (channel)
	dmactrl_t dmactrl;
										// Structure (global)
	BOOL legacy_cnt_mode;
};

#endif	// dmac_h
