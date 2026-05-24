//---------------------------------------------------------------------------
//
//	XM6 Core C API Implementation
//
//	Wrapper layer: translates the public C API (xm6core.h) into
//	calls to the internal C++ VM classes.
//
//---------------------------------------------------------------------------

#if defined(_WIN32)
#include <windows.h>
#endif
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
#include "ymfm_opm_engine.h"
#include "x68sound_bridge.h"
#include "adpcm.h"
#include "dmac.h"
#include "sasi.h"
#include "scsi.h"
#include "sram.h"
#include "config.h"
#include "filepath.h"
#include "fileio.h"
#include <math.h>
#include <cstring>
#include <cstdarg>
#include <new>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

//---------------------------------------------------------------------------
//
//	Version information
//
//---------------------------------------------------------------------------
static const char XM6CORE_VERSION[] = "XM6 Core 2.06";
static const unsigned int k_video_probe_frames_after_mode_change = 12u;

#ifndef XM6CORE_ENABLE_VIDEO_PROBE_LOG
#define XM6CORE_ENABLE_VIDEO_PROBE_LOG 0
#endif

//---------------------------------------------------------------------------
//
//	Opaque emulator context handle
//	Tracks the state for a single emulator instance.
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
	BOOL surround_enabled;
	int hq_adpcm_level;
	int bass_enhancer_level;
	int reverb_level;
	int eq_sub_bass_level;
	int eq_bass_level;
	int eq_mid_level;
	int eq_presence_level;
	int eq_treble_level;
	int eq_air_level;

	// Runtime config applied to the VM devices
	Config runtime_config;
	int surround_prev_l;
	int surround_prev_r;
	int hq_adpcm_hp_prev_input;
	int hq_adpcm_hp_prev_output;
	double hq_adpcm_envelope;
	int hq_adpcm_last_level;
	double bass_enhancer_low_pass;
	double bass_enhancer_envelope;
	double bass_enhancer_period_samples;
	double bass_enhancer_sub_phase;
	double bass_enhancer_sub_phase2;
	double bass_enhancer_hp_prev_input;
	double bass_enhancer_hp_prev_output;
	double bass_enhancer_prev_low_pass;
	int bass_enhancer_samples_since_zero_cross;
	int bass_enhancer_last_level;
	int x68sound_adpcm_prev_in_l;
	int x68sound_adpcm_prev_in_r;
	int x68sound_adpcm_prev2_in_l;
	int x68sound_adpcm_prev2_in_r;
	int x68sound_adpcm_prev_out_l;
	int x68sound_adpcm_prev_out_r;
	int x68sound_adpcm_prev2_out_l;
	int x68sound_adpcm_prev2_out_r;
	int x68sound_adpcm_prev_in2_l;
	int x68sound_adpcm_prev_in2_r;
	int x68sound_adpcm_prev_out2_l;
	int x68sound_adpcm_prev_out2_r;
	int *reverb_buf;
	unsigned int reverb_buf_frames;
	unsigned int reverb_buf_pos;
	int reverb_lp_l;
	int reverb_lp_r;
	int eq_sub_bass_l;
	int eq_sub_bass_r;
	int eq_bass_l;
	int eq_bass_r;
	int eq_mid_l;
	int eq_mid_r;
	int eq_presence_l;
	int eq_presence_r;
	int eq_air_l;
	int eq_air_r;
	int eq_sub_bass_coeff_q14;
	int eq_bass_coeff_q14;
	int eq_mid_coeff_q14;
	int eq_presence_coeff_q14;
	int eq_air_coeff_q14;

	// Message callback (client-side)
	xm6_message_callback_t msg_callback;
	void *msg_user;

	// Internal video probe state
	unsigned int video_probe_signature;
	unsigned int video_probe_frames_remaining;
	unsigned int video_probe_frame_index;
	BOOL video_probe_has_signature;

	// Audio engine selection + PX68k-compatible ring state
	int audio_engine;
	short *px68k_ring;
	unsigned int px68k_ring_frames;
	unsigned int px68k_ring_read;
	unsigned int px68k_ring_write;
	unsigned int px68k_ring_count;
	unsigned int px68k_rate_accum;
	int px68k_lpf_prev_l;
	int px68k_lpf_prev_r;
	int px68k_last_out_l;
	int px68k_last_out_r;
};

#include "x68sound_adpcm_core.h"

//---------------------------------------------------------------------------
//
//	Helper: validate a context
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

static void reset_px68k_audio_state(XM6Context *ctx);
static void reset_hq_adpcm_state(XM6Context *ctx);
static void reset_bass_enhancer_state(XM6Context *ctx);
static void reset_x68sound_adpcm_state(XM6Context *ctx);
static void apply_x68sound_adpcm_fx(XM6Context *ctx, DWORD *buffer, unsigned int frames);
static void reset_reverb_state(XM6Context *ctx);
static void reset_eq_state(XM6Context *ctx);
static void sync_x68sound_state(XM6Context *ctx);
static void apply_reverb_fx(XM6Context *ctx, short *buffer, unsigned int frames);
static void apply_bass_enhancer_fx(XM6Context *ctx, short *buffer, unsigned int frames);
static void configure_eq_filters(XM6Context *ctx, unsigned int sample_rate);
static void apply_eq_fx(XM6Context *ctx, short *buffer, unsigned int frames);
static bool ensure_px68k_audio_ring(XM6Context *ctx);
static void produce_px68k_audio(XM6Context *ctx, DWORD hus);
static int configure_audio_backend(XM6Context *ctx, unsigned int sample_rate);
static inline short saturate_s16(int value);

static const unsigned int k_opm_clock_hz = 4000000u;

static int clamp_sample_32(long long value)
{
	if (value > 2147483647LL) {
		return 2147483647;
	}
	if (value < -2147483647LL - 1LL) {
		return (-2147483647 - 1);
	}
	return (int)value;
}

static void apply_stereo_widen(DWORD *buffer, unsigned int frames)
{
	if (!buffer || frames == 0) {
		return;
	}

	// Keep the effect subtle so it opens the image without becoming a fake reverb.
	enum {
		kWidthNum = 7,
		kWidthDen = 4
	};

	int *samples = (int*)buffer;
	for (unsigned int i = 0; i < frames; i++) {
		const long long left = samples[0];
		const long long right = samples[1];
		long long mid = (left + right) / 2;
		long long side = (left - right) / 2;
		side = (side * kWidthNum) / kWidthDen;
		samples[0] = clamp_sample_32(mid + side);
		samples[1] = clamp_sample_32(mid - side);
		samples += 2;
	}
}

static void apply_surround_fx(XM6Context *ctx, DWORD *buffer, unsigned int frames)
{
	if (!ctx || !buffer || frames == 0) {
		return;
	}

	// Surround is now a pure stereo widening pass.
	enum {
		kWidthNum = 15,
		kWidthDen = 8
	};

	int *samples = (int*)buffer;

	for (unsigned int i = 0; i < frames; i++) {
		const long long left = samples[0];
		const long long right = samples[1];
		long long mid = (left + right) / 2;
		long long side = (left - right) / 2;
		side = (side * kWidthNum) / kWidthDen;

		samples[0] = clamp_sample_32(mid + side);
		samples[1] = clamp_sample_32(mid - side);
		samples += 2;
	}
}

static void apply_surround_fx_s16(XM6Context *ctx, short *buffer, unsigned int frames)
{
	if (!ctx || !buffer || frames == 0) {
		return;
	}

	// Same pure widening as the 32-bit path, but applied after the X68Sound
	// backend has already produced the final mixed stereo PCM.
	enum {
		kWidthNum = 15,
		kWidthDen = 8
	};

	short *samples = buffer;

	for (unsigned int i = 0; i < frames; i++) {
		const long long left = samples[0];
		const long long right = samples[1];
		long long mid = (left + right) / 2;
		long long side = (left - right) / 2;
		side = (side * kWidthNum) / kWidthDen;

		samples[0] = saturate_s16((int)(mid + side));
		samples[1] = saturate_s16((int)(mid - side));
		samples += 2;
	}
}

static bool ensure_reverb_buffer(XM6Context *ctx, unsigned int sample_rate)
{
	if (!ctx || sample_rate == 0) {
		return false;
	}

	const unsigned int wanted_frames = ((sample_rate * 90u) / 1000u) + 1u;
	if (wanted_frames == 0u) {
		return false;
	}

	if (ctx->reverb_buf && ctx->reverb_buf_frames == wanted_frames) {
		memset(ctx->reverb_buf, 0, ctx->reverb_buf_frames * 2u * sizeof(int));
		ctx->reverb_buf_pos = 0;
		ctx->reverb_lp_l = 0;
		ctx->reverb_lp_r = 0;
		return true;
	}

	delete[] ctx->reverb_buf;
	ctx->reverb_buf = new(std::nothrow) int[wanted_frames * 2u];
	if (!ctx->reverb_buf) {
		ctx->reverb_buf_frames = 0;
		ctx->reverb_buf_pos = 0;
		ctx->reverb_lp_l = 0;
		ctx->reverb_lp_r = 0;
		return false;
	}

	ctx->reverb_buf_frames = wanted_frames;
	ctx->reverb_buf_pos = 0;
	ctx->reverb_lp_l = 0;
	ctx->reverb_lp_r = 0;
	memset(ctx->reverb_buf, 0, ctx->reverb_buf_frames * 2u * sizeof(int));
	return true;
}

static void reset_reverb_state(XM6Context *ctx)
{
	if (!ctx) {
		return;
	}

	ctx->reverb_buf_pos = 0;
	ctx->reverb_lp_l = 0;
	ctx->reverb_lp_r = 0;
	if (ctx->reverb_buf && ctx->reverb_buf_frames > 0) {
		memset(ctx->reverb_buf, 0, ctx->reverb_buf_frames * 2u * sizeof(int));
	}
}

