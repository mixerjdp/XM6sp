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
#include "memory.h"
#include "render.h"
#include "keyboard.h"
#include "mouse.h"
#include "fdd.h"
#include "ppi.h"
#include "opmif.h"
#include "opm.h"
#include "adpcm.h"
#include "sasi.h"
#include "scsi.h"
#include "filepath.h"
#include "fileio.h"
#include <cstring>
#include <new>

//---------------------------------------------------------------------------
//
//	繝舌・繧ｸ繝ｧ繝ｳ譁・ｭ怜・
//
//---------------------------------------------------------------------------
static const char XM6CORE_VERSION[] = "XM6 Core 2.06";

//---------------------------------------------------------------------------
//
//	蜀・Κ繧ｳ繝ｳ繝・く繧ｹ繝・
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

	// Message callback (client-side)
	xm6_message_callback_t msg_callback;
	void *msg_user;
};

//---------------------------------------------------------------------------
//
//	繝倥Ν繝代・: 繧ｳ繝ｳ繝・く繧ｹ繝域､懆ｨｼ
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
//	xm6_get_version 窶・Devuelve la cadena de versiﾃｳn del core.
//---------------------------------------------------------------------------
XM6CORE_API const char* XM6CORE_CALL xm6_get_version(void)
{
	return XM6CORE_VERSION;
}

//---------------------------------------------------------------------------
//	xm6_create 窶・Crea una nueva instancia del emulador.
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

	// Cachear punteros a dispositivos vﾃｭa SearchDevice
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
//	xm6_destroy 窶・Destruye una instancia del emulador.
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

	if (ctx->opmif) {
		ctx->opmif->SetEngine(NULL);
	}
	delete ctx->opm_engine;
	ctx->opm_engine = NULL;

	delete[] ctx->opm_buf;
	delete[] ctx->adpcm_buf;

	delete ctx;
}

//===========================================================================
//
//	Paso 2: Control y ejecuciﾃｳn
//
//===========================================================================

//---------------------------------------------------------------------------
//	Trampoline: adapta la convenciﾃｳn FASTCALL del callback interno de la
//	VM a la convenciﾃｳn __cdecl del API pﾃｺblico.
//	TCHAR == char en MultiByte, por lo que no se necesita conversiﾃｳn.
//---------------------------------------------------------------------------
static void FASTCALL xm6_msg_trampoline(const TCHAR* message, void *user)
{
	XM6Context *ctx = reinterpret_cast<XM6Context*>(user);
	if (ctx && ctx->msg_callback) {
		ctx->msg_callback(message, ctx->msg_user);
	}
}

//---------------------------------------------------------------------------
//	xm6_set_message_callback 窶・Registra un callback para mensajes del core.
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
//	xm6_exec 窶・Ejecuta la VM por 'hus' unidades de tiempo (0.5ﾂｵs cada una).
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
//	xm6_exec_to_frame 窶・Ejecuta la VM hasta que haya un frame de video listo.
//
//	Avanza el tiempo (hus) en chunks pequeﾃｱos hasta que Render::IsReady()
//	sea true. Sirve como base para el retro_run() de libretro.
//---------------------------------------------------------------------------
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
	// Poner un lﾃｭmite de seguridad (e.g. 10 frames 竕・360000 hus) para
	// evitar loops infinitos si el VBlank estﾃ｡ deshabilitado.
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
//	xm6_reset 窶・Resetea la VM (equivalente a pulsar el botﾃｳn de reset).
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
//	xm6_set_power 窶・Enciende o apaga la VM.
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
//	xm6_get_power 窶・Consulta el estado de alimentaciﾃｳn de la VM.
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
//	xm6_set_power_switch 窶・Controla el interruptor de encendido.
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
//	xm6_get_power_switch 窶・Consulta el estado del interruptor de encendido.
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
//	xm6_get_vm_version 窶・Obtiene la versiﾃｳn interna de la VM (major.minor).
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
//	xm6_mount_fdd 窶・Monta una imagen de disquete en un drive (0-3).
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
//	xm6_eject_fdd 窶・Expulsa la imagen de un drive FDD.
//
//	force: si != 0, fuerza la expulsiﾃｳn aunque el motor estﾃｩ activo.
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
//	xm6_fdd_is_inserted 窶・Verifica si hay un disco insertado en el drive.
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
	// Tambiﾃｩn podrﾃｭamos verificar drv_t::insert, pero GetFDI es la forma mﾃ｡s directa
	return ctx->fdd->GetFDI(drive) != NULL ? 1 : 0;
}

//---------------------------------------------------------------------------
//	xm6_fdd_get_name 窶・Obtiene la ruta del archivo del disco montado.
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
//	xm6_mount_sasi_hdd 窶・Monta una imagen de disco duro SASI.
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
	ctx->sasi->SetSASIPath(slot, path);

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_mount_scsi_hdd 窶・Monta una imagen de disco duro SCSI.
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

	Filepath path;
	path.SetPath(image_path);
	ctx->sasi->SetSCSIPath(slot, path);

	return XM6CORE_OK;
}

//===========================================================================
//
//	Paso 4: Input (Keyboard, Mouse, Joystick)
//
//===========================================================================

