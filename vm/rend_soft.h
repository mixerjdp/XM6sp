//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 PI(ytanaka@ipc-tokai.or.jp)
//	[ Software Renderer ]
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
//	Function prototype
//
//---------------------------------------------------------------------------
void RendTextMem(const BYTE *tvrm, BOOL *flag, BYTE *buf);
										// Text rendering (with color conversion)
void RendTextPal(const BYTE *buf, DWORD *out, BOOL *flag, const DWORD *pal);
										// Text rendering (palette)
void RendTextAll(const BYTE *buf, DWORD *out, const DWORD *pal);
										// Text rendering (palette all)
void RendTextCopy(const BYTE *src, const BYTE *dst, DWORD plane, BOOL *textmem, BOOL *textflag);
										// Text copy

int Rend1024A(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// Graphics 1024 rendering (Page 0,1-All)
int Rend1024B(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// Graphics 1024 rendering (Page 2,3-All)
void Rend1024C(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// Graphics 1024 rendering (Page 0)
void Rend1024D(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// Graphics 1024 rendering (Page 1)
void Rend1024E(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// Graphics 1024 rendering (Page 2)
void Rend1024F(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// Graphics 1024 rendering (Page 3)
int Rend16A(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// Graphics 16 rendering (Page 0-All)
int Rend16B(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// Graphics 16 rendering (Page 0)
int Rend16C(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// Graphics 16 rendering (Page 1-All)
int Rend16D(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// Graphics 16 rendering (Page 1)
int Rend16E(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// Graphics 16 rendering (Page 2-All)
int Rend16F(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// Graphics 16 rendering (Page 2)
int Rend16G(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// Graphics 16 rendering (Page 3-All)
int Rend16H(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// Graphics 16 rendering (Page 3)
void Rend256A(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// Graphics 256 rendering (Page 0)
void Rend256B(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// Graphics 256 rendering (Page 1)
int Rend256C(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// Graphics 256 rendering (Page 0-All)
int Rend256D(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// Graphics 256 rendering (Page 1-All)
void Rend64KA(const BYTE *gvrm, DWORD *buf, BOOL *flag, BYTE *plt, DWORD *pal);
										// Graphics 64K rendering
int Rend64KB(const BYTE *gvrm, DWORD *buf, BYTE *plt, DWORD *pal);
										// Graphics 64K rendering (All)

void RendClrSprite(DWORD *buf, DWORD color, int len);
										// Sprite buffer clear
void RendSprite(const DWORD *line, DWORD *buf, DWORD x, DWORD flag);
										// Sprite rendering (single)
void RendSpriteC(const DWORD *line, DWORD *buf, DWORD x, DWORD flag);
										// Sprite rendering (single, CMOV)
void RendPCGNew(DWORD index, const BYTE *mem, DWORD *buf, DWORD *pal);
										// PCG rendering (NewVer)
void RendBG8(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(8x8, priority)
void RendBG8C(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(8x8, priority, CMOV)
void RendBG8F(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(8x8, priority, force base)
void RendBG8P(DWORD **ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(8x8, priority copy)
void RendBG8FP(DWORD **ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(8x8, priority copy, force base)
void RendBG16(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(16x16, priority)
void RendBG16C(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(16x16, priority, CMOV)
void RendBG16F(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(16x16, priority, force base)
void RendBG16P(DWORD **ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal, BOOL legacy_bg_transparency);	// BG(16x16, priority copy)
void RendBG16FP(DWORD **ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(16x16, priority copy, force base)

void RendMix00(DWORD *buf, BOOL *flag, int len);
										// Mix(0 layer)
void RendMix01(DWORD *buf, const DWORD *src, BOOL *flag, int len);
										// Mix(1 layer)
void RendMix02(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len);
										// Mix(2 layer, Color0 priority)
void RendMix02C(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len);
										// Mix(2 layer, Color0 priority, CMOV)
void RendMix03(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len);
										// Mix(2 layer, normal priority)
void RendMix03C(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len);
										// Mix(2 layer, normal priority, CMOV)
void RendMix04(DWORD *buf, const DWORD *f, const DWORD *s, DWORD *t, BOOL *flag, int len);
										// Mix(3 layer, normal priority, CMOV)
void RendMix04C(DWORD *buf, const DWORD *f, const DWORD *s, DWORD *t, BOOL *flag, int len);
										// Mix(3 layer, normal priority, CMOV)
void RendGrp02(DWORD *buf, const DWORD *f, const DWORD *s, int len);
										// Group rendering (2 layer)
void RendGrp02C(DWORD *buf, const DWORD *f, const DWORD *s, int len);
										// Group rendering (2 layer, CMOV)
void RendGrp03(DWORD *buf, const DWORD *f, const DWORD *s, const DWORD *t, int len);
										// Group rendering (3 layer)
void RendGrp03C(DWORD *buf, const DWORD *f, const DWORD *s, const DWORD *t, int len);
										// Group rendering (3 layer, CMOV)
void RendGrp04(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, DWORD *e, int len);
										// Group rendering (4 layer)

#if defined(__cplusplus)
}
#endif	//__cplusplus

#endif	// rend_asm_h
