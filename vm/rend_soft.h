//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ �����_�� �A�Z���u���T�u ]
//
//---------------------------------------------------------------------------

#if !defined (rend_asm_h)
#define rend_asm_h

#include "os.h"
#include "xm6.h"

#if defined(__cplusplus)
extern "C" {
#endif	//__cplusplus

//---------------------------------------------------------------------------
//
//	�v���g�^�C�v�錾
//
//---------------------------------------------------------------------------
void RendTextMem(const BYTE *tvrm, BOOL *flag, BYTE *buf);
										// �e�L�X�g�����_�����O(���������ϊ�)
void RendTextPal(const BYTE *buf, DWORD *out, BOOL *flag, const DWORD *pal);
										// �e�L�X�g�����_�����O(�p���b�g)
void RendTextAll(const BYTE *buf, DWORD *out, const DWORD *pal);
										// �e�L�X�g�����_�����O(�p���b�g�S��)
void RendTextCopy(const BYTE *src, const BYTE *dst, DWORD plane, BOOL *textmem, BOOL *textflag);
										// �e�L�X�g���X�^�R�s�[

int Rend1024A(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// �O���t�B�b�N1024�����_�����O(�y�[�W0,1-All)
int Rend1024B(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// �O���t�B�b�N1024�����_�����O(�y�[�W2,3-All)
void Rend1024C(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// �O���t�B�b�N1024�����_�����O(�y�[�W0)
void Rend1024D(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// �O���t�B�b�N1024�����_�����O(�y�[�W1)
void Rend1024E(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// �O���t�B�b�N1024�����_�����O(�y�[�W2)
void Rend1024F(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// �O���t�B�b�N1024�����_�����O(�y�[�W3)
int Rend16A(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// �O���t�B�b�N16�����_�����O(�y�[�W0-All)
int Rend16B(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// �O���t�B�b�N16�����_�����O(�y�[�W0)
int Rend16C(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// �O���t�B�b�N16�����_�����O(�y�[�W1-All)
int Rend16D(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// �O���t�B�b�N16�����_�����O(�y�[�W1)
int Rend16E(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// �O���t�B�b�N16�����_�����O(�y�[�W2-All)
int Rend16F(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// �O���t�B�b�N16�����_�����O(�y�[�W2)
int Rend16G(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// �O���t�B�b�N16�����_�����O(�y�[�W3-All)
int Rend16H(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// �O���t�B�b�N16�����_�����O(�y�[�W3)
void Rend256A(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// �O���t�B�b�N256�����_�����O(�y�[�W0)
void Rend256B(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// �O���t�B�b�N256�����_�����O(�y�[�W1)
int Rend256C(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// �O���t�B�b�N256�����_�����O(�y�[�W0-All)
int Rend256D(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// �O���t�B�b�N256�����_�����O(�y�[�W1-All)
void Rend64KA(const BYTE *gvrm, DWORD *buf, BOOL *flag, BYTE *plt, DWORD *pal);
										// �O���t�B�b�N64K�����_�����O
int Rend64KB(const BYTE *gvrm, DWORD *buf, BYTE *plt, DWORD *pal);
										// �O���t�B�b�N64K�����_�����O(All)

void RendClrSprite(DWORD *buf, DWORD color, int len);
										// �X�v���C�g�o�b�t�@�N���A
void RendSprite(const DWORD *line, DWORD *buf, DWORD x, DWORD flag);
										// �X�v���C�g�����_�����O(�P��)
void RendSpriteC(const DWORD *line, DWORD *buf, DWORD x, DWORD flag);
										// �X�v���C�g�����_�����O(�P�́ACMOV)
void RendPCGNew(DWORD index, const BYTE *mem, DWORD *buf, DWORD *pal);
										// PCG�����_�����O(NewVer)
void RendBG8(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(8x8�A����؂��)
void RendBG8C(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(8x8�A����؂��ACMOV)
void RendBG8P(DWORD **ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(8x8�A�����̂�)
void RendBG16(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(16x16�A����؂��)
void RendBG16C(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(16x16�A����؂��ACMOV)
void RendBG16P(DWORD **ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(16x16�A�����̂�)

void RendMix00(DWORD *buf, BOOL *flag, int len);
										// ����(0��)
void RendMix01(DWORD *buf, const DWORD *src, BOOL *flag, int len);
										// ����(1��)
void RendMix02(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len);
										// ����(2�ʁA�J���[0�d�ˍ��킹)
void RendMix02C(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len);
										// ����(2�ʁA�J���[0�d�ˍ��킹�ACMOV)
void RendMix03(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len);
										// ����(2�ʁA�ʏ�d�ˍ��킹)
void RendMix03C(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len);
										// ����(2�ʁA�ʏ�d�ˍ��킹�ACMOV)
void RendMix04(DWORD *buf, const DWORD *f, const DWORD *s, DWORD *t, BOOL *flag, int len);
										// ����(3�ʁA�ʏ�d�ˍ��킹�ACMOV)
void RendMix04C(DWORD *buf, const DWORD *f, const DWORD *s, DWORD *t, BOOL *flag, int len);
										// ����(3�ʁA�ʏ�d�ˍ��킹�ACMOV)
void RendGrp02(DWORD *buf, const DWORD *f, const DWORD *s, int len);
										// �O���t�B�b�N����(2��)
void RendGrp02C(DWORD *buf, const DWORD *f, const DWORD *s, int len);
										// �O���t�B�b�N����(2�ʁACMOV)
void RendGrp03(DWORD *buf, const DWORD *f, const DWORD *s, const DWORD *t, int len);
										// �O���t�B�b�N����(3��)
void RendGrp03C(DWORD *buf, const DWORD *f, const DWORD *s, const DWORD *t, int len);
										// �O���t�B�b�N����(3�ʁACMOV)
void RendGrp04(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, DWORD *e, int len);
										// �O���t�B�b�N����(4��)

#if defined(__cplusplus)
}
#endif	//__cplusplus

#endif	// rend_asm_h
