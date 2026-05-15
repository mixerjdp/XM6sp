//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//Copyright (C) 2001-2003  PI.(ytanaka@ipc-tokai.or.jp)
//	[ SCC(Z8530) ]
//
//---------------------------------------------------------------------------

#if !defined(scc_h)
#define scc_h

#include "device.h"
#include "event.h"

//===========================================================================
//
//	SCC
//
//===========================================================================
class SCC : public MemDevice
{
public:
	//Interrupt type
	enum itype_t {
		rxi,							//Receive interrupt
		rsi,							//Special Rx condition interrupt
		txi,							//Send interrupt
		exti							//External status change interrupt
	};

	//Channel definition
	typedef struct {
		//Global
		DWORD index;					//Channel number (0 or 1)

		// RR0
		BOOL ba;						// Break/Abort
		BOOL tu;						//Tx underrun
		BOOL cts;						// CTS
		BOOL sync;						// SYNC
		BOOL dcd;						// DCD
		BOOL zc;						//Zero count

		// WR0
		DWORD reg;						//Access register select
		BOOL ph;						//Point high (upper register select)
		BOOL txpend;					//Send interrupt pending
		BOOL rxno;						//No receive data

		// RR1
		BOOL framing;					//Framing error
		BOOL overrun;					//Overrun error
		BOOL parerr;					//Parity error
		BOOL txsent;					//Transmit complete

		// WR1
		BOOL extie;						//External status interrupt enable
		BOOL txie;						//Send interrupt enable
		BOOL parsp;						//Parity error to S-Rx interrupt
		DWORD rxim;						//Receive interrupt mode

		// RR3
		BOOL rxip;						//Receive interrupt pending
		BOOL rsip;						//Special Rx interrupt pending
		BOOL txip;						//Send interrupt pending
		BOOL extip;						//External status change interrupt pending

		// WR3
		DWORD rxbit;					//Receive character bit length (5-8)
		BOOL aen;						//Auto mode enable
		BOOL rxen;						//Receive enable

		// WR4
		DWORD clkm;						//Clock mode
		DWORD stopbit;					//Stop bits
		DWORD parity;					//Parity mode

		// WR5
		BOOL dtr;						//DTR signal line
		DWORD txbit;					//Send character bit length (5-8)
		BOOL brk;						//Send break
		BOOL txen;						//Send enable
		BOOL rts;						//RTS signal line

		// WR8
		DWORD tdr;						//Send data register
		BOOL tdf;						//Send data valid

		// WR12, WR13
		DWORD tc;						//Baud rate setting

		// WR14
		BOOL loopback;					//Loopback mode
		BOOL aecho;						//Auto echo mode
		BOOL dtrreq;					//DTR signal line enable
		BOOL brgsrc;					//Baud rate generator clock source
		BOOL brgen;						//Baud rate generator enable

		// WR15
		BOOL baie;						//Break/Abort interrupt enable
		BOOL tuie;						//Tx underrun interrupt enable
		BOOL ctsie;						//CTS interrupt enable
		BOOL syncie;					// SYNC interrupt enable
		BOOL dcdie;						//DCD interrupt enable
		BOOL zcie;						//Zero count interrupt enable

		//Communication speed
		DWORD baudrate;					//Baud rate
		DWORD cps;						//Character/sec
		DWORD speed;					//Speed (hus unit)

		//Receive FIFO
		DWORD rxfifo;					//Receive FIFO valid count
		DWORD rxdata[3];				//Receive FIFO data

		//Receive buffer
		BYTE rxbuf[0x1000];				//Receive data
		DWORD rxnum;					//Receive data count
		DWORD rxread;					//Receive read pointer
		DWORD rxwrite;					//Receive write pointer
		DWORD rxtotal;					// Receive total

		//Send buffer
		BYTE txbuf[0x1000];				//Send data
		DWORD txnum;					//Send data count
		DWORD txread;					//Send read pointer
		DWORD txwrite;					//Send write pointer
		DWORD txtotal;					// Tx total
		BOOL txwait;					//Tx wait flag
	} ch_t;

	//Internal data definition
	typedef struct {
		//Channel
		ch_t ch[2];						//Channel data

		// RR2
		DWORD request;					//Interrupt vector (requested)

		// WR2
		DWORD vbase;					//Interrupt vector (base)

		// WR9
		BOOL shsl;						//Vector change mode b4-b6/b3-b1
		BOOL mie;						//Interrupt enable
		BOOL dlc;						//Disable lower chain
		BOOL nv;						// Interrupt vector output enable
		BOOL vis;						//Interrupt vector change mode

		int ireq;						//Requested interrupt type
		int vector;						//Requested vector
	} scc_t;

public:
	//Basic functions
	SCC(VM *p);
										//Constructor
	BOOL FASTCALL Init();
										//Initialize
	void FASTCALL Cleanup();
										//Cleanup
	void FASTCALL Reset();
										//Reset
	BOOL FASTCALL Save(Fileio *fio, int ver);
										//Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										//Load
	void FASTCALL ApplyCfg(const Config *config);
										//Apply settings

