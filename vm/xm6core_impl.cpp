//---------------------------------------------------------------------------
//
//	XM6 Core C API Implementation
//
//	Wrapper layer: translates the public C API (xm6core.h) into
//	calls to the internal C++ VM classes.
//
//---------------------------------------------------------------------------

#include <windows.h>
#include "os.h"
#include "xm6.h"
#if !defined(XM6CORE_EXPORTS) && !defined(XM6CORE_STATIC)
#define XM6CORE_STATIC
#endif
#include "xm6core.h"
#include "vm.h"
#include "device.h"
#include "schedule.h"
#include "cpu.h"
#include "memory.h"
#include "render.h"
#include "crtc.h"
#include "vc.h"
#include "keyboard.h"
#include "mouse.h"
#include "fdd.h"
#include "fdi.h"
#include "ppi.h"
#include "opmif.h"
#include "opm.h"
#include "adpcm.h"
#include "sasi.h"
#include "scsi.h"
#include "config.h"
#include "filepath.h"
#include "fileio.h"
#include <cstring>
#include <cstdarg>
#include <new>

//---------------------------------------------------------------------------
//
//	バ�Eジョン斁E���E
//
//---------------------------------------------------------------------------
static const char XM6CORE_VERSION[] = "XM6 Core 2.06";
static const unsigned int k_video_probe_frames_after_mode_change = 12u;

#ifndef XM6CORE_ENABLE_VIDEO_PROBE_LOG
#define XM6CORE_ENABLE_VIDEO_PROBE_LOG 0
#endif

//---------------------------------------------------------------------------
//
//	冁E��コンチE��スチE
//	Handle opaco que mantiene el estado de una instancia del emulador.
//
//---------------------------------------------------------------------------
struct XM6Context {
	VM *vm;

	// Cached device pointers (resolved once at create time)
	Scheduler *scheduler;
	Render *render;
	Keyboard *keyboard;
	Mouse *mouse;
	FDD *fdd;
	OPMIF *opmif;
	FM::OPM *opm_engine;
	ADPCM *adpcm;
	SASI *sasi;
	SCSI *scsi;
	PPI *ppi;

	// Audio mixing workspace
	DWORD *opm_buf;
	DWORD *adpcm_buf;
	unsigned int audio_rate;
	unsigned int audio_buf_frames;

	// Runtime config applied to the VM devices
	Config runtime_config;

	// Message callback (client-side)
	xm6_message_callback_t msg_callback;
	void *msg_user;

	// Internal video probe state
	unsigned int video_probe_signature;
	unsigned int video_probe_frames_remaining;
	unsigned int video_probe_frame_index;
	BOOL video_probe_has_signature;
};

//---------------------------------------------------------------------------
//
//	ヘルパ�E: コンチE��スト検証
//
//---------------------------------------------------------------------------
static inline XM6Context* ctx_from_handle(XM6Handle handle)
{
	return reinterpret_cast<XM6Context*>(handle);
}

static inline bool ctx_valid(XM6Context *ctx)
{
	return (ctx != NULL && ctx->vm != NULL);
}

static void emit_messagef(XM6Context *ctx, const char *format, ...)
{
	if (!ctx || !ctx->msg_callback || !format) {
		return;
	}

	char buffer[512];
	va_list args;
	va_start(args, format);
#if defined(_MSC_VER)
	_vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
#else
	vsnprintf(buffer, sizeof(buffer), format, args);
#endif
	va_end(args);
	buffer[sizeof(buffer) - 1] = '\0';
	ctx->msg_callback(buffer, ctx->msg_user);
}

static XM6Context *g_message_ctx = NULL;

void xm6_debug_message(const char *format, ...)
{
	if (!g_message_ctx || !g_message_ctx->msg_callback || !format) {
		return;
	}

	char buffer[512];
	va_list args;
	va_start(args, format);
#if defined(_MSC_VER)
	_vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
#else
	vsnprintf(buffer, sizeof(buffer), format, args);
#endif
	va_end(args);
	buffer[sizeof(buffer) - 1] = '\0';
	g_message_ctx->msg_callback(buffer, g_message_ctx->msg_user);
}

static unsigned long long query_file_size_bytes(const char *path)
{
	WIN32_FILE_ATTRIBUTE_DATA data;
	if (!path || !GetFileAttributesExA(path, GetFileExInfoStandard, &data)) {
		return 0;
	}
	return (static_cast<unsigned long long>(data.nFileSizeHigh) << 32) |
		static_cast<unsigned long long>(data.nFileSizeLow);
}

static const char* fdi_id_name(DWORD id)
{
	switch (id) {
	case MAKEID('D', 'I', 'M', ' '): return "DIM";
	case MAKEID('D', '6', '8', ' '): return "D68";
	case MAKEID('2', 'H', 'D', ' '): return "2HD";
	case MAKEID('2', 'D', 'D', ' '): return "2DD";
	case MAKEID('2', 'H', 'Q', ' '): return "2HQ";
	case MAKEID('B', 'A', 'D', ' '): return "BAD";
	case MAKEID('N', 'U', 'L', 'L'): return "NULL";
	default: return "UNKNOWN";
	}
}

static unsigned int hash_u32(unsigned int h, unsigned int v)
{
	h ^= v;
	h *= 16777619u;
	return h;
}

static unsigned int render_grpen_mask(const Render::render_t *r)
{
	unsigned int mask = 0u;
	if (!r) {
		return mask;
	}
	for (int i = 0; i < 4; ++i) {
		if (r->grpen[i]) {
			mask |= (1u << i);
		}
	}
	return mask;
}