static void apply_reverb_fx(XM6Context *ctx, short *buffer, unsigned int frames)
{
	if (!ctx || !buffer || frames == 0) {
		return;
	}

	const int level = (ctx->reverb_level < 0) ? 0 : (ctx->reverb_level > 100 ? 100 : ctx->reverb_level);
	if (level <= 0) {
		return;
	}

	const int dry_gain = 224 - ((level * 64) / 100);
	const int wet_gain = (level * 96) / 100;

	if (!ctx->reverb_buf || ctx->reverb_buf_frames == 0) {
		return;
	}

	const unsigned int delay_a_ms = 18u + (((unsigned int)level * 8u) / 100u);
	const unsigned int delay_b_ms = 37u + (((unsigned int)level * 18u) / 100u);
	unsigned int delay_a = (ctx->audio_rate * delay_a_ms) / 1000u;
	unsigned int delay_b = (ctx->audio_rate * delay_b_ms) / 1000u;
	if (delay_a == 0u) {
		delay_a = 1u;
	}
	if (delay_b <= delay_a) {
		delay_b = delay_a + 1u;
	}
	if (delay_b >= ctx->reverb_buf_frames) {
		delay_b = ctx->reverb_buf_frames - 1u;
	}

	const int feedback_gain = 64 + ((level * 96) / 100);
	const int damp = 208 - ((level * 72) / 100);
	const int cross_gain = 16 + ((level * 32) / 100);

	unsigned int pos = ctx->reverb_buf_pos;
	short *samples = buffer;
	int *ring = ctx->reverb_buf;

	for (unsigned int i = 0; i < frames; i++) {
		const int dry_l = samples[0];
		const int dry_r = samples[1];

		const unsigned int tap_a_pos = (pos + ctx->reverb_buf_frames - delay_a) % ctx->reverb_buf_frames;
		const unsigned int tap_b_pos = (pos + ctx->reverb_buf_frames - delay_b) % ctx->reverb_buf_frames;
		const int tap_a_l = ring[(tap_a_pos * 2u) + 0];
		const int tap_a_r = ring[(tap_a_pos * 2u) + 1];
		const int tap_b_l = ring[(tap_b_pos * 2u) + 0];
		const int tap_b_r = ring[(tap_b_pos * 2u) + 1];

		long long wet_l = (tap_a_l * 5LL + tap_b_l * 3LL + tap_a_r + tap_b_r) / 10LL;
		long long wet_r = (tap_a_r * 5LL + tap_b_r * 3LL + tap_a_l + tap_b_l) / 10LL;

		ctx->reverb_lp_l = (int)(((long long)ctx->reverb_lp_l * damp + wet_l * (256 - damp)) >> 8);
		ctx->reverb_lp_r = (int)(((long long)ctx->reverb_lp_r * damp + wet_r * (256 - damp)) >> 8);

		const long long filtered_l = ctx->reverb_lp_l;
		const long long filtered_r = ctx->reverb_lp_r;
		const long long room_l = filtered_l + ((filtered_r * cross_gain) >> 8);
		const long long room_r = filtered_r + ((filtered_l * cross_gain) >> 8);

		const long long feed_l = dry_l + ((room_l * feedback_gain) >> 8);
		const long long feed_r = dry_r + ((room_r * feedback_gain) >> 8);
		ring[(pos * 2u) + 0] = clamp_sample_32(feed_l);
		ring[(pos * 2u) + 1] = clamp_sample_32(feed_r);

		samples[0] = saturate_s16((int)(((long long)dry_l * dry_gain + room_l * wet_gain) >> 8));
		samples[1] = saturate_s16((int)(((long long)dry_r * dry_gain + room_r * wet_gain) >> 8));

		pos++;
		if (pos >= ctx->reverb_buf_frames) {
			pos = 0;
		}
		samples += 2;
	}

	ctx->reverb_buf_pos = pos;
}

static void reset_eq_state(XM6Context *ctx)
{
	if (!ctx) {
		return;
	}

	ctx->eq_sub_bass_l = 0;
	ctx->eq_sub_bass_r = 0;
	ctx->eq_bass_l = 0;
	ctx->eq_bass_r = 0;
	ctx->eq_mid_l = 0;
	ctx->eq_mid_r = 0;
	ctx->eq_presence_l = 0;
	ctx->eq_presence_r = 0;
	ctx->eq_air_l = 0;
	ctx->eq_air_r = 0;
}

static inline unsigned char x68sound_compose_dcr(const DMAC::dma_t &dma)
{
	unsigned char data = static_cast<unsigned char>((dma.xrm & 0x03u) << 6);
	data |= static_cast<unsigned char>((dma.dtyp & 0x03u) << 4);
	if (dma.dps) {
		data |= 0x08;
	}
	data |= static_cast<unsigned char>(dma.pcl & 0x03u);
	return data;
}

static inline unsigned char x68sound_compose_ocr(const DMAC::dma_t &dma)
{
	unsigned char data = 0;
	if (dma.dir) {
		data |= 0x80;
	}
	if (dma.btd) {
		data |= 0x40;
	}
	data |= static_cast<unsigned char>((dma.size & 0x03u) << 4);
	data |= static_cast<unsigned char>((dma.chain & 0x03u) << 2);
	data |= static_cast<unsigned char>(dma.reqg & 0x03u);
	return data;
}

static inline unsigned char x68sound_compose_scr(const DMAC::dma_t &dma)
{
	unsigned char data = static_cast<unsigned char>((dma.mac & 0x03u) << 2);
	data |= static_cast<unsigned char>(dma.dac & 0x03u);
	return data;
}

static inline unsigned char x68sound_compose_ccr(const DMAC::dma_t &dma)
{
	unsigned char data = 0;
	if (dma.intr) {
		data |= 0x08;
	}
	if (dma.hlt) {
		data |= 0x20;
	}
	if (dma.str) {
		data |= 0x80;
	}
	if (dma.cnt) {
		data |= 0x40;
	}
	if (dma.sab) {
		data |= 0x10;
	}
	return data;
}

static inline unsigned char x68sound_compose_gcr(const DMAC::dma_t &dma)
{
	return static_cast<unsigned char>(((dma.bt & 0x03u) << 2) | (dma.br & 0x03u));
}

static bool g_x68sound_adpcm_started_synced = false;
static bool g_x68sound_adpcm_play_synced = false;
static bool g_x68sound_adpcm_rec_synced = false;

static void push_x68sound_state(XM6Context *ctx, const char *trace_source)
{
#if defined(XM6CORE_ENABLE_X68SOUND)
	if (!ctx_valid(ctx) || ctx->audio_engine != XM6CORE_AUDIO_ENGINE_X68SOUND) {
		return;
	}

	Xm6X68Sound::SetTraceSource(trace_source);

	if (trace_source && trace_source[0] == 's') {
		if (ctx->opmif) {
			OPMIF::opm_t opm_state;
			ctx->opmif->GetOPM(&opm_state);
			Xm6X68Sound::WriteOpm(0x0f, static_cast<unsigned char>(opm_state.reg[0x0f]));
			Xm6X68Sound::WriteOpm(0x14, static_cast<unsigned char>(opm_state.reg[0x14]));
			Xm6X68Sound::WriteOpm(0x18, static_cast<unsigned char>(opm_state.reg[0x18]));
			Xm6X68Sound::WriteOpm(0x19, static_cast<unsigned char>(opm_state.reg[0x19]));
			Xm6X68Sound::WriteOpm(0x1b, static_cast<unsigned char>(opm_state.reg[0x1b]));
			for (int i = 0x20; i < 0x100; ++i) {
				Xm6X68Sound::WriteOpm(static_cast<unsigned char>(i), static_cast<unsigned char>(opm_state.reg[i]));
			}
			for (int i = 0; i < 8; ++i) {
				Xm6X68Sound::WriteOpm(8, static_cast<unsigned char>(opm_state.key[i]));
			}
			if (opm_state.reg[0x14] & 0x80) {
				Xm6X68Sound::TimerA();
			}
		}

	}

		ADPCM::adpcm_t adpcm_state;
		ctx->adpcm->GetADPCM(&adpcm_state);
		if (adpcm_state.started) {
			const bool want_play = adpcm_state.play != 0;
			const bool want_rec = adpcm_state.rec != 0;
			g_x68sound_adpcm_started_synced = true;
			g_x68sound_adpcm_play_synced = want_play;
			g_x68sound_adpcm_rec_synced = want_rec;
		} else if (g_x68sound_adpcm_started_synced) {
			g_x68sound_adpcm_started_synced = false;
			g_x68sound_adpcm_play_synced = false;
			g_x68sound_adpcm_rec_synced = false;
		}

	Xm6X68Sound::SetTraceSource("vm");
#else
	(void)ctx;
	(void)trace_source;
#endif
}

static void sync_x68sound_state(XM6Context *ctx)
{
#if defined(XM6CORE_ENABLE_X68SOUND)
	if (!ctx_valid(ctx) || ctx->audio_engine != XM6CORE_AUDIO_ENGINE_X68SOUND) {
		return;
	}

	Memory *memory = (Memory*)ctx->vm->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	if (!memory) {
		return;
	}

	Xm6X68Sound::SetMemory(memory);
	Xm6X68Sound::Start(ctx->audio_rate != 0 ? ctx->audio_rate : 44100u);
	Xm6X68Sound::Reset();
	g_x68sound_adpcm_started_synced = false;
	g_x68sound_adpcm_play_synced = false;
	g_x68sound_adpcm_rec_synced = false;
	push_x68sound_state(ctx, "sync");
#else
	(void)ctx;
#endif
}

