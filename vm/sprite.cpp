//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ �X�v���C�g(CYNTHIA) ]
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
//	�X�v���C�g
//
//===========================================================================
//#define SPRITE_LOG

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
Sprite::Sprite(VM *p) : MemDevice(p)
{
	// �f�o�C�XID��������
	dev.id = MAKEID('S', 'P', 'R', ' ');
	dev.desc = "Sprite (CYNTHIA)";

	// �J�n�A�h���X�A�I���A�h���X
	memdev.first = 0xeb0000;
	memdev.last = 0xebffff;

	// ���̑�
	sprite = NULL;
	render = NULL;
	spr.mem = NULL;
	spr.pcg = NULL;
}

//---------------------------------------------------------------------------
//
//	������
//
//---------------------------------------------------------------------------
BOOL FASTCALL Sprite::Init()
{
	ASSERT(this);

	// ��{�N���X
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// �������m�ہA�N���A
	try {
		sprite = new BYTE[ 0x10000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!sprite) {
		return FALSE;
	}

	// EB0400-EB07FF, EB0812-EB7FFF��Reserved(FF)
	memset(sprite, 0, 0x10000);
	memset(&sprite[0x400], 0xff, 0x400);
	memset(&sprite[0x812], 0xff, 0x77ee);

	// ���[�N������
	memset(&spr, 0, sizeof(spr));
	spr.mem = &sprite[0x0000];
	spr.pcg = &sprite[0x8000];

	// �����_���擾
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�N���[���A�b�v
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::Cleanup()
{
	// ���������
	if (sprite) {
		delete[] sprite;
		sprite = NULL;
		spr.mem = NULL;
		spr.pcg = NULL;
	}

	// ��{�N���X��
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	���Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::Reset()
{
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "���Z�b�g");

	// ���W�X�^�ݒ�
	spr.connect = FALSE;
	spr.disp = FALSE;

	// BG�y�[�W������
	for (i=0; i<2; i++) {
		spr.bg_on[i] = FALSE;
		spr.bg_area[i] = 0;
		spr.bg_scrlx[i] = 0;
		spr.bg_scrly[i] = 0;
	}

	// BG�T�C�Y������
	spr.bg_size = FALSE;

	// �^�C�~���O������
	spr.h_total = 0;
	spr.h_disp = 0;
	spr.v_disp = 0;
	spr.lowres = FALSE;
	spr.v_res = 0;
	spr.h_res = 0;
}

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL Sprite::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(spr.mem);

	LOG0(Log::Normal, "�Z�[�u");

	// �T�C�Y���Z�[�u
	sz = sizeof(sprite_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// ���̂��Z�[�u
	if (!fio->Write(&spr, (int)sz)) {
		return FALSE;
	}

	// ���������Z�[�u
	if (!fio->Write(sprite, 0x10000)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h
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

	LOG0(Log::Normal, "���[�h");

	// �T�C�Y�����[�h�A�ƍ�
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(sprite_t)) {
		return FALSE;
	}

	// ���̂����[�h
	if (!fio->Read(&spr, (int)sz)) {
		return FALSE;
	}

	// �����������[�h
	if (!fio->Read(sprite, 0x10000)) {
		return FALSE;
	}

	// �|�C���^���㏑��
	spr.mem = &sprite[0x0000];
	spr.pcg = &sprite[0x8000];

	// �����_���֒ʒm(���W�X�^)
	render->BGCtrl(4, spr.bg_size);
	for (i=0; i<2; i++) {
		// BG�f�[�^�G���A
		if (spr.bg_area[i] & 1) {
			render->BGCtrl(i + 2, TRUE);
		}
		else {
			render->BGCtrl(i + 2, FALSE);
		}

		// BG�\��ON/OFF
		render->BGCtrl(i, spr.bg_on[i]);

		// BG�X�N���[��
		render->BGScrl(i, spr.bg_scrlx[i], spr.bg_scrly[i]);
	}

	// �����_���֒ʒm(������:�����A�h���X�̂�)
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
//	�ݒ�K�p
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::ApplyCfg(const Config *config)
{
	ASSERT(config);
	LOG0(Log::Normal, "�ݒ�K�p");
	printf("%p", (const void*)config);
}

//---------------------------------------------------------------------------
//
//	�o�C�g�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL Sprite::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// �I�t�Z�b�g�Z�o
	addr &= 0xffff;

	// 0800�`7FFF�̓o�X�G���[�̉e�����󂯂Ȃ�
	if ((addr >= 0x800) && (addr < 0x8000)) {
		return sprite[addr ^ 1];
	}

	// �ڑ��`�F�b�N
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xff;
	}

	// �E�F�C�g(�G�g���[���v�����Z�X)
	if (addr & 1) {
		if (spr.disp) {
			scheduler->Wait(4);
		}
		else {
			scheduler->Wait(2);
		}
	}

	// �G���f�B�A���𔽓]�����ēǂݍ���
	return sprite[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	���[�h�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL Sprite::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	// �I�t�Z�b�g�Z�o
	addr &= 0xffff;

	// 0800�`7FFF�̓o�X�G���[�̉e�����󂯂Ȃ�
	if ((addr >= 0x800) && (addr < 0x8000)) {
		return *(WORD *)(&sprite[addr]);
	}

	// �ڑ��`�F�b�N
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xff;
	}

	// �E�F�C�g(�G�g���[���v�����Z�X)
	if (spr.disp) {
		scheduler->Wait(4);
	}
	else {
		scheduler->Wait(2);
	}

	// �ǂݍ���
	return *(WORD *)(&sprite[addr]);
}

//---------------------------------------------------------------------------
//
//	�o�C�g��������
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::WriteByte(DWORD addr, DWORD data)
{
	DWORD ctrl;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);

	// �I�t�Z�b�g�Z�o
	addr &= 0xffff;

	// ��v�`�F�b�N
	if (sprite[addr ^ 1] == data) {
		return;
	}

	// 800�`811�̓R���g���[�����W�X�^
	if ((addr >= 0x800) && (addr < 0x812)) {
		// �f�[�^��������
		sprite[addr ^ 1] = (BYTE)data;

		if (addr & 1) {
			// ���ʏ������݁B��ʂƂ��킹�ăR���g���[��
			ctrl = (DWORD)sprite[addr];
			ctrl <<= 8;
			ctrl |= data;
			Control((DWORD)(addr & 0xfffe), ctrl);
		}
		else {
			// ��ʏ������݁B���ʂƂ��킹�ăR���g���[��
			ctrl = data;
			ctrl <<= 8;
			ctrl |= (DWORD)sprite[addr];
			Control(addr, ctrl);
		}
		return;
	}

	// 0812-7FFF�̓��U�[�u(�o�X�G���[�̉e�����󂯂Ȃ�)
	if ((addr >= 0x812) && (addr < 0x8000)) {
		return;
	}

	// �ڑ��`�F�b�N
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, FALSE);
		return;
	}

	// �E�F�C�g(�G�g���[���v�����Z�X)
	if (addr & 1) {
		if (spr.disp) {
			scheduler->Wait(4);
		}
		else {
			scheduler->Wait(2);
		}
	}

	// 0400-07FF�̓��U�[�u(�o�X�G���[�̉e�����󂯂�)
	if ((addr >= 0x400) && (addr < 0x800)) {
		return;
	}

	// ��������
	sprite[addr ^ 1] = (BYTE)data;

	// �����_������
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
//	���[�h��������
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	// �I�t�Z�b�g�Z�o
	addr &= 0xfffe;

	// ��v�`�F�b�N
	if (*(WORD *)(&sprite[addr]) == data) {
		return;
	}

	// 800�`811�̓R���g���[�����W�X�^
	if ((addr >= 0x800) && (addr < 0x812)) {
		*(WORD *)(&sprite[addr]) = (WORD)data;
		Control(addr, data);
		return;
	}
	// 0812-7FFF�̓��U�[�u(�o�X�G���[�̉e�����󂯂Ȃ�)
	if ((addr >= 0x812) && (addr < 0x8000)) {
		return;
	}

	// �E�F�C�g(�G�g���[���v�����Z�X)
	if (spr.disp) {
		scheduler->Wait(4);
	}
	else {
		scheduler->Wait(2);
	}

	// 0400-07FF�̓��U�[�u(�o�X�G���[�̉e�����󂯂�)
	if ((addr >= 0x400) && (addr < 0x800)) {
		return;
	}

	// ��������
	*(WORD *)(&sprite[addr]) = (WORD)data;

	// �����_��
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
//	�ǂݍ��݂̂�
//
//---------------------------------------------------------------------------
DWORD FASTCALL Sprite::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// �I�t�Z�b�g�Z�o
	addr &= 0xffff;

	// �G���f�B�A���𔽓]�����ēǂݍ���
	return sprite[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	�R���g���[��
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::Control(DWORD addr, DWORD data)
{
	ASSERT((addr >= 0x800) && (addr < 0x812));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	// �A�h���X�𐮗�
	addr -= 0x800;
	addr >>= 1;

	switch (addr) {
		// BG0�X�N���[��X
		case 0:
			spr.bg_scrlx[0] = data & 0x3ff;
			render->BGScrl(0, spr.bg_scrlx[0], spr.bg_scrly[0]);
			break;

		// BG0�X�N���[��Y
		case 1:
			spr.bg_scrly[0] = data & 0x3ff;
			render->BGScrl(0, spr.bg_scrlx[0], spr.bg_scrly[0]);
			break;

		// BG1�X�N���[��X
		case 2:
			spr.bg_scrlx[1] = data & 0x3ff;
			render->BGScrl(1, spr.bg_scrlx[1], spr.bg_scrly[1]);
			break;

		// BG1�X�N���[��Y
		case 3:
			spr.bg_scrly[1] = data & 0x3ff;
			render->BGScrl(1, spr.bg_scrlx[1], spr.bg_scrly[1]);
			break;

		// BG�R���g���[��
		case 4:
#if defined(SPRITE_LOG)
			LOG1(Log::Normal, "BG�R���g���[�� $%04X", data);
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
				LOG1(Log::Warning, "BG1�f�[�^�G���A����` $%02X", spr.bg_area[1]);
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
				LOG1(Log::Warning, "BG0�f�[�^�G���A����` $%02X", spr.bg_area[0]);
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

		// �����g�[�^��
		case 5:
			spr.h_total = data & 0xff;
			break;

		// �����\��
		case 6:
			spr.h_disp = data & 0x3f;
			break;

		// �����\��
		case 7:
			spr.v_disp = data & 0xff;
			break;

		// ��ʃ��[�h
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

			// BG�T�C�Y
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
				LOG1(Log::Warning, "BG/�X�v���C�g H-Res����` %d", spr.h_res);
			}
			break;

		// ���̑�
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	�����f�[�^�擾
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::GetSprite(sprite_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	// �������[�N���R�s�[
	*buffer = spr;
}

//---------------------------------------------------------------------------
//
//	�������G���A�擾
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
//	PCG�G���A�擾
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL Sprite::GetPCG() const
{
	ASSERT(this);
	ASSERT(spr.pcg);

	return spr.pcg;
}
