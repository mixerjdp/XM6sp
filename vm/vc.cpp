//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ Video Controller (CATHY & VIPS) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "schedule.h"
#include "fileio.h"
#include "sprite.h"
#include "render.h"
#include "renderin.h"
#include "vc.h"

//===========================================================================
//
//	Video Controller
//
//===========================================================================
//#define VC_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
VC::VC(VM *p) : MemDevice(p)
{
	// Device ID initialization
	dev.id = MAKEID('V', 'C', ' ', ' ');
	dev.desc = "VC (CATHY & VIPS)";

	// Start address, end address
	memdev.first = 0xe82000;
	memdev.last = 0xe83fff;

	// Other
	render = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization

//---------------------------------------------------------------------------
BOOL FASTCALL VC::Init()
{
	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get sprite controller
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);

	// Get renderer
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// Clear palette work (default $FF)
	memset(palette, 0xff, sizeof(palette));

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL VC::Cleanup()
{
	ASSERT(this);

	// Base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL VC::Reset()
{
	ASSERT(this);
	LOG0(Log::Normal, "Reset");

	// Clear video work
	memset(&vc, 0, sizeof(vc));

	// Ensure invalid ports return inverted value
	vc.vr1h = 0xff;
	vc.vr1l = 0xff;
	vc.vr2h = 0xff;
	vc.vr2l = 0xff;

	// Register 1(H) set
	vr1h = TRUE;

	// Register 2(H) set
	vr2h = TRUE;

	// Notify renderer
	HSync();
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL VC::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Save");

	// Save size
	sz = sizeof(vc_t);
	if (!fio->Write(&sz, (int)sizeof(sz))) {
		return FALSE;
	}

	// Save body
	if (!fio->Write(&vc, (int)sz)) {
		return FALSE;
	}

	// Save palette
	if (!fio->Write(palette, sizeof(palette))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL VC::Load(Fileio *fio, int /*ver*/)
{
	size_t sz;
	DWORD addr;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Load");

	// Load size and verify
	if (!fio->Read(&sz, (int)sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(vc_t)) {
		return FALSE;
	}

	// Load body
	if (!fio->Read(&vc, (int)sz)) {
		return FALSE;
	}

	// Load palette
	if (!fio->Read(palette, sizeof(palette))) {
		return FALSE;
	}

	// Notify renderer
	render->SetVC();
	for (addr=0; addr<0x200; addr++) {
		render->SetPalette(addr);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL VC::ApplyCfg(const Config *config)
{
	ASSERT(config);
	LOG0(Log::Normal, "Apply config");

	UNREFERENCED_PARAMETER(config);
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Loop in $1000 units
	addr &= 0xfff;

	// Decode
	if (addr < 0x400) {
		// Palette area
		scheduler->Wait(2);

		// add
		addr ^= 1;
		return palette[addr];
	}

	// �r�f�I�R���g���[�����W�X�^
	if (addr < 0x500) {
		if (addr & 1) {
			return (BYTE)GetVR0();
		}
		else {
			return (GetVR0() >> 8);
		}
	}
	if (addr < 0x600) {
		if (addr & 1) {
			return (BYTE)GetVR1();
		}
		else {
			return (GetVR1() >> 8);
		}
	}
	if (addr < 0x700) {
		if (addr & 1) {
			return (BYTE)GetVR2();
		}
		else {
			return (GetVR2() >> 8);
		}
	}

	// Undecoded area returns 0
	return 0;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	// Loop in $1000 units
	addr &= 0xfff;

	// Decode
	if (addr < 0x400) {
		// Wait
		scheduler->Wait(2);

		// Palette
		return *(WORD *)(&palette[addr]);
	}

	// Video control register
	if (addr < 0x500) {
		return GetVR0();
	}
	if (addr < 0x600) {
		return GetVR1();
	}
	if (addr < 0x700) {
		return GetVR2();
	}

	// Undecoded area returns 0
	return 0;
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL VC::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);

#if defined(VC_LOG)
	if ((addr & 0xfff) >= 0x400) {
		LOG2(Log::Normal, "VC write %08X <- %02X", addr, data);
	}
#endif	// VC_LOG

	// Loop in $1000 units
	addr &= 0xfff;

	// Decode
	if (addr < 0x400) {
		// Wait
		scheduler->Wait(2);

		// Palette area
		addr ^= 1;

		// Compare
		if (palette[addr] != data) {
			palette[addr] = (BYTE)data;

			// Notify renderer
			render->SetPalette(addr >> 1);
			render->VCtrlWrite(0x00e82000 + (addr ^ 1), (BYTE)data);
		}
		return;
	}

	// �r�f�I�R���g���[�����W�X�^
	if (addr < 0x500) {
		if (addr & 1) {
			SetVR0L(data);
			render->VCtrlWrite(0x00e82000 + addr, (BYTE)data);
		}
		return;
	}
	if (addr < 0x600) {
		if (addr & 1) {
			SetVR1L(data);
			render->VCtrlWrite(0x00e82000 + addr, (BYTE)data);
		}
		else {
			SetVR1H(data);
			render->VCtrlWrite(0x00e82000 + addr, (BYTE)data);
		}
		return;
	}
	if (addr < 0x700) {
		if (addr & 1) {
			SetVR2L(data);
			render->VCtrlWrite(0x00e82000 + addr, (BYTE)data);
		}
		else {
			SetVR2H(data);
			render->VCtrlWrite(0x00e82000 + addr, (BYTE)data);
		}
		return;
	}

	// ����ȊO�̓f�R�[�h����Ă��Ȃ�
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL VC::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

#if defined(VC_LOG)
	if ((addr & 0xfff) >= 0x400) {
		LOG2(Log::Normal, "VC write %08X <- %04X", addr, data);
	}
#endif	// VC_LOG

	// Loop in $1000 units
	addr &= 0xfff;

	// Decode
	if (addr < 0x400) {
		// Wait
		scheduler->Wait(2);

		// Palette area

		// Compare
		if (data != *(WORD*)(&palette[addr])) {
			*(WORD *)(&palette[addr]) = (WORD)data;

			// Notify renderer
			render->SetPalette(addr >> 1);
			render->VCtrlWrite(0x00e82000 + addr, (BYTE)((data >> 8) & 0xff));
			render->VCtrlWrite(0x00e82000 + addr + 1, (BYTE)(data & 0xff));
		}
		return;
	}

	// Video control register
	if (addr < 0x500) {
		SetVR0L((BYTE)data);
		render->VCtrlWrite(0x00e82000 + addr + 1, (BYTE)(data & 0xff));
		return;
	}
	if (addr < 0x600) {
		SetVR1L((BYTE)data);
		SetVR1H(data >> 8);
		render->VCtrlWrite(0x00e82000 + addr, (BYTE)((data >> 8) & 0xff));
		render->VCtrlWrite(0x00e82000 + addr + 1, (BYTE)(data & 0xff));
		return;
	}
	if (addr < 0x700) {
		SetVR2L((BYTE)data);
		SetVR2H(data >> 8);
		render->VCtrlWrite(0x00e82000 + addr, (BYTE)((data >> 8) & 0xff));
		render->VCtrlWrite(0x00e82000 + addr + 1, (BYTE)(data & 0xff));
		return;
	}

	// Other areas are not decoded
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Loop in $1000 units
	addr &= 0xfff;

	// Decode
	if (addr < 0x400) {
		// Wait
		addr ^= 1;
		return palette[addr];
	}

	// �r�f�I�R���g���[�����W�X�^
	if (addr < 0x500) {
		if (addr & 1) {
			return (BYTE)GetVR0();
		}
		else {
			return (GetVR0() >> 8);
		}
	}
	if (addr < 0x600) {
		if (addr & 1) {
			return (BYTE)GetVR1();
		}
		else {
			return (GetVR1() >> 8);
		}
	}
	if (addr < 0x700) {
		if (addr & 1) {
			return (BYTE)GetVR2();
		}
		else {
			return (GetVR2() >> 8);
		}
	}

	// Undecoded area returns 0
	return 0;
}

//---------------------------------------------------------------------------
//
//	Get work data
//
//---------------------------------------------------------------------------
void FASTCALL VC::GetVC(vc_t *buffer)
{
	ASSERT(this);
	ASSERT(buffer);

	// Copy work memory
	*buffer = vc;
}

//---------------------------------------------------------------------------
//
//	Video register 0(L) set
//
//---------------------------------------------------------------------------
void FASTCALL VC::SetVR0L(DWORD data)
{
	BOOL siz;
	DWORD col;

	ASSERT(this);
	ASSERT(data < 0x100);

	// Backup
	siz = vc.siz;
	col = vc.col;

	// Set
	if (data & 4) {
		vc.siz = TRUE;
	}
	else {
		vc.siz = FALSE;
	}
	vc.col = (data & 3);

	// Compare
	if ((vc.siz != siz) || (vc.col != col)) {
		render->SetVC();
	}
}

//---------------------------------------------------------------------------
//
//	Get video register 0
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::GetVR0() const
{
	DWORD data;

	ASSERT(this);

	data = 0;
	if (vc.siz) {
		data |= 0x04;
	}
	data |= vc.col;

	return data;
}

//---------------------------------------------------------------------------
//
//	Video register 1(H) set
//
//---------------------------------------------------------------------------
void FASTCALL VC::SetVR1H(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	data &= 0x3f;

	// Compare
	if (vc.vr1h == data) {
		return;
	}
	vc.vr1h = data;

	// Flag update
	vr1h = TRUE;
}

//---------------------------------------------------------------------------
//
//	Video register 1(L) set
//
//---------------------------------------------------------------------------
void FASTCALL VC::SetVR1L(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	// Compare
	if (vc.vr1l == data) {
		return;
	}
	vc.vr1l = data;

	vc.gp[0] = (data & 3);
	data >>= 2;
	vc.gp[1] = (data & 3);
	data >>= 2;
	vc.gp[2] = (data & 3);
	data >>= 2;
	vc.gp[3] = (data & 3);

	// Notify
	render->SetVC();
}

//---------------------------------------------------------------------------
//
//	Get video register 1
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::GetVR1() const
{
	ASSERT(this);

	return (vc.vr1h << 8) | vc.vr1l;
}

//---------------------------------------------------------------------------
//
//	Video register 2(H) set
//
//---------------------------------------------------------------------------
void FASTCALL VC::SetVR2H(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	// Data compare
	if (vc.vr2h == data) {
		return;
	}
	vc.vr2h = data;

	// Flag update
	vr2h = TRUE;
}

//---------------------------------------------------------------------------
//
//	Video register 2(L) set
//
//---------------------------------------------------------------------------
void FASTCALL VC::SetVR2L(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	// Compare
	if (vc.vr2l == data) {
		return;
	}
	vc.vr2l = data;

	// BCON
	if (data & 0x80) {
		vc.bcon = TRUE;
	}
	else {
		vc.bcon = FALSE;
	}

	// SON
	if (data & 0x40) {
		vc.son = TRUE;
	}
	else {
		vc.son = FALSE;
	}

	// TON
	if (data & 0x20) {
		vc.ton = TRUE;
	}
	else {
		vc.ton = FALSE;
	}

	// GON
	if (data & 0x10) {
		vc.gon = TRUE;
	}
	else {
		vc.gon = FALSE;
	}

	// GS[3]
	if (data & 0x08) {
		vc.gs[3] = TRUE;
	}
	else {
		vc.gs[3] = FALSE;
	}

	// GS[2]
	if (data & 0x04) {
		vc.gs[2] = TRUE;
	}
	else {
		vc.gs[2] = FALSE;
	}

	// GS[1]
	if (data & 0x02) {
		vc.gs[1] = TRUE;
	}
	else {
		vc.gs[1] = FALSE;
	}

	// GS[0]
	if (data & 0x01) {
		vc.gs[0] = TRUE;
	}
	else {
		vc.gs[0] = FALSE;
	}

	// Notify
	render->SetVC();
}

//---------------------------------------------------------------------------
//
//	Get video register 2
//
//---------------------------------------------------------------------------
DWORD FASTCALL VC::GetVR2() const
{
	ASSERT(this);

	// Upper byte is delayed but before the delay
	// READ is possible, so generate from register value
	// (For StarLuster cockpit kit)
	return (vc.vr2h << 8) | vc.vr2l;
}

//---------------------------------------------------------------------------
//
//	H-Sync notification
//
//---------------------------------------------------------------------------
void FASTCALL VC::HSync()
{
	DWORD data;

	ASSERT(this);

	// vr1h change detection
	if (vr1h) {
		// Flag off
		vr1h = FALSE;

		data = vc.vr1h;

		vc.gr = (data & 3);
		data >>= 2;
		vc.tx = (data & 3);
		data >>= 2;
		vc.sp = data;

		// Notify
		render->SetVC();
	}

	// vr2h change detection
	if (vr2h) {
		// Flag off
		vr2h = FALSE;

		data = vc.vr2h;

		// YS
		if (data & 0x80) {
			vc.ys = TRUE;
		}
		else {
			vc.ys = FALSE;
		}

		// AH
		if (data & 0x40) {
			vc.ah = TRUE;
		}
		else {
			vc.ah = FALSE;
		}

		// VHT
		if (data & 0x20) {
			vc.vht = TRUE;
		}
		else {
			vc.vht = FALSE;
		}

		// EXON
		if (data & 0x10) {
			vc.exon = TRUE;
		}
		else {
			vc.exon = FALSE;
		}

		// H/P
		if (data & 0x08) {
			vc.hp = TRUE;
		}
		else {
			vc.hp = FALSE;
		}

		// B/P
		if (data & 0x04) {
			vc.bp = TRUE;
		}
		else {
			vc.bp = FALSE;
		}

		// G/G
		if (data & 0x02) {
			vc.gg = TRUE;
		}
		else {
			vc.gg = FALSE;
		}

		// G/T
		if (data & 0x01) {
			vc.gt = TRUE;
		}
		else {
			vc.gt = FALSE;
		}

		// Notify
		render->SetVC();
	}
}
