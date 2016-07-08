// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <cstring>
#include <array>
#include <mutex>
#include <vector>

#include "AudioCommon/WaveFile.h"

// converts [-32768, 32767] -> [-1.0, 1.0)
inline float Signed16ToFloat(const s16 s)
{
	return s * 0.000030517578125f;//(1.0f/32768.0f)
}

class CMixer
{

public:
	CMixer(u32 BackendSampleRate);

	static const u32 MAX_SAMPLES = (1024 * 4); // 128 ms
	static const u32 INDEX_MASK = MAX_SAMPLES * 2 - 1;
	static const float MAX_FREQ_SHIFT;
	static const float CONTROL_FACTOR;
	static const float CONTROL_AVG;

	virtual ~CMixer()
	{}

	// Called from audio threads
	u32 Mix(s16* samples, u32 numSamples, bool consider_framelimit = true);
	u32 Mix(float* samples, u32 numSamples, bool consider_framelimit = true);
	u32 AvailableSamples();
	// Called from main thread
	virtual void PushSamples(const s16* samples, u32 num_samples);
	virtual void PushStreamingSamples(const s16* samples, u32 num_samples);
	virtual void PushWiimoteSpeakerSamples(const s16* samples, u32 num_samples, u32 sample_rate);
	u32 GetSampleRate() const
	{
		return m_sample_rate;
	}

	void SetDMAInputSampleRate(u32 rate);
	void SetStreamInputSampleRate(u32 rate);
	void SetStreamingVolume(u32 lvolume, u32 rvolume);
	void SetWiimoteSpeakerVolume(u32 lvolume, u32 rvolume);

	void StartLogDTKAudio(const std::string& filename);
	void StopLogDTKAudio();

	void StartLogDSPAudio(const std::string& filename);
	void StopLogDSPAudio();

	std::mutex& MixerCritical()
	{
		return m_cs_mixing;
	}

	float GetCurrentSpeed() const
	{
		return m_speed.load();
	}
	void UpdateSpeed(float val)
	{
		m_speed.store(val);
	}

protected:
	class MixerFifo
	{
	public:
		MixerFifo(CMixer *mixer, unsigned sample_rate)
			: m_mixer(mixer)
			, m_input_sample_rate(sample_rate)
			, m_write_index(0)
			, m_read_index(0)
			, m_lvolume(255)
			, m_rvolume(255)
			, m_num_left_i(0.0f)
			, m_fraction(0)
		{
			srand((u32)time(nullptr));
			m_float_buffer.fill(0.0f);
		}
		virtual u32 GetWindowSize() = 0;
		virtual void Interpolate(u32 left_input_index, float* left_output, float* right_output) = 0;
		void PushSamples(const s16* samples, u32 num_samples);
		void Mix(float* samples, u32 numSamples, bool consider_framelimit = true);
		void SetInputSampleRate(u32 rate);
		unsigned int GetInputSampleRate() const;
		void SetVolume(u32 lvolume, u32 rvolume);
		void GetVolume(u32* lvolume, u32* rvolume) const;
		u32 AvailableSamples();
	protected:
		CMixer *m_mixer;
		unsigned m_input_sample_rate;

		std::array<float, MAX_SAMPLES * 2> m_float_buffer;

		std::atomic<u32> m_write_index;
		std::atomic<u32> m_read_index;

		// Volume ranges from 0-255
		std::atomic<s32> m_lvolume;
		std::atomic<s32> m_rvolume;

		float m_num_left_i;
		float m_fraction;
	};

	class LinearMixerFifo: public MixerFifo
	{
	public:
		LinearMixerFifo(CMixer* mixer, u32 sample_rate): MixerFifo(mixer, sample_rate)
		{}
		void Interpolate(u32 left_input_index, float* left_output, float* right_output) override;
		u32 GetWindowSize() override
		{
			return 4;
		};
	};

	class CubicMixerFifo: public MixerFifo
	{
	public:
		CubicMixerFifo(CMixer* mixer, u32 sample_rate): MixerFifo(mixer, sample_rate)
		{}
		void Interpolate(u32 left_input_index, float* left_output, float* right_output) override;
		u32 GetWindowSize() override
		{
			return 8;
		};
	};

	CubicMixerFifo m_dma_mixer;
	CubicMixerFifo m_streaming_mixer;

	// Linear interpolation seems to be the best for Wiimote 3khz -> 48khz, for now.
	// TODO: figure out why and make it work with the above FIR
	LinearMixerFifo m_wiimote_speaker_mixer;

	u32 m_sample_rate;

	WaveFileWriter g_wave_writer_dtk;
	WaveFileWriter g_wave_writer_dsp;

	bool m_log_dtk_audio;
	bool m_log_dsp_audio;

	std::mutex m_cs_mixing;

	std::atomic<float> m_speed; // Current rate of the emulation (1.0 = 100% speed)

private:

	std::vector<float> m_output_buffer;
};

