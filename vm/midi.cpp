//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ytanaka (ytanaka@ipc-tokai.or.jp)
//	[ MIDI(YM3802) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "sync.h"
#include "config.h"
#include "fileio.h"
#include "midi.h"

//===========================================================================
//
//	MIDI
//
//===========================================================================
//#define MIDI_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
MIDI::MIDI(VM *p) : MemDevice(p)
{
	// Device ID setting
	dev.id = MAKEID('M', 'I', 'D', 'I');
	dev.desc = "MIDI (YM3802)";

	// Start address, End address
	memdev.first = 0xeae000;
	memdev.last = 0xeaffff;

	// Device
	sync = NULL;

	// Transmit buffer, Receive buffer
	midi.transbuf = NULL;
	midi.recvbuf = NULL;

	// Queue initialization (Real-time priority)
	midi.transnum = 0;
	midi.transread = 0;
	midi.transwrite = 0;
	midi.recvnum = 0;
	midi.recvread = 0;
	midi.recvwrite = 0;
	midi.normnum = 0;
	midi.normread = 0;
	midi.normwrite = 0;
	midi.rtnum = 0;
	midi.rtread = 0;
	midi.rtwrite = 0;
	midi.stdnum = 0;
	midi.stdread = 0;
	midi.stdwrite = 0;
	midi.rrnum = 0;
	midi.rrread = 0;
	midi.rrwrite = 0;
	midi.bid = 0;
	midi.ilevel = 4;
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL MIDI::Init()
{
 ASSERT(this);

 // Constructor
 if (!MemDevice::Init()) {
  return FALSE;
 }

 // Sync create
 ASSERT(!sync);
 sync = new Sync;
 ASSERT(sync);

 // Transmit buffer allocation
 try {
  midi.transbuf = new mididata_t[TransMax];
 }
 catch (...) {
  midi.transbuf = NULL;
  return FALSE;
 }
 if (!midi.transbuf) {
  return FALSE;
 }

 // Receive buffer allocation
 try {
  midi.recvbuf = new mididata_t[RecvMax];
 }
 catch (...) {
  midi.recvbuf = NULL;
  return FALSE;
 }
 if (!midi.recvbuf) {
  return FALSE;
 }

 // Event setup
 event[0].SetDevice(this);
 event[0].SetDesc("MIDI 31250bps");
 event[0].SetUser(0);
 event[0].SetTime(0);
 event[1].SetDevice(this);
 event[1].SetDesc("Clock");
 event[1].SetUser(1);
 event[1].SetTime(0);
 event[2].SetDevice(this);
 event[2].SetDesc("General Timer");
 event[2].SetUser(2);
 event[2].SetTime(0);

 // Board ID not set, priority level 4
 midi.bid = 0;
 midi.ilevel = 4;

 // Receive delay initial 0ms
 recvdelay = 0;

 return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::Cleanup()
{
 ASSERT(this);

 // Receive buffer delete
 if (midi.recvbuf) {
  delete[] midi.recvbuf;
  midi.recvbuf = NULL;
 }

 // Transmit buffer delete
 if (midi.transbuf) {
  delete[] midi.transbuf;
  midi.transbuf = NULL;
 }

 // Sync delete
 if (sync) {
  delete sync;
  sync = NULL;
 }

 // Constructor release
 MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::Reset()
{
 int i;

 ASSERT(this);
 ASSERT_DIAG();

 LOG0(Log::Normal, "Reset");

 // Not accessed
 midi.access = FALSE;

 // Reset state
 midi.reset = TRUE;

 // Register reset
 ResetReg();

 // Event reset
 for (i=0; i<3; i++) {
  event[i].SetTime(0);
 }
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL MIDI::Save(Fileio *fio, int ver)
{
 size_t sz;
 int i;

 ASSERT(this);
 ASSERT(fio);
 ASSERT(ver >= 0x0200);
 ASSERT_DIAG();

 LOG0(Log::Normal, "Save");

 // Size save
 sz = sizeof(midi_t);
 if (!fio->Write(&sz, sizeof(sz))) {
  return FALSE;
 }

 // Body save
 if (!fio->Write(&midi, (int)sz)) {
  return FALSE;
 }

 // Event save
 for (i=0; i<3; i++) {
  if (!event[i].Save(fio, ver)) {
   return FALSE;
  }
 }

 return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL MIDI::Load(Fileio *fio, int ver)
{
 midi_t bk;
 size_t sz;
 int i;

 ASSERT(this);
 ASSERT(fio);
 ASSERT(ver >= 0x0200);
 ASSERT_DIAG();

 LOG0(Log::Normal, "Load");

 // Status register save
 bk = midi;

 // Size load, verify
 if (!fio->Read(&sz, sizeof(sz))) {
  return FALSE;
 }
 if (sz != sizeof(midi_t)) {
  return FALSE;
 }

 // Body load
 if (!fio->Read(&midi, (int)sz)) {
  return FALSE;
 }

 // Event load
 for (i=0; i<3; i++) {
  if (!event[i].Load(fio, ver)) {
   return FALSE;
  }
 }

 // Event dynamically add/delete
 if (midi.bid == 0) {
  // Event delete
  if (scheduler->HasEvent(&event[0])) {
   scheduler->DelEvent(&event[0]);
   scheduler->DelEvent(&event[1]);
   scheduler->DelEvent(&event[2]);
  }
 }
 else {
  // Event add
  if (!scheduler->HasEvent(&event[0])) {
   scheduler->AddEvent(&event[0]);
   scheduler->AddEvent(&event[1]);
   scheduler->AddEvent(&event[2]);
  }
 }

 // Transmit buffer restore
 midi.transbuf = bk.transbuf;
 midi.transread = bk.transread;
 midi.transwrite = bk.transwrite;
 midi.transnum = bk.transnum;

 // Receive buffer restore
 midi.recvbuf = bk.recvbuf;
 midi.recvread = bk.recvread;
 midi.recvwrite = bk.recvwrite;
 midi.recvnum = bk.recvnum;

 return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::ApplyCfg(const Config *config)
{
 ASSERT(this);
 ASSERT(config);
 ASSERT_DIAG();

 LOG0(Log::Normal, "Apply config");

 if (midi.bid != (DWORD)config->midi_bid) {
  midi.bid = (DWORD)config->midi_bid;

  // Event dynamically add/delete
  if (midi.bid == 0) {
   // Event delete
   if (scheduler->HasEvent(&event[0])) {
    scheduler->DelEvent(&event[0]);
    scheduler->DelEvent(&event[1]);
    scheduler->DelEvent(&event[2]);
   }
  }
  else {
   // Event add
   if (!scheduler->HasEvent(&event[0])) {
    scheduler->AddEvent(&event[0]);
    scheduler->AddEvent(&event[1]);
    scheduler->AddEvent(&event[2]);
   }
  }
 }
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Diagnostic
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::AssertDiag() const
{
 // Constructor
 MemDevice::AssertDiag();

 ASSERT(this);
 ASSERT(GetID() == MAKEID('M', 'I', 'D', 'I'));
 ASSERT(memdev.first == 0xeae000);
 ASSERT(memdev.last == 0xeaffff);
 ASSERT((midi.bid >= 0) && (midi.bid < 2));
 ASSERT((midi.ilevel == 2) || (midi.ilevel == 4));
 ASSERT(midi.transnum <= TransMax);
 ASSERT(midi.transread < TransMax);
 ASSERT(midi.transwrite < TransMax);
 ASSERT(midi.recvnum <= RecvMax);
 ASSERT(midi.recvread < RecvMax);
 ASSERT(midi.recvwrite < RecvMax);
 ASSERT(midi.normnum <= 16);
 ASSERT(midi.normread < 16);
 ASSERT(midi.normwrite < 16);
 ASSERT(midi.rtnum <= 4);
 ASSERT(midi.rtread < 4);
 ASSERT(midi.rtwrite < 4);
 ASSERT(midi.stdnum <= 0x80);
 ASSERT(midi.stdread < 0x80);
 ASSERT(midi.stdwrite < 0x80);
 ASSERT(midi.rrnum <= 4);
 ASSERT(midi.rrread < 4);
 ASSERT(midi.rrwrite < 4);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Read byte not used
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::ReadByte(DWORD addr)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT_DIAG();

 switch (midi.bid) {
  // Board not connected
  case 0:
   break;

  // Board ID1
  case 1:
   // Range check
   if ((addr >= 0xeafa00) && (addr < 0xeafa10)) {
    // Odd address read returns dummy
    if ((addr & 1) == 0) {
     return 0xff;
    }

    // Register read
    addr -= 0xeafa00;
    addr >>= 1;
    return ReadReg(addr);
   }
   break;

  // Board ID2
  case 2:
   // Range check
   if ((addr >= 0xeafa10) && (addr < 0xeafa20)) {
    // Odd address read returns dummy
    if ((addr & 1) == 0) {
     return 0xff;
    }

    // Register read
    addr -= 0xeafa10;
    addr >>= 1;
    return ReadReg(addr);
   }
   break;

  // Invalid (should not occur)
  default:
   ASSERT(FALSE);
   break;
 }

 // Bus error
 cpu->BusErr(addr, TRUE);

 return 0xff;
}

//---------------------------------------------------------------------------
//
//	Read word not used
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::ReadWord(DWORD addr)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT((addr & 1) == 0);
 ASSERT_DIAG();

 return (0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//	Write byte not used
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::WriteByte(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 switch (midi.bid) {
  // Board not connected
  case 0:
   break;

  // Board ID1
  case 1:
   // Range check
   if ((addr >= 0xeafa00) && (addr < 0xeafa10)) {
    // Odd address write is ignored
    if ((addr & 1) == 0) {
     return;
    }

    // Register write
    addr -= 0xeafa00;
    addr >>= 1;
    WriteReg(addr, data);
    return;
   }
   break;

  // Board ID2
  case 2:
   // Range check
   if ((addr >= 0xeafa10) && (addr < 0xeafa20)) {
    // Odd address write is ignored
    if ((addr & 1) == 0) {
     return;
    }

    // Register write
    addr -= 0xeafa10;
    addr >>= 1;
    WriteReg(addr, data);
    return;
   }
   break;

  // Invalid (should not occur)
  default:
   ASSERT(FALSE);
   break;
 }

 // Bus error
 cpu->BusErr(addr, FALSE);
}

//---------------------------------------------------------------------------
//
//	Write word not used
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::WriteWord(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT((addr & 1) == 0);
 ASSERT(data < 0x10000);
 ASSERT_DIAG();

 WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//	Read only dummy
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::ReadOnly(DWORD addr) const
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT_DIAG();

 switch (midi.bid) {
  // Board not connected
  case 0:
   break;

  // Board ID1
  case 1:
   // Range check
   if ((addr >= 0xeafa00) && (addr < 0xeafa10)) {
    // Odd address read returns dummy
    if ((addr & 1) == 0) {
     return 0xff;
    }

    // Register read
    addr -= 0xeafa00;
    addr >>= 1;
    return ReadRegRO(addr);
   }
   break;

  // Board ID2
  case 2:
   // Range check
   if ((addr >= 0xeafa10) && (addr < 0xeafa20)) {
    // Odd address read returns dummy
    if ((addr & 1) == 0) {
     return 0xff;
    }

    // Register read
    addr -= 0xeafa10;
    addr >>= 1;
    return ReadRegRO(addr);
   }
   break;

  // Invalid (should not occur)
  default:
   ASSERT(FALSE);
   break;
 }

 return 0xff;
}

//---------------------------------------------------------------------------
//
//	MIDI Active Data Check
//
//---------------------------------------------------------------------------
BOOL FASTCALL MIDI::IsActive() const
{
 ASSERT(this);
 ASSERT_DIAG();

 // Board ID is not 0
 if (midi.bid != 0) {
  // After reset, if more than 1 time accessed
  if (midi.access) {
   return TRUE;
  }
 }

 // Not active
 return FALSE;
}

//---------------------------------------------------------------------------
//
//	Event Callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL MIDI::Callback(Event *ev)
{
 DWORD mtr;
 DWORD hus;

 ASSERT(this);
 ASSERT(ev);
 ASSERT_DIAG();

 // If access flag is FALSE, MIDI application does not exist
 if (!midi.access) {
  // No data, first Data check
  return TRUE;
 }

 // Parameter determines division ratio
 switch (ev->GetUser()) {
  // MIDI receive 31250bps
  case 0:
   // Receive and Transmit
   Receive();
   Transmit();
   return TRUE;

  // MIDI clock(Period timer)
  case 1:
   // Calculate MTR from divisor
   if (midi.mtr <= 1) {
    mtr = 0x40000;
   }
   else {
    mtr = midi.mtr << 4;
   }

   // Serial overflow counter (8bit counter)
   midi.srr++;
   midi.srr &= 0xff;
   if (midi.srr == 0) {
    // Interrupt if necessary
    Interrupt(3, TRUE);
   }

   // Down counter (15bit up/down counter)
   if (midi.str != 0xffff8000) {
    midi.str--;
   }
   if (midi.str >= 0xffff8000) {
    // Interrupt if necessary
    Interrupt(2, TRUE);
   }

   // Clock period counter reset, if overflow not occur then Data check
   midi.sct--;
   if (midi.sct != 0) {
    // If SCT=1, adjust time to avoid skew, correct overflow
    if (midi.sct == 1) {
     hus = mtr - (mtr / midi.scr) * (midi.scr - 1);
     if (event[1].GetTime() != hus) {
      event[1].SetTime(hus);
     }
    }
    return TRUE;
   }
   midi.sct = midi.scr;

   // Clock process
   Clock();

   // Receive enable
   if (midi.rcr & 1) {
    // Clock timer used
    if ((midi.dmr & 0x07) == 0x03) {
     if (midi.dmr & 0x08) {
      // Realtime clock receive buffer insert
      InsertRR(0xf8);
     }
    }
   }

   // Buffer Data check
   CheckRR();

   // Timer value is transferred (Mu-1)
   mtr /= midi.scr;
   if (ev->GetTime() != mtr) {
    ev->SetTime(mtr);
   }
   return TRUE;

  // General timer
  case 2:
   General();
   return TRUE;
 }

 // Otherwise is error. returns
 ASSERT(FALSE);
 ev->SetTime(0);
 return FALSE;
}

//---------------------------------------------------------------------------
//
//	Receive Callback
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::Receive()
{
 BOOL recv;
 mididata_t *p;
 DWORD diff;
 DWORD data;

 ASSERT(this);
 ASSERT_DIAG();

 // Receiver READY
 midi.rsr &= ~0x01;

 // If receive disabled then exit
 if ((midi.rcr & 1) == 0) {
  // No receive
  return;
 }

 // Receive parameter Data check
 if (midi.rrr != 0x08) {
  // Not 31250bps
  return;
 }
 if ((midi.rmr & 0x32) != 0) {
  // 8bit character, 1stop bit, Parity is not even
  return;
 }

 // Receive timeout OFF
 recv = FALSE;

 // Receive buffer Data check
 sync->Lock();
 if (midi.recvnum > 0) {
  // Valid data exists. Compare time stamp
  diff = scheduler->GetTotalTime();
  p = &midi.recvbuf[midi.recvread];
  diff -= p->vtime;
  if (diff > recvdelay) {
   // Receive done
   recv = TRUE;
  }
 }
 sync->Unlock();

 // If no receive available, then wait receive time-out
 if (!recv) {
  if (midi.rcn == 0x3ff) {
   // 327.68ms no receive, so clear status if necessary
   midi.rsr |= 0x04;
   if ((midi.imr & 4) == 0) {
    Interrupt(4, TRUE);
   }
  }
  midi.rcn++;
  if (midi.rcn >= 0x400) {
   midi.rcn = 0;
  }

  // No receive
  return;
 }

 // Receive counter reset
 midi.rcn = 0;

 // Receive buffer read data
 sync->Lock();
 data = midi.recvbuf[midi.recvread].data;
 midi.recvread = (midi.recvread + 1) & (RecvMax - 1);
 midi.recvnum--;
 sync->Unlock();

 if (data == 0xf8) {
  // Realtime clock receive buffer insert
  if (midi.dmr & 0x08) {
   InsertRR(0xf8);

   // Clock process
   Clock();

   // Buffer Data check
   CheckRR();
  }

  // Clock filter
  if (midi.rcr & 0x10) {
#if defined(MIDI_LOG)
   LOG0(Log::Normal, "MIDI Clock Filter pass $F8 Skip");
#endif	// MIDI_LOG
   return;
  }
 }
 if ((data >= 0xf9) && (data <= 0xfd)) {
  // Realtime clock receive buffer insert
  if (midi.dmr & 0x08) {
   InsertRR(data);
  }
 }

 // Address matching
 if (midi.rcr & 2) {
  if (data == 0xf0) {
   // SysEx ID Data check start
   midi.asr = 1;
  }
  if (data == 0xf7) {
   // Address matching end
   midi.asr = 0;
  }
  switch (midi.asr) {
   // SysEx ID Data check
   case 1:
    if ((midi.amr & 0x7f) == data) {
#if defined(MIDI_LOG)
     LOG1(Log::Normal, "SysEx ID match $%02X", midi.amr & 0x7f);
#endif	// MIDI_LOG
     midi.asr = 2;
    }
    else {
#if defined(MIDI_LOG)
     LOG1(Log::Normal, "SysEx ID mismatch $%02X", data);
#endif	// MIDI_LOG
     midi.asr = 3;
    }
    break;

   // Device ID Data check
   case 2:
    if (midi.amr & 0x80) {
     if ((midi.adr & 0x7f) != data) {
#if defined(MIDI_LOG)
      LOG1(Log::Normal, "Device ID mismatch $%02X", data);
#endif	// MIDI_LOG
      midi.asr = 3;
     }
    }
    break;

   default:
    break;
  }
  if (midi.asr == 3) {
   // Address matching inside. Skip
   return;
  }
 }
 else {
  // Address matching disable
  midi.asr = 0;
 }

 // Receiver BUSY
 midi.rsr |= 0x01;

 // Standard buffer insert
 InsertStd(data);
}

//---------------------------------------------------------------------------
//
//	Transmit Callback
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::Transmit()
{
 ASSERT(this);
 ASSERT_DIAG();

 // Transmit Ready
 midi.tbs = FALSE;

 // Realtime clock transmit buffer Data check
 if (midi.rtnum > 0) {
  // Realtime clock transmit buffer data to transmit buffer transfer
  ASSERT(midi.rtnum <= 4);
  if (midi.tcr & 1) {
   InsertTrans(midi.rtbuf[midi.rtread]);
  }
  midi.rtnum--;
  midi.rtread = (midi.rtread + 1) & 3;

  // Transmit BUSY, if not transmitting then
  midi.tbs = TRUE;
  midi.tcn = 0;
  return;
 }

 // Normal buffer Data check
 if (midi.normnum > 0) {
  // Normal buffer data to transmit buffer transfer
  ASSERT(midi.normnum <= 16);
  if (midi.tcr & 1) {
   InsertTrans(midi.normbuf[midi.normread]);
  }
  midi.normnum--;
  midi.normread = (midi.normread + 1) & 15;

  // If normal buffer completely empty, then transmit buffer empty interrupt
  if (midi.normnum == 0) {
   Interrupt(6, TRUE);
  }

  // Transmit BUSY, if not transmitting then
  midi.tbs = TRUE;
  midi.tcn = 0;
  return;
 }

 // If more transmit cannot do so. Wait transmit time
 if (midi.tcn == 0xff) {
  // 81.92ms transmit
  if (midi.tcr & 1) {
   if (midi.dmr & 0x20) {
    // Active buffer mode transmit(DMR flag specified)
    InsertRT(0xfe);
   }
  }
 }
 midi.tcn++;
 if (midi.tcn >= 0x100) {
  midi.tcn = 0x100;
 }
}

//---------------------------------------------------------------------------
//
//	MIDI Clock Output
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::Clock()
{
 ASSERT(this);
 ASSERT_DIAG();

 // Clock counter
 midi.ctr--;
 if (midi.ctr == 0) {
  // Reload
  if (midi.cdr == 0) {
   midi.ctr = 0x80;
  }
  else {
   midi.ctr = midi.cdr;
  }

  // Interrupt if necessary
  if ((midi.imr & 8) == 0) {
   Interrupt(1, TRUE);
  }
 }
}

//---------------------------------------------------------------------------
//
//	General Timer Callback
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::General()
{
 DWORD hus;

 ASSERT(this);
 ASSERT_DIAG();

 // Interrupt if necessary Timer request
 Interrupt(7, TRUE);

 // GTR changed, re-set time
 hus = midi.gtr << 4;
 if (event[2].GetTime() != hus) {
  event[2].SetTime(hus);
 }
}

//---------------------------------------------------------------------------
//
//	Transmit buffer insert
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::InsertTrans(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Transmit buffer insert $%02X", data);
#endif	// MIDI_LOG

 // Lock
 sync->Lock();

 // Data insert
 midi.transbuf[midi.transwrite].data = data;
 midi.transbuf[midi.transwrite].vtime = scheduler->GetTotalTime();
 midi.transwrite = (midi.transwrite + 1) & (TransMax - 1);
 midi.transnum++;
 if (midi.transnum > TransMax) {
  // Transmit buffer overflow occurs (I/O operation not executed)
  midi.transnum = TransMax;
  midi.transread = midi.transwrite;
 }

 // Unlock
 sync->Unlock();
}

//---------------------------------------------------------------------------
//
//	Receive buffer insert
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::InsertRecv(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Receive buffer insert $%02X", data);
#endif	// MIDI_LOG

 // Lock
 sync->Lock();

 // Data insert
 midi.recvbuf[midi.recvwrite].data = data;
 midi.recvbuf[midi.recvwrite].vtime = scheduler->GetTotalTime();
 midi.recvwrite = (midi.recvwrite + 1) & (RecvMax - 1);
 midi.recvnum++;
 if (midi.recvnum > RecvMax) {
  LOG0(Log::Warning, "Receive buffer Overflow");
  midi.recvnum = RecvMax;
  midi.recvread = midi.recvwrite;
 }

 // Unlock
 sync->Unlock();
}

//---------------------------------------------------------------------------
//
//	Normal buffer insert
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::InsertNorm(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Normal buffer insert $%02X", data);
#endif	// MIDI_LOG

 // Lock
 sync->Lock();

 // Data insert
 midi.normbuf[midi.normwrite] = data;
 midi.normwrite = (midi.normwrite + 1) & 15;
 midi.normnum++;
 midi.normtotal++;
 if (midi.normnum > 16) {
  LOG0(Log::Warning, "Normal buffer Overflow");
  midi.normnum = 16;
  midi.normread = midi.normwrite;
 }

 // Unlock
 sync->Unlock();
}

//---------------------------------------------------------------------------
//
//	Realtime clock transmit buffer insert
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::InsertRT(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Realtime clock transmit buffer insert $%02X", data);
#endif	// MIDI_LOG

 // Lock
 sync->Lock();

 // Data insert
 midi.rtbuf[midi.rtwrite] = data;
 midi.rtwrite = (midi.rtwrite + 1) & 3;
 midi.rtnum++;
 midi.rttotal++;
 if (midi.rtnum > 4) {
  LOG0(Log::Warning, "Realtime clock transmit buffer Overflow");
  midi.rtnum = 4;
  midi.rtread = midi.rtwrite;
 }

 // Unlock
 sync->Unlock();
}

//---------------------------------------------------------------------------
//
//	Standard buffer insert
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::InsertStd(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Standard buffer insert $%02X", data);
#endif	// MIDI_LOG

 // Lock
 sync->Lock();

 // Data insert
 midi.stdbuf[midi.stdwrite] = data;
 midi.stdwrite = (midi.stdwrite + 1) & 0x7f;
 midi.stdnum++;
 midi.stdtotal++;
 if (midi.stdnum > 0x80) {
#if defined(MIDI_LOG)
  LOG0(Log::Warning, "Standard buffer Overflow");
#endif	// MIDI_LOG
  midi.stdnum = 0x80;
  midi.stdread = midi.stdwrite;

  // Receive status set (overflow)
  midi.rsr |= 0x40;
 }

 // Unlock
 sync->Unlock();

 // Receive status set (Receive data ready)
 midi.rsr |= 0x80;

 // Interrupt if necessary(Receive data available)
 if (midi.stdnum == 1) {
  Interrupt(5, TRUE);
 }
}

//---------------------------------------------------------------------------
//
//	Realtime clock receive buffer insert
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::InsertRR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Realtime clock receive buffer insert $%02X", data);
#endif	// MIDI_LOG

 // Lock
 sync->Lock();

 // Data insert
 midi.rrbuf[midi.rrwrite] = data;
 midi.rrwrite = (midi.rrwrite + 1) & 3;
 midi.rrnum++;
 midi.rrtotal++;
 if (midi.rrnum > 4) {
#if defined(MIDI_LOG)
  LOG0(Log::Warning, "Realtime clock receive buffer Overflow");
#endif	// MIDI_LOG
  midi.rrnum = 4;
  midi.rrread = midi.rrwrite;
 }

 // Unlock
 sync->Unlock();
}

//---------------------------------------------------------------------------
//
//	Transmit buffer number get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::GetTransNum() const
{
 ASSERT(this);
 ASSERT_DIAG();

 // 4 byte data read ahead, No lock
 // (Can determine if data exists for reading)
 return  midi.transnum;
}

//---------------------------------------------------------------------------
//
//	Transmit buffer data get
//
//---------------------------------------------------------------------------
const MIDI::mididata_t* FASTCALL MIDI::GetTransData(DWORD proceed)
{
 const mididata_t *ptr;
 int offset;

 ASSERT(this);
 ASSERT(proceed < TransMax);
 ASSERT_DIAG();

 // Lock
 sync->Lock();

 // Get
 offset = midi.transread;
 offset += proceed;
 offset &= (TransMax - 1);
 ptr = &(midi.transbuf[offset]);

 // Unlock
 sync->Unlock();

 // Return
 return ptr;
}

//---------------------------------------------------------------------------
//
//	Transmit buffer delete
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::DelTransData(DWORD number)
{
 ASSERT(this);
 ASSERT(number < TransMax);
 ASSERT_DIAG();

 // Lock
 sync->Lock();

 // Number fix
 if (number > midi.transnum) {
  number = midi.transnum;
 }

 // Number read pointer change
 midi.transnum -= number;
 midi.transread = (midi.transread + number) & (TransMax - 1);

 // Unlock
 sync->Unlock();
}

//---------------------------------------------------------------------------
//
//	Transmit buffer clear
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::ClrTransData()
{
 ASSERT(this);
 ASSERT_DIAG();

 // Lock
 sync->Lock();

 // Clear
 midi.transnum = 0;
 midi.transread = 0;
 midi.transwrite = 0;

 // Unlock
 sync->Unlock();
}

//---------------------------------------------------------------------------
//
//	Receive buffer data set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetRecvData(const BYTE *ptr, DWORD length)
{
 DWORD lp;

 ASSERT(this);
 ASSERT(ptr);
 ASSERT_DIAG();

 // If Board ID is 0, ignore
 if (midi.bid == 0) {
  return;
 }

 // If receive enable then, data insert
 if (midi.rcr & 1) {
  // Insert to receive buffer
  for (lp=0; lp<length; lp++) {
   InsertRecv((DWORD)ptr[lp]);
  }
 }

 // If receive enable and also not enable, do not THRU set as specified then transmit buffer insert
 if (midi.trr & 0x40) {
  for (lp=0; lp<length; lp++) {
   InsertTrans((DWORD)ptr[lp]);
  }
 }
}

//---------------------------------------------------------------------------
//
//	Receive delay set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetRecvDelay(int delay)
{
 ASSERT(this);
 ASSERT(delay >= 0);
 ASSERT_DIAG();

 // Is in hus unit
 recvdelay = delay;
}

//---------------------------------------------------------------------------
//
//	Register reset
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::ResetReg()
{
 ASSERT(this);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG0(Log::Normal, "Register reset");
#endif	// MIDI_LOG

 // Interrupt if necessary
 midi.vector = -1;

 // MCS register(read)
 midi.wdr = 0;
 midi.rgr = 0xff;

 // MCS register(Interrupt if necessary)
 // IMR bit1 is set(General interrupt priority)
 midi.ivr = 0x10;
 midi.isr = 0;
 midi.imr = 0x02;
 midi.ier = 0;

 // MCS register(Realtime clock message)
 midi.dmr = 0;
 midi.dcr = 0;

 // MCS register(Receive)
 midi.rrr = 0;
 midi.rmr = 0;
 midi.rcn = 0;
 midi.amr = 0;
 midi.adr = 0;
 midi.asr = 0;
 midi.rsr = 0;
 midi.rcr = 0;

 // MCS register(Transmit)
 midi.trr = 0;
 midi.tmr = 0;
 midi.tbs = FALSE;
 midi.tcn = 0;
 midi.tcr = 0;

 // MCS register(FSK)
 midi.fsr = 0;
 midi.fcr = 0;

 // MCS register(Counter)
 midi.ccr = 2;
 midi.cdr = 0;
 midi.ctr = 0x80;
 midi.srr = 0;
 midi.scr = 1;
 midi.sct = 1;
 midi.spr = 0;
 midi.str = 0xffff8000;
 midi.gtr = 0;
 midi.mtr = 0;

 // MCS register(GPIO)
 midi.edr = 0;
 midi.eor = 0;
 midi.eir = 0;

 // Normal buffer
 midi.normnum = 0;
 midi.normread = 0;
 midi.normwrite = 0;
 midi.normtotal = 0;

 // Realtime clock transmit buffer
 midi.rtnum = 0;
 midi.rtread = 0;
 midi.rtwrite = 0;
 midi.rttotal = 0;

 // Standard buffer
 midi.stdnum = 0;
 midi.stdread = 0;
 midi.stdwrite = 0;
 midi.stdtotal = 0;

 // Realtime clock receive buffer
 midi.rrnum = 0;
 midi.rrread = 0;
 midi.rrwrite = 0;
 midi.rrtotal = 0;

 // Transmit buffer
 sync->Lock();
 midi.transnum = 0;
 midi.transread = 0;
 midi.transwrite = 0;
 sync->Unlock();

 // Receive buffer
 sync->Lock();
 midi.recvnum = 0;
 midi.recvread = 0;
 midi.recvwrite = 0;
 sync->Unlock();

 // Event stop(General timer)
 event[2].SetTime(0);

 // Event start(MIDI clock)(OPMDRV3.X)
 event[1].SetDesc("Clock");
 event[1].SetTime(0x40000);

 // Extension counter reset
 ex_cnt[0] = 0;
 ex_cnt[1] = 0;
 ex_cnt[2] = 0;
 ex_cnt[3] = 0;

 // All interrupt if necessary are processed
 IntCheck();

 // Normal buffer empty interrupt (Actually ISR change)
 Interrupt(6, TRUE);
}

//---------------------------------------------------------------------------
//
//	Register read out
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::ReadReg(DWORD reg)
{
 DWORD group;

 ASSERT(this);
 ASSERT(reg < 8);
 ASSERT_DIAG();

 // Access flag ON
 if (!midi.access) {
  event[0].SetTime(640);
  midi.access = TRUE;
 }

 // Wait
 scheduler->Wait(2);

 switch (reg) {
  // IVR
  case 0:
#if defined(MIDI_LOG)
   LOG1(Log::Normal, "Interrupt vector read $%02X", midi.ivr);
#endif	// MIDI_LOG
   return midi.ivr;

  // RGR
  case 1:
   return midi.rgr;

  // ISR
  case 2:
#if defined(MIDI_LOG)
   LOG1(Log::Normal, "Interrupt service register read $%02X", midi.isr);
#endif	// MIDI_LOG
   return midi.isr;

  // ICR
  case 3:
   LOG1(Log::Normal, "Register read R%d", reg);
   return midi.wdr;
 }

 // Group from register number create
 group = midi.rgr & 0x0f;
 if (group >= 0x0a) {
  return 0xff;
 }

 // Register number add
 reg += (group * 10);

 // Register read(Register 4 and above)
 switch (reg) {
  // DSR
  case 16:
   return GetDSR();

  // RSR
  case 34:
   return GetRSR();

  // RDR(ReadOnly)
  case 36:
   return GetRDR();

  // TSR
  case 54:
   return GetTSR();

  // FSR
  case 64:
   return GetFSR();

  // SRR
  case 74:
   return GetSRR();

  // EIR
  case 96:
   return GetEIR();
 }

 LOG1(Log::Normal, "Register read R%d", reg);
 return midi.wdr;
}

//---------------------------------------------------------------------------
//
//	Register write not used
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::WriteReg(DWORD reg, DWORD data)
{
 DWORD group;

 ASSERT(this);
 ASSERT(reg < 8);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Access flag ON
 if (!midi.access) {
  event[0].SetTime(640);
  midi.access = TRUE;
 }

 // Wait
 scheduler->Wait(2);

 // Write not used data store (Send command register to access)
 midi.wdr = data;

 // Register write
 switch (reg) {
  // IVR
  case 0:
   LOG2(Log::Normal, "Register write R%d <- $%02X", reg, data);
   return;

  // RGR
  case 1:
   // First bit is register reset
   if (data & 0x80) {
    ResetReg();
   }
   midi.rgr = data;
   return;

  // ISR
  case 2:
   LOG2(Log::Normal, "Register write R%d <- $%02X", reg, data);
   return;

  // ICR
  case 3:
   SetICR(data);
   return;
 }

 // Group from register number create
 group = midi.rgr & 0x0f;
 if (group >= 0x0a) {
  return;
 }

 // Register number add
 reg += (group * 10);

 // Register write(Register 4 and above)
 switch (reg) {
  // IOR
  case 4:
   SetIOR(data);
   return;

  // IMR
  case 5:
   SetIMR(data);
   return;

  // IER
  case 6:
   SetIER(data);
   return;

  // DMR
  case 14:
   SetDMR(data);
   return;

  // DCR
  case 15:
   SetDCR(data);
   return;

  // DNR
  case 17:
   SetDNR(data);
   return;

  // RRR
  case 24:
   SetRRR(data);
   return;

  // RMR
  case 25:
   SetRMR(data);
   return;

  // AMR
  case 26:
   SetAMR(data);
   return;

  // ADR
  case 27:
   SetADR(data);
   return;

  // RCR
  case 35:
   SetRCR(data);
   return;

  // TRR
  case 44:
   SetTRR(data);
   return;

  // TMR
  case 45:
   SetTMR(data);
   return;

  // TCR
  case 55:
   SetTCR(data);
   return;

  // TDR
  case 56:
   SetTDR(data);
   return;

  // FCR
  case 65:
   SetFCR(data);
   return;

  // CCR
  case 66:
   SetCCR(data);
   return;

  // CDR
  case 67:
   SetCDR(data);
   return;

  // SCR
  case 75:
   SetSCR(data);
   return;

  // SPR(L)
  case 76:
   SetSPR(data, FALSE);
   return;

  // SPR(H)
  case 77:
   SetSPR(data, TRUE);
   return;

  // GTR(L)
  case 84:
   SetGTR(data, FALSE);
   return;

  // GTR(H)
  case 85:
   SetGTR(data, TRUE);
   return;

  // MTR(L)
  case 86:
   SetMTR(data, FALSE);
   return;

  // MTR(H)
  case 87:
   SetMTR(data, TRUE);
   return;

  // EDR
  case 94:
   SetEDR(data);
   return;

  // EOR
  case 96:
   SetEOR(data);
   return;
 }

 LOG2(Log::Normal, "Register write R%d <- $%02X", reg, data);
}

//---------------------------------------------------------------------------
//
//	Register read out(ReadOnly)
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::ReadRegRO(DWORD reg) const
{
 DWORD group;

 ASSERT(this);
 ASSERT(reg < 8);
 ASSERT_DIAG();

 switch (reg) {
  // IVR
  case 0:
   return midi.ivr;

  // RGR
  case 1:
   return midi.rgr;

  // ISR
  case 2:
   return midi.isr;

  // ICR
  case 3:
   return midi.wdr;
 }

 // Group from register number create
 group = midi.rgr & 0x0f;
 if (group >= 0x0a) {
  return 0xff;
 }

 // Register number add
 reg += (group * 10);

 // Register read(Register 4 and above)
 switch (reg) {
  // DSR
  case 16:
   return GetDSR();

  // RSR
  case 34:
   return GetRSR();

  // RDR(ReadOnly)
  case 36:
   return GetRDRRO();

  // TSR
  case 54:
   return GetTSR();

  // FSR
  case 64:
   return GetFSR();

  // SRR
  case 74:
   return GetSRR();

  // EIR
  case 96:
   return GetEIR();
 }

 return midi.wdr;
}

//---------------------------------------------------------------------------
//
//	ICR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetICR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Interrupt clear register set $%02X", data);
#endif

 // Bit inverted data used, Clear service
 midi.isr &= ~data;

 // Interrupt if necessary Data check
 IntCheck();
}

//---------------------------------------------------------------------------
//
//	IOR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetIOR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Upper 3bit valid
 data &= 0xe0;

 LOG1(Log::Detail, "Interrupt vector base $%02X", data);

 // IVR change
 midi.ivr &= 0x1f;
 midi.ivr |= data;

 // Interrupt if necessary Data check
 IntCheck();
}

//---------------------------------------------------------------------------
//
//	IMR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetIMR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 data &= 0x0f;

 if (midi.imr != data) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "Interrupt mask register set $%02X", data);
#endif	// MIDI_LOG
  midi.imr = data;

  // Interrupt if necessary Data check
  IntCheck();
 }
}

//---------------------------------------------------------------------------
//
//	IER set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetIER(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 if (midi.ier != data) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "Interrupt enable register set $%02X", data);
#endif	// MIDI_LOG

  // Set
  midi.ier = data;

  // Interrupt if necessary Data check
  IntCheck();
 }
}

