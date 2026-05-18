//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001,2002 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2012 GIMONS
//	[ レンダラ(インライン) ]
//
//---------------------------------------------------------------------------

#if !defined(renderin_h)
#define renderin_h

#include "render.h"

//---------------------------------------------------------------------------
//
//	パレット設定
//
//---------------------------------------------------------------------------
inline void FASTCALL Render::SetPalette(int index)
{
	// VCの時点で比較チェックを行う
	render.palmod[index] = TRUE;
	render.palette = TRUE;
}

//---------------------------------------------------------------------------
//
//	テキストVRAM変更
//
//---------------------------------------------------------------------------
inline void FASTCALL Render::TextMem(DWORD addr)
{
	// テキストVRAMの時点で比較チェックを行う
	addr &= 0x1ffff;
	addr >>= 2;
	render.textflag[addr] = TRUE;
	addr >>= 5;
	render.textmod[addr] = TRUE;
}

//---------------------------------------------------------------------------
//
//	グラフィックVRAM変更
//
//---------------------------------------------------------------------------
inline void FASTCALL Render::GrpMem(DWORD addr, DWORD block)
{
	const VC::vc_t *p;
	int i;
	int gd;
	DWORD line;
	DWORD part;

	// グラフィックVRAMの時点で比較チェックを行う
	ASSERT(addr <= 0x7ffff);
	ASSERT(block <= 3);

	// 表示がなければ更新しない
	if (render.mixpage==0) {
		return;
	}

	// VCデータを取得
	p = vc->GetWorkAddr();

	switch (render.grptype) {
		case 0: // 1024x1
			for (i=0; i<4; i++) {
				if ( p->gp[i] == block ) {
					// 更新部分の算出
					line = addr >> 10;
					part = (addr >> 5) & 0x1f;

					// 16dotフラグ(16dot単位、(512/16)ｘ512ｘ4 = 0x10000)
					gd = i << 14;
					render.grpflag[(line<<5) | part | gd] = TRUE;

					// ラインフラグ(ライン単位、512x4 = 2048)
					gd =  i << 9;
					render.grpmod[line | gd] = TRUE;
				}
			}
			return;
		case 1: // 16x4
			for (i=0; i<4; i++) {
				if (p->gp[i] == block ) {
					// 更新部分の算出
					line = addr >> 10;
					part = (addr >> 5) & 0x1f;

					// 16dotフラグ(16dot単位、(512/16)ｘ512ｘ4 = 0x10000)
					gd = i << 14;
					render.grpflag[(line<<5) | part | gd] = TRUE;

					// ラインフラグ(ライン単位、512x4 = 2048)
					gd =  i << 9;
					render.grpmod[line | gd] = TRUE;
				}
			}
			return;
		case 2: // 256x2
			for (i=0; i<2; i++) {
				if ( p->gp[i] == block ) {
					// 更新部分の算出
					line = addr >> 10;
					part = (addr >> 5) & 0x1f;

					// スクロール値を考慮
					line = (line + render.grpy[p->gp[0]] - render.grpy[p->gp[i]]) & 0x1FF;
					part = (part + render.grpx[p->gp[0]] - render.grpx[p->gp[i]]) & 0x1FF;

					// 16dotフラグ(16dot単位、(512/16)ｘ512ｘ4 = 0x10000)
					gd = 0 << 14;
					render.grpflag[(line<<5) | part | gd] = TRUE;

					// ラインフラグ(ライン単位、512x4 = 2048)
					gd = 0 << 9;
					render.grpmod[line | gd] = TRUE;
				}
			}

			for (i=2; i<4; i++) {
				if ( p->gp[i] == block ) {
					// 更新部分の算出
					line = addr >> 10;
					part = (addr >> 5) & 0x1f;

					// スクロール値を考慮
					line = (line + render.grpy[p->gp[2]] - render.grpy[p->gp[i]]) & 0x1FF;
					part = (part + render.grpx[p->gp[2]] - render.grpx[p->gp[i]]) & 0x1FF;

					// 16dotフラグ(16dot単位、(512/16)ｘ512ｘ4 = 0x10000)
					gd = 2 << 14;
					render.grpflag[(line<<5) | part | gd] = TRUE;

					// ラインフラグ(ライン単位、512x4 = 2048)
					gd = 2 << 9;
					render.grpmod[line | gd] = TRUE;
				}
			}
			return;
		case 3: // Ndef
		case 4: // 64kx1
			for (i=0; i<4; i++) {
				if ( p->gp[i] == block ) {
					// 更新部分の算出
					line = addr >> 10;
					part = (addr >> 5) & 0x1f;

					// スクロール値を考慮
					line = (line + render.grpy[p->gp[0]] - render.grpy[p->gp[i]]) & 0x1FF;
					part = (part + render.grpx[p->gp[0]] - render.grpx[p->gp[i]]) & 0x1FF;

					// 16dotフラグ(16dot単位、(512/16)ｘ512ｘ4 = 0x10000)
					gd = 0 << 14;
					render.grpflag[(line<<5) | part | gd] = TRUE;

					// ラインフラグ(ライン単位、512x4 = 2048)
					gd = 0 << 9;
					render.grpmod[line | gd] = TRUE;
				}
			}
			return;
	}
}

//---------------------------------------------------------------------------
//
//	グラフィックVRAM変更(全て)
//
//---------------------------------------------------------------------------
inline void FASTCALL Render::GrpAll(DWORD line, DWORD block)
{
	const VC::vc_t *p;
	int i;
	int gd;

	ASSERT(line <= 0x1ff);
	ASSERT(block <= 3);

	// VCデータを取得
	p = vc->GetWorkAddr();

	for (i=0; i<4; i++) {
		if ( p->gp[i] == block ) {
			// 変更のあったブロックから表示ページを算出
			gd = i;
			render.grppal[(gd << 9) | line] = TRUE;
		}
	}
}

#endif	// renderin_h
