//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//Copyright (C) 2001-2006  PI.(ytanaka@ipc-tokai.or.jp)
//	[ SCC(Z8530) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "cpu.h"
#include "mouse.h"
#include "log.h"
#include "schedule.h"
#include "config.h"
#include "fileio.h"
#include "scc.h"

//===========================================================================
//
//	SCC
//
//===========================================================================
//#define SCC_LOG

//---------------------------------------------------------------------------
//
//Constructor
//
//---------------------------------------------------------------------------
SCC::SCC(VM *p) : MemDevice(p)
{
	//Initialize device ID
	dev.id = MAKEID('S', 'C', 'C', ' ');
	dev.desc = "SCC (Z8530)";

	//Start address, end address
	memdev.first = 0xe98000;
	memdev.last = 0xe99fff;
}

//---------------------------------------------------------------------------
//
//Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::Init()
{
	ASSERT(this);

	//Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	//Work clear (some state remains even after hardware reset)
	memset(&scc, 0, sizeof(scc));
	scc.ch[0].index = 0;
	scc.ch[1].index = 1;
	scc.ireq = -1;
	scc.vector = -1;
	clkup = FALSE;

	//Get mouse
	mouse = (Mouse*)vm->SearchDevice(MAKEID('M', 'O', 'U', 'S'));
	ASSERT(mouse);

	//Add event
	event[0].SetDevice(this);
	event[0].SetDesc("Channel-A");
	event[0].SetUser(0);
	event[0].SetTime(0);
	scheduler->AddEvent(&event[0]);
	event[1].SetDevice(this);
	event[1].SetDesc("Channel-B");
	event[1].SetUser(1);
	event[1].SetTime(0);
	scheduler->AddEvent(&event[1]);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL SCC::Cleanup()
{
	//To base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//Reset
//
//---------------------------------------------------------------------------
void FASTCALL SCC::Reset()
{
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "リセット");

	//Reset by RESET signal (different from channel reset)
	ResetSCC(&scc.ch[0]);
	ResetSCC(&scc.ch[1]);
	ResetSCC(NULL);

	//Speed
	for (i=0; i<2; i++ ){
		scc.ch[i].brgen = FALSE;
		scc.ch[i].speed = 0;
		event[i].SetTime(0);
	}
}

//---------------------------------------------------------------------------
//
//Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	LOG0(Log::Normal, "セーブ");

	//Save size
	sz = sizeof(scc_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (!fio->Write(&scc, sizeof(scc))) {
		return FALSE;
	}

	//Save event
	if (!event[0].Save(fio, ver)) {
		return FALSE;
	}
	if (!event[1].Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::Load(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	LOG0(Log::Normal, "ロード");

	//Load main
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(scc_t)) {
		return FALSE;
	}
	if (!fio->Read(&scc, sizeof(scc))) {
		return FALSE;
	}

	//Event
	if (!event[0].Load(fio, ver)) {
		return FALSE;
	}
	if (!event[1].Load(fio, ver)) {
		return FALSE;
	}

	//Recalculate speed
	ClockSCC(&scc.ch[0]);
	ClockSCC(&scc.ch[1]);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL SCC::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	LOG0(Log::Normal, "設定適用");

	//SCC clock
	if (clkup != config->scc_clkup) {
		//Different, so set it
		clkup = config->scc_clkup;

		//Recalculate speed
		ClockSCC(&scc.ch[0]);
		ClockSCC(&scc.ch[1]);
	}
}

//---------------------------------------------------------------------------
//
//Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ReadByte(DWORD addr)
{
	static const DWORD table[] = {
		0, 1, 2, 3, 0, 1, 2, 3, 8, 13, 10, 15, 12, 13, 10, 15
	};
	DWORD reg;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	//Only odd addresses are decoded
	if ((addr & 1) != 0) {
		//Loop in 8-byte units
		addr &= 7;
		addr >>= 1;

		//Wait
		scheduler->Wait(3);

		//Register distribution
		switch (addr) {
			//Channel B command port
			case 0:
				//Determine register
				ASSERT(scc.ch[1].reg <= 7);
				if (scc.ch[1].ph) {
					reg = table[scc.ch[1].reg + 8];
				}
				else {
					reg = table[scc.ch[1].reg];
				}

				//Select & high point
				scc.ch[1].reg = 0;
				scc.ch[1].ph = FALSE;

				//Data read
				return (BYTE)ReadSCC(&scc.ch[1], reg);

			//Channel B data port
			case 1:
				return (BYTE)ReadRR8(&scc.ch[1]);

			//Channel A command port
			case 2:
				//Determine register
				ASSERT(scc.ch[0].reg <= 7);
				if (scc.ch[0].ph) {
					reg = table[scc.ch[0].reg + 8];
				}
				else {
					reg = table[scc.ch[0].reg];
				}

				//Select & high point
				scc.ch[0].reg = 0;
				scc.ch[0].ph = FALSE;

				//Data read
				return (BYTE)ReadSCC(&scc.ch[0], reg);

			//Channel A data port
			case 3:
				return (BYTE)ReadRR8(&scc.ch[0]);
		}

		//Never comes here
		ASSERT(FALSE);
		return 0xff;
	}

	//Even addresses return $FF
	return 0xff;
}

//---------------------------------------------------------------------------
//
//Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	return (WORD)(0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//Byte write
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteByte(DWORD addr, DWORD data)
{
	DWORD reg;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	//Only odd addresses are decoded
	if ((addr & 1) != 0) {
		//Loop in 8-byte units
		addr &= 7;
		addr >>= 1;

		//Wait
		scheduler->Wait(3);

		//Register distribution
		switch (addr) {
			//Channel B command port
			case 0:
				//Determine register
				ASSERT(scc.ch[1].reg <= 7);
				reg = scc.ch[1].reg;
				if (scc.ch[1].ph) {
					reg += 8;
				}

				//Select & high point
				scc.ch[1].reg = 0;
				scc.ch[1].ph = FALSE;

				//Data write
				WriteSCC(&scc.ch[1], reg, (DWORD)data);
				return;

			//Channel B data port
			case 1:
				WriteWR8(&scc.ch[1], (DWORD)data);
				return;

			//Channel A command port
			case 2:
				//Determine register
				ASSERT(scc.ch[0].reg <= 7);
				reg = scc.ch[0].reg;
				if (scc.ch[0].ph) {
					reg += 8;
				}

				//Select & high point
				scc.ch[0].reg = 0;
				scc.ch[0].ph = FALSE;

				//Data write
				WriteSCC(&scc.ch[0], reg, (DWORD)data);
				return;

			//Channel A data port
			case 3:
				WriteWR8(&scc.ch[0], (DWORD)data);
				return;
		}

		//Never comes here
		ASSERT(FALSE);
		return;
	}

	//Even addresses are not decoded
}

//---------------------------------------------------------------------------
//
//Word write
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ReadOnly(DWORD addr) const
{
	static const DWORD table[] = {
		0, 1, 2, 3, 0, 1, 2, 3, 8, 13, 10, 15, 12, 13, 10, 15
	};
	DWORD reg;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	//Only odd addresses are decoded
	if ((addr & 1) != 0) {
		//Loop in 8-byte units
		addr &= 7;
		addr >>= 1;

		//Register distribution
		switch (addr) {
			//Channel B command port
			case 0:
				//Determine register
				ASSERT(scc.ch[1].reg <= 7);
				if (scc.ch[1].ph) {
					reg = table[scc.ch[1].reg + 8];
				}
				else {
					reg = table[scc.ch[1].reg];
				}

				//Data read
				return (BYTE)ROSCC(&scc.ch[1], reg);

			//Channel B data port
			case 1:
				return (BYTE)ROSCC(&scc.ch[1], 8);

			//Channel A command port
			case 2:
				//Determine register
				ASSERT(scc.ch[0].reg <= 7);
				if (scc.ch[0].ph) {
					reg = table[scc.ch[0].reg + 8];
				}
				else {
					reg = table[scc.ch[0].reg];
				}

				//Data read
				return (BYTE)ROSCC(&scc.ch[0], reg);

			//Channel A data port
			case 3:
				return (BYTE)ROSCC(&scc.ch[0], 8);
		}

		//Never comes here
		ASSERT(FALSE);
		return 0xff;
	}

	//Even addresses return $FF
	return 0xff;
}

//---------------------------------------------------------------------------
//
//Get internal data
//
//---------------------------------------------------------------------------
void FASTCALL SCC::GetSCC(scc_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	//Copy internal work
	*buffer = scc;
}

//---------------------------------------------------------------------------
//
//Get internal work
//
//---------------------------------------------------------------------------
const SCC::scc_t* FASTCALL SCC::GetWork() const
{
	ASSERT(this);

	//Return internal work
	return (const scc_t*)&scc;
}

//---------------------------------------------------------------------------
//
//Get vector
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::GetVector(int type) const
{
	DWORD vector;

	ASSERT(this);
	ASSERT((type >= 0) && (type < 8));

	//If NV=1, fixed at 0
	if (scc.nv) {
		return 0;
	}

	//Set base vector
	vector = scc.vbase;

	//bit3-1 or bit6-4
	if (scc.vis) {
		if (scc.shsl) {
			// bit6-4
			vector &= 0x8f;
			vector |= (type << 4);
		}
		else {
			// bit3-1
			vector &= 0xf1;
			vector |= (type << 1);
		}
	}

	return vector;
}

//---------------------------------------------------------------------------
//
//Channel reset
//
//---------------------------------------------------------------------------
void FASTCALL SCC::ResetSCC(ch_t *p)
{
	int i;

	ASSERT(this);

	//NULL triggers hardware reset
	if (p == NULL) {
#if defined(SCC_LOG)
		LOG0(Log::Normal, "ハードウェアリセット");
#endif	// SCC_LOG

		// WR9
		scc.shsl = FALSE;
		scc.mie = FALSE;
		scc.dlc = FALSE;

		// WR11
		return;
	}

#if defined(SCC_LOG)
	LOG1(Log::Normal, "チャネル%d リセット", p->index);
#endif	// SCC_LOG

	// RR0
	p->tu = TRUE;
	p->zc = FALSE;

	// WR0
	p->reg = 0;
	p->ph = FALSE;
	p->rxno = TRUE;
	p->txpend = FALSE;

	// RR1
	p->framing = FALSE;
	p->overrun = FALSE;
	p->parerr = FALSE;

	// WR1
	p->rxim = 0;
	p->txie = FALSE;
	p->extie = FALSE;

	// RR3
	for (i=0; i<2; i++) {
		scc.ch[i].rsip = FALSE;
		scc.ch[i].rxip = FALSE;
		scc.ch[i].txip = FALSE;
		scc.ch[i].extip = FALSE;
	}

	// WR3
	p->rxen = FALSE;

	// WR4
	p->stopbit |= 0x01;

	// WR5
	p->dtr = FALSE;
	p->brk = FALSE;
	p->txen = FALSE;
	p->rts = FALSE;
	if (p->index == 1) {
		mouse->MSCtrl(!p->rts, 1);
	}

	// WR14
	p->loopback = FALSE;
	p->aecho = FALSE;
	p->dtrreq = FALSE;

	// WR15
	p->baie = TRUE;
	p->tuie = TRUE;
	p->ctsie = TRUE;
	p->syncie = TRUE;
	p->dcdie = TRUE;
	p->zcie = FALSE;

	//Receive FIFO
	p->rxfifo = 0;

	//Receive buffer
	p->rxnum = 0;
	p->rxread = 0;
	p->rxwrite = 0;
	p->rxtotal = 0;

	//Send buffer
	p->txnum = 0;
	p->txread = 0;
	p->txwrite = 0;
	p->tdf = FALSE;
	p->txtotal = 0;
	p->txwait = FALSE;

	//Interrupt
	IntCheck();
}

//---------------------------------------------------------------------------
//
//Channel read
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ReadSCC(ch_t *p, DWORD reg)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));