//---------------------------------------------------------------------------
//
//	DMR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetDMR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Valid data extract
 data &= 0x3f;

 if (midi.dmr != data) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "Realtime clock message mode register set $%02X", data);
#endif

  // Set
  midi.dmr = data;
 }
}

//---------------------------------------------------------------------------
//
//	DCR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetDCR(DWORD data)
{
 DWORD msg;

 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Realtime clock message control register set $%02X", data);
#endif

 // Set
 midi.dcr = data;

 // Message create
 msg = data & 0x07;
 msg += 0xf8;
 if (msg >= 0xfe) {
  LOG1(Log::Warning, "Invalid realtime clock message $%02X", msg);
  return;
 }

 // MIDI clock message is created by DMR bit2 controlling
 if (msg == 0xf8) {
  if ((midi.dmr & 4) == 0) {
   // MIDI clock timer, $F8 clocks etc. not count
   return;
  }

  // During timing, MIDI clock also counted
  if (midi.rcr & 1) {
   if (midi.dmr & 8) {
    // Realtime clock receive buffer insert
    InsertRR(0xf8);

    // Clock process
    Clock();

    // Buffer Data check
    CheckRR();
   }
  }
  return;
 }
 else {
  // Other messages are inserted to realtime receive buffer
  if ((data & 0x80) == 0) {
   return;
  }
 }

 // Realtime clock receive buffer insert data
 InsertRT(msg);
}

