//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ Sprite (CYNTHIA) ]
//
//---------------------------------------------------------------------------

/*

Port status area
--------------------------------------------------------------------------

  In screen modes other than 512x512 or 256x256: bus error

    $eb0000 - $eb03ff Bus error
    $eb0400 - $eb07ff Bus error
    $eb0800 - $eb0811 Accessible
    $eb0812 - $eb7fff Returns $ff
    $eb8000 - $ebffff Bus error

  In 512x512 or 256x256 screen mode

    $eb0000 - $eb03ff Accessible
    $eb0400 - $eb07ff Returns $ff
    $eb0800 - $eb0811 Accessible
    $eb0812 - $eb7fff Returns $ff
    $eb8000 - $ebffff Accessible

  Word access / Byte access

    $eb0000 - $eb03ff R: word/byte  W: word
    $eb0400 - $eb07ff Returns $ff
    $eb0800 - $eb0811 R: word/byte  W: word/byte
    $eb0812 - $eb7fff Returns $ff
    $eb8000 - $ebffff R: word/byte  W: word

    When writing in byte-disabled mode...
      If the upper byte, the upper value is written to the lower;
      if the lower byte, the lower value is written to the upper.

Valid bits of registers
--------------------------------------------------------------------------
  Sprite scroll registers: PRW (priority) held in bit 2
  BG control ($eb0808): bit 10 held

Sprite scroll registers and PCG/BG buffer contents at reset
--------------------------------------------------------------------------
  Not cleared

*/