static unsigned int vc_gs_mask(const VC::vc_t *v)
{
	unsigned int mask = 0u;
	if (!v) {
		return mask;
	}
	for (int i = 0; i < 4; ++i) {
		if (v->gs[i]) {
			mask |= (1u << i);
		}
	}
	return mask;
}

static void emit_video_probe_internal(XM6Context *ctx, const Render::render_t *r)
{
	if (!ctx || !r || !ctx->render) {
		return;
	}

	const VC::vc_t *v = NULL;
	if (ctx->vm) {
		Device *dev = ctx->vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
		if (dev && dev->GetID() == MAKEID('V', 'C', ' ', ' ')) {
			VC *vc = static_cast<VC*>(dev);
			v = vc->GetWorkAddr();
		}
	}

	const unsigned int grpen_mask = render_grpen_mask(r);
	const unsigned int gs_mask = vc_gs_mask(v);
	const unsigned int sp_pri = v ? (v->sp & 3u) : 0u;
	const unsigned int tx_pri = v ? (v->tx & 3u) : 0u;
	const unsigned int gr_pri = v ? (v->gr & 3u) : 0u;
	const unsigned int gp0 = v ? (v->gp[0] & 3u) : 0u;
	const unsigned int gp1 = v ? (v->gp[1] & 3u) : 0u;
	const unsigned int gp2 = v ? (v->gp[2] & 3u) : 0u;
	const unsigned int gp3 = v ? (v->gp[3] & 3u) : 0u;
	const unsigned int vr2h = v ? (v->vr2h & 0xffu) : 0u;
	const unsigned int vr2l = v ? (v->vr2l & 0xffu) : 0u;
	const unsigned int son = (v && v->son) ? 1u : 0u;
	const unsigned int ton = (v && v->ton) ? 1u : 0u;
	const unsigned int gon = (v && v->gon) ? 1u : 0u;
	const unsigned int exon = (v && v->exon) ? 1u : 0u;
	const unsigned int fast_fallback_count = (unsigned int)ctx->render->GetFastFallbackCount();

	unsigned int signature = 2166136261u;
	signature = hash_u32(signature, (unsigned int)r->width);
	signature = hash_u32(signature, (unsigned int)r->height);
	signature = hash_u32(signature, (unsigned int)r->h_mul);
	signature = hash_u32(signature, (unsigned int)r->v_mul);
	signature = hash_u32(signature, (unsigned int)(r->lowres ? 1u : 0u));
	signature = hash_u32(signature, (unsigned int)r->grptype);
	signature = hash_u32(signature, (unsigned int)r->mixpage);
	signature = hash_u32(signature, (unsigned int)r->mixtype);
	signature = hash_u32(signature, grpen_mask);
	signature = hash_u32(signature, gs_mask);
	signature = hash_u32(signature, sp_pri);
	signature = hash_u32(signature, tx_pri);
	signature = hash_u32(signature, gr_pri);
	signature = hash_u32(signature, gp0);
	signature = hash_u32(signature, gp1);
	signature = hash_u32(signature, gp2);
	signature = hash_u32(signature, gp3);
	signature = hash_u32(signature, vr2h);
	signature = hash_u32(signature, vr2l);
	signature = hash_u32(signature, son);
	signature = hash_u32(signature, ton);
	signature = hash_u32(signature, gon);
	signature = hash_u32(signature, exon);

	if (!ctx->video_probe_has_signature || signature != ctx->video_probe_signature) {
		ctx->video_probe_has_signature = TRUE;
		ctx->video_probe_signature = signature;
		ctx->video_probe_frames_remaining = k_video_probe_frames_after_mode_change;
		ctx->video_probe_frame_index = 0;

	/*	emit_messagef(ctx,
			"[video-probe-int] core=xm6 mode-change raw=%ux%u hm=%u vm=%u low=%d mode=%s",
			(unsigned int)r->width,
			(unsigned int)r->height,
			(unsigned int)r->h_mul,
			(unsigned int)r->v_mul,
			r->lowres ? 1 : 0,
			(ctx->render->GetCompositorMode() == 1) ? "fast" : "original"); */
	}

	if (ctx->video_probe_frames_remaining == 0) {
		return;
	}

#if XM6CORE_ENABLE_VIDEO_PROBE_LOG
	emit_messagef(ctx,
		"[video-probe-int] core=xm6 frame=%u/%u raw=%ux%u hm=%u vm=%u low=%d mode=%s grptype=%d mixpage=%d mixtype=%d pri=%u/%u/%u gp=%u,%u,%u,%u gs=%u,%u,%u,%u en=%u,%u,%u,%u grpen=%u,%u,%u,%u text=%d bgsp=%d,%d vr2=%02X/%02X ffb=%u",
		ctx->video_probe_frame_index + 1,
		k_video_probe_frames_after_mode_change,
		(unsigned int)r->width,
		(unsigned int)r->height,
		(unsigned int)r->h_mul,
		(unsigned int)r->v_mul,
		r->lowres ? 1 : 0,
		(ctx->render->GetCompositorMode() == 1) ? "fast" : "original",
		r->grptype,
		r->mixpage,
		r->mixtype,
		sp_pri,
		tx_pri,
		gr_pri,
		gp0,
		gp1,
		gp2,
		gp3,
		(gs_mask >> 0) & 1u,
		(gs_mask >> 1) & 1u,
		(gs_mask >> 2) & 1u,
		(gs_mask >> 3) & 1u,
		son,
		ton,
		gon,
		exon,
		(grpen_mask >> 0) & 1u,
		(grpen_mask >> 1) & 1u,
		(grpen_mask >> 2) & 1u,
		(grpen_mask >> 3) & 1u,
		r->texten ? 1 : 0,
		r->bgspflag ? 1 : 0,
		r->bgspdisp ? 1 : 0,
		vr2h,
		vr2l,
		fast_fallback_count);
#endif

	ctx->video_probe_frame_index++;
	ctx->video_probe_frames_remaining--;
}