static void configure_eq_filters(XM6Context *ctx, unsigned int sample_rate)
{
	if (!ctx || sample_rate == 0) {
		return;
	}

	const double pi = 3.14159265358979323846;
	const double sub_bass_fc = 60.0;
	const double bass_fc = 200.0;
	const double mid_fc = 800.0;
	const double presence_fc = 3000.0;
	const double air_fc = 8000.0;
	const double sub_bass_alpha = 1.0 - exp((-2.0 * pi * sub_bass_fc) / (double)sample_rate);
	const double bass_alpha = 1.0 - exp((-2.0 * pi * bass_fc) / (double)sample_rate);
	const double mid_alpha = 1.0 - exp((-2.0 * pi * mid_fc) / (double)sample_rate);
	const double presence_alpha = 1.0 - exp((-2.0 * pi * presence_fc) / (double)sample_rate);
	const double air_alpha = 1.0 - exp((-2.0 * pi * air_fc) / (double)sample_rate);
	int sub_bass_coeff = (int)(sub_bass_alpha * 16384.0 + 0.5);
	int bass_coeff = (int)(bass_alpha * 16384.0 + 0.5);
	int mid_coeff = (int)(mid_alpha * 16384.0 + 0.5);
	int presence_coeff = (int)(presence_alpha * 16384.0 + 0.5);
	int air_coeff = (int)(air_alpha * 16384.0 + 0.5);
	if (sub_bass_coeff < 1) {
		sub_bass_coeff = 1;
	} else if (sub_bass_coeff > 16384) {
		sub_bass_coeff = 16384;
	}
	if (bass_coeff < 1) {
		bass_coeff = 1;
	} else if (bass_coeff > 16384) {
		bass_coeff = 16384;
	}
	if (mid_coeff < 1) {
		mid_coeff = 1;
	} else if (mid_coeff > 16384) {
		mid_coeff = 16384;
	}
	if (presence_coeff < 1) {
		presence_coeff = 1;
	} else if (presence_coeff > 16384) {
		presence_coeff = 16384;
	}
	if (air_coeff < 1) {
		air_coeff = 1;
	} else if (air_coeff > 16384) {
		air_coeff = 16384;
	}
	ctx->eq_sub_bass_coeff_q14 = sub_bass_coeff;
	ctx->eq_bass_coeff_q14 = bass_coeff;
	ctx->eq_mid_coeff_q14 = mid_coeff;
	ctx->eq_presence_coeff_q14 = presence_coeff;
	ctx->eq_air_coeff_q14 = air_coeff;
	reset_eq_state(ctx);
}

static void apply_eq_fx(XM6Context *ctx, short *buffer, unsigned int frames)
{
	if (!ctx || !buffer || frames == 0) {
		return;
	}

	const int sub_bass_level = (ctx->eq_sub_bass_level < 0) ? 0 : (ctx->eq_sub_bass_level > 100 ? 100 : ctx->eq_sub_bass_level);
	const int bass_level = (ctx->eq_bass_level < 0) ? 0 : (ctx->eq_bass_level > 100 ? 100 : ctx->eq_bass_level);
	const int mid_level = (ctx->eq_mid_level < 0) ? 0 : (ctx->eq_mid_level > 100 ? 100 : ctx->eq_mid_level);
	const int presence_level = (ctx->eq_presence_level < 0) ? 0 : (ctx->eq_presence_level > 100 ? 100 : ctx->eq_presence_level);
	const int treble_level = (ctx->eq_treble_level < 0) ? 0 : (ctx->eq_treble_level > 100 ? 100 : ctx->eq_treble_level);
	const int air_level = (ctx->eq_air_level < 0) ? 0 : (ctx->eq_air_level > 100 ? 100 : ctx->eq_air_level);

	if (sub_bass_level == 50 && bass_level == 50 && mid_level == 50 &&
	    presence_level == 50 && treble_level == 50 && air_level == 50) {
		return;
	}

	const long long kEqGainSpanQ14 = 24576LL;
	const int sub_bass_gain_q14 = 16384 + (((long long)(sub_bass_level - 50) * kEqGainSpanQ14) / 100LL);
	const int bass_gain_q14 = 16384 + (((long long)(bass_level - 50) * kEqGainSpanQ14) / 100LL);
	const int mid_gain_q14 = 16384 + (((long long)(mid_level - 50) * kEqGainSpanQ14) / 100LL);
	const int presence_gain_q14 = 16384 + (((long long)(presence_level - 50) * kEqGainSpanQ14) / 100LL);
	const int treble_gain_q14 = 16384 + (((long long)(treble_level - 50) * kEqGainSpanQ14) / 100LL);
	const int air_gain_q14 = 16384 + (((long long)(air_level - 50) * kEqGainSpanQ14) / 100LL);
	const int sub_bass_coeff = ctx->eq_sub_bass_coeff_q14;
	const int bass_coeff = ctx->eq_bass_coeff_q14;
	const int mid_coeff = ctx->eq_mid_coeff_q14;
	const int presence_coeff = ctx->eq_presence_coeff_q14;
	const int air_coeff = ctx->eq_air_coeff_q14;

	int sub_bass_l = ctx->eq_sub_bass_l;
	int sub_bass_r = ctx->eq_sub_bass_r;
	int bass_l = ctx->eq_bass_l;
	int bass_r = ctx->eq_bass_r;
	int mid_l = ctx->eq_mid_l;
	int mid_r = ctx->eq_mid_r;
	int presence_l = ctx->eq_presence_l;
	int presence_r = ctx->eq_presence_r;
	int air_l = ctx->eq_air_l;
	int air_r = ctx->eq_air_r;
	short *samples = buffer;

	for (unsigned int i = 0; i < frames; i++) {
		const int in_l = samples[0];
		const int in_r = samples[1];

		sub_bass_l += (int)(((long long)(in_l - sub_bass_l) * sub_bass_coeff) >> 14);
		sub_bass_r += (int)(((long long)(in_r - sub_bass_r) * sub_bass_coeff) >> 14);
		bass_l += (int)(((long long)(in_l - bass_l) * bass_coeff) >> 14);
		bass_r += (int)(((long long)(in_r - bass_r) * bass_coeff) >> 14);
		mid_l += (int)(((long long)(in_l - mid_l) * mid_coeff) >> 14);
		mid_r += (int)(((long long)(in_r - mid_r) * mid_coeff) >> 14);
		presence_l += (int)(((long long)(in_l - presence_l) * presence_coeff) >> 14);
		presence_r += (int)(((long long)(in_r - presence_r) * presence_coeff) >> 14);
		air_l += (int)(((long long)(in_l - air_l) * air_coeff) >> 14);
		air_r += (int)(((long long)(in_r - air_r) * air_coeff) >> 14);

		const long long sub_bass_band_l = sub_bass_l;
		const long long sub_bass_band_r = sub_bass_r;
		const long long bass_band_l = (long long)bass_l - sub_bass_l;
		const long long bass_band_r = (long long)bass_r - sub_bass_r;
		const long long mid_band_l = (long long)mid_l - bass_l;
		const long long mid_band_r = (long long)mid_r - bass_r;
		const long long presence_band_l = (long long)presence_l - mid_l;
		const long long presence_band_r = (long long)presence_r - mid_r;
		const long long treble_band_l = (long long)air_l - presence_l;
		const long long treble_band_r = (long long)air_r - presence_r;
		const long long air_band_l = (long long)in_l - air_l;
		const long long air_band_r = (long long)in_r - air_r;

		const long long out_l =
			((sub_bass_band_l * sub_bass_gain_q14) +
			 (bass_band_l * bass_gain_q14) +
			 (mid_band_l * mid_gain_q14) +
			 (presence_band_l * presence_gain_q14) +
			 (treble_band_l * treble_gain_q14) +
			 (air_band_l * air_gain_q14)) >> 14;
		const long long out_r =
			((sub_bass_band_r * sub_bass_gain_q14) +
			 (bass_band_r * bass_gain_q14) +
			 (mid_band_r * mid_gain_q14) +
			 (presence_band_r * presence_gain_q14) +
			 (treble_band_r * treble_gain_q14) +
			 (air_band_r * air_gain_q14)) >> 14;

		samples[0] = saturate_s16((int)out_l);
		samples[1] = saturate_s16((int)out_r);
		samples += 2;
	}

	ctx->eq_sub_bass_l = sub_bass_l;
	ctx->eq_sub_bass_r = sub_bass_r;
	ctx->eq_bass_l = bass_l;
	ctx->eq_bass_r = bass_r;
	ctx->eq_mid_l = mid_l;
	ctx->eq_mid_r = mid_r;
	ctx->eq_presence_l = presence_l;
	ctx->eq_presence_r = presence_r;
	ctx->eq_air_l = air_l;
	ctx->eq_air_r = air_r;
}

static void reset_hq_adpcm_state(XM6Context *ctx)
{
	if (!ctx) {
		return;
	}

	ctx->hq_adpcm_hp_prev_input = 0;
	ctx->hq_adpcm_hp_prev_output = 0;
	ctx->hq_adpcm_envelope = 0.0;
	ctx->hq_adpcm_last_level = -1;
	reset_bass_enhancer_state(ctx);
}

static void reset_bass_enhancer_state(XM6Context *ctx)
{
	if (!ctx) {
		return;
	}

	ctx->bass_enhancer_low_pass = 0.0;
	ctx->bass_enhancer_envelope = 0.0;
	ctx->bass_enhancer_period_samples = 0.0;
	ctx->bass_enhancer_sub_phase = 0.0;
	ctx->bass_enhancer_sub_phase2 = 0.0;
	ctx->bass_enhancer_hp_prev_input = 0.0;
	ctx->bass_enhancer_hp_prev_output = 0.0;
	ctx->bass_enhancer_prev_low_pass = 0.0;
	ctx->bass_enhancer_samples_since_zero_cross = 0;
	ctx->bass_enhancer_last_level = -1;
}

