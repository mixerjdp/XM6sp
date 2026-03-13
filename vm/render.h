//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001,2002 ・ｽo・ｽh・ｽD(ytanaka@ipc-tokai.or.jp)
//	[ ・ｽ・ｽ・ｽ・ｽ・ｽ_・ｽ・ｽ ]
//
//---------------------------------------------------------------------------

#if !defined(render_h)
#define render_h

#include "device.h"

//===========================================================================
//
//	・ｽ・ｽ・ｽ・ｽ・ｽ_・ｽ・ｽ
//
//===========================================================================
class Render : public Device
{
public:
		enum compositor_mode_t {
			compositor_original = 0,
			compositor_fast = 1
		};

	// ・ｽ・ｽ・ｽ・ｽ・ｽf・ｽ[・ｽ^・ｽ・ｽ`
	typedef struct {
		// ・ｽS・ｽﾌ撰ｿｽ・ｽ・ｽ
		BOOL act;						// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽﾄゑｿｽ・ｽ驍ｩ
		BOOL enable;					// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
		int count;						// ・ｽX・ｽP・ｽW・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽA・ｽg・ｽJ・ｽE・ｽ・ｽ・ｽ^
		BOOL ready;						// ・ｽ`・ｽ謠・ｽ・ｽ・ｽﾅゑｿｽ・ｽﾄゑｿｽ・ｽ驍ｩ
		int first;						// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽX・ｽ^
		int last;						// ・ｽ\・ｽ・ｽ・ｽI・ｽ・ｽ・ｽ・ｽ・ｽX・ｽ^

		// CRTC
		BOOL crtc;						// CRTC・ｽﾏ更・ｽt・ｽ・ｽ・ｽO
		int width;						// X・ｽ・ｽ・ｽ・ｽ・ｽh・ｽb・ｽg・ｽ・ｽ(256・ｽ`)
		int h_mul;						// X・ｽ・ｽ・ｽ・ｽ・ｽ{・ｽ・ｽ(1,2)
		int height;						// Y・ｽ・ｽ・ｽ・ｽ・ｽh・ｽb・ｽg・ｽ・ｽ(256・ｽ`)
		int v_mul;						// Y・ｽ・ｽ・ｽ・ｽ・ｽ{・ｽ・ｽ(0,1,2)
		BOOL lowres;					// 15kHz・ｽt・ｽ・ｽ・ｽO

		// VC
		BOOL vc;						// VC・ｽﾏ更・ｽt・ｽ・ｽ・ｽO

		// ・ｽ・ｽ・ｽ・ｽ
		BOOL mix[1024];					// ・ｽ・ｽ・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO(・ｽ・ｽ・ｽC・ｽ・ｽ)
		DWORD *mixbuf;					// ・ｽ・ｽ・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@
		DWORD *mixptr[8];				// ・ｽ・ｽ・ｽ・ｽ・ｽ|・ｽC・ｽ・ｽ・ｽ^
		DWORD mixshift[8];				// ・ｽ・ｽ・ｽ・ｽ・ｽ|・ｽC・ｽ・ｽ・ｽ^・ｽ・ｽY・ｽV・ｽt・ｽg
		DWORD *mixx[8];					// ・ｽ・ｽ・ｽ・ｽ・ｽ|・ｽC・ｽ・ｽ・ｽ^・ｽ・ｽX・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ|・ｽC・ｽ・ｽ・ｽ^
		DWORD *mixy[8];					// ・ｽ・ｽ・ｽ・ｽ・ｽ|・ｽC・ｽ・ｽ・ｽ^・ｽ・ｽY・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ|・ｽC・ｽ・ｽ・ｽ^
		DWORD mixand[8];				// ・ｽ・ｽ・ｽ・ｽ・ｽ|・ｽC・ｽ・ｽ・ｽ^・ｽﾌス・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽAND・ｽl
		int mixmap[3];					// ・ｽ・ｽ・ｽ・ｽ・ｽ}・ｽb・ｽv
		int mixtype;					// ・ｽ・ｽ・ｽ・ｽ・ｽ^・ｽC・ｽv
		int mixpage;					// ・ｽ・ｽ・ｽ・ｽ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽy・ｽ[・ｽW・ｽ・ｽ
		int mixwidth;					// ・ｽ・ｽ・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ
		int mixheight;					// ・ｽ・ｽ・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽ・ｽ・ｽ・ｽ
		int mixlen;						// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ(x・ｽ・ｽ・ｽ・ｽ)

		// ・ｽ`・ｽ・ｽ
		BOOL draw[1024];				// ・ｽ`・ｽ・ｽt・ｽ・ｽ・ｽO(・ｽ・ｽ・ｽC・ｽ・ｽ)
		BOOL *drawflag;					// ・ｽ`・ｽ・ｽt・ｽ・ｽ・ｽO(16dot)

		// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽX・ｽg
		BOOL contrast;					// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽX・ｽg・ｽﾏ更・ｽt・ｽ・ｽ・ｽO
		int contlevel;					// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽX・ｽg

		// ・ｽp・ｽ・ｽ・ｽb・ｽg
		BOOL palette;					// ・ｽp・ｽ・ｽ・ｽb・ｽg・ｽﾏ更・ｽt・ｽ・ｽ・ｽO
		BOOL palmod[0x200];				// ・ｽp・ｽ・ｽ・ｽb・ｽg・ｽﾏ更・ｽt・ｽ・ｽ・ｽO
		DWORD *palbuf;					// ・ｽp・ｽ・ｽ・ｽb・ｽg・ｽo・ｽb・ｽt・ｽ@
		DWORD *palptr;					// ・ｽp・ｽ・ｽ・ｽb・ｽg・ｽ|・ｽC・ｽ・ｽ・ｽ^
		const WORD *palvc;				// ・ｽp・ｽ・ｽ・ｽb・ｽgVC・ｽ|・ｽC・ｽ・ｽ・ｽ^
		DWORD paldata[0x200];			// ・ｽp・ｽ・ｽ・ｽb・ｽg・ｽf・ｽ[・ｽ^
		BYTE pal64k[0x200];				// ・ｽp・ｽ・ｽ・ｽb・ｽg・ｽf・ｽ[・ｽ^・ｽﾏ形

		// ・ｽe・ｽL・ｽX・ｽgVRAM
		BOOL texten;					// ・ｽe・ｽL・ｽX・ｽg・ｽ\・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO
		BOOL textpal[1024];				// ・ｽe・ｽL・ｽX・ｽg・ｽp・ｽ・ｽ・ｽb・ｽg・ｽt・ｽ・ｽ・ｽO
		BOOL textmod[1024];				// ・ｽe・ｽL・ｽX・ｽg・ｽX・ｽV・ｽt・ｽ・ｽ・ｽO(・ｽ・ｽ・ｽC・ｽ・ｽ)
		BOOL *textflag;					// ・ｽe・ｽL・ｽX・ｽg・ｽX・ｽV・ｽt・ｽ・ｽ・ｽO(32dot)
		BYTE *textbuf;					// ・ｽe・ｽL・ｽX・ｽg・ｽo・ｽb・ｽt・ｽ@(・ｽp・ｽ・ｽ・ｽb・ｽg・ｽO)
		DWORD *textout;					// ・ｽe・ｽL・ｽX・ｽg・ｽo・ｽb・ｽt・ｽ@(・ｽp・ｽ・ｽ・ｽb・ｽg・ｽ・ｽ)
		const BYTE *texttv;				// ・ｽe・ｽL・ｽX・ｽgTVRAM・ｽ|・ｽC・ｽ・ｽ・ｽ^
		DWORD textx;					// ・ｽe・ｽL・ｽX・ｽg・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽX
		DWORD texty;					// ・ｽe・ｽL・ｽX・ｽg・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽY

		// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽNVRAM
		int grptype;					// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽ^・ｽC・ｽv(0・ｽ`4)
		BOOL grpen[4];					// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽu・ｽ・ｽ・ｽb・ｽN・ｽ\・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO
		BOOL grppal[2048];				// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽp・ｽ・ｽ・ｽb・ｽg・ｽt・ｽ・ｽ・ｽO
		BOOL grpmod[2048];				// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽX・ｽV・ｽt・ｽ・ｽ・ｽO(・ｽ・ｽ・ｽC・ｽ・ｽ)
		BOOL *grpflag;					// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽX・ｽV・ｽt・ｽ・ｽ・ｽO(16dot)
		DWORD *grpbuf[4];				// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽu・ｽ・ｽ・ｽb・ｽN・ｽo・ｽb・ｽt・ｽ@
		const BYTE* grpgv;				// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽNGVRAM・ｽ|・ｽC・ｽ・ｽ・ｽ^
		DWORD grpx[4];					// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽu・ｽ・ｽ・ｽb・ｽN・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽX
		DWORD grpy[4];					// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽu・ｽ・ｽ・ｽb・ｽN・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽY

		// PCG
		BOOL pcgready[256 * 16];		// PCG・ｽ・ｽ・ｽ・ｽOK・ｽt・ｽ・ｽ・ｽO
		DWORD pcguse[256 * 16];			// PCG・ｽg・ｽp・ｽ・ｽ・ｽJ・ｽE・ｽ・ｽ・ｽg
		DWORD pcgpal[16];				// PCG・ｽp・ｽ・ｽ・ｽb・ｽg・ｽg・ｽp・ｽJ・ｽE・ｽ・ｽ・ｽg
		DWORD *pcgbuf;					// PCG・ｽo・ｽb・ｽt・ｽ@
		const BYTE* sprmem;				// ・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ

		// ・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg
		DWORD **spptr;					// ・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽ|・ｽC・ｽ・ｽ・ｽ^・ｽo・ｽb・ｽt・ｽ@
		DWORD spreg[0x200];				// ・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽ・ｽ・ｽW・ｽX・ｽ^・ｽﾛ托ｿｽ
		BOOL spuse[128];				// ・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽg・ｽp・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO

		// BG
		DWORD bgreg[2][64 * 64];		// BG・ｽ・ｽ・ｽW・ｽX・ｽ^・ｽ{・ｽﾏ更・ｽt・ｽ・ｽ・ｽO($10000)
		BOOL bgall[2][64];				// BG・ｽﾏ更・ｽt・ｽ・ｽ・ｽO(・ｽu・ｽ・ｽ・ｽb・ｽN・ｽP・ｽ・ｽ)
		BOOL bgdisp[2];					// BG・ｽ\・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO
		BOOL bgarea[2];					// BG・ｽ\・ｽ・ｽ・ｽG・ｽ・ｽ・ｽA
		BOOL bgsize;					// BG・ｽ\・ｽ・ｽ・ｽT・ｽC・ｽY(16dot=TRUE)
		DWORD **bgptr[2];				// BG・ｽ|・ｽC・ｽ・ｽ・ｽ^+・ｽf・ｽ[・ｽ^
		BOOL bgmod[2][1024];			// BG・ｽX・ｽV・ｽt・ｽ・ｽ・ｽO
		DWORD bgx[2];					// BG・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ(X)
		DWORD bgy[2];					// BG・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ(Y)

		// BG/・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽ・ｽ・ｽ・ｽ
		BOOL bgspflag;					// BG/・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽ\・ｽ・ｽ・ｽt・ｽ・ｽ・ｽO
		BOOL bgspdisp;					// BG/・ｽX・ｽv・ｽ・ｽ・ｽC・ｽgCPU/Video・ｽt・ｽ・ｽ・ｽO
		BOOL bgspmod[512];				// BG/・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽX・ｽV・ｽt・ｽ・ｽ・ｽO
		DWORD *bgspbuf;					// BG/・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽo・ｽb・ｽt・ｽ@
		DWORD fast_bg_linebuf[1024];
		WORD fast_bg_pribuf[1024];
		BYTE fast_text_trflag[1024];
		DWORD fast_grp_linebuf[1024];
		DWORD fast_grp_linebuf_sp[1024];
		DWORD fast_grp_linebuf_sp2[1024];
		BOOL fast_grp_linebuf_sp_tr[1024];
		DWORD fast_stamp_counter;
		DWORD fast_mix_stamp[1024];
		DWORD fast_mix_done[1024];
		DWORD fast_bg_stamp[512];
		DWORD fast_bg_done[512];
		DWORD zero;						// ・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽ_・ｽ~・ｽ[(0)
	} render_t;