static void reset_video_probe_state(XM6Context *ctx)
{
	if (!ctx) {
		return;
	}
	ctx->video_probe_signature = 0;
	ctx->video_probe_frames_remaining = 0;
	ctx->video_probe_frame_index = 0;
	ctx->video_probe_has_signature = FALSE;
}

static void apply_runtime_config(XM6Context *ctx)
{
	ctx->vm->ApplyCfg(&ctx->runtime_config);
}

static void apply_default_runtime_config(XM6Context *ctx)
{
	Config *config = &ctx->runtime_config;
	std::memset(config, 0, sizeof(*config));

	config->system_clock = 0;
	config->mpu_fullspeed = FALSE;
	config->vm_fullspeed = FALSE;
	config->ram_size = 5;
	config->ram_sramsync = TRUE;
	config->mem_type = Memory::SASI;

	config->sample_rate = 5;
	config->master_volume = 100;
	config->fm_volume = 54;
	config->adpcm_volume = 52;
	config->fm_enable = TRUE;
	config->adpcm_enable = TRUE;
	config->kbd_connect = TRUE;
	config->mouse_speed = 205;
	config->mouse_port = 0;
	config->mouse_swap = FALSE;
	config->mouse_mid = TRUE;

	config->joy_type[0] = 1;
	config->joy_type[1] = 0;

	config->sasi_drives = 0;
	config->sasi_sramsync = TRUE;
	config->sxsi_drives = 0;
	config->scsi_ilevel = 1;
	config->scsi_drives = 0;
	config->scsi_sramsync = TRUE;
	config->sasi_parity = TRUE;

	apply_runtime_config(ctx);
}

static void post_state_load_sync(XM6Context *ctx)
{
	if (!ctx_valid(ctx)) {
		return;
	}

	apply_runtime_config(ctx);

	if (ctx->opmif) {
		ctx->opmif->SetEngine(ctx->opm_engine);
		ctx->opmif->EnableFM(ctx->runtime_config.fm_enable);
		if (ctx->audio_rate != 0) {
			ctx->opmif->InitBuf((DWORD)ctx->audio_rate);
		}
	}
	if (ctx->opm_engine) {
		ctx->opm_engine->SetVolume(ctx->runtime_config.fm_volume);
	}
	if (ctx->adpcm) {
		ctx->adpcm->EnableADPCM(ctx->runtime_config.adpcm_enable);
		ctx->adpcm->SetVolume(ctx->runtime_config.adpcm_volume);
		if (ctx->audio_rate != 0) {
			ctx->adpcm->InitBuf((DWORD)ctx->audio_rate);
		}
	}
	if (ctx->render) {
		ctx->render->Complete();
	}

	reset_video_probe_state(ctx);
}

static bool create_temp_state_filepath(Filepath *out_path)
{
	TCHAR temp_dir[_MAX_PATH];
	TCHAR temp_file[_MAX_PATH];
	UINT len;

	if (!out_path) {
		return false;
	}

	len = ::GetTempPath(_MAX_PATH, temp_dir);
	if (len == 0 || len > (_MAX_PATH - 1)) {
		return false;
	}

	if (::GetTempFileName(temp_dir, _T("xm6"), 0, temp_file) == 0) {
		return false;
	}

	out_path->SetPath(temp_file);
	return true;
}

static void delete_temp_state_file(const Filepath& path)
{
	if (!path.IsClear()) {
		::DeleteFile(path.GetPath());
	}
}

//===========================================================================
//
//	Paso 1: Version + Lifecycle
//
//===========================================================================

//---------------------------------------------------------------------------
//	xm6_get_version  EDevuelve la cadena de versión del core.
//---------------------------------------------------------------------------
XM6CORE_API const char* XM6CORE_CALL xm6_get_version(void)
{
	return XM6CORE_VERSION;
}

//---------------------------------------------------------------------------
//	xm6_create  ECrea una nueva instancia del emulador.
//
//	Instancia internamente la VM y todos sus dispositivos, luego cachea
//	los punteros a los subsistemas que el API necesita exponer.
//	Retorna NULL en caso de fallo.
//---------------------------------------------------------------------------
XM6CORE_API XM6Handle XM6CORE_CALL xm6_create(void)
{
	// Alocar contexto
	XM6Context *ctx = new(std::nothrow) XM6Context();
	if (!ctx) {
		return NULL;
	}
	memset(ctx, 0, sizeof(XM6Context));

	// Crear e inicializar la VM
	ctx->vm = new(std::nothrow) VM();
	if (!ctx->vm) {
		delete ctx;
		return NULL;
	}

	if (!ctx->vm->Init()) {
		delete ctx->vm;
		delete ctx;
		return NULL;
	}

	// Mantener el formato de savestate alineado con la version moderna del core.
	ctx->vm->SetVersion(0x02, 0x06);

	// Cachear punteros a dispositivos vía SearchDevice
	ctx->scheduler = (Scheduler*)ctx->vm->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ctx->render    = (Render*)ctx->vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ctx->keyboard  = (Keyboard*)ctx->vm->SearchDevice(MAKEID('K', 'E', 'Y', 'B'));
	ctx->mouse     = (Mouse*)ctx->vm->SearchDevice(MAKEID('M', 'O', 'U', 'S'));
	ctx->fdd       = (FDD*)ctx->vm->SearchDevice(MAKEID('F', 'D', 'D', ' '));
	ctx->opmif     = (OPMIF*)ctx->vm->SearchDevice(MAKEID('O', 'P', 'M', ' '));
	ctx->adpcm     = (ADPCM*)ctx->vm->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
	ctx->sasi      = (SASI*)ctx->vm->SearchDevice(MAKEID('S', 'A', 'S', 'I'));
	ctx->scsi      = (SCSI*)ctx->vm->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ctx->ppi       = (PPI*)ctx->vm->SearchDevice(MAKEID('P', 'P', 'I', ' '));

	apply_default_runtime_config(ctx);
	reset_video_probe_state(ctx);
	g_message_ctx = ctx;

	return reinterpret_cast<XM6Handle>(ctx);
}

