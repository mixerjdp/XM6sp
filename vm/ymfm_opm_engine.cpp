#include "ymfm_opm_engine.h"

#if defined(XM6CORE_ENABLE_YMFM)

#include "ymfm_opm.h"
#include <math.h>
#include <new>

namespace {

class YmfmBridge : public ymfm::ymfm_interface
{
public:
	YmfmBridge() : busy_end_(0), current_time_(0) {}

	void Reset()
	{
		busy_end_ = 0;
		current_time_ = 0;
	}

	void Advance(uint32_t clocks)
	{
		current_time_ += clocks;
	}

	virtual void ymfm_set_busy_end(uint32_t clocks)
	{
		busy_end_ = current_time_ + clocks;
	}

	virtual bool ymfm_is_busy()
	{
		return current_time_ < busy_end_;
	}

private:
	uint64_t busy_end_;
	uint64_t current_time_;
};

class Ymfm2151 : public ymfm::ym2151
{
public:
	Ymfm2151(ymfm::ymfm_interface &intf) : ymfm::ym2151(intf) {}

	ymfm::ym2151::fm_engine &Engine()
	{
		return m_fm;
	}
};

class YmfmOpmEngine : public FM::OPM
{
public:
	enum {
		kSampleClocks = ymfm::opm_registers::DEFAULT_PRESCALE * ymfm::opm_registers::OPERATORS
	};

	YmfmOpmEngine() :
		direct_mode_(FALSE),
		clock_(0),
		rate_(0),
		volume_scale_(16384),
		chip_(bridge_)
	{
	}

	void SetDirectMode(BOOL direct_mode)
	{
		direct_mode_ = direct_mode ? TRUE : FALSE;
	}

	virtual bool UseRawModeReg() const
	{
		return true;
	}

	virtual bool Init(uint c, uint r, bool)
	{
		if (!SetRate(c, r, false)) {
			return false;
		}

		Reset();
		SetVolume(0);
		return true;
	}

	virtual bool SetRate(uint c, uint r, bool)
	{
		if (!YmfmOpmEngineSupportsRate(c, r)) {
			return false;
		}

		clock_ = c;
		rate_ = r;
		return true;
	}

	virtual void Reset()
	{
		bridge_.Reset();
		chip_.reset();
	}

	virtual void SetReg(uint addr, uint data)
	{
		chip_.write_address((uint8_t)addr);
		chip_.write_data((uint8_t)data);
	}

	virtual void Mix(FM::Sample *buffer, int nsamples)
	{
		for (int i = 0; i < nsamples; i++) {
			ymfm::ym2151::output_data output;
			chip_.Engine().clock(ymfm::ym2151::fm_engine::ALL_CHANNELS);
			output.clear();
			chip_.Engine().output(output, 0, 32767, ymfm::ym2151::fm_engine::ALL_CHANNELS);
			if (!direct_mode_) {
				output.roundtrip_fp();
			}
			buffer[0] += (output.data[0] * volume_scale_) >> 14;
			buffer[1] += (output.data[1] * volume_scale_) >> 14;
			buffer += 2;
			bridge_.Advance(kSampleClocks);
		}
	}

	virtual void SetVolume(int db)
	{
		double offset = (double)(db - 140) / 200.0;
		double vol = pow(10.0, offset) * 16384.0;
		volume_scale_ = (int)vol;
	}

	virtual void TimerA()
	{
		chip_.Engine().engine_timer_expired(0);
	}

private:
	BOOL direct_mode_;
	uint clock_;
	uint rate_;
	int volume_scale_;
	YmfmBridge bridge_;
	Ymfm2151 chip_;
};

}	// namespace

BOOL YmfmOpmEngineSupportsRate(uint clock, uint rate)
{
	const uint native_rate =
		clock / (ymfm::opm_registers::DEFAULT_PRESCALE * ymfm::opm_registers::OPERATORS);
	return (rate == native_rate) ? TRUE : FALSE;
}

FM::OPM *CreateYmfmOpmEngine(BOOL direct_mode)
{
	YmfmOpmEngine *engine = new(std::nothrow) YmfmOpmEngine;
	if (engine) {
		engine->SetDirectMode(direct_mode);
	}
	return engine;
}

#endif	// XM6CORE_ENABLE_YMFM