#include "os.h"
#include "xm6.h"
#include "vm.h"
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

	// Other
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
	int i;

	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Memory allocation, clear
	try {
		sprite = new BYTE[ 0x10000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!sprite) {
		return FALSE;
	}

	// EB0400-EB07FF, EB0812-EB7FFF are Reserved (FF)
	memset(sprite, 0, 0x10000);
	memset(&sprite[0x400], 0xff, 0x400);
	memset(&sprite[0x812], 0xff, 0x77ee);

	// Work initialization
	memset(&spr, 0, sizeof(spr));
	spr.mem = &sprite[0x0000];
	spr.pcg = &sprite[0x8000];

	// Get renderer
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// Sprite HSYNC schedule
	for (i=0; i<128; i++) {
		sphsync[i] = 3;
	}

	// BG HSYNC schedule
	bghsync = 0x1f;

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

	// Clear register area (EB0800-EB0811)
	memset(&sprite[0x800], 0, 0x12);

	// Register settings
	spr.connect = FALSE;
	spr.disp = FALSE;

	// BG page initialization
	for (i=0; i<2; i++) {
		spr.bg_on[i] = FALSE;
		spr.bg_area[i] = 0;
		spr.bg_scrlx[i] = 0;
		spr.bg_scrly[i] = 0;
	}

	// BG size initialization
	spr.bg_size = FALSE;

	// Timing initialization
	spr.h_total = 0;
	spr.h_disp = 0;
	spr.v_disp = 0;
	spr.lowres = FALSE;
	spr.v_res = 0;
	spr.h_res = 0;

	// Sprite HSYNC schedule
	for (i=0; i<128; i++) {
		sphsync[i] = 3;
	}

	// BG HSYNC schedule
	bghsync = 0x1f;
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

	// Save size
	sz = sizeof(sprite_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save body
	if (!fio->Write(&spr, (int)sz)) {
		return FALSE;
	}

	// Save memory
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

	ASSERT(this);
	ASSERT(fio);
	ASSERT(spr.mem);

	LOG0(Log::Normal, "Load");

	// Load size and verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(sprite_t)) {
		return FALSE;
	}

	// Load body
	if (!fio->Read(&spr, (int)sz)) {
		return FALSE;
	}

	// Load memory
	if (!fio->Read(sprite, 0x10000)) {
		return FALSE;
	}

	// Overwrite pointers
	spr.mem = &sprite[0x0000];
	spr.pcg = &sprite[0x8000];

	// Notify renderer
	NotifyRender();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::ApplyCfg(const Config *config)
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
DWORD FASTCALL Sprite::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Offset calculation
	addr &= 0xffff;

	// Wait (Ethernet emulation)
	if (addr & 1) {
		if (spr.disp) {
			scheduler->Wait(7);
		}
		else {
			scheduler->Wait(4);
		}
	}

	// 0800-7FFF is not affected by bus error
	if ((addr >= 0x800) && (addr < 0x8000)) {
		return sprite[addr ^ 1];
	}

	// Connection check
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xff;
	}

	// Read with endian reversal
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

	// Offset calculation
	addr &= 0xffff;

	// Wait (Ethernet emulation)
	if (spr.disp) {
		scheduler->Wait(7);
	}
	else {
		scheduler->Wait(4);
	}

	// 0800-7FFF is not affected by bus error
	if ((addr >= 0x800) && (addr < 0x8000)) {
		return *(WORD *)(&sprite[addr]);
	}

	// Connection check
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xffff;
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
	if (render && (render->GetCompositorMode() == Render::compositor_fast)) {
			DWORD ctrl;

			ASSERT(this);
			ASSERT((addr >= memdev.first) && (addr <= memdev.last));
			ASSERT(data < 0x100);

			addr &= 0xffff;

			if (sprite[addr ^ 1] == data) {
				return;
			}

			if ((addr >= 0x800) && (addr < 0x812)) {
				sprite[addr ^ 1] = (BYTE)data;
				addr &= 0xfffe;
				ctrl = *(WORD *)(&sprite[addr]);
				Control(addr, ctrl);
				NotifyPx68kBGWrite(addr, (WORD)ctrl);
				return;
			}

			if ((addr >= 0x812) && (addr < 0x8000)) {
				return;
			}

			if (!IsConnect()) {
				cpu->BusErr(memdev.first + addr, FALSE);
				return;
			}

			if (addr & 1) {
				if (spr.disp) {
					scheduler->Wait(4);
				}
				else {
					scheduler->Wait(2);
				}
			}

			if ((addr >= 0x400) && (addr < 0x800)) {
				return;
			}

			sprite[addr ^ 1] = (BYTE)data;
			addr &= 0xfffe;

			if (addr < 0x400) {
				ctrl = *(WORD *)(&sprite[addr]);
				render->SpriteReg(addr, ctrl);
				NotifyPx68kBGWrite(addr, (WORD)ctrl);
				return;
			}
			if (addr >= 0x8000) {
				render->PCGMem(addr);
			}
			if (addr >= 0xc000) {
				ctrl = *(WORD *)(&sprite[addr]);
				render->BGMem(addr, (WORD)ctrl);
			}
			if (addr >= 0x8000) {
				ctrl = *(WORD *)(&sprite[addr]);
				NotifyPx68kBGWrite(addr, (WORD)ctrl);
			}
		return;
	}
	DWORD base;
	DWORD reg[4];

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);

	// Offset calculation
	addr &= 0xffff;

	// Wait (Ethernet emulation)
	if (spr.disp) {
		scheduler->Wait(7);
	}
	else {
		scheduler->Wait(3);
	}

	// 800-811 are control registers
	if ((addr >= 0x800) && (addr < 0x812)) {
		// Match check
		if (sprite[addr ^ 1] == data) {
			return;
		}

		// Data update
		sprite[addr ^ 1] = (BYTE)data;

		// Align to word boundary
		addr &= 0xfffe;
		data = *(WORD *)(&sprite[addr]);

		// Register mask
		switch (addr & 0xff) {
			// BG scroll register
			case 0x00:
			case 0x02:
			case 0x04:
			case 0x06:
				data &= 0x03ff;
				break;

			// BG control (bit10 valid)
			case 0x08:
				data &= 0x063f;
				break;

			// Horizontal total
			case 0x0a:
				data &= 0x00ff;
				break;

			// Horizontal display
			case 0x0c:
				data &= 0x003f;
				break;

			// Vertical display
			case 0x0e:
				data &= 0x00ff;
				break;

			// Resolution setting
			case 0x10:
				data &= 0x001f;
				break;
		}

		// Mask data update
		*(WORD *)(&sprite[addr]) = (WORD)data;

		// �R���g���[��
		Control(addr, data);
		NotifyPx68kBGWrite(addr, (WORD)data);
		return;
	}

	// 0812-7FFF is reserved (not affected by bus error)
	if ((addr >= 0x812) && (addr < 0x8000)) {
		return;
	}

	// Connection check
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, FALSE);
		return;
	}

	// 0400-07FF is reserved (affected by bus error)
	if ((addr >= 0x400) && (addr < 0x800)) {
		return;
	}

	// 0000-03FF and 8000-FFFF cannot be byte-accessed
	// Becomes word access with the same value in upper/lower byte
	sprite[addr ^ 1] = (BYTE)data;
	addr &= 0xfffe;

	if (addr < 0x400) {
		base = addr & 0xfff8;
		reg[0] = *(WORD*)(&sprite[base    ]);
		reg[1] = *(WORD*)(&sprite[base + 2]);
		reg[2] = *(WORD*)(&sprite[base + 4]);
		reg[3] = *(WORD*)(&sprite[base + 6]);
		render->SpriteReg(base, reg);
		NotifyPx68kBGWrite(addr, (WORD)*(WORD*)(&sprite[addr]));
		return;
	}

	render->PCGMem(addr);

	if (addr >= 0xc000) {
		data = *(WORD*)(&sprite[addr]);
		render->BGMem(addr, (WORD)data);
	}
	if (addr >= 0x8000) {
		data = *(WORD*)(&sprite[addr]);
		NotifyPx68kBGWrite(addr, (WORD)data);
	}

}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::WriteWord(DWORD addr, DWORD data)
{
	if (render && (render->GetCompositorMode() == Render::compositor_fast)) {
			ASSERT(this);
			ASSERT((addr >= memdev.first) && (addr <= memdev.last));
			ASSERT((addr & 1) == 0);
			ASSERT(data < 0x10000);

			addr &= 0xfffe;

			if (*(WORD *)(&sprite[addr]) == data) {
				return;
			}

			if ((addr >= 0x800) && (addr < 0x812)) {
				*(WORD *)(&sprite[addr]) = (WORD)data;
				Control(addr, data);
				NotifyPx68kBGWrite(addr, (WORD)data);
				return;
			}
			if ((addr >= 0x812) && (addr < 0x8000)) {
				return;
			}

			if (!IsConnect()) {
				cpu->BusErr(memdev.first + addr, FALSE);
				return;
			}

			if (spr.disp) {
				scheduler->Wait(4);
			}
			else {
				scheduler->Wait(2);
			}

			if ((addr >= 0x400) && (addr < 0x800)) {
				return;
			}

			*(WORD *)(&sprite[addr]) = (WORD)data;

			if (addr < 0x400) {
				render->SpriteReg(addr, data);
				NotifyPx68kBGWrite(addr, (WORD)data);
				return;
			}
			if (addr >= 0x8000) {
				render->PCGMem(addr);
			}
			if (addr >= 0xc000) {
				render->BGMem(addr, (WORD)data);
			}
			if (addr >= 0x8000) {
				NotifyPx68kBGWrite(addr, (WORD)data);
			}
		return;
	}
	int index;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	// Offset calculation
	addr &= 0xfffe;

	// Wait (Ethernet emulation)
	if (spr.disp) {
		scheduler->Wait(7);
	}
	else {
		scheduler->Wait(3);
	}

	// 800-811 are control registers
	if ((addr >= 0x800) && (addr < 0x812)) {
		// Match check
		if (*(WORD *)(&sprite[addr]) == data) {
			return;
		}

		// Register mask
		switch (addr & 0xff) {
			// BG scroll register
			case 0x00:
			case 0x02:
			case 0x04:
			case 0x06:
				data &= 0x03ff;
				break;

			// BG control (bit10 valid)
			case 0x08:
				data &= 0x063f;
				break;

			// Horizontal total
			case 0x0a:
				data &= 0x00ff;
				break;

			// Horizontal display
			case 0x0c:
				data &= 0x003f;
				break;

			// Vertical display
			case 0x0e:
				data &= 0x00ff;
				break;

			// Resolution setting
			case 0x10:
				data &= 0x001f;
				break;
		}

		// Mask data update
		*(WORD *)(&sprite[addr]) = (WORD)data;

		// Control
		Control(addr, data);
		NotifyPx68kBGWrite(addr, (WORD)data);
		return;
	}

	// 0812-7FFF is reserved (not affected by bus error)
	if ((addr >= 0x812) && (addr < 0x8000)) {
		return;
	}

	// Connection check
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, FALSE);
		return;
	}

	// 0400-07FF is reserved (affected by bus error)
	if ((addr >= 0x400) && (addr < 0x800)) {
		return;
	}

	// Match check
	if (*(WORD *)(&sprite[addr]) == data) {
		return;
	}

	// Sprite scroll register
	if (addr < 0x400) {
		// Register mask
		switch (addr & 0x07) {
			// XPOS, YPOS
			case 0x00:
			case 0x02:
				data &= 0x03ff;
				break;

			// VR|HR|COLOR|SPAT#
			case 0x04:
				data &= 0xcfff;
				break;

			// PRW (bit2 valid)
			case 0x06:
				data &= 0x0007;
				break;
		}

		// Mask data update
		*(WORD *)(&sprite[addr]) = (WORD)data;

		// Sprite scroll registers are written directly
		// Effect is reflected after 3 scanlines
		index = (int)(addr >> 3);
		if (sphsync[index]==0) {
			sphsync[index]=3;

			// BG HSYNC schedule
			bghsync |= 0x10;
		}
		NotifyPx68kBGWrite(addr, (WORD)data);
		return;
	}

	// Other (write directly to memory)
	*(WORD *)(&sprite[addr]) = (WORD)data;

	if (addr >= 0x8000) {
		render->PCGMem(addr);
	}

	if (addr >= 0xc000) {
		render->BGMem(addr, (WORD)data);
	}
	if (addr >= 0x8000) {
		NotifyPx68kBGWrite(addr, (WORD)data);
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

	// Offset calculation
	addr &= 0xffff;

	// Read with endian reversal
	return sprite[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	Control
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::Control(DWORD addr, DWORD data)
{
	if (render && (render->GetCompositorMode() == Render::compositor_fast)) {
			ASSERT((addr >= 0x800) && (addr < 0x812));
			ASSERT((addr & 1) == 0);
			ASSERT(data < 0x10000);

			// Normalize address
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
						if (!spr.disp) {
							spr.disp = TRUE;
							render->BGCtrl(5, TRUE);
						}
					}
					else {
						if (spr.disp) {
							spr.disp = FALSE;
							render->BGCtrl(5, FALSE);
						}
					}

					// BG1
					spr.bg_area[1] = (data >> 4) & 0x03;
					if (spr.bg_area[1] & 2) {
						LOG1(Log::Warning, "BG1 data area invalid $%02X", spr.bg_area[1]);
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
						LOG1(Log::Warning, "BG0 data area invalid $%02X", spr.bg_area[0]);
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

				// Screen mode
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
						LOG1(Log::Warning, "BG/Sprite H-Res invalid %d", spr.h_res);
					}
					break;

	// Other
				default:
					ASSERT(FALSE);
					break;
			}

			// Notify renderer for display position update
			if (addr > 4) {
				render->SetCRTC();
			}
		return;
	}
	ASSERT((addr >= 0x800) && (addr < 0x812));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	// Normalize address
	addr -= 0x800;
	addr >>= 1;

	switch (addr) {
		// BG0 scroll X
		case 0:
			spr.bg_scrlx[0] = data & 0x3ff;
			bghsync |= 0x01;
			break;

		// BG0 scroll Y
		case 1:
			spr.bg_scrly[0] = data & 0x3ff;
			bghsync |= 0x02;
			break;

		// BG1 scroll X
		case 2:
			spr.bg_scrlx[1] = data & 0x3ff;
			bghsync |= 0x04;
			break;

		// BG1 scroll Y
		case 3:
			spr.bg_scrly[1] = data & 0x3ff;
			bghsync |= 0x08;
			break;

		// BG control
		case 4:
#if defined(SPRITE_LOG)
			LOG1(Log::Normal, "BG control $%04X", data);
#endif	// SPRITE_LOG
			// bit17 : DISP
			if (data & 0x0200) {
				if (!spr.disp) {
					spr.disp = TRUE;
					render->BGCtrl(5, TRUE);
				}
			}
			else {
				if (spr.disp) {
					spr.disp = FALSE;
					render->BGCtrl(5, FALSE);
				}
			}

			// BG1
			spr.bg_area[1] = (data >> 4) & 0x03;
			if (spr.bg_area[1] & 2) {
				LOG1(Log::Warning, "BG1 data area invalid $%02X", spr.bg_area[1]);
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
				LOG1(Log::Warning, "BG0 data area invalid $%02X", spr.bg_area[0]);
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

		// Screen mode
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
				LOG1(Log::Warning, "BG/Sprite H-Res invalid %d", spr.h_res);
			}
			break;

	// Other
	default:
			ASSERT(FALSE);
			break;
	}

	// Notify renderer for display position update
	if (addr > 4) {
		render->SetCRTC();
	}

}