//---------------------------------------------------------------------------
//
//	DSR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::GetDSR() const
{
 ASSERT(this);
 ASSERT_DIAG();

 // FIFO-IRx data read out. If data none then 0
 if (midi.rrnum == 0) {
  return 0;
 }

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "FIFO-IRx get $%02X", midi.rrbuf[midi.rrread]);
#endif	// MIDI_LOG

 return midi.rrbuf[midi.rrread];
}

//---------------------------------------------------------------------------
//
//	DNR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetDNR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 if (data & 0x01) {
#if defined(MIDI_LOG)
  LOG0(Log::Normal, "FIFO-IRx Update");
#endif	// MIDI_LOG

  // Realtime clock receive buffer update
  if (midi.rrnum > 0) {
   midi.rrnum--;
   midi.rrread = (midi.rrread + 1) & 3;
  }

  // Realtime clock receive buffer Interrupt if necessary Data check
  CheckRR();
 }
}

//---------------------------------------------------------------------------
//
//	RRR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetRRR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Valid bit mask
 data &= 0x38;

 if (midi.rrr != data) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "Receive mode set $%02X", data);
#endif	// MIDI_LOG

  // Set
  midi.rrr = data;
 }

 // Receive mode Data check
 if (midi.rrr != 0x08) {
  LOG1(Log::Normal, "Receive mode error $%02X", midi.rrr);
 }
}

