//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001,2002 PI (ytanaka@ipc-tokai.or.jp)
//	[ Video ]
//
//---------------------------------------------------------------------------

#if !defined(render_h)
#define render_h

#include "device.h"

//===========================================================================
//
//	Video
//
//===========================================================================
class Render : public Device
{
public:
		enum compositor_mode_t {
			compositor_original = 0,
			compositor_fast = 1
		};

	// Render work area structure
	typedef struct {
		// General state
		BOOL act;						// �E��E��E��E��E��E��E�Ă��E�邩
		BOOL enable;					// �E��E��E��E��E��E��E��E�
		int count;						// �E�X�E�P�E�W�E��E��E�[�E��E��E�A�E�g�E�J�E�E�E��E��E�^
		BOOL ready;						// �E�`�E�揀�E��E��E�ł��E�Ă��E�邩
		int first;						// �E��E��E��E��E��E��E��E��E�X�E�^
		int last;						// �E�\�E��E��E�I�E��E��E��E��E�X�E�^

		// CRTC
		BOOL crtc;						// CRTC�E�ύX�E�t�E��E��E�O
		int width;						// X�E��E��E��E��E�h�E�b�E�g�E��E�(256�E�`)
		int h_mul;						// X�E��E��E��E��E�{�E��E�(1,2)
		int height;						// Y�E��E��E��E��E�h�E�b�E�g�E��E�(256�E�`)
		int v_mul;						// Y�E��E��E��E��E�{�E��E�(0,1,2)
		BOOL lowres;					// 15kHz�E�t�E��E��E�O

		// VC
		BOOL vc;						// VC�E�ύX�E�t�E��E��E�O

		// �E��E��E��E�
		BOOL mix[1024];					// �E��E��E��E��E�t�E��E��E�O(�E��E��E�C�E��E�)
		DWORD *mixbuf;					// �E��E��E��E��E�o�E�b�E�t�E�@
		DWORD *mixptr[8];				// �E��E��E��E��E�|�E�C�E��E��E�^
		DWORD mixshift[8];				// �E��E��E��E��E�|�E�C�E��E��E�^�E��E�Y�E�V�E�t�E�g
		DWORD *mixx[8];					// �E��E��E��E��E�|�E�C�E��E��E�^�E��E�X�E�X�E�N�E��E��E�[�E��E��E�|�E�C�E��E��E�^
		DWORD *mixy[8];					// �E��E��E��E��E�|�E�C�E��E��E�^�E��E�Y�E�X�E�N�E��E��E�[�E��E��E�|�E�C�E��E��E�^
		DWORD mixand[8];				// �E��E��E��E��E�|�E�C�E��E��E�^�E�̃X�E�N�E��E��E�[�E��E�AND�E�l
		int mixmap[3];					// �E��E��E��E��E�}�E�b�E�v
		int mixtype;					// �E��E��E��E��E�^�E�C�E�v
		int mixpage;					// �E��E��E��E��E�O�E��E��E�t�E�B�E�b�E�N�E�y�E�[�E�W�E��E�
		int mixwidth;					// �E��E��E��E��E�o�E�b�E�t�E�@�E��E�
		int mixheight;					// �E��E��E��E��E�o�E�b�E�t�E�@�E��E��E��E�
		int mixlen;						// �E��E��E��E��E��E��E��E��E��E��E��E��E��E�(x�E��E��E��E�)

		// Draw
		BOOL draw[1024];				// �E�`�E��E�t�E��E��E�O(�E��E��E�C�E��E�)
		BOOL *drawflag;					// �E�`�E��E�t�E��E��E�O(16dot)

		// �E�R�E��E��E�g�E��E��E�X�E�g
		BOOL contrast;					// �E�R�E��E��E�g�E��E��E�X�E�g�E�ύX�E�t�E��E��E�O
		int contlevel;					// �E�R�E��E��E�g�E��E��E�X�E�g

		// Palette
		BOOL palette;					// �E�p�E��E��E�b�E�g�E�ύX�E�t�E��E��E�O
		BOOL palmod[0x200];				// �E�p�E��E��E�b�E�g�E�ύX�E�t�E��E��E�O
		DWORD *palbuf;					// �E�p�E��E��E�b�E�g�E�o�E�b�E�t�E�@
		DWORD *palptr;					// �E�p�E��E��E�b�E�g�E�|�E�C�E��E��E�^
		const WORD *palvc;				// �E�p�E��E��E�b�E�gVC�E�|�E�C�E��E��E�^
		DWORD paldata[0x200];			// �E�p�E��E��E�b�E�g�E�f�E�[�E�^
		BYTE pal64k[0x200];				// �E�p�E��E��E�b�E�g�E�f�E�[�E�^�E�ό`

		// Text VRAM
		BOOL texten;					// �E�e�E�L�E�X�E�g�E�\�E��E��E�t�E��E��E�O
		BOOL textpal[1024];				// �E�e�E�L�E�X�E�g�E�p�E��E��E�b�E�g�E�t�E��E��E�O
		BOOL textmod[1024];				// �E�e�E�L�E�X�E�g�E�X�E�V�E�t�E��E��E�O(�E��E��E�C�E��E�)
		BOOL *textflag;					// �E�e�E�L�E�X�E�g�E�X�E�V�E�t�E��E��E�O(32dot)
		BYTE *textbuf;					// �E�e�E�L�E�X�E�g�E�o�E�b�E�t�E�@(�E�p�E��E��E�b�E�g�E�O)
		DWORD *textout;					// �E�e�E�L�E�X�E�g�E�o�E�b�E�t�E�@(�E�p�E��E��E�b�E�g�E��E�)
		const BYTE *texttv;				// �E�e�E�L�E�X�E�gTVRAM�E�|�E�C�E��E��E�^
		DWORD textx;					// �E�e�E�L�E�X�E�g�E�X�E�N�E��E��E�[�E��E�X
		DWORD texty;					// �E�e�E�L�E�X�E�g�E�X�E�N�E��E��E�[�E��E�Y

		// Graphic VRAM
		int grptype;					// �E�O�E��E��E�t�E�B�E�b�E�N�E�^�E�C�E�v(0�E�`4)
		BOOL grpen[4];					// �E�O�E��E��E�t�E�B�E�b�E�N�E�u�E��E��E�b�E�N�E�\�E��E��E�t�E��E��E�O
		BOOL grppal[2048];				// �E�O�E��E��E�t�E�B�E�b�E�N�E�p�E��E��E�b�E�g�E�t�E��E��E�O
		BOOL grpmod[2048];				// �E�O�E��E��E�t�E�B�E�b�E�N�E�X�E�V�E�t�E��E��E�O(�E��E��E�C�E��E�)
		BOOL *grpflag;					// �E�O�E��E��E�t�E�B�E�b�E�N�E�X�E�V�E�t�E��E��E�O(16dot)
		DWORD *grpbuf[4];				// �E�O�E��E��E�t�E�B�E�b�E�N�E�u�E��E��E�b�E�N�E�o�E�b�E�t�E�@
		const BYTE* grpgv;				// �E�O�E��E��E�t�E�B�E�b�E�NGVRAM�E�|�E�C�E��E��E�^
		DWORD grpx[4];					// �E�O�E��E��E�t�E�B�E�b�E�N�E�u�E��E��E�b�E�N�E�X�E�N�E��E��E�[�E��E�X
		DWORD grpy[4];					// �E�O�E��E��E�t�E�B�E�b�E�N�E�u�E��E��E�b�E�N�E�X�E�N�E��E��E�[�E��E�Y

		// PCG
		BOOL pcgready[256 * 16];		// PCG�E��E��E��E�OK�E�t�E��E��E�O
		DWORD pcguse[256 * 16];			// PCG�E�g�E�p�E��E��E�J�E�E�E��E��E�g
		DWORD pcgpal[16];				// PCG�E�p�E��E��E�b�E�g�E�g�E�p�E�J�E�E�E��E��E�g
		DWORD *pcgbuf;					// PCG�E�o�E�b�E�t�E�@
		const BYTE* sprmem;				// �E�X�E�v�E��E��E�C�E�g�E��E��E��E��E��E�

		// �E�X�E�v�E��E��E�C�E�g
		DWORD **spptr;					// �E�X�E�v�E��E��E�C�E�g�E�|�E�C�E��E��E�^�E�o�E�b�E�t�E�@
		DWORD spreg[0x200];				// �E�X�E�v�E��E��E�C�E�g�E��E��E�W�E�X�E�^�E�ۑ�
		BOOL spuse[128];				// �E�X�E�v�E��E��E�C�E�g�E�g�E�p�E��E��E�t�E��E��E�O

		// BG
		DWORD bgreg[2][64 * 64];		// BG�E��E��E�W�E�X�E�^�E�{�E�ύX�E�t�E��E��E�O($10000)
		BOOL bgall[2][64];				// BG�E�ύX�E�t�E��E��E�O(�E�u�E��E��E�b�E�N�E�P�E��E�)
		BOOL bgdisp[2];					// BG�E�\�E��E��E�t�E��E��E�O
		BOOL bgarea[2];					// BG�E�\�E��E��E�G�E��E��E�A
		BOOL bgsize;					// BG�E�\�E��E��E�T�E�C�E�Y(16dot=TRUE)
		DWORD **bgptr[2];				// BG�E�|�E�C�E��E��E�^+�E�f�E�[�E�^
		BOOL bgmod[2][1024];			// BG�E�X�E�V�E�t�E��E��E�O
		DWORD bgx[2];					// BG�E�X�E�N�E��E��E�[�E��E�(X)
		DWORD bgy[2];					// BG�E�X�E�N�E��E��E�[�E��E�(Y)

		// BG/Sprite combined
		BOOL bgspflag;					// BG/�E�X�E�v�E��E��E�C�E�g�E�\�E��E��E�t�E��E��E�O
		BOOL bgspdisp;					// BG/�E�X�E�v�E��E��E�C�E�gCPU/Video�E�t�E��E��E�O
		BOOL bgspmod[512];				// BG/�E�X�E�v�E��E��E�C�E�g�E�X�E�V�E�t�E��E��E�O
		DWORD *bgspbuf;					// BG/�E�X�E�v�E��E��E�C�E�g�E�o�E�b�E�t�E�@
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
		DWORD zero;						// �E�X�E�N�E��E��E�[�E��E��E�_�E�~�E�[(0)
	} render_t;

