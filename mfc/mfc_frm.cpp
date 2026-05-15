//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[MFC application]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "schedule.h"
#include "memory.h"
#include "sasi.h"
#include "scsi.h"
#include "fdd.h"
#include "fdc.h"
#include "fdi.h"
#include "render.h"
#include "crtc.h"
#include "vc.h"
#include "sprite.h"
#include "keyboard.h"
#include "ppi.h"
#include "mfc_frm.h"
#include "mfc_draw.h"
#include "mfc_res.h"
#include "mfc_sub.h"
#include "mfc_cpu.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_snd.h"
#include "mfc_inp.h"
#include "mfc_port.h"
#include "mfc_midi.h"
#include "mfc_tkey.h"
#include "mfc_host.h"
#include "mfc_info.h"
#include "mfc_cfg.h"
#include "mfc_stat.h"
static void FASTCALL VMHostSyncLockCallback(void *user)
{
	(void)user;
	::LockVM();
}

static void FASTCALL VMHostSyncUnlockCallback(void *user)
{
	(void)user;
	::UnlockVM();
}

static void FASTCALL VMHostMessageCallback(const TCHAR* message, void *user)
{
	(void)user;
	if (message) {
		::OutputDebugString(message);
		::OutputDebugString(_T("\n"));
	}
}


//===========================================================================
//
//	Frame window

//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Shell constant definitions
//	Must be defined by the application, not by an include file.
//
//---------------------------------------------------------------------------
#define SHCNRF_InterruptLevel			0x0001
#define SHCNRF_ShellLevel				0x0002
#define SHCNRF_NewDelivery				0x8000
static BOOL IsSmokeSaveStateCommand()
{
	return (_tcsstr(AfxGetApp()->m_lpCmdLine, _T("--smoke-savestate")) != NULL);
}

static BOOL IsSmokeVisibleCommand()
{
	return (_tcsstr(AfxGetApp()->m_lpCmdLine, _T("--smoke-visible")) != NULL);
}

static BOOL IsSmokePx68kVideoCommand(LPCTSTR lpszCmd)
{
	return (_tcsstr(lpszCmd, _T("--smoke-px68k-video")) != NULL) ||
		(_tcsstr(lpszCmd, _T("--smoke-render-px68k")) != NULL);
}

enum {
	SmokeActionKey = 1,
	SmokeActionJoy = 2,
	SmokeActionJoyAxis = 3,
	SmokeActionMax = 64
};

typedef struct {
	int type;
	DWORD key;
	int port;
	int button;
	int axis;
	DWORD axis_value;
	DWORD start;
	DWORD end;
	BOOL active;
	TCHAR name[32];
} smoke_action_t;

static BOOL g_smokeVisibleActive = FALSE;
static BOOL g_smokeVisibleSaved = FALSE;
static BOOL g_smokeVisibleTickLogged = FALSE;
static BOOL g_smokeVisibleActionsParsed = FALSE;
static TCHAR g_smokeVisibleCmd[2048];
static smoke_action_t g_smokeVisibleActions[SmokeActionMax];
static int g_smokeVisibleActionCount = 0;
static DWORD g_smokeVisibleFrames = 0;
static DWORD g_smokeVisibleTargetFrames = 0;
static DWORD g_smokeVisibleSaveFrame = 0;
static DWORD g_smokeVisibleFirstActionFrame = 0xffffffff;
static DWORD g_smokeVisibleLastActionFrame = 0;
static DWORD g_smokeVisibleLastCount = 0;
static DWORD g_smokeVisibleHoldMs = 0;
static DWORD g_smokeVisibleRunStartTick = 0;
static DWORD g_smokeVisiblePollTick = 0;
static Filepath g_smokeVisibleStatePath;
static PPI::joyinfo_t g_smokeVisibleJoy[PPI::PortMax];
static CRTC *g_smokeVisibleCRTC = NULL;
static Keyboard *g_smokeVisibleKeyboard = NULL;
static PPI *g_smokeVisiblePPI = NULL;
static BOOL g_smokeVisiblePx68kVideo = FALSE;

static void SmokeLogLine(LPCTSTR msg)
{
	FILE *fp;

	fp = _tfopen(_T("C:\\tmp2\\xm6_smoke_savestate.log"), _T("at"));
	if (!fp) {
		fp = _tfopen(_T("xm6_smoke_savestate.log"), _T("at"));
	}
	if (fp) {
		_ftprintf(fp, _T("%s\n"), msg);
		fclose(fp);
	}
}

static void SmokeLogFormat(LPCTSTR fmt, LPCTSTR value)
{
	FILE *fp;

	fp = _tfopen(_T("C:\\tmp2\\xm6_smoke_savestate.log"), _T("at"));
	if (!fp) {
		fp = _tfopen(_T("xm6_smoke_savestate.log"), _T("at"));
	}
	if (fp) {
		_ftprintf(fp, fmt, value);
		_ftprintf(fp, _T("\n"));
		fclose(fp);
	}
}

static void SmokeLogFormatDword(LPCTSTR fmt, DWORD value)
{
	FILE *fp;

	fp = _tfopen(_T("C:\\tmp2\\xm6_smoke_savestate.log"), _T("at"));
	if (!fp) {
		fp = _tfopen(_T("xm6_smoke_savestate.log"), _T("at"));
	}
	if (fp) {
		_ftprintf(fp, fmt, (unsigned long)value);
		_ftprintf(fp, _T("\n"));
		fclose(fp);
	}
}

static DWORD SmokeHashDword(DWORD hash, DWORD value)
{
	hash ^= value;
	hash *= 16777619u;
	return hash;
}

static void SmokeLogRenderPage(const Render::render_t *r, int page)
{
	const DWORD *ptr;
	DWORD hash;
	DWORD nonzero;
	DWORD transparent;
	DWORD half;
	int width;
	int height;
	int y;
	int x;
	TCHAR line[256];

	ASSERT(r);
	ASSERT(page >= 0 && page < 4);

	ptr = r->grpbuf[page];
	if (!ptr) {
		_sntprintf(line, _countof(line), _T("render-page%d: missing"), page);
		SmokeLogLine(line);
		return;
	}

	width = r->mixlen;
	if (width <= 0 || width > 512) {
		width = 512;
	}
	height = r->height;
	if (height <= 0 || height > 512) {
		height = 512;
	}

	hash = 2166136261u;
	nonzero = 0;
	transparent = 0;
	half = 0;
	for (y = 0; y < height; y++) {
		const DWORD *row = ptr + (y << 10);
		for (x = 0; x < width; x++) {
			DWORD value = row[x];
			hash = SmokeHashDword(hash, value);
			if ((value & 0x00ffffff) != 0) {
				nonzero++;
			}
			if (value & 0x80000000) {
				transparent++;
			}
			if (value & 0x40000000) {
				half++;
			}
		}
	}

	_sntprintf(line, _countof(line),
		_T("render-page%d: sig=%08lX size=%dx%d nonzero=%lu transparent=%lu half=%lu first=%08lX,%08lX,%08lX,%08lX"),
		page, (unsigned long)hash, width, height, (unsigned long)nonzero,
		(unsigned long)transparent, (unsigned long)half,
		(unsigned long)ptr[0], (unsigned long)ptr[1],
		(unsigned long)ptr[2], (unsigned long)ptr[3]);
	SmokeLogLine(line);
}

static void SmokeLogRenderBuffer(LPCTSTR name, const DWORD *ptr, int stride, int width, int height)
{
	DWORD hash;
	DWORD nonzero;
	DWORD transparent;
	DWORD half;
	int y;
	int x;
	TCHAR line[256];

	if (!ptr || (stride <= 0) || (width <= 0) || (height <= 0)) {
		_sntprintf(line, _countof(line), _T("%s: missing"), name);
		SmokeLogLine(line);
		return;
	}
	if (width > stride) {
		width = stride;
	}

	hash = 2166136261u;
	nonzero = 0;
	transparent = 0;
	half = 0;
	for (y = 0; y < height; y++) {
		const DWORD *row = ptr + (y * stride);
		for (x = 0; x < width; x++) {
			DWORD value = row[x];
			hash = SmokeHashDword(hash, value);
			if ((value & 0x00ffffff) != 0) {
				nonzero++;
			}
			if (value & 0x80000000) {
				transparent++;
			}
			if (value & 0x40000000) {
				half++;
			}
		}
	}

	_sntprintf(line, _countof(line),
		_T("%s: sig=%08lX size=%dx%d stride=%d nonzero=%lu transparent=%lu half=%lu first=%08lX,%08lX,%08lX,%08lX"),
		name, (unsigned long)hash, width, height, stride,
		(unsigned long)nonzero, (unsigned long)transparent, (unsigned long)half,
		(unsigned long)ptr[0], (unsigned long)ptr[1],
		(unsigned long)ptr[2], (unsigned long)ptr[3]);
	SmokeLogLine(line);
}

static void SmokeLogRenderState()
{
	Render *render;
	const Render::render_t *r;
	const CRTC *crtc;
	const CRTC::crtc_t *c;
	const VC *vc;
	const VC::vc_t *v;
	const Sprite *sprite;
	Sprite::sprite_t spr;
	int page;

	render = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	if (!render) {
		SmokeLogLine(_T("render-state: missing render"));
		return;
	}

	r = render->GetWorkAddr();
	crtc = render->GetCRTCDevice();
	c = crtc ? crtc->GetWorkAddr() : NULL;
	vc = render->GetVCDevice();
	v = vc ? vc->GetWorkAddr() : NULL;
	sprite = render->GetSpriteDevice();

	if (r) {
		TCHAR line[512];
		_sntprintf(line, _countof(line),
			_T("render-state: size=%dx%d hmul=%d vmul=%d hdisp=%d vdisp=%d hres=%d hd=%d vd=%d low=%d siz=%d mix=%dx%d page=%d type=%d len=%d mode=%d text=%d bgsp=%d/%d grptype=%d grppen=%d,%d,%d,%d grpx=%lu,%lu,%lu,%lu grpy=%lu,%lu,%lu,%lu"),
			r->width, r->height, r->h_mul, r->v_mul,
			r->h_disp, r->v_disp, r->hres, r->hd, r->vd,
			r->lowres ? 1 : 0, r->siz ? 1 : 0,
			r->mixwidth, r->mixheight, r->mixpage, r->mixtype,
			r->mixlen, r->mixmode, r->texten ? 1 : 0,
			r->bgspflag ? 1 : 0, r->bgspdisp ? 1 : 0, r->grptype,
			r->grppen[0] ? 1 : 0, r->grppen[1] ? 1 : 0,
			r->grppen[2] ? 1 : 0, r->grppen[3] ? 1 : 0,
			(unsigned long)r->grpx[0], (unsigned long)r->grpx[1],
			(unsigned long)r->grpx[2], (unsigned long)r->grpx[3],
			(unsigned long)r->grpy[0], (unsigned long)r->grpy[1],
			(unsigned long)r->grpy[2], (unsigned long)r->grpy[3]);
		SmokeLogLine(line);

		_sntprintf(line, _countof(line),
			_T("render-mix-scroll: x=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu y=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu"),
			r->mixx[0] ? (unsigned long)*r->mixx[0] : 0,
			r->mixx[1] ? (unsigned long)*r->mixx[1] : 0,
			r->mixx[2] ? (unsigned long)*r->mixx[2] : 0,
			r->mixx[3] ? (unsigned long)*r->mixx[3] : 0,
			r->mixx[4] ? (unsigned long)*r->mixx[4] : 0,
			r->mixx[5] ? (unsigned long)*r->mixx[5] : 0,
			r->mixx[6] ? (unsigned long)*r->mixx[6] : 0,
			r->mixx[7] ? (unsigned long)*r->mixx[7] : 0,
			r->mixy[0] ? (unsigned long)*r->mixy[0] : 0,
			r->mixy[1] ? (unsigned long)*r->mixy[1] : 0,
			r->mixy[2] ? (unsigned long)*r->mixy[2] : 0,
			r->mixy[3] ? (unsigned long)*r->mixy[3] : 0,
			r->mixy[4] ? (unsigned long)*r->mixy[4] : 0,
			r->mixy[5] ? (unsigned long)*r->mixy[5] : 0,
			r->mixy[6] ? (unsigned long)*r->mixy[6] : 0,
			r->mixy[7] ? (unsigned long)*r->mixy[7] : 0);
		SmokeLogLine(line);

		_sntprintf(line, _countof(line),
			_T("render-mix-shift: shift=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu r=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu l=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu"),
			(unsigned long)r->mixshift[0], (unsigned long)r->mixshift[1],
			(unsigned long)r->mixshift[2], (unsigned long)r->mixshift[3],
			(unsigned long)r->mixshift[4], (unsigned long)r->mixshift[5],
			(unsigned long)r->mixshift[6], (unsigned long)r->mixshift[7],
			(unsigned long)r->mixrshift[0], (unsigned long)r->mixrshift[1],
			(unsigned long)r->mixrshift[2], (unsigned long)r->mixrshift[3],
			(unsigned long)r->mixrshift[4], (unsigned long)r->mixrshift[5],
			(unsigned long)r->mixrshift[6], (unsigned long)r->mixrshift[7],
			(unsigned long)r->mixlshift[0], (unsigned long)r->mixlshift[1],
			(unsigned long)r->mixlshift[2], (unsigned long)r->mixlshift[3],
			(unsigned long)r->mixlshift[4], (unsigned long)r->mixlshift[5],
			(unsigned long)r->mixlshift[6], (unsigned long)r->mixlshift[7]);
		SmokeLogLine(line);

		_sntprintf(line, _countof(line),
			_T("render-mix-mask: andx=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu andy=%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu"),
			(unsigned long)r->mixandx[0], (unsigned long)r->mixandx[1],
			(unsigned long)r->mixandx[2], (unsigned long)r->mixandx[3],
			(unsigned long)r->mixandx[4], (unsigned long)r->mixandx[5],
			(unsigned long)r->mixandx[6], (unsigned long)r->mixandx[7],
			(unsigned long)r->mixandy[0], (unsigned long)r->mixandy[1],
			(unsigned long)r->mixandy[2], (unsigned long)r->mixandy[3],
			(unsigned long)r->mixandy[4], (unsigned long)r->mixandy[5],
			(unsigned long)r->mixandy[6], (unsigned long)r->mixandy[7]);
		SmokeLogLine(line);

		_sntprintf(line, _countof(line),
			_T("render-pal-flags: p0=%08lX p1=%08lX gb0=%08lX gb1=%08lX gs0=%08lX gs1=%08lX pb0=%08lX pb1=%08lX ps0=%08lX ps1=%08lX tx0=%08lX"),
			(unsigned long)r->paldata[0], (unsigned long)r->paldata[1],
			(unsigned long)r->paldataGB[0], (unsigned long)r->paldataGB[1],
			(unsigned long)r->paldataGS[0], (unsigned long)r->paldataGS[1],
			(unsigned long)r->paldataPB[0], (unsigned long)r->paldataPB[1],
			(unsigned long)r->paldataPS[0], (unsigned long)r->paldataPS[1],
			(unsigned long)r->paldata[0x100]);
		SmokeLogLine(line);

		for (page = 0; page < 4; page++) {
			if (r->grppen[page]) {
				SmokeLogRenderPage(r, page);
			}
		}
		SmokeLogRenderBuffer(_T("render-textout"), r->textout, 1024, 512, 512);
		SmokeLogRenderBuffer(_T("render-bgspbuf"), r->bgspbuf, 1024, 512, 512);
		SmokeLogRenderBuffer(_T("render-mixbuf"), r->mixbuf, r->mixwidth, r->mixlen, r->height);
	}
	if (v) {
		TCHAR line[384];
		_sntprintf(line, _countof(line),
			_T("vc-state: vr1=%02lX/%02lX vr2=%02lX/%02lX pri=%lu,%lu,%lu gp=%lu,%lu,%lu,%lu gs=%d,%d,%d,%d flags siz=%d exon=%d hp=%d bp=%d gg=%d gt=%d son=%d ton=%d gon=%d"),
			(unsigned long)v->vr1h, (unsigned long)v->vr1l,
			(unsigned long)v->vr2h, (unsigned long)v->vr2l,
			(unsigned long)v->sp, (unsigned long)v->tx, (unsigned long)v->gr,
			(unsigned long)v->gp[0], (unsigned long)v->gp[1],
			(unsigned long)v->gp[2], (unsigned long)v->gp[3],
			v->gs[0] ? 1 : 0, v->gs[1] ? 1 : 0,
			v->gs[2] ? 1 : 0, v->gs[3] ? 1 : 0,
			v->siz ? 1 : 0, v->exon ? 1 : 0, v->hp ? 1 : 0,
			v->bp ? 1 : 0, v->gg ? 1 : 0, v->gt ? 1 : 0,
			v->son ? 1 : 0, v->ton ? 1 : 0, v->gon ? 1 : 0);
		SmokeLogLine(line);
	}
	if (c) {
		TCHAR line[384];
		_sntprintf(line, _countof(line),
			_T("crtc-state: reg28=%02X reg29=%02X text=%lu,%lu grp=%lu,%lu/%lu,%lu/%lu,%lu/%lu,%lu vscan=%d vdots=%d vcount=%lu vdisp=%d vblank=%d raster=%d"),
			c->reg[0x28], c->reg[0x29],
			(unsigned long)c->text_scrlx, (unsigned long)c->text_scrly,
			(unsigned long)c->grp_scrlx[0], (unsigned long)c->grp_scrly[0],
			(unsigned long)c->grp_scrlx[1], (unsigned long)c->grp_scrly[1],
			(unsigned long)c->grp_scrlx[2], (unsigned long)c->grp_scrly[2],
			(unsigned long)c->grp_scrlx[3], (unsigned long)c->grp_scrly[3],
			c->v_scan, c->v_dots, (unsigned long)c->v_count,
			c->v_disp ? 1 : 0, c->v_blank ? 1 : 0, c->raster_count);
		SmokeLogLine(line);
	}
	if (sprite) {
		TCHAR line[384];
		sprite->GetSprite(&spr);
		_sntprintf(line, _countof(line),
			_T("sprite-state: connect=%d disp=%d bg_on=%d,%d bg_area=%lu,%lu bg_scroll=%lu,%lu/%lu,%lu bg_size=%d hres=%lu vres=%lu low=%d hdisp=%lu vdisp=%lu"),
			spr.connect ? 1 : 0, spr.disp ? 1 : 0,
			spr.bg_on[0] ? 1 : 0, spr.bg_on[1] ? 1 : 0,
			(unsigned long)spr.bg_area[0], (unsigned long)spr.bg_area[1],
			(unsigned long)spr.bg_scrlx[0], (unsigned long)spr.bg_scrly[0],
			(unsigned long)spr.bg_scrlx[1], (unsigned long)spr.bg_scrly[1],
			spr.bg_size ? 1 : 0,
			(unsigned long)spr.h_res, (unsigned long)spr.v_res,
			spr.lowres ? 1 : 0,
			(unsigned long)spr.h_disp, (unsigned long)spr.v_disp);
		SmokeLogLine(line);
	}
}