//---------------------------------------------------------------------------
//
//	RMR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetRMR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Valid bit mask
 data &= 0x3f;

 if (midi.rmr != data) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "Receive parameter set $%02X", data);
#endif	// MIDI_LOG

  // Set
  midi.rmr = data;
 }

 // Receive parameter Data check
 if ((midi.rmr & 0x32) != 0) {
  LOG1(Log::Warning, "Receive parameter error $%02X", midi.rmr);
 }
}

//---------------------------------------------------------------------------
//
//	AMR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetAMR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 if (midi.amr != data) {
#if defined(MIDI_LOG)
  if (data & 0x80) {
   LOG1(Log::Normal, "Receive SysExID+DeviceID check. SysExID $%02X", data & 0x7f);
  }
  else {
   LOG1(Log::Normal, "Receive SysExID only check. SysExID $%02X", data & 0x7f);
  }
#endif	// MIDI_LOG

  midi.amr = data;
 }
}

//---------------------------------------------------------------------------
//
//	ADR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetADR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 if (midi.adr != data) {
#if defined(MIDI_LOG)
  if (data & 0x80) {
   LOG1(Log::Normal, "Universal allow. DeviceID $%02X", data & 0x7f);
  }
  else {
   LOG1(Log::Normal, "Universal disallow. DeviceID $%02X", data & 0x7f);
  }