	// �E��E�{�E�t�E�@�E��E��E�N�E�V�E��E��E��E�
	Render(VM *p);
										// �E�R�E��E��E�X�E�g�E��E��E�N�E�^
	BOOL FASTCALL Init();
										// �E��E��E��E��E��E�
	void FASTCALL Cleanup();
										// �E�N�E��E��E�[�E��E��E�A�E�b�E�v
	void FASTCALL Reset();
										// �E��E��E�Z�E�b�E�g
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// �E�Z�E�[�E�u
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// �E��E��E�[�E�h
	void FASTCALL ApplyCfg(const Config *config);
										// �E�ݒ�K�E�p

	// Render API (Constructor/Destructor)
	void FASTCALL EnableAct(BOOL enable){ render.enable = enable; }
										// Rendering enable
	BOOL FASTCALL IsActive() const		{ return render.act; }
										// Is active
	BOOL FASTCALL IsReady() const		{ return (BOOL)(render.count > 0); }
	void FASTCALL Complete()			{ render.count = 0; }
	void FASTCALL SetTransparencyEnabled(BOOL enabled)	{ transparency_enabled = enabled ? TRUE : FALSE; }
	BOOL FASTCALL IsTransparencyEnabled() const		{ return transparency_enabled; }
	void FASTCALL SetOriginalBG0RenderEnabled(BOOL enabled)	{ original_bg0_render_enabled = enabled ? TRUE : FALSE; }
	BOOL FASTCALL IsOriginalBG0RenderEnabled() const		{ return original_bg0_render_enabled; }
	BOOL FASTCALL SetCompositorMode(int mode);
	int FASTCALL GetCompositorMode() const		{ return compositor_mode; }
	DWORD FASTCALL GetFastFallbackCount() const	{ return fast_fallback_count; }
	void FASTCALL StartFrame();
										// Frame start (V-DISP)
	void FASTCALL EndFrame();
										// Frame end (V-BLANK)
	void FASTCALL HSync(int raster);
										// Horizontal sync (raster until end)
	void FASTCALL SetMixBuf(DWORD *buf, int width, int height);
										// Mix buffer set
	render_t* FASTCALL GetWorkAddr() 	{ return &render; }
										// Get work address