static void reset_x68sound_adpcm_state(XM6Context *ctx)
{
	X68SoundAdpcmCore::Reset(ctx);
}

static void apply_x68sound_adpcm_fx(XM6Context *ctx, DWORD *buffer, unsigned int frames)
{
	X68SoundAdpcmCore::Apply(ctx, buffer, frames);
}

static void apply_hq_adpcm_fx(XM6Context *ctx, DWORD *buffer, unsigned int frames)
{
	if (!ctx || !buffer || frames == 0 || ctx->hq_adpcm_level <= 0 || ctx->audio_rate == 0) {
		return;
	}

	const int level = (ctx->hq_adpcm_level > 100) ? 100 : ctx->hq_adpcm_level;
	if (ctx->hq_adpcm_last_level != level) {
		reset_hq_adpcm_state(ctx);
		ctx->hq_adpcm_last_level = level;
	}

	double prev_input = static_cast<double>(ctx->hq_adpcm_hp_prev_input);
	double prev_output = static_cast<double>(ctx->hq_adpcm_hp_prev_output);
	double envelope = ctx->hq_adpcm_envelope;
	int *samples = reinterpret_cast<int*>(buffer);

	const double sample_rate = static_cast<double>(ctx->audio_rate);
	const double rc = 1.0 / (2.0 * 3.14159265358979323846 * 8000.0);
	const double dt = 1.0 / sample_rate;
	const double alpha = rc / (rc + dt);
	const double gain = static_cast<double>(level) / 100.0;

	for (unsigned int i = 0; i < frames; i++) {
		const double dry_l = static_cast<double>(samples[0]) / 32768.0;
		const double dry_r = static_cast<double>(samples[1]) / 32768.0;
		const double input = (dry_l + dry_r) * 0.5;

		const double high = alpha * (prev_output + input - prev_input);
		prev_input = input;
		prev_output = high;

		envelope = (envelope * 0.985) + (fabs(high) * 0.015);
		const double drive = 1.0 + (envelope * 3.25);
		const double shaped = tanh(high * drive);
		const double harmonic = shaped - (high * 0.55);
		// Keep the exciter focused up to the 3rd harmonic without pushing a lot of
		// extra upper-octave content.
		double boosted = ((high * 0.82) + (harmonic * 1.35)) * gain * 2.0;

		if (boosted > 1.0) {
			boosted = 1.0 + ((boosted - 1.0) * 0.4);
		} else if (boosted < -1.0) {
			boosted = -1.0 + ((boosted + 1.0) * 0.4);
		}

		const double out_l = dry_l + boosted;
		const double out_r = dry_r + boosted;
		samples[0] = clamp_sample_32(static_cast<long long>(out_l * 32768.0));
		samples[1] = clamp_sample_32(static_cast<long long>(out_r * 32768.0));
		samples += 2;
	}

	ctx->hq_adpcm_hp_prev_input = static_cast<int>(prev_input);
	ctx->hq_adpcm_hp_prev_output = static_cast<int>(prev_output);
	ctx->hq_adpcm_envelope = envelope;
}

static void apply_bass_enhancer_fx(XM6Context *ctx, short *buffer, unsigned int frames)
{
	if (!ctx || !buffer || frames == 0 || ctx->bass_enhancer_level <= 0 || ctx->audio_rate == 0) {
		return;
	}

	const int level = (ctx->bass_enhancer_level > 100) ? 100 : ctx->bass_enhancer_level;
	if (ctx->bass_enhancer_last_level != level) {
		reset_bass_enhancer_state(ctx);
		ctx->bass_enhancer_last_level = level;
	}

	double low_pass = ctx->bass_enhancer_low_pass;
	double envelope = ctx->bass_enhancer_envelope;
	double period_samples = ctx->bass_enhancer_period_samples;
	double sub_phase = ctx->bass_enhancer_sub_phase;
	double sub_phase2 = ctx->bass_enhancer_sub_phase2;
	double hp_prev_input = ctx->bass_enhancer_hp_prev_input;
	double hp_prev_output = ctx->bass_enhancer_hp_prev_output;
	double prev_low_pass = ctx->bass_enhancer_prev_low_pass;
	int samples_since_zero_cross = ctx->bass_enhancer_samples_since_zero_cross;

	const double sample_rate = static_cast<double>(ctx->audio_rate);
	const double alpha = exp((-2.0 * 3.14159265358979323846 * 140.0) / sample_rate);
	const double gain = static_cast<double>(level) / 100.0;
	const double hp_rc = 1.0 / (2.0 * 3.14159265358979323846 * 25.0);
	const double hp_dt = 1.0 / sample_rate;
	const double hp_alpha = hp_rc / (hp_rc + hp_dt);

	short *samples = buffer;
	for (unsigned int i = 0; i < frames; i++) {
		const double dry_l = static_cast<double>(samples[0]) / 32768.0;
		const double dry_r = static_cast<double>(samples[1]) / 32768.0;
		const double input = (dry_l + dry_r) * 0.5;

		low_pass = (low_pass * alpha) + (input * (1.0 - alpha));
		envelope = (envelope * 0.985) + (fabs(low_pass) * 0.015);

		samples_since_zero_cross++;
		if ((prev_low_pass <= 0.0) && (0.0 < low_pass)) {
			const int min_period = (sample_rate > 120.0) ? (int)(sample_rate / 120.0) : 1;
			if (min_period <= samples_since_zero_cross) {
				const double detected_period = (double)samples_since_zero_cross;
				if (period_samples <= 0.0) {
					period_samples = detected_period;
				} else {
					period_samples = (period_samples * 0.85) + (detected_period * 0.15);
				}
			}
			samples_since_zero_cross = 0;
		}
		prev_low_pass = low_pass;

		double sub1 = 0.0;
		double sub2 = 0.0;
		if (period_samples > 0.0) {
			const double sub_period1 = (period_samples * 2.0 < 4.0) ? 4.0 : (period_samples * 2.0);
			const double sub_period2 = (period_samples * 4.0 < 8.0) ? 8.0 : (period_samples * 4.0);

			sub_phase += (2.0 * 3.14159265358979323846) / sub_period1;
			if (sub_phase > (2.0 * 3.14159265358979323846)) {
				sub_phase -= (2.0 * 3.14159265358979323846);
			}

			sub_phase2 += (2.0 * 3.14159265358979323846) / sub_period2;
			if (sub_phase2 > (2.0 * 3.14159265358979323846)) {
				sub_phase2 -= (2.0 * 3.14159265358979323846);
			}

			sub1 = sin(sub_phase);
			sub2 = sin(sub_phase2);
		}

		const double raw_enhanced =
			((low_pass * 0.55) +
			 (sub1 * envelope * 1.26) +
			 (sub2 * envelope * 1.578)) * gain * 2.25;

		double enhanced = hp_alpha * (hp_prev_output + raw_enhanced - hp_prev_input);
		hp_prev_input = raw_enhanced;
		hp_prev_output = enhanced;

		if (enhanced > 1.0) {
			enhanced = 1.0 + ((enhanced - 1.0) * 0.4);
		} else if (enhanced < -1.0) {
			enhanced = -1.0 + ((enhanced + 1.0) * 0.4);
		}

		const double out_l = dry_l + enhanced;
		const double out_r = dry_r + enhanced;
		samples[0] = saturate_s16((int)(out_l * 32768.0));
		samples[1] = saturate_s16((int)(out_r * 32768.0));
		samples += 2;
	}

	ctx->bass_enhancer_low_pass = low_pass;
	ctx->bass_enhancer_envelope = envelope;
	ctx->bass_enhancer_period_samples = period_samples;
	ctx->bass_enhancer_sub_phase = sub_phase;
	ctx->bass_enhancer_sub_phase2 = sub_phase2;
	ctx->bass_enhancer_hp_prev_input = hp_prev_input;
	ctx->bass_enhancer_hp_prev_output = hp_prev_output;
	ctx->bass_enhancer_prev_low_pass = prev_low_pass;
	ctx->bass_enhancer_samples_since_zero_cross = samples_since_zero_cross;
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
	struct stat st;
	if (!path || stat(path, &st) != 0) {
		return 0;
	}
	if ((st.st_mode & S_IFDIR) != 0) {
		return 0;
	}
	return static_cast<unsigned long long>(st.st_size);
}

static bool path_has_extension_ci(const char *path, const char *ext)
{
	if (!path || !ext) {
		return false;
	}

	const char *dot = strrchr(path, '.');
	if (!dot) {
		return false;
	}

	return (_stricmp(dot, ext) == 0);
}