#endif	// MIDI_LOG

  midi.adr = data;
 }
}

//---------------------------------------------------------------------------
//
//	RSR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::GetRSR() const
{
 ASSERT(this);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Receive buffer status get $%02X", midi.rsr);
#endif	// MIDI_LOG

 return midi.rsr;
}

//---------------------------------------------------------------------------
//
//	RCR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetRCR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Valid bit mask
 data &= 0xdf;

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Receive control register $%02X", data);
#endif	// MIDI_LOG

 // Data set
 midi.rcr = data;

 // Overrun flag clear
 if (midi.rcr & 0x04) {
  midi.rsr &= ~0x04;

  // Overrun Interrupt if necessary process
  if ((midi.imr & 4) == 0) {
   Interrupt(4, FALSE);
  }
 }

 // Break flag clear
 if (midi.rcr & 0x08) {
  midi.rsr &= ~0x08;

  // Break Interrupt if necessary process
  if (midi.imr & 4) {
   Interrupt(4, FALSE);
  }
 }

 // Overrun flag clear
 if (midi.rcr & 0x40) {
  midi.rsr &= ~0x40;
 }

 // Receive buffer clear
 if (midi.rcr & 0x80) {
  // Normal buffer
  midi.stdnum = 0;
  midi.stdread = 0;
  midi.stdwrite = 0;

  // Realtime clock receive buffer
  midi.rrnum = 0;
  midi.rrread = 0;
  midi.rrwrite = 0;

  // Interrupt if necessary clear
  Interrupt(5, FALSE);
  Interrupt(0, FALSE);
  if (midi.imr & 8) {
   Interrupt(1, FALSE);
  }
 }

 // Address matching clear
 if (midi.rcr & 0x02) {
#if defined(MIDI_LOG)
  LOG0(Log::Normal, "Address matching enable");
#endif	// MIDI_LOG
  midi.rsr |= 0x02;
 }
 else {
#if defined(MIDI_LOG)
  LOG0(Log::Normal, "Address matching disable");
#endif	// MIDI_LOG
  midi.rsr &= ~0x02;
 }

 // Receive enable
 if (midi.rcr & 0x01) {
#if defined(MIDI_LOG)
  LOG0(Log::Normal, "Receive enable");
#endif	// MIDI_LOG
 }
 else {
#if defined(MIDI_LOG)
  LOG0(Log::Normal, "Receive disable");
#endif	// MIDI_LOG
 }
}