//---------------------------------------------------------------------------
//	xm6_destroy  EDestruye una instancia del emulador.
//
//	Libera todos los recursos asociados (VM, dispositivos, buffers de
//	audio). Acepta handle NULL sin efecto.
//---------------------------------------------------------------------------
XM6CORE_API void XM6CORE_CALL xm6_destroy(XM6Handle handle)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx) {
		return;
	}

	// Detach the external OPM engine before VM cleanup destroys cached devices.
	if (ctx->opmif) {
		ctx->opmif->SetEngine(NULL);
	}

	if (ctx->vm) {
		ctx->vm->Cleanup();
		delete ctx->vm;
		ctx->vm = NULL;
	}

	ctx->scheduler = NULL;
	ctx->render = NULL;
	ctx->keyboard = NULL;
	ctx->mouse = NULL;
	ctx->fdd = NULL;
	ctx->opmif = NULL;
	ctx->adpcm = NULL;
	ctx->sasi = NULL;
	ctx->scsi = NULL;
	ctx->ppi = NULL;

	delete ctx->opm_engine;
	ctx->opm_engine = NULL;

	delete[] ctx->opm_buf;
	ctx->opm_buf = NULL;
	delete[] ctx->adpcm_buf;
	ctx->adpcm_buf = NULL;

	if (g_message_ctx == ctx) {
		 g_message_ctx = NULL;
	}
	delete ctx;
}

//===========================================================================
//
//	Paso 2: Control y ejecución
//
//===========================================================================

//---------------------------------------------------------------------------
//	Trampoline: adapta la convención FASTCALL del callback interno de la
//	VM a la convención __cdecl del API público.
//	TCHAR == char en MultiByte, por lo que no se necesita conversión.
//---------------------------------------------------------------------------
static void FASTCALL xm6_msg_trampoline(const TCHAR* message, void *user)
{
	XM6Context *ctx = reinterpret_cast<XM6Context*>(user);
	if (ctx && ctx->msg_callback) {
		ctx->msg_callback(message, ctx->msg_user);
	}
}

static void xm6_log_state_probe(XM6Context *ctx, const char *tag, const void *buffer, unsigned int size)
{
	char msg[160];
	unsigned long first_id = 0;

	if (!ctx || !ctx->msg_callback) {
		return;
	}

	if (buffer && size >= 20) {
		const unsigned char *bytes = reinterpret_cast<const unsigned char*>(buffer);
		first_id =
			((unsigned long)bytes[16]) |
			((unsigned long)bytes[17] << 8) |
			((unsigned long)bytes[18] << 16) |
			((unsigned long)bytes[19] << 24);
	}

	wsprintfA(
		msg,
		"[xm6core] %s size=%u first_id=%08lX",
		tag,
		(unsigned int)size,
		first_id);
	ctx->msg_callback(msg, ctx->msg_user);
}

//---------------------------------------------------------------------------
//	xm6_set_message_callback  ERegistra un callback para mensajes del core.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_set_message_callback(
	XM6Handle handle, xm6_message_callback_t callback, void* user)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->msg_callback = callback;
	ctx->msg_user = user;

	// Registrar el trampoline en la VM, pasando el contexto como user_data
	ctx->vm->SetHostMessageCallback(xm6_msg_trampoline, ctx);

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_set_system_dir - Configura el directorio base para BIOS/ROM.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_set_system_dir(const char* system_dir)
{
	if (!system_dir || system_dir[0] == '\0') {
		Filepath::ClearSystemDir();
		return XM6CORE_OK;
	}

	Filepath::SetSystemDir(system_dir);
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_exec  EEjecuta la VM por 'hus' unidades de tiempo (0.5µs cada una).
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_exec(XM6Handle handle, unsigned int hus)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	BOOL result = ctx->vm->Exec((DWORD)hus);
	return result ? XM6CORE_OK : XM6CORE_ERR_NOT_READY;
}

//---------------------------------------------------------------------------
//	xm6_exec_to_frame  EEjecuta la VM hasta que haya un frame de video listo.
//
//	Avanza el tiempo (hus) en chunks pequeños hasta que Render::IsReady()
//	sea true. Sirve como base para el retro_run() de libretro.
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//	xm6_exec_events_only  EAvanza eventos/scheduler sin ejecutar CPU.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_exec_events_only(XM6Handle handle, unsigned int hus)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->scheduler) {
		return XM6CORE_ERR_NOT_READY;
	}

	ctx->scheduler->ExecEventsOnly((DWORD)hus);
	return XM6CORE_OK;
}

//----------------------------------------------------------------------------
//	xm6_exec_mpu_nowait_step
//
//	Replica el hack "No Wait Operation with MPU" del frontend MFC:
//	ejecuta CPU->Exec(GetCPUSpeed()) pero restaura el contador de ciclos del
//	scheduler para no avanzar el tiempo del sistema.
//----------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_exec_mpu_nowait_step(XM6Handle handle)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (!ctx->scheduler) {
		return XM6CORE_ERR_NOT_READY;
	}

	CPU *cpu = (CPU*)ctx->vm->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	if (!cpu) {
		return XM6CORE_ERR_NOT_READY;
	}

	int backup_cycle = ctx->scheduler->GetCPUCycle();
	cpu->Exec((int)ctx->scheduler->GetCPUSpeed());
	ctx->scheduler->SetCPUCycle(backup_cycle);

	return XM6CORE_OK;
}