static BOOL SmokeValidatePx68kVideoFrame()
{
	Render *render;
	const WORD *pixels;
	int width;
	int height;
	int stride;
	DWORD nonzero;
	DWORD signature;
	WORD min_value;
	WORD max_value;
	WORD first_values[8];
	int first_count;
	int x;
	int y;
	BOOL haveScreen;

	render = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	if (!render) {
		SmokeLogLine(_T("px68k-video: missing render device"));
		return FALSE;
	}
	if (!render->IsRenderFastDummyEnabled()) {
		SmokeLogLine(_T("px68k-video: backend not active"));
		return FALSE;
	}
	pixels = NULL;
	width = height = stride = 0;
	haveScreen = render->GetPx68kScreen(&pixels, &width, &height, &stride) &&
		pixels && (width > 0) && (height > 0) && (stride >= width);
	if (!haveScreen) {
		SmokeLogLine(_T("px68k-video: no screen buffer"));
		return FALSE;
	}
	nonzero = 0;
	signature = 2166136261u;
	min_value = 0xffff;
	max_value = 0x0000;
	first_count = 0;
	for (y = 0; y < height; y++) {
		const WORD *row = pixels + (y * stride);
		for (x = 0; x < width; x++) {
			WORD value = row[x];
			if (first_count < 8) {
				first_values[first_count++] = value;
			}
			if (value < min_value) {
				min_value = value;
			}
			if (value > max_value) {
				max_value = value;
			}
			if (value != 0) {
				nonzero++;
			}
			signature ^= (DWORD)value;
			signature *= 16777619u;
		}
	}
	if (nonzero == 0) {
		if (!render->EnsurePx68kFrame()) {
			SmokeLogLine(_T("px68k-video: EnsurePx68kFrame failed"));
			return FALSE;
		}
		pixels = NULL;
		width = height = stride = 0;
		if (!render->GetPx68kScreen(&pixels, &width, &height, &stride) ||
			!pixels || (width <= 0) || (height <= 0) || (stride < width)) {
			SmokeLogLine(_T("px68k-video: no screen buffer after redraw"));
			return FALSE;
		}
		nonzero = 0;
		signature = 2166136261u;
		min_value = 0xffff;
		max_value = 0x0000;
		first_count = 0;
		for (y = 0; y < height; y++) {
			const WORD *row = pixels + (y * stride);
			for (x = 0; x < width; x++) {
				WORD value = row[x];
				if (first_count < 8) {
					first_values[first_count++] = value;
				}
				if (value < min_value) {
					min_value = value;
				}
				if (value > max_value) {
					max_value = value;
				}
				if (value != 0) {
					nonzero++;
				}
				signature ^= (DWORD)value;
				signature *= 16777619u;
			}
		}
	}

	SmokeLogFormatDword(_T("px68k-video-width=%lu"), (DWORD)width);
	SmokeLogFormatDword(_T("px68k-video-height=%lu"), (DWORD)height);
	SmokeLogFormatDword(_T("px68k-video-nonzero=%lu"), nonzero);
	SmokeLogFormatDword(_T("px68k-video-signature=%lu"), signature);
	{
		TCHAR line[256];
		_sntprintf(line, _countof(line),
			_T("px68k-video-sample: min=%04X max=%04X first=%04X,%04X,%04X,%04X,%04X,%04X,%04X,%04X"),
			min_value, max_value,
			first_values[0], first_values[1], first_values[2], first_values[3],
			first_values[4], first_values[5], first_values[6], first_values[7]);
		SmokeLogLine(line);
	}
	if (nonzero == 0) {
		SmokeLogLine(_T("px68k-video: blank framebuffer"));
		return FALSE;
	}
	SmokeLogLine(_T("px68k-video: ok"));
	return TRUE;
}

static LPCTSTR SmokeFindOption(LPCTSTR cmd, LPCTSTR opt, LPCTSTR after)
{
	LPCTSTR p = after ? after : cmd;

	return _tcsstr(p, opt);
}

static BOOL SmokeReadOptionValue(LPCTSTR cmd, LPCTSTR opt, LPCTSTR *after, TCHAR *value, int valueCount)
{
	LPCTSTR p;
	LPCTSTR q;
	int len;

	ASSERT(value);
	ASSERT(valueCount > 0);

	p = SmokeFindOption(cmd, opt, after ? *after : NULL);
	if (!p) {
		return FALSE;
	}
	p += _tcslen(opt);
	while (*p && (*p <= _T(' '))) {
		p++;
	}
	if (*p == _T('\"')) {
		p++;
		q = _tcschr(p, _T('\"'));
	}
	else {
		q = p;
		while (*q && (*q > _T(' '))) {
			q++;
		}
	}
	if (!q || q <= p) {
		value[0] = _T('\0');
		if (after) {
			*after = p;
		}
		return FALSE;
	}

	len = (int)(q - p);
	if (len >= valueCount) {
		len = valueCount - 1;
	}
	_tcsncpy(value, p, len);
	value[len] = _T('\0');
	if (after) {
		*after = q;
	}
	return TRUE;
}

static DWORD SmokeReadDwordOption(LPCTSTR cmd, LPCTSTR opt, DWORD def)
{
	TCHAR value[32];

	if (!SmokeReadOptionValue(cmd, opt, NULL, value, _countof(value))) {
		return def;
	}
	return (DWORD)_tcstoul(value, NULL, 10);
}

static void SmokePumpMessages()
{
	MSG msg;

	while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}
}

static DWORD SmokeKeyCode(LPCTSTR name)
{
	if (!_tcsicmp(name, _T("A"))) return 0x1e;
	if (!_tcsicmp(name, _T("B"))) return 0x2e;
	if (!_tcsicmp(name, _T("Z"))) return 0x2a;
	if (!_tcsicmp(name, _T("X"))) return 0x2b;
	if (!_tcsicmp(name, _T("SPACE"))) return 0x35;
	if (!_tcsicmp(name, _T("ENTER")) || !_tcsicmp(name, _T("RETURN"))) return 0x1d;
	if (!_tcsicmp(name, _T("OPT1")) || !_tcsicmp(name, _T("OPT.1"))) return 0x72;
	if (!_tcsicmp(name, _T("OPT2")) || !_tcsicmp(name, _T("OPT.2"))) return 0x73;
	if (!_tcsicmp(name, _T("F1"))) return 0x63;
	if (!_tcsicmp(name, _T("F2"))) return 0x64;
	if (!_tcsicmp(name, _T("F3"))) return 0x65;
	if (!_tcsicmp(name, _T("ESC"))) return 0x01;
	if (!_tcsicmp(name, _T("UP"))) return 0x3c;
	if (!_tcsicmp(name, _T("DOWN"))) return 0x3e;
	if (!_tcsicmp(name, _T("LEFT"))) return 0x3b;
	if (!_tcsicmp(name, _T("RIGHT"))) return 0x3d;
	if (!_tcsnicmp(name, _T("0x"), 2)) return (DWORD)_tcstoul(name + 2, NULL, 16);
	return 0;
}

static BOOL SmokeSplit3(LPCTSTR spec, TCHAR *a, int aCount, DWORD *b, DWORD *c)
{
	LPCTSTR p1;
	LPCTSTR p2;
	int len;

	p1 = _tcschr(spec, _T(':'));
	if (!p1) {
		return FALSE;
	}
	p2 = _tcschr(p1 + 1, _T(':'));
	if (!p2) {
		return FALSE;
	}

	len = (int)(p1 - spec);
	if (len <= 0) {
		return FALSE;
	}
	if (len >= aCount) {
		len = aCount - 1;
	}
	_tcsncpy(a, spec, len);
	a[len] = _T('\0');

	*b = (DWORD)_tcstoul(p1 + 1, NULL, 10);
	*c = (DWORD)_tcstoul(p2 + 1, NULL, 10);
	return TRUE;
}

static BOOL SmokeParseKeyAction(LPCTSTR spec, smoke_action_t *action)
{
	TCHAR name[32];
	DWORD start;
	DWORD duration;
	DWORD code;

	if (!SmokeSplit3(spec, name, _countof(name), &start, &duration)) {
		return FALSE;
	}
	code = SmokeKeyCode(name);
	if (!code || !duration) {
		return FALSE;
	}

	memset(action, 0, sizeof(*action));
	action->type = SmokeActionKey;
	action->key = code;
	action->start = start;
	action->end = start + duration;
	_tcsncpy(action->name, name, _countof(action->name) - 1);
	return TRUE;
}

static int SmokeJoyButton(LPCTSTR name)
{
	if (!_tcsicmp(name, _T("A"))) return 0;
	if (!_tcsicmp(name, _T("B"))) return 1;
	return -1;
}

static BOOL SmokeJoyAxis(LPCTSTR name, int *axis, DWORD *value)
{
	if (!_tcsicmp(name, _T("UP"))) { *axis = 1; *value = (DWORD)-2048; return TRUE; }
	if (!_tcsicmp(name, _T("DOWN"))) { *axis = 1; *value = (DWORD)2047; return TRUE; }
	if (!_tcsicmp(name, _T("LEFT"))) { *axis = 0; *value = (DWORD)-2048; return TRUE; }
	if (!_tcsicmp(name, _T("RIGHT"))) { *axis = 0; *value = (DWORD)2047; return TRUE; }
	return FALSE;
}

static BOOL SmokeParseJoyAction(LPCTSTR spec, smoke_action_t *action)
{
	TCHAR name[32];
	DWORD start;
	DWORD duration;
	int port;
	int button;

	if (!SmokeSplit3(spec, name, _countof(name), &start, &duration)) {
		return FALSE;
	}
	port = 0;
	if ((name[0] >= _T('0')) && (name[0] <= _T('1')) && (name[1] == _T(':'))) {
		port = name[0] - _T('0');
		memmove(name, name + 2, (_tcslen(name + 2) + 1) * sizeof(TCHAR));
	}
	button = SmokeJoyButton(name);
	if (!duration) {
		return FALSE;
	}

	memset(action, 0, sizeof(*action));
	if (button >= 0) {
		action->type = SmokeActionJoy;
		action->button = button;
	}
	else if (SmokeJoyAxis(name, &action->axis, &action->axis_value)) {
		action->type = SmokeActionJoyAxis;
	}
	else {
		return FALSE;
	}
	action->port = port;
	action->start = start;
	action->end = start + duration;
	_tcsncpy(action->name, name, _countof(action->name) - 1);
	return TRUE;
}

static int SmokeParseActions(LPCTSTR cmd, smoke_action_t *actions, int maxActions)
{
	LPCTSTR after;
	TCHAR value[96];
	int count;

	count = 0;
	after = NULL;
	while ((count < maxActions) &&
		SmokeReadOptionValue(cmd, _T("--smoke-key-hold"), &after, value, _countof(value))) {
		if (SmokeParseKeyAction(value, &actions[count])) {
			count++;
		}
	}

	after = NULL;
	while ((count < maxActions) &&
		SmokeReadOptionValue(cmd, _T("--smoke-joy-hold"), &after, value, _countof(value))) {
		if (SmokeParseJoyAction(value, &actions[count])) {
			count++;
		}
	}

	return count;
}

static void SmokeEnsureJoyPort(PPI *ppi, int port)
{
	PPI::ppi_t state;

	if (!ppi || (port < 0) || (port >= PPI::PortMax)) {
		return;
	}

	ppi->GetPPI(&state);
	if (state.type[port] != 1) {
		ppi->SetJoyType(port, 1);
		SmokeLogFormatDword(_T("joy-port-atari=%lu"), (DWORD)port);
	}
}

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CFrmWnd::CFrmWnd()
{
	// VM pointer and status codes
	::pVM = NULL;
	m_nStatus = -1;

	// Devices
	m_pFDD = NULL;
	m_pSASI = NULL;
	m_pSCSI = NULL;
	m_pScheduler = NULL;
	m_pKeyboard = NULL;
	m_pMouse = NULL;

	// Components
	m_pFirstComponent = NULL;
	m_pDrawView = NULL;
	m_pStatusView = NULL;
	m_pSch = NULL;
	m_pSound = NULL;
	m_pInput = NULL;
	m_pPort = NULL;
	m_pMIDI = NULL;
	m_pTKey = NULL;
	m_pHost = NULL;
	m_pInfo = NULL;
	m_pConfig = NULL;

	// Full-screen state
	m_bFullScreen = FALSE;
	m_bBorderless = FALSE;
	m_dwPrevStyle = 0;
	m_dwPrevExStyle = 0;
	memset(&m_wpPrev, 0, sizeof(m_wpPrev));
	m_hTaskBar = NULL;
	memset(&m_DevMode, 0, sizeof(m_DevMode));
	m_nWndLeft = 0;
	m_nWndTop = 0;
	m_bVSyncEnabled = TRUE;

	// Child window
	m_strWndClsName.Empty();

	// Status bar, menu bar, and caption
	m_bStatusBar = FALSE;
	m_bMenuBar = TRUE;
	m_bCaption = TRUE;

	// Shell notifications
	m_uNotifyId = NULL;

	// Settings
	m_bMouseMid = TRUE;
	m_bPopup = FALSE;
	m_bAutoMouse = TRUE;

	// Other state
	m_bExit = FALSE;
	m_bSaved = FALSE;
	m_nFDDStatus[0] = 0;
	m_nFDDStatus[1] = 0;
	m_dwExec = 0;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CFrmWnd, CFrameWnd)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_DESTROY()
	ON_MESSAGE(WM_DISPLAYCHANGE, OnDisplayChange)
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_MOVE()
	ON_WM_ACTIVATE()
	ON_WM_ACTIVATEAPP()
	ON_WM_ENTERMENULOOP()
	ON_WM_EXITMENULOOP()
	ON_WM_PARENTNOTIFY()
	ON_MESSAGE(WM_KICK, OnKick)
	ON_WM_DRAWITEM()
	ON_WM_CONTEXTMENU()
	ON_MESSAGE(WM_POWERBROADCAST, OnPowerBroadCast)
	ON_WM_SYSCOMMAND()
#if _MFC_VER >= 0x700
	ON_WM_COPYDATA()
#else
	ON_MESSAGE(WM_COPYDATA, OnCopyData)
#endif
	ON_WM_ENDSESSION()
	ON_MESSAGE(WM_SHELLNOTIFY, OnShellNotify)

	ON_COMMAND(ID_FILE_CARGAR40006, OnFastOpen)

	ON_COMMAND(IDM_OPEN, OnOpen)
	ON_UPDATE_COMMAND_UI(IDM_OPEN, OnOpenUI)
	ON_COMMAND(IDM_SAVE, OnSave)
	ON_UPDATE_COMMAND_UI(IDM_SAVE, OnSaveUI)
	ON_COMMAND(IDM_SAVEAS, OnSaveAs)
	ON_UPDATE_COMMAND_UI(IDM_SAVEAS, OnSaveAsUI)
	ON_COMMAND(IDM_RESET, OnReset)
	ON_UPDATE_COMMAND_UI(IDM_RESET, OnResetUI)
	ON_COMMAND(IDM_SAVECUSTOMCONFIG, OnScc)
	ON_UPDATE_COMMAND_UI(IDM_SAVECUSTOMCONFIG, OnSccUI)
	ON_COMMAND(IDM_SAVEGLOBALCONFIG, OnSgc)
	ON_UPDATE_COMMAND_UI(IDM_SAVEGLOBALCONFIG, OnSgcUI)
	ON_COMMAND(IDM_SAVEGLOBALCONFIGANDRESET, OnSgcr)
	ON_UPDATE_COMMAND_UI(IDM_SAVEGLOBALCONFIGANDRESET, OnSgcrUI)
	ON_COMMAND(IDM_INTERRUPT, OnDump)
	ON_UPDATE_COMMAND_UI(IDM_INTERRUPT, OnDumpUI)
	ON_COMMAND(IDM_POWER, OnPower)
	ON_UPDATE_COMMAND_UI(IDM_POWER, OnPowerUI)
	ON_COMMAND_RANGE(IDM_XM6_MRU0, IDM_XM6_MRU8, OnMRU)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_XM6_MRU0, IDM_XM6_MRU8, OnMRUUI)
	ON_COMMAND(IDM_EXIT, OnExit)

	ON_COMMAND_RANGE(IDM_D0OPEN, IDM_D1_MRU8, OnFD)
	ON_UPDATE_COMMAND_UI(IDM_D0OPEN, OnFDOpenUI)
	ON_UPDATE_COMMAND_UI(IDM_D1OPEN, OnFDOpenUI)
	ON_UPDATE_COMMAND_UI(IDM_D0EJECT, OnFDEjectUI)
	ON_UPDATE_COMMAND_UI(IDM_D1EJECT, OnFDEjectUI)
	ON_UPDATE_COMMAND_UI(IDM_D0WRITEP, OnFDWritePUI)
	ON_UPDATE_COMMAND_UI(IDM_D1WRITEP, OnFDWritePUI)
	ON_UPDATE_COMMAND_UI(IDM_D0FORCE, OnFDForceUI)
	ON_UPDATE_COMMAND_UI(IDM_D1FORCE, OnFDForceUI)
	ON_UPDATE_COMMAND_UI(IDM_D0INVALID, OnFDInvalidUI)
	ON_UPDATE_COMMAND_UI(IDM_D1INVALID, OnFDInvalidUI)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_D0_MEDIA0, IDM_D0_MEDIAF, OnFDMediaUI)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_D1_MEDIA0, IDM_D1_MEDIAF, OnFDMediaUI)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_D0_MRU0, IDM_D0_MRU8, OnFDMRUUI)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_D1_MRU0, IDM_D1_MRU8, OnFDMRUUI)

	ON_COMMAND(IDM_MOOPEN, OnMOOpen)
	ON_UPDATE_COMMAND_UI(IDM_MOOPEN, OnMOOpenUI)
	ON_COMMAND(IDM_MOEJECT, OnMOEject)
	ON_UPDATE_COMMAND_UI(IDM_MOEJECT, OnMOEjectUI)
	ON_COMMAND(IDM_MOWRITEP, OnMOWriteP)
	ON_UPDATE_COMMAND_UI(IDM_MOWRITEP, OnMOWritePUI)
	ON_COMMAND(IDM_MOFORCE, OnMOForce)
	ON_UPDATE_COMMAND_UI(IDM_MOFORCE, OnMOForceUI)
	ON_COMMAND_RANGE(IDM_MO_MRU0, IDM_MO_MRU8, OnMOMRU)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_MO_MRU0, IDM_MO_MRU8, OnMOMRUUI)

	ON_COMMAND(IDM_CDOPEN, OnCDOpen)
	ON_UPDATE_COMMAND_UI(IDM_CDOPEN, OnCDOpenUI)
	ON_COMMAND(IDM_CDEJECT, OnCDEject)
	ON_UPDATE_COMMAND_UI(IDM_CDEJECT, OnCDEjectUI)
	ON_COMMAND(IDM_CDFORCE, OnCDForce)
	ON_UPDATE_COMMAND_UI(IDM_CDFORCE, OnCDForceUI)
	ON_COMMAND_RANGE(IDM_CD_MRU0, IDM_CD_MRU8, OnCDMRU)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_CD_MRU0, IDM_CD_MRU8, OnCDMRUUI)

	ON_COMMAND(IDM_LOG, OnLog)
	ON_UPDATE_COMMAND_UI(IDM_LOG, OnLogUI)
	ON_COMMAND(IDM_SCHEDULER, OnScheduler)
	ON_UPDATE_COMMAND_UI(IDM_SCHEDULER, OnSchedulerUI)
	ON_COMMAND(IDM_DEVICE, OnDevice)
	ON_UPDATE_COMMAND_UI(IDM_DEVICE, OnDeviceUI)
	ON_COMMAND(IDM_CPUREG, OnCPUReg)
	ON_UPDATE_COMMAND_UI(IDM_CPUREG, OnCPURegUI)
	ON_COMMAND(IDM_INT, OnInt)
	ON_UPDATE_COMMAND_UI(IDM_INT, OnIntUI)
	ON_COMMAND(IDM_DISASM, OnDisasm)
	ON_UPDATE_COMMAND_UI(IDM_DISASM, OnDisasmUI)
	ON_COMMAND(IDM_MEMORY, OnMemory)
	ON_UPDATE_COMMAND_UI(IDM_MEMORY, OnMemoryUI)
	ON_COMMAND(IDM_BREAKP, OnBreakP)
	ON_UPDATE_COMMAND_UI(IDM_BREAKP, OnBreakPUI)
	ON_COMMAND(IDM_MFP, OnMFP)
	ON_UPDATE_COMMAND_UI(IDM_MFP, OnMFPUI)
	ON_COMMAND(IDM_DMAC, OnDMAC)
	ON_UPDATE_COMMAND_UI(IDM_DMAC, OnDMACUI)
	ON_COMMAND(IDM_CRTC, OnCRTC)
	ON_UPDATE_COMMAND_UI(IDM_CRTC, OnCRTCUI)
	ON_COMMAND(IDM_VC, OnVC)
	ON_UPDATE_COMMAND_UI(IDM_VC, OnVCUI)
	ON_COMMAND(IDM_RTC, OnRTC)
	ON_UPDATE_COMMAND_UI(IDM_RTC, OnRTCUI)
	ON_COMMAND(IDM_OPM, OnOPM)
	ON_UPDATE_COMMAND_UI(IDM_OPM, OnOPMUI)
	ON_COMMAND(IDM_KEYBOARD, OnKeyboard)
	ON_UPDATE_COMMAND_UI(IDM_KEYBOARD, OnKeyboardUI)
	ON_COMMAND(IDM_FDD, OnFDD)
	ON_UPDATE_COMMAND_UI(IDM_FDD, OnFDDUI)
	ON_COMMAND(IDM_FDC, OnFDC)
	ON_UPDATE_COMMAND_UI(IDM_FDC, OnFDCUI)
	ON_COMMAND(IDM_SCC, OnSCC)
	ON_UPDATE_COMMAND_UI(IDM_SCC, OnSCCUI)
	ON_COMMAND(IDM_CYNTHIA, OnCynthia)
	ON_UPDATE_COMMAND_UI(IDM_CYNTHIA, OnCynthiaUI)
	ON_COMMAND(IDM_SASI, OnSASI)
	ON_UPDATE_COMMAND_UI(IDM_SASI, OnSASIUI)
	ON_COMMAND(IDM_MIDI, OnMIDI)
	ON_UPDATE_COMMAND_UI(IDM_MIDI, OnMIDIUI)
	ON_COMMAND(IDM_SCSI, OnSCSI)
	ON_UPDATE_COMMAND_UI(IDM_SCSI, OnSCSIUI)
