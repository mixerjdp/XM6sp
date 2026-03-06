#include "os.h"
#include "xm6.h"
#include "xm6core.h"
#include "vm.h"
#include "opm.h"
#include "adpcm.h"
#include "ppi.h"
#include "config.h"

struct XM6ContextRuntimeShim {
	VM *vm;
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
	DWORD *opm_buf;
	DWORD *adpcm_buf;
	unsigned int audio_rate;
	unsigned int audio_buf_frames;
	Config runtime_config;
};

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_joy_type(XM6Handle handle, int port, int type)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (port < 0 || port >= PPI::PortMax) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	PPI *ppi = ctx->ppi;
	if (!ppi) {
		ppi = (PPI*)ctx->vm->SearchDevice(MAKEID('P', 'P', 'I', ' '));
		if (!ppi) {
			return XM6CORE_ERR_NOT_READY;
		}
		ctx->ppi = ppi;
	}

	ppi->SetJoyType(port, type);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_system_clock(XM6Handle handle, int system_clock)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (system_clock < 0 || system_clock > 5) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.system_clock = system_clock;
	ctx->vm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_ram_size(XM6Handle handle, int ram_size)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (ram_size < 0 || ram_size > 5) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.ram_size = ram_size;
	ctx->vm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_fast_floppy(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.floppy_speed = enabled ? TRUE : FALSE;
	ctx->vm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

static int clamp_volume(int volume)
{
	if (volume < 0) return 0;
	if (volume > 100) return 100;
	return volume;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_master_volume(XM6Handle handle, int volume)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.master_volume = clamp_volume(volume);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_fm_volume(XM6Handle handle, int volume)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	volume = clamp_volume(volume);
	ctx->runtime_config.fm_volume = volume;
	if (ctx->opm_engine) {
		ctx->opm_engine->SetVolume(volume);
	}
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_adpcm_volume(XM6Handle handle, int volume)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	volume = clamp_volume(volume);
	ctx->runtime_config.adpcm_volume = volume;
	if (ctx->adpcm) {
		ctx->adpcm->SetVolume(volume);
	}
	return XM6CORE_OK;
}