static void set_sram_boot_scsi(XM6Context *ctx, int slot)
{
	if (!ctx || !ctx->vm) {
		return;
	}

	SRAM *sram = (SRAM*)ctx->vm->SearchDevice(MAKEID('S', 'R', 'A', 'M'));
	if (!sram) {
		return;
	}

	if (slot < 0) {
		slot = 0;
	} else if (slot > 7) {
		slot = 7;
	}

	// This backend mounts SCSI HDDs as SCSIExt, so the IPL should jump to the
	// external SCSI ROM entry point range.
	const DWORD rom_addr = 0x00EA0020u + (static_cast<DWORD>(slot) << 2);

	// BOOT = ROM, ROM boot address = SCSI target entry point.
	sram->SetMemSw(0x18, 0xA0);
	sram->SetMemSw(0x19, 0x00);
	sram->SetMemSw(0x0C, static_cast<BYTE>((rom_addr >> 24) & 0xFF));
	sram->SetMemSw(0x0D, static_cast<BYTE>((rom_addr >> 16) & 0xFF));
	sram->SetMemSw(0x0E, static_cast<BYTE>((rom_addr >> 8) & 0xFF));
	sram->SetMemSw(0x0F, static_cast<BYTE>(rom_addr & 0xFF));

	emit_messagef(ctx,
		"[xm6-core] SRAM boot selector set to SCSI%d (BOOT=ROM, ROM=$%08X)",
		slot, rom_addr);
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

static const char* audio_engine_name(int audio_engine)
{
	switch (audio_engine) {
	case XM6CORE_AUDIO_ENGINE_PX68K:
		return "PX68k";
	case XM6CORE_AUDIO_ENGINE_YMFM:
		return "YMFM";
	case XM6CORE_AUDIO_ENGINE_YMFM_DIRECT:
		return "YMFM";
	case XM6CORE_AUDIO_ENGINE_X68SOUND:
		return "X68Sound";
	default:
		return "XM6";
	}
}

static const char* audio_backend_name(int audio_engine, bool using_ymfm)
{
	switch (audio_engine) {
	case XM6CORE_AUDIO_ENGINE_PX68K:
		return "PX68k";
	case XM6CORE_AUDIO_ENGINE_X68SOUND:
		return "X68Sound";
	case XM6CORE_AUDIO_ENGINE_YMFM:
	case XM6CORE_AUDIO_ENGINE_YMFM_DIRECT:
		return using_ymfm ? "YMFM" : "FMGEN/XM6";
	default:
		return "FMGEN/XM6";
	}
}

static FM::OPM *create_selected_opm_engine(XM6Context *ctx, unsigned int sample_rate, bool *out_using_ymfm)
{
	FM::OPM *engine = NULL;
	const bool use_ymfm = (ctx->audio_engine == XM6CORE_AUDIO_ENGINE_YMFM ||
		ctx->audio_engine == XM6CORE_AUDIO_ENGINE_YMFM_DIRECT ||
		ctx->audio_engine == XM6CORE_AUDIO_ENGINE_YMFM_RAW);
	const bool direct_mode = (ctx->audio_engine == XM6CORE_AUDIO_ENGINE_YMFM_DIRECT ||
		ctx->audio_engine == XM6CORE_AUDIO_ENGINE_YMFM_RAW);

	if (out_using_ymfm) {
		*out_using_ymfm = false;
	}

#if defined(XM6CORE_ENABLE_YMFM)
	if (use_ymfm) {
		if (YmfmOpmEngineSupportsRate(k_opm_clock_hz, sample_rate)) {
			engine = CreateYmfmOpmEngine(direct_mode ? TRUE : FALSE);
			if (engine) {
				if (!engine->Init(k_opm_clock_hz, sample_rate, true)) {
					delete engine;
					engine = NULL;
				} else if (out_using_ymfm) {
					*out_using_ymfm = true;
				}
			}
			if (!engine) {
				emit_messagef(ctx,
					"[xm6-core] %s init failed at %u Hz; falling back to FMGEN/XM6 backend",
					"YMFM",
					sample_rate);
			}
		} else {
			emit_messagef(ctx,
				"[xm6-core] %s requires native OPM rate (%u Hz requested, %u Hz required); falling back to FMGEN/XM6 backend",
				"YMFM",
				sample_rate,
				k_opm_clock_hz / 64u);
		}
	}
#else
	if (use_ymfm) {
		emit_messagef(ctx,
			"[xm6-core] %s requested but this build was compiled without XM6CORE_ENABLE_YMFM; falling back to FMGEN/XM6 backend",
			"YMFM");
	}
#endif

	if (!engine) {
		engine = new(std::nothrow) FM::OPM;
		if (!engine) {
			return NULL;
		}
		if (!engine->Init(k_opm_clock_hz, sample_rate, true)) {
			delete engine;
			return NULL;
		}
	}

	engine->Reset();
	engine->SetVolume(ctx->runtime_config.fm_volume);
	emit_messagef(ctx,
		"[xm6-core] OPM backend ready: requested=%s effective=%s at %u Hz",
		audio_engine_name(ctx->audio_engine),
		audio_backend_name(ctx->audio_engine, out_using_ymfm ? *out_using_ymfm : false),
		sample_rate);
	return engine;
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
	config->master_volume = 80;
	config->fm_volume = 50;
	config->adpcm_volume = 50;
	config->fm_enable = TRUE;
	config->adpcm_enable = TRUE;
	config->kbd_connect = TRUE;
	config->mouse_speed = 205;
	config->mouse_port = 0;
	config->mouse_swap = FALSE;
	config->mouse_mid = TRUE;
	config->alt_raster = FALSE;

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

	reset_px68k_audio_state(ctx);
	reset_hq_adpcm_state(ctx);
	reset_x68sound_adpcm_state(ctx);
	reset_reverb_state(ctx);
	reset_eq_state(ctx);
	sync_x68sound_state(ctx);

	reset_video_probe_state(ctx);
}

static bool create_temp_state_filepath(Filepath *out_path)
{
	TCHAR temp_dir[_MAX_PATH];
	TCHAR temp_file[_MAX_PATH];
	size_t len;

	if (!out_path) {
		return false;
	}

#if defined(_WIN32)
	len = static_cast<size_t>(::GetTempPathA(_MAX_PATH, temp_dir));
	if (len == 0 || len > (_MAX_PATH - 1)) {
		return false;
	}
	if (::GetTempFileNameA(temp_dir, _T("xm6"), 0, temp_file) == 0) {
		return false;
	}
	out_path->SetPath(temp_file);
	return true;
#else
	const char* tmp = getenv("TMPDIR");
	if (!tmp || !*tmp) {
		tmp = "/tmp";
	}
	strncpy(temp_dir, tmp, sizeof(temp_dir) - 1);
	temp_dir[sizeof(temp_dir) - 1] = '\0';
	len = strlen(temp_dir);
	if (len > 0 && temp_dir[len - 1] != '/' && temp_dir[len - 1] != '\\') {
		strncat(temp_dir, PATH_SEPARATOR_STR, sizeof(temp_dir) - strlen(temp_dir) - 1);
	}
	strncpy(temp_file, temp_dir, sizeof(temp_file) - 1);
	temp_file[sizeof(temp_file) - 1] = '\0';
	strncat(temp_file, "xm6stateXXXXXX", sizeof(temp_file) - strlen(temp_file) - 1);
	int fd = mkstemp(temp_file);
	if (fd < 0) {
		return false;
	}
	close(fd);
	out_path->SetPath(temp_file);
	return true;
#endif
}

static void delete_temp_state_file(const Filepath& path)
{
	if (!path.IsClear()) {
#if defined(_WIN32)
		::DeleteFileA(path.GetPath());
#else
		unlink(path.GetPath());
#endif
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

	// Create e inicializar la VM
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

    // Cache device pointers via SearchDevice
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
	ctx->audio_engine = XM6CORE_AUDIO_ENGINE_XM6;
	ctx->reverb_level = 0;
	ctx->bass_enhancer_level = 0;
	ctx->eq_sub_bass_level = 50;
	ctx->eq_bass_level = 50;
	ctx->eq_mid_level = 50;
	ctx->eq_presence_level = 50;
	ctx->eq_treble_level = 50;
	ctx->eq_air_level = 50;

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
#if defined(XM6CORE_ENABLE_X68SOUND)
	Xm6X68Sound::Shutdown();
#endif

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

	delete[] ctx->reverb_buf;
	ctx->reverb_buf = NULL;
	ctx->reverb_buf_frames = 0;
	ctx->reverb_buf_pos = 0;

	delete[] ctx->px68k_ring;
	ctx->px68k_ring = NULL;
	ctx->px68k_ring_frames = 0;

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
//	Trampoline: adapts the FASTCALL convention of the internal callback of the
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

#if defined(_MSC_VER) && (_MSC_VER < 1900)
	_snprintf_s(
		msg,
		sizeof(msg),
		_TRUNCATE,
		"[xm6core] %s size=%u first_id=%08lX",
		tag,
		(unsigned int)size,
		first_id);
#else
	snprintf(
		msg,
		sizeof(msg),
		"[xm6core] %s size=%u first_id=%08lX",
		tag,
		(unsigned int)size,
		first_id);
#endif
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
//	xm6_set_system_dir - Set the base directory for BIOS/ROM.
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
	if (result) {
		produce_px68k_audio(ctx, (DWORD)hus);
	}
	return result ? XM6CORE_OK : XM6CORE_ERR_NOT_READY;
}

//---------------------------------------------------------------------------
//	xm6_exec_to_frame  EEjecuta la VM hasta que haya un frame de video listo.
//
//	Advance time (hus) in small chunks until Render::IsReady()
//	sea true. Sirve como base para el retro_run() de libretro.
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//	xm6_exec_events_only - Advance events/scheduler without executing the CPU.
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
	produce_px68k_audio(ctx, (DWORD)hus);
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

	// Un frame a 55Hz son ~36000 hus. Execute en pasos de 1000 hus.
	// Poner un límite de seguridad (e.g. 10 frames ≁E360000 hus) para
	// evitar loops infinitos si el VBlank está deshabilitado.
	const DWORD CHUNK = 1000;
	const DWORD MAX_HUS = 360000;
	DWORD total = 0;

	while (total < MAX_HUS) {
		ctx->vm->Exec(CHUNK);
		produce_px68k_audio(ctx, CHUNK);
		total += CHUNK;

		if (ctx->render->IsReady()) {
			break;
		}
	}

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_reset - Reset the VM (equivalent to pressing the reset button).
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_reset(XM6Handle handle)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->vm->Reset();
	reset_px68k_audio_state(ctx);
	reset_hq_adpcm_state(ctx);
	reset_x68sound_adpcm_state(ctx);
	reset_reverb_state(ctx);
	reset_eq_state(ctx);
	sync_x68sound_state(ctx);
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
		reset_px68k_audio_state(ctx);
		reset_hq_adpcm_state(ctx);
		reset_x68sound_adpcm_state(ctx);
		reset_reverb_state(ctx);
		reset_eq_state(ctx);
		reset_video_probe_state(ctx);
		Xm6X68Sound::Reset();
	} else {
		sync_x68sound_state(ctx);
	}
	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_get_power - Check the VM power state.
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
//	xm6_get_power_switch - Check the power switch state.
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
//	xm6_get_vm_version - Get the internal VM version (major.minor).
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
		return 0;// Invalid = false
	}

	if (!ctx->fdd || drive < 0 || drive > 3) {
		return 0;
	}

	// Si GetFDI devuelve un puntero no nulo, hay un disco (o al menos un objeto de imagen)
	// También podríamos verificar drv_t::insert, pero GetFDI es la forma más directa
	return ctx->fdd->GetFDI(drive) != NULL ? 1 : 0;
}

