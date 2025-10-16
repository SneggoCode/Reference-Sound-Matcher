#pragma once

#include <array>
#include <juce_dsp/juce_dsp.h>

namespace reference_tone_matcher
{
    /**
        Implements a 16 band peaking EQ that matches the spectral signature of the reference profile.
    */
    class EQDesigner
    {
    public:
        EQDesigner() = default;

        void prepare (const juce::dsp::ProcessSpec& spec);
        void reset() noexcept;
        void setBandGain (size_t index, float gainDb);
        void setQFactor (float newQ) noexcept { qFactor = newQ; }
        void process (juce::dsp::AudioBlock<float>& block) noexcept;

    private:
        void updateBandCoefficients (size_t band);

        juce::dsp::ProcessSpec currentSpec{};
        bool isPrepared = false;
        float qFactor = 1.0f;
        std::array<float, 16> bandFrequencies{};
        std::array<float, 16> bandGainsDb{};
        std::array<std::array<juce::dsp::IIR::Filter<float>, 16>, 2> filters;
    };
}