//---------------------------------------------------------------------------
//
//	RDR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::GetRDR()
{
 DWORD data;

 ASSERT(this);
 ASSERT_DIAG();

 // Data to wdr(dummy)
 data = midi.wdr;

 // If buffer has valid data
 if (midi.stdnum > 0) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "Standard buffer get $%02X", midi.stdbuf[midi.stdread]);
#endif	// MIDI_LOG

  // Buffer read data get
  data = midi.stdbuf[midi.stdread];

  // Buffer decrement
  midi.stdnum--;
  midi.stdread = (midi.stdread + 1) & 0x7f;

  // Interrupt if necessary process
  Interrupt(5, FALSE);

  // If buffer becomes empty
  if (midi.stdnum == 0) {
   // Receive buffer enable flag clear
   midi.rsr &= 0x7f;
  }
  else {
   // Receive buffer enable flag set
   midi.rsr |= 0x80;
  }

  // Data Data check(for verify, Extension counter increment)
  if (data == 0xf0) {
   ex_cnt[2]++;
  }
  if (data == 0xf7) {
   ex_cnt[3]++;
  }
 }

 return data;
}

//---------------------------------------------------------------------------
//
//	RDR get (Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::GetRDRRO() const
{
 DWORD data;

 ASSERT(this);
 ASSERT_DIAG();

 // Data to wdr(dummy)
 data = midi.wdr;

 // If buffer has valid data
 if (midi.stdnum > 0) {
  data = midi.stdbuf[midi.stdread];
 }

 return data;
}