	// Render API (Device)
	void FASTCALL SetCRTC();
										// CRTC reset
	void FASTCALL SetVC();
										// VC reset
	void FASTCALL ForceRecompose();
	void FASTCALL SetContrast(int cont);
										// Contrast setting
	int FASTCALL GetContrast() const;
										// Contrast get
	void FASTCALL SetPalette(int index);
										// Palette setting
	const DWORD* FASTCALL GetPalette() const;
										// Palette buffer get
	void FASTCALL TextMem(DWORD addr);
										// Text VRAM change
	void FASTCALL TextScrl(DWORD x, DWORD y);
										// Text scroll change
	void FASTCALL TextCopy(DWORD src, DWORD dst, DWORD plane);
										// Text copy
	void FASTCALL GrpMem(DWORD addr, DWORD block);
										// Graphic VRAM change
	void FASTCALL GrpAll(DWORD line, DWORD block);
										// Graphic VRAM change
	void FASTCALL GrpScrl(int block, DWORD x, DWORD y);
										// Graphic scroll change
	void FASTCALL SpriteReg(DWORD addr, DWORD data);
										// Sprite register change
	void FASTCALL BGScrl(int page, DWORD x, DWORD y);
										// BG scroll change
	void FASTCALL BGCtrl(int index, BOOL flag);
										// BG control change
	void FASTCALL BGMem(DWORD addr, WORD data);
										// BG change
	void FASTCALL PCGMem(DWORD addr);
										// PCG change

