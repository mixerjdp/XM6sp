//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ レンダラ アセンブラサブ ]
//
//---------------------------------------------------------------------------

#ifndef rend_asm_h
#define rend_asm_h

#ifdef __cplusplus
extern "C" {
#endif	// __cplusplus

//---------------------------------------------------------------------------
//
//	プロトタイプ宣言
//
//---------------------------------------------------------------------------

typedef struct {
	DWORD *pcg;
	DWORD bg;
} bgdata_t;

void __cdecl RendTextMem(const BYTE *tvrm, BOOL *flag, BYTE *buf);
										// テキストレンダリング(水平垂直変換)
void __cdecl RendTextPal(const BYTE *buf, DWORD *out, BOOL *flag, const DWORD *pal);
										// テキストレンダリング(パレット)
void __cdecl RendTextAll(const BYTE *buf, DWORD *out, const DWORD *pal);
										// テキストレンダリング(パレット全て)
void __cdecl RendTextCopy(const BYTE *src, const BYTE *dst, DWORD plane, BOOL *textmem, BOOL *textflag);
										// テキストラスタコピー

int __cdecl Rend1024A(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// グラフィック1024レンダリング(ページ0-All)
void __cdecl Rend1024B(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// グラフィック1024レンダリング(ページ0)
int __cdecl Rend1024C(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// グラフィック1024レンダリング(ページ1-All)
void __cdecl Rend1024D(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// グラフィック1024レンダリング(ページ1)
int __cdecl Rend1024E(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// グラフィック1024レンダリング(ページ2-All)
void __cdecl Rend1024F(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// グラフィック1024レンダリング(ページ2)
int __cdecl Rend1024G(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// グラフィック1024レンダリング(ページ3-All)
void __cdecl Rend1024H(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// グラフィック1024レンダリング(ページ3)
int __cdecl Rend16A(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// グラフィック16レンダリング(ページ0-All)
void __cdecl Rend16B(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// グラフィック16レンダリング(ページ0)
int __cdecl Rend16C(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// グラフィック16レンダリング(ページ1-All)
void __cdecl Rend16D(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// グラフィック16レンダリング(ページ1)
int __cdecl Rend16E(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// グラフィック16レンダリング(ページ2-All)
void __cdecl Rend16F(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// グラフィック16レンダリング(ページ2)
int __cdecl Rend16G(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// グラフィック16レンダリング(ページ3-All)
void __cdecl Rend16H(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// グラフィック16レンダリング(ページ3)
int __cdecl Rend256A(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// グラフィック256レンダリング(ページ0-All)
void __cdecl Rend256B(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// グラフィック256レンダリング(ページ0)
int __cdecl Rend256C(const BYTE *gvrm, DWORD *buf, const DWORD *pal);
										// グラフィック256レンダリング(ページ1-All)
void __cdecl Rend256D(const BYTE *gvrm, DWORD *buf, BOOL *flag, const DWORD *pal);
										// グラフィック256レンダリング(ページ1)
void __cdecl Rend64KA(const BYTE *gvrm, DWORD *buf, BOOL *flag, BYTE *plt, DWORD *pal);
										// グラフィック64Kレンダリング
int __cdecl Rend64KB(const BYTE *gvrm, DWORD *buf, BYTE *plt, DWORD *pal);
										// グラフィック64Kレンダリング(All)
void __cdecl Rend64KAH(const BYTE *gvrm, DWORD *buf, BOOL *flag, BYTE *plt, DWORD *pal);
										// グラフィック64Kレンダリング 半透明
int __cdecl Rend64KBH(const BYTE *gvrm, DWORD *buf, BYTE *plt, DWORD *pal);
										// グラフィック64Kレンダリング(All) 半透明
void __cdecl Rend64KAHP(const BYTE *gvrm, DWORD *buf, BOOL *flag, BYTE *plt, DWORD *pal);
										// グラフィック64Kレンダリング 半透明、パレット領域指定
int __cdecl Rend64KBHP(const BYTE *gvrm, DWORD *buf, BYTE *plt, DWORD *pal);
										// グラフィック64Kレンダリング(All) 半透明、パレット領域指定
void __cdecl RendClrSprite(DWORD *buf, DWORD color, int len);
										// スプライトバッファクリア
void __cdecl RendSprite(const DWORD *line, DWORD *buf, DWORD x, DWORD flag, int spno, BYTE *pri);
										// スプライトレンダリング(単体)
void __cdecl RendSpriteP(const DWORD *line, DWORD *buf, DWORD x, BOOL hflag, int spno, BYTE *pri, DWORD len);
										// スプライトレンダリング(単体) 16ドット以下
void __cdecl RendPCGNew(DWORD idx, const BYTE *mem, DWORD *buf, DWORD *pal);
										// PCGレンダリング(NewVer)
void __cdecl RendBG8(bgdata_t *ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(8x8、割り切れる)
void __cdecl RendBG8F(bgdata_t *ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(8x8、割り切れる、上書き)
void __cdecl RendBG8P(bgdata_t *ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(8x8、部分のみ)
void __cdecl RendBG8FP(bgdata_t *ptrr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(8x8、部分のみ、上書き)
void __cdecl RendBG16(bgdata_t *ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(16x16、割り切れる)
void __cdecl RendBG16F(bgdata_t *ptrr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(16x16、割り切れる、上書き)
void __cdecl RendBG16P(bgdata_t *ptrr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(16x16、部分のみ)
void __cdecl RendBG16FP(bgdata_t *ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
			DWORD *pcgbuf, DWORD *pal);	// BG(16x16、部分のみ、上書き)

void RendTextBGSprite(DWORD *buf, DWORD *s, DWORD *t, int len);
										// テキスト・スプライト合成

void __cdecl RendGrp02(DWORD *buf, const DWORD *f, const DWORD *s, int len);
										// グラフィック合成(2面)
void __cdecl RendGrp03(DWORD *buf, const DWORD *f, const DWORD *s, const DWORD *t, int len);
										// グラフィック合成(3面)
void __cdecl RendGrp04(DWORD *buf, const DWORD *f, const DWORD *s, const DWORD *t, const DWORD *e, int len);
										// グラフィック合成(4面)
void __cdecl RendGrp02HBS(DWORD *buf, const DWORD *f, const DWORD *s, int len);
										// グラフィック合成(2面 半透明,領域指定あり、ベース表示、セカンド表示)
void __cdecl RendGrp02HB(DWORD *buf, const DWORD *f, const DWORD *s, int len);
										// グラフィック合成(2面 半透明,領域指定あり、ベース表示、セカンド非表示)

void __cdecl RendMix00(DWORD *buf, BOOL *flag, int len);
										// 合成(0面)
void __cdecl RendMix01(DWORD *buf, DWORD *f, BOOL *flag, int len);
										// 合成(1面)
void __cdecl RendMix01H(DWORD *buf, DWORD *f, DWORD c, BOOL *flag, int len);
										// 合成(1面、半透明、グラフィックのみ)
void __cdecl RendMix02A(DWORD *buf, DWORD *f, DWORD *s, BOOL *flag, int len);
										// 合成(2面、カラー０重ね合わせ)
void __cdecl RendMix02B(DWORD *buf, DWORD *f, DWORD *s, BOOL *flag, int len);
										// 合成(2面、通常重ね合わせ、グラフィックス前面)
void __cdecl RendMix02BH(DWORD *buf, DWORD *f, DWORD *s, BOOL *flag, int len);
										// 合成(2面、半透明、グラフィックス前面)
void __cdecl RendMix02C(DWORD *buf, DWORD *f, DWORD *s, BOOL *flag, int len);
										// 合成(2面、通常重ね合わせ、、グラフィック中間)
void __cdecl RendMix02CH(DWORD *buf, DWORD *f, DWORD *s, DWORD c, BOOL *flag, int len);
										// 合成(2面、半透明、グラフィック中間)
void __cdecl RendMix02CS(DWORD *buf, DWORD *f, DWORD *s, BOOL *flag, int len);
										// 合成(2面、特殊プライオリティ、グラフィック中間)
void __cdecl RendMix02D(DWORD *buf, DWORD *f, DWORD *s, BOOL *flag, int len);
										// 合成(2面、通常重ね合わせ、グラフィック後面)
void __cdecl RendMix02DH(DWORD *buf, DWORD *f, DWORD *s, DWORD c, BOOL *flag, int len);
										// 合成(2面、半透明、グラフィック後面)
void __cdecl RendMix02DS(DWORD *buf, DWORD *f, DWORD *s, BOOL *flag, int len);
										// 合成(2面、特殊プライオリティ、グラフィック後面)
void __cdecl RendMixP0H(DWORD *buf, DWORD *f, DWORD col, BOOL *flag, int len);
										// 合成(1面、パレット0半透明)
void __cdecl RendMix03A(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, BOOL *flag, int len);
										// 合成(3面、通常重ね合わせ、グラフィックス前面)
void __cdecl RendMix03AH(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, BOOL *flag, int len);
										// 合成(3面、半透明、グラフィックス前面)
void __cdecl RendMix03B(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, BOOL *flag, int len);
										// 合成(3面、通常重ね合わせ、グラフィック中間)
void __cdecl RendMix03BH(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, BOOL *flag, int len);
										// 合成(3面、半透明、グラフィック中間)
void __cdecl RendMix03BS(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, BOOL *flag, int len);
										// 合成(3面、特殊プライオリティ、グラフィック中間)
void __cdecl RendMix03C(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, BOOL *flag, int len);
										// 合成(3面、通常重ね合わせ、グラフィック後面)
void __cdecl RendMix03CH(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, DWORD c, BOOL *flag, int len);
										// 合成(3面、半透明、グラフィック後面)
void __cdecl RendMix03CS(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, BOOL *flag, int len);
										// 合成(3面、特殊プライオリティ、グラフィック後面)

#ifdef __cplusplus
}
#endif	// __cplusplus

#endif	// rend_asm_h
