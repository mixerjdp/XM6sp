//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	[MFC smoke tests]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "crtc.h"
#include "vc.h"
#include "sprite.h"
#include "keyboard.h"
#include "ppi.h"
#include "render.h"
#include "mfc_frm.h"
#include "mfc_com.h"
#include "mfc_inp.h"
#include "mfc_sch.h"
#include "mfc_snd.h"
#include "mfc_smoke.h"

BOOL SmokeIsSaveStateCommand()
{
	return (_tcsstr(AfxGetApp()->m_lpCmdLine, _T("--smoke-savestate")) != NULL);
}

BOOL SmokeIsVisibleCommand()
{
	return (_tcsstr(AfxGetApp()->m_lpCmdLine, _T("--smoke-visible")) != NULL);
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
static BOOL g_smokeVisibleSaveRequested = FALSE;
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

BOOL SmokeVisibleShouldPoll(DWORD dwNow)
{
	if (!g_smokeVisibleActive) {
		return FALSE;
	}
	if ((dwNow - g_smokeVisiblePollTick) < 16) {
		return FALSE;
	}
	g_smokeVisiblePollTick = dwNow;
	return TRUE;
}

void SmokeLogLine(LPCTSTR msg)
{
	FILE *fp;

	fp = _tfopen(_T("C:\\tmp\\xm6_smoke_savestate.log"), _T("at"));
	if (!fp) {
		fp = _tfopen(_T("xm6_smoke_savestate.log"), _T("at"));
	}
	if (fp) {
		_ftprintf(fp, _T("%s\n"), msg);
		fclose(fp);
	}
}

void SmokeLogFormat(LPCTSTR fmt, LPCTSTR value)
{
	FILE *fp;

	fp = _tfopen(_T("C:\\tmp\\xm6_smoke_savestate.log"), _T("at"));
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

	fp = _tfopen(_T("C:\\tmp\\xm6_smoke_savestate.log"), _T("at"));
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

static void SmokeLogRenderBuffer(LPCTSTR name, const DWORD *ptr, int stride, int width, int height)
{
	DWORD hash;
	DWORD nonzero;
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
	if (width > 1024) {
		width = 1024;
	}
	if (height > 1024) {
		height = 1024;
	}

	hash = 2166136261u;
	nonzero = 0;
	for (y=0; y<height; y++) {
		const DWORD *row = ptr + (y * stride);
		for (x=0; x<width; x++) {
			DWORD value = row[x];
			hash = SmokeHashDword(hash, value);
			if ((value & 0x00ffffff) != 0) {
				nonzero++;
			}
		}
	}

	_sntprintf(line, _countof(line),
		_T("%s: sig=%08lX size=%dx%d stride=%d nonzero=%lu first=%08lX,%08lX,%08lX,%08lX"),
		name, (unsigned long)hash, width, height, stride,
		(unsigned long)nonzero,
		(unsigned long)ptr[0], (unsigned long)ptr[1],
		(unsigned long)ptr[2], (unsigned long)ptr[3]);
	SmokeLogLine(line);
}

static void SmokeLogRenderPage(const Render::render_t *r, int page)
{
	TCHAR name[32];
	int width;
	int height;

	if (!r || (page < 0) || (page >= 4)) {
		return;
	}
	_sntprintf(name, _countof(name), _T("render-page%d"), page);
	width = r->mixlen;
	if (width <= 0 || width > 512) {
		width = 512;
	}
	height = r->height;
	if (height <= 0 || height > 512) {
		height = 512;
	}
	SmokeLogRenderBuffer(name, r->grpbuf[page], 1024, width, height);
}

static BOOL SmokeValidateRenderFrame()
{
	Render *render;
	Render::render_t *r;
	DWORD nonzero;
	int width;
	int height;
	int y;
	int x;

	render = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	if (!render) {
		SmokeLogLine(_T("render-validate: missing render"));
		return FALSE;
	}
	if (!render->IsReady()) {
		render->ForceRecompose();
		render->Complete();
	}
	r = render->GetWorkAddr();
	if (!r || !r->mixbuf) {
		SmokeLogLine(_T("render-validate: missing mixbuf"));
		return FALSE;
	}
	width = r->mixlen;
	if (width <= 0 || width > r->mixwidth) {
		width = r->mixwidth;
	}
	height = r->height;
	if (height <= 0 || height > r->mixheight) {
		height = r->mixheight;
	}
	if ((width <= 0) || (height <= 0)) {
		SmokeLogLine(_T("render-validate: invalid dimensions"));
		return FALSE;
	}
	nonzero = 0;
	for (y=0; y<height; y++) {
		const DWORD *row = r->mixbuf + (y * r->mixwidth);
		for (x=0; x<width; x++) {
			if ((row[x] & 0x00ffffff) != 0) {
				nonzero++;
			}
		}
	}
	SmokeLogFormatDword(_T("render-validate-nonzero=%lu"), nonzero);
	if (nonzero == 0) {
		SmokeLogLine(_T("render-validate: blank mixbuf"));
	}
	return nonzero != 0;
}

static void SmokeLogRenderState()
{
	Render *render;
	Render::render_t *r;
	CRTC *crtc;
	const CRTC::crtc_t *c;
	VC *vc;
	const VC::vc_t *v;
	Sprite *sprite;
	Sprite::sprite_t spr;
	int page;

	render = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	if (!render) {
		SmokeLogLine(_T("render-state: missing render"));
		return;
	}

	r = render->GetWorkAddr();
	crtc = (CRTC*)::GetVM()->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	c = crtc ? crtc->GetWorkAddr() : NULL;
	vc = (VC*)::GetVM()->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	v = vc ? vc->GetWorkAddr() : NULL;
	sprite = (Sprite*)::GetVM()->SearchDevice(MAKEID('S', 'P', 'R', ' '));

	if (r) {
		TCHAR line[384];
		_sntprintf(line, _countof(line),
			_T("render-state: size=%dx%d hmul=%d vmul=%d low=%d mix=%dx%d page=%d type=%d len=%d text=%d bgsp=%d/%d grptype=%d grpen=%d,%d,%d,%d"),
			r->width, r->height, r->h_mul, r->v_mul,
			r->lowres ? 1 : 0, r->mixwidth, r->mixheight,
			r->mixpage, r->mixtype, r->mixlen, r->texten ? 1 : 0,
			r->bgspflag ? 1 : 0, r->bgspdisp ? 1 : 0, r->grptype,
			r->grpen[0] ? 1 : 0, r->grpen[1] ? 1 : 0,
			r->grpen[2] ? 1 : 0, r->grpen[3] ? 1 : 0);
		SmokeLogLine(line);
		for (page=0; page<4; page++) {
			if (r->grpen[page]) {
				SmokeLogRenderPage(r, page);
			}
		}
		SmokeLogRenderBuffer(_T("render-mixbuf"), r->mixbuf, r->mixwidth, r->mixlen, r->height);
	}
	if (v) {
		TCHAR line[256];
		_sntprintf(line, _countof(line),
			_T("vc-state: vr1=%02lX/%02lX vr2=%02lX/%02lX pri=%lu,%lu,%lu flags son=%d ton=%d gon=%d"),
			(unsigned long)v->vr1h, (unsigned long)v->vr1l,
			(unsigned long)v->vr2h, (unsigned long)v->vr2l,
			(unsigned long)v->sp, (unsigned long)v->tx, (unsigned long)v->gr,
			v->son ? 1 : 0, v->ton ? 1 : 0, v->gon ? 1 : 0);
		SmokeLogLine(line);
	}
	if (c) {
		TCHAR line[256];
		_sntprintf(line, _countof(line),
			_T("crtc-state: text=%lu,%lu grp0=%lu,%lu hmul=%d vmul=%d vscan=%d vcount=%lu vdisp=%d vblank=%d raster=%d"),
			(unsigned long)c->text_scrlx, (unsigned long)c->text_scrly,
			(unsigned long)c->grp_scrlx[0], (unsigned long)c->grp_scrly[0],
			c->h_mul, c->v_mul, c->v_scan, (unsigned long)c->v_count,
			c->v_disp ? 1 : 0, c->v_blank ? 1 : 0, c->raster_count);
		SmokeLogLine(line);
	}
	if (sprite) {
		TCHAR line[256];
		sprite->GetSprite(&spr);
		_sntprintf(line, _countof(line),
			_T("sprite-state: connect=%d disp=%d bg_on=%d,%d bg_scroll=%lu,%lu/%lu,%lu bg_size=%d"),
			spr.connect ? 1 : 0, spr.disp ? 1 : 0,
			spr.bg_on[0] ? 1 : 0, spr.bg_on[1] ? 1 : 0,
			(unsigned long)spr.bg_scrlx[0], (unsigned long)spr.bg_scrly[0],
			(unsigned long)spr.bg_scrlx[1], (unsigned long)spr.bg_scrly[1],
			spr.bg_size ? 1 : 0);
		SmokeLogLine(line);
	}
}

static LPCTSTR SmokeFindOption(LPCTSTR cmd, LPCTSTR opt, LPCTSTR after)
{
	LPCTSTR p = after ? after : cmd;
	return _tcsstr(p, opt);
}

BOOL SmokeReadOptionValue(LPCTSTR cmd, LPCTSTR opt, LPCTSTR *after, TCHAR *value, int valueCount)
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
//	Headless savestate smoke test
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::SmokeSaveState(LPCTSTR lpszCmd)
{
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
	BOOL saveRequested;

	fp = _tfopen(_T("C:\\tmp\\xm6_smoke_savestate.log"), _T("wt"));
	if (!fp) {
		fp = _tfopen(_T("xm6_smoke_savestate.log"), _T("wt"));
	}

#define SMOKE_LOG(msg) do { if (fp) { _ftprintf(fp, _T("%s\n"), _T(msg)); fflush(fp); } } while (0)
#define SMOKE_LOG1(fmt,a) do { if (fp) { _ftprintf(fp, _T(fmt) _T("\n"), a); fflush(fp); } } while (0)

	SetEnvironmentVariableA("XM6_SMOKE_SAVESTATE", "1");
	SMOKE_LOG("smoke: start");

	if (!SmokeReadOptionValue(lpszCmd, _T("--smoke-savestate"), NULL, szDisk, _countof(szDisk))) {
		SMOKE_LOG("smoke: invalid disk argument");
		if (fp) {
			fclose(fp);
		}
		return FALSE;
	}
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

	actionCount = SmokeParseActions(lpszCmd, actions, SmokeActionMax);
	runFrames = SmokeReadDwordOption(lpszCmd, _T("--smoke-run-frames"), 0);
	saveFrame = SmokeReadDwordOption(lpszCmd, _T("--smoke-save-frame"), 0xffffffff);
	saveRequested = (BOOL)(saveFrame != 0xffffffff);
	scheduled = (BOOL)((actionCount > 0) || (runFrames > 0) || saveRequested);
	saved = FALSE;

	if (saveRequested) {
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
	}
	else {
		SMOKE_LOG("save-frame=disabled");
	}

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
		for (i=0; i<actionCount; i++) {
			if ((actions[i].type == SmokeActionJoy) ||
				(actions[i].type == SmokeActionJoyAxis)) {
				SmokeEnsureJoyPort(ppi, actions[i].port);
			}
		}
		for (i=0; i<PPI::PortMax; i++) {
			const PPI::joyinfo_t *src = ppi->GetJoyInfo(i);
			if (src) {
				joy[i] = *src;
			}
		}

		maxActionFrame = 0;
		for (i=0; i<actionCount; i++) {
			if (actions[i].end > maxActionFrame) {
				maxActionFrame = actions[i].end;
			}
		}
		targetFrames = runFrames;
		if (saveRequested && (saveFrame > targetFrames)) {
			targetFrames = saveFrame;
		}
		if (maxActionFrame > targetFrames) {
			targetFrames = maxActionFrame;
		}

		SMOKE_LOG1("scheduled-actions=%d", actionCount);
		SMOKE_LOG1("run-frames=%lu", (unsigned long)targetFrames);
		if (saveRequested) {
			SMOKE_LOG1("save-frame=%lu", (unsigned long)saveFrame);
		}
		else {
			SMOKE_LOG("save-frame=disabled");
		}

		frames = 0;
		lastCount = crtc->GetDispCount();
		while (frames <= targetFrames) {
			for (i=0; i<actionCount; i++) {
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
					else if (actions[i].type == SmokeActionJoyAxis) {
						joy[actions[i].port].axis[actions[i].axis] = actions[i].axis_value;
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
					else if (actions[i].type == SmokeActionJoyAxis) {
						joy[actions[i].port].axis[actions[i].axis] = 0;
						ppi->SetJoyInfo(actions[i].port, &joy[actions[i].port]);
						SMOKE_LOG1("joy-up=%s", actions[i].name);
					}
				}
			}

			if (saveRequested && !saved && (frames == saveFrame)) {
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

		for (i=0; i<actionCount; i++) {
			if (actions[i].active) {
				actions[i].active = FALSE;
				if (actions[i].type == SmokeActionKey) {
					keyboard->BreakKey(actions[i].key);
				}
				else if (actions[i].type == SmokeActionJoy) {
					joy[actions[i].port].button[actions[i].button] = FALSE;
					ppi->SetJoyInfo(actions[i].port, &joy[actions[i].port]);
				}
				else if (actions[i].type == SmokeActionJoyAxis) {
					joy[actions[i].port].axis[actions[i].axis] = 0;
					ppi->SetJoyInfo(actions[i].port, &joy[actions[i].port]);
				}
			}
		}
	}

	if (fp) {
		fclose(fp);
		fp = NULL;
	}
	SmokeLogRenderState();
	if (!SmokeValidateRenderFrame()) {
		SmokeLogLine(_T("smoke: render probe blank"));
	}
	fp = _tfopen(_T("C:\\tmp\\xm6_smoke_savestate.log"), _T("at"));
	if (!fp) {
		fp = _tfopen(_T("xm6_smoke_savestate.log"), _T("at"));
	}

	if (!saveRequested) {
		SMOKE_LOG("quick-save skipped");
		SMOKE_LOG("quick-load skipped");
		SMOKE_LOG("smoke: ok");
		if (fp) {
			fclose(fp);
		}
		return TRUE;
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

	SmokeLogLine(_T("smoke-visible: start"));
	SetEnvironmentVariableA("XM6_SMOKE_SAVESTATE", "1");

	_tcsncpy(g_smokeVisibleCmd, lpszCmd, _countof(g_smokeVisibleCmd) - 1);
	g_smokeVisibleCmd[_countof(g_smokeVisibleCmd) - 1] = _T('\0');
	g_smokeVisibleActionCount = 0;
	g_smokeVisibleActionsParsed = FALSE;
	runFrames = SmokeReadDwordOption(lpszCmd, _T("--smoke-run-frames"), 0);
	saveFrame = SmokeReadDwordOption(lpszCmd, _T("--smoke-save-frame"), 0xffffffff);
	g_smokeVisibleSaveRequested = (BOOL)(saveFrame != 0xffffffff);
	g_smokeVisibleHoldMs = SmokeReadDwordOption(lpszCmd, _T("--smoke-visible-hold-ms"), 0);
	if (g_smokeVisibleSaveRequested) {
		UpdateStateFileName();
		if (!BuildQuickStatePath(g_smokeVisibleStatePath)) {
			SmokeLogLine(_T("smoke-visible: BuildQuickStatePath failed"));
			return FALSE;
		}
		SmokeLogFormat(_T("state-path=%s"), g_smokeVisibleStatePath.GetPath());
	}
	else {
		SmokeLogLine(_T("state-path=disabled"));
	}
	SmokeLogLine(_T("smoke-visible: action parse deferred"));

	g_smokeVisibleFirstActionFrame = 0xffffffff;
	g_smokeVisibleLastActionFrame = 0;
	g_smokeVisibleTargetFrames = runFrames;
	if (g_smokeVisibleSaveRequested && (saveFrame > g_smokeVisibleTargetFrames)) {
		g_smokeVisibleTargetFrames = saveFrame;
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
		int i;
		for (i=0; i<PPI::PortMax; i++) {
			int nButton;
			int nAxis;
			for (nButton=0; nButton<PPI::ButtonMax; nButton++) {
				GetInput()->SetSmokeJoyButton(i, nButton, FALSE);
			}
			for (nAxis=0; nAxis<PPI::AxisMax; nAxis++) {
				GetInput()->SetSmokeJoyAxis(i, nAxis, FALSE, 0);
			}
		}
	}
	{
		int i;
		for (i=0; i<PPI::PortMax; i++) {
			const PPI::joyinfo_t *src = ppi->GetJoyInfo(i);
			if (src) {
				g_smokeVisibleJoy[i] = *src;
			}
		}
	}
	g_smokeVisibleLastCount = crtc->GetDispCount();
	g_smokeVisibleRunStartTick = ::GetTickCount();
	g_smokeVisiblePollTick = g_smokeVisibleRunStartTick;
	SmokeLogLine(_T("smoke-visible: initial input state ready"));

	SmokeLogFormatDword(_T("run-frames=%lu"), g_smokeVisibleTargetFrames);
	if (g_smokeVisibleSaveRequested) {
		SmokeLogFormatDword(_T("save-frame=%lu"), g_smokeVisibleSaveFrame);
	}
	else {
		SmokeLogLine(_T("save-frame=disabled"));
	}
	SmokeLogFormatDword(_T("visible-hold-ms=%lu"), g_smokeVisibleHoldMs);

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
	deltaFrames = (curCount > g_smokeVisibleLastCount) ? (curCount - g_smokeVisibleLastCount) : 1;
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
			for (i=0; i<g_smokeVisibleActionCount; i++) {
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
			if (g_smokeVisibleLastActionFrame > g_smokeVisibleTargetFrames) {
				g_smokeVisibleTargetFrames = g_smokeVisibleLastActionFrame;
			}
			g_smokeVisibleActionsParsed = TRUE;
			SmokeLogFormatDword(_T("parsed-actions=%lu"), (DWORD)g_smokeVisibleActionCount);
			SmokeLogFormatDword(_T("run-frames=%lu"), g_smokeVisibleTargetFrames);
		}

		if ((g_smokeVisibleFrames >= g_smokeVisibleFirstActionFrame) &&
			(g_smokeVisibleFrames <= g_smokeVisibleLastActionFrame)) {
			for (i=0; i<g_smokeVisibleActionCount; i++) {
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
						g_smokeVisibleJoy[g_smokeVisibleActions[i].port].button[g_smokeVisibleActions[i].button] = TRUE;
						ppi->SetJoyInfo(g_smokeVisibleActions[i].port, &g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
						SmokeLogFormat(_T("joy-down=%s"), g_smokeVisibleActions[i].name);
					}
					else if (g_smokeVisibleActions[i].type == SmokeActionJoyAxis) {
						if (GetInput()) {
							GetInput()->SetSmokeJoyAxis(g_smokeVisibleActions[i].port,
								g_smokeVisibleActions[i].axis, TRUE, g_smokeVisibleActions[i].axis_value);
						}
						g_smokeVisibleJoy[g_smokeVisibleActions[i].port].axis[g_smokeVisibleActions[i].axis] =
							g_smokeVisibleActions[i].axis_value;
						ppi->SetJoyInfo(g_smokeVisibleActions[i].port, &g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
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
						g_smokeVisibleJoy[g_smokeVisibleActions[i].port].button[g_smokeVisibleActions[i].button] = FALSE;
						ppi->SetJoyInfo(g_smokeVisibleActions[i].port, &g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
						SmokeLogFormat(_T("joy-up=%s"), g_smokeVisibleActions[i].name);
					}
					else if (g_smokeVisibleActions[i].type == SmokeActionJoyAxis) {
						if (GetInput()) {
							GetInput()->SetSmokeJoyAxis(g_smokeVisibleActions[i].port,
								g_smokeVisibleActions[i].axis, FALSE, 0);
						}
						g_smokeVisibleJoy[g_smokeVisibleActions[i].port].axis[g_smokeVisibleActions[i].axis] = 0;
						ppi->SetJoyInfo(g_smokeVisibleActions[i].port, &g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
						SmokeLogFormat(_T("joy-up=%s"), g_smokeVisibleActions[i].name);
					}
				}
			}
		}

		if (g_smokeVisibleSaveRequested && !g_smokeVisibleSaved &&
			(g_smokeVisibleFrames >= g_smokeVisibleSaveFrame)) {
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
	if (!SmokeValidateRenderFrame()) {
		SmokeLogLine(_T("smoke-visible: render probe blank"));
	}

	for (i=0; i<g_smokeVisibleActionCount; i++) {
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
				g_smokeVisibleJoy[g_smokeVisibleActions[i].port].button[g_smokeVisibleActions[i].button] = FALSE;
				ppi->SetJoyInfo(g_smokeVisibleActions[i].port, &g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
			}
			else if (g_smokeVisibleActions[i].type == SmokeActionJoyAxis) {
				if (GetInput()) {
					GetInput()->SetSmokeJoyAxis(g_smokeVisibleActions[i].port,
						g_smokeVisibleActions[i].axis, FALSE, 0);
				}
				g_smokeVisibleJoy[g_smokeVisibleActions[i].port].axis[g_smokeVisibleActions[i].axis] = 0;
				ppi->SetJoyInfo(g_smokeVisibleActions[i].port, &g_smokeVisibleJoy[g_smokeVisibleActions[i].port]);
			}
		}
	}

	if (g_smokeVisibleSaveRequested && !g_smokeVisibleSaved) {
		SmokeLogLine(_T("quick-save begin"));
		OnSaveSub(g_smokeVisibleStatePath);
		g_smokeVisibleSaved = TRUE;
	}

	ok = TRUE;
	if (g_smokeVisibleSaveRequested) {
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
	}
	else {
		SmokeLogLine(_T("quick-save skipped"));
		SmokeLogLine(_T("quick-load skipped"));
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


#endif	// _WIN32