#if defined(SCC_LOG)
	LOG2(Log::Normal, "チャネル%d読み出し R%d", p->index, reg);
#endif	// SCC_LOG

	switch (reg) {
		//RR0 - Extended status
		case 0:
			return ReadRR0(p);

		//RR1 - Special Rx condition
		case 1:
			return ReadRR1(p);

		//RR2 - Vector
		case 2:
			return ReadRR2(p);

		//RR3 - Interrupt pending
		case 3:
			return ReadRR3(p);

		//RR8 - Receive data
		case 8:
			return ReadRR8(p);

		//RR10 - SDLC loop mode
		case 10:
			return 0;

		//RR12 - Baud rate low
		case 12:
			return p->tc;

		//RR13 - Baud rate high
		case 13:
			return (p->tc >> 8);

		//RR15 - External status interrupt control
		case 15:
			return ReadRR15(p);
	}

#if defined(SCC_LOG)
	LOG1(Log::Normal, "未実装レジスタ読み出し R%d", reg);
#endif	// SCC_LOG

	return 0xff;
}

//---------------------------------------------------------------------------
//
//Read RR0
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ReadRR0(const ch_t *p) const
{
	DWORD data;

	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));

	//Initialize
	data = 0;

	//bit7: Break/Abort status
	if (p->ba) {
		data |= 0x80;
	}

	//bit6: Send underrun status
	if (p->tu) {
		data |= 0x40;
	}

	//bit5: CTS status
	if (p->cts) {
		data |= 0x20;
	}

	//bit5: Sync/Hunt status
	if (p->sync) {
		data |= 0x10;
	}

	//bit3: DCD status
	if (p->dcd) {
		data |= 0x08;
	}

	//bit2: Send buffer empty status
	if (!p->txwait) {
		//Always buffer full when waiting to send
		if (!p->tdf) {
			data |= 0x04;
		}
	}

	//bit1: Zero count status
	if (p->zc) {
		data |= 0x02;
	}

	//bit0: Receive buffer valid status
	if (p->rxfifo > 0) {
		data |= 0x01;
	}

	return data;
}

