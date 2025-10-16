#pragma once

#include <juce_dsp/juce_dsp.h>

namespace reference_tone_matcher
{
    /**
        Three band dynamics processor that glues the spectrum together with musical compression.
    */
    class MultiBandDynamics
    {
    public:
        MultiBandDynamics() = default;

        void prepare (const juce::dsp::ProcessSpec& spec);
        void reset() noexcept;
        void setAmount (float glueAmount) noexcept;
        void process (juce::dsp::AudioBlock<float>& block) noexcept;

    private:
        juce::dsp::ProcessSpec currentSpec{};
        juce::dsp::LinkwitzRileyFilter<float> lowMidCrossover;
        juce::dsp::LinkwitzRileyFilter<float> midHighCrossover;
        juce::dsp::Compressor<float> lowBandComp;
        juce::dsp::Compressor<float> midBandComp;
        juce::dsp::Compressor<float> highBandComp;
        float glue = 0.5f;
    };
}