	//Memory device
	DWORD FASTCALL ReadByte(DWORD addr);
										//Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										//Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										//Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										//Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										//Read only

	//External API
	void FASTCALL GetSCC(scc_t *buffer) const;
										//Get internal data
	const SCC::scc_t* FASTCALL GetWork() const;
										//Get work
	DWORD FASTCALL GetVector(int type) const;
										//Get vector
	BOOL FASTCALL Callback(Event *ev);
										//Event callback
	void FASTCALL IntAck();
										//Interrupt acknowledge

	//Send API (send to SCC)
	void FASTCALL Send(int channel, DWORD data);
										//Send data
	void FASTCALL ParityErr(int channel);
										//Generate parity error
	void FASTCALL FramingErr(int channel);
										//Generate framing error
	void FASTCALL SetBreak(int channel, BOOL flag);
										//Notify break state
	BOOL FASTCALL IsRxEnable(int channel) const;
										//Receive check
	BOOL FASTCALL IsBaudRate(int channel, DWORD baudrate) const;
										//Baud rate check
	DWORD FASTCALL GetRxBit(int channel) const;
										//Get receive data bit count
	DWORD FASTCALL GetStopBit(int channel) const;
										//Get stop bits
	DWORD FASTCALL GetParity(int channel) const;
										//Get parity
	BOOL FASTCALL IsRxBufEmpty(int channel) const;
										//Check receive buffer free

	//Receive API (receive from SCC)
	DWORD FASTCALL Receive(int channel);
										//Receive data
	BOOL FASTCALL IsTxEmpty(int channel);
										//Check send buffer empty
	BOOL FASTCALL IsTxFull(int channel);
										//Check send buffer full
	void FASTCALL WaitTx(int channel, BOOL wait);
										//Send block

	//Hard flow
	void FASTCALL SetCTS(int channel, BOOL flag);
										//Set CTS
	void FASTCALL SetDCD(int channel, BOOL flag);
										//Set DCD
	BOOL FASTCALL GetRTS(int channel);
										//Get RTS
	BOOL FASTCALL GetDTR(int channel);
										//Get DTR
	BOOL FASTCALL GetBreak(int channel);
										//Get break

private:
	void FASTCALL ResetCh(ch_t *p);
										//Channel reset
	DWORD FASTCALL ReadSCC(ch_t *p, DWORD reg);
										//Channel read
	DWORD FASTCALL ReadRR0(const ch_t *p) const;
										//Read RR0
	DWORD FASTCALL ReadRR1(const ch_t *p) const;
										//Read RR1
	DWORD FASTCALL ReadRR2(ch_t *p);
										//Read RR2
	DWORD FASTCALL ReadRR3(const ch_t *p) const;
										//Read RR3
	DWORD FASTCALL ReadRR8(ch_t *p);
										//Read RR8
	DWORD FASTCALL ReadRR15(const ch_t *p) const;
										//Read RR15
	DWORD FASTCALL ROSCC(const ch_t *p, DWORD reg) const;
										//Read only
	void FASTCALL WriteSCC(ch_t *p, DWORD reg, DWORD data);
										//Channel write
	void FASTCALL WriteWR0(ch_t *p, DWORD data);
										//Write WR0
	void FASTCALL WriteWR1(ch_t *p, DWORD data);
										//Write WR1
	void FASTCALL WriteWR3(ch_t *p, DWORD data);
										//Write WR3
	void FASTCALL WriteWR4(ch_t *p, DWORD data);
										//Write WR4
	void FASTCALL WriteWR5(ch_t *p, DWORD data);
										//Write WR5
	void FASTCALL WriteWR8(ch_t *p, DWORD data);
										//Write WR8
	void FASTCALL WriteWR9(DWORD data);
										//Write WR9
	void FASTCALL WriteWR10(ch_t *p, DWORD data);
										//Write WR10
	void FASTCALL WriteWR11(ch_t *p, DWORD data);
										//Write WR11
	void FASTCALL WriteWR12(ch_t *p, DWORD data);
										//Write WR12
	void FASTCALL WriteWR13(ch_t *p, DWORD data);
										//Write WR13
	void FASTCALL WriteWR14(ch_t *p, DWORD data);
										//Write WR14
	void FASTCALL WriteWR15(ch_t *p, DWORD data);
										//Write WR15
	void FASTCALL ResetSCC(ch_t *p);
										//Reset
	void FASTCALL ClockSCC(ch_t *p);
										//Recalculate baud rate
	void FASTCALL IntSCC(ch_t *p, itype_t type, BOOL flag);
										//Interrupt request
	void FASTCALL IntCheck();
										//Check interrupt
	void FASTCALL EventRx(ch_t *p);
										//Event (receive)
	void FASTCALL EventTx(ch_t *p);
										//Event (send)
	Mouse *mouse;
										//Mouse
	scc_t scc;
										//Internal data
	Event event[2];
										//Event
	BOOL clkup;
										//7.5MHz mode
};

#endif	// scc_h
