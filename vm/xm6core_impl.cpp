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
#if !defined(XM6CORE_EXPORTS)
#define XM6CORE_STATIC
#endif
#include "xm6core.h"
#include "vm.h"
#include "device.h"
#include "schedule.h"
#include "cpu.h"
#include "render.h"
#include "keyboard.h"
#include "mouse.h"
#include "fdd.h"
#include "ppi.h"
#include "opmif.h"
#include "adpcm.h"
#include "sasi.h"
#include "scsi.h"
#include "filepath.h"
#include "fileio.h"
#include <cstring>
#include <new>

//---------------------------------------------------------------------------
//
//	バージョン文字列
//
//---------------------------------------------------------------------------
static const char XM6CORE_VERSION[] = "XM6 Core 2.06";

//---------------------------------------------------------------------------
//
//	内部コンテキスト
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
	ADPCM *adpcm;
	SASI *sasi;
	SCSI *scsi;
	PPI *ppi;

	// Audio mixing workspace
	DWORD *opm_buf;
	DWORD *adpcm_buf;
	unsigned int audio_rate;
	unsigned int audio_buf_frames;

	// Message callback (client-side)
	xm6_message_callback_t msg_callback;
	void *msg_user;
};

//---------------------------------------------------------------------------
//
//	ヘルパー: コンテキスト検証
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

//===========================================================================
//
//	Paso 1: Version + Lifecycle
//
//===========================================================================

//---------------------------------------------------------------------------
//	xm6_get_version — Devuelve la cadena de versión del core.
//---------------------------------------------------------------------------
XM6CORE_API const char* XM6CORE_CALL xm6_get_version(void)
{
	return XM6CORE_VERSION;
}

//---------------------------------------------------------------------------
//	xm6_create — Crea una nueva instancia del emulador.
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

	return reinterpret_cast<XM6Handle>(ctx);
}

//---------------------------------------------------------------------------
//	xm6_destroy — Destruye una instancia del emulador.
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

	if (ctx->vm) {
		ctx->vm->Cleanup();
		delete ctx->vm;
		ctx->vm = NULL;
	}

	delete[] ctx->opm_buf;
	delete[] ctx->adpcm_buf;

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

//---------------------------------------------------------------------------
//	xm6_set_message_callback — Registra un callback para mensajes del core.
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
//	xm6_exec — Ejecuta la VM por 'hus' unidades de tiempo (0.5µs cada una).
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
//	xm6_reset — Resetea la VM (equivalente a pulsar el botón de reset).
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_reset(XM6Handle handle)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->vm->Reset();
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_set_power — Enciende o apaga la VM.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_set_power(XM6Handle handle, int enabled)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->vm->SetPower(enabled ? TRUE : FALSE);
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_get_power — Consulta el estado de alimentación de la VM.
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
//	xm6_set_power_switch — Controla el interruptor de encendido.
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
//	xm6_get_power_switch — Consulta el estado del interruptor de encendido.
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
//	xm6_get_vm_version — Obtiene la versión interna de la VM (major.minor).
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
//	xm6_mount_fdd — Monta una imagen de disquete en un drive (0-3).
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

	if (!ctx->fdd->Open(drive, path, media_hint)) {
		return XM6CORE_ERR_IO;
	}

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_eject_fdd — Expulsa la imagen de un drive FDD.
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
//	xm6_mount_sasi_hdd — Monta una imagen de disco duro SASI.
//
//	NOTA: La clase SASI mantiene los paths de HD en arrays privados que
//	solo se pueblan vía ApplyCfg(Config*). Se requiere refactorizar SASI
//	para exponer un setter público. Por ahora se retorna NOT_READY.
//	TODO: Añadir SASI::SetHDPath(int slot, const Filepath& path) público.
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

	// TODO: Implementar cuando SASI exponga setter público para paths
	(void)image_path;
	return XM6CORE_ERR_NOT_READY;
}

//---------------------------------------------------------------------------
//	xm6_mount_scsi_hdd — Monta una imagen de disco duro SCSI.
//
//	Mismo caso que SASI: los paths de SCSI-HD son privados en SASI.
//	TODO: Añadir SASI::SetSCSIPath(int slot, const Filepath& path) público.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_mount_scsi_hdd(
	XM6Handle handle, int slot, const char* image_path)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->sasi || slot < 0 || slot >= 6 || !image_path) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	// TODO: Implementar cuando SASI exponga setter público para paths
	(void)image_path;
	return XM6CORE_ERR_NOT_READY;
}

