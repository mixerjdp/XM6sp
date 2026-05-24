//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ Text VRAM ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "fileio.h"
#include "schedule.h"
#include "render.h"
#include "renderin.h"
#include "tvram.h"

//===========================================================================
//
//	Text VRAM handler
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
TVRAMHandler::TVRAMHandler(Render *rend, BYTE *mem)
{
ASSERT(rend);
ASSERT(mem);

	// Reference
	render = rend;
	tvram = mem;

	// Structure initialization
	multi = 0;
	mask = 0;
	rev = 0;
	maskh = 0;
	revh = 0;
}

//===========================================================================
//
//	Text VRAM handler (normal)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
TVRAMNormal::TVRAMNormal(Render *rend, BYTE *mem) : TVRAMHandler(rend, mem)
{
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL TVRAMNormal::WriteByte(DWORD addr, DWORD data)
{
ASSERT(this);
ASSERT(addr < 0x80000);
ASSERT(data < 0x100);

	if (tvram[addr] != data) {
		tvram[addr] = (BYTE)data;
		render->TextMem(addr);
	}
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL TVRAMNormal::WriteWord(DWORD addr, DWORD data)
{
ASSERT(this);
ASSERT(addr < 0x80000);
ASSERT(data < 0x10000);

	if ((DWORD)*(WORD*)(&tvram[addr]) != data) {
		*(WORD*)(&tvram[addr]) = (WORD)data;
		render->TextMem(addr);
	}
}

//===========================================================================
//
//	Text VRAM handler (mask)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
TVRAMMask::TVRAMMask(Render *rend, BYTE *mem) : TVRAMHandler(rend, mem)
{
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL TVRAMMask::WriteByte(DWORD addr, DWORD data)
{
	DWORD mem;

ASSERT(this);
ASSERT(addr < 0x80000);
ASSERT(data < 0x100);

	// mask is 1 does not change, 0 changes
	mem = (DWORD)tvram[addr];
	if (addr & 1) {
		// At 68000, odd address uses b15-b8
		mem &= maskh;
		data &= revh;
	}
	else {
		// At 68000, even address uses b7-b0
		mem &= mask;
		data &= rev;
	}

	// Merge
	data |= mem;

	// Write
	if (tvram[addr] != data) {
		tvram[addr] = (BYTE)data;
		render->TextMem(addr);
	}
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL TVRAMMask::WriteWord(DWORD addr, DWORD data)
{
	DWORD mem;

ASSERT(this);
ASSERT(addr < 0x80000);
ASSERT(data < 0x10000);

	// mask is 1 does not change, 0 changes
	mem = (DWORD)*(WORD*)(&tvram[addr]);
	mem &= mask;
	data &= rev;

	// Merge
	data |= mem;

	if ((DWORD)*(WORD*)(&tvram[addr]) != data) {
		*(WORD*)(&tvram[addr]) = (WORD)data;
		render->TextMem(addr);
	}
}

//===========================================================================
//
//	Text VRAM handler (multi)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
TVRAMMulti::TVRAMMulti(Render *rend, BYTE *mem) : TVRAMHandler(rend, mem)
{
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL TVRAMMulti::WriteByte(DWORD addr, DWORD data)
{
	BOOL flag;

ASSERT(this);
ASSERT(addr < 0x80000);
ASSERT(data < 0x100);

	// Address mask
	addr &= 0x1ffff;
	flag = FALSE;

	// Plane B
	if (multi & 1) {
		if (tvram[addr] != data) {
			tvram[addr] = (BYTE)data;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane G
	if (multi & 2) {
		if (tvram[addr] != data) {
			tvram[addr] = (BYTE)data;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane R
	if (multi & 4) {
		if (tvram[addr] != data) {
			tvram[addr] = (BYTE)data;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane I
	if (multi & 8) {
		if (tvram[addr] != data) {
			tvram[addr] = (BYTE)data;
			flag = TRUE;
		}
	}

	// Render notify
	if (flag) {
		render->TextMem(addr);
	}
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL TVRAMMulti::WriteWord(DWORD addr, DWORD data)
{
	BOOL flag;

ASSERT(this);
ASSERT(addr < 0x80000);
ASSERT(data < 0x10000);

	// Address mask
	addr &= 0x1fffe;
	flag = FALSE;

	// Plane B
	if (multi & 1) {
		if ((DWORD)*(WORD*)(&tvram[addr]) != data) {
			*(WORD*)(&tvram[addr]) = (WORD)data;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane G
	if (multi & 2) {
		if ((DWORD)*(WORD*)(&tvram[addr]) != data) {
			*(WORD*)(&tvram[addr]) = (WORD)data;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane R
	if (multi & 4) {
		if ((DWORD)*(WORD*)(&tvram[addr]) != data) {
			*(WORD*)(&tvram[addr]) = (WORD)data;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane I
	if (multi & 8) {
		if ((DWORD)*(WORD*)(&tvram[addr]) != data) {
			*(WORD*)(&tvram[addr]) = (WORD)data;
			flag = TRUE;
		}
	}

	// Render notify
	if (flag) {
		render->TextMem(addr);
	}
}

//===========================================================================
//
//	Text VRAM handler (both)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
TVRAMBoth::TVRAMBoth(Render *rend, BYTE *mem) : TVRAMHandler(rend, mem)
{
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL TVRAMBoth::WriteByte(DWORD addr, DWORD data)
{
	DWORD mem;
	DWORD maskhl;
	BOOL flag;

ASSERT(this);
ASSERT(addr < 0x80000);
ASSERT(data < 0x100);

	// Odd/even difference is same as mask
	if (addr & 1) {
		maskhl = maskh;
		data &= revh;
	}
	else {
		maskhl = mask;
		data &= rev;
	}

	// Address mask
	addr &= 0x1ffff;
	flag = FALSE;

	// Plane B
	if (multi & 1) {
		mem = (DWORD)tvram[addr];
		mem &= maskhl;
		mem |= data;

		if (tvram[addr] != mem) {
			tvram[addr] = (BYTE)mem;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane G
	if (multi & 2) {
		mem = (DWORD)tvram[addr];
		mem &= maskhl;
		mem |= data;

		if (tvram[addr] != mem) {
			tvram[addr] = (BYTE)mem;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane R
	if (multi & 4) {
		mem = (DWORD)tvram[addr];
		mem &= maskhl;
		mem |= data;

		if (tvram[addr] != mem) {
			tvram[addr] = (BYTE)mem;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane I
	if (multi & 8) {
		mem = (DWORD)tvram[addr];
		mem &= maskhl;
		mem |= data;

		if (tvram[addr] != mem) {
			tvram[addr] = (BYTE)mem;
			flag = TRUE;
		}
	}

	// Render notify
	if (flag) {
		render->TextMem(addr);
	}
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL TVRAMBoth::WriteWord(DWORD addr, DWORD data)
{
	DWORD mem;
	BOOL flag;

ASSERT(this);
ASSERT(addr < 0x80000);
ASSERT(data < 0x10000);

	// Data is masked
	data &= rev;

	// Address mask
	addr &= 0x1fffe;
	flag = FALSE;

	// Plane B
	if (multi & 1) {
		mem = (DWORD)*(WORD*)(&tvram[addr]);
		mem &= mask;
		mem |= data;

		if ((DWORD)*(WORD*)(&tvram[addr]) != mem) {
			*(WORD*)(&tvram[addr]) = (WORD)mem;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane G
	if (multi & 2) {
		mem = (DWORD)*(WORD*)(&tvram[addr]);
		mem &= mask;
		mem |= data;

		if ((DWORD)*(WORD*)(&tvram[addr]) != mem) {
			*(WORD*)(&tvram[addr]) = (WORD)mem;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane R
	if (multi & 4) {
		mem = (DWORD)*(WORD*)(&tvram[addr]);
		mem &= mask;
		mem |= data;

		if ((DWORD)*(WORD*)(&tvram[addr]) != mem) {
			*(WORD*)(&tvram[addr]) = (WORD)mem;
			flag = TRUE;
		}
	}
	addr += 0x20000;

	// Plane I
	if (multi & 8) {
		mem = (DWORD)*(WORD*)(&tvram[addr]);
		mem &= mask;
		mem |= data;

		if ((DWORD)*(WORD*)(&tvram[addr]) != mem) {
			*(WORD*)(&tvram[addr]) = (WORD)mem;
			flag = TRUE;
		}
	}

	// Render notify
	if (flag) {
		render->TextMem(addr);
	}
}

//===========================================================================
//
//	Text VRAM
//
//===========================================================================
//#define TVRAM_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
TVRAM::TVRAM(VM *p) : MemDevice(p)
{
	// Device ID initialization
	dev.id = MAKEID('T', 'V', 'R', 'M');
	dev.desc = "Text VRAM";

	// Start address, end address
	memdev.first = 0xe00000;
	memdev.last = 0xe7ffff;

	// Handlers
	normal = NULL;
	mask = NULL;
	multi = NULL;
	both = NULL;

	// Others
	render = NULL;
	tvram = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL TVRAM::Init()
{
ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Render get
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
ASSERT(render);

	// Memory allocation, initialization
	try {
		tvram = new BYTE[ 0x80000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!tvram) {
		return FALSE;
	}
	memset(tvram, 0, 0x80000);

	// Handler create
	normal = new TVRAMNormal(render, tvram);
	mask = new TVRAMMask(render, tvram);
	multi = new TVRAMMulti(render, tvram);
	both = new TVRAMBoth(render, tvram);
	handler = normal;

	// Structure initialization
	tvdata.multi = 0;
	tvdata.mask = 0;
	tvdata.rev = 0xffffffff;
	tvdata.maskh = 0;
	tvdata.revh = 0xffffffff;
	tvdata.src = 0;
	tvdata.dst = 0;
	tvdata.plane = 0;
	tvcount = 0;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::Cleanup()
{
ASSERT(this);

	// Handler delete
	if (both) {
		delete both;
		both = NULL;
	}
	if (multi) {
		delete multi;
		multi = NULL;
	}
	if (mask) {
		delete mask;
		mask = NULL;
	}
	if (normal) {
		delete normal;
		normal = NULL;
	}
	handler = NULL;

	// Memory release
	delete[] tvram;
	tvram = NULL;

	// Base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::Reset()
{
ASSERT(this);
ASSERT_DIAG();

	LOG0(Log::Normal, "Reset");

	// Structure initialization
	tvdata.multi = 0;
	tvdata.mask = 0;
	tvdata.rev = 0xffffffff;
	tvdata.maskh = 0;
	tvdata.revh = 0xffffffff;
	tvdata.src = 0;
	tvdata.dst = 0;
	tvdata.plane = 0;

	// Access count 0
	tvcount = 0;

	// Handler is normal
	handler = normal;
ASSERT(handler);
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL TVRAM::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

ASSERT(this);
ASSERT(fio);
ASSERT_DIAG();

	LOG0(Log::Normal, "Save");

	// Memory save
	if (!fio->Write(tvram, 0x80000)) {
		return FALSE;
	}

	// Size save
	sz = sizeof(tvram_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Data save
	if (!fio->Write(&tvdata, (int)sz)) {
		return FALSE;
	}

	// tvcount (added in version 2.04)
	if (!fio->Write(&tvcount, sizeof(tvcount))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL TVRAM::Load(Fileio *fio, int ver)
{
	size_t sz;
	DWORD addr;

ASSERT(this);
ASSERT(fio);
ASSERT(ver >= 0x0200);
ASSERT_DIAG();

	LOG0(Log::Normal, "Load");

	// Memory load
	if (!fio->Read(tvram, 0x80000)) {
		return FALSE;
	}

	// Size load, compare
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(tvram_t)) {
		return FALSE;
	}

	// Data load
	if (!fio->Read(&tvdata, (int)sz)) {
		return FALSE;
	}

	// tvcount (added in version 2.04)
	tvcount = 0;
	if (ver >= 0x0204) {
		if (!fio->Read(&tvcount, sizeof(tvcount))) {
			return FALSE;
		}
	}

	// Render notify
	for(addr=0; addr<0x20000; addr++) {
		render->TextMem(addr);
	}

	// Handler setting
	SelectHandler();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::ApplyCfg(const Config* /*config*/)
{
ASSERT(this);
ASSERT_DIAG();

	LOG0(Log::Normal, "Apply configuration");
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::AssertDiag() const
{
	// Base class
	MemDevice::AssertDiag();

ASSERT(this);
ASSERT(GetID() == MAKEID('T', 'V', 'R', 'M'));
ASSERT(memdev.first == 0xe00000);
ASSERT(memdev.last == 0xe7ffff);
ASSERT(tvram);
ASSERT(normal);
ASSERT(mask);
ASSERT(multi);
ASSERT(both);
ASSERT(handler);
ASSERT(tvdata.multi <= 0x1f);
ASSERT(tvdata.mask < 0x10000);
ASSERT(tvdata.rev >= 0xffff0000);
ASSERT(tvdata.maskh < 0x100);
ASSERT(tvdata.revh >= 0xffffff00);
ASSERT(tvdata.src < 0x100);
ASSERT(tvdata.dst < 0x100);
ASSERT(tvdata.plane <= 0x0f);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL TVRAM::ReadByte(DWORD addr)
{
ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT_DIAG();

	// Wait (0.75 wait)
	tvcount++;
	if (tvcount & 3) {
		scheduler->Wait(1);
	}

	// Endian convert and read
	return (DWORD)tvram[(addr & 0x7ffff) ^ 1];
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL TVRAM::ReadWord(DWORD addr)
{
ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT((addr & 1) == 0);
ASSERT_DIAG();

	// Wait (0.75 wait)
	tvcount++;
	if (tvcount & 3) {
		scheduler->Wait(1);
	}

	// Read
	return (DWORD)*(WORD *)(&tvram[addr & 0x7ffff]);
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::WriteByte(DWORD addr, DWORD data)
{
ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT(data < 0x100);
ASSERT_DIAG();

	// Wait (0.75 wait)
	tvcount++;
	if (tvcount & 3) {
		scheduler->Wait(1);
	}

	// Write
	handler->WriteByte((addr & 0x7ffff) ^ 1, data);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::WriteWord(DWORD addr, DWORD data)
{
ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT((addr & 1) == 0);
ASSERT(data < 0x10000);
ASSERT_DIAG();

	// Wait (0.75 wait)
	tvcount++;
	if (tvcount & 3) {
		scheduler->Wait(1);
	}

	// Write
	handler->WriteWord(addr & 0x7ffff, data);
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL TVRAM::ReadOnly(DWORD addr) const
{
ASSERT(this);
ASSERT((addr >= memdev.first) && (addr <= memdev.last));
ASSERT_DIAG();

	// Endian convert and read
	return tvram[(addr & 0x7ffff) ^ 1];
}

//---------------------------------------------------------------------------
//
//	Get TVRAM
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL TVRAM::GetTVRAM() const
{
ASSERT(this);
ASSERT_DIAG();

	return tvram;
}

//---------------------------------------------------------------------------
//
//	Multi access setting
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::SetMulti(DWORD data)
{
ASSERT(this);
ASSERT(data <= 0x1f);
ASSERT_DIAG();

	// Equal check
	if (tvdata.multi == data) {
		return;
	}

	// Data copy
	tvdata.multi = data;

	// Handler select
	SelectHandler();
}

//---------------------------------------------------------------------------
//
//	Access mask setting
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::SetMask(DWORD data)
{
ASSERT(this);
ASSERT(data < 0x10000);
ASSERT_DIAG();

	// Equal check
	if (tvdata.mask == data) {
		return;
	}

	// Data copy
	tvdata.mask = data;
	tvdata.rev = ~tvdata.mask;
	tvdata.maskh = tvdata.mask >> 8;
	tvdata.revh = ~tvdata.maskh;

	// Handler select
	SelectHandler();
}

//---------------------------------------------------------------------------
//
//	Handler select
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::SelectHandler()
{
ASSERT(this);
ASSERT_DIAG();

	// Normal
	handler = normal;

	// Multi check
	if (tvdata.multi != 0) {
		// Mask and multi
		if (tvdata.mask != 0) {
			// Both
			handler = both;

			// Multi data set
			handler->multi = tvdata.multi;

			// Mask data set
			handler->mask = tvdata.mask;
			handler->rev = tvdata.rev;
			handler->maskh = tvdata.maskh;
			handler->revh = tvdata.revh;
		}
		else {
			// Multi
			handler = multi;

			// Multi data set
			handler->multi = tvdata.multi;
		}
		return;
	}

	// Mask check
	if (tvdata.mask != 0) {
		// Mask
		handler = mask;

		// Mask data set
		handler->mask = tvdata.mask;
		handler->rev = tvdata.rev;
		handler->maskh = tvdata.maskh;
		handler->revh = tvdata.revh;
	}
}

//---------------------------------------------------------------------------
//
//	Raster copy setting
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::SetCopyRaster(DWORD src, DWORD dst, DWORD plane)
{
ASSERT(this);
ASSERT(src < 0x100);
ASSERT(dst < 0x100);
ASSERT(plane <= 0x0f);
ASSERT_DIAG();

	tvdata.src = src;
	tvdata.dst = dst;
	tvdata.plane = plane;
}

//---------------------------------------------------------------------------
//
//	Raster copy execution
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::RasterCopy()
{
ASSERT(this);
ASSERT_DIAG();
#if 0
	DWORD *p;
	DWORD *q;
	int i;
	int j;
	DWORD plane;

	// Pointer, virtual address convert
	p = (DWORD*)&tvram[tvdata.src << 9];
	q = (DWORD*)&tvram[tvdata.dst << 9];
	plane = tvdata.plane;

	// Plane copy
	for (i=0; i<4; i++) {
		if (plane & 1) {
			for (j=7; j>=0; j--) {
				q[0] = p[0];
				q[1] = p[1];
				q[2] = p[2];
				q[3] = p[3];
				q[4] = p[4];
				q[5] = p[5];
				q[6] = p[6];
				q[7] = p[7];
				q[8] = p[8];
				q[9] = p[9];
				q[10] = p[10];
				q[11] = p[11];
				q[12] = p[12];
				q[13] = p[13];
				q[14] = p[14];
				q[15] = p[15];
				p += 16;
				q += 16;
			}
			p -= 128;
			q -= 128;
		}
		p += 0x8000;
		q += 0x8000;
		plane >>= 1;
	}

	// To render, copy area notify to start rendering when finished
	plane = tvdata.dst;
	plane <<= 9;
	for (i=0; i<0x200; i++) {
		render->TextMem(plane + i);
	}
#endif
	// Render notify
	if (tvdata.plane != 0) {
		render->TextCopy(tvdata.src, tvdata.dst, tvdata.plane);
	}
}