//---------------------------------------------------------------------------
//
//Read RR1
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ReadRR1(const ch_t *p) const
{
	DWORD data;

	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));

	//Initialize
	data = 0x06;

	//bit6: Framing error
	if (p->framing) {
		data |= 0x40;
	}

	//bit5: Overrun error
	if (p->overrun) {
		data |= 0x20;
	}

	//bit4: Parity error
	if (p->parerr) {
		data |= 0x10;
	}

	//bit0: Send complete
	if (p->txsent) {
		data |= 0x01;
	}

	return data;
}

//---------------------------------------------------------------------------
//
//Read RR2
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ReadRR2(ch_t *p)
{
	DWORD data;

	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));

	//Channel identification
	if (p->index == 0) {
		//Channel A returns WR2 content as-is
		data = scc.vbase;
	}
	else {
		//Channel B returns currently requested interrupt vector
		data = scc.request;
	}

	//Do nothing if no interrupt requested
	if (scc.ireq < 0) {
		return data;
	}

	//Move to next interrupt here (Mac emulator)
	ASSERT((scc.ireq >= 0) && (scc.ireq <= 7));
	if (scc.ireq >= 4) {
		p = &scc.ch[1];
	}
	else {
		p = &scc.ch[0];
	}
	switch (scc.ireq & 3) {
		// Rs
		case 0:
			p->rsip = FALSE;
			break;
		// Rx
		case 1:
			p->rxip = FALSE;
			break;
		// Ext
		case 2:
			p->extip = FALSE;
			break;
		// Tx
		case 3:
			p->txip = FALSE;
			break;
	}
	scc.ireq = -1;

	//Check interrupt
	IntCheck();

	//Return data
	return data;
}

//---------------------------------------------------------------------------
//
//Read RR3
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ReadRR3(const ch_t *p) const
{
	int i;
	DWORD data;

	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));

	//Only valid for Channel A. Channel B returns 0
	if (p->index == 1) {
		return 0;
	}

	//Clear
	data = 0;

	//From bit0: ext,tx,rx, in order Channel B -> Channel A
	for (i=0; i<2; i++) {
		data <<= 3;

		//Set all valid (requested) interrupt bits
		if (scc.ch[i].extip) {
			data |= 0x01;
		}
		if (scc.ch[i].txip) {
			data |= 0x02;
		}
		if (scc.ch[i].rxip) {
			data |= 0x04;
		}
		if (scc.ch[i].rsip) {
			data |= 0x04;
		}
	}

	return data;
}

//---------------------------------------------------------------------------
//
//Read RR8
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ReadRR8(ch_t *p)
{
	DWORD data;

	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));

	//Is there receive data
	if (p->rxfifo == 0) {
		if (p->index == 0) {
			LOG1(Log::Warning, "チャネル%d 受信データ空読み", p->index);
		}
		return 0;
	}

	//Data is always at the tail
#if defined(SCC_LOG)
	LOG2(Log::Normal, "チャネル%d データ引き取り$%02X", p->index, p->rxdata[2]);
#endif
	data = p->rxdata[2];

	//Move receive FIFO
	p->rxdata[2] = p->rxdata[1];
	p->rxdata[1] = p->rxdata[0];

	//Decrement count
	p->rxfifo--;

	//Drop interrupt first. If FIFO has more, next event
	IntSCC(p, rxi, FALSE);

#if defined(SCC_LOG)
	LOG2(Log::Normal, "残りFIFO=%d 残りRxNum=%d", p->rxfifo, p->rxnum);
#endif

	return data;
}

//---------------------------------------------------------------------------
//
//Read RR15
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ReadRR15(const ch_t *p) const
{
	DWORD data;

	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));

	//Initialize data
	data = 0;

	//bit7 - Break/Abort interrupt
	if (p->baie) {
		data |= 0x80;
	}

	//bit6 - Tx underrun interrupt
	if (p->tuie) {
		data |= 0x40;
	}

	//bit5 - CTS interrupt
	if (p->ctsie) {
		data |= 0x20;
	}

	//bit4 - SYNC interrupt
	if (p->syncie) {
		data |= 0x10;
	}

	//bit3 - DCD interrupt
	if (p->dcdie) {
		data |= 0x08;
	}

	//bit1 - Zero count interrupt
	if (p->zcie) {
		data |= 0x02;
	}

	return data;
}