XM6CORE_API int XM6CORE_CALL xm6_exec_to_frame(XM6Handle handle)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->render) {
		return XM6CORE_ERR_NOT_READY;
	}

	// Un frame a 55Hz son ~36000 hus. Ejecutar en pasos de 1000 hus.
	// Poner un límite de seguridad (e.g. 10 frames ≁E360000 hus) para
	// evitar loops infinitos si el VBlank está deshabilitado.
	const DWORD CHUNK = 1000;
	const DWORD MAX_HUS = 360000;
	DWORD total = 0;

	while (total < MAX_HUS) {
		ctx->vm->Exec(CHUNK);
		total += CHUNK;

		if (ctx->render->IsReady()) {
			break;
		}
	}

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_reset  EResetea la VM (equivalente a pulsar el botón de reset).
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_reset(XM6Handle handle)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->vm->Reset();
	reset_video_probe_state(ctx);
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_set_power  EEnciende o apaga la VM.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_set_power(XM6Handle handle, int enabled)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->vm->SetPower(enabled ? TRUE : FALSE);
	if (!enabled) {
		reset_video_probe_state(ctx);
	}
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_get_power  EConsulta el estado de alimentación de la VM.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_get_power(XM6Handle handle)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	return ctx->vm->IsPower() ? 1 : 0;
}

//---------------------------------------------------------------------------
//	xm6_set_power_switch  EControla el interruptor de encendido.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_set_power_switch(XM6Handle handle, int enabled)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->vm->PowerSW(enabled ? TRUE : FALSE);
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_get_power_switch  EConsulta el estado del interruptor de encendido.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_get_power_switch(XM6Handle handle)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	return ctx->vm->IsPowerSW() ? 1 : 0;
}

//---------------------------------------------------------------------------
//	xm6_get_vm_version  EObtiene la versión interna de la VM (major.minor).
//---------------------------------------------------------------------------
XM6CORE_API void XM6CORE_CALL xm6_get_vm_version(
	XM6Handle handle, unsigned int* out_major, unsigned int* out_minor)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx) || !out_major || !out_minor) {
		if (out_major) *out_major = 0;
		if (out_minor) *out_minor = 0;
		return;
	}

	DWORD major = 0, minor = 0;
	ctx->vm->GetVersion(major, minor);
	*out_major = (unsigned int)major;
	*out_minor = (unsigned int)minor;
}

//===========================================================================
//
//	Paso 3: Medios (FDD / HDD)
//
//===========================================================================

//---------------------------------------------------------------------------
//	xm6_mount_fdd  EMonta una imagen de disquete en un drive (0-3).
//
//	media_hint: tipo de medio (0 = auto-detect, ver FDD::Open).
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_mount_fdd(
	XM6Handle handle, int drive, const char* image_path, int media_hint)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->fdd || drive < 0 || drive > 3 || !image_path) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	Filepath path;
	path.SetPath(image_path);

	emit_messagef(ctx,
		"[xm6core] mount_fdd drive=%d media_hint=%d size=%llu path=%s",
		drive, media_hint, query_file_size_bytes(image_path), image_path);

	if (!ctx->fdd->Open(drive, path, media_hint)) {
		emit_messagef(ctx,
			"[xm6core] mount_fdd open failed drive=%d media_hint=%d path=%s",
			drive, media_hint, image_path);
		return XM6CORE_ERR_IO;
	}

	FDI *fdi = ctx->fdd->GetFDI(drive);
	emit_messagef(ctx,
		"[xm6core] mount_fdd open ok drive=%d media=%d inserted=%d fdi_id=0x%08lx parser=%s",
		drive,
		fdi ? fdi->GetMedia() : -1,
		fdi ? 1 : 0,
		(unsigned long)(fdi ? fdi->GetID() : MAKEID('N', 'U', 'L', 'L')),
		fdi ? fdi_id_name(fdi->GetID()) : "NULL");

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_eject_fdd  EExpulsa la imagen de un drive FDD.
//
//	force: si != 0, fuerza la expulsión aunque el motor esté activo.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_eject_fdd(
	XM6Handle handle, int drive, int force)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->fdd || drive < 0 || drive > 3) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	ctx->fdd->Eject(drive, force ? TRUE : FALSE);
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_fdd_is_inserted  EVerifica si hay un disco insertado en el drive.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_fdd_is_inserted(XM6Handle handle, int drive)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return 0; // Invalid = false
	}

	if (!ctx->fdd || drive < 0 || drive > 3) {
		return 0;
	}

	// Si GetFDI devuelve un puntero no nulo, hay un disco (o al menos un objeto de imagen)
	// También podríamos verificar drv_t::insert, pero GetFDI es la forma más directa
	return ctx->fdd->GetFDI(drive) != NULL ? 1 : 0;
}

//---------------------------------------------------------------------------
//	xm6_fdd_get_name  EObtiene la ruta del archivo del disco montado.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_fdd_get_name(
	XM6Handle handle, int drive, char* out_name, unsigned int max_len)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->fdd || drive < 0 || drive > 3 || !out_name || max_len == 0) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	out_name[0] = '\0';

	if (ctx->fdd->GetFDI(drive) == NULL) {
		return XM6CORE_ERR_NOT_READY; // Empty drive
	}

	Filepath path;
	ctx->fdd->GetPath(drive, path);

	const char* path_str = path.GetPath(); // assuming TCHAR == char, validated before
	if (path_str) {
		strncpy(out_name, path_str, max_len - 1);
		out_name[max_len - 1] = '\0';
		return XM6CORE_OK;
	}

	return XM6CORE_ERR_IO;
}