//===========================================================================
//
//	Paso 4: Input (Keyboard, Mouse, Joystick)
//
//===========================================================================

//---------------------------------------------------------------------------
//	xm6_input_key — Envía un evento de tecla al emulador.
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
//	xm6_input_mouse — Establece el estado del ratón.
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
//	xm6_input_mouse_reset — Reinicia los datos acumulados del ratón.
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
//	xm6_input_joy — Establece el estado de un joystick/gamepad.
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
//	xm6_video_poll — Consulta si hay un frame de video disponible.
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

	out_frame->pixels_argb32 = (const unsigned int*)r->mixbuf;
	out_frame->width = (unsigned int)r->mixwidth;
	out_frame->height = (unsigned int)r->mixheight;
	out_frame->stride_pixels = (unsigned int)r->mixwidth;

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_video_consume — Marca el frame actual como consumido.
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

//===========================================================================
//
//	Paso 6: Audio
//
//===========================================================================

//---------------------------------------------------------------------------
//	Helper: saturación int32 → int16 (replica SoundMMXPortable)
//---------------------------------------------------------------------------
static inline short saturate_s16(int value)
{
	if (value > 32767) return 32767;
	if (value < -32768) return -32768;
	return (short)value;
}

//---------------------------------------------------------------------------
//	xm6_audio_configure — Configura el sistema de audio.
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
	ctx->opmif->InitBuf((DWORD)sample_rate);
	ctx->adpcm->InitBuf((DWORD)sample_rate);

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
//	xm6_audio_mix — Mezcla audio OPM + ADPCM + SCSI CD-DA.
//
//	Replica la lógica de CSound::Process de mfc_snd.cpp:
//	1. OPMIF::ProcessBuf()  → frames disponibles
//	2. OPMIF::GetBuf()      → escribe al buffer temporal (DWORD stereo)
//	3. ADPCM::GetBuf()      → suma al mismo buffer
//	4. SCSI::GetBuf()       → suma CD-DA al mismo buffer
//	5. Clamp int32 → int16  → salida interleaved stereo (L,R,L,R,...)
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
	if (ready > frames) {
		ready = frames;
	}

	// Limpiar buffer temporal
	memset(ctx->opm_buf, 0, frames * 2 * sizeof(DWORD));

	// OPM: llenar buffer (stereo DWORD interleaved)
	ctx->opmif->GetBuf(ctx->opm_buf, (int)ready);

	// ADPCM: sumar al mismo buffer
	ctx->adpcm->GetBuf(ctx->opm_buf, (int)ready);

	// ADPCM: sincronización
	if (ready < frames) {
		ctx->adpcm->Wait(0);
	} else {
		ctx->adpcm->Wait(0);
	}

	// SCSI CD-DA: sumar al mismo buffer
	if (ctx->scsi) {
		ctx->scsi->GetBuf(ctx->opm_buf, (int)ready, (DWORD)ctx->audio_rate);
	}

	// Convertir int32 stereo → int16 stereo interleaved
	const int *src = (const int*)ctx->opm_buf;
	unsigned int total_samples = ready * 2;  // L + R
	for (unsigned int i = 0; i < total_samples; i++) {
		out_interleaved_stereo[i] = saturate_s16(src[i]);
	}

	*out_frames = (unsigned int)ready;
	return XM6CORE_OK;
}

//===========================================================================
//
//	Paso 7: Save / Load State
//
//===========================================================================

//---------------------------------------------------------------------------
//	xm6_save_state — Guarda el estado completo de la VM a un archivo.
//
//	Retorna XM6CORE_OK si el guardado fue exitoso, XM6CORE_ERR_IO si no.
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
	return (result == 0) ? XM6CORE_OK : XM6CORE_ERR_IO;
}

//---------------------------------------------------------------------------
//	xm6_load_state — Carga el estado completo de la VM desde un archivo.
//
//	Retorna XM6CORE_OK si la carga fue exitosa, XM6CORE_ERR_IO si no.
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
	return (result == 0) ? XM6CORE_OK : XM6CORE_ERR_IO;
}