//---------------------------------------------------------------------------
//
//Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::ROSCC(const ch_t *p, DWORD reg) const
{
	DWORD data;

	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));

	switch (reg) {
		//RR0 - Extended status
		case 0:
			return ReadRR0(p);

		//RR1 - Special Rx condition
		case 1:
			return ReadRR1(p);

		//RR2 - Vector
		case 2:
			if (p->index == 0) {
				return scc.vbase;
			}
			return scc.request;

		//RR3 - Interrupt pending
		case 3:
			return ReadRR3(p);

		//RR8 - Receive data
		case 8:
			break;

		//RR10 - SDLC loop mode
		case 10:
			return 0;

		//RR12 - Baud rate low
		case 12:
			return p->tc;

		//RR13 - Baud rate high
		case 13:
			return (p->tc >> 8);

		//RR15 - External status interrupt control
		case 15:
			return ReadRR15(p);
	}

	//Initialize data
	data = 0xff;

	//If receive FIFO is valid, return
	if (p->rxfifo > 0) {
		data = p->rxdata[2];
	}

	return data;
}

//---------------------------------------------------------------------------
//
//Channel write
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteSCC(ch_t *p, DWORD reg, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

#if defined(SCC_LOG)
	LOG3(Log::Normal, "チャネル%d書き込み R%d <- $%02X", p->index, reg, data);
#endif	// SCC_LOG

	switch (reg) {
		//WR0 - Global control
		case 0:
			WriteWR0(p, data);
			return;

		//WR1 - Interrupt enable
		case 1:
			WriteWR1(p, data);
			return;

		//WR2 - Vector base
		case 2:
			LOG1(Log::Detail, "割り込みベクタベース $%02X", data);
			scc.vbase = data;
			return;

		//WR3 - Receive control
		case 3:
			WriteWR3(p, data);
			return;

		//WR4 - Communication mode setting
		case 4:
			WriteWR4(p, data);
			return;

		//WR5 - Send control
		case 5:
			WriteWR5(p, data);
			return;

		//WR8 - Send buffer
		case 8:
			WriteWR8(p, data);
			return;

		//WR9 - Interrupt control
		case 9:
			WriteWR9(data);
			return;

		//WR10 - SDLC control
		case 10:
			WriteWR10(p, data);
			return;

		//WR11 - Clock/terminal select
		case 11:
			WriteWR11(p, data);

		//WR12 - Baud rate low
		case 12:
			WriteWR12(p, data);
			return;

		//WR13 - Baud rate high
		case 13:
			WriteWR13(p, data);
			return;

		//WR14 - Clock control
		case 14:
			WriteWR14(p, data);
			return;

		//WR15 - Extended status interrupt control
		case 15:
			WriteWR15(p, data);
			return;
	}

	LOG3(Log::Warning, "チャネル%d未実装レジスタ書き込み R%d <- $%02X",
								p->index, reg, data);
}

//---------------------------------------------------------------------------
//
//Write WR0
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR0(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//bit2-0: Access register
	p->reg = (data & 0x07);

	//bit5-3: SCC command
	data >>= 3;
	data &= 0x07;
	switch (data) {
		//000: Null code
		case 0:
			break;

		//001: Select upper register
		case 1:
			p->ph = TRUE;
			break;

		//010: Reset external status
		case 2:
#if defined(SCC_LOG)
			LOG1(Log::Normal, "チャネル%d 外部ステータスリセット", p->index);
#endif	// SCC_LOG
			break;

		//100: Reset receive data
		case 4:
#if defined(SCC_LOG)
			LOG1(Log::Normal, "チャネル%d 受信データリセット", p->index);
#endif	// SCC_LOG
			p->rxno = TRUE;
			break;

		//101: Reset send interrupt pending
		case 5:
#if defined(SCC_LOG)
			LOG1(Log::Normal, "チャネル%d 送信割り込みペンディングリセット", p->index);
#endif	// SCC_LOG
			p->txpend = TRUE;
			IntSCC(p, txi, FALSE);

		//110: Reset receive error
		case 6:
#if defined(SCC_LOG)
			LOG1(Log::Normal, "チャネル%d 受信エラーリセット", p->index);
#endif	// SCC_LOG
			p->framing = FALSE;
			p->overrun = FALSE;
			p->parerr = FALSE;
			IntSCC(p, rsi, FALSE);
			break;

		//111: Reset highest in-service
		case 7:
			break;

		//Other
		default:
			LOG2(Log::Warning, "チャネル%d SCC未サポートコマンド $%02X",
								p->index, data);
			break;
	}
}