//---------------------------------------------------------------------------
//	xm6_mount_sasi_hdd  EMonta una imagen de disco duro SASI.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_mount_sasi_hdd(
	XM6Handle handle, int slot, const char* image_path)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->sasi || slot < 0 || slot >= 16 || !image_path) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	Filepath path;
	path.SetPath(image_path);

	int i;

	ctx->runtime_config.mem_type = Memory::SASI;
	ctx->runtime_config.scsi_drives = 0;
	ctx->runtime_config.sxsi_drives = 0;
	for (i=0; i<5; i++) {
		ctx->runtime_config.scsi_file[i][0] = _T('\0');
	}
	if (ctx->runtime_config.sasi_drives <= slot) {
		ctx->runtime_config.sasi_drives = slot + 1;
	}
	::lstrcpyn(ctx->runtime_config.sasi_file[slot], path.GetPath(), FILEPATH_MAX);
	apply_runtime_config(ctx);

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_mount_scsi_hdd  EMonta una imagen de disco duro SCSI.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_mount_scsi_hdd(
	XM6Handle handle, int slot, const char* image_path)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->scsi || slot < 0 || slot >= 5 || !image_path) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	Filepath path;
	int required_drives;
	path.SetPath(image_path);

	int i;

	ctx->runtime_config.mem_type = Memory::SCSIExt;
	ctx->runtime_config.sasi_drives = 0;
	for (i=0; i<16; i++) {
		ctx->runtime_config.sasi_file[i][0] = _T('\0');
	}
	required_drives = (slot == 0) ? 1 : (slot + 3);
	if (ctx->runtime_config.scsi_drives < required_drives) {
		ctx->runtime_config.scsi_drives = required_drives;
	}
	::lstrcpyn(ctx->runtime_config.scsi_file[slot], path.GetPath(), FILEPATH_MAX);
	apply_runtime_config(ctx);

	return XM6CORE_OK;
}

//===========================================================================
//
//	Paso 4: Input (Keyboard, Mouse, Joystick)
//
//===========================================================================

//---------------------------------------------------------------------------
//	xm6_input_key  EEnvía un evento de tecla al emulador.
//
//	xm6_keycode: código de tecla del X68000 (0x00-0x7F).
//	pressed: 1 = key down, 0 = key up.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_input_key(
	XM6Handle handle, unsigned int xm6_keycode, int pressed)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->keyboard || xm6_keycode > 0x7F) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	if (pressed) {
		ctx->keyboard->MakeKey((DWORD)xm6_keycode);
	} else {
		ctx->keyboard->BreakKey((DWORD)xm6_keycode);
	}

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_input_mouse  EEstablece el estado del ratón.
//
//	x, y: posición absoluta o relativa (según la convención del emulador).
//	left, right: 1 = botón pulsado, 0 = suelto.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_input_mouse(
	XM6Handle handle, int x, int y, int left, int right)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->mouse) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	ctx->mouse->SetMouse(x, y, left ? TRUE : FALSE, right ? TRUE : FALSE);
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_input_mouse_reset  EReinicia los datos acumulados del ratón.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_input_mouse_reset(XM6Handle handle)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->mouse) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	ctx->mouse->ResetMouse();
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_input_joy  EEstablece el estado de un joystick/gamepad.
//
//	port: puerto del joystick (0 o 1).
//	axes[4]: estado de cada eje (0=centro, 1=positivo, 2=negativo).
//	buttons[8]: estado de cada botón (1=pulsado, 0=suelto).
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_input_joy(
	XM6Handle handle, int port,
	const unsigned int axes[4], const int buttons[8])
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->ppi || port < 0 || port >= PPI::PortMax || !axes || !buttons) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	// Convertir arrays del API a joyinfo_t interna
	PPI::joyinfo_t info;
	int i;
	for (i = 0; i < PPI::AxisMax; i++) {
		info.axis[i] = (DWORD)axes[i];
	}
	for (i = 0; i < PPI::ButtonMax; i++) {
		info.button[i] = buttons[i] ? TRUE : FALSE;
	}

	ctx->ppi->SetJoyInfo(port, &info);
	return XM6CORE_OK;
}

//===========================================================================
//
//	Paso 5: Video
//
//===========================================================================