//---------------------------------------------------------------------------
//
//	TRR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetTRR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Valid bit mask
 data &= 0x78;

 if (midi.trr != data) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "Transmit mode set $%02X", data);
#endif	// MIDI_LOG

  // Set
  midi.trr = data;
 }

 // Transmit mode Data check
 if ((midi.trr & 0x38) != 0x08) {
  LOG1(Log::Warning, "Transmit mode error $%02X", midi.trr);
 }

#if defined(MIDI_LOG)
 if (midi.trr & 0x40) {
  LOG0(Log::Normal, "MIDI THRU set");
 }
#endif	// MIDI_LOG
}

//---------------------------------------------------------------------------
//
//	TMR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetTMR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Valid bit mask
 data &= 0x3f;

 if (midi.tmr != data) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "Transmit parameter set $%02X", data);
#endif	// MIDI_LOG

  // Set
  midi.tmr = data;
 }

 // Transmit parameter Data check
 if ((midi.tmr & 0x32) != 0) {
  LOG1(Log::Warning, "Transmit parameter error $%02X", midi.tmr);
 }
}

//---------------------------------------------------------------------------
//
//	TSR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::GetTSR() const
{
 DWORD data;

 ASSERT(this);
 ASSERT_DIAG();

 // Clear
 data = 0;

 // bit7:Normal buffer empty flag
 if (midi.normnum == 0) {
  data |= 0x80;
 }

 // bit6:Normal buffer data enable flag
 if (midi.normnum < 16) {
  data |= 0x40;
 }

 // bit2:Transmit complete(81.920ms) flag
 if (midi.tcn >= 0x100) {
  ASSERT(midi.tcn == 0x100);
  data |= 0x04;
 }

 // bit0:Transmitter BUSY flag
 if (midi.tbs) {
  data |= 0x01;
 }

 return data;
}

//---------------------------------------------------------------------------
//
//	TCR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetTCR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Valid bit mask
 data &= 0x8d;

#if defined(MIDI_LOG)
 if (midi.tcr != data) {
  LOG1(Log::Normal, "Transmit control register $%02X", data);
 }
#endif	// MIDI_LOG

 // Valid bit save
 midi.tcr = data;

 // Normal buffer and Realtime buffer transmit buffer clear
 if (data & 0x80) {
  // Normal buffer
  midi.normnum = 0;
  midi.normread = 0;
  midi.normwrite = 0;

  // Realtime buffer transmit buffer
  midi.rtnum = 0;
  midi.rtread = 0;
  midi.rtwrite = 0;

  // Interrupt if necessary clear
  Interrupt(6, TRUE);
 }

 // Transmit complete(81.920ms) flag clear
 if (data & 0x04) {
  midi.tcn = 0;
 }

 // Transmit enable
#if defined(MIDI_LOG)
 if (data & 0x01) {
  LOG0(Log::Normal, "Transmit enable");
 }
 else {
  LOG0(Log::Normal, "Transmit disable");
 }
#endif	// MIDI_LOG
}

//---------------------------------------------------------------------------
//
//	TDR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetTDR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Transmit buffer write $%02X", data);
#endif	// MIDI_LOG

 // Data Data check(for verify, Extension counter increment)
 if (data == 0xf0) {
  ex_cnt[0]++;
 }
 if (data == 0xf7) {
  ex_cnt[1]++;
 }

 // Transmit enable Data check(Transmit disable then return:Synchronous mode)
 if ((midi.trr & 0x38) != 0x08) {
  // Transmit mode is not 31250bps
  return;
 }
 if ((midi.tmr & 0x32) != 0) {
  // 8bit data, 1stop bit, Parity is not even
  return;
 }

 // Normal buffer insert data
 InsertNorm(data);

 // Transmit buffer empty interrupt then interrupt clear
 Interrupt(6, FALSE);
}

//---------------------------------------------------------------------------
//
//	FSR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::GetFSR() const
{
 ASSERT(this);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "FIFO SYNC Status get $%02X", midi.fsr);
#endif	// MIDI_LOG

 return midi.fsr;
}

//---------------------------------------------------------------------------
//
//	FCR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetFCR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Valid bit mask
 data &= 0x9f;

 if (midi.fcr != data) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "FIFO SYNC Command set $%02X", data);
#endif	// MIDI_LOG

  // Set
  midi.fcr = data;
 }
}

//---------------------------------------------------------------------------
//
//	CCR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetCCR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Clock control register set $%02X", data);
#endif	// MIDI_LOG

 // Lower 2bit valid
 data &= 0x03;

 // bit1 must be 1
 if ((data & 2) == 0) {
  LOG0(Log::Warning, "CLKM Clock use end Error");
 }

 // Set
 midi.ccr = data;
}

//---------------------------------------------------------------------------
//
//	CDR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetCDR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // CDR set
 if (midi.cdr != (data & 0x7f)) {
  midi.cdr = (data & 0x7f);

#if defined(MIDI_LOG)
  LOG1(Log::Normal, "Clock counter constant set $%02X", midi.cdr);
#endif	// MIDI_LOG
 }

 // bit7 load to CTR value
 if (data & 0x80) {
  // CTR set
  midi.ctr = midi.cdr;

#if defined(MIDI_LOG)
  LOG0(Log::Normal, "Clock counter constant set load");
#endif	// MIDI_LOG
 }
}

//---------------------------------------------------------------------------
//
//	SRR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::GetSRR() const
{
 ASSERT(this);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Serial overflow counter get $%02X", midi.srr);
#endif	// MIDI_LOG

 return midi.srr;
}

//---------------------------------------------------------------------------
//
//	SCR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetSCR(DWORD data)
{
 DWORD mtr;
 char desc[16];

 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 // Ratio set
 if (midi.scr != (data & 0x0f)) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "MIDI Clock ratio set $%02X", data & 0x0f);
#endif	// MIDI_LOG
  midi.scr = (data & 0x0f);

  // Ratio 0 is set prohibited
  if (midi.scr == 0) {
   LOG0(Log::Warning, "MIDI Clock ratio Set prohibition value");
   midi.scr = 1;
  }

  // Event re-set
  if (midi.mtr <= 1) {
   mtr = 0x40000;
  }
  else {
   mtr = midi.mtr << 4;
  }
  mtr /= midi.scr;
  if (event[1].GetTime() != mtr) {
   event[1].SetTime(mtr);
  }

  // SCT Data check
  if (midi.sct > midi.scr) {
   midi.sct = midi.scr;
  }
  ASSERT(midi.sct > 0);

  // Event description
  if (midi.scr == 1) {
   event[1].SetDesc("Clock");
  }
  else {
   sprintf(desc, "Clock (Div%d)", midi.scr);
   event[1].SetDesc(desc);
  }
 }

 // Down counter clear
 if (data & 0x10) {
#if defined(MIDI_LOG)
  LOG0(Log::Normal, "Down counter clear");
#endif	// MIDI_LOG
  midi.str = 0;
 }

 // Down counter add
 if (data & 0x20) {
  // Add
  midi.str += midi.spr;

  // Overflow Data check
  if ((midi.str >= 0x8000) && (midi.str < 0x10000)) {
   midi.str = 0x8000;
  }

#if defined(MIDI_LOG)
  LOG2(Log::Normal, "Down counter add $%04X->$%04X", midi.spr, midi.str);
#endif	// MIDI_LOG
 }
}