	// ・ｽ・ｽ{・ｽt・ｽ@・ｽ・ｽ・ｽN・ｽV・ｽ・ｽ・ｽ・ｽ
	Render(VM *p);
										// ・ｽR・ｽ・ｽ・ｽX・ｽg・ｽ・ｽ・ｽN・ｽ^
	BOOL FASTCALL Init();
										// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	void FASTCALL Cleanup();
										// ・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽA・ｽb・ｽv
	void FASTCALL Reset();
										// ・ｽ・ｽ・ｽZ・ｽb・ｽg
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// ・ｽZ・ｽ[・ｽu
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// ・ｽ・ｽ・ｽ[・ｽh
	void FASTCALL ApplyCfg(const Config *config);
										// ・ｽﾝ抵ｿｽK・ｽp

	// ・ｽO・ｽ・ｽAPI(・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ)
	void FASTCALL EnableAct(BOOL enable){ render.enable = enable; }
										// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ
	BOOL FASTCALL IsActive() const		{ return render.act; }
										// ・ｽA・ｽN・ｽe・ｽB・ｽu・ｽ・ｽ
	BOOL FASTCALL IsReady() const		{ return (BOOL)(render.count > 0); }
	void FASTCALL Complete()			{ render.count = 0; }
	BOOL FASTCALL SetCompositorMode(int mode);
	int FASTCALL GetCompositorMode() const		{ return compositor_mode; }
	DWORD FASTCALL GetFastFallbackCount() const	{ return fast_fallback_count; }
	void FASTCALL StartFrame();
										// ・ｽt・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽJ・ｽn(V-DISP)
	void FASTCALL EndFrame();
										// ・ｽt・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽI・ｽ・ｽ(V-BLANK)
	void FASTCALL HSync(int raster);
										// ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ・ｽ(raster・ｽﾜで終・ｽ・ｽ・ｽ)
	void FASTCALL SetMixBuf(DWORD *buf, int width, int height);
										// ・ｽ・ｽ・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽw・ｽ・ｽ
	render_t* FASTCALL GetWorkAddr() 	{ return &render; }
										// ・ｽ・ｽ・ｽ[・ｽN・ｽA・ｽh・ｽ・ｽ・ｽX・ｽ謫ｾ

