//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ・ｰ・ｩ・・ytanaka@ipc-tokai.or.jp)
//	[ 繝｡繝｢繝ｪ ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "filepath.h"
#include "fileio.h"
#include "cpu.h"
#include "areaset.h"
#include "gvram.h"
#include "tvram.h"
#include "sram.h"
#include "config.h"
#include "core_asm.h"
#include "memory.h"

//---------------------------------------------------------------------------
//
//	繧ｹ繧ｿ繝・ぅ繝・け 繝ｯ繝ｼ繧ｯ
//
//---------------------------------------------------------------------------
static CPU *pCPU;
static Memory *pMemory;

//---------------------------------------------------------------------------
//
//	繝舌せ繧ｨ繝ｩ繝ｼ蜀咲樟驛ｨ(繝｡繧､繝ｳ繝｡繝｢繝ｪ譛ｪ螳溯｣・お繝ｪ繧｢縺ｮ縺ｿ)
//
//---------------------------------------------------------------------------
extern "C" {

//---------------------------------------------------------------------------
//
//	隱ｭ縺ｿ霎ｼ縺ｿ繝舌せ繧ｨ繝ｩ繝ｼ
//
//---------------------------------------------------------------------------
void ReadBusErr(DWORD addr)
{
	pCPU->BusErr(addr, TRUE);
}

//---------------------------------------------------------------------------
//
//	譖ｸ縺崎ｾｼ縺ｿ繝舌せ繧ｨ繝ｩ繝ｼ
//
//---------------------------------------------------------------------------
void WriteBusErr(DWORD addr)
{
	pCPU->BusErr(addr, FALSE);
}
}

//===========================================================================
//
//	繝｡繝｢繝ｪ
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	繧ｳ繝ｳ繧ｹ繝医Λ繧ｯ繧ｿ
//
//---------------------------------------------------------------------------
Memory::Memory(VM *p) : MemDevice(p)
{
	// 繝・ヰ繧､繧ｹID繧貞・譛溷喧
	dev.id = MAKEID('M', 'E', 'M', ' ');
	dev.desc = "Memory Ctrl (OHM2)";

	// 髢句ｧ九い繝峨Ξ繧ｹ縲∫ｵゆｺ・い繝峨Ξ繧ｹ
	memdev.first = 0;
	memdev.last = 0xffffff;

	// RAM/ROM繝舌ャ繝輔ぃ
	mem.ram = NULL;
	mem.ipl = NULL;
	mem.cg = NULL;
	mem.scsi = NULL;

	// RAM縺ｯ2MB
	mem.size = 2;
	mem.config = 0;
	mem.length = 0;

	// 繝｡繝｢繝ｪ繧ｿ繧､繝励・譛ｪ繝ｭ繝ｼ繝・
	mem.type = None;
	mem.now = None;

	// 繧ｪ繝悶ず繧ｧ繧ｯ繝・
	areaset = NULL;
	sram = NULL;

	// 縺昴・莉・
	memset(mem.table, 0, sizeof(mem.table));
	mem.memsw = TRUE;

	// static繝ｯ繝ｼ繧ｯ
	::pCPU = NULL;
}

