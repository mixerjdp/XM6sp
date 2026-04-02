//----------------------------------------------------------------------------
//
//  X68Sound-style ADPCM post-processing core
//
//  This helper keeps the ADPCM path separate from the X68Sound backend.
//  It takes decoded VM ADPCM samples, applies the X68Sound-inspired shaping,
//  and writes the result back into the VM audio buffer.
//
//----------------------------------------------------------------------------

#ifndef X68SOUND_ADPCM_CORE_H
#define X68SOUND_ADPCM_CORE_H

struct XM6Context;

namespace X68SoundAdpcmCore {

static inline int clamp_sample_32(long long value)
{
	if (value > 2147483647LL) {
		return 2147483647;
	}
	if (value < -2147483647LL - 1LL) {
		return (-2147483647 - 1);
	}
	return (int)value;
}

static inline void Reset(XM6Context *ctx)
{
	if (!ctx) {
		return;
	}

	ctx->x68sound_adpcm_prev_in_l = 0;
	ctx->x68sound_adpcm_prev_in_r = 0;
	ctx->x68sound_adpcm_prev2_in_l = 0;
	ctx->x68sound_adpcm_prev2_in_r = 0;
	ctx->x68sound_adpcm_prev_out_l = 0;
	ctx->x68sound_adpcm_prev_out_r = 0;
	ctx->x68sound_adpcm_prev2_out_l = 0;
	ctx->x68sound_adpcm_prev2_out_r = 0;
	ctx->x68sound_adpcm_prev_in2_l = 0;
	ctx->x68sound_adpcm_prev_in2_r = 0;
	ctx->x68sound_adpcm_prev_out2_l = 0;
	ctx->x68sound_adpcm_prev_out2_r = 0;
}

static inline void Apply(XM6Context *ctx, DWORD *buffer, unsigned int frames)
{
	if (!ctx || !buffer || frames == 0 ||
		ctx->runtime_config.adpcm_volume <= 0 || !ctx->runtime_config.adpcm_enable) {
		if (ctx) {
			Reset(ctx);
		}
		if (buffer && frames > 0) {
			memset(buffer, 0, frames * 2 * sizeof(DWORD));
		}
		return;
	}

	const long long k_limit = (1LL << (15 + 4)) - 1;
	int prev_in_l = ctx->x68sound_adpcm_prev_in_l;
	int prev_in_r = ctx->x68sound_adpcm_prev_in_r;
	int prev2_in_l = ctx->x68sound_adpcm_prev2_in_l;
	int prev2_in_r = ctx->x68sound_adpcm_prev2_in_r;
	int prev_out_l = ctx->x68sound_adpcm_prev_out_l;
	int prev_out_r = ctx->x68sound_adpcm_prev_out_r;
	int prev2_out_l = ctx->x68sound_adpcm_prev2_out_l;
	int prev2_out_r = ctx->x68sound_adpcm_prev2_out_r;
	int prev_stage2_in_l = ctx->x68sound_adpcm_prev_out2_l;
	int prev_stage2_in_r = ctx->x68sound_adpcm_prev_out2_r;
	int prev_stage2_out_l = ctx->x68sound_adpcm_prev_in2_l;
	int prev_stage2_out_r = ctx->x68sound_adpcm_prev_in2_r;
	int *samples = reinterpret_cast<int*>(buffer);

	for (unsigned int i = 0; i < frames; ++i) {
		long long input_l = samples[0];
		long long input_r = samples[1];
		if (input_l > k_limit) input_l = k_limit;
		if (input_l < -k_limit) input_l = -k_limit;
		if (input_r > k_limit) input_r = k_limit;
		if (input_r < -k_limit) input_r = -k_limit;

		input_l *= 40LL;
		input_r *= 40LL;

		long long stage1_l = (input_l + prev_in_l + prev_in_l + prev2_in_l
			- prev_out_l * (-157LL) - prev2_out_l * 61LL) >> 8;
		long long stage1_r = (input_r + prev_in_r + prev_in_r + prev2_in_r
			- prev_out_r * (-157LL) - prev2_out_r * 61LL) >> 8;

		prev2_in_l = prev_in_l;
		prev2_in_r = prev_in_r;
		prev_in_l = (int)input_l;
		prev_in_r = (int)input_r;
		prev2_out_l = prev_out_l;
		prev2_out_r = prev_out_r;
		prev_out_l = (int)stage1_l;
		prev_out_r = (int)stage1_r;

		long long stage2_in_l = stage1_l * 356LL;
		long long stage2_in_r = stage1_r * 356LL;
		long long stage2_l = (stage2_in_l + prev_stage2_in_l
			- prev_stage2_out_l * (-312LL)) >> 10;
		long long stage2_r = (stage2_in_r + prev_stage2_in_r
			- prev_stage2_out_r * (-312LL)) >> 10;

		prev_stage2_in_l = (int)stage2_in_l;
		prev_stage2_in_r = (int)stage2_in_r;
		prev_stage2_out_l = (int)stage2_l;
		prev_stage2_out_r = (int)stage2_r;

		// Keep the X68Sound-style character, but let the VM-side ADPCM speak a
		// little louder so it sits closer to the XM6 reference mix.
		samples[0] = clamp_sample_32((int)(-(stage2_l * 5LL) >> 2));
		samples[1] = clamp_sample_32((int)(-(stage2_r * 5LL) >> 2));
		samples += 2;
	}

	ctx->x68sound_adpcm_prev_in_l = prev_in_l;
	ctx->x68sound_adpcm_prev_in_r = prev_in_r;
	ctx->x68sound_adpcm_prev2_in_l = prev2_in_l;
	ctx->x68sound_adpcm_prev2_in_r = prev2_in_r;
	ctx->x68sound_adpcm_prev_out_l = prev_out_l;
	ctx->x68sound_adpcm_prev_out_r = prev_out_r;
	ctx->x68sound_adpcm_prev2_out_l = prev2_out_l;
	ctx->x68sound_adpcm_prev2_out_r = prev2_out_r;
	ctx->x68sound_adpcm_prev_out2_l = prev_stage2_in_l;
	ctx->x68sound_adpcm_prev_out2_r = prev_stage2_in_r;
	ctx->x68sound_adpcm_prev_in2_l = prev_stage2_out_l;
	ctx->x68sound_adpcm_prev_in2_r = prev_stage2_out_r;
}

} // namespace X68SoundAdpcmCore

#endif // X68SOUND_ADPCM_CORE_H