	// ・ｽO・ｽ・ｽAPI(・ｽ・ｽ・ｽ)
	void FASTCALL SetCRTC();
										// CRTC・ｽZ・ｽb・ｽg
	void FASTCALL SetVC();
										// VC・ｽZ・ｽb・ｽg
	void FASTCALL SetContrast(int cont);
										// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽX・ｽg・ｽﾝ抵ｿｽ
	int FASTCALL GetContrast() const;
										// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽX・ｽg・ｽ謫ｾ
	void FASTCALL SetPalette(int index);
										// ・ｽp・ｽ・ｽ・ｽb・ｽg・ｽﾝ抵ｿｽ
	const DWORD* FASTCALL GetPalette() const;
										// ・ｽp・ｽ・ｽ・ｽb・ｽg・ｽo・ｽb・ｽt・ｽ@・ｽ謫ｾ
	void FASTCALL TextMem(DWORD addr);
										// ・ｽe・ｽL・ｽX・ｽgVRAM・ｽﾏ更
	void FASTCALL TextScrl(DWORD x, DWORD y);
										// ・ｽe・ｽL・ｽX・ｽg・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾏ更
	void FASTCALL TextCopy(DWORD src, DWORD dst, DWORD plane);
										// ・ｽ・ｽ・ｽX・ｽ^・ｽR・ｽs・ｽ[
	void FASTCALL GrpMem(DWORD addr, DWORD block);
										// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽNVRAM・ｽﾏ更
	void FASTCALL GrpAll(DWORD line, DWORD block);
										// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽNVRAM・ｽﾏ更
	void FASTCALL GrpScrl(int block, DWORD x, DWORD y);
										// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾏ更
	void FASTCALL SpriteReg(DWORD addr, DWORD data);
										// ・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽ・ｽ・ｽW・ｽX・ｽ^・ｽﾏ更
	void FASTCALL BGScrl(int page, DWORD x, DWORD y);
										// BG・ｽX・ｽN・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾏ更
	void FASTCALL BGCtrl(int index, BOOL flag);
										// BG・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽ[・ｽ・ｽ・ｽﾏ更
	void FASTCALL BGMem(DWORD addr, WORD data);
										// BG・ｽﾏ更
	void FASTCALL PCGMem(DWORD addr);
										// PCG・ｽﾏ更