//---------------------------------------------------------------------------
//
//	蛻晄悄蛹・
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::Init()
{
	ASSERT(this);

	// 蝓ｺ譛ｬ繧ｯ繝ｩ繧ｹ
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// 繝｡繧､繝ｳ繝｡繝｢繝ｪ
	mem.length = mem.size * 0x100000;
	try {
		mem.ram = new BYTE[ mem.length ];
	}
	catch (...) {
		return FALSE;
	}
	if (!mem.ram) {
		return FALSE;
	}

	// 繝｡繧､繝ｳ繝｡繝｢繝ｪ繧偵ぞ繝ｭ繧ｯ繝ｪ繧｢縺吶ｋ
	memset(mem.ram, 0x00, mem.length);

	// IPL ROM
	try {
		mem.ipl = new BYTE[ 0x20000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!mem.ipl) {
		return FALSE;
	}

	// CG ROM
	try {
		mem.cg = new BYTE[ 0xc0000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!mem.cg) {
		return FALSE;
	}

	// SCSI ROM
	try {
		mem.scsi = new BYTE[ 0x20000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!mem.scsi) {
		return FALSE;
	}

	// SASI縺ｮROM縺ｯ蠢・�医↑縺ｮ縺ｧ縲∝・縺ｫ繝ｭ繝ｼ繝峨☆繧・
	if (!LoadROM(SASI)) {
		// IPLROM.DAT, CGROM.DAT縺悟ｭ伜惠縺励↑縺・ヱ繧ｿ繝ｼ繝ｳ
		return FALSE;
	}

	// 莉悶・ROM縺後≠繧後・縲々VI竊辰ompact竊・30縺ｮ鬆・〒縲∝・縺ｫ隕九▽縺九▲縺溘ｂ縺ｮ繧貞━蜈医☆繧・
	if (LoadROM(XVI)) {
		mem.now = XVI;
	}
	if (mem.type == None) {
		if (LoadROM(Compact)) {
			mem.now = Compact;
		}
	}
	if (mem.type == None) {
		if (LoadROM(X68030)) {
			mem.now = X68030;
		}
	}

	// XVI,Compact,030縺・★繧後ｂ蟄伜惠縺励↑縺代ｌ縺ｰ縲∝・蠎ｦSASI繧定ｪｭ繧
	if (mem.type == None) {
		LoadROM(SASI);
		mem.now = SASI;
	}

	// 繝ｪ繝ｼ繧ｸ繝ｧ繝ｳ繧ｨ繝ｪ繧｢繧定ｨｭ螳・
	// NOTE: Musashi doesn't use Starscream-style memory regions.
	// Memory access is handled via m68k_read/write_memory callbacks.
	// Region pointers are kept in s68000context as stubs for compilation.
	// ::s68000context.u_fetch = u_pgr;
	// ::s68000context.s_fetch = s_pgr;
	// ::s68000context.u_readbyte = u_rbr;
	// ::s68000context.s_readbyte = s_rbr;
	// ::s68000context.u_readword = u_rwr;
	// ::s68000context.s_readword = s_rwr;
	// ::s68000context.u_writebyte = u_wbr;
	// ::s68000context.s_writebyte = s_wbr;
	// ::s68000context.u_writeword = u_wwr;
	// ::s68000context.s_writeword = s_wwr;

	// 繧ｨ繝ｪ繧｢繧ｻ繝・ヨ蜿門ｾ・
	areaset = (AreaSet*)vm->SearchDevice(MAKEID('A', 'R', 'E', 'A'));
	ASSERT(areaset);

	// SRAM蜿門ｾ・
	sram = (SRAM*)vm->SearchDevice(MAKEID('S', 'R', 'A', 'M'));
	ASSERT(sram);

	// static繝ｯ繝ｼ繧ｯ
	::pCPU = cpu;
	::pMemory = this;

	// 蛻晄悄蛹悶ユ繝ｼ繝悶Ν險ｭ螳・
	InitTable();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ROM繝ｭ繝ｼ繝・
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::LoadROM(memtype target)
{
	Filepath path;
	Fileio fio;
	int i;
	BYTE data;
	BYTE *ptr;
	BOOL scsi_req;
	int scsi_size;

	ASSERT(this);

	// 荳譌ｦ縺吶∋縺ｦ縺ｮROM繧ｨ繝ｪ繧｢繧呈ｶ亥悉縺励¨one縺ｫ
	memset(mem.ipl, 0xff, 0x20000);
	memset(mem.cg, 0xff, 0xc0000);
	memset(mem.scsi, 0xff, 0x20000);
	mem.type = None;

	// IPL
	switch (target) {
		case SASI:
		case SCSIInt:
		case SCSIExt:
			path.SysFile(Filepath::IPL);
			break;
		case XVI:
			path.SysFile(Filepath::IPLXVI);
			break;
		case Compact:
			path.SysFile(Filepath::IPLCompact);
			break;
		case X68030:
			path.SysFile(Filepath::IPL030);
			break;
		default:
			ASSERT(FALSE);
			return FALSE;
	}
	if (!fio.Load(path, mem.ipl, 0x20000)) {
		return FALSE;
	}

	// IPL繝舌う繝医せ繝ｯ繝・・
	ptr = mem.ipl;
	for (i=0; i<0x10000; i++) {
		data = ptr[0];
		ptr[0] = ptr[1];
		ptr[1] = data;
		ptr += 2;
	}

	// CG
	path.SysFile(Filepath::CG);
	if (!fio.Load(path, mem.cg, 0xc0000)) {
		// 繝輔ぃ繧､繝ｫ縺後↑縺代ｌ縺ｰ縲，GTMP縺ｧ繝ｪ繝医Λ繧､
		path.SysFile(Filepath::CGTMP);
		if (!fio.Load(path, mem.cg, 0xc0000)) {
			return FALSE;
		}
	}

	// CG繝舌う繝医せ繝ｯ繝・・
	ptr = mem.cg;
	for (i=0; i<0x60000; i++) {
		data = ptr[0];
		ptr[0] = ptr[1];
		ptr[1] = data;
		ptr += 2;
	}

	// SCSI
	scsi_req = FALSE;
	switch (target) {
		// 蜀・鳩
		case SCSIInt:
		case XVI:
		case Compact:
			path.SysFile(Filepath::SCSIInt);
			scsi_req = TRUE;
			break;
		case X68030:
			path.SysFile(Filepath::ROM030);
			scsi_req = TRUE;
			break;
		// 螟紋ｻ・
		case SCSIExt:
			path.SysFile(Filepath::SCSIExt);
			scsi_req = TRUE;
			break;
		// SASI(ROM蠢・ｦ√↑縺・
		case SASI:
			break;
		// 縺昴・莉・縺ゅｊ蠕励↑縺・
		default:
			ASSERT(FALSE);
			break;
	}
	if (scsi_req) {
		// X68030縺ｮ縺ｿROM30.DAT(0x20000繝舌う繝・縲√◎縺ｮ莉悶・0x2000繝舌う繝医〒繝医Λ繧､
		if (target == X68030) {
			scsi_size = 0x20000;
		}
		else {
			scsi_size = 0x2000;
		}

		// 蜈医↓繝昴う繝ｳ繧ｿ繧定ｨｭ螳・
		ptr = mem.scsi;

		// 繝ｭ繝ｼ繝・
		if (!fio.Load(path, mem.scsi, scsi_size)) {
			// SCSIExt縺ｯ0x1fe0繝舌う繝医ｂ險ｱ縺・WinX68k鬮倬溽沿縺ｨ莠呈鋤繧偵→繧・
			if (target != SCSIExt) {
				return FALSE;
			}

			// 0x1fe0繝舌う繝医〒蜀阪ヨ繝ｩ繧､
			scsi_size = 0x1fe0;
			ptr = &mem.scsi[0x20];
			if (!fio.Load(path, &mem.scsi[0x0020], scsi_size)) {
				return FALSE;
			}
		}

		// SCSI繝舌う繝医せ繝ｯ繝・・
		for (i=0; i<scsi_size; i+=2) {
			data = ptr[0];
			ptr[0] = ptr[1];
			ptr[1] = data;
			ptr += 2;
		}
	}

	// 繧ｿ繝ｼ繧ｲ繝・ヨ繧偵き繝ｬ繝ｳ繝医↓繧ｻ繝・ヨ縺励※縲∵・蜉・
	mem.type = target;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	蛻晄悄蛹悶ユ繝ｼ繝悶Ν菴懈・
//	窶ｻ繝｡繝｢繝ｪ繝・さ繝ｼ繝縺ｫ萓晏ｭ・
//
//---------------------------------------------------------------------------
void FASTCALL Memory::InitTable()
{
#if defined(_WIN32)
#pragma pack(push, 1)
#endif	// _WIN32
	MemDevice* devarray[0x40];
#if defined(_WIN32)
#pragma pack(pop)
#endif	// _WIN32

	MemDevice *mdev;
	BYTE *table;
	DWORD ptr;
	int i;

	ASSERT(this);

	// 繝昴う繝ｳ繧ｿ蛻晄悄蛹・
	mdev = this;
	i = 0;

	// Memory莉･髯阪・繝・ヰ繧､繧ｹ繧貞屓縺｣縺ｦ縲√・繧､繝ｳ繧ｿ繧帝・蛻励↓關ｽ縺ｨ縺・
	while (mdev) {
		devarray[i] = mdev;

		// 谺｡縺ｸ
		i++;
		mdev = (MemDevice*)mdev->GetNextDevice();
	}

	// 繧｢繧ｻ繝ｳ繝悶Λ繝ｫ繝ｼ繝√Φ繧貞他縺ｳ蜃ｺ縺励√ユ繝ｼ繝悶Ν繧貞ｼ輔″貂｡縺・
	MemInitDecode(this, devarray);

	// 繧｢繧ｻ繝ｳ繝悶Λ繝ｫ繝ｼ繝√Φ縺ｧ蜃ｺ譚･縺溘ユ繝ｼ繝悶Ν繧帝・↓謌ｻ縺・繧｢繝ｩ繧､繝ｳ繝｡繝ｳ繝医↓豕ｨ諢・
	table = MemDecodeTable;
	for (i=0; i<0x180; i++) {
		// 4繝舌う繝医＃縺ｨ縺ｫDWORD蛟､繧貞叙繧願ｾｼ縺ｿ縲√・繧､繝ｳ繧ｿ縺ｫ繧ｭ繝｣繧ｹ繝・
		ptr = *(DWORD*)table;
		mem.table[i] = (MemDevice*)ptr;

		// 谺｡縺ｸ
		table += 4;
	}
}

//---------------------------------------------------------------------------
//
//	繧ｯ繝ｪ繝ｼ繝ｳ繧｢繝・・
//
//---------------------------------------------------------------------------
void FASTCALL Memory::Cleanup()
{
	ASSERT(this);

	// 繝｡繝｢繝ｪ隗｣謾ｾ
	if (mem.ram) {
		delete[] mem.ram;
		mem.ram = NULL;
	}
	if (mem.ipl) {
		delete[] mem.ipl;
		mem.ipl = NULL;
	}
	if (mem.cg) {
		delete[] mem.cg;
		mem.cg = NULL;
	}
	if (mem.scsi) {
		delete[] mem.scsi;
		mem.scsi = NULL;
	}

	// 蝓ｺ譛ｬ繧ｯ繝ｩ繧ｹ縺ｸ
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	繝ｪ繧ｻ繝・ヨ
//
//---------------------------------------------------------------------------
void FASTCALL Memory::Reset()
{
	int size;

	ASSERT(this);
	LOG0(Log::Normal, "(message)");

	// 繝｡繝｢繝ｪ繧ｿ繧､繝励′荳閾ｴ縺励※縺・ｋ縺・
	if (mem.type != mem.now) {
		if (LoadROM(mem.type)) {
			// ROM縺悟ｭ伜惠縺励※縺・ｋ縲ゅΟ繝ｼ繝峨〒縺阪◆
			mem.now = mem.type;
		}
		else {
			// ROM縺悟ｭ伜惠縺励↑縺・４ASI繧ｿ繧､繝励→縺励※縲∬ｨｭ螳壹ｂSASI縺ｫ謌ｻ縺・
			LoadROM(SASI);
			mem.now = SASI;
			mem.type = SASI;
		}

		// 繧ｳ繝ｳ繝・く繧ｹ繝医ｒ菴懊ｊ逶ｴ縺・CPU::Reset縺ｯ螳御ｺ・＠縺ｦ縺・ｋ縺溘ａ縲∝ｿ・★FALSE)
		MakeContext(FALSE);
	}

	// 繝｡繝｢繝ｪ繧ｵ繧､繧ｺ縺御ｸ閾ｴ縺励※縺・ｋ縺・
	if (mem.size == ((mem.config + 1) * 2)) {
		// 荳閾ｴ縺励※縺・ｋ縺ｮ縺ｧ縲√Γ繝｢繝ｪ繧ｹ繧､繝・メ閾ｪ蜍墓峩譁ｰ繝√ぉ繝・け
		if (mem.memsw) {
			// $ED0008 : 繝｡繧､繝ｳRAM繧ｵ繧､繧ｺ
			size = mem.size << 4;
			sram->SetMemSw(0x08, 0x00);
			sram->SetMemSw(0x09, size);
			sram->SetMemSw(0x0a, 0x00);
			sram->SetMemSw(0x0b, 0x00);
		}
		return;
	}

	// 螟画峩
	mem.size = (mem.config + 1) * 2;

	// 蜀咲｢ｺ菫・
	ASSERT(mem.ram);
	delete[] mem.ram;
	mem.ram = NULL;
	mem.length = mem.size * 0x100000;
	try {
		mem.ram = new BYTE[ mem.length ];
	}
	catch (...) {
		// 繝｡繝｢繝ｪ荳崎ｶｳ縺ｮ蝣ｴ蜷医・2MB縺ｫ蝗ｺ螳・
		mem.config = 0;
		mem.size = 2;
		mem.length = mem.size * 0x100000;
		mem.ram = new BYTE[ mem.length ];
	}
	if (!mem.ram) {
		// 繝｡繝｢繝ｪ荳崎ｶｳ縺ｮ蝣ｴ蜷医・2MB縺ｫ蝗ｺ螳・
		mem.config = 0;
		mem.size = 2;
		mem.length = mem.size * 0x100000;
		mem.ram = new BYTE[ mem.length ];
	}

	// 繝｡繝｢繝ｪ縺檎｢ｺ菫昴〒縺阪※縺・ｋ蝣ｴ蜷医・縺ｿ
	if (mem.ram) {
		memset(mem.ram, 0x00, mem.length);

		// 繧ｳ繝ｳ繝・く繧ｹ繝医ｒ菴懊ｊ逶ｴ縺・CPU::Reset縺ｯ螳御ｺ・＠縺ｦ縺・ｋ縺溘ａ縲∝ｿ・★FALSE)
		MakeContext(FALSE);
	}

	// 繝｡繝｢繝ｪ繧ｹ繧､繝・メ閾ｪ蜍墓峩譁ｰ
	if (mem.memsw) {
		// $ED0008 : 繝｡繧､繝ｳRAM繧ｵ繧､繧ｺ
		size = mem.size << 4;
		sram->SetMemSw(0x08, 0x00);
		sram->SetMemSw(0x09, size);
		sram->SetMemSw(0x0a, 0x00);
		sram->SetMemSw(0x0b, 0x00);
	}
}

//---------------------------------------------------------------------------
//
//	繧ｻ繝ｼ繝・
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::Save(Fileio *fio, int /*ver*/)
{
	ASSERT(this);


	// 繧ｿ繧､繝励ｒ譖ｸ縺・
	if (!fio->Write(&mem.now, sizeof(mem.now))) {
		return FALSE;
	}

	// SCSI ROM縺ｮ蜀・ｮｹ繧呈嶌縺・(X68030莉･螟・
	if (mem.now != X68030) {
		if (!fio->Write(mem.scsi, 0x2000)) {
			return FALSE;
		}
	}

	// mem.size繧呈嶌縺・
	if (!fio->Write(&mem.size, sizeof(mem.size))) {
		return FALSE;
	}

	// 繝｡繝｢繝ｪ繧呈嶌縺・
	if (!fio->Write(mem.ram, mem.length)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	繝ｭ繝ｼ繝・
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::Load(Fileio *fio, int /*ver*/)
{
	int size;
	BOOL context;

	ASSERT(this);
	LOG0(Log::Normal, "Load");

	// 繧ｳ繝ｳ繝・く繧ｹ繝医ｒ菴懊ｊ逶ｴ縺輔↑縺・
	context = FALSE;

	// 繧ｿ繧､繝励ｒ隱ｭ繧
	if (!fio->Read(&mem.type, sizeof(mem.type))) {
		return FALSE;
	}

	// 繧ｿ繧､繝励′迴ｾ蝨ｨ縺ｮ繧ゅ・縺ｨ驕輔▲縺ｦ縺・ｌ縺ｰ
	if (mem.type != mem.now) {
		// ROM繧定ｪｭ縺ｿ逶ｴ縺・
		if (!LoadROM(mem.type)) {
			// 繧ｻ繝ｼ繝匁凾縺ｫ蟄伜惠縺励※縺・◆ROM縺後√↑縺上↑縺｣縺ｦ縺・ｋ
			LoadROM(mem.now);
			return FALSE;
		}

		// ROM縺ｮ隱ｭ縺ｿ逶ｴ縺励↓謌仙粥縺励◆
		mem.now = mem.type;
		context = TRUE;
	}

	// SCSI ROM縺ｮ蜀・ｮｹ繧定ｪｭ繧 (X68030莉･螟・
	if (mem.type != X68030) {
		if (!fio->Read(mem.scsi, 0x2000)) {
			return FALSE;
		}
	}

	// mem.size繧定ｪｭ繧
	if (!fio->Read(&size, sizeof(size))) {
		return FALSE;
	}

	// mem.size縺ｨ荳閾ｴ縺励※縺・↑縺代ｌ縺ｰ
	if (mem.size != size) {
		// 螟画峩縺励※
		mem.size = size;

		// 蜀咲｢ｺ菫・
		delete[] mem.ram;
		mem.ram = NULL;
		mem.length = mem.size * 0x100000;
		try {
			mem.ram = new BYTE[ mem.length ];
		}
		catch (...) {
			mem.ram = NULL;
		}
		if (!mem.ram) {
			// 繝｡繝｢繝ｪ荳崎ｶｳ縺ｮ蝣ｴ蜷医・2MB縺ｫ蝗ｺ螳・
			mem.config = 0;
			mem.size = 2;
			mem.length = mem.size * 0x100000;
			mem.ram = new BYTE[ mem.length ];

			// 繝ｭ繝ｼ繝牙､ｱ謨・
			return FALSE;
		}

		// 繧ｳ繝ｳ繝・く繧ｹ繝亥・菴懈・縺悟ｿ・ｦ・
		context = TRUE;
	}

	// 繝｡繝｢繝ｪ繧定ｪｭ繧
	if (!fio->Read(mem.ram, mem.length)) {
		return FALSE;
	}

	// 蠢・ｦ√〒縺ゅｌ縺ｰ縲√さ繝ｳ繝・く繧ｹ繝医ｒ菴懊ｊ逶ｴ縺・
	if (context) {
		MakeContext(FALSE);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	險ｭ螳夐←逕ｨ
//
//---------------------------------------------------------------------------
void FASTCALL Memory::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	LOG0(Log::Normal, "險ｭ螳夐←逕ｨ");

	// 繝｡繝｢繝ｪ遞ｮ蛻･(ROM繝ｭ繝ｼ繝峨・谺｡蝗槭Μ繧ｻ繝・ヨ譎・
	mem.type = (memtype)config->mem_type;

	// RAM繧ｵ繧､繧ｺ(繝｡繝｢繝ｪ遒ｺ菫昴・谺｡蝗槭Μ繧ｻ繝・ヨ譎・
	mem.config = config->ram_size;
	ASSERT((mem.config >= 0) && (mem.config <= 5));

	// 繝｡繝｢繝ｪ繧ｹ繧､繝・メ閾ｪ蜍墓峩譁ｰ
	mem.memsw = config->ram_sramsync;
}

//---------------------------------------------------------------------------
//
//	繝舌う繝郁ｪｭ縺ｿ霎ｼ縺ｿ
//
//---------------------------------------------------------------------------
DWORD FASTCALL Memory::ReadByte(DWORD addr)
{
	DWORD index;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT(mem.now != None);

	// IPL ROM shadowed at 0x000000 during reset vector fetch ONLY.
	// In the original Starscream implementation, MakeContext(TRUE) set up
	// a temporary program region mapping 0x0000-0xFFFF to IPL ROM,
	// and MakeContext(FALSE) replaced it with normal RAM mapping.
	// We emulate this by only activating the shadow during m68k_pulse_reset().
	if (addr < 0x10000 && musashi_is_resetting) {
		return (DWORD)mem.ipl[(addr + 0x10000) ^ 1];
	}

	// 繝｡繧､繝ｳRAM
	if (addr < mem.length) {
		return (DWORD)mem.ram[addr ^ 1];
	}

	// IPL
	if (addr >= 0xfe0000) {
		addr &= 0x1ffff;
		addr ^= 1;
		return (DWORD)mem.ipl[addr];
	}

	// IPL繧､繝｡繝ｼ繧ｸ or SCSI蜀・鳩
	if (addr >= 0xfc0000) {
		// IPL繧､繝｡繝ｼ繧ｸ縺・
		if ((mem.now == SASI) || (mem.now == SCSIExt)) {
			// IPL繧､繝｡繝ｼ繧ｸ
			addr &= 0x1ffff;
			addr ^= 1;
			return (DWORD)mem.ipl[addr];
		}
		// SCSI蜀・鳩縺・遽・峇繝√ぉ繝・け)
		if (addr < 0xfc2000) {
			// SCSI蜀・鳩
			addr &= 0x1fff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
		// X68030 IPL蜑榊濠縺・
		if (mem.now == X68030) {
			// X68030 IPL蜑榊濠
			addr &= 0x1ffff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
		// SCSI蜀・鳩繝｢繝・Ν縺ｧ縲ヽOM遽・峇螟・
		return 0xff;
	}

	// CG
	if (addr >= 0xf00000) {
		addr &= 0xfffff;
		addr ^= 1;
		return (DWORD)mem.cg[addr];
	}

	// SCSI螟紋ｻ・
	if (mem.now == SCSIExt) {
		if ((addr >= 0xea0020) && (addr <= 0xea1fff)) {
			addr &= 0x1fff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
	}

	// 繝・ヰ繧､繧ｹ繝・ぅ繧ｹ繝代ャ繝・
	if (addr >= 0xc00000) {
		index = addr - 0xc00000;
		index >>= 13;
		ASSERT(index < 0x180);
		if (mem.table[index] != (MemDevice*)this) {
			return mem.table[index]->ReadByte(addr);
		}
	}

	LOG1(Log::Warning, "(message)", addr);
	cpu->BusErr(addr, TRUE);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	繝ｯ繝ｼ繝芽ｪｭ縺ｿ霎ｼ縺ｿ
//
//---------------------------------------------------------------------------
DWORD FASTCALL Memory::ReadWord(DWORD addr)
{
	DWORD data;
	DWORD index;
	WORD *ptr;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT(mem.now != None);

	// CPU縺九ｉ縺ｮ蝣ｴ蜷医・蛛ｶ謨ｰ菫晁ｨｼ縺輔ｌ縺ｦ縺・ｋ縺後．MAC縺九ｉ縺ｮ蝣ｴ蜷医・繝√ぉ繝・け蠢・ｦ√≠繧・
	if (addr & 1) {
		// 荳譌ｦCPU縺ｸ貂｡縺・CPU邨檎罰縺ｧDMA縺ｸ)
		cpu->AddrErr(addr, TRUE);
		return 0xffff;
	}

	// IPL ROM shadowed at 0x000000 during reset vector fetch ONLY.
	if (addr < 0x10000 && musashi_is_resetting) {
		ptr = (WORD*)(&mem.ipl[addr + 0x10000]);
		data = (DWORD)*ptr;
		return data;
	}

	// 繝｡繧､繝ｳRAM
	if (addr < mem.length) {
		ptr = (WORD*)(&mem.ram[addr]);
		data = (DWORD)*ptr;
		return data;
	}

	// IPL
	if (addr >= 0xfe0000) {
		addr &= 0x1ffff;
		ptr = (WORD*)(&mem.ipl[addr]);
		data = (DWORD)*ptr;
		return data;
	}

	// IPL繧､繝｡繝ｼ繧ｸ or SCSI蜀・鳩
	if (addr >= 0xfc0000) {
		// IPL繧､繝｡繝ｼ繧ｸ縺・
		if ((mem.now == SASI) || (mem.now == SCSIExt)) {
			// IPL繧､繝｡繝ｼ繧ｸ
			addr &= 0x1ffff;
			ptr = (WORD*)(&mem.ipl[addr]);
			data = (DWORD)*ptr;
			return data;
		}
		// SCSI蜀・鳩縺・遽・峇繝√ぉ繝・け)
		if (addr < 0xfc2000) {
			// SCSI蜀・鳩
			addr &= 0x1fff;
			ptr = (WORD*)(&mem.scsi[addr]);
			data = (DWORD)*ptr;
			return data;
		}
		// X68030 IPL蜑榊濠縺・
		if (mem.now == X68030) {
			// X68030 IPL蜑榊濠
			addr &= 0x1ffff;
			ptr = (WORD*)(&mem.scsi[addr]);
			data = (DWORD)*ptr;
			return data;
		}
		// SCSI蜀・鳩繝｢繝・Ν縺ｧ縲ヽOM遽・峇螟・
		return 0xffff;
	}

	// CG
	if (addr >= 0xf00000) {
		addr &= 0xfffff;
		ptr = (WORD*)(&mem.cg[addr]);
		data = (DWORD)*ptr;
		return data;
	}

	// SCSI螟紋ｻ・
	if (mem.now == SCSIExt) {
		if ((addr >= 0xea0020) && (addr <= 0xea1fff)) {
			addr &= 0x1fff;
			ptr = (WORD*)(&mem.scsi[addr]);
			data = (DWORD)*ptr;
			return data;
		}
	}

	// 繝・ヰ繧､繧ｹ繝・ぅ繧ｹ繝代ャ繝・
	if (addr >= 0xc00000) {
		index = addr - 0xc00000;
		index >>= 13;
		ASSERT(index < 0x180);
		if (mem.table[index] != (MemDevice*)this) {
			return mem.table[index]->ReadWord(addr);
		}
	}

	// 繝舌せ繧ｨ繝ｩ繝ｼ
	LOG1(Log::Warning, "(message)", addr);
	cpu->BusErr(addr, TRUE);
	return 0xffff;
}

//---------------------------------------------------------------------------
//
//	繝舌う繝域嶌縺崎ｾｼ縺ｿ
//
//---------------------------------------------------------------------------
void FASTCALL Memory::WriteByte(DWORD addr, DWORD data)
{
	DWORD index;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT(data < 0x100);
	ASSERT(mem.now != None);

	// 繝｡繧､繝ｳRAM
	if (addr < mem.length) {
		mem.ram[addr ^ 1] = (BYTE)data;
		return;
	}

	// IPL,SCSI,CG
	if (addr >= 0xf00000) {
		return;
	}

	// 繝・ヰ繧､繧ｹ繝・ぅ繧ｹ繝代ャ繝・
	if (addr >= 0xc00000) {
		index = addr - 0xc00000;
		index >>= 13;
		ASSERT(index < 0x180);
		if (mem.table[index] != (MemDevice*)this) {
			mem.table[index]->WriteByte(addr, data);
			return;
		}
	}

	// 繝舌せ繧ｨ繝ｩ繝ｼ
	cpu->BusErr(addr, FALSE);
	LOG2(Log::Warning, "(message)", addr, data);
}

//---------------------------------------------------------------------------
//
//	繝ｯ繝ｼ繝画嶌縺崎ｾｼ縺ｿ
//
//---------------------------------------------------------------------------
void FASTCALL Memory::WriteWord(DWORD addr, DWORD data)
{
	WORD *ptr;
	DWORD index;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT(data < 0x10000);
	ASSERT(mem.now != None);

	// CPU縺九ｉ縺ｮ蝣ｴ蜷医・蛛ｶ謨ｰ菫晁ｨｼ縺輔ｌ縺ｦ縺・ｋ縺後．MAC縺九ｉ縺ｮ蝣ｴ蜷医・繝√ぉ繝・け蠢・ｦ√≠繧・
	if (addr & 1) {
		// 荳譌ｦCPU縺ｸ貂｡縺・CPU邨檎罰縺ｧDMA縺ｸ)
		cpu->AddrErr(addr, FALSE);
		return;
	}

	// 繝｡繧､繝ｳRAM
	if (addr < mem.length) {
		ptr = (WORD*)(&mem.ram[addr]);
		*ptr = (WORD)data;
		return;
	}

	// IPL,SCSI,CG
	if (addr >= 0xf00000) {
		return;
	}

	// 繝・ヰ繧､繧ｹ繝・ぅ繧ｹ繝代ャ繝・
	if (addr >= 0xc00000) {
		index = addr - 0xc00000;
		index >>= 13;
		ASSERT(index < 0x180);
		if (mem.table[index] != (MemDevice*)this) {
			mem.table[index]->WriteWord(addr, data);
			return;
		}
	}

	// 繝舌せ繧ｨ繝ｩ繝ｼ
	cpu->BusErr(addr, FALSE);
	LOG2(Log::Warning, "(message)", addr, data);
}

//---------------------------------------------------------------------------
//
//	隱ｭ縺ｿ霎ｼ縺ｿ縺ｮ縺ｿ
//
//---------------------------------------------------------------------------
DWORD FASTCALL Memory::ReadOnly(DWORD addr) const
{
	DWORD index;

	ASSERT(this);
	ASSERT(addr <= 0xffffff);
	ASSERT(mem.now != None);

	// IPL ROM shadowed at 0x000000 during reset vector fetch ONLY.
	if (addr < 0x10000 && musashi_is_resetting) {
		return (DWORD)mem.ipl[(addr + 0x10000) ^ 1];
	}

	// 繝｡繧､繝ｳRAM
	if (addr < mem.length) {
		return (DWORD)mem.ram[addr ^ 1];
	}

	// IPL
	if (addr >= 0xfe0000) {
		addr &= 0x1ffff;
		addr ^= 1;
		return (DWORD)mem.ipl[addr];
	}

	// IPL繧､繝｡繝ｼ繧ｸ or SCSI蜀・鳩
	if (addr >= 0xfc0000) {
		// IPL繧､繝｡繝ｼ繧ｸ縺・
		if ((mem.now == SASI) || (mem.now == SCSIExt)) {
			// IPL繧､繝｡繝ｼ繧ｸ
			addr &= 0x1ffff;
			addr ^= 1;
			return (DWORD)mem.ipl[addr];
		}
		// SCSI蜀・鳩縺・遽・峇繝√ぉ繝・け)
		if (addr < 0xfc2000) {
			// SCSI蜀・鳩
			addr &= 0x1fff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
		// X68030 IPL蜑榊濠縺・
		if (mem.now == X68030) {
			// X68030 IPL蜑榊濠
			addr &= 0x1ffff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
		// SCSI蜀・鳩繝｢繝・Ν縺ｧ縲ヽOM遽・峇螟・
		return 0xff;
	}

	// CG
	if (addr >= 0xf00000) {
		addr &= 0xfffff;
		addr ^= 1;
		return (DWORD)mem.cg[addr];
	}

	// SCSI螟紋ｻ・
	if (mem.now == SCSIExt) {
		if ((addr >= 0xea0020) && (addr <= 0xea1fff)) {
			addr &= 0x1fff;
			addr ^= 1;
			return (DWORD)mem.scsi[addr];
		}
	}

	// 繝・ヰ繧､繧ｹ繝・ぅ繧ｹ繝代ャ繝・
	if (addr >= 0xc00000) {
		index = addr - 0xc00000;
		index >>= 13;
		ASSERT(index < 0x180);
		if (mem.table[index] != (MemDevice*)this) {
			return mem.table[index]->ReadOnly(addr);
		}
	}

	// 繝槭ャ繝励＆繧後※縺・↑縺・
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	繧ｳ繝ｳ繝・く繧ｹ繝井ｽ懈・
//
//---------------------------------------------------------------------------
void FASTCALL Memory::MakeContext(BOOL reset)
{
	ASSERT(this);

	// On reset, reset the AreaSet (called from CPU::Reset)
	if (reset) {
		ASSERT(areaset);
		areaset->Reset();
		return;
	}

	// NOTE: Musashi uses m68k_read/write_memory callbacks instead of
	// Starscream memory region tables. Memory dispatch is handled by
	// Memory::ReadByte/ReadWord/WriteByte/WriteWord which already
	// implement the full address decoding logic.
	// The old Starscream region table construction code has been removed.

	// Force CPU to exit current instruction (was at the end of old MakeContext)
	cpu->Release();
}
void FASTCALL Memory::TerminateProgramRegion(int index, STARSCREAM_PROGRAMREGION *spr)
{
	ASSERT(this);
	ASSERT((index >= 0) && (index < 10));
	ASSERT(spr);

	spr[index].lowaddr = (DWORD)-1;
	spr[index].highaddr = (DWORD)-1;
	spr[index].offset = 0;
}

//---------------------------------------------------------------------------
//
//	繝・・繧ｿ繝ｪ繝ｼ繧ｸ繝ｧ繝ｳ邨ゆｺ・
//
//---------------------------------------------------------------------------
void FASTCALL Memory::TerminateDataRegion(int index, STARSCREAM_DATAREGION *sdr)
{
	ASSERT(this);
	ASSERT((index >= 0) && (index < 10));
	ASSERT(sdr);

	sdr[index].lowaddr = (DWORD)-1;
	sdr[index].highaddr = (DWORD)-1;
	sdr[index].memorycall = NULL;
	sdr[index].userdata = NULL;
}

//---------------------------------------------------------------------------
//
//	IPL繝舌・繧ｸ繝ｧ繝ｳ繝√ぉ繝・け
//	窶ｻIPL縺計ersion1.00(87/05/07)縺ｧ縺ゅｋ縺句凄縺九ｒ繝√ぉ繝・け
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::CheckIPL() const
{
	ASSERT(this);
	ASSERT(mem.now != None);

	// 蟄伜惠繝√ぉ繝・け
	if (!mem.ipl) {
		return FALSE;
	}

	// SASI繧ｿ繧､繝励・蝣ｴ蜷医・縺ｿ繝√ぉ繝・け縺吶ｋ
	if (mem.now != SASI) {
		return TRUE;
	}

	// 譌･莉・BCD)繧偵メ繧ｧ繝・け
	if (mem.ipl[0x1000a] != 0x87) {
		return FALSE;
	}
	if (mem.ipl[0x1000c] != 0x07) {
		return FALSE;
	}
	if (mem.ipl[0x1000d] != 0x05) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	CG繝√ぉ繝・け
//	窶ｻ8x8繝峨ャ繝医ヵ繧ｩ繝ｳ繝・蜈ｨ讖溽ｨｮ蜈ｱ騾・縺ｮSum,Xor縺ｧ繝√ぉ繝・け
//
//---------------------------------------------------------------------------
BOOL FASTCALL Memory::CheckCG() const
{
	BYTE add;
	BYTE eor;
	BYTE *ptr;
	int i;

	ASSERT(this);
	ASSERT(mem.now != None);

	// 蟄伜惠繝√ぉ繝・け
	if (!mem.cg) {
		return FALSE;
	}

	// 蛻晄悄險ｭ螳・
	add = 0;
	eor = 0;
	ptr = &mem.cg[0x3a800];

	// ADD, XOR繝ｫ繝ｼ繝・
	for (i=0; i<0x1000; i++) {
		add = (BYTE)(add + *ptr);
		eor ^= *ptr;
		ptr++;
	}

	// 繝√ぉ繝・け(XVI縺ｧ縺ｮ螳滓ｸｬ蛟､)
	if ((add != 0xec) || (eor != 0x84)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	CG蜿門ｾ・
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL Memory::GetCG() const
{
	ASSERT(this);
	ASSERT(mem.cg);

	return mem.cg;
}

//---------------------------------------------------------------------------
//
//	SCSI蜿門ｾ・
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL Memory::GetSCSI() const
{
	ASSERT(this);
	ASSERT(mem.scsi);

	return mem.scsi;
}

//---------------------------------------------------------------------------
//
//	Musashi Memory Bridge Functions
//	Called from musashi_mem.cpp -> m68k_read/write_memory_*
//
//---------------------------------------------------------------------------
extern "C" {

DWORD FASTCALL MusashiReadByte(DWORD addr)
{
	ASSERT(pMemory);
	return pMemory->ReadByte(addr);
}

DWORD FASTCALL MusashiReadWord(DWORD addr)
{
	ASSERT(pMemory);
	return pMemory->ReadWord(addr);
}

void FASTCALL MusashiWriteByte(DWORD addr, DWORD data)
{
	ASSERT(pMemory);
	pMemory->WriteByte(addr, data);
}

void FASTCALL MusashiWriteWord(DWORD addr, DWORD data)
{
	ASSERT(pMemory);
	pMemory->WriteWord(addr, data);
}

} // extern "C"