//---------------------------------------------------------------------------
//	xm6_video_poll  EConsulta si hay un frame de video disponible.
//
//	Si hay un frame listo, rellena out_frame con el puntero al buffer
//	ARGB32 interno (no copia), las dimensiones y el stride.
//	El puntero es válido hasta la siguiente llamada a xm6_exec().
//	Retorna XM6CORE_OK si hay frame, XM6CORE_ERR_NOT_READY si no.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_video_poll(
	XM6Handle handle, xm6_video_frame_t* out_frame)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->render || !out_frame) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	// Verificar si el Render tiene un frame listo
	if (!ctx->render->IsReady()) {
		return XM6CORE_ERR_NOT_READY;
	}

	// Obtener el workspace del Render
	Render::render_t *r = ctx->render->GetWorkAddr();
	if (!r || !r->mixbuf) {
		return XM6CORE_ERR_NOT_READY;
	}

	if (r->width <= 0 || r->height <= 0 || r->mixwidth <= 0 || r->mixheight <= 0) {
		return XM6CORE_ERR_NOT_READY;
	}

	unsigned int visible_w = (unsigned int)r->width;
	unsigned int visible_h = (unsigned int)r->height;
	unsigned int stride = (unsigned int)r->mixwidth;
	unsigned int max_h = (unsigned int)r->mixheight;

	if (visible_w > stride) {
		visible_w = stride;
	}
	if (visible_h > max_h) {
		visible_h = max_h;
	}
	if (visible_w == 0 || visible_h == 0) {
		return XM6CORE_ERR_NOT_READY;
	}

	out_frame->pixels_argb32 = (const unsigned int*)r->mixbuf;
	out_frame->width = visible_w;
	out_frame->height = visible_h;
	out_frame->stride_pixels = stride;

	emit_video_probe_internal(ctx, r);

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_video_consume  EMarca el frame actual como consumido.
//
//	Debe llamarse después de xm6_video_poll exitoso, cuando el frontend
//	haya terminado de copiar o mostrar el buffer de video.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_video_consume(XM6Handle handle)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->render) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	ctx->render->Complete();
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_get_video_refresh_hz - Obtiene la frecuencia vertical actual.
//
//	Devuelve la tasa real calculada por CRTC::GetHVHz() en Hz.
//	Permite al frontend adaptar audio/video cuando el modo cambia entre
//	15kHz (~61.46Hz) y 31kHz (~55.46Hz).
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_get_video_refresh_hz(XM6Handle handle, double *out_hz)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!out_hz) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	CRTC *crtc = (CRTC*)ctx->vm->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	if (!crtc) {
		return XM6CORE_ERR_NOT_READY;
	}

	DWORD h = 0;
	DWORD v = 0;
	crtc->GetHVHz(&h, &v);
	if (v == 0) {
		return XM6CORE_ERR_NOT_READY;
	}

	*out_hz = (double)v / 100.0;
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_get_video_layout - Obtiene layout de video activo del render.
//
//	Expone ancho/alto de buffer visible y multiplicadores de presentación
//	(h_mul/v_mul) para que el frontend ajuste geometría/aspecto.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_get_video_layout(
	XM6Handle handle,
	unsigned int *out_width,
	unsigned int *out_height,
	unsigned int *out_h_mul,
	unsigned int *out_v_mul,
	int *out_lowres)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!out_width || !out_height || !out_h_mul || !out_v_mul || !out_lowres) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	if (!ctx->render) {
		return XM6CORE_ERR_NOT_READY;
	}

	Render::render_t *r = ctx->render->GetWorkAddr();
	if (!r || r->width <= 0 || r->height <= 0) {
		return XM6CORE_ERR_NOT_READY;
	}

	unsigned int h_mul = (r->h_mul > 0) ? (unsigned int)r->h_mul : 1u;
	unsigned int v_mul = 1u;
	if (r->v_mul == 0 || r->v_mul == 1 || r->v_mul == 2) {
		v_mul = (unsigned int)r->v_mul;
	}

	*out_width = (unsigned int)r->width;
	*out_height = (unsigned int)r->height;
	*out_h_mul = h_mul;
	*out_v_mul = v_mul;
	*out_lowres = r->lowres ? 1 : 0;

	return XM6CORE_OK;
}

//===========================================================================
//
//	Paso 6: Audio
//
//===========================================================================

//---------------------------------------------------------------------------
//	Helper: saturación int32 ↁEint16 (replica SoundMMXPortable)
//---------------------------------------------------------------------------
static inline short saturate_s16(int value)
{
	if (value > 32767) return 32767;
	if (value < -32768) return -32768;
	return (short)value;
}

//---------------------------------------------------------------------------
//	xm6_audio_configure  EConfigura el sistema de audio.
//
//	sample_rate: frecuencia en Hz (ej: 44100, 48000).
//	Inicializa los buffers internos de OPM y ADPCM.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_audio_configure(
	XM6Handle handle, unsigned int sample_rate)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->opmif || !ctx->adpcm || sample_rate == 0) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	// Inicializar buffers de síntesis en OPM y ADPCM
	if (ctx->opmif) {
		ctx->opmif->SetEngine(NULL);
	}
	delete ctx->opm_engine;
	ctx->opm_engine = new(std::nothrow) FM::OPM;
	if (!ctx->opm_engine) {
		return XM6CORE_ERR_INIT_FAILED;
	}
	ctx->opm_engine->Init(4000000, sample_rate, true);
	ctx->opm_engine->Reset();
	ctx->opm_engine->SetVolume(ctx->runtime_config.fm_volume);

	ctx->opmif->InitBuf((DWORD)sample_rate);
	ctx->opmif->SetEngine(ctx->opm_engine);
	ctx->opmif->EnableFM(TRUE);
	ctx->adpcm->InitBuf((DWORD)sample_rate);
	ctx->adpcm->EnableADPCM(TRUE);
	ctx->adpcm->SetVolume(ctx->runtime_config.adpcm_volume);

	// Realocar buffer temporal de mezcla (stereo: 2 DWORDs por frame)
	// Tamaño máximo razonable: 1 segundo de audio
	unsigned int max_frames = sample_rate;
	delete[] ctx->opm_buf;
	ctx->opm_buf = new(std::nothrow) DWORD[max_frames * 2];
	if (!ctx->opm_buf) {
		return XM6CORE_ERR_INIT_FAILED;
	}

	ctx->audio_rate = sample_rate;
	ctx->audio_buf_frames = max_frames;

	// No necesitamos buffer separado de ADPCM:
	// ADPCM::GetBuf y SCSI::GetBuf suman al mismo buffer
	delete[] ctx->adpcm_buf;
	ctx->adpcm_buf = NULL;

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_audio_mix  EMezcla audio OPM + ADPCM + SCSI CD-DA.
//
//	Replica la lógica de CSound::Process de mfc_snd.cpp:
//	1. OPMIF::ProcessBuf()  ↁEframes disponibles
//	2. OPMIF::GetBuf()      ↁEescribe al buffer temporal (DWORD stereo)
//	3. ADPCM::GetBuf()      ↁEsuma al mismo buffer
//	4. SCSI::GetBuf()       ↁEsuma CD-DA al mismo buffer
//	5. Clamp int32 ↁEint16  ↁEsalida interleaved stereo (L,R,L,R,...)
//
//	out_interleaved_stereo: buffer del cliente (short, L/R interleaved)
//	frames: número máximo de frames solicitados
//	out_frames: frames realmente escritos
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_audio_mix(
	XM6Handle handle,
	short* out_interleaved_stereo,
	unsigned int frames,
	unsigned int* out_frames)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->opmif || !ctx->adpcm || !out_interleaved_stereo || !out_frames) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	if (!ctx->opm_buf || ctx->audio_rate == 0) {
		*out_frames = 0;
		return XM6CORE_ERR_NOT_READY;
	}

	// Limitar a la capacidad del buffer temporal
	if (frames > ctx->audio_buf_frames) {
		frames = ctx->audio_buf_frames;
	}

	// OPM: procesar y obtener samples disponibles
	DWORD ready = ctx->opmif->ProcessBuf();

	// Limpiar buffer temporal
	memset(ctx->opm_buf, 0, frames * 2 * sizeof(DWORD));

	// Igual que frontend MFC: pedir frames y luego recortar a ready.
	ctx->opmif->GetBuf(ctx->opm_buf, (int)frames);
	if (ready < frames) {
		frames = ready;
	}

	// ADPCM: sumar al mismo buffer
	ctx->adpcm->GetBuf(ctx->opm_buf, (int)frames);

	// ADPCM: sincronizacion
	if (ready > frames) {
		ctx->adpcm->Wait((int)(ready - frames));
	} else {
		ctx->adpcm->Wait(0);
	}

	// SCSI CD-DA: sumar al mismo buffer
	if (ctx->scsi) {
		ctx->scsi->GetBuf(ctx->opm_buf, (int)frames, (DWORD)ctx->audio_rate);
	}

	// Convertir int32 stereo -> int16 stereo interleaved
	const int *src = (const int*)ctx->opm_buf;
	unsigned int total_samples = frames * 2;  // L + R
	const int master = (ctx->runtime_config.master_volume < 0) ? 0 :
		(ctx->runtime_config.master_volume > 100 ? 100 : ctx->runtime_config.master_volume);
	for (unsigned int i = 0; i < total_samples; i++) {
		int mixed = src[i];
		if (master != 100) {
			mixed = (mixed * master) / 100;
		}
		out_interleaved_stereo[i] = saturate_s16(mixed);
	}

	*out_frames = frames;
	return XM6CORE_OK;
}

