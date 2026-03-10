//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ CRTC(VICON) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "tvram.h"
#include "mfp.h"
#include "sprite.h"
#include "render.h"
#include "schedule.h"
#include "cpu.h"
#include "gvram.h"
#include "printer.h"
#include "fileio.h"
#include "crtc.h"
#include "config.h"

//===========================================================================
//
//	CRTC
//
//===========================================================================
//#define CRTC_LOG

namespace {
BOOL g_alt_raster_timing = FALSE;
}

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
CRTC::CRTC(VM *p) : MemDevice(p)
{
	// �f�o�C�XID��������
	dev.id = MAKEID('C', 'R', 'T', 'C');
	dev.desc = "CRTC (VICON)";

	// �J�n�A�h���X�A�I���A�h���X
	memdev.first = 0xe80000;
	memdev.last = 0xe81fff;

	// ���̑����[�N
	tvram = NULL;
	gvram = NULL;
	sprite = NULL;
	mfp = NULL;
	render = NULL;
	printer = NULL;
}

//---------------------------------------------------------------------------
//
//	������
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Init()
{
	ASSERT(this);

	// ��{�N���X
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// �e�L�X�gVRAM���擾
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);

	// �O���t�B�b�NVRAM���擾
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);

	// �X�v���C�g�R���g���[�����擾
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);

	// MFP���擾
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(mfp);

	// �����_�����擾
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// �v�����^���擾
	printer = (Printer*)vm->SearchDevice(MAKEID('P', 'R', 'N', ' '));
	ASSERT(printer);

	// �C�x���g������
	event.SetDevice(this);
	event.SetDesc("H-Sync");
	event.SetTime(0);
	scheduler->AddEvent(&event);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�N���[���A�b�v
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Cleanup()
{
	ASSERT(this);

	// ��{�N���X��
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	���Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Reset()
{
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "���Z�b�g");

	// ���W�X�^���N���A
	memset(crtc.reg, 0, sizeof(crtc.reg));
	for (i=0; i<18; i++) {
		crtc.reg[i] = ResetTable[i];
	}
	for (i=0; i<8; i++) {
		crtc.reg[i + 0x28] = ResetTable[i + 18];
	}

	// �𑜓x
	crtc.hrl = FALSE;
	crtc.lowres = FALSE;
	crtc.textres = TRUE;
	crtc.changed = FALSE;

	// ����@�
	crtc.raster_count = 0;
	crtc.raster_int = 0;
	crtc.raster_copy = FALSE;
	crtc.raster_exec = FALSE;
	crtc.fast_clr = 0;

	// ����
	crtc.h_sync = 31745;
	crtc.h_pulse = 3450;
	crtc.h_back = 4140;
	crtc.h_front = 2070;
	crtc.h_dots = 768;
	crtc.h_mul = 1;
	crtc.hd = 2;

	// ����
	crtc.v_sync = 568;
	crtc.v_pulse = 6;
	crtc.v_back = 35;
	crtc.v_front = 15;
	crtc.v_dots = 512;
	crtc.v_mul = 1;
	crtc.vd = 1;

	// �C�x���g
	crtc.ns = 0;
	crtc.hus = 0;
	crtc.v_synccnt = 1;
	crtc.v_blankcnt = 1;
	crtc.h_disp = TRUE;
	crtc.v_disp = TRUE;
	crtc.v_blank = TRUE;
	crtc.v_count = 0;
	crtc.v_scan = 0;

	// �ȉ�����Ȃ�
	crtc.h_disptime = 0;
	crtc.h_synctime = 0;
	crtc.v_cycletime = 0;
	crtc.v_blanktime = 0;
	crtc.v_backtime = 0;
	crtc.v_synctime = 0;

	// ���������[�h
	crtc.tmem = FALSE;
	crtc.gmem = TRUE;
	crtc.siz = 0;
	crtc.col = 3;

	// �X�N���[��
	crtc.text_scrlx = 0;
	crtc.text_scrly = 0;
	for (i=0; i<4; i++) {
		crtc.grp_scrlx[i] = 0;
		crtc.grp_scrly[i] = 0;
	}

	// H-Sync�C�x���g��ݒ�(31.5us)
	event.SetTime(63);
}

//---------------------------------------------------------------------------
//
//	CRTC���Z�b�g�f�[�^
//
//---------------------------------------------------------------------------
const BYTE CRTC::ResetTable[] = {
	0x00, 0x89, 0x00, 0x0e, 0x00, 0x1c, 0x00, 0x7c,
	0x02, 0x37, 0x00, 0x05, 0x00, 0x28, 0x02, 0x28,
	0x00, 0x1b,
	0x0b, 0x16, 0x00, 0x33, 0x7a, 0x7b, 0x00, 0x00
};

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "�Z�[�u");

	// �T�C�Y���Z�[�u
	sz = sizeof(crtc_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// ���̂��Z�[�u
	if (!fio->Write(&crtc, (int)sz)) {
		return FALSE;
	}

	// �C�x���g���Z�[�u
	if (!event.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Load(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "���[�h");

	// �T�C�Y�����[�h
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(crtc_t)) {
		return FALSE;
	}

	// ���̂����[�h
	if (!fio->Read(&crtc, (int)sz)) {
		return FALSE;
	}

	// �C�x���g�����[�h
	if (!event.Load(fio, ver)) {
		return FALSE;
	}

	// �����_���֒ʒm
	render->TextScrl(crtc.text_scrlx, crtc.text_scrly);
	render->GrpScrl(0, crtc.grp_scrlx[0], crtc.grp_scrly[0]);
	render->GrpScrl(1, crtc.grp_scrlx[1], crtc.grp_scrly[1]);
	render->GrpScrl(2, crtc.grp_scrlx[2], crtc.grp_scrly[2]);
	render->GrpScrl(3, crtc.grp_scrlx[3], crtc.grp_scrly[3]);
	render->SetCRTC();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�ݒ�K�p
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	g_alt_raster_timing = config->alt_raster;
	LOG0(Log::Normal, "�ݒ�K�p");
}