//---------------------------------------------------------------------------
//	xm6_input_key 窶・Envﾃｭa un evento de tecla al emulador.
//
//	xm6_keycode: cﾃｳdigo de tecla del X68000 (0x00-0x7F).
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
//	xm6_input_mouse 窶・Establece el estado del ratﾃｳn.
//
//	x, y: posiciﾃｳn absoluta o relativa (segﾃｺn la convenciﾃｳn del emulador).
//	left, right: 1 = botﾃｳn pulsado, 0 = suelto.
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
//	xm6_input_mouse_reset 窶・Reinicia los datos acumulados del ratﾃｳn.
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
//	xm6_input_joy 窶・Establece el estado de un joystick/gamepad.
//
//	port: puerto del joystick (0 o 1).
//	axes[4]: estado de cada eje (0=centro, 1=positivo, 2=negativo).
//	buttons[8]: estado de cada botﾃｳn (1=pulsado, 0=suelto).
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
//	xm6_video_poll 窶・Consulta si hay un frame de video disponible.
//
//	Si hay un frame listo, rellena out_frame con el puntero al buffer
//	ARGB32 interno (no copia), las dimensiones y el stride.
//	El puntero es vﾃ｡lido hasta la siguiente llamada a xm6_exec().
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

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_video_consume 窶・Marca el frame actual como consumido.
//
//	Debe llamarse despuﾃｩs de xm6_video_poll exitoso, cuando el frontend
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
//	Helper: saturaciﾃｳn int32 竊・int16 (replica SoundMMXPortable)
//---------------------------------------------------------------------------
static inline short saturate_s16(int value)
{
	if (value > 32767) return 32767;
	if (value < -32768) return -32768;
	return (short)value;
}

//---------------------------------------------------------------------------
//	xm6_audio_configure 窶・Configura el sistema de audio.
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

	// Inicializar buffers de sﾃｭntesis en OPM y ADPCM
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
	ctx->opm_engine->SetVolume(54);

	ctx->opmif->InitBuf((DWORD)sample_rate);
	ctx->opmif->SetEngine(ctx->opm_engine);
	ctx->opmif->EnableFM(TRUE);
	ctx->adpcm->InitBuf((DWORD)sample_rate);
	ctx->adpcm->EnableADPCM(TRUE);
	ctx->adpcm->SetVolume(52);

	// Realocar buffer temporal de mezcla (stereo: 2 DWORDs por frame)
	// Tamaﾃｱo mﾃ｡ximo razonable: 1 segundo de audio
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
//	xm6_audio_mix 窶・Mezcla audio OPM + ADPCM + SCSI CD-DA.
//
//	Replica la lﾃｳgica de CSound::Process de mfc_snd.cpp:
//	1. OPMIF::ProcessBuf()  竊・frames disponibles
//	2. OPMIF::GetBuf()      竊・escribe al buffer temporal (DWORD stereo)
//	3. ADPCM::GetBuf()      竊・suma al mismo buffer
//	4. SCSI::GetBuf()       竊・suma CD-DA al mismo buffer
//	5. Clamp int32 竊・int16  竊・salida interleaved stereo (L,R,L,R,...)
//
//	out_interleaved_stereo: buffer del cliente (short, L/R interleaved)
//	frames: nﾃｺmero mﾃ｡ximo de frames solicitados
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
	for (unsigned int i = 0; i < total_samples; i++) {
		out_interleaved_stereo[i] = saturate_s16(src[i]);
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

	Filepath temp;
	if (!create_temp_state_filepath(&temp)) {
		return XM6CORE_ERR_IO;
	}

	DWORD res = ctx->vm->Save(temp);
	if (res == 0) {
		delete_temp_state_file(temp);
		return XM6CORE_ERR_IO;
	}

	Fileio fio;
	if (!fio.Open(temp, Fileio::ReadOnly)) {
		delete_temp_state_file(temp);
		return XM6CORE_ERR_IO;
	}

	DWORD file_size = fio.GetFileSize();
	fio.Close();
	delete_temp_state_file(temp);

	if (file_size == 0) {
		return XM6CORE_ERR_IO;
	}

	*out_size = (unsigned int)file_size;
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

	Filepath temp;
	if (!create_temp_state_filepath(&temp)) {
		return XM6CORE_ERR_IO;
	}

	DWORD res = ctx->vm->Save(temp);
	if (res == 0) {
		delete_temp_state_file(temp);
		return XM6CORE_ERR_IO;
	}

	Fileio fio;
	if (!fio.Open(temp, Fileio::ReadOnly)) {
		delete_temp_state_file(temp);
		return XM6CORE_ERR_IO;
	}

	DWORD file_size = fio.GetFileSize();
	if (file_size == 0 || file_size > size) {
		fio.Close();
		delete_temp_state_file(temp);
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	int ret = XM6CORE_ERR_IO;
	if (fio.Read(buffer, (int)file_size)) {
		ret = XM6CORE_OK;
	}
	fio.Close();
	delete_temp_state_file(temp);

	return ret;
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

	Filepath temp;
	if (!create_temp_state_filepath(&temp)) {
		return XM6CORE_ERR_IO;
	}

	Fileio fio;
	if (!fio.Open(temp, Fileio::WriteOnly)) {
		delete_temp_state_file(temp);
		return XM6CORE_ERR_IO;
	}

	if (!fio.Write(buffer, (int)size)) {
		fio.Close();
		delete_temp_state_file(temp);
		return XM6CORE_ERR_IO;
	}
	fio.Close();

	DWORD res = ctx->vm->Load(temp);
	delete_temp_state_file(temp);

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