//---------------------------------------------------------------------------
//	xm6_fdd_get_name - Get the path of the mounted disk image.
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

	const char * path_str = path.GetPath(); // assuming TCHAR == char, validated before
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
	if (path_has_extension_ci(image_path, ".hds")) {
		set_sram_boot_scsi(ctx, slot);
	}

	return XM6CORE_OK;
}

//===========================================================================
//
//	Paso 4: Input (Keyboard, Mouse, Joystick)
//
//===========================================================================

//---------------------------------------------------------------------------
//	xm6_input_key - Send a key event to the emulator.
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
//	xm6_input_mouse - Set the mouse state.
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
//	xm6_input_mouse_reset - Reset the accumulated mouse data.
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
//	xm6_input_joy - Set the state of a joystick/gamepad.
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
//	xm6_video_poll  ECheck whether a video frame is available.
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

	// Get el workspace del Render
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
//	xm6_get_video_refresh_hz - Get the current vertical frequency.
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
//	xm6_get_video_layout - Get the active render video layout.
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
//	Helper: int32 to int16 saturation (replicates SoundMMXPortable)
//---------------------------------------------------------------------------
static inline short saturate_s16(int value)
{
	if (value > 32767) return 32767;
	if (value < -32768) return -32768;
	return (short)value;
}

static void reset_px68k_audio_state(XM6Context *ctx)
{
	if (!ctx) {
		return;
	}

	ctx->px68k_ring_read = 0;
	ctx->px68k_ring_write = 0;
	ctx->px68k_ring_count = 0;
	ctx->px68k_rate_accum = 0;
	ctx->px68k_lpf_prev_l = 0;
	ctx->px68k_lpf_prev_r = 0;
	ctx->px68k_last_out_l = 0;
	ctx->px68k_last_out_r = 0;

	if (ctx->px68k_ring && ctx->px68k_ring_frames > 0) {
		memset(ctx->px68k_ring, 0, ctx->px68k_ring_frames * 2 * sizeof(short));
	}
}

static bool ensure_px68k_audio_ring(XM6Context *ctx)
{
	if (!ctx || ctx->audio_rate == 0) {
		return false;
	}

	unsigned int want_frames = ctx->audio_rate;
	if (want_frames < 2048) {
		want_frames = 2048;
	}

	if (ctx->px68k_ring && ctx->px68k_ring_frames == want_frames) {
		return true;
	}

	short *new_ring = new(std::nothrow) short[want_frames * 2];
	if (!new_ring) {
		return false;
	}
	memset(new_ring, 0, want_frames * 2 * sizeof(short));

	delete[] ctx->px68k_ring;
	ctx->px68k_ring = new_ring;
	ctx->px68k_ring_frames = want_frames;
	reset_px68k_audio_state(ctx);
	return true;
}

static void px68k_ring_push_frame(XM6Context *ctx, short left, short right)
{
	if (!ctx || !ctx->px68k_ring || ctx->px68k_ring_frames == 0) {
		return;
	}

	if (ctx->px68k_ring_count >= ctx->px68k_ring_frames) {
		ctx->px68k_ring_read++;
		if (ctx->px68k_ring_read >= ctx->px68k_ring_frames) {
			ctx->px68k_ring_read = 0;
		}
		ctx->px68k_ring_count = ctx->px68k_ring_frames - 1;
	}

	unsigned int write = ctx->px68k_ring_write;
	ctx->px68k_ring[(write * 2) + 0] = left;
	ctx->px68k_ring[(write * 2) + 1] = right;

	write++;
	if (write >= ctx->px68k_ring_frames) {
		write = 0;
	}
	ctx->px68k_ring_write = write;
	ctx->px68k_ring_count++;
}

static unsigned int px68k_ring_pop_frames(XM6Context *ctx, short *out, unsigned int frames)
{
	unsigned int produced = 0;

	if (!ctx || !out || !ctx->px68k_ring || ctx->px68k_ring_frames == 0) {
		return 0;
	}

	while (produced < frames && ctx->px68k_ring_count > 0) {
		unsigned int read = ctx->px68k_ring_read;
		out[(produced * 2) + 0] = ctx->px68k_ring[(read * 2) + 0];
		out[(produced * 2) + 1] = ctx->px68k_ring[(read * 2) + 1];

		read++;
		if (read >= ctx->px68k_ring_frames) {
			read = 0;
		}
		ctx->px68k_ring_read = read;
		ctx->px68k_ring_count--;
		produced++;
	}

	return produced;
}

static int px68k_clamp_volume16(int value)
{
	if (value < 0) {
		return 0;
	}
	if (value > 15) {
		return 15;
	}
	return value;
}

static int px68k_runtime_to_fm16(int volume)
{
	if (volume <= 0) {
		return 0;
	}
	int mapped = 12 + ((volume - 50) * 15) / 100;
	return px68k_clamp_volume16(mapped);
}

static int px68k_runtime_to_adpcm16(int volume)
{
	if (volume <= 0) {
		return 0;
	}
	int mapped = 15 + ((volume - 50) * 15) / 100;
	return px68k_clamp_volume16(mapped);
}

static int px68k_opm_gain_q14(int vol16)
{
	if (vol16 <= 0) {
		return 0;
	}
	double db = -((16.0 - (double)vol16) * 4.0);
	double gain = pow(10.0, db / 40.0);
	if (gain < 0.0) {
		gain = 0.0;
	}
	return (int)(gain * 16384.0 + 0.5);
}

static int px68k_adpcm_gain_q14(int vol16)
{
	if (vol16 <= 0) {
		return 0;
	}
	double gain = 1.0 / pow(1.189207115, (double)(16 - vol16));
	if (gain < 0.0) {
		gain = 0.0;
	}
	return (int)(gain * 16384.0 + 0.5);
}

static void produce_px68k_audio(XM6Context *ctx, DWORD hus)
{
	if (!ctx || ctx->audio_engine != XM6CORE_AUDIO_ENGINE_PX68K ||
		ctx->audio_rate == 0 || hus == 0 || !ctx->opmif || !ctx->adpcm ||
		!ctx->opm_buf || !ctx->adpcm_buf) {
		return;
	}

	if (!ensure_px68k_audio_ring(ctx)) {
		return;
	}

	const unsigned int k_den = 2000000;
	unsigned long long acc = (unsigned long long)ctx->px68k_rate_accum;
	acc += (unsigned long long)ctx->audio_rate * (unsigned long long)hus;
	unsigned int frames_to_generate = (unsigned int)(acc / k_den);
	ctx->px68k_rate_accum = (unsigned int)(acc % k_den);

	const int fm_gain_q14 = px68k_opm_gain_q14(px68k_runtime_to_fm16(ctx->runtime_config.fm_volume));
	const int adpcm_gain_q14 = px68k_adpcm_gain_q14(px68k_runtime_to_adpcm16(ctx->runtime_config.adpcm_volume));
	const int px_drive_q14 = 19661; // 1.20x
	const int px_output_boost_q14 = 22938; // 1.40x

	while (frames_to_generate > 0) {
		unsigned int batch = frames_to_generate;
		if (batch > ctx->audio_buf_frames) {
			batch = ctx->audio_buf_frames;
		}

		DWORD ready = ctx->opmif->ProcessBuf();
		memset(ctx->opm_buf, 0, batch * 2 * sizeof(DWORD));
		memset(ctx->adpcm_buf, 0, batch * 2 * sizeof(DWORD));
		ctx->opmif->GetBuf(ctx->opm_buf, (int)batch);

		unsigned int mix_frames = batch;
		if (ready < mix_frames) {
			mix_frames = (unsigned int)ready;
		}

		if (mix_frames > 0) {
			if (ctx->runtime_config.adpcm_volume <= 0 || !ctx->runtime_config.adpcm_enable) {
				memset(ctx->adpcm_buf, 0, mix_frames * 2 * sizeof(DWORD));
				reset_hq_adpcm_state(ctx);
				reset_x68sound_adpcm_state(ctx);
			} else {
				ctx->adpcm->GetBuf(ctx->adpcm_buf, (int)mix_frames);
			}
		}

		if (ready > mix_frames) {
			ctx->adpcm->Wait((int)(ready - mix_frames));
		} else {
			ctx->adpcm->Wait(0);
		}

		if (ctx->surround_enabled) {
			apply_surround_fx(ctx, ctx->opm_buf, mix_frames);
			apply_hq_adpcm_fx(ctx, ctx->adpcm_buf, mix_frames);
			apply_surround_fx(ctx, ctx->adpcm_buf, mix_frames);
		}
		else {
			apply_hq_adpcm_fx(ctx, ctx->adpcm_buf, mix_frames);
		}

		int *fm_src = (int*)ctx->opm_buf;
		int *adpcm_src = (int*)ctx->adpcm_buf;
		for (unsigned int i = 0; i < batch; i++) {
			int left = 0;
			int right = 0;

			if (i < mix_frames) {
				const int fm_l = fm_src[(i * 2) + 0];
				const int fm_r = fm_src[(i * 2) + 1];
				const int pcm_l = adpcm_src[(i * 2) + 0];
				const int pcm_r = adpcm_src[(i * 2) + 1];

				left = ((fm_l * fm_gain_q14) + (pcm_l * adpcm_gain_q14)) >> 14;
				right = ((fm_r * fm_gain_q14) + (pcm_r * adpcm_gain_q14)) >> 14;

				left = (left * px_drive_q14) >> 14;
				right = (right * px_drive_q14) >> 14;
				left = (left * px_output_boost_q14) >> 14;
				right = (right * px_output_boost_q14) >> 14;
			}

			ctx->px68k_lpf_prev_l = (ctx->px68k_lpf_prev_l * 3 + left) >> 2;
			ctx->px68k_lpf_prev_r = (ctx->px68k_lpf_prev_r * 3 + right) >> 2;

			px68k_ring_push_frame(
				ctx,
				saturate_s16(ctx->px68k_lpf_prev_l),
				saturate_s16(ctx->px68k_lpf_prev_r));
		}

		frames_to_generate -= batch;
	}
}