//---------------------------------------------------------------------------
//
//	�o�C�g�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadByte(DWORD addr)
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $800�P�ʂŃ��[�v
	addr &= 0x7ff;

	// �E�F�C�g
	scheduler->Wait(1);

	// $E80000-$E803FF : ���W�X�^�G���A
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// R20, R21�̂ݓǂݏ����\�B����ȊO��$00
		if ((addr < 40) || (addr > 43)) {
			return 0;
		}

		// �ǂݍ���(�G���f�B�A���𔽓]������)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// $E80480-$E804FF : ����|�[�g
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// ��ʃo�C�g�� 0
		if ((addr & 1) == 0) {
			return 0;
		}

		// ���ʃo�C�g�̓��X�^�R�s�[�A�O���t�B�b�N�����N���A�̂�
		data = 0;
		if (crtc.raster_copy) {
			data |= 0x08;
		}
		if (crtc.fast_clr == 2) {
			data |= 0x02;
		}
		return data;
	}

	LOG1(Log::Warning, "�������A�h���X�ǂݍ��� $%06X", memdev.first + addr);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	�o�C�g��������
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::WriteByte(DWORD addr, DWORD data)
{
	int reg;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $800�P�ʂŃ��[�v
	addr &= 0x7ff;

	// �E�F�C�g
	scheduler->Wait(1);

	// $E80000-$E803FF : ���W�X�^�G���A
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return;
		}

		// ��������(�G���f�B�A���𔽓]������)
		addr ^= 1;
		if (crtc.reg[addr] == data) {
			return;
		}
		crtc.reg[addr] = (BYTE)data;

		// GVRAM�A�h���X�\��
		if (addr == 0x29) {
			if (data & 0x10) {
				crtc.tmem = TRUE;
			}
			else {
				crtc.tmem = FALSE;
			}
			if (data & 0x08) {
				crtc.gmem = TRUE;
			}
			else {
				crtc.gmem = FALSE;
			}
			crtc.siz = (data & 4) >> 2;
			crtc.col = (data & 3);

			// �O���t�B�b�NVRAM�֒ʒm
			gvram->SetType(data & 0x0f);
			return;
		}

		// �𑜓x�ύX
		if ((addr <= 15) || (addr == 40)) {
			// �X�v���C�g�������̐ڑ��E�ؒf�͏u���ɍs��(OS-9/68000)
			if (addr == 0x28) {
				if ((crtc.reg[0x28] & 3) >= 2) {
					sprite->Connect(FALSE);
				}
				else {
					sprite->Connect(TRUE);
				}
			}

			// ���̎����ōČv�Z
			crtc.changed = TRUE;
			return;
		}

		// ���X�^���荞��
		if ((addr == 18) || (addr == 19)) {
			crtc.raster_int = (crtc.reg[19] << 8) + crtc.reg[18];
			crtc.raster_int &= 0x3ff;
			if (g_alt_raster_timing) {
				CheckRaster();
			}
			return;
		}

		// �e�L�X�g�X�N���[��
		if ((addr >= 20) && (addr <= 23)) {
			crtc.text_scrlx = (crtc.reg[21] << 8) + crtc.reg[20];
			crtc.text_scrlx &= 0x3ff;
			crtc.text_scrly = (crtc.reg[23] << 8) + crtc.reg[22];
			crtc.text_scrly &= 0x3ff;
			render->TextScrl(crtc.text_scrlx, crtc.text_scrly);

#if defined(CRTC_LOG)
			LOG2(Log::Normal, "�e�L�X�g�X�N���[�� x=%d y=%d", crtc.text_scrlx, crtc.text_scrly);
#endif	// CRTC_LOG
			return;
		}

		// �O���t�B�b�N�X�N���[��
		if ((addr >= 24) && (addr <= 39)) {
			reg = addr & ~3;
			addr -= 24;
			addr >>= 2;
			ASSERT(addr <= 3);
			crtc.grp_scrlx[addr] = (crtc.reg[reg+1] << 8) + crtc.reg[reg+0];
			crtc.grp_scrly[addr] = (crtc.reg[reg+3] << 8) + crtc.reg[reg+2];
			if (addr == 0) {
				crtc.grp_scrlx[addr] &= 0x3ff;
				crtc.grp_scrly[addr] &= 0x3ff;
			}
			else {
				crtc.grp_scrlx[addr] &= 0x1ff;
				crtc.grp_scrly[addr] &= 0x1ff;
			}
			render->GrpScrl(addr, crtc.grp_scrlx[addr], crtc.grp_scrly[addr]);
			return;
		}

		// �e�L�X�gVRAM
		if ((addr >= 42) && (addr <= 47)) {
			TextVRAM();
		}
		return;
	}

	// $E80480-$E804FF : ����|�[�g
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// ��ʃo�C�g�͉����Ȃ�
		if ((addr & 1) == 0) {
			return;
		}

		// ���ʃo�C�g�̓��X�^�R�s�[�E�����N���A����
		if (data & 0x08) {
			crtc.raster_copy = TRUE;
		}
		else {
			crtc.raster_copy = FALSE;
		}
		if (data & 0x02) {
			// ���X�^�R�s�[�Ƌ��p�A���X�^�R�s�[�D��(��헪III'90)
			if ((crtc.fast_clr == 0) && !crtc.raster_copy) {
#if defined(CRTC_LOG)
				LOG0(Log::Normal, "�O���t�B�b�N�����N���A�w��");
#endif	// CRTC_LOG
				crtc.fast_clr = 1;
			}
#if defined(CRTC_LOG)
			else {
				LOG1(Log::Normal, "�O���t�B�b�N�����N���A�w������ State=%d", crtc.fast_clr);
			}
#endif	//CRTC_LOG
		}
		return;
	}

	LOG2(Log::Warning, "�������A�h���X�������� $%06X <- $%02X",
							memdev.first + addr, data);
}