//---------------------------------------------------------------------------
//
//Write WR1
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR1(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//bit4-3: Rx interrupt mode
	p->rxim = (data >> 3) & 0x03;

	//bit2: Parity error to special Rx condition interrupt
	if (data & 0x04) {
		p->parsp = TRUE;
	}
	else {
		p->parsp = FALSE;
	}

	//bit1: Send interrupt enable
	if (data & 0x02) {
		p->txie = TRUE;
	}
	else {
		p->txie = FALSE;
	}

	//bit0: External status interrupt enable
	if (data & 0x01) {
		p->extie = TRUE;
	}
	else {
		p->extie = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//Write WR3
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR3(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//bit7-6: Receive character length
	p->rxbit = ((data & 0xc0) >> 6) + 5;
	ASSERT((p->rxbit >= 5) && (p->rxbit <= 8));

	//bit5: Auto enable
	if (data & 0x20) {
		p->aen = TRUE;
	}
	else {
		p->aen = FALSE;
	}

	//bit0: Receive enable
	if (data & 0x01) {
		if (!p->rxen) {
			//Clear error state
			p->framing = FALSE;
			p->overrun = FALSE;
			p->parerr = FALSE;
			IntSCC(p, rsi, FALSE);

			//Clear receive buffer
			p->rxnum = 0;
			p->rxread = 0;
			p->rxwrite = 0;
			p->rxfifo = 0;

			//Receive enable
			p->rxen = TRUE;
		}
	}
	else {
		p->rxen = FALSE;
	}

	//Recalculate baud rate
	ClockSCC(p);
}

//---------------------------------------------------------------------------
//
//Write WR4
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR4(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//bit7-6: Clock mode
	switch ((data & 0xc0) >> 6) {
		//x1 clock mode
		case 0:
			p->clkm = 1;
			break;
		//x16 clock mode
		case 1:
			p->clkm = 16;
			break;
		//x32 clock mode
		case 2:
			p->clkm = 32;
			break;
		//x64 clock mode
		case 3:
			p->clkm = 64;
			break;
		//Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}

	//bit3-2: Stop bits
	p->stopbit = (data & 0x0c) >> 2;
	if (p->stopbit == 0) {
		LOG1(Log::Warning, "チャネル%d 同期モード", p->index);
	}

	//bit1-0: Parity
	if (data & 1) {
		//1: Odd parity, 2: Even parity
		p->parity = ((data & 2) >> 1) + 1;
	}
	else {
		//0: No parity
		p->parity = 0;
	}

	//Recalculate baud rate
	ClockSCC(p);
}

//---------------------------------------------------------------------------
//
//Write WR5
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR5(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//bit7: DTR control (Low active, also see p->dtrreq)
	if (data & 0x80) {
		p->dtr = TRUE;
	}
	else {
		p->dtr = FALSE;
	}

	//bit6-5: Send character bit length
	p->txbit = ((data & 0x60) >> 5) + 5;
	ASSERT((p->txbit >= 5) && (p->txbit <= 8));

	//bit4: Send break signal
	if (data & 0x10) {
#if defined(SCC_LOG)
		LOG1(Log::Normal, "チャネル%d ブレーク信号ON", p->index);
#endif	// SCC_LOG
		p->brk = TRUE;
	}
	else {
#if defined(SCC_LOG)
		LOG1(Log::Normal, "チャネル%d ブレーク信号OFF", p->index);
#endif	// SCC_LOG
		p->brk = FALSE;
	}

	//bit3: Send enable
	if (data & 0x08) {
		if (!p->txen) {
			//Clear send buffer
			p->txnum = 0;
			if (!p->txpend) {
				IntSCC(p, txi, TRUE);
			}

			//Clear send work
			p->txsent = FALSE;
			p->tdf = FALSE;
			p->txwait = FALSE;

			//Send enable
			p->txen = TRUE;
		}
	}
	else {
		p->txen = FALSE;
	}

	//bit1: RTS control
	if (data & 0x02) {
#if defined(SCC_LOG)
		LOG1(Log::Normal, "チャネル%d RTS信号ON", p->index);
#endif	// SCC_LOG
		p->rts = TRUE;
	}
	else {
#if defined(SCC_LOG)
		LOG1(Log::Normal, "チャネル%d RTS信号OFF", p->index);
#endif	// SCC_LOG
		p->rts = FALSE;
	}

	//If Channel B, tell mouse about MSCTRL
	if (p->index == 1) {
		//RTS is Low active, followed by inverter (see circuit diagram)
		mouse->MSCtrl(!(p->rts), 1);
	}
}

//---------------------------------------------------------------------------
//
//Write WR8
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR8(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//Remember send data, send data exists
	p->tdr = data;
	p->tdf = TRUE;

	//Cancel send interrupt
	IntSCC(p, txi, FALSE);

	//Auto off send interrupt pending
	p->txpend = FALSE;
}

//---------------------------------------------------------------------------
//
//Write WR9
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR9(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	//bit7-6: Reset
	switch ((data & 0xc0) >> 3) {
		case 1:
			ResetSCC(&scc.ch[0]);
			return;
		case 2:
			ResetSCC(&scc.ch[1]);
			return;
		case 3:
			ResetSCC(&scc.ch[0]);
			ResetSCC(&scc.ch[1]);
			ResetSCC(NULL);
			return;
		default:
			break;
	}

	//bit4: Interrupt change mode select
	if (data & 0x10) {
#if defined(SCC_LOG)
		LOG0(Log::Warning, "割り込みベクタ変化モード bit4-bit6");
#endif	// SCC_LOG
		scc.shsl = TRUE;
	}
	else {
#if defined(SCC_LOG)
		LOG0(Log::Normal, "割り込みベクタ変化モード bit3-bit1");
#endif	// SCC_LOG
		scc.shsl = FALSE;
	}

	//bit3: Interrupt enable
	if (data & 0x08) {
#if defined(SCC_LOG)
		LOG0(Log::Normal, "割り込みイネーブル");
#endif	// SCC_LOG
		scc.mie = TRUE;
	}
	else {
#if defined(SCC_LOG)
		LOG0(Log::Warning, "割り込みディセーブル");
#endif	// SCC_LOG
		scc.mie = FALSE;
	}

	//bit2: Disable digit chain
	if (data & 0x04) {
		scc.dlc = TRUE;
	}
	else {
		scc.dlc = FALSE;
	}

	//bit1: No interrupt vector response
	if (data & 0x02) {
		scc.nv = TRUE;
#if defined(SCC_LOG)
		LOG0(Log::Warning, "割り込みアクノリッジなし");
#endif	// SCC_LOG
	}
	else {
#if defined(SCC_LOG)
		LOG0(Log::Normal, "割り込みアクノリッジあり");
#endif	// SCC_LOG
		scc.nv = FALSE;
	}

	//bit0: Interrupt vector variable
	if (data & 0x01) {
		scc.vis = TRUE;
#if defined(SCC_LOG)
		LOG0(Log::Normal, "割り込みベクタ変化");
#endif	// SCC_LOG
	}
	else {
		scc.vis = FALSE;
#if defined(SCC_LOG)
		LOG0(Log::Warning, "割り込みベクタ固定");
#endif	// SCC_LOG
	}

	//Check interrupt
	IntCheck();
}

//---------------------------------------------------------------------------
//
//Write WR10
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR10(ch_t* /*p*/, DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	data &= 0x60;
	if (data != 0) {
		LOG0(Log::Warning, "NRZ以外の変調モード");
	}
}

//---------------------------------------------------------------------------
//
//Write WR11
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR11(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//bit7: RTxC crystal present/absent
	if (data & 0x80) {
		LOG1(Log::Warning, "チャネル%d bRTxCとbSYNCでクロック作成", p->index);
	}

	//bit6-5: Receive clock source select
	if ((data & 0x60) != 0x40) {
		LOG1(Log::Warning, "チャネル%d ボーレートジェネレータ出力を使わない", p->index);
	}
}

//---------------------------------------------------------------------------
//
//Write WR12
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR12(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//Lower 8 bits only
	p->tc &= 0xff00;
	p->tc |= data;

	//Recalculate baud rate
	ClockSCC(p);
}

//---------------------------------------------------------------------------
//
//Write WR13
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR13(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//Upper 8 bits only
	p->tc &= 0x00ff;
	p->tc |= (data << 8);

	//Recalculate baud rate
	ClockSCC(p);
}

//---------------------------------------------------------------------------
//
//Write WR14
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR14(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//bit4: Local loopback
	if (data & 0x10) {
#if defined(SCC_LOG)
		LOG1(Log::Normal, "チャネル%d ローカルループバックモード", p->index);
#endif	// SCC_LOG
		p->loopback = TRUE;
	}
	else {
		p->loopback = FALSE;
	}

	//bit3: Auto echo
	if (data & 0x08) {
#if defined(SCC_LOG)
		LOG1(Log::Normal, "チャネル%d オートエコーモード", p->index);
#endif	// SCC_LOG
		p->aecho = TRUE;
	}
	else {
		p->aecho = FALSE;
	}

	// bit2 : DTR/REQbit2: DTR/REQ select
	if (data & 0x04) {
		p->dtrreq = TRUE;
	}
	else {
		p->dtrreq = FALSE;
	}

	//bit1: Baud rate generator clock source
	if (data & 0x02) {
		p->brgsrc = TRUE;
	}
	else {
		p->brgsrc = FALSE;
	}

	//bit0: Baud rate generator enable
	if (data & 0x01) {
		p->brgen = TRUE;
	}
	else {
		p->brgen = FALSE;
	}

	//Recalculate baud rate
	ClockSCC(p);
}

//---------------------------------------------------------------------------
//
//Recalculate baud rate
//
//---------------------------------------------------------------------------
void FASTCALL SCC::ClockSCC(ch_t *p)
{
	DWORD len;
	DWORD speed;

	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));

	//If baud rate generator not valid, stop event
	if (!p->brgsrc || !p->brgen) {
		event[p->index].SetTime(0);
		p->speed = 0;
		return;
	}

	//Reference clock
	if (clkup) {
		p->baudrate = 3750000;
	}
	else {
		p->baudrate = 2500000;
	}

	//Calculate baud rate from setting and clock multiplier
	speed = (p->tc + 2);
	speed *= p->clkm;
	p->baudrate /= speed;

	//Calculate 2-byte length from data bits, parity, stop bits
	len = p->rxbit + 1;
	if (p->parity != 0) {
		len++;
	}
	len <<= 1;
	switch (p->stopbit) {
		//No stop bits
		case 0:
			break;
		//1 stop bit
		case 1:
			len += 2;
			break;
		//1.5 stop bits
		case 2:
			len += 3;
		//2 stop bits
		case 3:
			len += 4;
	}

	//Set to CPS
	if (clkup) {
		p->cps = 3750000;
	}
	else {
		p->cps = 2500000;
	}
	p->cps /= ((len * speed) >> 1);

	//Final speed calculation
	p->speed = 100;
	p->speed *= (len * speed);
	if (clkup) {
		p->speed /= 375;
	}
	else {
		p->speed /= 250;
	}

	//Settings
	if (event[p->index].GetTime() == 0) {
		event[p->index].SetTime(p->speed);
	}
}

//---------------------------------------------------------------------------
//
//Write WR15
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WriteWR15(ch_t *p, DWORD data)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(data < 0x100);

	//bit7 - Break/Abort interrupt
	if (data & 0x80) {
		p->baie = TRUE;
	}
	else {
		p->baie = FALSE;
	}

	//bit6 - Tx underrun interrupt
	if (data & 0x40) {
		p->tuie = TRUE;
	}
	else {
		p->tuie = FALSE;
	}

	//bit5 - CTS interrupt
	if (data & 0x20) {
		p->ctsie = TRUE;
	}
	else {
		p->ctsie = FALSE;
	}

	//bit4 - SYNC interrupt
	if (data & 0x10) {
		p->syncie = TRUE;
	}
	else {
		p->syncie = FALSE;
	}

	//bit3 - DCD interrupt
	if (data & 0x08) {
		p->dcdie = TRUE;
	}
	else {
		p->dcdie = FALSE;
	}

	//bit1 - Zero count interrupt
	if (data & 0x02) {
		p->zcie = TRUE;
	}
	else {
		p->zcie = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//Interrupt request
//
//---------------------------------------------------------------------------
void FASTCALL SCC::IntSCC(ch_t *p, itype_t type, BOOL flag)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));

	//If interrupt valid, raise pending bit
	switch (type) {
		//Receive interrupt
		case rxi:
			p->rxip = FALSE;
			if (flag) {
				if ((p->rxim == 1) || (p->rxim == 2)) {
					p->rxip = TRUE;
				}
			}
			break;

		//Special Rx condition interrupt
		case rsi:
			p->rsip = FALSE;
			if (flag) {
				if (p->rxim >= 2) {
					p->rsip = TRUE;
				}
			}
			break;

		//Send interrupt
		case txi:
			p->txip = FALSE;
			if (flag) {
				if (p->txie) {
					p->txip = TRUE;
				}
			}
			break;

		//External status change interrupt
		case exti:
			p->extip = FALSE;
			if (flag) {
				if (p->extie) {
					p->extip = TRUE;
				}
			}
			break;

		//Other (impossible)
		default:
			ASSERT(FALSE);
			break;
	}

	//Check interrupt
	IntCheck();
}

