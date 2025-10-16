#pragma once

#include <array>
#include <memory>
#include <juce_dsp/juce_dsp.h>

namespace reference_tone_matcher
{
    /**
        Adds high frequency sparkle by generating controlled harmonic content with oversampling.
    */
    class Exciter
    {
    public:
        Exciter() = default;

        void prepare (const juce::dsp::ProcessSpec& spec);
        void reset() noexcept;
        void setAmounts (float crisp, float sparkle) noexcept;
        void process (juce::dsp::AudioBlock<float>& block) noexcept;

    private:
        float crispAmount = 0.5f;
        float sparkleAmount = 0.5f;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
        std::array<juce::dsp::IIR::Filter<float>, 2> highpassFilters;
        juce::dsp::WaveShaper<float> shaper { [] (float x) { return std::tanh (x); } };
        float driveLinear = 1.0f;
        juce::AudioBuffer<float> dryBuffer;
        juce::dsp::ProcessSpec currentSpec{};
    };
}