static int configure_audio_backend(XM6Context *ctx, unsigned int sample_rate)
{
	bool using_ymfm = false;

	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->opmif || !ctx->adpcm || sample_rate == 0) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

#if defined(XM6CORE_ENABLE_X68SOUND)
	Xm6X68Sound::Shutdown();
#endif

	if (ctx->opmif) {
		ctx->opmif->SetEngine(NULL);
	}

	delete ctx->opm_engine;
	ctx->opm_engine = create_selected_opm_engine(ctx, sample_rate, &using_ymfm);
	if (!ctx->opm_engine) {
		return XM6CORE_ERR_INIT_FAILED;
	}

	ctx->opmif->InitBuf((DWORD)sample_rate);
	ctx->opmif->SetEngine(ctx->opm_engine);
	ctx->opmif->EnableFM(ctx->runtime_config.fm_enable);
	ctx->adpcm->InitBuf((DWORD)sample_rate);
	ctx->adpcm->EnableADPCM(ctx->runtime_config.adpcm_enable);
	ctx->adpcm->SetVolume(ctx->runtime_config.adpcm_volume);

	delete[] ctx->opm_buf;
	ctx->opm_buf = new(std::nothrow) DWORD[sample_rate * 2];
	if (!ctx->opm_buf) {
		return XM6CORE_ERR_INIT_FAILED;
	}

	ctx->audio_rate = sample_rate;
	ctx->audio_buf_frames = sample_rate;
	ctx->surround_prev_l = 0;
	ctx->surround_prev_r = 0;
	reset_hq_adpcm_state(ctx);
	reset_x68sound_adpcm_state(ctx);
	reset_reverb_state(ctx);

	delete[] ctx->adpcm_buf;
	ctx->adpcm_buf = new(std::nothrow) DWORD[sample_rate * 2];
	if (!ctx->adpcm_buf) {
		return XM6CORE_ERR_INIT_FAILED;
	}

	if (ctx->audio_engine == XM6CORE_AUDIO_ENGINE_PX68K) {
		if (!ensure_px68k_audio_ring(ctx)) {
			return XM6CORE_ERR_INIT_FAILED;
		}
	}
	if (ctx->reverb_level > 0) {
		if (!ensure_reverb_buffer(ctx, sample_rate)) {
			return XM6CORE_ERR_INIT_FAILED;
		}
	} else {
		delete[] ctx->reverb_buf;
		ctx->reverb_buf = NULL;
		ctx->reverb_buf_frames = 0;
		ctx->reverb_buf_pos = 0;
	}
	configure_eq_filters(ctx, sample_rate);
	reset_px68k_audio_state(ctx);

#if defined(XM6CORE_ENABLE_X68SOUND)
	if (ctx->audio_engine == XM6CORE_AUDIO_ENGINE_X68SOUND) {
		sync_x68sound_state(ctx);
		emit_messagef(ctx,
			"[xm6-core] Audio backend ready: requested=%s effective=%s at %u Hz",
			audio_engine_name(ctx->audio_engine),
			audio_backend_name(ctx->audio_engine, false),
			sample_rate);
	}
#endif

	return XM6CORE_OK;
}

//---------------------------------------------------------------------------
//	xm6_audio_configure - Configure the audio system.
//
//	sample_rate: frecuencia en Hz (ej: 44100, 48000).
//	Inicializa los buffers internos de OPM y ADPCM.
//---------------------------------------------------------------------------
XM6CORE_API int XM6CORE_CALL xm6_audio_configure(
	XM6Handle handle, unsigned int sample_rate)
{
	XM6Context *ctx = ctx_from_handle(handle);
	return configure_audio_backend(ctx, sample_rate);
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

	const bool use_reverb = (ctx->reverb_level > 0);
	const bool use_bass_enhancer = (ctx->bass_enhancer_level > 0);
	const bool use_eq = !(ctx->eq_sub_bass_level == 50 &&
	                      ctx->eq_bass_level == 50 &&
	                      ctx->eq_mid_level == 50 &&
	                      ctx->eq_presence_level == 50 &&
	                      ctx->eq_treble_level == 50 &&
	                      ctx->eq_air_level == 50);

	if (ctx->audio_engine == XM6CORE_AUDIO_ENGINE_PX68K) {
		if (!ctx->px68k_ring && !ensure_px68k_audio_ring(ctx)) {
			for (unsigned int i = 0; i < (frames * 2); i++) {
				out_interleaved_stereo[i] = 0;
			}
			*out_frames = frames;
			return XM6CORE_OK;
		}

		unsigned int produced = px68k_ring_pop_frames(ctx, out_interleaved_stereo, frames);
		unsigned int total_samples = produced * 2;
		for (unsigned int i = 0; i < total_samples; i++) {
			out_interleaved_stereo[i] = saturate_s16(out_interleaved_stereo[i]);
		}

		if (produced > 0) {
			ctx->px68k_last_out_l = out_interleaved_stereo[(produced * 2) - 2];
			ctx->px68k_last_out_r = out_interleaved_stereo[(produced * 2) - 1];
		}

		for (unsigned int i = produced; i < frames; i++) {
			ctx->px68k_last_out_l = (ctx->px68k_last_out_l * 255) / 256;
			ctx->px68k_last_out_r = (ctx->px68k_last_out_r * 255) / 256;
			out_interleaved_stereo[(i * 2) + 0] = saturate_s16(ctx->px68k_last_out_l);
			out_interleaved_stereo[(i * 2) + 1] = saturate_s16(ctx->px68k_last_out_r);
		}

		if (use_bass_enhancer) {
			apply_bass_enhancer_fx(ctx, out_interleaved_stereo, frames);
		}
		if (use_reverb) {
			apply_reverb_fx(ctx, out_interleaved_stereo, frames);
		}
		if (use_eq) {
			apply_eq_fx(ctx, out_interleaved_stereo, frames);
		}

		*out_frames = frames;
		return XM6CORE_OK;
	}

#if defined(XM6CORE_ENABLE_X68SOUND)
	if (ctx->audio_engine == XM6CORE_AUDIO_ENGINE_X68SOUND) {
		const int master = (ctx->runtime_config.master_volume < 0) ? 0 :
			(ctx->runtime_config.master_volume > 100 ? 100 : ctx->runtime_config.master_volume);
		const int fm_volume = (ctx->runtime_config.fm_volume < 0) ? 0 :
			(ctx->runtime_config.fm_volume > 100 ? 100 : ctx->runtime_config.fm_volume);
		short *x68sound_src = reinterpret_cast<short*>(ctx->opm_buf);
		const int *adpcm_src = reinterpret_cast<const int*>(ctx->adpcm_buf);

		memset(ctx->opm_buf, 0, frames * 2 * sizeof(DWORD));
		memset(ctx->adpcm_buf, 0, frames * 2 * sizeof(DWORD));

		if (Xm6X68Sound::GetPcm(x68sound_src, (int)frames) != 0) {
			for (unsigned int i = 0; i < (frames * 2); i++) {
				out_interleaved_stereo[i] = 0;
			}
			*out_frames = frames;
			return XM6CORE_OK;
		}

		if (frames > 0) {
			if (ctx->runtime_config.adpcm_volume <= 0 || !ctx->runtime_config.adpcm_enable) {
				memset(ctx->adpcm_buf, 0, frames * 2 * sizeof(DWORD));
				reset_hq_adpcm_state(ctx);
				reset_x68sound_adpcm_state(ctx);
			} else {
				ctx->adpcm->GetBuf(ctx->adpcm_buf, (int)frames);
				apply_x68sound_adpcm_fx(ctx, ctx->adpcm_buf, frames);
			}
		}

		if (ctx->scsi) {
			ctx->scsi->GetBuf(ctx->opm_buf, (int)frames, (DWORD)ctx->audio_rate);
		}

		for (unsigned int i = 0; i < frames; i++) {
			// X68Sound FM runs a little quieter than its ADPCM mix and the other engines,
			// so give FM a small 20% headroom here without touching ADPCM.
			const int fm_l = (fm_volume <= 0) ? 0 :
				static_cast<int>((static_cast<int64_t>(x68sound_src[(i * 2) + 0]) * fm_volume * 120) / 10000);
			const int fm_r = (fm_volume <= 0) ? 0 :
				static_cast<int>((static_cast<int64_t>(x68sound_src[(i * 2) + 1]) * fm_volume * 120) / 10000);
			const int mixed_l = fm_l + adpcm_src[(i * 2) + 0];
			const int mixed_r = fm_r + adpcm_src[(i * 2) + 1];
			const int out_l = (master != 100) ? ((mixed_l * master) / 100) : mixed_l;
			const int out_r = (master != 100) ? ((mixed_r * master) / 100) : mixed_r;
			out_interleaved_stereo[(i * 2) + 0] = saturate_s16(out_l);
			out_interleaved_stereo[(i * 2) + 1] = saturate_s16(out_r);
		}

		if (ctx->surround_enabled) {
			apply_surround_fx_s16(ctx, out_interleaved_stereo, frames);
		}
		if (use_bass_enhancer) {
			apply_bass_enhancer_fx(ctx, out_interleaved_stereo, frames);
		}
		if (use_reverb) {
			apply_reverb_fx(ctx, out_interleaved_stereo, frames);
		}
		if (use_eq) {
			apply_eq_fx(ctx, out_interleaved_stereo, frames);
		}

		*out_frames = frames;
		return XM6CORE_OK;
	}
#endif

	// OPM: procesar y obtener samples disponibles
	DWORD ready = ctx->opmif->ProcessBuf();

	// Limpiar buffer temporal
	memset(ctx->opm_buf, 0, frames * 2 * sizeof(DWORD));
	memset(ctx->adpcm_buf, 0, frames * 2 * sizeof(DWORD));

	// Igual que frontend MFC: pedir frames y luego recortar a ready.
	ctx->opmif->GetBuf(ctx->opm_buf, (int)frames);
		if (ready < frames) {
			frames = ready;
		}

		if (ctx->runtime_config.fm_volume <= 0) {
			memset(ctx->opm_buf, 0, frames * 2 * sizeof(DWORD));
		}

	// ADPCM: buffer separado para poder aplicar surround sin alterar FM o CD-DA
	if (frames > 0) {
		if (ctx->runtime_config.adpcm_volume <= 0 || !ctx->runtime_config.adpcm_enable) {
			memset(ctx->adpcm_buf, 0, frames * 2 * sizeof(DWORD));
			reset_hq_adpcm_state(ctx);
			reset_x68sound_adpcm_state(ctx);
		} else {
			ctx->adpcm->GetBuf(ctx->adpcm_buf, (int)frames);
		}
	}

	// ADPCM: sincronizacion
	if (ready > frames) {
		ctx->adpcm->Wait((int)(ready - frames));
	} else {
		ctx->adpcm->Wait(0);
	}

	if (ctx->surround_enabled) {
		apply_surround_fx(ctx, ctx->opm_buf, frames);
		apply_hq_adpcm_fx(ctx, ctx->adpcm_buf, frames);
		apply_surround_fx(ctx, ctx->adpcm_buf, frames);
	}
	else {
		apply_hq_adpcm_fx(ctx, ctx->adpcm_buf, frames);
	}

	// SCSI CD-DA: sumar al mismo buffer
	if (ctx->scsi) {
		ctx->scsi->GetBuf(ctx->opm_buf, (int)frames, (DWORD)ctx->audio_rate);
	}

	// Convertir int32 stereo -> int16 stereo interleaved
	const int *fm_src = (const int*)ctx->opm_buf;
	const int *adpcm_src = (const int*)ctx->adpcm_buf;
	for (unsigned int i = 0; i < frames; i++) {
		const int fm_l = (ctx->runtime_config.fm_volume <= 0) ? 0 : fm_src[(i * 2) + 0];
		const int fm_r = (ctx->runtime_config.fm_volume <= 0) ? 0 : fm_src[(i * 2) + 1];
		const int mixed_l = fm_l + adpcm_src[(i * 2) + 0];
		const int mixed_r = fm_r + adpcm_src[(i * 2) + 1];
		out_interleaved_stereo[(i * 2) + 0] = saturate_s16(mixed_l);
		out_interleaved_stereo[(i * 2) + 1] = saturate_s16(mixed_r);
	}

	if (use_bass_enhancer) {
		apply_bass_enhancer_fx(ctx, out_interleaved_stereo, frames);
	}
	if (use_reverb) {
		apply_reverb_fx(ctx, out_interleaved_stereo, frames);
	}
	if (use_eq) {
		apply_eq_fx(ctx, out_interleaved_stereo, frames);
	}

	*out_frames = frames;
	return XM6CORE_OK;
}