//---------------------------------------------------------------------------
//
//	�ǂݍ��݂̂�
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadOnly(DWORD addr) const
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $800�P�ʂŃ��[�v
	addr &= 0x7ff;

	// $E80000-$E803FF : ���W�X�^�G���A
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// �ǂݍ���(�G���f�B�A���𔽓]������)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// $E80480-$E804FF : ����|�[�g
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// ��ʃo�C�g��0
		if ((addr & 1) == 0) {
			return 0;
		}

		// ���ʃo�C�g�̓O���t�B�b�N�����N���A�̂�
		data = 0;
		if (crtc.raster_copy) {
			data |= 0x08;
		}
		if (crtc.fast_clr == 2) {
			data |= 0x02;
		}
		return data;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	�����f�[�^�擾
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetCRTC(crtc_t *buffer) const
{
	ASSERT(buffer);

	// �����f�[�^���R�s�[
	*buffer = crtc;
}

//---------------------------------------------------------------------------
//
//	�C�x���g�R�[���o�b�N
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Callback(Event* /*ev*/)
{
	ASSERT(this);

	// HSync,HDisp��2���Ăѕ�����
	if (crtc.h_disp) {
		HSync();
	}
	else {
		HDisp();
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	H-SYNC�J�n
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HSync()
{
	int hus;

	ASSERT(this);

	// �v�����^�ɒʒm(����I��BUSY�𗎂Ƃ�����)
	ASSERT(printer);
	printer->HSync();

	// V-SYNC�J�E���g
	crtc.v_synccnt--;
	if (crtc.v_synccnt == 0) {
		VSync();
	}

	// V-BLANK�J�E���g
	crtc.v_blankcnt--;
	if (crtc.v_blankcnt == 0) {
		VBlank();
	}

	// ���̃^�C�~���O(H-DISP�J�n)�܂ł̎��Ԃ�ݒ�
	crtc.ns += crtc.h_pulse;
	hus = Ns2Hus(crtc.ns);
	hus -= crtc.hus;
	event.SetTime(hus);
	crtc.hus += hus;

	// ��������(40ms����)
	if (crtc.hus >= 80000) {
		crtc.hus -= 80000;
		ASSERT(crtc.ns >= 40000000);
		crtc.ns -= 40000000;
	}

	// �t���O�ݒ�
	crtc.h_disp = FALSE;

	// GPIP�ݒ�
	mfp->SetGPIP(7, 1);

	// ���X�^���荞�� (Alt timing)
	if (g_alt_raster_timing) {
		CheckRaster();
	}

	// �`��
	crtc.v_scan++;
	if (!crtc.v_blank) {
		// �����_�����O
		render->HSync(crtc.v_scan);
	}

	// �e�L�X�g��ʃ��X�^�R�s�[
	if (crtc.raster_copy && crtc.raster_exec) {
		tvram->RasterCopy();
		crtc.raster_exec = FALSE;
	}

	// �O���t�B�b�N��ʍ����N���A
	if (crtc.fast_clr == 2) {
		gvram->FastClr(&crtc);
	}

	if (g_alt_raster_timing) {
		crtc.raster_count++;
	}
}

//---------------------------------------------------------------------------
//
//	H-DISP�J�n
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HDisp()
{
	int ns;
	int hus;

	ASSERT(this);

	// ���X�^���荞�� (Original timing)
	if (!g_alt_raster_timing) {
		CheckRaster();
		crtc.raster_count++;
	}
	else {
		CheckRaster();
	}

	// ���̃^�C�~���O(H-SYNC�J�n)�܂ł̎��Ԃ�ݒ�
	ns = crtc.h_sync - crtc.h_pulse;
	ASSERT(ns > 0);
	crtc.ns += ns;
	hus = Ns2Hus(crtc.ns);
	hus -= crtc.hus;
	event.SetTime(hus);
	crtc.hus += hus;

	// �t���O�ݒ�
	crtc.h_disp = TRUE;

	// GPIP�ݒ�
	mfp->SetGPIP(7,0);

	// ���X�^�R�s�[����
	crtc.raster_exec = TRUE;
}

//---------------------------------------------------------------------------
//
//	V-SYNC�J�n(V-DISP�J�n���܂�)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VSync()
{
	ASSERT(this);

	// V-SYNC�I���Ȃ�
	if (!crtc.v_disp) {
		// �t���O�ݒ�
		crtc.v_disp = TRUE;

		// ���Ԑݒ�
		crtc.v_synccnt = (crtc.v_sync - crtc.v_pulse);
		return;
	}

	// �𑜓x�ύX������΁A�����ŕύX
	if (crtc.changed) {
		ReCalc();
	}

	// V-SYNC�I���܂ł̎��Ԃ�ݒ�
	crtc.v_synccnt = crtc.v_pulse;

	// V-BLANK�̏�ԂƁA���Ԃ�ݒ�
	if (crtc.v_front < 0) {
		// �܂��\����(����)
		crtc.v_blank = FALSE;
		crtc.v_blankcnt = (-crtc.v_front) + 1;
	}
	else {
		// ���łɃu�����N��(�ʏ�)
		crtc.v_blank = TRUE;
		crtc.v_blankcnt = (crtc.v_pulse + crtc.v_back + 1);
	}

	// �t���O�ݒ�
	crtc.v_disp = FALSE;

	// ���X�^�J�E���g������
	crtc.raster_count = 0;
}

//---------------------------------------------------------------------------
//
//	�Čv�Z
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ReCalc()
{
	int dc;
	int over;
	WORD *p;

	ASSERT(this);
	ASSERT(crtc.changed);

	// CRTC���W�X�^0���N���A����Ă���΁A����(Mac�G�~�����[�^)
	if (crtc.reg[0x0] != 0) {
#if defined(CRTC_LOG)
		LOG0(Log::Normal, "�Čv�Z");
#endif	// CRTC_LOG

		// �h�b�g�N���b�N���擾
		dc = Get8DotClock();

		// ����(���ׂ�ns�P��)
		crtc.h_sync = (crtc.reg[0x0] + 1) * dc / 100;
		crtc.h_pulse = (crtc.reg[0x02] + 1) * dc / 100;
		crtc.h_back = (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1) * dc / 100;
		crtc.h_front = (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5) * dc / 100;

		// ����(���ׂ�H-Sync�P��)
		p = (WORD *)crtc.reg;
		crtc.v_sync = ((p[4] & 0x3ff) + 1);
		crtc.v_pulse = ((p[5] & 0x3ff) + 1);
		crtc.v_back = ((p[6] & 0x3ff) + 1) - crtc.v_pulse;
		crtc.v_front = crtc.v_sync - ((p[7] & 0x3ff) + 1);

		// V-FRONT���}�C�i�X������ꍇ�́A1�������ԕ��̂�(�w���n�E���h�A�R�b�g��)
		if (crtc.v_front < 0) {
			over = -crtc.v_front;
			over -= crtc.v_back;
			if (over >= crtc.v_pulse) {
				crtc.v_front = -1;
			}
		}

		// �h�b�g�����Z�o
		crtc.h_dots = (crtc.reg[0x0] + 1);
		crtc.h_dots -= (crtc.reg[0x02] + 1);
		crtc.h_dots -= (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1);
		crtc.h_dots -= (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5);
		crtc.h_dots *= 8;
		crtc.v_dots = crtc.v_sync - crtc.v_pulse - crtc.v_back - crtc.v_front;
	}

	// �{���ݒ�(����)
	crtc.hd = (crtc.reg[0x28] & 3);
	if (crtc.hd == 3) {
		LOG0(Log::Warning, "���h�b�g��50MHz���[�h(CompactXVI)");
	}
	if (crtc.hd == 0) {
		crtc.h_mul = 2;
	}
	else {
		crtc.h_mul = 1;
	}

	// crtc.hd��2�ȏ�̏ꍇ�A�X�v���C�g�͐؂藣�����
	if (crtc.hd >= 2) {
		// 768x512 or VGA���[�h(�X�v���C�g�Ȃ�)
		sprite->Connect(FALSE);
		crtc.textres = TRUE;
	}
	else {
		// 256x256 or 512x512���[�h(�X�v���C�g����)
		sprite->Connect(TRUE);
		crtc.textres = FALSE;
	}

	// �{���ݒ�(����)
	crtc.vd = (crtc.reg[0x28] >> 2) & 3;
	if (crtc.reg[0x28] & 0x10) {
		// 31kHz
		crtc.lowres = FALSE;
		if (crtc.vd == 3) {
			// �C���^���[�X1024dot���[�h
			crtc.v_mul = 0;
		}
		else {
			// �C���^���[�X�A�ʏ�512���[�h(x1)�A�{256dot���[�h(x2)
			crtc.v_mul = 2 - crtc.vd;
		}
	}
	else {
		// 15kHz
		crtc.lowres = TRUE;
		if (crtc.vd == 0) {
			// �ʏ��256dot���[�h(x2)
			crtc.v_mul = 2;
		}
		else {
			// �C���^���[�X512dot���[�h(x1)
			crtc.v_mul = 0;
		}
	}

	// �����_���֒ʒm
	render->SetCRTC();

	// �t���O���낷
	crtc.changed = FALSE;
}


//---------------------------------------------------------------------------
//
//	V-BLANK�J�n(V-SCREEN�J�n���܂�)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VBlank()
{
	ASSERT(this);

	// �\�����ł���΁A�u�����N�J�n
	if (!crtc.v_blank) {
		// �u�����N��Ԃ�ݒ�
		crtc.v_blankcnt = crtc.v_pulse + crtc.v_back + crtc.v_front;
		ASSERT((crtc.v_front < 0) || ((int)crtc.v_synccnt == crtc.v_front));

		// �t���O
		crtc.v_blank = TRUE;

		// GPIP
		mfp->EventCount(0, 0);
		mfp->SetGPIP(4, 0);

		// �O���t�B�b�N�����N���A
		if (crtc.fast_clr == 2) {
#if defined(CRTC_LOG)
			LOG0(Log::Normal, "�O���t�B�b�N�����N���A�I��");
#endif	// CRTC_LOG
			crtc.fast_clr = 0;
		}

		// �����_�������I��
		render->EndFrame();
		crtc.v_scan = crtc.v_dots + 1;
		return;
	}

	// �\����Ԃ�ݒ�
	crtc.v_blankcnt = crtc.v_sync;
	crtc.v_blankcnt -= (crtc.v_pulse + crtc.v_back + crtc.v_front);

	// �t���O
	crtc.v_blank = FALSE;

	// GPIP
	mfp->EventCount(0, 1);
	mfp->SetGPIP(4, 1);

	// �O���t�B�b�N�����N���A
	if (crtc.fast_clr == 1) {
#if defined(CRTC_LOG)
		LOG1(Log::Normal, "�O���t�B�b�N�����N���A�J�n data=%02X", crtc.reg[42]);
#endif	// CRTC_LOG
		crtc.fast_clr = 2;
		gvram->FastSet((DWORD)crtc.reg[42]);
		gvram->FastClr(&crtc);
	}

	// �����_�������J�n�A�J�E���^�A�b�v
	crtc.v_scan = 0;
	render->StartFrame();
	crtc.v_count++;
}

//---------------------------------------------------------------------------
//
//	�\�����g���擾
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetHVHz(DWORD *h, DWORD *v) const
{
	DWORD d;
	DWORD t;

	// assert
	ASSERT(h);
	ASSERT(v);

	// �`�F�b�N
	if ((crtc.h_sync == 0) || (crtc.v_sync < 100)) {
		// NO SIGNAL
		*h = 0;
		*v = 0;
		return;
	}

	// ex. 31.5kHz = 3150
	d = 100 * 1000 * 1000;
	d /= crtc.h_sync;
	*h = d;

	// ex. 55.46Hz = 5546
	t = crtc.v_sync;
    t *= crtc.h_sync;
	t /= 100;
	d = 1000 * 1000 * 1000;
	d /= t;
	*v = d;
}

//---------------------------------------------------------------------------
//
//	8�h�b�g�N���b�N���擾(�~100)
//
//---------------------------------------------------------------------------
int FASTCALL CRTC::Get8DotClock() const
{
	int hf;
	int hd;
	int index;

	ASSERT(this);

	// HF, HD��CRTC R20���擾
	hf = (crtc.reg[0x28] >> 4) & 1;
	hd = (crtc.reg[0x28] & 3);

	// �C���f�b�N�X�쐬
	index = hf * 4 + hd;
	if (crtc.hrl) {
		index += 8;
	}

	return DotClockTable[index];
}

//---------------------------------------------------------------------------
//
//	8�h�b�g�N���b�N�e�[�u��
//	(HRL,HF,HD���瓾����l�B0.01ns�P��)
//
//---------------------------------------------------------------------------
const int CRTC::DotClockTable[16] = {
	// HRL=0
	164678, 82339, 164678, 164678,
	69013, 34507, 23004, 31778,
	// HRL=1
	164678, 82339, 164678, 164678,
	92017, 46009, 23004, 31778
};

//---------------------------------------------------------------------------
//
//	HRL�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::SetHRL(BOOL flag)
{
	if (crtc.hrl != flag) {
		// ���̎����ōČv�Z
		crtc.hrl = flag;
		crtc.changed = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	HRL�擾
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::GetHRL() const
{
	return crtc.hrl;
}

//---------------------------------------------------------------------------
//
//	���X�^���荞�݃`�F�b�N
//	���C���^���[�X���[�h�ɂ͖��Ή�
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::CheckRaster()
{
	BOOL hit;

	if (g_alt_raster_timing) {
		hit = (crtc.raster_count == crtc.raster_int);
	}
	else {
		hit = (crtc.raster_count == crtc.raster_int);
	}

	if (hit) {
		// �v��
		mfp->SetGPIP(6, 0);
#if defined(CRTC_LOG)
		LOG2(Log::Normal, "���X�^���荞�ݗv�� raster=%d scan=%d", crtc.raster_count, crtc.v_scan);
#endif	// CRTC_LOG
	}
	else {
		// ��艺��
		mfp->SetGPIP(6, 1);
	}
}

//---------------------------------------------------------------------------
//
//	�e�L�X�gVRAM����
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::TextVRAM()
{
	DWORD b;
	DWORD w;

	// �����A�N�Z�X
	if (crtc.reg[43] & 1) {
		b = (DWORD)crtc.reg[42];
		b >>= 4;

		// b4�̓}���`�t���O
		b |= 0x10;
		tvram->SetMulti(b);
	}
	else {
		tvram->SetMulti(0);
	}

	// �A�N�Z�X�}�X�N
	if (crtc.reg[43] & 2) {
		w = (DWORD)crtc.reg[47];
		w <<= 8;
		w |= (DWORD)crtc.reg[46];
		tvram->SetMask(w);
	}
	else {
		tvram->SetMask(0);
	}

	// ���X�^�R�s�[
	tvram->SetCopyRaster((DWORD)crtc.reg[45], (DWORD)crtc.reg[44],
						(DWORD)(crtc.reg[42] & 0x0f));
}
