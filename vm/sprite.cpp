//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ Sprite (CYNTHIA) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "fileio.h"
#include "render.h"
#include "sprite.h"

//===========================================================================
//
//	Sprite
//
//===========================================================================
//#define SPRITE_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Sprite::Sprite(VM *p) : MemDevice(p)
{
	// Device ID initialization
	dev.id = MAKEID('S', 'P', 'R', ' ');
	dev.desc = "Sprite (CYNTHIA)";

	// Start address, end address
	memdev.first = 0xeb0000;
	memdev.last = 0xebffff;

	// Others
	sprite = NULL;
	render = NULL;
	spr.mem = NULL;
	spr.pcg = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL Sprite::Init()
{
ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Memory allocation, initialization
	try {
		sprite = new BYTE[ 0x10000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!sprite) {
		return FALSE;
	}

	// EB0400-EB07FF, EB0812-EB7FFF Reserved(FF)
	memset(sprite, 0, 0x10000);
	memset(&sprite[0x400], 0xff, 0x400);
	memset(&sprite[0x812], 0xff, 0x77ee);

	// Structure initialization
	memset(&spr, 0, sizeof(spr));
	spr.mem = &sprite[0x0000];
	spr.pcg = &sprite[0x8000];

	// Render get
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
ASSERT(render);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::Cleanup()
{
	// Memory release
	if (sprite) {
		delete[] sprite;
		sprite = NULL;
		spr.mem = NULL;
		spr.pcg = NULL;
	}

	// Base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::Reset()
{
	int i;

ASSERT(this);
LOG0(Log::Normal, "Reset");

	// Register setting
	spr.connect = FALSE;
	spr.disp = FALSE;

	// BG page initialize
	for (i=0; i<2; i++) {
		spr.bg_on[i] = FALSE;
		spr.bg_area[i] = 0;
		spr.bg_scrlx[i] = 0;
		spr.bg_scrly[i] = 0;
	}

	// BG size initialize
	spr.bg_size = FALSE;

	// Timer initialize
	spr.h_total = 0;
	spr.h_disp = 0;
	spr.v_disp = 0;
	spr.lowres = FALSE;
	spr.v_res = 0;
	spr.h_res = 0;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Sprite::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

ASSERT(this);
ASSERT(fio);
ASSERT(spr.mem);

LOG0(Log::Normal, "Save");

	// Structure size save
	sz = sizeof(sprite_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Data save
	if (!fio->Write(&spr, (int)sz)) {
		return FALSE;
	}

	// Memory save
	if (!fio->Write(sprite, 0x10000)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Sprite::Load(Fileio *fio, int /*ver*/)
{
	size_t sz;
	int i;
	DWORD addr;
	DWORD data;

ASSERT(this);
ASSERT(fio);
ASSERT(spr.mem);

LOG0(Log::Normal, "Load");

	// Structure size load, compare
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(sprite_t)) {
		return FALSE;
	}

	// Data load
	if (!fio->Read(&spr, (int)sz)) {
		return FALSE;
	}

	// Memory load
	if (!fio->Read(sprite, 0x10000)) {
		return FALSE;
	}

	// Pointer update
	spr.mem = &sprite[0x0000];
	spr.pcg = &sprite[0x8000];

	// Render register notify (register)
	render->BGCtrl(4, spr.bg_size);
	for (i=0; i<2; i++) {
		// BG data area
		if (spr.bg_area[i] & 1) {
			render->BGCtrl(i + 2, TRUE);
		}
		else {
			render->BGCtrl(i + 2, FALSE);
		}

		// BG display ON/OFF
		render->BGCtrl(i, spr.bg_on[i]);

		// BG scroll
		render->BGScrl(i, spr.bg_scrlx[i], spr.bg_scrly[i]);
	}

	// Render register notify (memory: only address)
	for (addr=0; addr<0x10000; addr+=2) {
		if (addr < 0x400) {
			data = *(WORD*)(&sprite[addr]);
			render->SpriteReg(addr, data);
			continue;
		}
		if (addr < 0x8000) {
			continue;
		}
		if (addr >= 0xc000) {
			data = *(WORD*)(&sprite[addr]);
			render->BGMem(addr, (WORD)data);
		}
		render->PCGMem(addr);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::ApplyCfg(const Config *config)
{
ASSERT(config);
LOG0(Log::Normal, "Apply configuration");
printf("%p", (const void*)config);
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL Sprite::ReadByte(DWORD addr)
{
ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Offset calculate
	addr &= 0xffff;

	// 0800~7FFF is not affected by bus error
	if ((addr >= 0x800) && (addr < 0x8000)) {
		return sprite[addr ^ 1];
	}

	// Connect check
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xff;
	}

	// Wait (bus access time)
	if (addr & 1) {
		if (spr.disp) {
			scheduler->Wait(4);
		}
		else {
			scheduler->Wait(2);
		}
	}

	// Endian convert and read
	return sprite[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL Sprite::ReadWord(DWORD addr)
{
ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT((addr & 1) == 0);

	// Offset calculate
	addr &= 0xffff;

	// 0800~7FFF is not affected by bus error
	if ((addr >= 0x800) && (addr < 0x8000)) {
		return *(WORD *)(&sprite[addr]);
	}

	// Connect check
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xff;
	}

	// Wait (bus access time)
	if (spr.disp) {
		scheduler->Wait(4);
	}
	else {
		scheduler->Wait(2);
	}

	// Read
	return *(WORD *)(&sprite[addr]);
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::WriteByte(DWORD addr, DWORD data)
{
	DWORD ctrl;

ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT(data < 0x100);

	// Offset calculate
	addr &= 0xffff;

	// Equal check
	if (sprite[addr ^ 1] == data) {
		return;
	}

	// 800~811 is control register
	if ((addr >= 0x800) && (addr < 0x812)) {
		// Data set
		sprite[addr ^ 1] = (BYTE)data;

		if (addr & 1) {
			// Odd access. Align with even and call control
			ctrl = (DWORD)sprite[addr];
			ctrl <<= 8;
			ctrl |= data;
			Control((DWORD)(addr & 0xfffe), ctrl);
		}
		else {
			// Even access. Align with odd and call control
			ctrl = data;
			ctrl <<= 8;
			ctrl |= (DWORD)sprite[addr];
			Control(addr, ctrl);
		}
		return;
	}

	// 0812-7FFF is unused (not affected by bus error)
	if ((addr >= 0x812) && (addr < 0x8000)) {
		return;
	}

	// Connect check
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, FALSE);
		return;
	}

	// Wait (bus access time)
	if (addr & 1) {
		if (spr.disp) {
			scheduler->Wait(4);
		}
		else {
			scheduler->Wait(2);
		}
	}

	// 0400-07FF is unused (not affected by bus error)
	if ((addr >= 0x400) && (addr < 0x800)) {
		return;
	}

	// Memory set
	sprite[addr ^ 1] = (BYTE)data;

	// Render notify
	addr &= 0xfffe;
	if (addr < 0x400) {
		ctrl = *(WORD*)(&sprite[addr]);
		render->SpriteReg(addr, ctrl);
		return;
	}
	if (addr >= 0x8000) {
		render->PCGMem(addr);
	}
	if (addr >= 0xc000) {
		ctrl = *(WORD*)(&sprite[addr]);
		render->BGMem(addr, (WORD)ctrl);
	}
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::WriteWord(DWORD addr, DWORD data)
{
ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT((addr & 1) == 0);
ASSERT(data < 0x10000);

	// Offset calculate
	addr &= 0xfffe;

	// Equal check
	if (*(WORD *)(&sprite[addr]) == data) {
		return;
	}

	// 800~811 is control register
	if ((addr >= 0x800) && (addr < 0x812)) {
		*(WORD *)(&sprite[addr]) = (WORD)data;
		Control(addr, data);
		return;
	}
	// 0812-7FFF is unused (not affected by bus error)
	if ((addr >= 0x812) && (addr < 0x8000)) {
		return;
	}

	// Wait (bus access time)
	if (spr.disp) {
		scheduler->Wait(4);
	}
	else {
		scheduler->Wait(2);
	}

	// 0400-07FF is unused (not affected by bus error)
	if ((addr >= 0x400) && (addr < 0x800)) {
		return;
	}

	// Memory set
	*(WORD *)(&sprite[addr]) = (WORD)data;

	// Render notify
	if (addr < 0x400) {
		render->SpriteReg(addr, data);
		return;
	}
	if (addr >= 0x8000) {
		render->PCGMem(addr);
	}
	if (addr >= 0xc000) {
		render->BGMem(addr, (WORD)data);
	}
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL Sprite::ReadOnly(DWORD addr) const
{
ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Offset calculate
	addr &= 0xffff;

	// Endian convert and read
	return sprite[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	Control
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::Control(DWORD addr, DWORD data)
{
ASSERT((addr >= 0x800) && (addr < 0x812));
ASSERT((addr & 1) == 0);
ASSERT(data < 0x10000);

	// Address convert
	addr -= 0x800;
	addr >>= 1;

	switch (addr) {
		// BG0 scroll X
		case 0:
			spr.bg_scrlx[0] = data & 0x3ff;
			render->BGScrl(0, spr.bg_scrlx[0], spr.bg_scrly[0]);
			break;

		// BG0 scroll Y
		case 1:
			spr.bg_scrly[0] = data & 0x3ff;
			render->BGScrl(0, spr.bg_scrlx[0], spr.bg_scrly[0]);
			break;

		// BG1 scroll X
		case 2:
			spr.bg_scrlx[1] = data & 0x3ff;
			render->BGScrl(1, spr.bg_scrlx[1], spr.bg_scrly[1]);
			break;

		// BG1 scroll Y
		case 3:
			spr.bg_scrly[1] = data & 0x3ff;
			render->BGScrl(1, spr.bg_scrlx[1], spr.bg_scrly[1]);
			break;

		// BG control
		case 4:
#if defined(SPRITE_LOG)
			LOG1(Log::Normal, "BG control $%04X", data);
#endif	// SPRITE_LOG
			// bit17 : DISP
			if (data & 0x0200) {
				spr.disp = TRUE;
			}
			else {
				spr.disp = FALSE;
			}

			// BG1
			spr.bg_area[1] = (data >> 4) & 0x03;
			if (spr.bg_area[1] & 2) {
				LOG1(Log::Warning, "BG1 data area undefined $%02X", spr.bg_area[1]);
			}
			if (spr.bg_area[1] & 1) {
				render->BGCtrl(3, TRUE);
			}
			else {
				render->BGCtrl(3, FALSE);
			}
			if (data & 0x08) {
				spr.bg_on[1] = TRUE;
			}
			else {
				spr.bg_on[1] = FALSE;
			}
			render->BGCtrl(1, spr.bg_on[1]);

			// BG0
			spr.bg_area[0] = (data >> 1) & 0x03;
			if (spr.bg_area[0] & 2) {
				LOG1(Log::Warning, "BG0 data area undefined $%02X", spr.bg_area[0]);
			}
			if (spr.bg_area[0] & 1) {
				render->BGCtrl(2, TRUE);
			}
			else {
				render->BGCtrl(2, FALSE);
			}
			if (data & 0x01) {
				spr.bg_on[0] = TRUE;
			}
			else {
				spr.bg_on[0] = FALSE;
			}
			render->BGCtrl(0, spr.bg_on[0]);
			break;

		// Horizontal total
		case 5:
			spr.h_total = data & 0xff;
			break;

		// Horizontal display
		case 6:
			spr.h_disp = data & 0x3f;
			break;

		// Vertical display
		case 7:
			spr.v_disp = data & 0xff;
			break;

		// Display mode
		case 8:
			spr.h_res = data & 0x03;
			spr.v_res = (data >> 2) & 0x03;

			// 15kHz
			if (data & 0x10) {
				spr.lowres = FALSE;
			}
			else {
				spr.lowres = TRUE;
			}

			// BG size
			if (spr.h_res == 0) {
				// 8x8
				spr.bg_size = FALSE;
			}
			else {
				// 16x16
				spr.bg_size = TRUE;
			}
			render->BGCtrl(4, spr.bg_size);
			if (spr.h_res & 2) {
				LOG1(Log::Warning, "BG/Sprite H-Resolution undefined %d", spr.h_res);
			}
			break;

		// Others
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Get sprite data
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::GetSprite(sprite_t *buffer) const
{
ASSERT(this);
ASSERT(buffer);

	// Structure copy
	*buffer = spr;
}

//---------------------------------------------------------------------------
//
//	Get BG area
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL Sprite::GetMem() const
{
ASSERT(this);
ASSERT(spr.mem);

	return spr.mem;
}

//---------------------------------------------------------------------------
//
//	Get PCG area
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL Sprite::GetPCG() const
{
ASSERT(this);
ASSERT(spr.pcg);

	return spr.pcg;
}