//---------------------------------------------------------------------------
//
//Check interrupt
//
//---------------------------------------------------------------------------
void FASTCALL SCC::IntCheck()
{
	int i;
	DWORD v;

	ASSERT(this);

	//Clear requested interrupt
	scc.ireq = -1;

	//Master interrupt enable
	if (scc.mie) {

		//Channel A -> Channel B order
		for (i=0; i<2; i++) {
			//Check pending bits in Rs,Rx,Tx,Ext order
			if (scc.ch[i].rsip) {
				scc.ireq = (i << 2) + 0;
				break;
			}
			if (scc.ch[i].rxip) {
				scc.ireq = (i << 2) + 1;
				break;
			}
			if (scc.ch[i].txip) {
				scc.ireq = (i << 2) + 3;
				break;
			}
			if (scc.ch[i].extip) {
				scc.ireq = (i << 2) + 2;
				break;
			}
		}

		//Create interrupt vector based on type
		if (scc.ireq >= 0) {
			//Get base vector
			v = scc.vbase;

			//bit3-1 or bit6-4
			if (scc.shsl) {
				// bit6-4
				v &= 0x8f;
				v |= ((7 - scc.ireq) << 4);
			}
			else {
				// bit3-1
				v &= 0xf1;
				v |= ((7 - scc.ireq) << 1);
			}

			//Store interrupt vector
			scc.request = v;

			//Vector readable from RR2 always changes regardless of WR9 VIS
			//Differs from data sheet description (Mac emulator)
			if (!scc.vis) {
				v = scc.vbase;
			}

			//Interrupt request if NV (No Vector) not raised
			if (!scc.nv) {
				//OK if already requested
				if (scc.vector == (int)v) {
					return;
				}

				//Cancel if different vector requested
				if (scc.vector >= 0) {
					cpu->IntCancel(5);
				}

				//Interrupt request
#if defined(SCC_LOG)
				LOG2(Log::Normal, "割り込み要求 リクエスト$%02X ベクタ$%02X ", scc.request, v);
#endif	// SCC_LOG
				cpu->Interrupt(5, (BYTE)v);
				scc.vector = (int)v;
			}
			return;
		}
	}

	//Generate vector without interrupt request
	scc.request = scc.vbase;

	//bit3-1 or bit6-4
	if (scc.shsl) {
		// bit6-4
		scc.request &= 0x8f;
		scc.request |= 0x60;
	}
	else {
		// bit3-1
		scc.request &= 0xf1;
		scc.request |= 0x06;
	}

	//Cancel interrupt if already requested
	if (scc.vector >= 0) {
		cpu->IntCancel(5);
		scc.vector = -1;
	}
}