XM6CORE_API int XM6CORE_CALL xm6_set_audio_engine(XM6Handle handle, int audio_engine)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (audio_engine != XM6CORE_AUDIO_ENGINE_XM6 &&
		audio_engine != XM6CORE_AUDIO_ENGINE_PX68K &&
		audio_engine != XM6CORE_AUDIO_ENGINE_YMFM &&
		audio_engine != XM6CORE_AUDIO_ENGINE_YMFM_DIRECT &&
		audio_engine != XM6CORE_AUDIO_ENGINE_X68SOUND) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	if (ctx->audio_engine == audio_engine) {
		return XM6CORE_OK;
	}

	const int previous_audio_engine = ctx->audio_engine;
	ctx->audio_engine = audio_engine;

	if (ctx->audio_rate > 0) {
		const int rc = configure_audio_backend(ctx, ctx->audio_rate);
		if (rc != XM6CORE_OK) {
			emit_messagef(ctx,
				"[xm6-core] Audio engine change failed: requested=%s, reverting to %s",
				audio_engine_name(audio_engine),
				audio_engine_name(previous_audio_engine));
			ctx->audio_engine = previous_audio_engine;
			configure_audio_backend(ctx, ctx->audio_rate);
			return rc;
		}
	}

	if (audio_engine == XM6CORE_AUDIO_ENGINE_PX68K &&
		ctx->audio_rate > 0 && !ensure_px68k_audio_ring(ctx)) {
		ctx->audio_engine = previous_audio_engine;
		configure_audio_backend(ctx, ctx->audio_rate);
		return XM6CORE_ERR_INIT_FAILED;
	}

	reset_px68k_audio_state(ctx);
	reset_x68sound_adpcm_state(ctx);
	emit_messagef(ctx, "[xm6-core] Audio engine selected: %s", audio_engine_name(audio_engine));
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
//	xm6_state_size - Calcula el size necesario para guardar el estado.
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
//	xm6_get_main_ram - Exposes main RAM for cheats/achievements.
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

XM6CORE_API int XM6CORE_CALL xm6_diag_get_adpcm_telemetry(
	XM6Handle handle, xm6_adpcm_telemetry_t* out_telemetry)
{
	XM6Context *ctx = ctx_from_handle(handle);
	if (!ctx_valid(ctx)) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (!out_telemetry) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	memset(out_telemetry, 0, sizeof(*out_telemetry));
	if (!ctx->adpcm) {
		return XM6CORE_ERR_NOT_READY;
	}

	ADPCM::adpcm_t state;
	ADPCM::adpcm_diag_t diag;
	ctx->adpcm->GetADPCM(&state);
	ctx->adpcm->GetDiag(&diag);

	out_telemetry->quirk_arianshuu_fix = ctx->adpcm->IsArianshuuLoopFixEnabled() ? 1u : 0u;
	out_telemetry->play = state.play ? 1u : 0u;
	out_telemetry->rec = state.rec ? 1u : 0u;
	out_telemetry->active = state.active ? 1u : 0u;
	out_telemetry->started = state.started ? 1u : 0u;
	out_telemetry->buffer_samples = state.number;
	out_telemetry->wait_counter = state.wait;
	out_telemetry->last_data = diag.last_data;

	out_telemetry->start_events = diag.start_events;
	out_telemetry->stop_events = diag.stop_events;
	out_telemetry->req_total = diag.req_total;
	out_telemetry->req_ok = diag.req_ok;
	out_telemetry->req_fail = diag.req_fail;
	out_telemetry->decode_calls = diag.decode_calls;
	out_telemetry->underrun_head_events = diag.underrun_head_events;
	out_telemetry->underrun_interp_events = diag.underrun_interp_events;
	out_telemetry->underrun_linear_events = diag.underrun_linear_events;
	out_telemetry->silence_fill_events = diag.silence_fill_events;
	out_telemetry->stale_nonzero_events = diag.stale_nonzero_events;
	out_telemetry->max_buffer_samples = diag.max_buffer_samples;

	DMAC *dmac = (DMAC*)ctx->vm->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	out_telemetry->dmac_ch3_active = (dmac && dmac->IsAct(3)) ? 1u : 0u;
	if (dmac) {
		DMAC::dma_t ch3;
		dmac->GetDMA(3, &ch3);
		out_telemetry->dmac3_mar = ch3.mar;
		out_telemetry->dmac3_mtc = ch3.mtc;
		out_telemetry->dmac3_btc = ch3.btc;
		out_telemetry->dmac3_csr =
			(ch3.coc ? 0x80u : 0u) |
			(ch3.boc ? 0x40u : 0u) |
			(ch3.ndt ? 0x20u : 0u) |
			(ch3.err ? 0x10u : 0u) |
			(ch3.act ? 0x08u : 0u) |
			(ch3.dit ? 0x04u : 0u) |
			(ch3.pct ? 0x02u : 0u) |
			(ch3.pcs ? 0x01u : 0u);
		out_telemetry->dmac3_ccr =
			(ch3.str ? 0x80u : 0u) |
			(ch3.cnt ? 0x40u : 0u) |
			(ch3.hlt ? 0x20u : 0u) |
			(ch3.sab ? 0x10u : 0u) |
			(ch3.intr ? 0x08u : 0u);
		out_telemetry->dmac3_cer = static_cast<unsigned int>(ch3.ecode & 0xffu);
	}

#if defined(XM6CORE_ENABLE_X68SOUND)
	if (ctx->audio_engine == XM6CORE_AUDIO_ENGINE_X68SOUND) {
		out_telemetry->x68sound_error_code = static_cast<unsigned int>(Xm6X68Sound::GetErrorCode());
		out_telemetry->x68sound_debug_value = static_cast<unsigned int>(Xm6X68Sound::GetDebugValue());
		out_telemetry->x68sound_write_value = static_cast<unsigned int>(Xm6X68Sound::GetWriteValue());
		out_telemetry->x68sound_trace_value = static_cast<unsigned int>(Xm6X68Sound::GetTraceValue());
	}
#endif

	return XM6CORE_OK;
}