//	ON_COMMAND(IDM_TVRAM, OnTVRAM)
//	ON_UPDATE_COMMAND_UI(IDM_TVRAM, OnTVRAMUI)
	ON_COMMAND(IDM_G1024, OnG1024)
	ON_UPDATE_COMMAND_UI(IDM_G1024, OnG1024UI)
	ON_COMMAND_RANGE(IDM_G16P0, IDM_G16P3, OnG16)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_G16P0, IDM_G16P3, OnG16UI)
	ON_COMMAND_RANGE(IDM_G256P0, IDM_G256P1, OnG256)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_G256P0, IDM_G256P1, OnG256UI)
	ON_COMMAND(IDM_G64K, OnG64K)
	ON_UPDATE_COMMAND_UI(IDM_G64K, OnG64KUI)
	ON_COMMAND(IDM_PCG, OnPCG)
	ON_UPDATE_COMMAND_UI(IDM_PCG, OnPCGUI)
	ON_COMMAND_RANGE(IDM_BG0, IDM_BG1, OnBG)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_BG0, IDM_BG1, OnBGUI)
	ON_COMMAND(IDM_PALET, OnPalet)
	ON_UPDATE_COMMAND_UI(IDM_PALET, OnPaletUI)
	ON_COMMAND(IDM_REND_TEXT, OnTextBuf)
	ON_UPDATE_COMMAND_UI(IDM_REND_TEXT, OnTextBufUI)
	ON_COMMAND_RANGE(IDM_REND_GP0, IDM_REND_GP3, OnGrpBuf)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_REND_GP0, IDM_REND_GP3, OnGrpBufUI)
	ON_COMMAND(IDM_REND_PCG, OnPCGBuf)
	ON_UPDATE_COMMAND_UI(IDM_REND_PCG, OnPCGBufUI)
	ON_COMMAND(IDM_REND_BGSPRITE, OnBGSpBuf)
	ON_UPDATE_COMMAND_UI(IDM_REND_BGSPRITE, OnBGSpBufUI)
	ON_COMMAND(IDM_REND_PALET, OnPaletBuf)
	ON_UPDATE_COMMAND_UI(IDM_REND_PALET, OnPaletBufUI)
	ON_COMMAND(IDM_REND_MIX, OnMixBuf)
	ON_UPDATE_COMMAND_UI(IDM_REND_MIX, OnMixBufUI)
	ON_COMMAND(IDM_COMPONENT, OnComponent)
	ON_UPDATE_COMMAND_UI(IDM_COMPONENT, OnComponentUI)
	ON_COMMAND(IDM_OSINFO, OnOSInfo)
	ON_UPDATE_COMMAND_UI(IDM_OSINFO, OnOSInfoUI)
	ON_COMMAND(IDM_SOUND, OnSound)
	ON_UPDATE_COMMAND_UI(IDM_SOUND, OnSoundUI)
	ON_COMMAND(IDM_INPUT, OnInput)
	ON_UPDATE_COMMAND_UI(IDM_INPUT, OnInputUI)
	ON_COMMAND(IDM_PORT, OnPort)
	ON_UPDATE_COMMAND_UI(IDM_PORT, OnPortUI)
	ON_COMMAND(IDM_BITMAP, OnBitmap)
	ON_UPDATE_COMMAND_UI(IDM_BITMAP, OnBitmapUI)
	ON_COMMAND(IDM_MIDIDRV, OnMIDIDrv)
	ON_UPDATE_COMMAND_UI(IDM_MIDIDRV, OnMIDIDrvUI)
	ON_COMMAND(IDM_CAPTION, OnCaption)
	ON_UPDATE_COMMAND_UI(IDM_CAPTION, OnCaptionUI)
	ON_COMMAND(IDM_MENU, OnMenu)
	ON_UPDATE_COMMAND_UI(IDM_MENU, OnMenuUI)
	ON_COMMAND(IDM_STATUS, OnStatus)
	ON_UPDATE_COMMAND_UI(IDM_STATUS, OnStatusUI)
	ON_COMMAND_RANGE(IDM_SCALE_100, IDM_SCALE_300, OnWindowScale)
	ON_UPDATE_COMMAND_UI_RANGE(IDM_SCALE_100, IDM_SCALE_300, OnWindowScaleUI)
	ON_COMMAND(IDM_REFRESH, OnRefresh)
	ON_COMMAND(IDM_FULLSCREEN, OnFullScreen)
	ON_UPDATE_COMMAND_UI(IDM_FULLSCREEN, OnFullScreenUI)
	ON_COMMAND(IDM_YMFM, OnYmfm)
	ON_UPDATE_COMMAND_UI(IDM_YMFM, OnYmfmUI)
	ON_COMMAND(IDM_TOGGLE_RENDERER, OnToggleRenderer)
	ON_COMMAND(IDM_TOGGLE_RENDER_FAST_DUMMY, OnToggleRenderFastDummy)
	ON_UPDATE_COMMAND_UI(IDM_TOGGLE_RENDER_FAST_DUMMY, OnToggleRenderFastDummyUI)
	ON_COMMAND(IDM_TOGGLE_OSD, OnToggleOSD)
	ON_COMMAND(IDM_TOGGLE_VSYNC, OnToggleVSync)
	ON_COMMAND(IDM_TOGGLE_SHADER, OnToggleShader)
	ON_UPDATE_COMMAND_UI(IDM_TOGGLE_SHADER, OnToggleShaderUI)

	ON_COMMAND(IDM_EXEC, OnExec)
	ON_UPDATE_COMMAND_UI(IDM_EXEC, OnExecUI)
	ON_COMMAND(IDM_BREAK, OnBreak)
	ON_UPDATE_COMMAND_UI(IDM_BREAK, OnBreakUI)
	ON_COMMAND(IDM_STEP_FRAME, OnStepFrame)
	ON_UPDATE_COMMAND_UI(IDM_STEP_FRAME, OnStepFrameUI)
	ON_COMMAND(IDM_TRACE, OnTrace)
	ON_UPDATE_COMMAND_UI(IDM_TRACE, OnTraceUI)

	ON_COMMAND(IDM_MOUSEMODE, OnMouseMode)
	ON_COMMAND(IDM_SOFTKEY, OnSoftKey)
	ON_UPDATE_COMMAND_UI(IDM_SOFTKEY, OnSoftKeyUI)
	ON_COMMAND(IDM_TIMEADJ, OnTimeAdj)
	ON_COMMAND(IDM_TRAP0, OnTrap)
	ON_UPDATE_COMMAND_UI(IDM_TRAP0, OnTrapUI)
	ON_COMMAND(IDM_SAVEWAV, OnSaveWav)
	ON_UPDATE_COMMAND_UI(IDM_SAVEWAV, OnSaveWavUI)
	ON_COMMAND(IDM_NEWFD, OnNewFD)
	ON_COMMAND_RANGE(IDM_NEWSASI, IDM_NEWMO, OnNewDisk)
	ON_COMMAND(IDM_OPTIONS, OnOptions)

	ON_COMMAND(IDM_CASCADE, OnCascade)
	ON_UPDATE_COMMAND_UI(IDM_CASCADE, OnCascadeUI)
	ON_COMMAND(IDM_TILE, OnTile)
	ON_UPDATE_COMMAND_UI(IDM_TILE, OnTileUI)
	ON_COMMAND(IDM_ICONIC, OnIconic)
	ON_UPDATE_COMMAND_UI(IDM_ICONIC, OnIconicUI)
	ON_COMMAND(IDM_ARRANGEICON, OnArrangeIcon)
	ON_UPDATE_COMMAND_UI(IDM_ARRANGEICON, OnArrangeIconUI)
	ON_COMMAND(IDM_HIDE, OnHide)
	ON_UPDATE_COMMAND_UI(IDM_HIDE, OnHideUI)
	ON_COMMAND(IDM_RESTORE, OnRestore)
	ON_UPDATE_COMMAND_UI(IDM_RESTORE, OnRestoreUI)
	ON_COMMAND_RANGE(IDM_SWND_START, IDM_SWND_END, OnWindow)

	ON_COMMAND(IDM_ABOUT, OnAbout)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::Init()
{
	// Create the main frame window

	if (!Create(NULL, _T("XM6"),
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
			WS_BORDER | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
			rectDefault, NULL, NULL, 0, NULL)) {
		return FALSE;
	}

	// Leave the rest of initialization to OnCreate.
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window creation preprocessing
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	// Base class processing
	if (!CFrameWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// Remove client-edge border
	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CFrmWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	LONG lUser;
	CMenu *pSysMenu;
	UINT nCount;
	CString string;

	// Base class processing
	if (CFrameWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Set custom user data marker
	lUser = (LONG)MAKEID('X', 'M', '6', ' ');
	::SetWindowLong(m_hWnd, GWL_USERDATA, lUser);

	// Accelerator, icon, and IME context
	LoadAccelTable(MAKEINTRESOURCE(IDR_ACCELERATOR));
	SetIcon(AfxGetApp()->LoadIcon(IDI_APPICON), TRUE);
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Window menus
	if (::IsJapanese()) {
		// Japanese menu resources
		m_Menu.LoadMenu(IDR_MENU);
		m_PopupMenu.LoadMenu(IDR_MENUPOPUP);
	}
	else {
		// English menu resources
		m_Menu.LoadMenu(IDR_US_MENU);
		m_PopupMenu.LoadMenu(IDR_US_MENUPOPUP);
	}
	SetMenu(&m_Menu);
	m_bMenuBar = TRUE;
	m_bPopupMenu = FALSE;

	// System menu
	::GetMsg(IDS_STDWIN, string);
	pSysMenu = GetSystemMenu(FALSE);
	ASSERT(pSysMenu);
	nCount = pSysMenu->GetMenuItemCount();

	// Insert "Standard window position"
	pSysMenu->InsertMenu(nCount - 2, MF_BYPOSITION | MF_STRING, IDM_STDWIN, string);
	pSysMenu->InsertMenu(nCount - 2, MF_BYPOSITION | MF_SEPARATOR);

	// Initialize child views
	if (!InitChild()) {
		return -1;
	}

	// Initialize window position and rectangle
	InitPos();

	// Initialize shell notifications
	InitShell();

	// Initialize virtual machine
	if (!InitVM()) {
		// VM initialization failed
		m_nStatus = 1;
		PostMessage(WM_KICK, 0, 0);
		return 0;
	}

	// Pass executable version info to the VM
	InitVer();

	// Cache device pointers
	m_pFDD = (FDD*)::GetVM()->SearchDevice(MAKEID('F', 'D', 'D', ' '));
	ASSERT(m_pFDD);
	m_pSASI = (SASI*)::GetVM()->SearchDevice(MAKEID('S', 'A', 'S', 'I'));
	ASSERT(m_pSASI);
	m_pSCSI = (SCSI*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(m_pSCSI);
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);
	m_pKeyboard = (Keyboard*)::GetVM()->SearchDevice(MAKEID('K', 'E', 'Y', 'B'));
	ASSERT(m_pKeyboard);
	m_pMouse = (Mouse*)::GetVM()->SearchDevice(MAKEID('M', 'O', 'U', 'S'));
	ASSERT(m_pMouse);

	// Create and initialize components
	if (!InitComponent()) {
		// Component initialization failed
		m_nStatus = 2;
		PostMessage(WM_KICK, 0, 0);
		return 0;
	}

	// Apply settings (like OnOption) while VM is locked
	::LockVM();
	ApplyCfg();
	::UnlockVM();

	// Reset VM
	::GetVM()->Reset();

	// Actualizar nombre de savestate de SASI/FDD configurados por MRU en el inicio
	// UpdateStateFileName se movio a OnKick para que los medios ya esten restaurados

	// Restore frame state while startup status is non-zero
	ASSERT(m_nStatus != 0);
	RestoreFrameWnd(FALSE);

	// Post kick message and continue startup
	m_nStatus = 0;
	PostMessage(WM_KICK, 0, 0);
	return 0;
}

//---------------------------------------------------------------------------
//
//	Child view initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::InitChild()
{
	HDC hDC;
	HFONT hFont;
	HFONT hDefFont;
	TEXTMETRIC tm;
	int i;
	int nWidth;
	UINT uIndicator[6];

	// Restore shader setting before initializing the view
	Config config;
	memset(&config, 0, sizeof(config));
	if (m_pConfig) {
		m_pConfig->GetConfig(&config);
	}
	else {
		// Early startup path: config component is not created yet.
		config.render_shader = FALSE;
		config.render_fast_dummy = FALSE;
	}

	// Create view with the initial shader state
	m_pDrawView = new CDrawView;
	if (!m_pDrawView->Init(this, config.render_shader)) {
		return FALSE;
	}

	// Initialize status bar contents
	ResetStatus();

	// Create status bar
	if (!m_StatusBar.Create(this, WS_CHILD | WS_VISIBLE | CBRS_BOTTOM,
							AFX_IDW_STATUS_BAR)) {
		return FALSE;
	}
	m_bStatusBar = TRUE;
	uIndicator[0] = ID_SEPARATOR;
	for (i=1; i<6; i++) {
		uIndicator[i] = (UINT)i;
	}
	m_StatusBar.SetIndicators(uIndicator, 6);

	// Get text metrics
	hDC = ::GetDC(m_hWnd);
	hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	hDefFont = (HFONT)::SelectObject(hDC, hFont);
	ASSERT(hDefFont);
	::GetTextMetrics(hDC, &tm);
	::SelectObject(hDC, hDefFont);
	::ReleaseDC(m_hWnd, hDC);

	// Pane width setup loop
	m_StatusBar.SetPaneInfo(0, 0, SBPS_NOBORDERS | SBPS_STRETCH, 0);
	nWidth = 0;
	for (i=1; i<6; i++) {
		switch (i) {
			// FD0, FD1
			case 1:
			case 2:
				nWidth = tm.tmAveCharWidth * 32;
				break;

			// HD BUSY
			case 3:
				nWidth = tm.tmAveCharWidth * 10;
				break;

			// TIMER
			case 4:
				nWidth = tm.tmAveCharWidth * 9;
				break;

			// POWER
			case 5:
				nWidth = tm.tmAveCharWidth * 9;
				break;
		}
		m_StatusBar.SetPaneInfo(i, i, SBPS_NORMAL | SBPS_OWNERDRAW, nWidth);
	}

	// Recalculate layout
	RecalcLayout();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Initialize position and window rectangle
//	If bStart is FALSE, restore position only in windowed mode.
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitPos(BOOL bStart)
{
	int cx;
	int cy;
	CRect rect;
	CRect rectStatus;
	CRect rectWnd;
	Config config;
	int scaleIndex;
	int scalePercent;

	ASSERT(this);

	// Get screen size and current window rectangle
	cx = ::GetSystemMetrics(SM_CXSCREEN);
	cy = ::GetSystemMetrics(SM_CYSCREEN);
	GetWindowRect(&rectWnd);
	memset(&config, 0, sizeof(config));
	if (m_pConfig) {
		m_pConfig->GetConfig(&config);
	}
	else {
		// Early startup path before component creation.
		config.window_scale = 0;
	}

	scaleIndex = config.window_scale;
	if (scaleIndex < 0) {
		scaleIndex = 0;
	}
	if (scaleIndex > 4) {
		scaleIndex = 4;
	}
	scalePercent = 100 + (scaleIndex * 50);

	// On 800x600 or smaller, force full-screen-sized window
	if ((cx <= 800) || (cy <= 600)) {
		if ((rectWnd.left != 0) || (rectWnd.top != 0)) {
			SetWindowPos(&wndTop, 0, 0, cx, cy, SWP_NOZORDER);
			return;
		}
		if ((rectWnd.Width() != cx) || (rectWnd.Height() != cy)) {
			SetWindowPos(&wndTop, 0, 0, cx, cy, SWP_NOZORDER);
			return;
		}
		return;
	}


	/* Set main-window size and fullscreen size here */
	// Match XM6p's 1.0x base draw area. 768x512 clips modes that need the
	// original CRT emulation margins (for example high-res HDF titles).
	rect.left = 0;
	rect.top = 0;

	/* In full-screen mode, use full monitor resolution */
	if (m_bFullScreen)
	{
		rect.right = cx;
		rect.bottom = cy;
	}
	else /* In windowed mode, 1024x768 is used */
	{
		rect.right = (824 * scalePercent) / 100;
		rect.bottom = (580 * scalePercent) / 100;
	}
	::AdjustWindowRectEx(&rect, GetView()->GetStyle(), FALSE, GetView()->GetExStyle());
	m_StatusBar.GetWindowRect(&rectStatus);
	rect.bottom += rectStatus.Height();
	::AdjustWindowRectEx(&rect, GetStyle(), TRUE, GetExStyle());

	// rect can become offset; normalize to width/height from origin
	rect.right -= rect.left;
	rect.left = 0;
	rect.bottom -= rect.top;
	rect.top = 0;

	// Center window when there is room
	if (rect.right < cx) {
		rect.left = (cx - rect.right) / 2;
	}
	if (rect.bottom < cy) {
		rect.top = (cy - rect.bottom) / 2;
	}

	// Branch by startup vs. runtime fullscreen/window transition
	if (bStart) {
		// Save initial window position once (RestoreFrameWnd may run later)
		m_nWndLeft = rect.left;
		m_nWndTop = rect.top;
	}
	else {
		// Keep corrected coordinates only in windowed mode
		if (!m_bFullScreen) {
			if ((rect.left == 0) && (rect.top == 0)) {
				// If WM_DISPLAYCHANGE shrank the window, store new position
				m_nWndLeft = rect.left;
				m_nWndTop = rect.top;
			}
			else {
				// Otherwise (including fullscreen->window), restore saved position
				rect.left = m_nWndLeft;
				rect.top = m_nWndTop;
			}
		}
	}


	if ((rect.left != rectWnd.left) || (rect.top != rectWnd.top)) {
		SetWindowPos(&wndTop, rect.left, rect.top, rect.right, rect.bottom, 0);
		return;
	}
	if ((rect.right != rectWnd.Width()) || (rect.bottom != rectWnd.Height())) {
		SetWindowPos(&wndTop, rect.left, rect.top, rect.right, rect.bottom, 0);
		return;
	}
}

//---------------------------------------------------------------------------
//
//	Recompute frame layout for the selected window scale
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ApplyWindowScale()
{
	InitPos(FALSE);
}

//---------------------------------------------------------------------------
//
//	Shell integration initialization
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitShell()
{
	int nSources;

	// Build shell notification flags
	if (::IsWinNT()) {
		// Windows 2000/XP: include shared-memory delivery flag
		nSources = SHCNRF_InterruptLevel | SHCNRF_ShellLevel | SHCNRF_NewDelivery;
	}
	else {
		// Windows 9x: shared-memory delivery is not used
		nSources = SHCNRF_InterruptLevel | SHCNRF_ShellLevel;
	}

	// Initialize one registration entry
	m_fsne[0].pidl = NULL;
	m_fsne[0].fRecursive = FALSE;

	// Register for shell media-change notifications
	m_uNotifyId = ::SHChangeNotifyRegister(m_hWnd,
							nSources,
							SHCNE_MEDIAINSERTED | SHCNE_MEDIAREMOVED | SHCNE_DRIVEADD | SHCNE_DRIVEREMOVED,
							WM_SHELLNOTIFY,
							sizeof(m_fsne)/sizeof(m_fsne[0]),
							m_fsne);
	ASSERT(m_uNotifyId);
}

//---------------------------------------------------------------------------
//
//	Virtual machine initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::InitVM()
{
	host_services_t host_services;
	host_services.lock_vm = VMHostSyncLockCallback;
	host_services.unlock_vm = VMHostSyncUnlockCallback;
	host_services.message = VMHostMessageCallback;
	host_services.user = NULL;

	::pVM = new VM;
	::GetVM()->SetHostServices(&host_services);
	if (!::GetVM()->Init()) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Component initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::InitComponent()
{
	BOOL bSuccess;
	CComponent *pComponent;

	ASSERT(!m_pFirstComponent);
	ASSERT(!m_pSch);
	ASSERT(!m_pSound);
	ASSERT(!m_pInput);
	ASSERT(!m_pPort);
	ASSERT(!m_pMIDI);
	ASSERT(!m_pTKey);
	ASSERT(!m_pHost);
	ASSERT(!m_pInfo);
	ASSERT(!m_pConfig);

	// Construct in order (config first, scheduler last)
	m_pConfig = new CConfig(this);
	m_pFirstComponent = m_pConfig;
	m_pSound = new CSound(this);
	m_pFirstComponent->AddComponent(m_pSound);
	m_pInput = new CInput(this);
	m_pFirstComponent->AddComponent(m_pInput);
	m_pPort = new CPort(this);
	m_pFirstComponent->AddComponent(m_pPort);
	m_pMIDI = new CMIDI(this);
	m_pFirstComponent->AddComponent(m_pMIDI);
	m_pTKey = new CTKey(this);
	m_pFirstComponent->AddComponent(m_pTKey);
	m_pHost = new CHost(this);
	m_pFirstComponent->AddComponent(m_pHost);
	m_pInfo = new CInfo(this, &m_StatusBar);
	m_pFirstComponent->AddComponent(m_pInfo);
	m_pSch = new CScheduler(this);
	m_pFirstComponent->AddComponent(m_pSch);

	// Initialize all components
	pComponent = m_pFirstComponent;
	bSuccess = TRUE;

	// Iterate component list
	while (pComponent) {
		if (!pComponent->Init()) {
			bSuccess = FALSE;
		}
		pComponent = pComponent->GetNextComponent();
	}

	return bSuccess;
}

//---------------------------------------------------------------------------
//
//	Version initialization
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitVer()
{
	TCHAR szPath[_MAX_PATH];
	DWORD dwHandle;
	DWORD dwLength;
	BYTE *pVerInfo;
	VS_FIXEDFILEINFO *pFileInfo;
	UINT uLength;
	DWORD dwMajor;
	DWORD dwMinor;

	ASSERT(this);

	// Get executable path
	::GetModuleFileName(NULL, szPath, _MAX_PATH);

	// Read version info block
	dwLength = GetFileVersionInfoSize(szPath, &dwHandle);
	if (dwLength == 0) {
		return;
	}

	pVerInfo = new BYTE[ dwLength ];
	if (::GetFileVersionInfo(szPath, dwHandle, dwLength, pVerInfo) == 0) {
		return;
	}

	// Query fixed file version info
	if (::VerQueryValue(pVerInfo, _T("\\"), (LPVOID*)&pFileInfo, &uLength) == 0) {
		delete[] pVerInfo;
		return;
	}

	// Build version numbers and pass them to the VM
	dwMajor = (DWORD)HIWORD(pFileInfo->dwProductVersionMS);
	dwMinor = (DWORD)(LOWORD(pFileInfo->dwProductVersionMS) * 16
					+ HIWORD(pFileInfo->dwProductVersionLS));
	::GetVM()->SetVersion(dwMajor, dwMinor);

	// Done
	delete[] pVerInfo;
}



void FASTCALL CFrmWnd::ReadFile(LPCTSTR pszFileName, CString& str)
{
   TRY
   {
      CFile file(pszFileName, CFile::modeRead);
	  UINT dwLength = (UINT)file.GetLength();
      file.Read(str.GetBuffer(dwLength), dwLength);
      str.ReleaseBuffer();
   }
   CATCH_ALL(e)
   {
      str.Empty();
e->ReportError();// see what's going wrong
   }
   END_CATCH_ALL
}



CString CFrmWnd::ProcessM3u(CString str)
{
	CString newPath = m_strXM6FilePath;
	PathRemoveFileSpecA(newPath.GetBuffer());
	newPath.ReleaseBuffer();

	int curPos = 0;
	CString resToken = str.Tokenize(_T("\r\n"), curPos);
	CString fullString;

	while (!resToken.IsEmpty())
	{
		fullString += "\"" + newPath + "\\" + resToken + "\"  ";
		resToken = str.Tokenize(_T("\r\n"), curPos);
	}

	return fullString;
}



//---------------------------------------------------------------------------
//
//	Command-line processing
//	Shared by startup command line and WM_COPYDATA
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::InitCmd(LPCTSTR lpszCmd)
{
	ASSERT(this);
	ASSERT(lpszCmd);

	CString sz;
	sz.Format(_T("%s"), lpszCmd);
	CString fileName = sz.Mid(sz.ReverseFind('\\') + 1);

	m_strXM6FilePath = lpszCmd;
	m_strXM6FileName = fileName;

	/* Parse and apply command-line parameters here */

	CString str = m_strXM6FilePath;
	CString extensionArchivo = "";

	int curPos = 0;
	CString resToken = str.Tokenize(_T("."), curPos);	// Tokenize full path by dot to get extension
	while (!resToken.IsEmpty())
	{
		extensionArchivo = resToken;
		resToken = str.Tokenize(_T("."), curPos);
	}

	/* If this is an M3U file, parse and expand its entries */
	if (extensionArchivo.MakeUpper() == "M3U")
	{
		CString m3uContent, cont2;
		ReadFile(lpszCmd, m3uContent);
		cont2 = ProcessM3u(m3uContent).Trim();
		strcpy((char*)lpszCmd, cont2);
	}

	// lpszCmd contains the full command line including quotes
	// Initialize parsing flags
	BOOL bReset = FALSE;

	// Create CString from command line
	CString cmdString(lpszCmd);

	// Split command line into parts
	curPos = 0;
	CString part = cmdString.Tokenize(_T(" "), curPos);
	int i = 0;
	// Process each token
	while (!part.IsEmpty())
	{
		// Strip surrounding quotes from each token
		if (part[0] == _T('\"') && part[part.GetLength() - 1] == _T('\"'))
		{
			part = part.Mid(1, part.GetLength() - 2);
		}

		// Convert token to LPCTSTR
		LPCTSTR szPath = (LPCTSTR)part;

		// Try to open token as media/state input
		bReset = InitCmdSub(i, szPath);

		// Read next token
		part = cmdString.Tokenize(_T(" "), curPos);
		i++;
	}

	// Trigger reset if requested by command processing
	if (bReset)
	{
		OnReset();
	}
}



//---------------------------------------------------------------------------
//
//	Command-line sub processing
//	Shared by command line, WM_COPYDATA, and drag-and-drop
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::InitCmdSub(int nDrive, LPCTSTR lpszPath)
{
	Filepath path;
	Fileio fio;
	LPTSTR lpszFile;
	DWORD dwSize;
	TCHAR szPath[_MAX_PATH];
	FDI *pFDI;
	CString strMsg;

	ASSERT(this);
	ASSERT((nDrive == 0) || (nDrive == 1));
	ASSERT(lpszPath);

	// Initialize FDI pointer
	pFDI = NULL;

	// Check whether the file can be opened
	path.SetPath(lpszPath);
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}
	dwSize = fio.GetFileSize();
	fio.Close();

	// Resolve absolute path
	::GetFullPathName(lpszPath, _MAX_PATH, szPath, &lpszFile);
	path.SetPath(szPath);

	// Lock VM
	::LockVM();

	// 128MO or 230MO or 540MO or 640MO
	if ((dwSize == 0x797f400) || (dwSize == 0xd9eea00) ||
		(dwSize == 0x1fc8b800) || (dwSize == 0x25e28000)) {
		// Try to mount as MO media
		nDrive = 2;

		if (!m_pSASI->Open(path)) {
			// MO mount failed
			GetScheduler()->Reset();
			ResetCaption();
			::UnlockVM();
			return FALSE;
		}
	}
	else {
		if (dwSize >= 0x200000) {
			// Try to load as VM state file
			nDrive = 4;

			// Pre-open validation
			if (!OnOpenPrep(path, FALSE)) {
				// Missing files, version mismatch, etc.
				GetScheduler()->Reset();
				ResetCaption();
				::UnlockVM();
				return FALSE;
			}

			// Execute load (handled by OnOpenSub)
			::UnlockVM();
			if (OnOpenSub(path)) {
				Filepath::SetDefaultDir(szPath);
			}
			// No reset required for this path
			return FALSE;
		}
		else {
			// Try to mount as floppy disk image
			/* Initialize floppy image from command line */

			if (!m_pFDD->Open(nDrive, path)) {
				// Floppy mount failed
				GetScheduler()->Reset();
				ResetCaption();
				::UnlockVM();
				return FALSE;
			}
			pFDI = m_pFDD->GetFDI(nDrive);
		}
	}

	// Reset scheduler state and unlock VM
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// Success: store default directory and add MRU entry
	Filepath::SetDefaultDir(szPath);
	GetConfig()->SetMRUFile(nDrive, szPath);

	// Warn on invalid floppy image
	if (pFDI) {
		if (pFDI->GetID() == MAKEID('B', 'A', 'D', ' ')) {
			::GetMsg(IDS_BADFDI_WARNING, strMsg);
			//MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		}

		// Reset is required only when a floppy is assigned
		return TRUE;
	}

	// Done
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Headless savestate smoke test
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::SmokeSaveState(LPCTSTR lpszCmd)
{
	LPCTSTR p;
	LPCTSTR q;
	TCHAR szDisk[_MAX_PATH];
	Filepath statePath;
	FILE *fp;
	BOOL ok;
	CFileStatus status;
	CFileStatus inputStatus;
	smoke_action_t actions[SmokeActionMax];
	int actionCount;
	DWORD runFrames;
	DWORD saveFrame;
	DWORD targetFrames;
	BOOL scheduled;
	BOOL saved;
	BOOL visible;
	DWORD visibleFrameMs;
	DWORD visibleHoldMs;
	DWORD nextFrameTick;

	fp = _tfopen(_T("C:\\tmp2\\xm6_smoke_savestate.log"), _T("wt"));
	if (!fp) {
		fp = _tfopen(_T("xm6_smoke_savestate.log"), _T("wt"));
	}

#define SMOKE_LOG(msg) do { if (fp) { _ftprintf(fp, _T("%s\n"), _T(msg)); fflush(fp); } } while (0)
#define SMOKE_LOG1(fmt,a) do { if (fp) { _ftprintf(fp, _T(fmt) _T("\n"), a); fflush(fp); } } while (0)

	SetEnvironmentVariableA("XM6_SMOKE_SAVESTATE", "1");
	visible = IsSmokeVisibleCommand();
	visibleFrameMs = SmokeReadDwordOption(lpszCmd, _T("--smoke-visible-frame-ms"), 16);
	visibleHoldMs = SmokeReadDwordOption(lpszCmd, _T("--smoke-visible-hold-ms"), 0);
	nextFrameTick = 0;
	if (visibleFrameMs < 1) {
		visibleFrameMs = 1;
	}
	if (visible) {
		GetScheduler()->Enable(FALSE);
	}
	SMOKE_LOG("smoke: start");
	SMOKE_LOG1("visible=%d", visible);
	SMOKE_LOG1("visible-frame-ms=%lu", (unsigned long)visibleFrameMs);

	p = _tcsstr(lpszCmd, _T("--smoke-savestate"));
	if (!p) {
		if (fp) {
			fclose(fp);
		}
		return FALSE;
	}
	p += _tcslen(_T("--smoke-savestate"));
	while (*p && (*p <= _T(' '))) {
		p++;
	}
	if (*p == _T('\"')) {
		p++;
		q = _tcschr(p, _T('\"'));
	}
	else {
		q = _tcschr(p, _T(' '));
	}
	if (!q) {
		q = p + _tcslen(p);
	}
	if ((q <= p) || ((q - p) >= _MAX_PATH)) {
		SMOKE_LOG("smoke: invalid disk argument");
		if (fp) {
			fclose(fp);
		}
		return FALSE;
	}

	_tcsnccpy(szDisk, p, (int)(q - p));
	szDisk[q - p] = _T('\0');
	SMOKE_LOG1("disk=%s", szDisk);
	if (!CFile::GetStatus(szDisk, inputStatus)) {
		SMOKE_LOG("smoke: disk not found");
		if (fp) {
			fclose(fp);
		}
		return FALSE;
	}

	ok = InitCmdSub(0, szDisk);
	SMOKE_LOG1("InitCmdSub reset=%d", ok);
	if (ok) {
		OnReset();
	}

	UpdateStateFileName();
	SMOKE_LOG1("state-name=%s", (LPCTSTR)m_strXM6FileName);
	SMOKE_LOG1("state-dir=%s", (LPCTSTR)m_strSaveStatePath);
	if (!BuildQuickStatePath(statePath)) {
		SMOKE_LOG("BuildQuickStatePath failed");
		if (fp) {
			fclose(fp);
		}
		return FALSE;
	}
	SMOKE_LOG1("state-path=%s", statePath.GetPath());

	actionCount = SmokeParseActions(lpszCmd, actions, SmokeActionMax);
	runFrames = SmokeReadDwordOption(lpszCmd, _T("--smoke-run-frames"), 0);
	saveFrame = SmokeReadDwordOption(lpszCmd, _T("--smoke-save-frame"), 0xffffffff);
	scheduled = (BOOL)((actionCount > 0) || (runFrames > 0) || (saveFrame != 0xffffffff));
	saved = FALSE;

	if (scheduled) {
		CRTC *crtc;
		Keyboard *keyboard;
		PPI *ppi;
		Render *render;
		PPI::joyinfo_t joy[PPI::PortMax];
		DWORD frames;
		DWORD lastCount;
		DWORD curCount;
		DWORD guard;
		DWORD maxActionFrame;
		int i;

		crtc = (CRTC*)::GetVM()->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
		keyboard = (Keyboard*)::GetVM()->SearchDevice(MAKEID('K', 'E', 'Y', 'B'));
		ppi = (PPI*)::GetVM()->SearchDevice(MAKEID('P', 'P', 'I', ' '));
		render = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
		if (!crtc || !keyboard || !ppi) {
			SMOKE_LOG("smoke: missing input/frame device");
			if (fp) {
				fclose(fp);
			}
			return FALSE;
		}

		memset(joy, 0, sizeof(joy));
		for (i = 0; i < actionCount; i++) {
			if (actions[i].type == SmokeActionJoy) {
				ppi->SetJoyType(actions[i].port, 1);
			}
		}
		for (i = 0; i < PPI::PortMax; i++) {
			const PPI::joyinfo_t *src = ppi->GetJoyInfo(i);
			if (src) {
				joy[i] = *src;
			}
		}

		maxActionFrame = 0;
		for (i = 0; i < actionCount; i++) {
			if (actions[i].end > maxActionFrame) {
				maxActionFrame = actions[i].end;
			}
		}
		targetFrames = runFrames;
		if ((saveFrame != 0xffffffff) && (saveFrame > targetFrames)) {
			targetFrames = saveFrame;
		}
		if (maxActionFrame > targetFrames) {
			targetFrames = maxActionFrame;
		}
		if ((saveFrame == 0xffffffff) && (targetFrames > 0)) {
			saveFrame = targetFrames;
		}

		SMOKE_LOG1("scheduled-actions=%d", actionCount);
		SMOKE_LOG1("run-frames=%lu", (unsigned long)targetFrames);
		SMOKE_LOG1("save-frame=%lu", (unsigned long)saveFrame);

		frames = 0;
		lastCount = crtc->GetDispCount();
		if (visible) {
			ShowWindow(SW_SHOW);
			UpdateWindow();
			SmokePumpMessages();
			nextFrameTick = ::GetTickCount() + visibleFrameMs;
			GetScheduler()->Enable(TRUE);
			while (frames <= targetFrames) {
				curCount = crtc->GetDispCount();
				if (curCount != lastCount) {
					frames++;
					lastCount = curCount;
					for (i = 0; i < actionCount; i++) {
						if (!actions[i].active && (frames == actions[i].start)) {
							actions[i].active = TRUE;
							if (actions[i].type == SmokeActionKey) {
								keyboard->MakeKey(actions[i].key);
								SMOKE_LOG1("key-down=%s", actions[i].name);
							}
							else if (actions[i].type == SmokeActionJoy) {
								joy[actions[i].port].button[actions[i].button] = TRUE;
								ppi->SetJoyInfo(actions[i].port, &joy[actions[i].port]);
								SMOKE_LOG1("joy-down=%s", actions[i].name);
							}
						}
						if (actions[i].active && (frames == actions[i].end)) {
							actions[i].active = FALSE;
							if (actions[i].type == SmokeActionKey) {
								keyboard->BreakKey(actions[i].key);
								SMOKE_LOG1("key-up=%s", actions[i].name);
							}
							else if (actions[i].type == SmokeActionJoy) {
								joy[actions[i].port].button[actions[i].button] = FALSE;
								ppi->SetJoyInfo(actions[i].port, &joy[actions[i].port]);
								SMOKE_LOG1("joy-up=%s", actions[i].name);
							}
						}
					}
					if (!saved && (frames == saveFrame)) {
						GetScheduler()->Enable(FALSE);
						SMOKE_LOG("quick-save begin");
						OnSaveSub(statePath);
						saved = TRUE;
					}
					if (frames >= targetFrames) {
						break;
					}
					nextFrameTick += visibleFrameMs;
				}
				SmokePumpMessages();
				while ((int)(nextFrameTick - ::GetTickCount()) > 0) {
					DWORD wait = nextFrameTick - ::GetTickCount();
					::Sleep(wait > 4 ? 4 : wait);
					SmokePumpMessages();
				}
			}
		}
		else {
			while (frames <= targetFrames) {
				for (i = 0; i < actionCount; i++) {
					if (!actions[i].active && (frames == actions[i].start)) {
						actions[i].active = TRUE;
						if (actions[i].type == SmokeActionKey) {
							keyboard->MakeKey(actions[i].key);
							SMOKE_LOG1("key-down=%s", actions[i].name);
						}
						else if (actions[i].type == SmokeActionJoy) {
							joy[actions[i].port].button[actions[i].button] = TRUE;
							ppi->SetJoyInfo(actions[i].port, &joy[actions[i].port]);
							SMOKE_LOG1("joy-down=%s", actions[i].name);
						}
					}
					if (actions[i].active && (frames == actions[i].end)) {
						actions[i].active = FALSE;
						if (actions[i].type == SmokeActionKey) {
							keyboard->BreakKey(actions[i].key);
							SMOKE_LOG1("key-up=%s", actions[i].name);
						}
						else if (actions[i].type == SmokeActionJoy) {
							joy[actions[i].port].button[actions[i].button] = FALSE;
							ppi->SetJoyInfo(actions[i].port, &joy[actions[i].port]);
							SMOKE_LOG1("joy-up=%s", actions[i].name);
						}
					}
				}

				if (!saved && (frames == saveFrame)) {
					SMOKE_LOG("quick-save begin");
					OnSaveSub(statePath);
					saved = TRUE;
				}

				if (frames >= targetFrames) {
					break;
				}

				guard = 0;
				do {
					if (render) {
						render->EnableAct(TRUE);
					}
					if (!::GetVM()->Exec(2000)) {
						SMOKE_LOG("smoke: VM exec failed");
						if (fp) {
							fclose(fp);
						}
						return FALSE;
					}
					curCount = crtc->GetDispCount();
					guard++;
				} while ((curCount == lastCount) && (guard < 2000));

				if (guard >= 2000) {
					SMOKE_LOG("smoke: frame wait timeout");
					if (fp) {
						fclose(fp);
					}
					return FALSE;
				}
				frames++;
				lastCount = curCount;
			}
		}

		for (i = 0; i < actionCount; i++) {
			if (actions[i].active) {
				actions[i].active = FALSE;
				if (actions[i].type == SmokeActionKey) {
					keyboard->BreakKey(actions[i].key);
				}
				else if (actions[i].type == SmokeActionJoy) {
					joy[actions[i].port].button[actions[i].button] = FALSE;
					ppi->SetJoyInfo(actions[i].port, &joy[actions[i].port]);
				}
			}
		}
	}

	if (!saved) {
		SMOKE_LOG("quick-save begin");
		OnSaveSub(statePath);
		saved = TRUE;
	}
	if (!CFile::GetStatus(statePath.GetPath(), status)) {
		SMOKE_LOG("quick-save missing output");
		if (fp) {
			fclose(fp);
		}
		return FALSE;
	}
	SMOKE_LOG("quick-save output exists");

	SMOKE_LOG("quick-load prep begin");
	if (!OnOpenPrep(statePath, FALSE)) {
		SMOKE_LOG("OnOpenPrep failed");
		if (fp) {
			fclose(fp);
		}
		return FALSE;
	}
	SMOKE_LOG("quick-load sub begin");
	if (!OnOpenSub(statePath)) {
		SMOKE_LOG("OnOpenSub failed");
		if (fp) {
			fclose(fp);
		}
		return FALSE;
	}

	if (visible && (visibleHoldMs > 0)) {
		DWORD endTick = ::GetTickCount() + visibleHoldMs;
		SMOKE_LOG1("visible-hold-ms=%lu", (unsigned long)visibleHoldMs);
		while ((int)(endTick - ::GetTickCount()) > 0) {
			SmokePumpMessages();
			::Sleep(16);
		}
	}

	SMOKE_LOG("smoke: ok");
	if (fp) {
		fclose(fp);
	}

#undef SMOKE_LOG
#undef SMOKE_LOG1

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Visible savestate smoke test
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::SmokeStartVisible(LPCTSTR lpszCmd)
{
	CRTC *crtc;
	PPI *ppi;
	DWORD runFrames;
	DWORD saveFrame;
	DWORD maxActionFrame;
	int i;

	SmokeLogLine(_T("smoke-visible: start"));
	SetEnvironmentVariableA("XM6_SMOKE_SAVESTATE", "1");

	UpdateStateFileName();
	if (!BuildQuickStatePath(g_smokeVisibleStatePath)) {
		SmokeLogLine(_T("smoke-visible: BuildQuickStatePath failed"));
		return FALSE;
	}
	SmokeLogFormat(_T("state-path=%s"), g_smokeVisibleStatePath.GetPath());

	_tcsncpy(g_smokeVisibleCmd, lpszCmd, _countof(g_smokeVisibleCmd) - 1);
	g_smokeVisibleCmd[_countof(g_smokeVisibleCmd) - 1] = _T('\0');
	g_smokeVisibleActionCount = 0;
	g_smokeVisibleActionsParsed = FALSE;
	g_smokeVisiblePx68kVideo = IsSmokePx68kVideoCommand(lpszCmd);
	runFrames = SmokeReadDwordOption(lpszCmd, _T("--smoke-run-frames"), 0);
	saveFrame = SmokeReadDwordOption(lpszCmd, _T("--smoke-save-frame"), 0xffffffff);
	g_smokeVisibleHoldMs = SmokeReadDwordOption(lpszCmd, _T("--smoke-visible-hold-ms"), 0);
	SmokeLogLine(_T("smoke-visible: action parse deferred"));

	maxActionFrame = 0;
	g_smokeVisibleFirstActionFrame = 0xffffffff;
	g_smokeVisibleLastActionFrame = 0;
	for (i = 0; i < g_smokeVisibleActionCount; i++) {
		if (g_smokeVisibleActions[i].start < g_smokeVisibleFirstActionFrame) {
			g_smokeVisibleFirstActionFrame = g_smokeVisibleActions[i].start;
		}
		if (g_smokeVisibleActions[i].end > maxActionFrame) {
			maxActionFrame = g_smokeVisibleActions[i].end;
		}
	}
	g_smokeVisibleLastActionFrame = maxActionFrame;
	g_smokeVisibleTargetFrames = runFrames;
	if ((saveFrame != 0xffffffff) && (saveFrame > g_smokeVisibleTargetFrames)) {
		g_smokeVisibleTargetFrames = saveFrame;
	}
	if (maxActionFrame > g_smokeVisibleTargetFrames) {
		g_smokeVisibleTargetFrames = maxActionFrame;
	}
	if ((saveFrame == 0xffffffff) && (g_smokeVisibleTargetFrames > 0)) {
		saveFrame = g_smokeVisibleTargetFrames;
	}
	if (saveFrame == 0xffffffff) {
		saveFrame = 0;
	}
	g_smokeVisibleSaveFrame = saveFrame;
	g_smokeVisibleFrames = 0;
	g_smokeVisibleSaved = FALSE;
	g_smokeVisibleTickLogged = FALSE;
	SmokeLogLine(_T("smoke-visible: frame plan ready"));

	crtc = (CRTC*)::GetVM()->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	ppi = (PPI*)::GetVM()->SearchDevice(MAKEID('P', 'P', 'I', ' '));
	g_smokeVisibleCRTC = crtc;
	g_smokeVisibleKeyboard = (Keyboard*)::GetVM()->SearchDevice(MAKEID('K', 'E', 'Y', 'B'));
	g_smokeVisiblePPI = ppi;
	if (!crtc || !ppi || !g_smokeVisibleKeyboard) {
		SmokeLogLine(_T("smoke-visible: missing input/frame device"));
		return FALSE;
	}
	SmokeLogLine(_T("smoke-visible: devices ready"));

	memset(g_smokeVisibleJoy, 0, sizeof(g_smokeVisibleJoy));
	if (GetInput()) {
		for (i = 0; i < PPI::PortMax; i++) {
			int nButton;
			for (nButton = 0; nButton < PPI::ButtonMax; nButton++) {
				GetInput()->SetSmokeJoyButton(i, nButton, FALSE);
			}
		}
	}
	for (i = 0; i < PPI::PortMax; i++) {
		const PPI::joyinfo_t *src = ppi->GetJoyInfo(i);
		if (src) {
			g_smokeVisibleJoy[i] = *src;
		}
	}
	g_smokeVisibleLastCount = crtc->GetDispCount();
	g_smokeVisibleRunStartTick = ::GetTickCount();
	g_smokeVisiblePollTick = g_smokeVisibleRunStartTick;
	SmokeLogLine(_T("smoke-visible: initial input state ready"));

	SmokeLogFormatDword(_T("scheduled-actions=%lu"), (DWORD)g_smokeVisibleActionCount);
	SmokeLogFormatDword(_T("run-frames=%lu"), g_smokeVisibleTargetFrames);
	SmokeLogFormatDword(_T("save-frame=%lu"), g_smokeVisibleSaveFrame);
	SmokeLogFormatDword(_T("visible-hold-ms=%lu"), g_smokeVisibleHoldMs);
	SmokeLogFormatDword(_T("px68k-video-smoke=%lu"), g_smokeVisiblePx68kVideo ? 1 : 0);

	ShowWindow(SW_SHOW);
	SetForegroundWindow();
	UpdateWindow();

	g_smokeVisibleActive = TRUE;
	SmokeLogLine(_T("smoke-visible: controller active"));
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Visible savestate smoke timer
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::SmokeVisibleTimer()
{
	CFileStatus status;
	CRTC *crtc;
	Keyboard *keyboard;
	PPI *ppi;
	DWORD curCount;
	DWORD deltaFrames;
	int i;
	BOOL ok;

	if (!g_smokeVisibleActive) {
		return;
	}
	if (!g_smokeVisibleTickLogged) {
		g_smokeVisibleTickLogged = TRUE;
		SmokeLogLine(_T("smoke-visible: first tick"));
	}

	crtc = g_smokeVisibleCRTC;
	keyboard = g_smokeVisibleKeyboard;
	ppi = g_smokeVisiblePPI;
	if (!crtc || !keyboard || !ppi) {
		SmokeLogLine(_T("smoke-visible: missing timer device"));
		g_smokeVisibleActive = FALSE;
		::ExitProcess(1);
		return;
	}

	curCount = crtc->GetDispCount();
	if (curCount == g_smokeVisibleLastCount) {
		return;
	}
	if (curCount > g_smokeVisibleLastCount) {
		deltaFrames = curCount - g_smokeVisibleLastCount;
	}
	else {
		deltaFrames = 1;
	}
	g_smokeVisibleLastCount = curCount;

	while (deltaFrames > 0) {
		deltaFrames--;
		g_smokeVisibleFrames++;
		if ((g_smokeVisibleFrames % 55) == 0) {
			SmokeLogFormatDword(_T("frame=%lu"), g_smokeVisibleFrames);
		}

		if (!g_smokeVisibleActionsParsed && (g_smokeVisibleFrames >= 55)) {
			g_smokeVisibleActionCount = SmokeParseActions(g_smokeVisibleCmd,
				g_smokeVisibleActions, SmokeActionMax);
			g_smokeVisibleFirstActionFrame = 0xffffffff;
			g_smokeVisibleLastActionFrame = 0;
			for (i = 0; i < g_smokeVisibleActionCount; i++) {
				if (g_smokeVisibleActions[i].start < g_smokeVisibleFirstActionFrame) {
					g_smokeVisibleFirstActionFrame = g_smokeVisibleActions[i].start;
				}
				if (g_smokeVisibleActions[i].end > g_smokeVisibleLastActionFrame) {
					g_smokeVisibleLastActionFrame = g_smokeVisibleActions[i].end;
				}
				if ((g_smokeVisibleActions[i].type == SmokeActionJoy) ||
					(g_smokeVisibleActions[i].type == SmokeActionJoyAxis)) {
					SmokeEnsureJoyPort(ppi, g_smokeVisibleActions[i].port);
				}
			}
			g_smokeVisibleActionsParsed = TRUE;
			SmokeLogFormatDword(_T("parsed-actions=%lu"), (DWORD)g_smokeVisibleActionCount);
		}

		if ((g_smokeVisibleFrames >= g_smokeVisibleFirstActionFrame) &&
			(g_smokeVisibleFrames <= g_smokeVisibleLastActionFrame)) {
			for (i = 0; i < g_smokeVisibleActionCount; i++) {
				if (!g_smokeVisibleActions[i].active &&
					(g_smokeVisibleFrames == g_smokeVisibleActions[i].start)) {
					g_smokeVisibleActions[i].active = TRUE;
					if (g_smokeVisibleActions[i].type == SmokeActionKey) {
						keyboard->MakeKey(g_smokeVisibleActions[i].key);
						SmokeLogFormat(_T("key-down=%s"), g_smokeVisibleActions[i].name);
					}
					else if (g_smokeVisibleActions[i].type == SmokeActionJoy) {
						if (GetInput()) {
							GetInput()->SetSmokeJoyButton(g_smokeVisibleActions[i].port,
								g_smokeVisibleActions[i].button, TRUE);
						}
						g_smokeVisibleJoy[g_smokeVisibleActions[i].port].
							button[g_smokeVisibleActions[i].button] = TRUE;
						ppi->SetJoyInfo(g_smokeVisibleActions[i].port,
							&g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
						SmokeLogFormat(_T("joy-down=%s"), g_smokeVisibleActions[i].name);
					}
					else if (g_smokeVisibleActions[i].type == SmokeActionJoyAxis) {
						if (GetInput()) {
							GetInput()->SetSmokeJoyAxis(g_smokeVisibleActions[i].port,
								g_smokeVisibleActions[i].axis, TRUE, g_smokeVisibleActions[i].axis_value);
						}
						g_smokeVisibleJoy[g_smokeVisibleActions[i].port].axis[g_smokeVisibleActions[i].axis] =
							g_smokeVisibleActions[i].axis_value;
						ppi->SetJoyInfo(g_smokeVisibleActions[i].port,
							&g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
						SmokeLogFormat(_T("joy-down=%s"), g_smokeVisibleActions[i].name);
					}
				}
				if (g_smokeVisibleActions[i].active &&
					(g_smokeVisibleFrames == g_smokeVisibleActions[i].end)) {
					g_smokeVisibleActions[i].active = FALSE;
					if (g_smokeVisibleActions[i].type == SmokeActionKey) {
						keyboard->BreakKey(g_smokeVisibleActions[i].key);
						SmokeLogFormat(_T("key-up=%s"), g_smokeVisibleActions[i].name);
					}
					else if (g_smokeVisibleActions[i].type == SmokeActionJoy) {
						if (GetInput()) {
							GetInput()->SetSmokeJoyButton(g_smokeVisibleActions[i].port,
								g_smokeVisibleActions[i].button, FALSE);
						}
						g_smokeVisibleJoy[g_smokeVisibleActions[i].port].
							button[g_smokeVisibleActions[i].button] = FALSE;
						ppi->SetJoyInfo(g_smokeVisibleActions[i].port,
							&g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
						SmokeLogFormat(_T("joy-up=%s"), g_smokeVisibleActions[i].name);
					}
					else if (g_smokeVisibleActions[i].type == SmokeActionJoyAxis) {
						if (GetInput()) {
							GetInput()->SetSmokeJoyAxis(g_smokeVisibleActions[i].port,
								g_smokeVisibleActions[i].axis, FALSE, 0);
						}
						g_smokeVisibleJoy[g_smokeVisibleActions[i].port].axis[g_smokeVisibleActions[i].axis] = 0;
						ppi->SetJoyInfo(g_smokeVisibleActions[i].port,
							&g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
						SmokeLogFormat(_T("joy-up=%s"), g_smokeVisibleActions[i].name);
					}
				}
			}
		}

		if (!g_smokeVisibleSaved && (g_smokeVisibleFrames >= g_smokeVisibleSaveFrame)) {
			GetScheduler()->Enable(FALSE);
			SmokeLogLine(_T("quick-save begin"));
			OnSaveSub(g_smokeVisibleStatePath);
			g_smokeVisibleSaved = TRUE;
			GetScheduler()->Enable(TRUE);
		}

		if (g_smokeVisibleFrames >= g_smokeVisibleTargetFrames) {
			break;
		}
	}

	if (g_smokeVisibleFrames < g_smokeVisibleTargetFrames) {
		return;
	}

	g_smokeVisibleActive = FALSE;
	GetScheduler()->Enable(FALSE);
	SmokeLogRenderState();

	for (i = 0; i < g_smokeVisibleActionCount; i++) {
		if (g_smokeVisibleActions[i].active) {
			g_smokeVisibleActions[i].active = FALSE;
			if (g_smokeVisibleActions[i].type == SmokeActionKey) {
				keyboard->BreakKey(g_smokeVisibleActions[i].key);
			}
			else if (g_smokeVisibleActions[i].type == SmokeActionJoy) {
				if (GetInput()) {
					GetInput()->SetSmokeJoyButton(g_smokeVisibleActions[i].port,
						g_smokeVisibleActions[i].button, FALSE);
				}
				g_smokeVisibleJoy[g_smokeVisibleActions[i].port].
					button[g_smokeVisibleActions[i].button] = FALSE;
				ppi->SetJoyInfo(g_smokeVisibleActions[i].port,
					&g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
			}
			else if (g_smokeVisibleActions[i].type == SmokeActionJoyAxis) {
				if (GetInput()) {
					GetInput()->SetSmokeJoyAxis(g_smokeVisibleActions[i].port,
						g_smokeVisibleActions[i].axis, FALSE, 0);
				}
				g_smokeVisibleJoy[g_smokeVisibleActions[i].port].axis[g_smokeVisibleActions[i].axis] = 0;
				ppi->SetJoyInfo(g_smokeVisibleActions[i].port,
					&g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
			}
		}
	}

	if (g_smokeVisiblePx68kVideo && !SmokeValidatePx68kVideoFrame()) {
		SmokeLogLine(_T("smoke-visible: px68k video failed"));
		::ExitProcess(1);
		return;
	}

	if (!g_smokeVisibleSaved) {
		SmokeLogLine(_T("quick-save begin"));
		OnSaveSub(g_smokeVisibleStatePath);
		g_smokeVisibleSaved = TRUE;
	}

	ok = CFile::GetStatus(g_smokeVisibleStatePath.GetPath(), status);
	if (ok) {
		SmokeLogLine(_T("quick-save output exists"));
		SmokeLogLine(_T("quick-load prep begin"));
		ok = OnOpenPrep(g_smokeVisibleStatePath, FALSE);
	}
	if (ok) {
		SmokeLogLine(_T("quick-load sub begin"));
		ok = OnOpenSub(g_smokeVisibleStatePath);
	}
	if (ok && (g_smokeVisibleHoldMs > 0)) {
		DWORD endTick = ::GetTickCount() + g_smokeVisibleHoldMs;
		while ((int)(endTick - ::GetTickCount()) > 0) {
			SmokePumpMessages();
			::Sleep(16);
		}
	}

	SmokeLogLine(ok ? _T("smoke-visible: ok") : _T("smoke-visible: failed"));
	::ExitProcess(ok ? 0 : 1);
}

//---------------------------------------------------------------------------
//
//	Save components
//	Scheduler is stopped, but CSound and CInput keep running.
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::SaveComponent(const Filepath& path, DWORD dwPos)
{
	Fileio fio;
	DWORD dwID;
	CComponent *pComponent;
	DWORD dwMajor;
	DWORD dwMinor;
	int nVer;

	ASSERT(this);
	ASSERT(dwPos > 0);

	// Build version information value
	::GetVM()->GetVersion(dwMajor, dwMinor);
	nVer = (int)((dwMajor << 8) | dwMinor);

	// Open file and seek to target position
	if (!fio.Open(path, Fileio::Append)) {
		return FALSE;
	}
	if (!fio.Seek(dwPos)) {
		fio.Close();
		return FALSE;
	}

	// Write main component marker
	dwID = MAKEID('M', 'A', 'I', 'N');
	if (!fio.Write(&dwID, sizeof(dwID))) {
		fio.Close();
		return FALSE;
	}

	// Component loop
	pComponent = m_pFirstComponent;
	while (pComponent) {
		// Save component ID
		dwID = pComponent->GetID();
		if (!fio.Write(&dwID, sizeof(dwID))) {
			fio.Close();
			return FALSE;
		}

		// Save component-specific data
		if (!pComponent->Save(&fio, nVer)) {
			fio.Close();
			return FALSE;
		}

		// Next component
		pComponent = pComponent->GetNextComponent();
	}

	// Write end marker
	dwID = MAKEID('E', 'N', 'D', ' ');
	if (!fio.Write(&dwID, sizeof(dwID))) {
		fio.Close();
		return FALSE;
	}

	// Finish
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load components
//	Scheduler is stopped, but CSound and CInput keep running.
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::LoadComponent(const Filepath& path, DWORD dwPos)
{
	Fileio fio;
	DWORD dwID;
	CComponent *pComponent;
	char cHeader[0x10];
	int nVer;

	ASSERT(this);
	ASSERT(dwPos > 0);

	// Open file
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Read header
	if (!fio.Read(cHeader, sizeof(cHeader))) {
		fio.Close();
		return FALSE;
	}

	// Validate header and read version information
	cHeader[0x0a] = '\0';
	nVer = ::strtoul(&cHeader[0x09], NULL, 16);
	nVer <<= 8;
	cHeader[0x0d] = '\0';
	nVer |= ::strtoul(&cHeader[0x0b], NULL, 16);
	cHeader[0x09] = '\0';
	if (strcmp(cHeader, "XM6 DATA ") != 0) {
		fio.Close();
		return FALSE;
	}

	// Seek to component section
	if (!fio.Seek(dwPos)) {
		fio.Close();
		return FALSE;
	}

	// Read main component marker
	if (!fio.Read(&dwID, sizeof(dwID))) {
		fio.Close();
		return FALSE;
	}
	if (dwID != MAKEID('M', 'A', 'I', 'N')) {
		fio.Close();
		return FALSE;
	}

	// Component loop
	for (;;) {
		// Read component ID
		if (!fio.Read(&dwID, sizeof(dwID))) {
			fio.Close();
			return FALSE;
		}

		// End marker check
		if (dwID == MAKEID('E', 'N', 'D', ' ')) {
			break;
		}

		// Find component instance
		pComponent = m_pFirstComponent->SearchComponent(dwID);
		if (!pComponent) {
			// Component existed at save time but is missing now
			fio.Close();
			return FALSE;
		}

		// Load component-specific data
		if (!pComponent->Load(&fio, nVer)) {
			fio.Close();
			return FALSE;
		}
	}

	// Close file
	fio.Close();

	// Apply settings (with VM lock)
	if (GetConfig()->IsApply()) {
		::LockVM();
		ApplyCfg();
		::UnlockVM();
	}

	// Refresh window content
	GetView()->Invalidate(FALSE);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ApplyCfg()
{
	Config config;
	CComponent *pComponent;

	// Retrieve current configuration
	GetConfig()->GetConfig(&config);

	// Apply to VM first
	::GetVM()->ApplyCfg(&config);

	// Then apply to each component
	pComponent = m_pFirstComponent;
	while (pComponent) {
		pComponent->ApplyCfg(&config);
		pComponent = pComponent->GetNextComponent();
	}

	// Then apply to the draw view
	GetView()->ApplyCfg(&config);

	// Sync the top-level window size with the saved scale after the config is applied.
	// InitPos() runs once before the config component exists, so the initial frame size
	// can otherwise stay at the 1.0x fallback even when the INI says otherwise.
	ApplyWindowScale();

	// Popup subwindow mode
	if (config.popup_swnd != m_bPopup) {
		// Clear all subwindows
		GetView()->ClrSWnd();

		// Update flag
		m_bPopup = config.popup_swnd;
	}



	// Frame window mouse options
	m_bMouseMid = config.mouse_mid;
	m_bAutoMouse = config.auto_mouse;

	// Apply VSync setting
	m_bVSyncEnabled = config.render_vsync;
	GetView()->SetVSync(m_bVSyncEnabled);

	// Apply renderer mode setting
	GetView()->ApplyRendererConfig(config.render_mode);

	// Keep SaveState path initialized once
	if (m_strSaveStatePath.GetLength() == 0)
		m_strSaveStatePath = config.ruta_savestate;
	//int msgboxID = MessageBox(m_strSaveStatePath,"rutasave",  2 );
	if (config.mouse_port == 0) {
		// Disable mouse-capture mode when no mouse is connected.
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}
}

//---------------------------------------------------------------------------
//
//	Kick handler
//
//---------------------------------------------------------------------------
LONG CFrmWnd::OnKick(UINT /*uParam*/, LONG /*lParam*/)
{
	CComponent *pComponent;
	CInfo *pInfo;
	Config config;
	CString strMsg;
	MSG msg;
	Memory *pMemory;
	DWORD dwTick20;
	DWORD dwTick40;
	DWORD dwTick80;
	DWORD dwNow;
	LPSTR lpszCmd;
	LPCTSTR lpszCommand;
	BOOL bFullScreen;
	BOOL bSmokeSaveState;
	BOOL bSmokeVisible;
	BOOL bSmokePx68kVideo;

	bSmokeSaveState = IsSmokeSaveStateCommand();
	bSmokeVisible = IsSmokeVisibleCommand();
	bSmokePx68kVideo = IsSmokePx68kVideoCommand(A2T(AfxGetApp()->m_lpCmdLine));

	// Handle startup errors first
	switch (m_nStatus) {
		// VM initialization error
		case 1:
			if (bSmokeSaveState) {
				::ExitProcess(1);
			}
			::GetMsg(IDS_INIT_VMERR, strMsg);
			MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
			PostMessage(WM_CLOSE, 0, 0);
			return 0;

		// Component initialization error
		case 2:
			if (bSmokeSaveState) {
				::ExitProcess(1);
			}
			::GetMsg(IDS_INIT_COMERR, strMsg);
			MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
			PostMessage(WM_CLOSE, 0, 0);
			return 0;
	}
	// Normal startup path
	ASSERT(m_nStatus == 0);

	// Check required ROM data
	pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(pMemory);
	if (!pMemory->CheckIPL()) {
		if (bSmokeSaveState) {
			::ExitProcess(1);
		}
		::GetMsg(IDS_INIT_IPLERR, strMsg);
		if (MessageBox(strMsg, NULL, MB_ICONSTOP | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
			PostMessage(WM_CLOSE, 0, 0);
			return 0;
		}
	}
	if (!pMemory->CheckCG()) {
		if (bSmokeSaveState) {
			::ExitProcess(1);
		}
		::GetMsg(IDS_INIT_CGERR, strMsg);
		if (MessageBox(strMsg, NULL, MB_ICONSTOP | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
			PostMessage(WM_CLOSE, 0, 0);
			return 0;
		}
	}

	// Read config (power_off startup option)
	GetConfig()->GetConfig(&config);
	if (config.power_off) {
		// Start with power switch OFF
		::GetVM()->SetPower(FALSE);
		::GetVM()->PowerSW(FALSE);
	}

	// PLAN G: Hold VM lock across initial bootstrap
	::LockVM();

	// Register child-window class
	m_strWndClsName = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS);

	// Enable components; scheduler is enabled later.
	// PLAN G: atomic startup sequence
	GetView()->Enable(TRUE);
	pComponent = m_pFirstComponent;
	while (pComponent) {
		if (pComponent->GetID() == MAKEID('S', 'C', 'H', 'E')) {
			pComponent = pComponent->GetNextComponent();
			continue;
		}
		pComponent->Enable(TRUE);
		pComponent = pComponent->GetNextComponent();
	}

	if (!config.power_off) {
		OnReset();
	}

	lpszCmd = AfxGetApp()->m_lpCmdLine;
	lpszCommand = A2T(lpszCmd);
	if (bSmokeSaveState && !bSmokeVisible) {
		BOOL bSmoke = SmokeSaveState(lpszCommand);
		::ExitProcess(bSmoke ? 0 : 1);
	}

	if (bSmokeSaveState && bSmokeVisible) {
		TCHAR szSmokeDisk[_MAX_PATH];
		CFileStatus smokeInputStatus;

		SmokeLogLine(_T("smoke-visible: early mount"));
		if (!SmokeReadOptionValue(lpszCommand, _T("--smoke-savestate"), NULL,
			szSmokeDisk, _countof(szSmokeDisk))) {
			SmokeLogLine(_T("smoke-visible: invalid disk argument"));
			::ExitProcess(1);
		}
		SmokeLogFormat(_T("disk=%s"), szSmokeDisk);
		if (!CFile::GetStatus(szSmokeDisk, smokeInputStatus)) {
			SmokeLogLine(_T("smoke-visible: disk not found"));
			::ExitProcess(1);
		}
		InitCmd(szSmokeDisk);
		SmokeLogLine(_T("smoke-visible: early mount complete"));
	}
	else if (!bSmokeSaveState) {
		RestoreDiskState();
	}

	if (!bSmokeSaveState && (_tcslen(lpszCommand) > 0)) {
		InitCmd(lpszCommand);
	}

	// Sincronizar nombre de savestate tras restaurar medios (INI/MRU o Parametros)
	UpdateStateFileName();

	bFullScreen = FALSE;
	if (IsZoomed()) {
		ShowWindow(SW_RESTORE);
		bFullScreen = TRUE;
	}

	bFullScreen = RestoreFrameWnd(bFullScreen);
	if (bFullScreen) {
		PostMessage(WM_COMMAND, IDM_FULLSCREEN);
	}

	if (bSmokeSaveState && bSmokePx68kVideo) {
		BOOL bActive = FALSE;
		::LockVM();
		if (m_pDrawView) {
			bActive = m_pDrawView->SetRenderFastDummyEnabled(TRUE);
		}
		::UnlockVM();
		SmokeLogFormatDword(_T("px68k-video-forced=%lu"), bActive ? 1 : 0);
		if (!bActive) {
			::ExitProcess(1);
		}
	}

	if (bSmokeSaveState && bSmokeVisible) {
		if (!SmokeStartVisible(lpszCommand)) {
			::ExitProcess(1);
		}
	}

	pComponent = m_pFirstComponent;
	while (pComponent) {
		if (pComponent->GetID() == MAKEID('S', 'C', 'H', 'E')) {
			if (!config.power_off) {
				if (bSmokeSaveState && bSmokeVisible) {
					SmokeLogLine(_T("smoke-visible: enabling scheduler"));
				}
				pComponent->Enable(TRUE);
				if (bSmokeSaveState && bSmokeVisible) {
					SmokeLogLine(_T("smoke-visible: scheduler component enabled"));
				}
			}
			break;
		}
		pComponent = pComponent->GetNextComponent();
	}

	::UnlockVM();
	if (bSmokeSaveState && bSmokeVisible) {
		SmokeLogLine(_T("smoke-visible: bootstrap unlocked"));
	}

	// Main loop
	DWORD dwStartTick = ::GetTickCount();
	BOOL bAutoResetDone = FALSE;
	dwTick20 = ::GetTickCount();
	dwTick40 = dwTick20;
	dwTick80 = dwTick20;
	while (!m_bExit) {
		// Check and pump pending window messages
		if (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			if (!AfxGetApp()->PumpMessage()) {
				::PostQuitMessage(0);
				return 0;
			}
			// Continue to guarantee m_bExit is checked after WM_DESTROY
			continue;
		}

		// Sleep briefly when idle
		if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			Sleep(1);
			if (::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
				continue;
			}

			// Refresh info pointer each idle tick
			pInfo = GetInfo();
			if (!pInfo) {
				continue;
			}

			// Update periodic timers
			dwNow = ::GetTickCount();

			if (g_smokeVisibleActive && ((dwNow - g_smokeVisiblePollTick) >= 16)) {
				g_smokeVisiblePollTick = dwNow;
				SmokeVisibleTimer();
			}

			if ((dwNow - dwTick20) >= 20) {
				dwTick20 = dwNow;
				pInfo->UpdateStatus();
				UpdateExec();

				// HACK: auto-reset to work around cold-boot bug
				if (!bSmokeSaveState && !bAutoResetDone && (dwNow - dwStartTick) >= 90) {
					bAutoResetDone = TRUE;
					OnReset();
				}
			}

			if ((dwNow - dwTick40) >= 40) {
				dwTick40 = dwNow;
				// Update render view every 40ms
				GetView()->Update();
			}

			if ((dwNow - dwTick80) >= 80) {
				dwTick80 = dwNow;
				// Update caption/info every 80ms
				pInfo->UpdateCaption();
				pInfo->UpdateInfo();
			}
		}
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	Get window class name
//
//---------------------------------------------------------------------------
LPCTSTR FASTCALL CFrmWnd::GetWndClassName() const
{
	ASSERT(this);
	ASSERT(m_strWndClsName.GetLength() != 0);

	return (LPCTSTR)m_strWndClsName;
}

//---------------------------------------------------------------------------
//
//	Popup mode flag
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::IsPopupSWnd() const
{
	ASSERT(this);
	return m_bPopup;
}

//---------------------------------------------------------------------------
//
//	Window close
//
//---------------------------------------------------------------------------
void CFrmWnd::OnClose()
{
	CString strFormat;
	CString strText;
	Filepath path;

	ASSERT(this);
	ASSERT(!m_bSaved);


	//int msgboxID = MessageBox("close", "Window to close", 2);

/* Save-confirmation dialog is intentionally disabled here */
	// If a valid state file exists, this is where save prompting would run
	::LockVM();
	::GetVM()->GetPath(path);
	::UnlockVM();

	// If a valid state file exists
	if (!path.IsClear()) {
		// If runtime activity history is at least 20ms on Windows side
	/*	if (m_dwExec >= 2) {
			// Confirmation dialog
			::GetMsg(IDS_SAVECLOSE, strFormat);
			strText.Format(strFormat, path.GetFileExt());
			nResult = MessageBox(strText, NULL, MB_ICONQUESTION | MB_YESNOCANCEL);

			// Handle confirmation result
			switch (nResult) {
				// YES
				case IDYES:
					// Save
					OnSaveSub(path);
					break;

				// NO
				case IDNO:
					// Continue without state
					::GetVM()->Clear();
					break;

				// CANCEL
				case IDCANCEL:
					// Abort close and keep running
					return;
			}
		}*/
	}

	// If initialization already completed
	if ((m_nStatus == 0) && !m_bSaved) {
		// Save frame and disk resume state
		SaveFrameWnd();
		SaveDiskState();
		m_bSaved = TRUE;
	}

	// Exit fullscreen first
	if (m_bFullScreen) {
		ASSERT(m_nStatus == 0);
		OnFullScreen();
	}

	// If initialization already completed
	if (m_nStatus == 0) {
		// Release mouse capture mode
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}

	OutputDebugString("\n\nOnClose executed...\n\n");
	// Base class handling
	CFrameWnd::OnClose();
}

//---------------------------------------------------------------------------
//
//	Window ofstroy
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDestroy()
{
	ASSERT(this);

	// If initialization already completed
	if ((m_nStatus == 0) && !m_bSaved)
	{
		// Save frame and disk resume state
		SaveFrameWnd();
		SaveDiskState();
		m_bSaved = TRUE;
	}

	// Exit fullscreen first
	if (m_bFullScreen) {
		ASSERT(m_nStatus == 0);
		OnFullScreen();
	}

	// Shared cleanup path (also used by WM_ENDSESSION)
	CleanSub();


	OutputDebugString("\n\nOnDestroy executed...\n\n");

	// Base class handling
	CFrameWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	End session
//
//---------------------------------------------------------------------------
void CFrmWnd::OnEndSession(BOOL bEnding)
{
	ASSERT(this);

	// Cleanup during system logoff/shutdown
	if (bEnding) {
		// If initialization already completed
		if (m_nStatus == 0) {
			// Save frame and disk resume state
			if (!m_bSaved) {
				SaveFrameWnd();
				SaveDiskState();
				m_bSaved = TRUE;
			}

			// Cleanup
			CleanSub();
		}
	}


	OutputDebugString("\n\nOnEndSession executed...\n\n");

	// Base class handling
	CFrameWnd::OnEndSession(bEnding);
}

//---------------------------------------------------------------------------
//
//	Shared cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::CleanSub()
{
	CComponent *pComponent;
	CComponent *pNext;
	int i;

	// Set exit flag
	m_bExit = TRUE;

	// Disable view and all components
	GetView()->Enable(FALSE);
	pComponent = m_pFirstComponent;
	while (pComponent) {
		pComponent->Enable(FALSE);
		pComponent = pComponent->GetNextComponent();
	}

	// Release mouse capture mode
	if (m_nStatus == 0) {
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}

	// Wait for scheduler execution to settle
	for (i=0; i<8; i++) {
		::LockVM();
		::UnlockVM();
	}

	// Stop scheduler (CScheduler)
	if (m_nStatus == 0) {
		GetScheduler()->Stop();
	}

	// Cleanup and delete all components
	pComponent = m_pFirstComponent;
	while (pComponent) {
		pComponent->Cleanup();
		pComponent = pComponent->GetNextComponent();
	}
	pComponent = m_pFirstComponent;
	while (pComponent) {
		pNext = pComponent->GetNextComponent();
		delete pComponent;
		pComponent = pNext;
	}

	// Cleanup and destroy VM instance
	if (::pVM) {
		::LockVM();
		::GetVM()->Cleanup();
		delete ::pVM;
		::pVM = NULL;
		::UnlockVM();
	}

	// Unregister shell notifications
	if (m_uNotifyId) {
		 VERIFY(::SHChangeNotifyDeregister(m_uNotifyId));
		 m_uNotifyId = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Save frame window state
//
//---------------------------------------------------------------------------
void CFrmWnd::SaveFrameWnd()
{
	CRect rectWnd;
	Config config;

	ASSERT(this);
	ASSERT_VALID(this);

	// Get config snapshot
	GetConfig()->GetConfig(&config);

	// Caption/menu/status visibility
	config.caption = m_bCaption;
	config.menu_bar = m_bMenuBar;
	config.status_bar = m_bStatusBar;

	// Window position
	if (m_bFullScreen) {
		// In fullscreen, keep last windowed position
		config.window_left = m_nWndLeft;
		config.window_top = m_nWndTop;
	}
	else {
		// In windowed mode, save current position
		GetWindowRect(&rectWnd);
		config.window_left = rectWnd.left;
		config.window_top = rectWnd.top;
	}

	// Fullscreen flag
	config.window_full = m_bFullScreen;

	// Shader state
	if (m_pDrawView) {
		config.render_shader = m_pDrawView->IsShaderEnabled();
		config.render_fast_dummy = m_pDrawView->IsRenderFastDummyEnabled();
	}

	// Store updated config
	GetConfig()->SetConfig(&config);
}

//---------------------------------------------------------------------------
//
//	Save disk/media state
//
//---------------------------------------------------------------------------
void CFrmWnd::SaveDiskState()
{
	int nDrive;
	Filepath path;
	Config config;

	ASSERT(this);
	ASSERT_VALID(this);

	// Lock VM
	::LockVM();

	// Get config snapshot
	GetConfig()->GetConfig(&config);

	// Floppy disk state
	for (nDrive=0; nDrive<2; nDrive++) {
		// Ready state
		config.resume_fdi[nDrive] = m_pFDD->IsReady(nDrive, FALSE);

		// If not ready, skip remaining fields
		if (!config.resume_fdi[nDrive]) {
			continue;
		}

		// Current media type
		config.resume_fdm[nDrive]  = m_pFDD->GetMedia(nDrive);

		// Write-protect state
		config.resume_fdw[nDrive] = m_pFDD->IsWriteP(nDrive);
	}

	// MO disk state
	config.resume_mos = m_pSASI->IsReady();
	if (config.resume_mos) {
		config.resume_mow = m_pSASI->IsWriteP();
	}

	// CD-ROM
	config.resume_iso = m_pSCSI->IsReady(FALSE);

	// Save-state file availability
	::GetVM()->GetPath(path);
	config.resume_xm6 = !path.IsClear();

	// Default directory
	_tcscpy(config.resume_path, Filepath::GetDefaultDir());

	// Store updated config
	GetConfig()->SetConfig(&config);

	// Unlock VM
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Restore frame window state
//	OnCreate and OnKick invoke this twice.
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::RestoreFrameWnd(BOOL bFullScreen)
{
	int nWidth;
	int nHeight;
	int nLeft;
	int nTop;
	CRect rectWnd;
	BOOL bValid;
	Config config;

	ASSERT(this);

	// Get config snapshot
	GetConfig()->GetConfig(&config);

	// Keep default placement if window-position restore is disabled
	if (!config.resume_screen) {
		return bFullScreen;
	}

	// Caption
	m_bCaption = config.caption;
	ShowCaption();

	// Menu bar
	m_bMenuBar = config.menu_bar;
	ShowMenu();

	// Status bar
	m_bStatusBar = config.status_bar;
	ShowStatus();

	// Get virtual-screen size and origin
	nWidth = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
	nHeight = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
	nLeft = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
	nTop = ::GetSystemMetrics(SM_YVIRTUALSCREEN);

	// Get current window rectangle
	GetWindowRect(&rectWnd);

	// Restore window position only when it is still within visible bounds
	bValid = TRUE;
	if (config.window_left < nLeft) {
		if (config.window_left < nLeft - rectWnd.Width()) {
			bValid = FALSE;
		}
	}
	else {
		if (config.window_left >= (nLeft + nWidth)) {
			bValid = FALSE;
		}
	}
	if (config.window_top < nTop) {
		if (config.window_top < nTop - rectWnd.Height()) {
			bValid = FALSE;
		}
	}
	else {
		if (config.window_top >= (nTop + nHeight)) {
			bValid = FALSE;
		}
	}

	// Apply restored window position
	if (bValid) {
		SetWindowPos(&wndTop, config.window_left, config.window_top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

		// Update saved work-area position at the same time
		m_nWndLeft = config.window_left;
		m_nWndTop = config.window_top;
	}



	/*char cadena[20],cadena2[20];
    sprintf(cadena, "%d", nHeight);
	sprintf(cadena2, "%d", nWidth);
	 int msgboxID = MessageBox(
       cadena,"Height",
        2 );
	 int msgboxID2 = MessageBox(
       cadena2,"Width",
        2 );
		*/




	// Stop here when VM is not fully initialized.
	if (m_nStatus != 0) {
		return FALSE;
	}





	// Fullscreen restore behavior.
	if (bFullScreen || config.window_full) {
		// Start maximized if requested now or in previous session.
		return TRUE;
	}
	else {
		// Stay in normal windowed mode.
		return FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Restore disk/media state
//
//---------------------------------------------------------------------------
void CFrmWnd::RestoreDiskState()
{
	int nDrive;
	TCHAR szMRU[_MAX_PATH];
	BOOL bResult;
	Filepath path;
	Config config;

	ASSERT(this);

	// Get config snapshot
	GetConfig()->GetConfig(&config);

	// If resume-from-state is enabled, try it first
	if (config.resume_state) {
		// A previous state file exists
		if (config.resume_xm6) {
			// Get saved path
			GetConfig()->GetMRUFile(4, 0, szMRU);
			path.SetPath(szMRU);

			// Pre-open validation
			if (OnOpenPrep(path)) {
				// Open state via subroutine
				if (OnOpenSub(path)) {
					// On success, restore only default directory if configured
					if (config.resume_dir) {
						Filepath::SetDefaultDir(config.resume_path);
					}

					// Stop here after state load (state already contains disk/media state)
					return;
				}
			}
		}
	}

	// Floppy disk resume
	if (config.resume_fd) {
		for (nDrive=0; nDrive<2; nDrive++) {
			// Was media inserted at save time?
			if (!config.resume_fdi[nDrive]) {
				// No inserted media recorded; skip
				continue;
			}

			// Reinsert media
			GetConfig()->GetMRUFile(nDrive, 0, szMRU);
			ASSERT(szMRU[0] != _T('\0'));
			path.SetPath(szMRU);

			// Lock VM and attempt media mount
			::LockVM();
			bResult = m_pFDD->Open(nDrive, path, config.resume_fdm[nDrive]);
			::UnlockVM();

			// If mount fails, skip this drive
			if (!bResult) {
				continue;
			}

			// Restore write-protect flag
			if (config.resume_fdw[nDrive]) {
				::LockVM();
				m_pFDD->WriteP(nDrive, TRUE);
				::UnlockVM();
			}
		}
	}

	// MO resume
	if (config.resume_mo) {
		// Was media inserted at save time?
		if (config.resume_mos) {
			// Reinsert media
			GetConfig()->GetMRUFile(2, 0, szMRU);
			ASSERT(szMRU[0] != _T('\0'));
			path.SetPath(szMRU);

			// Lock VM and attempt media mount
			::LockVM();
			bResult = m_pSASI->Open(path);
			::UnlockVM();

			// If mount succeeds
			if (bResult) {
				// Restore write-protect flag
				if (config.resume_mow) {
					::LockVM();
					m_pSASI->WriteP(TRUE);
					::UnlockVM();
				}
			}
		}
	}

	// CD-ROM
	if (config.resume_cd) {
		// Was media inserted at save time?
		if (config.resume_iso) {
			// Reinsert media
			GetConfig()->GetMRUFile(3, 0, szMRU);
			ASSERT(szMRU[0] != _T('\0'));
			path.SetPath(szMRU);

			// Lock VM and attempt media mount
			::LockVM();
			m_pSCSI->Open(path, FALSE);
			::UnlockVM();
		}
	}

	// Restore default directory
	if (config.resume_dir) {
		Filepath::SetDefaultDir(config.resume_path);
	}
}

//---------------------------------------------------------------------------
//
//	Display change
//
//---------------------------------------------------------------------------
LRESULT CFrmWnd::OnDisplayChange(UINT uParam, LONG lParam)
{
	LRESULT lResult;
	uParam = 0;
	lParam = 0;
	// Base class behavior
	lResult=0;//CFrameWnd::OnDisplayChange(0, uParam, lParam);

	// Ignore while minimized
	if (IsIconic()) {
		return lResult;
	}

	// Recalculate position
	InitPos(FALSE);

	return lResult;
}

//---------------------------------------------------------------------------
//
//	Window background erase
//
//---------------------------------------------------------------------------
BOOL CFrmWnd::OnEraseBkgnd(CDC * /* pDC */)
{
	// Suppress background erase to reduce flicker
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window paint
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPaint()
{
	PAINTSTRUCT ps;

	// Always paint while VM is locked
	::LockVM();

	BeginPaint(&ps);

	// Refresh caption and status when VM is active
	if (m_nStatus == 0) {
		ResetCaption();
		ResetStatus();
	}

	EndPaint(&ps);

	// Unlock VM
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Window move
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMove(int x, int y)
{
	CRect rect;

	// If initialization already completed
	if (m_nStatus == 0) {
		// Check mouse-capture mode
		if (GetInput()->GetMouseMode()) {
			// Rebuild cursor clip rectangle around moved window
			ClipCursor(NULL);
			GetWindowRect(&rect);
			SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
			ClipCursor(&rect);
		}
	}

	// Base class handling
	CFrameWnd::OnMove(x, y);
}

//---------------------------------------------------------------------------
//
//	Activation
//
//---------------------------------------------------------------------------
void CFrmWnd::OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized)
{
	CInput *pInput;
	CScheduler *pScheduler;

	// If initialization already completed
	if (m_nStatus == 0) {
		// Notify input and scheduler about activation change
		pInput = GetInput();
		pScheduler = GetScheduler();
		if (pInput && pScheduler) {
			// On inactive/minimized, deactivate input and slow scheduler
			if ((nState == WA_INACTIVE) || bMinimized) {
				// Stop accepting input and run at inactive speed
				pInput->Activate(FALSE);
				pScheduler->Activate(FALSE);

				// Force mouse mode off (popup-window safety)
				if (pInput->GetMouseMode()) {
					OnMouseMode();
				}
			}
			else {
				// Resume input and normal execution speed
				pInput->Activate(TRUE);
				pScheduler->Activate(TRUE);
			}
		}
	}

	// Base class handling
	CFrameWnd::OnActivate(nState, pWnd, bMinimized);
}

//---------------------------------------------------------------------------
//
//	Application activation
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
void CFrmWnd::OnActivateApp(BOOL bActive, DWORD dwThreadID)
#else
void CFrmWnd::OnActivateApp(BOOL bActive, HTASK hTask)
#endif
{
	// If initialization already completed
	if (m_nStatus == 0) {
		// Full-screen-specific handling
		if (m_bFullScreen) {
			if (bActive) {
				// Becoming active
				HideTaskBar(TRUE, TRUE);
				RecalcStatusView();
			}
			else {
				// Becoming inactive
				HideTaskBar(FALSE, FALSE);
			}
		}
	}

	// Base class handling
#if _MFC_VER >= 0x700
	CFrameWnd::OnActivateApp(bActive, dwThreadID);
#else
	CFrameWnd::OnActivateApp(bActive, hTask);
#endif
}

//---------------------------------------------------------------------------
//
//	Enter menu loop
//
//---------------------------------------------------------------------------
void CFrmWnd::OnEnterMenuLoop(BOOL bTrackPopup)
{
	CInput *pInput;
	CScheduler *pScheduler;

	// Refresh caption before entering menu loop
	ResetCaption();

	::LockVM();

	// Notify input component
	pInput = GetInput();
	if (pInput) {
		pInput->Menu(TRUE);
	}

	// Disable mouse mode so menu can be operated normally
	if (pInput->GetMouseMode()) {
		OnMouseMode();
	}

	// Notify scheduler
	pScheduler = GetScheduler();
	if (pScheduler) {
		pScheduler->Menu(TRUE);
	}

	::UnlockVM();

	// Base class handling
	CFrameWnd::OnEnterMenuLoop(bTrackPopup);
}

//---------------------------------------------------------------------------
//
//	Exit menu loop
//
//---------------------------------------------------------------------------
void CFrmWnd::OnExitMenuLoop(BOOL bTrackPopup)
{
	CInput *pInput;
	CScheduler *pScheduler;

	::LockVM();

	// Notify input component
	pInput = GetInput();
	if (pInput) {
		pInput->Menu(FALSE);
	}

	// Notify scheduler
	pScheduler = GetScheduler();
	if (pScheduler) {
		pScheduler->Menu(FALSE);
	}

	::UnlockVM();

	// Refresh caption after leaving menu loop
	ResetCaption();

	// Base class handling
	CFrameWnd::OnExitMenuLoop(bTrackPopup);
}

//---------------------------------------------------------------------------
//
//	Parent window notification
//
//---------------------------------------------------------------------------
void CFrmWnd::OnParentNotify(UINT message, LPARAM lParam)
{
	CInput *pInput;

	// Forward middle-button events to CInput
	if ((message == WM_MBUTTONDOWN) && (m_nStatus == 0)) {
		// Get input component
		pInput = GetInput();
		if (pInput) {
			// Only enable mouse mode if it is currently disabled
			if (!pInput->GetMouseMode()) {
				// Only when middle-button mouse-mode toggle is enabled
				if (m_bMouseMid) {
					OnMouseMode();
				}
			}
		}
	}

	// Base class handling
	CFrameWnd::OnParentNotify(message, lParam);
}

//---------------------------------------------------------------------------
//
//	Context menu
//
//---------------------------------------------------------------------------
void CFrmWnd::OnContextMenu(CWnd * /*pWnd*/, CPoint pos)
{
	CMenu *pMenu;
	SHORT sF10;
	SHORT sShift;

	// Handle keyboard-triggered context menu invocation
	if ((pos.x == -1) && (pos.y == -1)) {
		// Check scheduler and input state
		if (GetScheduler()->IsEnable()) {
			if (GetInput()->IsActive() && !GetInput()->IsMenu()) {
				// If DIK_APPS is mapped
				if (GetInput()->IsKeyMapped(DIK_APPS)) {
					// Check whether SHIFT+F10 is currently pressed
					sF10 = ::GetAsyncKeyState(VK_F10);
					sShift = ::GetAsyncKeyState(VK_SHIFT);
					if (((sF10 & 0x8000) == 0) || ((sShift & 0x8000) == 0)) {
						// Ignore VK_APPS-generated invocation
						return;
					}
				}
			}
		}

		// If mouse mode is active, release it for keyboard menu invocation
		if (GetInput()->GetMouseMode()) {
			OnMouseMode();
		}
	}
	else {
		// Ignore mouse-triggered context menu while mouse mode is active
		if (GetInput()->GetMouseMode()) {
			return;
		}
	}

	// Show popup menu
	m_bPopupMenu = TRUE;
	pMenu = m_PopupMenu.GetSubMenu(0);
	pMenu->TrackPopupMenu(TPM_CENTERALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							pos.x, pos.y, this, 0);
	m_bPopupMenu = FALSE;
}

//---------------------------------------------------------------------------
//
//	Power change notification
//
//---------------------------------------------------------------------------
LONG CFrmWnd::OnPowerBroadCast(UINT /*uParam*/, LONG /*lParam*/)
{
	// If initialization already completed
	if (m_nStatus == 0) {
		// Lock VM and refresh timer resolution settings
		::LockVM();
		timeEndPeriod(1);
		timeBeginPeriod(1);
		::UnlockVM();
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	System commands
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSysCommand(UINT nID, LPARAM lParam)
{
	// Handle "standard window position" command
	if ((nID & 0xfff0) == IDM_STDWIN) {
		InitPos(TRUE);
		return;
	}

	// Redirect maximize to fullscreen toggle
	if ((nID & 0xfff0) == SC_MAXIMIZE) {
		if (!m_bFullScreen) {
			PostMessage(WM_COMMAND, IDM_FULLSCREEN);
		}
		return;
	}

	// Base class handling
	CFrameWnd::OnSysCommand(nID, lParam);
}

//---------------------------------------------------------------------------
//
//	Data transfer
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
afx_msg BOOL CFrmWnd::OnCopyData(CWnd* /*pWnd*/, COPYDATASTRUCT* pCopyDataStruct)
#else
LONG CFrmWnd::OnCopyData(UINT /*uParam*/, LONG pCopyDataStruct)
#endif
{
	PCOPYDATASTRUCT pCDS;

	// Get received COPYDATA parameters
	pCDS = (PCOPYDATASTRUCT)pCopyDataStruct;

	// Process forwarded command line
	InitCmd((LPSTR)pCDS->lpData);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Shell notifications
//
//---------------------------------------------------------------------------
LRESULT CFrmWnd::OnShellNotify(UINT uParam, LONG lParam)
{
	HANDLE hMemoryMap;
	DWORD dwProcessId;
	LPITEMIDLIST *pidls;
	HANDLE hLock;
	LONG nEvent;
	TCHAR szPath[_MAX_PATH];
	CHost *pHost;

	// Branch by Windows platform family
	if (::IsWinNT()) {
		// Windows 2000/XP: lock shell notification payload
		hMemoryMap = (HANDLE)uParam;
		dwProcessId = (DWORD)lParam;
		hLock = ::SHChangeNotification_Lock(hMemoryMap, dwProcessId, &pidls, &nEvent);
		if (hLock == NULL) {
			return 0;
		}
	}
	else {
		// Windows 9x: PIDLs/event arrive directly in uParam/lParam
		pidls = (LPITEMIDLIST*)uParam;
		nEvent = lParam;
		hLock = NULL;
	}

	// While running, forward notifications to CHost when available
	if (m_nStatus == 0) {
		pHost = GetHost();

#if 1
		// If WinDRV is not effectively enabled, suppress host notification (v2.04)
		{
			Config config;
			GetConfig()->GetConfig(&config);
			if ((config.windrv_enable <= 0) || (config.windrv_enable > 3)) {
				pHost = NULL;
			}
		}
#endif

		if (pHost) {
			// Resolve path from PIDL
			::SHGetPathFromIDList(pidls[0], szPath);

			// Notify host component
			pHost->ShellNotify(nEvent, szPath);
		}
	}

	// On NT-class systems, unlock shell notification payload
	if (::IsWinNT()) {
		ASSERT(hLock);
		::SHChangeNotification_Unlock(hLock);
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	Execution counter update
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::UpdateExec()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// While scheduler runs, increment execution counter (cleared on save)
	if (GetScheduler()->IsEnable()) {
		m_dwExec++;
		if (m_dwExec == 0) {
			m_dwExec--;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Provide message string
//
//---------------------------------------------------------------------------
void CFrmWnd::GetMessageString(UINT nID, CString& rMessage) const
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];
	TCHAR szName[60];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	int nMRU;
	int nDisk;
	BOOL bValid;
	CInfo *pInfo;

	// Start as unresolved
	bValid = FALSE;

	// Resolve menu strings first (including English resources + MRU)
	if ((nID >= IDM_OPEN) && (nID <= IDM_ABOUT)) {
		// English environment?
		if (!::IsJapanese()) {
			// Resolve through safe message loader (handles +5000 fallback safely).
			::GetMsg(nID, rMessage);
			if (!rMessage.IsEmpty()) {
				bValid = TRUE;
			}
		}
	}

	// Special-case menu string (IDM_STDWIN)
	if (nID == IDM_STDWIN) {
		// English environment?
		if (!::IsJapanese()) {
			// Resolve through safe message loader (handles +5000 fallback safely).
			::GetMsg(nID, rMessage);
			if (!rMessage.IsEmpty()) {
				bValid = TRUE;
			}
		}
	}

	// YMFM runtime toggle
	if (nID == IDM_YMFM) {
		rMessage = _T("YMFM runtime audio backend");
		bValid = TRUE;
	}

	// MRU0
	if ((nID >= IDM_D0_MRU0) && (nID <= IDM_D0_MRU8)) {
		nMRU = nID - IDM_D0_MRU0;
		ASSERT((nMRU >= 0) && (nMRU <= 8));
		GetConfig()->GetMRUFile(0, nMRU, szPath);
		szPath[60] = _T('\0');
		rMessage = szPath;
		bValid = TRUE;
	}

	// MRU1
	if ((nID >= IDM_D1_MRU0) && (nID <= IDM_D1_MRU8)) {
		nMRU = nID - IDM_D1_MRU0;
		ASSERT((nMRU >= 0) && (nMRU <= 8));
		GetConfig()->GetMRUFile(1, nMRU, szPath);
		szPath[60] = _T('\0');
		rMessage = szPath;
		bValid = TRUE;
	}

	// MRU2
	if ((nID >= IDM_MO_MRU0) && (nID <= IDM_MO_MRU8)) {
		nMRU = nID - IDM_MO_MRU0;
		ASSERT((nMRU >= 0) && (nMRU <= 8));
		GetConfig()->GetMRUFile(2, nMRU, szPath);
		szPath[60] = _T('\0');
		rMessage = szPath;
		bValid = TRUE;
	}

	// MRU3
	if ((nID >= IDM_CD_MRU0) && (nID <= IDM_CD_MRU8)) {
		nMRU = nID - IDM_CD_MRU0;
		ASSERT((nMRU >= 0) && (nMRU <= 8));
		GetConfig()->GetMRUFile(3, nMRU, szPath);
		szPath[60] = _T('\0');
		rMessage = szPath;
		bValid = TRUE;
	}

	// MRU4
	if ((nID >= IDM_XM6_MRU0) && (nID <= IDM_XM6_MRU8)) {
		nMRU = nID - IDM_XM6_MRU0;
		ASSERT((nMRU >= 0) && (nMRU <= 8));
		GetConfig()->GetMRUFile(4, nMRU, szPath);
		szPath[60] = _T('\0');
		rMessage = szPath;
		bValid = TRUE;
	}

	// Disk label entry for drive 0
	if ((nID >= IDM_D0_MEDIA0) && (nID <= IDM_D0_MEDIAF)) {
		nDisk = nID - IDM_D0_MEDIA0;
		ASSERT((nDisk >= 0) && (nDisk <= 15));
		::LockVM();
		m_pFDD->GetName(0, szName, nDisk);
		m_pFDD->GetPath(0, path);
		::UnlockVM();
		_tsplitpath(path.GetPath(), szDrive, szDir, szFile, szExt);
		rMessage = szName;
		rMessage += _T(" (");
		rMessage += szFile;
		rMessage += szExt;
		rMessage += _T(")");
		bValid = TRUE;
	}

	// Disk label entry for drive 1
	if ((nID >= IDM_D1_MEDIA0) && (nID <= IDM_D1_MEDIAF)) {
		nDisk = nID - IDM_D1_MEDIA0;
		ASSERT((nDisk >= 0) && (nDisk <= 15));
		::LockVM();
		m_pFDD->GetName(1, szName, nDisk);
		m_pFDD->GetPath(1, path);
		::UnlockVM();
		_tsplitpath(path.GetPath(), szDrive, szDir, szFile, szExt);
		rMessage = szName;
		rMessage += _T(" (");
		rMessage += szFile;
		rMessage += szExt;
		rMessage += _T(")");
		bValid = TRUE;
	}

	// If nothing matched, defer to base class
	if (!bValid) {
		CFrameWnd::GetMessageString(nID, rMessage);
	}

	// Pass message to Info (internal retention)
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->SetMessageString(rMessage);
	}

	// Pass message to status view
	if (m_pStatusView) {
		m_pStatusView->SetMenuString(rMessage);
	}
}

//---------------------------------------------------------------------------
//
//	Hide/show taskbar
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::HideTaskBar(BOOL bHide, BOOL bFore)
{
	if (bHide) {
		// Enter always-on-top presentation
		m_hTaskBar = ::FindWindow(_T("Shell_TrayWnd"), NULL);
		if (m_hTaskBar) {
			::ShowWindow(m_hTaskBar, SW_HIDE);
		}
		ModifyStyleEx(0, WS_EX_TOPMOST, 0);
	}
	else {
		// Return to normal z-order
		ModifyStyleEx(WS_EX_TOPMOST, 0, 0);
		if (m_hTaskBar) {
			::ShowWindow(m_hTaskBar, SW_SHOWNA);
		}
	}

	// Optionally force foreground activation
	if (bFore) {
		SetForegroundWindow();
	}
}

//---------------------------------------------------------------------------
//
//	Status bar visibility
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ShowStatus()
{
	ASSERT(this);

	// Lock VM when required
	if (m_nStatus == 0) {
		::LockVM();
	}

	// Fullscreen path
	if (m_bFullScreen) {
		// Always hide the standard status bar in fullscreen
		ShowControlBar(&m_StatusBar, FALSE, FALSE);

		// If status display is enabled
		if (m_bStatusBar) {
			// Create fullscreen status view if missing
			if (!m_pStatusView) {
				// Create it
				CreateStatusView();

				// Recalculate placement
				if (m_bStatusBar) {
					RecalcStatusView();
				}
			}
		}
		else {
			// Destroy fullscreen status view when disabled
			if (m_pStatusView) {
				// Destroy it
				DestroyStatusView();

				// Recalculate placement
				RecalcStatusView();
			}
		}

		// Unlock VM when needed
		if (m_nStatus == 0) {
			::UnlockVM();
		}
		return;
	}

	// Status view is fullscreen-only; destroy it in windowed mode
	if (m_pStatusView) {
		DestroyStatusView();
		RecalcLayout();
	}

	// In windowed mode, control visibility through ShowControlBar
	ShowControlBar(&m_StatusBar, m_bStatusBar, FALSE);

	// Unlock VM when needed
	if (m_nStatus == 0) {
		::UnlockVM();
	}
}

//---------------------------------------------------------------------------
//
//	Create status view (fullscreen)
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::CreateStatusView()
{
	CInfo *pInfo;

	ASSERT(!m_pStatusView);

	if (m_bStatusBar) {
		// Create status view (layout update handled elsewhere)
		m_pStatusView = new CStatusView;
		if (m_pStatusView->Init(this)) {
			// Creation succeeded
			pInfo = GetInfo();
			if (pInfo) {
				// Inform Info that status view is now available
				pInfo->SetStatusView(m_pStatusView);
			}
		}
		else {
			// Creation failed
			m_bStatusBar = FALSE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Destroy status view (fullscreen)
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::DestroyStatusView()
{
	CInfo *pInfo;

	// Only when a valid status view exists
	if (m_pStatusView) {
		// Get Info component
		pInfo = GetInfo();
		if (pInfo) {
			// Inform Info that status view is being removed
			pInfo->SetStatusView(NULL);
		}

		// Destroy status view (layout update handled elsewhere)
		m_pStatusView->DestroyWindow();
		m_pStatusView = NULL;
	}
}

//---------------------------------------------------------------------------
// Recalculates and adjusts positions/sizes for two key child views:
// Draw view (`m_pDrawView`), where the emulated screen is rendered.
// Status view (`m_pStatusView`), the custom fullscreen status bar.
//---------------------------------------------------------------------------
void CFrmWnd::RecalcStatusView()
{
	CRect rectClient;
	GetClientRect(&rectClient);// Current client area

	const int clientWidth = rectClient.Width();
	const int clientHeight = rectClient.Height();

	// Common flags for SetWindowPos
	const UINT swpFlags = SWP_NOZORDER | SWP_NOACTIVATE;

	if (m_pStatusView && m_pStatusView->GetSafeHwnd())
	{
		CRect rectStatus;
		m_pStatusView->GetWindowRect(&rectStatus);
		const int statusHeight = rectStatus.Height();

		// Compute draw-view height
		const int drawHeight = clientHeight - statusHeight;

		// Resize only when dimensions changed
		if (m_pDrawView->GetSafeHwnd())
		{
			CRect currentDrawRect;
			m_pDrawView->GetWindowRect(&currentDrawRect);

			if (currentDrawRect.Height() != drawHeight ||
				currentDrawRect.Width() != clientWidth)
			{
				m_pDrawView->SetWindowPos(
					nullptr,
					0, 0,
					clientWidth, drawHeight,
					swpFlags
				);
			}
		}

		// Move/resize status view only when needed
		if (m_pStatusView->GetSafeHwnd())
		{
			CRect currentStatusRect;
			m_pStatusView->GetWindowRect(&currentStatusRect);

			if (currentStatusRect.top != drawHeight ||
				currentStatusRect.Height() != statusHeight ||
				currentStatusRect.Width() != clientWidth)
			{
				m_pStatusView->SetWindowPos(
					nullptr,
					0, drawHeight,
					clientWidth, statusHeight,
					swpFlags
				);
			}
		}
	}
	else
	{
		// Normal mode without custom status view
		if (m_pDrawView->GetSafeHwnd())
		{
			m_pDrawView->SetWindowPos(
				nullptr,
				0, 0,
				clientWidth, clientHeight,
				swpFlags
			);
		}
	}

	// Force redraw only when client rect is valid
	if (!rectClient.IsRectEmpty())
	{
		m_pDrawView->InvalidateRect(nullptr, FALSE);
		if (m_pStatusView) {
			m_pStatusView->InvalidateRect(nullptr, FALSE);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Reset status bar contents
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ResetStatus()
{
	CInfo *pInfo;

	// Reset via Info component when available
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->ResetStatus();
	}
}

//---------------------------------------------------------------------------
//
//	Owner-draw handling
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDrawItem(int nID, LPDRAWITEMSTRUCT lpDIS)
{
	int nPane;
	HDC hDC;
	CRect rectDraw;
	CInfo *pInfo;

	// Ensure this draw request is for the status bar
	if (lpDIS->hwndItem != m_StatusBar.m_hWnd) {
		CFrameWnd::OnDrawItem(nID, lpDIS);
		return;
	}

	// Get pane index, DC, and draw rectangle
	nPane = lpDIS->itemID;
	if (nPane == 0) {
		return;
	}
	nPane--;
	hDC = lpDIS->hDC;
	rectDraw = &lpDIS->rcItem;

	// Check Info component
	pInfo = GetInfo();
	if (!pInfo) {
		// Fill black as fallback
		::SetBkColor(hDC, RGB(0, 0, 0));
		::ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &rectDraw, NULL, 0, NULL);
		return;
	}

	// Delegate drawing to Info
	pInfo->DrawStatus(nPane, hDC, rectDraw);
}

//---------------------------------------------------------------------------
//
//	Menu bar visibility
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ShowMenu()
{
	HMENU hMenu;

	ASSERT(this);

	// Lock VM when required
	if (m_nStatus == 0) {
		::LockVM();
	}

	// Get currently attached menu
	hMenu = ::GetMenu(m_hWnd);

	// Case: menu should be hidden
	if (m_bFullScreen || !m_bMenuBar) {
		// If a menu is attached
		if (hMenu != NULL) {
			// Detach menu
			SetMenu(NULL);
		}
		if (m_nStatus == 0) {
			::UnlockVM();
		}
		return;
	}

	// Case: menu should be visible
	if (hMenu != NULL) {
		// If desired menu is already set
		if (m_Menu.GetSafeHmenu() == hMenu) {
			// No change needed
			if (m_nStatus == 0) {
				::UnlockVM();
			}
			return;
		}
	}

	// Attach standard menu
	SetMenu(&m_Menu);

	// Unlock VM when required
	if (m_nStatus == 0) {
		::UnlockVM();
	}
}

//---------------------------------------------------------------------------
//
//	Caption visibility
//
//---------------------------------------------------------------------------
void CFrmWnd::ShowCaption()
{
	const DWORD dwCaptionStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
	const BOOL bShouldShowCaption = !m_bFullScreen && m_bCaption;

	// 1) Skip if current style already matches target
	DWORD dwCurrentStyle = GetStyle();
	if (bShouldShowCaption == ((dwCurrentStyle & dwCaptionStyle) == dwCaptionStyle)) {
		return;// No style update needed
	}

	// 2) Lock VM only when needed
	const BOOL bVMLockNeeded = (m_nStatus == 0);
	if (bVMLockNeeded) {
		::LockVM();
	}

	// 3) Update window styles
	ModifyStyle(
		bShouldShowCaption?0:dwCaptionStyle,// Styles to remove
		bShouldShowCaption?dwCaptionStyle:0,// Styles to add
		SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED
	);

	// 4) Redraw menu bar when needed
	if (bShouldShowCaption && m_bMenuBar) {
		DrawMenuBar();// Redraw menu bar if visible
	}

	// 5) Unlock VM if it was locked
	if (bVMLockNeeded) {
		::UnlockVM();
	}
}

//---------------------------------------------------------------------------
//
//	Reset caption text
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::ResetCaption()
{
	CInfo *pInfo;

	// Reset through Info when available
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->ResetCaption();
	}
}

//---------------------------------------------------------------------------
//
//	Set info text
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::SetInfo(CString& strInfo)
{
	CInfo *pInfo;

	// Update through Info when available
	pInfo = GetInfo();
	if (pInfo) {
		pInfo->SetInfo(strInfo);
	}
}

//---------------------------------------------------------------------------
//
//	Get draw view
//
//---------------------------------------------------------------------------
CDrawView* FASTCALL CFrmWnd::GetView() const
{
	ASSERT(this);
	ASSERT(m_pDrawView);
	ASSERT(m_pDrawView->m_hWnd);
	return m_pDrawView;
}

//---------------------------------------------------------------------------
//
//	Get first component
//
//---------------------------------------------------------------------------
CComponent* FASTCALL CFrmWnd::GetFirstComponent() const
{
	ASSERT(this);
	return m_pFirstComponent;
}

//---------------------------------------------------------------------------
//
//	Get scheduler
//
//---------------------------------------------------------------------------
CScheduler* FASTCALL CFrmWnd::GetScheduler() const
{
	ASSERT(this);
	ASSERT(m_pSch);
	return m_pSch;
}

//---------------------------------------------------------------------------
//
//	Get sound component
//
//---------------------------------------------------------------------------
CSound* FASTCALL CFrmWnd::GetSound() const
{
	ASSERT(this);
	ASSERT(m_pSound);
	return m_pSound;
}

//---------------------------------------------------------------------------
//
//	Get input component
//
//---------------------------------------------------------------------------
CInput* FASTCALL CFrmWnd::GetInput() const
{
	ASSERT(this);
	ASSERT(m_pInput);
	return m_pInput;
}

//---------------------------------------------------------------------------
//
//	Get port component
//
//---------------------------------------------------------------------------
CPort* FASTCALL CFrmWnd::GetPort() const
{
	ASSERT(this);
	ASSERT(m_pPort);
	return m_pPort;
}

//---------------------------------------------------------------------------
//
//	Get MIDI component
//
//---------------------------------------------------------------------------
CMIDI* FASTCALL CFrmWnd::GetMIDI() const
{
	ASSERT(this);
	ASSERT(m_pMIDI);
	return m_pMIDI;
}

//---------------------------------------------------------------------------
//
//	Get TrueKey component
//
//---------------------------------------------------------------------------
CTKey* FASTCALL CFrmWnd::GetTKey() const
{
	ASSERT(this);
	ASSERT(m_pTKey);
	return m_pTKey;
}

//---------------------------------------------------------------------------
//
//	Get host component
//
//---------------------------------------------------------------------------
CHost* FASTCALL CFrmWnd::GetHost() const
{
	ASSERT(this);
	ASSERT(m_pHost);
	return m_pHost;
}

//---------------------------------------------------------------------------
//
//	Get info component
//
//---------------------------------------------------------------------------
CInfo* FASTCALL CFrmWnd::GetInfo() const
{
	ASSERT(this);

	// Return NULL when Info does not exist
	if (!m_pInfo) {
		return NULL;
	}

	// Return NULL when Info is disabled
	if (!m_pInfo->IsEnable()) {
		return NULL;
	}

	// Info is active; return pointer
	return m_pInfo;
}

//---------------------------------------------------------------------------
//
//	Get config component
//
//---------------------------------------------------------------------------
CConfig* FASTCALL CFrmWnd::GetConfig() const
{
	ASSERT(this);
	ASSERT(m_pConfig);
	return m_pConfig;
}

//---------------------------------------------------------------------------
//
//	Toggle Renderer (DX9/GDI)
//
//---------------------------------------------------------------------------
void CFrmWnd::OnYmfm()
{
	CSound *pSound;

	pSound = GetSound();
	if (!pSound) {
		return;
	}

	pSound->SetYmfm(!pSound->IsYmfm());

	::LockVM();
	ApplyCfg();
	::UnlockVM();

	CString info;
	info.Format(_T("YMFM: %s"), pSound->IsYmfm() ? _T("ON") : _T("OFF"));
	SetInfo(info);
}

void CFrmWnd::OnYmfmUI(CCmdUI *pCmdUI)
{
	CSound *pSound;

	if (!pCmdUI) {
		return;
	}

	pSound = GetSound();
	if (!pSound) {
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck(0);
		return;
	}

	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck(pSound->IsYmfm() ? 1 : 0);
}

void CFrmWnd::OnToggleRenderer()
{
	if (m_pDrawView) {
		m_pDrawView->ToggleRenderer();
	}
}

void CFrmWnd::OnToggleRenderFastDummy()
{
	if (!m_pDrawView) {
		return;
	}

	::LockVM();
	BOOL bEnabled = m_pDrawView->SetRenderFastDummyEnabled(!m_pDrawView->IsRenderFastDummyEnabled());
	::UnlockVM();

	Config config;
	GetConfig()->GetConfig(&config);
	config.render_fast_dummy = bEnabled;
	GetConfig()->SetConfig(&config);

	CString info;
	info.Format(_T("PX68k Video Engine (legacy alias): %s"), bEnabled ? _T("ON") : _T("OFF"));
	SetInfo(info);
}

void CFrmWnd::OnToggleRenderFastDummyUI(CCmdUI *pCmdUI)
{
	if (!pCmdUI) {
		return;
	}

	if (m_pDrawView) {
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck(m_pDrawView->IsRenderFastDummyEnabled() ? 1 : 0);
	}
	else {
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	Toggle VSync
//
//---------------------------------------------------------------------------
void CFrmWnd::OnToggleVSync()
{
	m_bVSyncEnabled = !m_bVSyncEnabled;

	if (m_pDrawView) {
		m_pDrawView->SetVSync(m_bVSyncEnabled);
		m_pDrawView->ShowRenderStatusOSD(m_bVSyncEnabled);
	}

	CString info;
	info.Format(_T("VSync: %s"), m_bVSyncEnabled ? _T("ON") : _T("OFF"));
	SetInfo(info);
}

//---------------------------------------------------------------------------
//
//	Enter borderless fullscreen mode
//
//---------------------------------------------------------------------------
void CFrmWnd::EnterBorderlessFullscreen()
{
	if (m_bBorderless) return;

	// Save current window state
	m_dwPrevStyle = GetWindowLong(m_hWnd, GWL_STYLE);
	m_dwPrevExStyle = GetWindowLong(m_hWnd, GWL_EXSTYLE);
	GetWindowPlacement(&m_wpPrev);

	// Get current monitor
	HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO mi = { sizeof(mi) };
	if (GetMonitorInfo(hMonitor, &mi)) {
		// Remove border-related styles
		SetWindowLong(m_hWnd, GWL_STYLE, m_dwPrevStyle & ~(WS_CAPTION | WS_THICKFRAME));
		SetWindowLong(m_hWnd, GWL_EXSTYLE, m_dwPrevExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

		// Expand to full monitor bounds
		SetWindowPos(&wndTop,
			mi.rcMonitor.left, mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

		m_bBorderless = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Exit borderless fullscreen mode
//
//---------------------------------------------------------------------------
void CFrmWnd::ExitBorderlessFullscreen()
{
	if (!m_bBorderless) return;

	// Restore saved styles and placement
	SetWindowLong(m_hWnd, GWL_STYLE, m_dwPrevStyle);
	SetWindowLong(m_hWnd, GWL_EXSTYLE, m_dwPrevExStyle);

	SetWindowPlacement(&m_wpPrev);
	SetWindowPos(NULL, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

	m_bBorderless = FALSE;
}

#endif	// _WIN32

void CFrmWnd::OnToggleOSD()
{
	if (m_pDrawView) {
		m_pDrawView->m_bShowOSD = !m_pDrawView->m_bShowOSD;
		m_pDrawView->Invalidate();
	}
}

void CFrmWnd::OnToggleShader()
{
	if (m_pDrawView && m_pDrawView->IsDX9Active()) {
		m_pDrawView->ToggleShader();

		Config config;
		GetConfig()->GetConfig(&config);
		config.render_shader = m_pDrawView->IsShaderEnabled();
		GetConfig()->SetConfig(&config);
	}
}

void CFrmWnd::OnToggleShaderUI(CCmdUI *pCmdUI)
{
	if (pCmdUI) {
		if (m_pDrawView && m_pDrawView->IsDX9Active()) {
			pCmdUI->SetCheck(m_pDrawView->IsShaderEnabled() ? 1 : 0);
			pCmdUI->Enable(TRUE);
		}
		else {
			pCmdUI->Enable(FALSE);
		}
	}
}