//---------------------------------------------------------------------------
//
//Interrupt acknowledge
//
//---------------------------------------------------------------------------
void FASTCALL SCC::IntAck()
{
	ASSERT(this);

	//Sometimes CPU incorrectly generates interrupt right after reset
	if (scc.vector < 0) {
		LOG0(Log::Warning, "要求していない割り込み");
		return;
	}

#if defined(SCC_LOG)
	LOG1(Log::Normal, "割り込み応答 ベクタ$%02X", scc.vector);
#endif

	//No vector requested
	scc.vector = -1;

	//Release interrupt here. If rxi interrupt continues, next
	//event (Nostalgia 1907, Demon Castle Dracula)
}

//---------------------------------------------------------------------------
//
//Send data
//
//---------------------------------------------------------------------------
void FASTCALL SCC::Send(int channel, DWORD data)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Limit to BYTE (comes as signed int from mouse)
	data &= 0xff;

#if defined(SCC_LOG)
	LOG2(Log::Normal, "チャネル%d データ送信$%02X", channel, data);
#endif	// SCC_LOG

	//Set pointer
	p = &scc.ch[channel];

	//Insert into receive buffer
	p->rxbuf[p->rxwrite] = (BYTE)data;
	p->rxwrite = (p->rxwrite + 1) & (sizeof(p->rxbuf) - 1);
	p->rxnum++;
	if (p->rxnum >= sizeof(p->rxbuf)) {
		LOG1(Log::Warning, "チャネル%d 受信バッファオーバーフロー", p->index);
		p->rxnum = sizeof(p->rxbuf);
		p->rxread = p->rxwrite;
	}
}

//----------------------------------------------------------------------------
//
//Parity error
//
//---------------------------------------------------------------------------
void FASTCALL SCC::ParityErr(int channel)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	if (!p->parerr) {
		LOG1(Log::Normal, "チャネル%d パリティエラー", p->index);
	}

	//Raise flag
	p->parerr = TRUE;

	//Interrupt
	if (p->parsp) {
		if (p->rxim >= 2) {
			IntSCC(p, rsi, TRUE);
		}
	}
}

//---------------------------------------------------------------------------
//
//Framing error
//
//---------------------------------------------------------------------------
void FASTCALL SCC::FramingErr(int channel)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	if (!p->framing) {
		LOG1(Log::Normal, "チャネル%d フレーミングエラー", p->index);
	}

	//Raise flag
	p->framing = TRUE;

	//Interrupt
	if (p->rxim >= 2) {
		IntSCC(p, rsi, TRUE);
	}
}

//---------------------------------------------------------------------------
//
//Is receive enabled
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::IsRxEnable(int channel) const
{
	const ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	return p->rxen;
}

//---------------------------------------------------------------------------
//
//Is receive buffer empty
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::IsRxBufEmpty(int channel) const
{
	const ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	if (p->rxnum == 0) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//Baud rate check
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::IsBaudRate(int channel, DWORD baudrate) const
{
	const ch_t *p;
	DWORD offset;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));
	ASSERT(baudrate >= 75);

	//Set pointer
	p = &scc.ch[channel];

	//Calculate 5% of given baud rate
	offset = baudrate * 5;
	offset /= 100;

	//OK if within range
	if (p->baudrate < (baudrate - offset)) {
		return FALSE;
	}
	if ((baudrate + offset) < p->baudrate) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//Get receive data bit length
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::GetRxBit(int channel) const
{
	const ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	return p->rxbit;
}

//---------------------------------------------------------------------------
//
//Get stop bit length
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::GetStopBit(int channel) const
{
	const ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	return p->stopbit;
}

//---------------------------------------------------------------------------
//
//Get parity
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::GetParity(int channel) const
{
	const ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	return p->parity;
}

//---------------------------------------------------------------------------
//
//Data receive
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCC::Receive(int channel)
{
	ch_t *p;
	DWORD data;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	//Initialize data
	data = 0;

	//If data exists
	if (p->txnum > 0) {
		//Take from send buffer
		data = p->txbuf[p->txread];
		p->txread = (p->txread + 1) & (sizeof(p->txbuf) - 1);
		p->txnum--;
	}

	return data;
}

//---------------------------------------------------------------------------
//
//Check send buffer empty
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::IsTxEmpty(int channel)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	//Return TRUE if send buffer empty
	if (p->txnum == 0) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//Check send buffer full
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::IsTxFull(int channel)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	//TRUE if send buffer 3/4 or more full
	if (p->txnum >= (sizeof(p->txbuf) * 3 / 4)) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//Send block
//
//---------------------------------------------------------------------------
void FASTCALL SCC::WaitTx(int channel, BOOL wait)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	//If releasing, need flag operation
	if (!wait) {
		if (p->txwait) {
			p->txsent = TRUE;
		}
	}

	//Settings
	p->txwait = wait;
}

