#include "Exciter.h"

namespace reference_tone_matcher
{
    void Exciter::prepare (const juce::dsp::ProcessSpec& spec)
    {
        currentSpec = spec;
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (static_cast<size_t> (spec.numChannels),
                                                                         2,
                                                                         juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
        oversampling->initProcessing (static_cast<size_t> (spec.maximumBlockSize));

        const auto oversampledRate = spec.sampleRate * static_cast<double> (1 << 2);
        juce::dsp::ProcessSpec hpSpec { oversampledRate,
                                        static_cast<juce::uint32> (static_cast<size_t> (spec.maximumBlockSize) * static_cast<size_t> (1 << 2)),
                                        1 };
        for (auto& filter : highpassFilters)
        {
            filter.prepare (hpSpec);
            filter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (oversampledRate, 6000.0, 0.707f);
        }

        reset();

        dryBuffer.setSize (static_cast<int> (spec.numChannels), static_cast<int> (spec.maximumBlockSize));
        dryBuffer.clear();
    }

    void Exciter::reset() noexcept
    {
        if (oversampling != nullptr)
            oversampling->reset();

        for (auto& filter : highpassFilters)
            filter.reset();

        driveLinear = 1.0f;
    }

    void Exciter::setAmounts (float crisp, float sparkle) noexcept
    {
        crispAmount = juce::jlimit (0.0f, 1.0f, crisp);
        sparkleAmount = juce::jlimit (0.0f, 1.0f, sparkle);

        const float driveDb = juce::jmap (crispAmount, 0.0f, 1.0f, -6.0f, 10.0f)
                            + juce::jmap (sparkleAmount, 0.0f, 1.0f, 0.0f, 8.0f);
        driveLinear = juce::Decibels::decibelsToGain (driveDb);
    }

    void Exciter::process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        if (oversampling == nullptr)
            return;

        const auto numChannels = static_cast<int> (block.getNumChannels());
        const auto numSamples = static_cast<int> (block.getNumSamples());

        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom (ch, 0, block.getChannelPointer (static_cast<size_t> (ch)), numSamples);

        auto oversampledBlock = oversampling->processSamplesUp (block);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto channelBlock = oversampledBlock.getSingleChannelBlock (static_cast<size_t> (ch));
            highpassFilters[static_cast<size_t> (ch)].process (juce::dsp::ProcessContextReplacing<float> (channelBlock));
            channelBlock.multiplyBy (driveLinear);
            shaper.process (juce::dsp::ProcessContextReplacing<float> (channelBlock));
        }

        oversampling->processSamplesDown (block);

        const float wetAmount = juce::jlimit (0.0f, 1.0f, 0.2f + 0.8f * sparkleAmount * crispAmount);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* dst = block.getChannelPointer (static_cast<size_t> (ch));
            const float* dry = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                dst[i] = dry[i] + wetAmount * dst[i];
        }
    }
}