//---------------------------------------------------------------------------
//
//	SPR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetSPR(DWORD data, BOOL high)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 if (high) {
  // Upper
  midi.spr &= 0x00ff;
  midi.spr |= ((data & 0x7f) << 8);
 }
 else {
  // Lower
  midi.spr &= 0xff00;
  midi.spr |= data;
 }

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Down counter constant set $%04X", midi.spr);
#endif	// MIDI_LOG
}

//---------------------------------------------------------------------------
//
//	GTR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetGTR(DWORD data, BOOL high)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 if (high) {
  // Upper
  midi.gtr &= 0x00ff;
  midi.gtr |= ((data & 0x3f) << 8);
 }
 else {
  // Lower
  midi.gtr &= 0xff00;
  midi.gtr |= data;
 }

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "General timer register set $%04X", midi.gtr);
#endif	// MIDI_LOG

 // bit7 data load
 if (high && (data & 0x80)) {
#if defined(MIDI_LOG)
  LOG0(Log::Normal, "General timer value load");
#endif	// MIDI_LOG

  // Data load, timer start
  event[2].SetTime(midi.gtr << 4);
 }
}

//---------------------------------------------------------------------------
//
//	MTR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetMTR(DWORD data, BOOL high)
{
 DWORD mtr;

 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 if (high) {
  // Upper
  midi.mtr &= 0x00ff;
  midi.mtr |= ((data & 0x3f) << 8);
 }
 else {
  // Lower
  midi.mtr &= 0xff00;
  midi.mtr |= data;
 }

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "MIDI Clock timer register set $%04X", midi.mtr);
#endif	// MIDI_LOG

 // bit7 data load
 if (high) {
  if (midi.mtr <= 1) {
   mtr = 0x40000;
  }
  else {
   mtr = midi.mtr << 4;
  }

  // SCR division value check
  ASSERT(midi.scr != 0);
  event[1].SetTime(mtr / midi.scr);
 }
}

//---------------------------------------------------------------------------
//
//	EDR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetEDR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 if (midi.edr != data) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "GPIO direction register set $%02X", data);
#endif	// MIDI_LOG

  midi.edr = data;
 }
}

//---------------------------------------------------------------------------
//
//	EOR set
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::SetEOR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

 if (midi.eor != data) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "GPIO output data set $%02X", data);
#endif	// MIDI_LOG

  midi.eor = data;
 }
}

//---------------------------------------------------------------------------
//
//	EIR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::GetEIR() const
{
 ASSERT(this);
 ASSERT_DIAG();

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "GPIO read $%02X", midi.eir);
#endif	// MIDI_LOG

 return midi.eir;
}

//---------------------------------------------------------------------------
//
//	Realtime clock receive buffer Data check
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::CheckRR()
{
 DWORD data;

 ASSERT(this);
 ASSERT_DIAG();

 // If buffer has no data
 if (midi.rrnum == 0) {
  // Interrupt if necessary clear
  Interrupt(0, FALSE);
  if (midi.imr & 8) {
   Interrupt(1, FALSE);
  }
  return;
 }

 // Latest data get
 data = midi.rrbuf[midi.rrread];

 // IRQ-1
 if (data == 0xf8) {
  if (midi.imr & 8) {
   Interrupt(1, TRUE);
   Interrupt(0, FALSE);
   return;
  }
 }

 // IRQ-0
 Interrupt(1, FALSE);
 if ((data >= 0xf9) && (data <= 0xfd)) {
  Interrupt(0, TRUE);
 }
 else {
  Interrupt(0, FALSE);
 }
}

//---------------------------------------------------------------------------
//
//	Interrupt if necessary request
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::Interrupt(int type, BOOL flag)
{
 ASSERT(this);
 ASSERT((type >= 0) && (type <= 7));
 ASSERT_DIAG();

 // ISR related bit change request
 if (flag) {
  midi.isr |= (1 << type);
 }
 else {
  midi.isr &= ~(1 << type);
 }

 // Interrupt if necessary Data check
 IntCheck();
}

//---------------------------------------------------------------------------
//
//	Interrupt if necessary ACK
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::IntAck(int level)
{
 ASSERT(this);
 ASSERT((level == 2) || (level == 4));
 ASSERT_DIAG();

 // If interrupt if necessary priority not correct then ignore
 if (level != (int)midi.ilevel) {
#if defined(MIDI_LOG)
  LOG1(Log::Normal, "Interrupt level error Level%d", level);
#endif	// MIDI_LOG
  return;
 }

 // If interrupt if necessary request not exist, then ignore
 if (midi.vector < 0) {
#if defined(MIDI_LOG)
  LOG0(Log::Normal, "Not requested interrupt");
#endif	// MIDI_LOG
  return;
 }

#if defined(MIDI_LOG)
 LOG1(Log::Normal, "Interrupt ACK Level%d", level);
#endif	// MIDI_LOG

 // Vector clear
 midi.vector = -1;

 // Sync interrupt if necessary synchronize
 IntCheck();
}

//---------------------------------------------------------------------------
//
//	Interrupt if necessary Data check
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::IntCheck()
{
 int i;
 int vector;
 DWORD bit;

 ASSERT(this);
 ASSERT_DIAG();

 // If interrupt if necessary mask control bit1=1 not, 68000 interrupt not match
 if ((midi.imr & 0x02) == 0) {
  // If interrupt if necessary has request then cancel
  if (midi.vector != -1) {
   cpu->IntCancel(midi.ilevel);
   midi.ivr &= 0xe0;
   midi.ivr |= 0x10;
   midi.vector = -1;
  }
  return;
 }

 // ISR Data check
 bit = 0x80;
 for (i=7; i>=0; i--) {
  if (midi.isr & bit) {
   if (midi.ier & bit) {
    // Interrupt if necessary Timer request, vector create
    vector = midi.ivr;
    vector &= 0xe0;
    vector += (i << 1);

    // Status matched, request
    if (midi.vector != vector) {
     // If has before request, cancel
     if (midi.vector >= 0) {
      cpu->IntCancel(midi.ilevel);
     }
     // Interrupt if necessary request
     cpu->Interrupt(midi.ilevel, vector);
     midi.vector = vector;
     midi.ivr = (DWORD)vector;

#if defined(MIDI_LOG)
     LOG3(Log::Normal, "Interrupt request Level%d Type%d Vector$%02X", midi.ilevel, i, vector);
#endif	// MIDI_LOG
    }
    return;
   }
  }

  // Next bit
  bit >>= 1;
 }

 // Timer has no interrupt if necessary, so cancel
 if (midi.vector != -1) {
  // Interrupt if necessary cancel
  cpu->IntCancel(midi.ilevel);
  midi.vector = -1;
  midi.ivr &= 0xe0;
  midi.ivr |= 0x10;
#if defined(MIDI_LOG)
  LOG0(Log::Normal, "Interrupt cancel");
#endif	// MIDI_LOG
 }
}

//---------------------------------------------------------------------------
//
//	Extension data get
//
//---------------------------------------------------------------------------
void FASTCALL MIDI::GetMIDI(midi_t *buffer) const
{
 ASSERT(this);
 ASSERT(buffer);
 ASSERT_DIAG();

 // Extension data copy
 *buffer = midi;
}

//---------------------------------------------------------------------------
//
//	Extension counter get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MIDI::GetExCount(int index) const
{
 ASSERT(this);
 ASSERT((index >= 0) && (index < 4));
 ASSERT_DIAG();

 return ex_cnt[index];
}