//---------------------------------------------------------------------------
//
//Set break
//
//---------------------------------------------------------------------------
void FASTCALL SCC::SetBreak(int channel, BOOL flag)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	//Check interrupt
	if (p->baie) {
		//Interrupt generation code
	}

	//Set
	p->ba = flag;
}

//---------------------------------------------------------------------------
//
//Set CTS
//
//---------------------------------------------------------------------------
void FASTCALL SCC::SetCTS(int channel, BOOL flag)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	//Check interrupt
	if (p->ctsie) {
		//Interrupt generation code
	}

	//Set
	p->cts = flag;
}

//---------------------------------------------------------------------------
//
//Set DCD
//
//---------------------------------------------------------------------------
void FASTCALL SCC::SetDCD(int channel, BOOL flag)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	//Check interrupt
	if (p->dcdie) {
		//Interrupt generation code
	}

	//Set
	p->dcd = flag;
}

//---------------------------------------------------------------------------
//
//Get break
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::GetBreak(int channel)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	//As is
	return p->brk;
}

//---------------------------------------------------------------------------
//
//Get RTS
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::GetRTS(int channel)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	//As is
	return p->rts;
}

//---------------------------------------------------------------------------
//
//Get DTR
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::GetDTR(int channel)
{
	ch_t *p;

	ASSERT(this);
	ASSERT((channel == 0) || (channel == 1));

	//Set pointer
	p = &scc.ch[channel];

	//If DTR/REQ FALSE, software specified
	if (!p->dtrreq) {
		return p->dtr;
	}

	//Indicates if send buffer is empty
	if (p->txwait) {
		return FALSE;
	}
	return !(p->tdf);
}

//---------------------------------------------------------------------------
//
//Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCC::Callback(Event *ev)
{
	ch_t *p;
	int channel;

	ASSERT(this);
	ASSERT(ev);

	//Take channel
	channel = ev->GetUser();
	ASSERT((channel == 0) || (channel == 1));
	p = &scc.ch[channel];

	//Receive
	if (p->rxen) {
		EventRx(p);
	}

	//Send
	if (p->txen) {
		EventTx(p);
	}

	//Speed change
	if (ev->GetTime() != p->speed) {
		ev->SetTime(p->speed);
	}

	//Continue interval
	return TRUE;
}

//---------------------------------------------------------------------------
//
//Event (receive)
//
//---------------------------------------------------------------------------
void FASTCALL SCC::EventRx(ch_t *p)
{
	BOOL flag;

	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(p->rxen);

	//Receive flag
	flag = TRUE;

	//Check DCD
	if (p->dcd && p->aen) {
		// If loopback down, cannot receive
		if (!p->loopback) {
			//Do not receive
			flag = FALSE;
		}
	}

	//If receive buffer valid, insert into FIFO
	if ((p->rxnum > 0) && flag) {
		if (p->rxfifo >= 3) {
			ASSERT(p->rxfifo == 3);

			//FIFO already full. Overrun
#if defined(SCC_LOG)
			LOG1(Log::Normal, "チャネル%d オーバーランエラー", p->index);
#endif	// SCC_LOG
			p->overrun = TRUE;
			p->rxfifo--;

			if (p->rxim >= 2) {
				IntSCC(p, rsi, TRUE);
			}
		}

		ASSERT(p->rxfifo <= 2);

		//Insert into receive FIFO
		p->rxdata[2 - p->rxfifo] = (DWORD)p->rxbuf[p->rxread];
		p->rxread = (p->rxread + 1) & (sizeof(p->rxbuf) - 1);
		p->rxnum--;
		p->rxfifo++;

		//Increase receive total
		p->rxtotal++;
	}

	//If receive FIFO valid, interrupt
	if (p->rxfifo > 0) {
		//Interrupt occurs
		switch (p->rxim) {
			//Interrupt only first time
			case 1:
				if (!p->rxno) {
					break;
				}
				p->rxno = FALSE;
				//Continue as is

			//Always interrupt
			case 2:
				IntSCC(p, rxi, TRUE);
				break;
		}
	}
}

//---------------------------------------------------------------------------
//
//Event (send)
//
//---------------------------------------------------------------------------
void FASTCALL SCC::EventTx(ch_t *p)
{
	ASSERT(this);
	ASSERT(p);
	ASSERT((p->index == 0) || (p->index == 1));
	ASSERT(p->txen);

	//If not wait mode, previous send completed
	if (!p->txwait) {
		p->txsent = TRUE;
	}

	//Check CTS
	if (p->cts && p->aen) {
		//In test mode, can send regardless of CTS
		if (!p->aecho && !p->loopback) {
			return;
		}
	}

	//Is there data
	if (p->tdf) {
		//Not in auto echo mode
		if (!p->aecho) {
			//Insert into send buffer
			p->txbuf[p->txwrite] = (BYTE)p->tdr;
			p->txwrite = (p->txwrite + 1) & (sizeof(p->txbuf) - 1);
			p->txnum++;

			//If overflow, discard old data
			if (p->txnum >= sizeof(p->txbuf)) {
				LOG1(Log::Warning, "チャネル%d 送信バッファオーバーフロー", p->index);
				p->txnum = sizeof(p->txbuf);
				p->txread = p->txwrite;
			}

			// Increase send total
			p->txtotal++;
		}

		//Send in progress
		p->txsent = FALSE;

		//Send buffer empty
		p->tdf = FALSE;

		//If local loopback mode, put data to receiver
		if (p->loopback && !p->aecho) {
			//Insert into receive buffer
			p->rxbuf[p->rxwrite] = (BYTE)p->tdr;
			p->rxwrite = (p->rxwrite + 1) & (sizeof(p->rxbuf) - 1);
			p->rxnum++;

			//If overflow, discard old data sequentially
			if (p->rxnum >= sizeof(p->rxbuf)) {
				LOG1(Log::Warning, "チャネル%d 受信バッファオーバーフロー", p->index);
				p->rxnum = sizeof(p->rxbuf);
				p->rxread = p->rxwrite;
			}
		}
	}

	//Interrupt if no send pending
	if (!p->txpend) {
		IntSCC(p, txi, TRUE);
	}
}