//===========================================================================
//
//	Paso 7: Save / Load State
//
//===========================================================================

//---------------------------------------------------------------------------
//	xm6_save_state - Guarda el estado completo de la VM a un archivo.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_save_state(
	XM6Handle handle, const char* state_path)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!state_path) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	Filepath path;
	path.SetPath(state_path);

	DWORD result = ctx->vm->Save(path);
	return (result != 0) ? XM6CORE_OK : XM6CORE_ERR_IO;
}

//---------------------------------------------------------------------------
//	xm6_load_state - Carga el estado completo de la VM desde un archivo.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_load_state(
	XM6Handle handle, const char* state_path)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!state_path) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	Filepath path;
	path.SetPath(state_path);

	DWORD result = ctx->vm->Load(path);
	if (result != 0) {
		post_state_load_sync(ctx);
	}
	return (result != 0) ? XM6CORE_OK : XM6CORE_ERR_IO;
}

//---------------------------------------------------------------------------
//	xm6_state_size - Calcula el tamano necesario para guardar el estado.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_state_size(XM6Handle handle, unsigned int *out_size)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (!out_size) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	*out_size = 0;

	Fileio fio;
	if (!fio.OpenMemoryWriteDynamic()) {
		return XM6CORE_ERR_IO;
	}

	DWORD res = ctx->vm->Save(fio);
	fio.Close();
	if (res == 0) {
		return XM6CORE_ERR_IO;
	}

	*out_size = (unsigned int)res;
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_save_state_mem - Guarda el estado a un buffer en memoria.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_save_state_mem(XM6Handle handle, void *buffer, unsigned int size)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (!buffer || size == 0) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	Fileio fio;
	if (!fio.OpenMemoryWriteDynamic()) {
		return XM6CORE_ERR_IO;
	}

	DWORD res = ctx->vm->Save(fio);
	if (res == 0) {
		fio.Close();
		return XM6CORE_ERR_IO;
	}
	if (res > size) {
		fio.Close();
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	::memcpy(buffer, fio.GetMemoryData(), res);
	fio.Close();
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_load_state_mem - Carga el estado desde un buffer en memoria.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_load_state_mem(XM6Handle handle, const void *buffer, unsigned int size)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (!buffer || size == 0) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	Fileio fio;
	if (!fio.OpenMemoryRead(buffer, size)) {
		return XM6CORE_ERR_IO;
	}

	DWORD res = ctx->vm->Load(fio);
	fio.Close();
	if (res != 0) {
		post_state_load_sync(ctx);
	}
	return (res != 0) ? XM6CORE_OK : XM6CORE_ERR_IO;
}

//---------------------------------------------------------------------------
//	xm6_get_main_ram - Expone la RAM principal para cheats/achievements.
//---------------------------------------------------------------------------
XM6CORE_API void* XM6CORE_CALL xm6_get_main_ram(XM6Handle handle, unsigned int* out_size)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (out_size) {
		*out_size = 0;
	}
	if (!ctx_valid(ctx)) {
		return NULL;
	}

	Memory *memory = (Memory*)ctx->vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	if (!memory) {
		return NULL;
	}

	if (out_size) {
		*out_size = (unsigned int)memory->GetRAMSize();
	}
	return (void*)memory->GetRAM();
}