//---------------------------------------------------------------------------
//
//	Notify renderer
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::NotifyPx68kBGWrite(DWORD addr, WORD data)
{
	if (!render) {
		return;
	}

	addr = memdev.first + (addr & 0xffff);
	render->SpriteBGWrite(addr, (BYTE)((data >> 8) & 0xff));
	render->SpriteBGWrite(addr + 1, (BYTE)(data & 0xff));
}

void FASTCALL Sprite::NotifyRender()
{
	int i;
	DWORD addr;
	DWORD data;
	DWORD reg[4];

	// Register
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

		// BG size
		render->BGCtrl(4, spr.bg_size);

		// DISP/CPU
		render->BGCtrl(5, spr.disp);

		// BG scroll
		render->BGScrl(i, spr.bg_scrlx[i], spr.bg_scrly[i]);
	}

	// Bulk: addresses only
	for (addr=0; addr<0x10000; addr+=2) {
		if (addr < 0x400) {
			if ((addr & 7)==0) {
				reg[0] = *(WORD*)(&sprite[addr  ]);
				reg[1] = *(WORD*)(&sprite[addr+2]);
				reg[2] = *(WORD*)(&sprite[addr+4]);
				reg[3] = *(WORD*)(&sprite[addr+6]);
				render->SpriteReg(addr, reg);
			}
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

	// Copy work memory
	*buffer = spr;
}

//---------------------------------------------------------------------------
//
//	Get work area
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

//---------------------------------------------------------------------------
//
//	H-Sync notification
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::HSync()
{
	if (render && (render->GetCompositorMode() == Render::compositor_fast)) {
		ASSERT(this);
		return;
	}
	BOOL flag;
	int i;
	DWORD addr;
	DWORD reg[4];

	ASSERT(this);

	// Check if BG update is needed on HSync
	if (bghsync == 0) {
		return;
	}

	if (bghsync & 0x01) {
		// BG0 scroll X
		render->BGScrl(0, spr.bg_scrlx[0], spr.bg_scrly[0]);
		bghsync &= ~0x01;
	}

	if (bghsync & 0x02) {
		// BG0 scroll Y
		render->BGScrl(0, spr.bg_scrlx[0], spr.bg_scrly[0]);
		bghsync &= ~0x02;
	}

	if (bghsync & 0x04) {
		// BG1 scroll X
		render->BGScrl(1, spr.bg_scrlx[1], spr.bg_scrly[1]);
		bghsync &= ~0x04;
	}

	if (bghsync & 0x08) {
		// BG1 scroll Y
		render->BGScrl(1, spr.bg_scrlx[1], spr.bg_scrly[1]);
		bghsync &= ~0x08;
	}

		// Sprite update target selection
	if (bghsync & 0x10) {
		// No match check initially
		flag = FALSE;

		for (i=0; i<128; i++) {
			// Update target
			if (sphsync[i] == 0) {
				continue;
			}

			// Count down
			sphsync[i]--;

			// 2 scanline stagger
			if (sphsync[i] == 0) {
				addr = i << 3;
				reg[0] = *(WORD*)(&sprite[addr  ]);
				reg[1] = *(WORD*)(&sprite[addr+2]);
				reg[2] = *(WORD*)(&sprite[addr+4]);
				reg[3] = *(WORD*)(&sprite[addr+6]);
				render->SpriteReg(addr, reg);
				continue;
			}

			// Match check still needed
			flag = TRUE;
		}

		// Clear flag if no longer needed
		if (!flag) {
			bghsync &= ~0x10;
		}
	}

}