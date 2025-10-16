#include "TransientDesigner.h"

#include <algorithm>
#include <cmath>

namespace reference_tone_matcher
{
    void TransientDesigner::prepare (const juce::dsp::ProcessSpec& spec)
    {
        currentSpec = spec;
        const size_t totalSamples = spec.maximumBlockSize * spec.numChannels;
        envelopeFast.resize (totalSamples);
        envelopeSlow.resize (totalSamples);
        reset();
    }

    void TransientDesigner::reset() noexcept
    {
        std::fill (envelopeFast.begin(), envelopeFast.end(), 0.0f);
        std::fill (envelopeSlow.begin(), envelopeSlow.end(), 0.0f);
    }

    void TransientDesigner::setAmount (float biteAmount) noexcept
    {
        bite = juce::jlimit (0.0f, 1.0f, biteAmount);
    }

    void TransientDesigner::process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        if (block.getNumSamples() == 0)
            return;

        const float fastAttack = std::exp (-1.0f / (0.001f * static_cast<float> (currentSpec.sampleRate)));
        const float fastRelease = std::exp (-1.0f / (0.02f * static_cast<float> (currentSpec.sampleRate)));
        const float slowAttack = std::exp (-1.0f / (0.01f * static_cast<float> (currentSpec.sampleRate)));
        const float slowRelease = std::exp (-1.0f / (0.2f * static_cast<float> (currentSpec.sampleRate)));

        const float amount = juce::jmap (bite, 0.0f, 1.0f, 0.0f, 0.6f);

        size_t idx = 0;
        for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
        {
            auto channelBlock = block.getSingleChannelBlock (channel);
            auto* data = channelBlock.getChannelPointer();

            for (size_t sample = 0; sample < channelBlock.getNumSamples(); ++sample)
            {
                const float input = std::abs (data[sample]);
                auto& fastEnv = envelopeFast[idx];
                auto& slowEnv = envelopeSlow[idx];

                fastEnv = (input > fastEnv) ? fastAttack * fastEnv + (1.0f - fastAttack) * input
                                            : fastRelease * fastEnv + (1.0f - fastRelease) * input;

                slowEnv = (input > slowEnv) ? slowAttack * slowEnv + (1.0f - slowAttack) * input
                                            : slowRelease * slowEnv + (1.0f - slowRelease) * input;

                const float diff = juce::jlimit (-1.0f, 1.0f, fastEnv - slowEnv);
                data[sample] += amount * diff * data[sample];
                ++idx;
            }
        }
    }
}

