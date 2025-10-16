#pragma once

#include <juce_dsp/juce_dsp.h>

namespace reference_tone_matcher
{
    /**
        Simple transient shaper emphasising attacks using dual envelope followers.
    */
    class TransientDesigner
    {
    public:
        TransientDesigner() = default;

        void prepare (const juce::dsp::ProcessSpec& spec);
        void reset() noexcept;
        void setAmount (float biteAmount) noexcept;
        void process (juce::dsp::AudioBlock<float>& block) noexcept;

    private:
        juce::dsp::ProcessSpec currentSpec{};
        float bite = 0.5f;
        std::vector<float> envelopeFast;
        std::vector<float> envelopeSlow;
    };
}

