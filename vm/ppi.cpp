//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ‚o‚hD(ytanaka@ipc-tokai.or.jp)
//	[ PPI(i8255A) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "adpcm.h"
#include "schedule.h"
#include "config.h"
#include "fileio.h"
#include "ppi.h"
#include "x68sound_bridge.h"

//===========================================================================
//
//	PPI
//
//===========================================================================
//#define PPI_LOG

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
PPI::PPI(VM *p) : MemDevice(p)
{
	int i;

	// ƒfƒoƒCƒXID‚ð‰Šú‰»
	dev.id = MAKEID('P', 'P', 'I', ' ');
	dev.desc = "PPI (i8255A)";

	// ŠJŽnƒAƒhƒŒƒXAI—¹ƒAƒhƒŒƒX
	memdev.first = 0xe9a000;
	memdev.last = 0xe9bfff;

	// “à•”ƒ[ƒN
	memset(&ppi, 0, sizeof(ppi));

	// ƒIƒuƒWƒFƒNƒg
	adpcm = NULL;
	for (i=0; i<PortMax; i++) {
		joy[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	‰Šú‰»
//
//---------------------------------------------------------------------------
BOOL FASTCALL PPI::Init()
{
	int i;

	ASSERT(this);

	// Šî–{ƒNƒ‰ƒX
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// ADPCMŽæ“¾
	ASSERT(!adpcm);
	adpcm = (ADPCM*)vm->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
	ASSERT(adpcm);

	// ƒWƒ‡ƒCƒXƒeƒBƒbƒNƒ^ƒCƒv
	for (i=0; i<PortMax; i++) {
		ppi.type[i] = 0;
		ASSERT(!joy[i]);
		joy[i] = new JoyDevice(this, i);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒNƒŠ[ƒ“ƒAƒbƒv
//
//---------------------------------------------------------------------------
void FASTCALL PPI::Cleanup()
{
	int i;

	ASSERT(this);

	// ƒWƒ‡ƒCƒXƒeƒBƒbƒNƒfƒoƒCƒX‚ð‰ð•ú
	for (i=0; i<PortMax; i++) {
		ASSERT(joy[i]);
		delete joy[i];
		joy[i] = NULL;
	}
	 
	// Šî–{ƒNƒ‰ƒX‚Ö
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	ƒŠƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL PPI::Reset()
{
	int i;

	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "ƒŠƒZƒbƒg");

	// ƒ|[ƒgC
	ppi.portc = 0;

	// ƒRƒ“ƒgƒ[ƒ‹
	for (i=0; i<PortMax; i++) {
		ppi.ctl[i] = 0;
	}

	// ƒWƒ‡ƒCƒXƒeƒBƒbƒNƒfƒoƒCƒX‚É‘Î‚µ‚ÄAƒŠƒZƒbƒg‚ð’Ê’m
	for (i=0; i<PortMax; i++) {
		joy[i]->Reset();
	}
}

//---------------------------------------------------------------------------
//
//	ƒZ[ƒu
//
//---------------------------------------------------------------------------
BOOL FASTCALL PPI::Save(Fileio *fio, int ver)
{
	size_t sz;
	int i;
	DWORD type;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);
	ASSERT_DIAG();

	LOG0(Log::Normal, "ƒZ[ƒu");

	// ƒTƒCƒY‚ðƒZ[ƒu
	sz = sizeof(ppi_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// ŽÀ‘Ì‚ðƒZ[ƒu
	if (!fio->Write(&ppi, (int)sz)) {
		return FALSE;
	}

	// ƒfƒoƒCƒX‚ðƒZ[ƒu
	for (i=0; i<PortMax; i++) {
		// ƒfƒoƒCƒXƒ^ƒCƒv
		type = joy[i]->GetType();
		if (!fio->Write(&type, sizeof(type))) {
			return FALSE;
		}

		// ƒfƒoƒCƒXŽÀ‘Ì
		if (!joy[i]->Save(fio, ver)) {
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh
//
//---------------------------------------------------------------------------
BOOL FASTCALL PPI::Load(Fileio *fio, int ver)
{
	size_t sz;
	int i;
	DWORD type;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);
	ASSERT_DIAG();

	LOG0(Log::Normal, "ƒ[ƒh");

	// ƒTƒCƒY‚ðƒ[ƒhAÆ‡
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(ppi_t)) {
		return FALSE;
	}

	// ŽÀ‘Ì‚ðƒ[ƒh
	if (!fio->Read(&ppi, (int)sz)) {
		return FALSE;
	}

	// version2.00‚Å‚ ‚ê‚Î‚±‚±‚Ü‚Å
	if (ver <= 0x200) {
		return TRUE;
	}

	// ƒfƒoƒCƒX‚ðƒ[ƒh
	for (i=0; i<PortMax; i++) {
		// ƒfƒoƒCƒXƒ^ƒCƒv‚ð“¾‚é
		if (!fio->Read(&type, sizeof(type))) {
			return FALSE;
		}

		// Œ»Ý‚ÌƒfƒoƒCƒX‚Æˆê’v‚µ‚Ä‚¢‚È‚¯‚ê‚ÎAì‚è’¼‚·
		if (joy[i]->GetType() != type) {
			delete joy[i];
			joy[i] = NULL;

			// PPI‚É‹L‰¯‚µ‚Ä‚¢‚éƒ^ƒCƒv‚à‚ ‚í‚¹‚Ä•ÏX
			ppi.type[i] = (int)type;

			// Äì¬
			joy[i] = CreateJoy(i, ppi.type[i]);
			ASSERT(joy[i]->GetType() == type);
		}

		// ƒfƒoƒCƒXŒÅ—L
		if (!joy[i]->Load(fio, ver)) {
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Ý’è“K—p
//
//---------------------------------------------------------------------------
void FASTCALL PPI::ApplyCfg(const Config *config)
{
	int i;

	ASSERT(this);
	ASSERT(config);
	ASSERT_DIAG();

	LOG0(Log::Normal, "Ý’è“K—p");

	// ƒ|[ƒgƒ‹[ƒv
	for (i=0; i<PortMax; i++) {
		// ƒ^ƒCƒvˆê’v‚È‚çŽŸ‚Ö
		if (config->joy_type[i] == ppi.type[i]) {
			continue;
		}

		// Œ»Ý‚ÌƒfƒoƒCƒX‚ðíœ
		ASSERT(joy[i]);
		delete joy[i];
		joy[i] = NULL;

		// ƒ^ƒCƒv‹L‰¯AƒWƒ‡ƒCƒXƒeƒBƒbƒNì¬
		ppi.type[i] = config->joy_type[i];
		joy[i] = CreateJoy(i, config->joy_type[i]);
	}
}

#if defined(_DEBUG)
//---------------------------------------------------------------------------
//
//	f’f
//
//---------------------------------------------------------------------------
void FASTCALL PPI::AssertDiag() const
{
	ASSERT(this);
	ASSERT(GetID() == MAKEID('P', 'P', 'I', ' '));
	ASSERT(adpcm);
	ASSERT(adpcm->GetID() == MAKEID('A', 'P', 'C', 'M'));
	ASSERT(joy[0]);
	ASSERT(joy[1]);
}
#endif	// _DEBUG

//---------------------------------------------------------------------------
//
//	ƒoƒCƒg“Ç‚Ýž‚Ý
//
//---------------------------------------------------------------------------
DWORD FASTCALL PPI::ReadByte(DWORD addr)
{
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(PortMax >= 2);
	ASSERT_DIAG();

	// Šï”ƒAƒhƒŒƒX‚Ì‚ÝƒfƒR[ƒh‚³‚ê‚Ä‚¢‚é
	if ((addr & 1) == 0) {
		return 0xff;
	}

	// 8ƒoƒCƒg’PˆÊ‚Åƒ‹[ƒv
	addr &= 7;

	// ƒEƒFƒCƒg
	scheduler->Wait(1);

	// ƒfƒR[ƒh
	addr >>= 1;
	switch (addr) {
		// ƒ|[ƒgA
		case 0:
#if defined(PPI_LOG)
			data = joy[0]->ReadPort(ppi.ctl[0]);
			LOG2(Log::Normal, "ƒ|[ƒg1“Ç‚Ýo‚µ ƒRƒ“ƒgƒ[ƒ‹$%02X ƒf[ƒ^$%02X",
								ppi.ctl[0], data);
#else
			data = joy[0]->ReadPort(ppi.ctl[0]);
#endif	// PPI_LOG

			// PC7,PC6‚ðl—¶
			if (ppi.ctl[0] & 0x80) {
				data &= ~0x40;
			}
			if (ppi.ctl[0] & 0x40) {
				data &= ~0x20;
			}
			return data;

		// ƒ|[ƒgB
		case 1:
#if defined(PPI_LOG)
			data = joy[1]->ReadPort(ppi.ctl[1]);
			LOG2(Log::Normal, "ƒ|[ƒg2“Ç‚Ýo‚µ ƒRƒ“ƒgƒ[ƒ‹$%02X ƒf[ƒ^$%02X",
								ppi.ctl[1], data);
			return data;
#else
			return joy[1]->ReadPort(ppi.ctl[1]);
#endif	// PPI_LOG

		// ƒ|[ƒgC
		case 2:
			return ppi.portc;
	}

	LOG1(Log::Warning, "–¢ŽÀ‘•ƒŒƒWƒXƒ^“Ç‚Ýž‚Ý R%02d", addr);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh“Ç‚Ýž‚Ý
//
//---------------------------------------------------------------------------
DWORD FASTCALL PPI::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT_DIAG();

	return (0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//	ƒoƒCƒg‘‚«ž‚Ý
//
//---------------------------------------------------------------------------
void FASTCALL PPI::WriteByte(DWORD addr, DWORD data)
{
	DWORD bit;
	int i;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// Šï”ƒAƒhƒŒƒX‚Ì‚ÝƒfƒR[ƒh‚³‚ê‚Ä‚¢‚é
	if ((addr & 1) == 0) {
		return;
	}

	// 8ƒoƒCƒg’PˆÊ‚Åƒ‹[ƒv
	addr &= 7;

	// ƒEƒFƒCƒg
	scheduler->Wait(1);

	// ƒ|[ƒgC‚Ö‚ÌWrite
	if (addr == 5) {
		// ƒWƒ‡ƒCƒXƒeƒBƒbƒNEADPCMƒRƒ“ƒgƒ[ƒ‹
		SetPortC(data);
		return;
	}

	// ƒ‚[ƒhƒRƒ“ƒgƒ[ƒ‹
	if (addr == 7) {
		if (data < 0x80) {
			// ƒrƒbƒgƒZƒbƒgEƒŠƒZƒbƒgƒ‚[ƒh
			i = ((data >> 1) & 0x07);
			bit = (DWORD)(1 << i);
			if (data & 1) {
				SetPortC(DWORD(ppi.portc | bit));
			}
			else {
				SetPortC(DWORD(ppi.portc & ~bit));
			}
			return;
		}

		// ƒ‚[ƒhƒRƒ“ƒgƒ[ƒ‹
		if (data != 0x92) {
			LOG0(Log::Warning, "ƒTƒ|[ƒgŠOƒ‚[ƒhŽw’è $%02X");
		}
		return;
	}

	LOG2(Log::Warning, "–¢ŽÀ‘•ƒŒƒWƒXƒ^‘‚«ž‚Ý R%02d <- $%02X",
							addr, data);
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh‘‚«ž‚Ý
//
//---------------------------------------------------------------------------
void FASTCALL PPI::WriteWord(DWORD addr, DWORD data)
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
//	“Ç‚Ýž‚Ý‚Ì‚Ý
//
//---------------------------------------------------------------------------
DWORD FASTCALL PPI::ReadOnly(DWORD addr) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(PortMax >= 2);
	ASSERT_DIAG();

	// Šï”ƒAƒhƒŒƒX‚Ì‚ÝƒfƒR[ƒh‚³‚ê‚Ä‚¢‚é
	if ((addr & 1) == 0) {
		return 0xff;
	}

	// 8ƒoƒCƒg’PˆÊ‚Åƒ‹[ƒv
	addr &= 7;

	// ƒfƒR[ƒh
	addr >>= 1;
	switch (addr) {
		// ƒ|[ƒgA
		case 0:
			// ƒf[ƒ^Žæ“¾
			data = joy[0]->ReadOnly(ppi.ctl[0]);

			// PC7,PC6‚ðl—¶
			if (ppi.ctl[0] & 0x80) {
				data &= ~0x40;
			}
			if (ppi.ctl[0] & 0x40) {
				data &= ~0x20;
			}
			return data;

		// ƒ|[ƒgB
		case 1:
			return joy[1]->ReadOnly(ppi.ctl[1]);

		// ƒ|[ƒgC
		case 2:
			return ppi.portc;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒgCƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL PPI::SetPortC(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);
	ASSERT(PortMax >= 2);
	ASSERT_DIAG();

	// ƒf[ƒ^‚ð‹L‰¯
	ppi.portc = data;
	static int portc_trace_count = 0;
	if (portc_trace_count < 24) {
		fprintf(stderr,
			"[vm][ppi-portc] n=%d data=$%02X ctl0=$%02X ctl1=$%02X pan=%u rate=%u\r\n",
			portc_trace_count,
			(unsigned)data,
			(unsigned)((ppi.portc & 0x03u)),
			(unsigned)((ppi.portc >> 2) & 0x03u),
			(unsigned)(ppi.portc & 3u),
			(unsigned)((ppi.portc >> 2) & 3u));
		++portc_trace_count;
	}

	// ƒRƒ“ƒgƒ[ƒ‹ƒf[ƒ^‘g‚Ý—§‚Ä(ƒ|[ƒgA)...bit0‚ªPC4Abit6,7‚ªPC6,7‚ðŽ¦‚·
	ppi.ctl[0] = ppi.portc & 0xc0;
	if (ppi.portc & 0x10) {
		ppi.ctl[0] |= 0x01;
	}
#if defined(PPI_LOG)
	LOG1(Log::Normal, "ƒ|[ƒg1 ƒRƒ“ƒgƒ[ƒ‹ $%02X", ppi.ctl[0]);
#endif	// PPI_LOG
	joy[0]->Control(ppi.ctl[0]);

	// ƒRƒ“ƒgƒ[ƒ‹ƒf[ƒ^‘g‚Ý—§‚Ä(ƒ|[ƒgB)...bit0‚ªPC5‚ðŽ¦‚·
	if (ppi.portc & 0x20) {
		ppi.ctl[1] = 0x01;
	}
	else {
		ppi.ctl[1] = 0x00;
	}
#if defined(PPI_LOG)
	LOG1(Log::Normal, "ƒ|[ƒg2 ƒRƒ“ƒgƒ[ƒ‹ $%02X", ppi.ctl[1]);
#endif	// PPI_LOG
	joy[1]->Control(ppi.ctl[1]);

	// ADPCMƒpƒ“ƒ|ƒbƒg
	adpcm->SetPanpot(data & 3);

	// ADPCM‘¬“x”ä—¦
	adpcm->SetRatio((data >> 2) & 3);
#if defined(XM6CORE_ENABLE_X68SOUND)
	Xm6X68Sound::WritePpi(static_cast<unsigned char>(data));
#endif
}

//---------------------------------------------------------------------------
//
//	“à•”ƒf[ƒ^Žæ“¾
//
//---------------------------------------------------------------------------
void FASTCALL PPI::GetPPI(ppi_t *buffer)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT_DIAG();

	// “à•”ƒ[ƒN‚ðƒRƒs[
	*buffer = ppi;
}

//---------------------------------------------------------------------------
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒNî•ñÝ’è
//
//---------------------------------------------------------------------------
void FASTCALL PPI::SetJoyInfo(int port, const joyinfo_t *info)
{
	ASSERT(this);
	ASSERT((port >= 0) && (port < PortMax));
	ASSERT(info);
	ASSERT(PortMax >= 2);
	ASSERT_DIAG();

	// ”äŠr‚µ‚ÄAˆê’v‚µ‚Ä‚¢‚ê‚Î‰½‚à‚µ‚È‚¢
	if (memcmp(&ppi.info[port], info, sizeof(joyinfo_t)) == 0) {
		return;
	}

	// •Û‘¶
	memcpy(&ppi.info[port], info, sizeof(joyinfo_t));

	// ‚»‚Ìƒ|[ƒg‚É‘Î‰ž‚·‚éƒWƒ‡ƒCƒXƒeƒBƒbƒNƒfƒoƒCƒX‚ÖA’Ê’m
	ASSERT(joy[port]);
	joy[port]->Notify();
}

//---------------------------------------------------------------------------
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒNî•ñŽæ“¾
//
//---------------------------------------------------------------------------
const PPI::joyinfo_t* FASTCALL PPI::GetJoyInfo(int port) const
{
	ASSERT(this);
	ASSERT((port >= 0) && (port < PortMax));
	ASSERT(PortMax >= 2);
	ASSERT_DIAG();

	return &(ppi.info[port]);
}

//---------------------------------------------------------------------------
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒNƒfƒoƒCƒXì¬
//
//---------------------------------------------------------------------------
void FASTCALL PPI::SetJoyType(int port, int type)
{
	ASSERT(this);
	ASSERT((port >= 0) && (port < PortMax));
	ASSERT(type >= 0);
	ASSERT(PortMax >= 2);

	if (ppi.type[port] == type) {
		return;
	}

	ASSERT(joy[port]);
	delete joy[port];
	joy[port] = NULL;

	ppi.type[port] = type;
	joy[port] = CreateJoy(port, type);
}

//---------------------------------------------------------------------------
//
//	Set joystick type
//
//---------------------------------------------------------------------------
JoyDevice* FASTCALL PPI::CreateJoy(int port, int type)
{
	ASSERT(this);
	ASSERT(type >= 0);
	ASSERT((port >= 0) && (port < PortMax));
	ASSERT(PortMax >= 2);

	// ƒ^ƒCƒv•Ê
	switch (type) {
		// Ú‘±‚È‚µ
		case 0:
			return new JoyDevice(this, port);

		// ATARI•W€
		case 1:
			return new JoyAtari(this, port);

		// ATARI•W€+START/SELCT
		case 2:
			return new JoyASS(this, port);

		// ƒTƒCƒo[ƒXƒeƒBƒbƒN(ƒAƒiƒƒO)
		case 3:
			return new JoyCyberA(this, port);

		// ƒTƒCƒo[ƒXƒeƒBƒbƒN(ƒfƒWƒ^ƒ‹)
		case 4:
			return new JoyCyberD(this, port);

		// MD3ƒ{ƒ^ƒ“
		case 5:
			return new JoyMd3(this, port);

		// MD6ƒ{ƒ^ƒ“
		case 6:
			return new JoyMd6(this, port);

		// CPSF-SFC
		case 7:
			return new JoyCpsf(this, port);

		// CPSF-MD
		case 8:
			return new JoyCpsfMd(this, port);

		// ƒ}ƒWƒJƒ‹ƒpƒbƒh
		case 9:
			return new JoyMagical(this, port);

		// XPD-1LR
		case 10:
			return new JoyLR(this, port);

		// ƒpƒbƒNƒ‰ƒ“ƒhê—pƒpƒbƒh
		case 11:
			return new JoyPacl(this, port);

		// BM68ê—pƒRƒ“ƒgƒ[ƒ‰
		case 12:
			return new JoyBM(this, port);

		// ‚»‚Ì‘¼
		default:
			ASSERT(FALSE);
			break;
	}

	// ’ÊíA‚±‚±‚É‚Í‚±‚È‚¢
	return new JoyDevice(this, port);
}

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒNƒfƒoƒCƒX
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyDevice::JoyDevice(PPI *parent, int no)
{
	ASSERT((no >= 0) || (no < PPI::PortMax));

	// ƒ^ƒCƒvNULL
	id = MAKEID('N', 'U', 'L', 'L');
	type = 0;

	// eƒfƒoƒCƒX(PPI)‚ð‹L‰¯Aƒ|[ƒg”Ô†Ý’è
	ppi = parent;
	port = no;

	// Ž²Eƒ{ƒ^ƒ“‚È‚µAƒfƒWƒ^ƒ‹Aƒf[ƒ^”0
	axes = 0;
	buttons = 0;
	analog = FALSE;
	datas = 0;

	// •\Ž¦
	axis_desc = NULL;
	button_desc = NULL;

	// ƒf[ƒ^ƒoƒbƒtƒ@‚ÍNULL
	data = NULL;

	// XVƒ`ƒFƒbƒN—v
	changed = TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒfƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyDevice::~JoyDevice()
{
	// ƒf[ƒ^ƒoƒbƒtƒ@‚ª‚ ‚ê‚Î‰ð•ú
	if (data) {
		delete[] data;
		data = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	ƒŠƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL JoyDevice::Reset()
{
	ASSERT(this);
}

//---------------------------------------------------------------------------
//
//	ƒZ[ƒu
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyDevice::Save(Fileio *fio, int /*ver*/)
{
	ASSERT(this);
	ASSERT(fio);

	// ƒf[ƒ^”‚ª0‚È‚çƒZ[ƒu‚µ‚È‚¢
	if (datas <= 0) {
		ASSERT(datas == 0);
		return TRUE;
	}

	// ƒf[ƒ^”‚¾‚¯•Û‘¶
	if (!fio->Write(data, sizeof(DWORD) * datas)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyDevice::Load(Fileio *fio, int /*ver*/)
{
	ASSERT(this);
	ASSERT(fio);

	// ƒf[ƒ^”‚ª0‚È‚çƒ[ƒh‚µ‚È‚¢
	if (datas <= 0) {
		ASSERT(datas == 0);
		return TRUE;
	}

	// XV‚ ‚è
	changed = TRUE;

	// ƒf[ƒ^ŽÀ‘Ì‚ðƒ[ƒh
	if (!fio->Read(data, sizeof(DWORD) * datas)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyDevice::ReadPort(DWORD ctl)
{
	ASSERT(this);
	ASSERT((port >= 0) && (port < PPI::PortMax));
	ASSERT(ppi);
	ASSERT(ctl < 0x100);

	// •ÏXƒtƒ‰ƒO‚ðƒ`ƒFƒbƒN
	if (changed) {
		// ƒtƒ‰ƒO—Ž‚Æ‚·
		changed = FALSE;

		// ƒf[ƒ^ì¬
		MakeData();
	}

	// ReadOnly‚Æ“¯‚¶ƒf[ƒ^‚ð•Ô‚·
	return ReadOnly(ctl);
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyDevice::ReadOnly(DWORD /*ctl*/) const
{
	ASSERT(this);

	// –¢Ú‘±
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒgƒ[ƒ‹
//
//---------------------------------------------------------------------------
void FASTCALL JoyDevice::Control(DWORD /*ctl*/)
{
	ASSERT(this);
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyDevice::MakeData()
{
	ASSERT(this);
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦
//
//---------------------------------------------------------------------------
const char* FASTCALL JoyDevice::GetAxisDesc(int axis) const
{
	ASSERT(this);
	ASSERT(axis >= 0);

	// Ž²”‚ð’´‚¦‚Ä‚¢‚ê‚ÎNULL
	if (axis >= axes) {
		return NULL;
	}

	// Ž²•\Ž¦ƒe[ƒuƒ‹‚ª‚È‚¯‚ê‚ÎNULL
	if (!axis_desc) {
		return NULL;
	}

	// Ž²•\Ž¦ƒe[ƒuƒ‹‚©‚ç•Ô‚·
	return axis_desc[axis];
}

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦
//
//---------------------------------------------------------------------------
const char* FASTCALL JoyDevice::GetButtonDesc(int button) const
{
	ASSERT(this);
	ASSERT(button >= 0);

	// ƒ{ƒ^ƒ“”‚ð’´‚¦‚Ä‚¢‚ê‚ÎNULL
	if (button >= buttons) {
		return NULL;
	}

	// ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹‚ª‚È‚¯‚ê‚ÎNULL
	if (!button_desc) {
		return NULL;
	}

	// ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹‚©‚ç•Ô‚·
	return button_desc[button];
}

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(ATARI•W€)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyAtari::JoyAtari(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvATAR
	id = MAKEID('A', 'T', 'A', 'R');
	type = 1;

	// 2Ž²2ƒ{ƒ^ƒ“Aƒf[ƒ^”1
	axes = 2;
	buttons = 2;
	datas = 1;

	// •\Ž¦ƒe[ƒuƒ‹
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyAtari::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);

	// PC4‚ª1‚È‚çA0xff
	if (ctl & 1) {
		return 0xff;
	}

	// ì¬Ï‚Ý‚Ìƒf[ƒ^‚ð•Ô‚·
	return data[0];
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyAtari::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“A
	if (info->button[0]) {
		data[0] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“B
	if (info->button[1]) {
		data[0] &= ~0x20;
	}
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyAtari::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyAtari::ButtonDescTable[] = {
	"A",
	"B"
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(ATARI•W€+START/SELECT)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyASS::JoyASS(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvATSS
	id = MAKEID('A', 'T', 'S', 'S');
	type = 2;

	// 2Ž²4ƒ{ƒ^ƒ“Aƒf[ƒ^”1
	axes = 2;
	buttons = 4;
	datas = 1;

	// •\Ž¦ƒe[ƒuƒ‹
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyASS::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);

	// PC4‚ª1‚È‚çA0xff
	if (ctl & 1) {
		return 0xff;
	}

	// ì¬Ï‚Ý‚Ìƒf[ƒ^‚ð•Ô‚·
	return data[0];
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyASS::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“A
	if (info->button[0]) {
		data[0] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“B
	if (info->button[1]) {
		data[0] &= ~0x20;
	}

	// START(¶‰E“¯Žž‰Ÿ‚µ‚Æ‚µ‚Ä•\Œ»)
	if (info->button[2]) {
		data[0] &= ~0x0c;
	}

	// SELECT(ã‰º“¯Žž‰Ÿ‚µ‚Æ‚µ‚Ä•\Œ»)
	if (info->button[3]) {
		data[0] &= ~0x03;
	}
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyASS::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyASS::ButtonDescTable[] = {
	"A",
	"B",
	"START",
	"SELECT"
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(ƒTƒCƒo[ƒXƒeƒBƒbƒNEƒAƒiƒƒO)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyCyberA::JoyCyberA(PPI *parent, int no) : JoyDevice(parent, no)
{
	int i;

	// ƒ^ƒCƒvCYBA
	id = MAKEID('C', 'Y', 'B', 'A');
	type = 3;

	// 3Ž²8ƒ{ƒ^ƒ“AƒAƒiƒƒOAƒf[ƒ^”11
	axes = 3;
	buttons = 8;
	analog = TRUE;
	datas = 12;

	// •\Ž¦ƒe[ƒuƒ‹
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	for (i=0; i<12; i++) {
		// ACK,L/H,ƒ{ƒ^ƒ“
		if (i & 1) {
			data[i] = 0xbf;
		}
		else {
			data[i] = 0x9f;
		}

		// ƒZƒ“ƒ^’l‚Í0x7f‚Æ‚·‚é
		if ((i >= 2) && (i <= 5)) {
			// ƒAƒiƒƒOƒf[ƒ^H‚ð7‚É‚·‚é
			data[i] &= 0xf7;
		}
	}

	// ƒXƒPƒWƒ…[ƒ‰Žæ“¾
	scheduler = (Scheduler*)ppi->GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(scheduler);

	// Ž©“®ƒŠƒZƒbƒg(ƒRƒ“ƒgƒ[ƒ‰‚ð·‚µ‘Ö‚¦‚½ê‡‚É”õ‚¦‚é)
	Reset();
}

//---------------------------------------------------------------------------
//
//	ƒŠƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL JoyCyberA::Reset()
{
	ASSERT(this);
	ASSERT(scheduler);

	// Šî–{ƒNƒ‰ƒX
	JoyDevice::Reset();

	// ƒV[ƒPƒ“ƒX‰Šú‰»
	seq = 0;

	// ƒRƒ“ƒgƒ[ƒ‹0
	ctrl = 0;

	// ŽžŠÔ‹L‰¯
	hus = scheduler->GetTotalTime();
}

//---------------------------------------------------------------------------
//
//	ƒZ[ƒu
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyCyberA::Save(Fileio *fio, int ver)
{
	ASSERT(this);
	ASSERT(fio);

	// Šî–{ƒNƒ‰ƒX
	if (!JoyDevice::Save(fio, ver)) {
		return FALSE;
	}

	// ƒV[ƒPƒ“ƒX‚ðƒZ[ƒu
	if (!fio->Write(&seq, sizeof(seq))) {
		return FALSE;
	}

	// ƒRƒ“ƒgƒ[ƒ‹‚ðƒZ[ƒu
	if (!fio->Write(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// ŽžŠÔ‚ðƒZ[ƒu
	if (!fio->Write(&hus, sizeof(hus))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyCyberA::Load(Fileio *fio, int ver)
{
	ASSERT(this);
	ASSERT(fio);

	// Šî–{ƒNƒ‰ƒX
	if (!JoyDevice::Load(fio, ver)) {
		return FALSE;
	}

	// ƒV[ƒPƒ“ƒX‚ðƒ[ƒh
	if (!fio->Read(&seq, sizeof(seq))) {
		return FALSE;
	}

	// ƒRƒ“ƒgƒ[ƒ‹‚ðƒ[ƒh
	if (!fio->Read(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// ŽžŠÔ‚ðƒ[ƒh
	if (!fio->Read(&hus, sizeof(hus))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	‰ž“š‘¬“xÝ’è(ŽÀ‹@‚Æ‚Ì”äŠr‚ÉŠî‚Ã‚­)
//	¦³Šm‚É‚ÍPC4‚ðÅ‰‚É—§‚Ä‚Ä‚©‚çAPA4‚Ìb5¨b6‚ª—Ž‚¿‚é‚Ü‚Å‚É100usˆÈã
//	‚©‚©‚Á‚Ä‚¢‚é•µˆÍ‹C‚ª‚ ‚é‚ªA‚»‚±‚Ü‚Å‚ÍƒGƒ~ƒ…ƒŒ[ƒVƒ‡ƒ“‚µ‚È‚¢
//
//---------------------------------------------------------------------------
#define JOYCYBERA_CYCLE		108

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCyberA::ReadPort(DWORD ctl)
{
	DWORD diff;
	DWORD n;

	// ƒV[ƒPƒ“ƒX0‚Í–³Œø
	if (seq == 0) {
		return 0xff;
	}

	// ƒV[ƒPƒ“ƒX12ˆÈã‚à–³Œø
	if (seq >= 13) {
		// ƒV[ƒPƒ“ƒX0
		seq = 0;
		return 0xff;
	}

	// •ÏXƒtƒ‰ƒO‚ðƒ`ƒFƒbƒN
	if (changed) {
		// ƒtƒ‰ƒO—Ž‚Æ‚·
		changed = FALSE;

		// ƒf[ƒ^ì¬
		MakeData();
	}

	ASSERT((seq >= 1) && (seq <= 12));

	// ·•ªŽæ“¾
	diff = scheduler->GetTotalTime();
	diff -= hus;

	// ·•ª‚©‚çŒvŽZ
	if (diff >= JOYCYBERA_CYCLE) {
		n = diff / JOYCYBERA_CYCLE;
		diff %= JOYCYBERA_CYCLE;

		// ƒV[ƒPƒ“ƒX‚ðƒŠƒZƒbƒg
		if ((seq & 1) == 0) {
			seq--;
		}
		// 2’PˆÊ‚Åi‚ß‚é
		seq += (2 * n);

		// ŽžŠÔ‚ð•â³
		hus += (JOYCYBERA_CYCLE * n);

		// +1
		if (diff >= (JOYCYBERA_CYCLE / 2)) {
			diff -= (JOYCYBERA_CYCLE / 2);
			seq++;
		}

		// ƒV[ƒPƒ“ƒXƒI[ƒo[‘Îô
		if (seq >= 13) {
			seq = 0;
			return 0xff;
		}
	}
	if (diff >= (JOYCYBERA_CYCLE / 2)) {
		// Œã”¼‚ÉÝ’è
		if (seq & 1) {
			seq++;
		}
	}

	// ƒf[ƒ^Žæ“¾
	return ReadOnly(ctl);
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCyberA::ReadOnly(DWORD /*ctl*/) const
{
	ASSERT(this);

	// ƒV[ƒPƒ“ƒX0‚Í–³Œø
	if (seq == 0) {
		return 0xff;
	}

	// ƒV[ƒPƒ“ƒX12ˆÈã‚à–³Œø
	if (seq >= 13) {
		return 0xff;
	}

	// ƒV[ƒPƒ“ƒX‚É]‚Á‚½ƒf[ƒ^‚ð•Ô‚·
	ASSERT((seq >= 1) && (seq <= 12));
	return data[seq - 1];
}

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒgƒ[ƒ‹
//
//---------------------------------------------------------------------------
void FASTCALL JoyCyberA::Control(DWORD ctl)
{
	ASSERT(this);
	ASSERT(ctl < 0x100);

	// ƒV[ƒPƒ“ƒX0(–³Œø)Žž‚ÆƒV[ƒPƒ“ƒX11ˆÈ~
	if ((seq == 0) || (seq >= 11)) {
		// 1¨0‚ÅAƒV[ƒPƒ“ƒXŠJŽn
		if (ctl) {
			// ¡‰ñ1‚É‚µ‚½
			ctrl = 1;
		}
		else {
			// ¡‰ñ0‚É‚µ‚½
			if (ctrl) {
				// 1¨0‚Ö‚Ì—§‚¿‰º‚°
				seq = 1;
				hus = scheduler->GetTotalTime();
			}
			ctrl = 0;
		}
		return;
	}

	// ƒV[ƒPƒ“ƒX1ˆÈ~‚Å‚ÍAACK‚Ì‚Ý—LŒø
	ctrl = ctl;
	if (ctl) {
		return;
	}

	// ƒV[ƒPƒ“ƒX‚ð2’PˆÊ‚Åi‚ß‚éŒø‰Ê‚ð‚à‚Â
	if ((seq & 1) == 0) {
		seq--;
	}
	seq += 2;

	// ŽžŠÔ‚ð‹L‰¯
	hus = scheduler->GetTotalTime();
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyCyberA::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// ƒWƒ‡ƒCƒXƒeƒBƒbƒNî•ñŽæ“¾
	info = ppi->GetJoyInfo(port);

	// data[0]:ƒ{ƒ^ƒ“A,ƒ{ƒ^ƒ“B,ƒ{ƒ^ƒ“C,ƒ{ƒ^ƒ“D
	data[0] |= 0x0f;
	if (info->button[0]) {
		data[0] &= ~0x08;
	}
	if (info->button[1]) {
		data[0] &= ~0x04;
	}
	if (info->button[2]) {
		data[0] &= ~0x02;
	}
	if (info->button[3]) {
		data[0] &= ~0x01;
	}

	// data[1]:ƒ{ƒ^ƒ“E1,ƒ{ƒ^ƒ“E2,ƒ{ƒ^ƒ“F,ƒ{ƒ^ƒ“G
	data[1] |= 0x0f;
	if (info->button[4]) {
		data[1] &= ~0x08;
	}
	if (info->button[5]) {
		data[1] &= ~0x04;
	}
	if (info->button[6]) {
		data[1] &= ~0x02;
	}
	if (info->button[7]) {
		data[1] &= ~0x01;
	}

	// data[2]:1H
	axis = info->axis[1];
	axis = (axis + 0x800) >> 4;
	data[2] &= 0xf0;
	data[2] |= (axis >> 4);

	// data[3]:2H
	axis = info->axis[0];
	axis = (axis + 0x800) >> 4;
	data[3] &= 0xf0;
	data[3] |= (axis >> 4);

	// data[4]:3H
	axis = info->axis[3];
	axis = (axis + 0x800) >> 4;
	data[4] &= 0xf0;
	data[4] |= (axis >> 4);

	// data[5]:4H(—\–ñ‚Æ‚È‚Á‚Ä‚¢‚éBŽÀ‹@‚Å‚Í0)
	data[5] &= 0xf0;

	// data[6]:1L
	axis = info->axis[1];
	axis = (axis + 0x800) >> 4;
	data[6] &= 0xf0;
	data[6] |= (axis & 0x0f);

	// data[7]:2L
	axis = info->axis[0];
	axis = (axis + 0x800) >> 4;
	data[7] &= 0xf0;
	data[7] |= (axis & 0x0f);

	// data[8]:3L
	axis = info->axis[3];
	axis = (axis + 0x800) >> 4;
	data[8] &= 0xf0;
	data[8] |= (axis & 0x0f);

	// data[9]:4L(—\–ñ‚Æ‚È‚Á‚Ä‚¢‚éBŽÀ‹@‚Å‚Í0)
	data[9] &= 0xf0;

	// data[10]:A,B,A',B'
	// A,B‚ÍƒŒƒo[ˆê‘Ì‚Ìƒ~ƒjƒ{ƒ^ƒ“AA'B'‚Í’Êí‚Ì‰Ÿ‚µƒ{ƒ^ƒ“
	// ƒŒƒo[ˆê‘Ì‚Ìƒ~ƒjƒ{ƒ^ƒ“‚Æ‚µ‚Äˆµ‚¤(ƒAƒtƒ^[ƒo[ƒi[II)
	data[10] &= 0xf0;
	data[10] |= 0x0f;
	if (info->button[0]) {
		data[10] &= ~0x08;
	}
	if (info->button[1]) {
		data[10] &= ~0x04;
	}
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyCyberA::AxisDescTable[] = {
	"Stick X",
	"Stick Y",
	"Throttle"
};

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyCyberA::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"D",
	"E1",
	"E2",
	"START",
	"SELECT"
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(ƒTƒCƒo[ƒXƒeƒBƒbƒNEƒfƒWƒ^ƒ‹)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyCyberD::JoyCyberD(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvCYBD
	id = MAKEID('C', 'Y', 'B', 'D');
	type = 4;

	// 3Ž²6ƒ{ƒ^ƒ“Aƒf[ƒ^”2
	axes = 3;
	buttons = 6;
	datas = 2;

	// •\Ž¦ƒe[ƒuƒ‹
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCyberD::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// PC4‚É‚æ‚Á‚Ä•ª‚¯‚é
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyCyberD::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;
	data[1] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“A
	if (info->button[0]) {
		data[0] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“B
	if (info->button[1]) {
		data[0] &= ~0x40;
	}

	// ƒXƒƒbƒgƒ‹Up
	axis = info->axis[2];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x01;
	}
	// ƒXƒƒbƒgƒ‹Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x02;
	}

	// ƒ{ƒ^ƒ“C
	if (info->button[2]) {
		data[1] &= ~0x04;
	}

	// ƒ{ƒ^ƒ“D
	if (info->button[3]) {
		data[1] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“E1
	if (info->button[4]) {
		data[1] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“E2
	if (info->button[5]) {
		data[1] &= ~0x40;
	}
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyCyberD::AxisDescTable[] = {
	"X",
	"Y",
	"Throttle"
};

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyCyberD::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"D",
	"E1",
	"E2"
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(MD3ƒ{ƒ^ƒ“)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyMd3::JoyMd3(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvMD3B
	id = MAKEID('M', 'D', '3', 'B');
	type = 5;

	// 2Ž²4ƒ{ƒ^ƒ“Aƒf[ƒ^”2
	axes = 2;
	buttons = 4;
	datas = 2;

	// •\Ž¦ƒe[ƒuƒ‹
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xf3;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyMd3::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// PC4‚É‚æ‚Á‚Ä•ª‚¯‚é
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyMd3::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xf3;
	data[1] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x01;
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x02;
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“B
	if (info->button[1]) {
		data[1] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“C
	if (info->button[2]) {
		data[1] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“A
	if (info->button[0]) {
		data[0] &= ~0x20;
	}

	// ƒXƒ^[ƒgƒ{ƒ^ƒ“
	if (info->button[3]) {
		data[0] &= ~0x40;
	}
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyMd3::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyMd3::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"START"
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(MD6ƒ{ƒ^ƒ“)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyMd6::JoyMd6(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvMD6B
	id = MAKEID('M', 'D', '6', 'B');
	type = 6;

	// 2Ž²8ƒ{ƒ^ƒ“Aƒf[ƒ^”3
	axes = 2;
	buttons = 8;
	datas = 5;

	// •\Ž¦ƒe[ƒuƒ‹
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xf3;
	data[1] = 0xff;
	data[2] = 0xf0;
	data[3] = 0xff;
	data[4] = 0xff;

	// ƒXƒPƒWƒ…[ƒ‰Žæ“¾
	scheduler = (Scheduler*)ppi->GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(scheduler);

	// Ž©“®ƒŠƒZƒbƒg(ƒRƒ“ƒgƒ[ƒ‰‚ð·‚µ‘Ö‚¦‚½ê‡‚É”õ‚¦‚é)
	Reset();
}

//---------------------------------------------------------------------------
//
//	ƒŠƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL JoyMd6::Reset()
{
	ASSERT(this);
	ASSERT(scheduler);

	// Šî–{ƒNƒ‰ƒX
	JoyDevice::Reset();

	// ƒV[ƒPƒ“ƒXAƒRƒ“ƒgƒ[ƒ‹AŽžŠÔ‚ð‰Šú‰»
	seq = 0;
	ctrl = 0;
	hus = scheduler->GetTotalTime();
}

//---------------------------------------------------------------------------
//
//	ƒZ[ƒu
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyMd6::Save(Fileio *fio, int ver)
{
	ASSERT(this);
	ASSERT(fio);

	// Šî–{ƒNƒ‰ƒX
	if (!JoyDevice::Save(fio, ver)) {
		return FALSE;
	}

	// ƒV[ƒPƒ“ƒX‚ðƒZ[ƒu
	if (!fio->Write(&seq, sizeof(seq))) {
		return FALSE;
	}

	// ƒRƒ“ƒgƒ[ƒ‹‚ðƒZ[ƒu
	if (!fio->Write(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// ŽžŠÔ‚ðƒZ[ƒu
	if (!fio->Write(&hus, sizeof(hus))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh
//
//---------------------------------------------------------------------------
BOOL FASTCALL JoyMd6::Load(Fileio *fio, int ver)
{
	ASSERT(this);
	ASSERT(fio);

	// Šî–{ƒNƒ‰ƒX
	if (!JoyDevice::Load(fio, ver)) {
		return FALSE;
	}

	// ƒV[ƒPƒ“ƒX‚ðƒ[ƒh
	if (!fio->Read(&seq, sizeof(seq))) {
		return FALSE;
	}

	// ƒRƒ“ƒgƒ[ƒ‹‚ðƒ[ƒh
	if (!fio->Read(&ctrl, sizeof(ctrl))) {
		return FALSE;
	}

	// ŽžŠÔ‚ðƒ[ƒh
	if (!fio->Read(&hus, sizeof(hus))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyMd6::ReadOnly(DWORD /*ctl*/) const
{
	ASSERT(this);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);
	ASSERT(data[2] < 0x100);
	ASSERT(data[3] < 0x100);
	ASSERT(data[4] < 0x100);

	// ƒV[ƒPƒ“ƒX•Ê
	switch (seq) {
		// ‰Šúó‘Ô CTL=0
		case 0:
			return data[0];

		// 1‰ñ–Ú CTL=1
		case 1:
			return data[1];

		// 1‰ñ–Ú CTL=0
		case 2:
			return data[0];

		// 2‰ñ–Ú CTL=1
		case 3:
			return data[1];

		// 6BŠm’èŒã CTL=0
		case 4:
			return data[2];

		// 6BŠm’èŒã CTL=1
		case 5:
			return data[3];

		// 6BŠm’èŒã CTL=0
		case 6:
			return data[4];

		// 6BŠm’èŒã CTL=1
		case 7:
			return data[1];

		// 6BŠm’èŒã CTL=0
		case 8:
			return data[0];

		// ‚»‚Ì‘¼(‚ ‚è“¾‚È‚¢)
		default:
			ASSERT(FALSE);
			break;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒgƒ[ƒ‹
//
//---------------------------------------------------------------------------
void FASTCALL JoyMd6::Control(DWORD ctl)
{
	DWORD diff;

	ASSERT(this);
	ASSERT(ctl < 0x100);

	// bit0‚Ì‚Ý•K—v
	ctl &= 0x01;

	// •K‚¸XV
	ctrl = ctl;

	// seq >= 3‚È‚çA‘O‰ñ‚Ì‹N“®‚©‚ç1.8ms(3600hus)Œo‰ß‚µ‚½‚©ƒ`ƒFƒbƒN
	// Œo‰ß‚µ‚Ä‚¢‚ê‚ÎAseq=0 or seq=1‚ÉƒŠƒZƒbƒg(—’éí‹LV4)
	if (seq >= 3) {
		diff = scheduler->GetTotalTime();
		diff -= hus;
		if (diff >= 3600) {
			// ƒŠƒZƒbƒg
			if (ctl) {
				seq = 1;
				hus = scheduler->GetTotalTime();
			}
			else {
				seq = 0;
			}
			return;
		}
	}

	switch (seq) {
		// ƒV[ƒPƒ“ƒXŠO CTL=0
		case 0:
			// 1‚È‚çAƒV[ƒPƒ“ƒX1‚ÆŽžŠÔ‹L‰¯
			if (ctl) {
				seq = 1;
				hus = scheduler->GetTotalTime();
			}
			break;

		// Å‰‚Ì1‚ÌŒã CTL=1
		case 1:
			// 0‚È‚çAƒV[ƒPƒ“ƒX2
			if (!ctl) {
				seq = 2;
			}
			break;

		// 1¨0‚ÌŒã CTL=0
		case 2:
			// 1‚È‚çAŽžŠÔƒ`ƒFƒbƒN
			if (ctl) {
				diff = scheduler->GetTotalTime();
				diff -= hus;
				if (diff <= 2200) {
					// 1.1ms(2200hus)ˆÈ‰º‚È‚çŽŸ‚ÌƒV[ƒPƒ“ƒX‚Ö(6B“Ç‚ÝŽæ‚è)
					seq = 3;
				}
				else {
					// \•ªŽžŠÔ‚ª‹ó‚¢‚Ä‚¢‚é‚Ì‚ÅAƒV[ƒPƒ“ƒX1‚Æ“¯‚¶‚Æ‚Ý‚È‚·(3B“Ç‚ÝŽæ‚è)
					seq = 1;
					hus = scheduler->GetTotalTime();
				}
			}
			break;

		// 6BŠm’èŒã CTL=1
		case 3:
			if (!ctl) {
				seq = 4;
			}
			break;

		// 6BŠm’èŒã CTL=0
		case 4:
			if (ctl) {
				seq = 5;
			}
			break;

		// 6BŠm’èŒã CTL=1
		case 5:
			if (!ctl) {
				seq = 6;
			}
			break;

		// 6BŠm’èŒã CTL=0
		case 6:
			if (ctl) {
				seq = 7;
			}
			break;

		// 1.8ms‚Ì‘Ò‚¿
		case 7:
			if (!ctl) {
				seq = 8;
			}
			break;

		// 1.8ms‚Ì‘Ò‚¿
		case 8:
			if (ctl) {
				seq = 7;
			}
			break;

		// ‚»‚Ì‘¼(‚ ‚è“¾‚È‚¢)
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyMd6::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xf3;
	data[1] = 0xff;
	data[2] = 0xf0;
	data[3] = 0xff;
	data[4] = 0xff;

	// Up(data[0], data[1], data[4])
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
		data[1] &= ~0x01;
		data[4] &= ~0x01;
	}
	// Down(data[0], data[1], data[4])
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
		data[1] &= ~0x02;
		data[4] &= ~0x02;
	}

	// Left(data[1], data[4])
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x04;
		data[4] &= ~0x04;
	}
	// Right(data[1], data[4])
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x08;
		data[4] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“B(data[1], data[3], data[4])
	if (info->button[1]) {
		// 3BŒÝŠ·
		data[1] &= ~0x20;

		// (—’éí‹LV4)
		data[3] &= ~0x20;

		// (SFII'patch)
		data[4] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“C(data[1], data[3])
	if (info->button[2]) {
		// 3BŒÝŠ·
		data[1] &= ~0x40;

		// (SFII'patch)
		data[3] &= ~0x20;

		// (—’éí‹LV4)
		data[3] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“A(data[0], data[2], data[4])
	if (info->button[0]) {
		// 3BŒÝŠ·
		data[0] &= ~0x20;

		// 6Bƒ}[ƒJ
		data[2] &= ~0x20;

		// (SFII'patch)
		data[4] &= ~0x20;
	}

	// ƒXƒ^[ƒgƒ{ƒ^ƒ“(data[0], data[2])
	if (info->button[6]) {
		// 3BŒÝŠ·
		data[0] &= ~0x40;

		// 6Bƒ}[ƒJ
		data[2] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“X(data[3])
	if (info->button[3]) {
		data[3] &= ~0x04;
	}

	// ƒ{ƒ^ƒ“Y(data[3])
	if (info->button[4]) {
		data[3] &= ~0x02;
	}

	// ƒ{ƒ^ƒ“Z(data[3])
	if (info->button[5]) {
		data[3] &= ~0x01;
	}

	// MODEƒ{ƒ^ƒ“(data[3])
	if (info->button[7]) {
		data[3] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyMd6::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyMd6::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"X",
	"Y",
	"Z",
	"START",
	"MODE"
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(CPSF-SFC)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyCpsf::JoyCpsf(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvCPSF
	id = MAKEID('C', 'P', 'S', 'F');
	type = 7;

	// 2Ž²8ƒ{ƒ^ƒ“Aƒf[ƒ^”2
	axes = 2;
	buttons = 8;
	datas = 2;

	// •\Ž¦ƒe[ƒuƒ‹
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCpsf::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// PC4‚É‚æ‚Á‚Ä•ª‚¯‚é
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyCpsf::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;
	data[1] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}


	// ƒ{ƒ^ƒ“Y
	if (info->button[0]) {
		data[1] &= ~0x02;
	}

	// ƒ{ƒ^ƒ“X
	if (info->button[1]) {
		data[1] &= ~0x04;
	}

	// ƒ{ƒ^ƒ“B
	if (info->button[2]) {
		data[0] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“A
	if (info->button[3]) {
		data[0] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“L
	if (info->button[4]) {
		data[1] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“R
	if (info->button[5]) {
		data[1] &= ~0x01;
	}


/* CPSFMD

// ƒ{ƒ^ƒ“A
	if (info->button[0]) {
		data[0] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“B
	if (info->button[1]) {
		data[0] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“C
	if (info->button[2]) {
		data[1] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“X
	if (info->button[3]) {
		data[1] &= ~0x04;
	}

	// ƒ{ƒ^ƒ“Y
	if (info->button[4]) {
		data[1] &= ~0x02;
	}

	// ƒ{ƒ^ƒ“Z
	if (info->button[5]) {
		data[1] &= ~0x01;
	}
	*/



	// ƒXƒ^[ƒgƒ{ƒ^ƒ“
	if (info->button[6]) {
		data[1] &= ~0x40;
	}

	// ƒZƒŒƒNƒgƒ{ƒ^ƒ“
	if (info->button[7]) {
		data[1] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyCpsf::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyCpsf::ButtonDescTable[] = {
	"Y",
	"X",
	"B",
	"A",
	"L",
	"R",
	"START",
	"SELECT",
	"ALT"
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(CPSF-MD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyCpsfMd::JoyCpsfMd(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvCPSM
	id = MAKEID('C', 'P', 'S', 'M');
	type = 8;

	// 2Ž²8ƒ{ƒ^ƒ“Aƒf[ƒ^”2
	axes = 2;
	buttons = 8;
	datas = 2;

	// •\Ž¦ƒe[ƒuƒ‹
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyCpsfMd::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// PC4‚É‚æ‚Á‚Ä•ª‚¯‚é
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyCpsfMd::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;
	data[1] = 0xff;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“A
	if (info->button[0]) {
		data[0] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“B
	if (info->button[1]) {
		data[0] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“C
	if (info->button[2]) {
		data[1] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“X
	if (info->button[3]) {
		data[1] &= ~0x04;
	}

	// ƒ{ƒ^ƒ“Y
	if (info->button[4]) {
		data[1] &= ~0x02;
	}

	// ƒ{ƒ^ƒ“Z
	if (info->button[5]) {
		data[1] &= ~0x01;
	}

	// ƒXƒ^[ƒgƒ{ƒ^ƒ“
	if (info->button[6]) {
		data[1] &= ~0x40;
	}

	// MODEƒ{ƒ^ƒ“
	if (info->button[7]) {
		data[1] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyCpsfMd::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyCpsfMd::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"X",
	"Y",
	"Z",
	"START",
	"MODE"
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(ƒ}ƒWƒJƒ‹ƒpƒbƒh)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyMagical::JoyMagical(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvMAGI
	id = MAKEID('M', 'A', 'G', 'I');
	type = 9;

	// 2Ž²6ƒ{ƒ^ƒ“Aƒf[ƒ^”2
	axes = 2;
	buttons = 6;
	datas = 2;

	// •\Ž¦ƒe[ƒuƒ‹
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xff;
	data[1] = 0xfc;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyMagical::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// PC4‚É‚æ‚Á‚Ä•ª‚¯‚é
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyMagical::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;
	data[1] = 0xfc;

	// Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“A
	if (info->button[0]) {
		data[0] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“B
	if (info->button[1]) {
		data[1] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“C
	if (info->button[2]) {
		data[0] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“D
	if (info->button[3]) {
		data[1] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“R
	if (info->button[4]) {
		data[1] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“L
	if (info->button[5]) {
		data[1] &= ~0x04;
	}
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyMagical::AxisDescTable[] = {
	"X",
	"Y"
};

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyMagical::ButtonDescTable[] = {
	"A",
	"B",
	"C",
	"D",
	"R",
	"L"
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(XPD-1LR)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyLR::JoyLR(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvXPLR
	id = MAKEID('X', 'P', 'L', 'R');
	type = 10;

	// 4Ž²2ƒ{ƒ^ƒ“Aƒf[ƒ^”2
	axes = 4;
	buttons = 2;
	datas = 2;

	// •\Ž¦ƒe[ƒuƒ‹
	axis_desc = AxisDescTable;
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xff;
	data[1] = 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyLR::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);
	ASSERT(data[1] < 0x100);

	// PC4‚É‚æ‚Á‚Ä•ª‚¯‚é
	if (ctl & 1) {
		return data[1];
	}
	else {
		return data[0];
	}
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyLR::MakeData()
{
	const PPI::joyinfo_t *info;
	DWORD axis;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;
	data[1] = 0xff;

	// ‰E‘¤Up
	axis = info->axis[3];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x01;
	}
	// ‰E‘¤Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x02;
	}

	// ‰E‘¤Left
	axis = info->axis[2];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[1] &= ~0x04;
	}
	// ‰E‘¤Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[1] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“A
	if (info->button[0]) {
		data[1] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“B
	if (info->button[1]) {
		data[1] &= ~0x20;
	}

	// ¶‘¤Up
	axis = info->axis[1];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x01;
	}
	// ¶‘¤Down
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x02;
	}

	// ‰E‘¤Left
	axis = info->axis[0];
	if ((axis >= 0xfffff800) && (axis < 0xfffffc00)) {
		data[0] &= ~0x04;
	}
	// ‰E‘¤Right
	if ((axis >= 0x00000400) && (axis < 0x00000800)) {
		data[0] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“A
	if (info->button[0]) {
		data[0] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“B
	if (info->button[1]) {
		data[0] &= ~0x20;
	}
}

//---------------------------------------------------------------------------
//
//	Ž²•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyLR::AxisDescTable[] = {
	"Left-X",
	"Left-Y",
	"Right-X",
	"Right-Y"
};

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyLR::ButtonDescTable[] = {
	"A",
	"B"
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(ƒpƒbƒNƒ‰ƒ“ƒhê—pƒpƒbƒh)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyPacl::JoyPacl(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvPACL
	id = MAKEID('P', 'A', 'C', 'L');
	type = 11;

	// 0Ž²3ƒ{ƒ^ƒ“Aƒf[ƒ^”1
	axes = 0;
	buttons = 3;
	datas = 1;

	// •\Ž¦ƒe[ƒuƒ‹
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyPacl::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);

	// PC4‚ª1‚È‚çA0xff
	if (ctl & 1) {
		return 0xff;
	}

	// ì¬Ï‚Ý‚Ìƒf[ƒ^‚ð•Ô‚·
	return data[0];
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyPacl::MakeData()
{
	const PPI::joyinfo_t *info;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;

	// ƒ{ƒ^ƒ“A(Left)
	if (info->button[0]) {
		data[0] &= ~0x04;
	}

	// ƒ{ƒ^ƒ“B(Jump)
	if (info->button[1]) {
		data[0] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“C(Right)
	if (info->button[2]) {
		data[0] &= ~0x08;
	}
}

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyPacl::ButtonDescTable[] = {
	"Left",
	"Jump",
	"Right",
};

//===========================================================================
//
//	ƒWƒ‡ƒCƒXƒeƒBƒbƒN(BM68ê—pƒRƒ“ƒgƒ[ƒ‰)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
JoyBM::JoyBM(PPI *parent, int no) : JoyDevice(parent, no)
{
	// ƒ^ƒCƒvBM68
	id = MAKEID('B', 'M', '6', '8');
	type = 12;

	// 0Ž²6ƒ{ƒ^ƒ“Aƒf[ƒ^”1
	axes = 0;
	buttons = 6;
	datas = 1;

	// •\Ž¦ƒe[ƒuƒ‹
	button_desc = ButtonDescTable;

	// ƒf[ƒ^ƒoƒbƒtƒ@Šm•Û
	data = new DWORD[datas];

	// ‰Šúƒf[ƒ^Ý’è
	data[0] = 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ|[ƒg“Ç‚ÝŽæ‚è(Read Only)
//
//---------------------------------------------------------------------------
DWORD FASTCALL JoyBM::ReadOnly(DWORD ctl) const
{
	ASSERT(this);
	ASSERT(ctl < 0x100);
	ASSERT(data[0] < 0x100);

	// PC4‚ª1‚È‚çA0xff
	if (ctl & 1) {
		return 0xff;
	}

	// ì¬Ï‚Ý‚Ìƒf[ƒ^‚ð•Ô‚·
	return data[0];
}

//---------------------------------------------------------------------------
//
//	ƒf[ƒ^ì¬
//
//---------------------------------------------------------------------------
void FASTCALL JoyBM::MakeData()
{
	const PPI::joyinfo_t *info;

	ASSERT(this);
	ASSERT(ppi);

	// ƒf[ƒ^‰Šú‰»
	info = ppi->GetJoyInfo(port);
	data[0] = 0xff;

	// ƒ{ƒ^ƒ“1(C)
	if (info->button[0]) {
		data[0] &= ~0x08;
	}

	// ƒ{ƒ^ƒ“2(C+,D-)
	if (info->button[1]) {
		data[0] &= ~0x04;
	}

	// ƒ{ƒ^ƒ“3(D)
	if (info->button[2]) {
		data[0] &= ~0x40;
	}

	// ƒ{ƒ^ƒ“4(D+,E-)
	if (info->button[3]) {
		data[0] &= ~0x20;
	}

	// ƒ{ƒ^ƒ“5(E)
	if (info->button[4]) {
		data[0] &= ~0x02;
	}

	// ƒ{ƒ^ƒ“F(Hat)
	if (info->button[5]) {
		data[0] &= ~0x01;
	}
}

//---------------------------------------------------------------------------
//
//	ƒ{ƒ^ƒ“•\Ž¦ƒe[ƒuƒ‹
//
//---------------------------------------------------------------------------
const char* JoyBM::ButtonDescTable[] = {
	"C",
	"C#",
	"D",
	"D#",
	"E",
	"HiHat"
};