	const DWORD* FASTCALL GetTextBuf() const;
										// ・ｽe・ｽL・ｽX・ｽg・ｽo・ｽb・ｽt・ｽ@・ｽ謫ｾ
	const DWORD* FASTCALL GetGrpBuf(int index) const;
										// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽo・ｽb・ｽt・ｽ@・ｽ謫ｾ
	const DWORD* FASTCALL GetPCGBuf() const;
										// PCG・ｽo・ｽb・ｽt・ｽ@・ｽ謫ｾ
	const DWORD* FASTCALL GetBGSpBuf() const;
										// BG/・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽo・ｽb・ｽt・ｽ@・ｽ謫ｾ
	const DWORD* FASTCALL GetMixBuf() const;
										// ・ｽ・ｽ・ｽ・ｽ・ｽo・ｽb・ｽt・ｽ@・ｽ謫ｾ

private:
	class Backend;
	void FASTCALL StartFrameOriginal();
	void FASTCALL StartFrameFast();
	void FASTCALL EndFrameOriginal();
	void FASTCALL HSyncOriginal(int raster);
	void FASTCALL SetCRTCOriginal();
	void FASTCALL SetVCOriginal();
	void FASTCALL InvalidateFrame();
	void FASTCALL InvalidateAll();
	void FASTCALL Process();
	void FASTCALL ProcessFast();
										// ・ｽ・ｽ・ｽ・ｽ・ｽ_・ｽ・ｽ・ｽ・ｽ・ｽO
	void FASTCALL Video();
										// VC・ｽ・ｽ・ｽ・ｽ
	void FASTCALL SetupGrp(int first);
										// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN・ｽZ・ｽb・ｽg・ｽA・ｽb・ｽv
	void FASTCALL Contrast();
										// ・ｽR・ｽ・ｽ・ｽg・ｽ・ｽ・ｽX・ｽg・ｽ・ｽ・ｽ・ｽ
	void FASTCALL Palette();
										// ・ｽp・ｽ・ｽ・ｽb・ｽg・ｽ・ｽ・ｽ・ｽ
	void FASTCALL MakePalette();
										// ・ｽp・ｽ・ｽ・ｽb・ｽg・ｽ・ｬ
	DWORD FASTCALL ConvPalette(int color, int ratio);
										// ・ｽF・ｽﾏ奇ｿｽ
	void FASTCALL Text(int raster);
										// ・ｽe・ｽL・ｽX・ｽg
	void FASTCALL Grp(int block, int raster);
										// ・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN
	void FASTCALL SpriteReset();
										// ・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg・ｽ・ｽ・ｽZ・ｽb・ｽg
	void FASTCALL BGSprite(int raster);
										// BG/・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg
	void FASTCALL BG(int page, int raster, DWORD *buf);
										// BG
	void FASTCALL BGBlock(int page, int y);
										// BG(・ｽ・ｽ・ｽu・ｽ・ｽ・ｽb・ｽN)
	void FASTCALL Mix(int offset);
										// ・ｽ・ｽ・ｽ・ｽ
	void FASTCALL MixFast(int y);
	void FASTCALL MixFastLine(int dst_y, int src_y);
	void FASTCALL FastBuildBGLinePX(int src_y, BOOL ton, int tx_pri, int sp_pri, DWORD *bg_line, BYTE *bg_flag, BOOL *active, BOOL *bg_opaq);
	void FASTCALL FastDrawSpriteLinePX(int raster, int pri, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active);
	void FASTCALL FastDrawBGPageLinePX(int page, int raster, BOOL gd, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active);
	void FASTCALL FastMixGrp(int y, DWORD *grp, DWORD *grp_sp, DWORD *grp_sp2,
		BOOL *grp_sp_tr, BOOL *gon, BOOL *tron, BOOL *pron);
	void FASTCALL MixGrp(int y, DWORD *buf);
										// ・ｽ・ｽ・ｽ・ｽ(・ｽO・ｽ・ｽ・ｽt・ｽB・ｽb・ｽN)
	CRTC *crtc;
										// CRTC
	VC *vc;
										// VC
	Sprite *sprite;
										// ・ｽX・ｽv・ｽ・ｽ・ｽC・ｽg
	Backend *backend;
	Backend *backend_original;
	Backend *backend_fast;
	int compositor_mode;
	DWORD fast_fallback_count;
	render_t render;
										// ・ｽ・ｽ・ｽ・ｽ・ｽf・ｽ[・ｽ^
	BOOL cmov;
										// CMOV・ｽL・ｽ・ｽ・ｽb・ｽV・ｽ・ｽ
};

#endif	// render_h

