//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 P.I. (ytanaka@ipc-tokai.or.jp)
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

	// Store
	render = rend;
	tvram = mem;

	// Initialize the state
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
void FASTCALL TVRAMHandler::NotifyPx68kTVRAMWrite(DWORD internal_addr, BYTE data)
{
	if (!render) {
		return;
	}
	render->TVRAMWrite(0xe00000 + ((internal_addr & 0x7ffff) ^ 1), data);
}

void FASTCALL TVRAMHandler::NotifyPx68kTVRAMWord(DWORD addr, WORD data)
{
	if (!render) {
		return;
	}
	addr &= 0x7fffe;
	render->TVRAMWrite(0xe00000 + addr, (BYTE)((data >> 8) & 0xff));
	render->TVRAMWrite(0xe00000 + addr + 1, (BYTE)(data & 0xff));
}

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
		NotifyPx68kTVRAMWrite(addr, (BYTE)data);
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
		NotifyPx68kTVRAMWord(addr, (WORD)data);
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

	// With the mask, 1 means unchanged and 0 means modified
	mem = (DWORD)tvram[addr];
	if (addr & 1) {
		// On the 68000, even addresses use b15-b8
		mem &= maskh;
		data &= revh;
	}
	else {
		// On the 68000, odd addresses use b7-b0
		mem &= mask;
		data &= rev;
	}

	// Combine
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

	// With the mask, 1 means unchanged and 0 means modified
	mem = (DWORD)*(WORD*)(&tvram[addr]);
	mem &= mask;
	data &= rev;

	// Combine
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

	// Initialize
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

	// Notify the renderer
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

	// Initialize
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

	// Notify the renderer
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

	// Determine even/odd first
	if (addr & 1) {
		maskhl = maskh;
		data &= revh;
	}
	else {
		maskhl = mask;
		data &= rev;
	}

	// Initialize
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

	// Notify the renderer
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

	// Mask the data first
	data &= rev;

	// Initialize
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

	// Notify the renderer
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
	// Initialize the device ID
	dev.id = MAKEID('T', 'V', 'R', 'M');
	dev.desc = "Text VRAM";

	// Start and end addresses
	memdev.first = 0xe00000;
	memdev.last = 0xe7ffff;

	// Handler
	normal = NULL;
	mask = NULL;
	multi = NULL;
	both = NULL;

	// Other
	render = NULL;
	tvram = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL TVRAM::Init()
{
	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get the renderer
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// Allocate and clear memory
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

	// Create the handlers
	normal = new TVRAMNormal(render, tvram);
	mask = new TVRAMMask(render, tvram);
	multi = new TVRAMMulti(render, tvram);
	both = new TVRAMBoth(render, tvram);
	handler = normal;

	// Initialize the work area
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

	// Delete the handlers
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

	// Release memory
	delete[] tvram;
	tvram = NULL;

	// Return to the base class
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

	LOG0(Log::Normal, "リセット");

	// Initialize the work area
	tvdata.multi = 0;
	tvdata.mask = 0;
	tvdata.rev = 0xffffffff;
	tvdata.maskh = 0;
	tvdata.revh = 0xffffffff;
	tvdata.src = 0;
	tvdata.dst = 0;
	tvdata.plane = 0;

	// Clear the access count
	tvcount = 0;

	// Use the normal handler
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

	LOG0(Log::Normal, "セーブ");

	// Save memory
	if (!fio->Write(tvram, 0x80000)) {
		return FALSE;
	}

	// Save the size
	sz = sizeof(tvram_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save the payload
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

	LOG0(Log::Normal, "ロード");

	// Load memory
	if (!fio->Read(tvram, 0x80000)) {
		return FALSE;
	}

	// Load and verify the size
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(tvram_t)) {
		return FALSE;
	}

	// Load the payload
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

	// Notify the renderer
	for(addr=0; addr<0x20000; addr++) {
		render->TextMem(addr);
	}

	// Configure the handlers
	SelectHandler();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "設定適用");
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Diagnostics
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

	// Load with endianness reversed
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
//	Read-only
//
//---------------------------------------------------------------------------
DWORD FASTCALL TVRAM::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// Load with endianness reversed
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
//	Set simultaneous writes
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::SetMulti(DWORD data)
{
	ASSERT(this);
	ASSERT(data <= 0x1f);
	ASSERT_DIAG();

	// Check for equality
	if (tvdata.multi == data) {
		return;
	}

	// Copy the data
	tvdata.multi = data;

	// Select the handler
	SelectHandler();
}

//---------------------------------------------------------------------------
//
//	Set the access mask
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::SetMask(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x10000);
	ASSERT_DIAG();

	// Check for equality
	if (tvdata.mask == data) {
		return;
	}

	// Copy the data
	tvdata.mask = data;
	tvdata.rev = ~tvdata.mask;
	tvdata.maskh = tvdata.mask >> 8;
	tvdata.revh = ~tvdata.maskh;

	// Select the handler
	SelectHandler();
}

//---------------------------------------------------------------------------
//
//	Select the handler
//
//---------------------------------------------------------------------------
void FASTCALL TVRAM::SelectHandler()
{
	ASSERT(this);
	ASSERT_DIAG();

	// Normal
	handler = normal;

	// Check the multi flag
	if (tvdata.multi != 0) {
		// Whether it is combined with the mask
		if (tvdata.mask != 0) {
			// Both
			handler = both;

			// Set the multi data
			handler->multi = tvdata.multi;

			// Set the mask data
			handler->mask = tvdata.mask;
			handler->rev = tvdata.rev;
			handler->maskh = tvdata.maskh;
			handler->revh = tvdata.revh;
		}
		else {
			// Multi
			handler = multi;

			// Set the multi data
			handler->multi = tvdata.multi;
		}
		return;
	}

	// Check the mask flag
	if (tvdata.mask != 0) {
		// Mask
		handler = mask;

		// Set the mask data
		handler->mask = tvdata.mask;
		handler->rev = tvdata.rev;
		handler->maskh = tvdata.maskh;
		handler->revh = tvdata.revh;
	}
}

//---------------------------------------------------------------------------
//
//	Set raster copy
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
//	Execute raster copy
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

	// Initialize the pointer and plane
	p = (DWORD*)&tvram[tvdata.src << 9];
	q = (DWORD*)&tvram[tvdata.dst << 9];
	plane = tvdata.plane;

	// Handle each plane separately
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

	// Notify the renderer that the destination area was replaced
	plane = tvdata.dst;
	plane <<= 9;
	for (i=0; i<0x200; i++) {
		render->TextMem(plane + i);
	}
#endif
	// Call the renderer
	if (tvdata.plane != 0) {
		render->TextCopy(tvdata.src, tvdata.dst, tvdata.plane);
	}
}
