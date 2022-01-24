#ifndef WAVETABLE_EXCITER_HPP
#define WAVETABLE_EXCITER_HPP

#include <cmath>

#define PI 3.14159265359

#define SAMPLE_RATE 44100
#define SAMPLES_PER_MILLISECOND SAMPLE_RATE / 1000.0

//There might be a optimization worth implementing involving how the table wraps back to the beginning worth checking out - https://docs.juce.com/master/tutorial_wavetable_synth.html//
class WavetableSynth
{
private:
	int wavetableSize = 0;
	float* wavetable;
	float currentIndex = 0.0f, tableDelta = 0.0f;

	double midiToFreq(int midiNote)
	{
		return 440 * (pow(2, (midiNote - 69) / 12.0));
	}
public:
	WavetableSynth() {}
	WavetableSynth(const float* aWavetableBuffer, const int aWavetableSize)
	{
		wavetableSize = aWavetableSize;
		wavetable = new float[wavetableSize];
		for (int i = 0; i != wavetableSize; ++i)
			wavetable[i] = aWavetableBuffer[i];
	}
	~WavetableSynth()
	{
		delete wavetable;	//This breaks it.
	}
	void setMidiNote(int midiNote, float sampleRate)
	{
		double frequency = midiToFreq(midiNote);
		tableDelta = frequency * (wavetableSize / sampleRate);
	}
	void setFrequency(float frequency, float sampleRate)
	{
		tableDelta = frequency * (wavetableSize / sampleRate);
	}
	float getNextSample() noexcept
	{
		auto index0 = (unsigned int)currentIndex;
		auto index1 = index0 == (wavetableSize - 1) ? (unsigned int)0 : index0 + 1;
		auto frac = currentIndex - (float)index0;
		float* table = wavetable;
		auto value0 = table[index0];
		auto value1 = table[index1];
		auto currentSample = value0 + frac * (value1 - value0);
		if ((currentIndex += tableDelta) > wavetableSize)
			currentIndex -= wavetableSize;
		return currentSample;
	}
	void setCurrentIndex(const float aCurrentIndex)
	{
		currentIndex = aCurrentIndex;
	}
};

class WavetableExciter
{
private:
	WavetableSynth wavetableSynth;

	unsigned int excitationNumSamples;
public:
	int index = 0;
	WavetableExciter(const int duration, const float* wavetableBuffer, const size_t wavetableSize) : wavetableSynth(wavetableBuffer, wavetableSize)
	{
		excitationNumSamples = duration * SAMPLES_PER_MILLISECOND;
		wavetableSynth.setFrequency(1440, 44100);
	}
	~WavetableExciter() {}
	float getNextSample()
	{
		++index;
		return wavetableSynth.getNextSample();
	}
	void setDuration(int aDuration)
	{
		excitationNumSamples = aDuration * SAMPLES_PER_MILLISECOND;
	}
	void resetExcitation()
	{
		index = 0;
		wavetableSynth.setCurrentIndex(0.0);
	}
	bool isExcitation()
	{
		if (index > excitationNumSamples)
			return false;
		return true;
	}
};

class SineOscillator
{
private:
	float currentAngle = 0.0f;	//Current angle/phase of wave.
	float angleDelta = 0.0f;	//Amount to increment between each cycle - Depending on frequency and sample rate.
public:
	SineOscillator(int sampleRate)
	{
		float midiNote = 70.0;
		float frequency = 440.0 * pow(2.0, (midiNote - 69.0) / 12.0);
		setFrequency(frequency, sampleRate);
	}
	void setFrequency(float frequency, float sampleRate)
	{
		auto cyclesPerSample = frequency / sampleRate;
		angleDelta = cyclesPerSample * 2 * PI;
	}
	void updateAngle() noexcept
	{
		currentAngle += angleDelta;
		if (currentAngle >= 2 * PI)
			currentAngle -= 2 * PI;
	}
	float getNextSample() noexcept
	{
		auto currentSample = std::sin(currentAngle);
		updateAngle();
		return currentSample;
	}
	void getAudioBlock(int bufferSize, float* buffer)
	{
		for (int i = 0; i != bufferSize; ++i)
		{
			*buffer = getNextSample();
			++buffer;
		}
	}
};

#endif