	const DWORD* FASTCALL GetTextBuf() const;
										// Text buffer get
	const DWORD* FASTCALL GetGrpBuf(int index) const;
										// Graphic buffer get
	const DWORD* FASTCALL GetPCGBuf() const;
										// PCG buffer get
	const DWORD* FASTCALL GetBGSpBuf() const;
										// BG/Sprite buffer get
	const DWORD* FASTCALL GetMixBuf() const;
										// Mix buffer get

private:
	class Backend;
	void FASTCALL StartFrameOriginal();
	void FASTCALL StartFrameFast();
	void FASTCALL EndFrameOriginal();
	void FASTCALL EndFrameFast();
	void FASTCALL HSyncOriginal(int raster);
	void FASTCALL HSyncFast(int raster);
	void FASTCALL SetCRTCOriginal();
	void FASTCALL SetCRTCFast();
	void FASTCALL SetVCOriginal();
	void FASTCALL SetVCFast();
	void FASTCALL InvalidateFrame();
	void FASTCALL InvalidateAll();
	void FASTCALL Process();
	void FASTCALL ProcessFast();
										// Video internal operation
	void FASTCALL Video();
	void FASTCALL VideoFastPX68K();
										// VC process
	void FASTCALL SetupGrp(int first);
										// Graphic reset setup
	void FASTCALL Contrast();
										// Contrast operation
	void FASTCALL Palette();
	void FASTCALL PaletteFastPX68K();
										// Palette operation
	void FASTCALL MakePalette();
										// Make palette
	DWORD FASTCALL ConvPalette(int color, int ratio);
										// Color conversion
	void FASTCALL Text(int raster);
	void FASTCALL TextFastPX68K(int raster);
										// Text
	void FASTCALL Grp(int block, int raster);
										// Graphic
	void FASTCALL SpriteReset();
										// Sprite register reset
	void FASTCALL BGSprite(int raster);
										// BG/Sprite
	void FASTCALL BG(int page, int raster, DWORD *buf, BOOL force);
										// BG
	void FASTCALL BGBlock(int page, int y);
										// BG (block unit)
	void FASTCALL Mix(int offset);
										// Mix
	void FASTCALL MixFast(int y);
	void FASTCALL MixFastLine(int dst_y, int src_y);
	void FASTCALL FastBuildBGLinePX(int sprite_raster, int bg_raster, BOOL ton, int tx_pri, int sp_pri, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active, BOOL *bg_opaq);
	void FASTCALL FastDrawSpriteLinePX(int raster, int pri, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active);
	void FASTCALL FastDrawBGPageLinePX(int page, int raster, BOOL gd, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active);
	void FASTCALL FastMixGrp(int y, DWORD *grp, DWORD *grp_sp, DWORD *grp_sp2,
		BOOL *grp_sp_tr, BOOL *gon, BOOL *tron, BOOL *pron);
	void FASTCALL MixGrp(int y, DWORD *buf);
										// Mix (graphic)
	CRTC *crtc;
										// CRTC
	VC *vc;
										// VC
	Sprite *sprite;
										// Sprite
	Backend *backend;
	Backend *backend_original;
	Backend *backend_fast;
	int compositor_mode;
	DWORD *palbuf_original;
	DWORD *palbuf_fast;
	DWORD fast_fallback_count;
	BOOL transparency_enabled;
	BOOL original_bg0_render_enabled;
	render_t render;
										// Render data
	BOOL cmov;
										// CMOV available flag
};

#endif	// render_h
