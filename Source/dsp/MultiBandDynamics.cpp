#include "MultiBandDynamics.h"

namespace reference_tone_matcher
{
    void MultiBandDynamics::prepare (const juce::dsp::ProcessSpec& spec)
    {
        currentSpec = spec;
        lowMidCrossover.reset();
        midHighCrossover.reset();

        lowMidCrossover.setCutoffFrequency (200.0f);
        midHighCrossover.setCutoffFrequency (4000.0f);

        lowMidCrossover.prepare (spec);
        midHighCrossover.prepare (spec);

        lowBandComp.prepare (spec);
        midBandComp.prepare (spec);
        highBandComp.prepare (spec);

        reset();
    }

    void MultiBandDynamics::reset() noexcept
    {
        lowMidCrossover.reset();
        midHighCrossover.reset();
        lowBandComp.reset();
        midBandComp.reset();
        highBandComp.reset();
    }

    void MultiBandDynamics::setAmount (float glueAmount) noexcept
    {
        glue = juce::jlimit (0.0f, 1.0f, glueAmount);

        const float thresholdLow = juce::jmap (glue, 0.0f, 1.0f, -6.0f, -20.0f);
        const float thresholdMid = juce::jmap (glue, 0.0f, 1.0f, -8.0f, -18.0f);
        const float thresholdHigh = juce::jmap (glue, 0.0f, 1.0f, -10.0f, -16.0f);
        const float ratio = juce::jmap (glue, 0.0f, 1.0f, 1.2f, 3.5f);

        lowBandComp.setThreshold (thresholdLow);
        lowBandComp.setRatio (ratio);
        lowBandComp.setAttack (juce::jmap (glue, 0.0f, 1.0f, 25.0f, 5.0f));
        lowBandComp.setRelease (juce::jmap (glue, 0.0f, 1.0f, 120.0f, 80.0f));

        midBandComp.setThreshold (thresholdMid);
        midBandComp.setRatio (ratio + 0.2f);
        midBandComp.setAttack (juce::jmap (glue, 0.0f, 1.0f, 15.0f, 4.0f));
        midBandComp.setRelease (juce::jmap (glue, 0.0f, 1.0f, 100.0f, 70.0f));

        highBandComp.setThreshold (thresholdHigh);
        highBandComp.setRatio (ratio + 0.5f);
        highBandComp.setAttack (juce::jmap (glue, 0.0f, 1.0f, 10.0f, 2.0f));
        highBandComp.setRelease (juce::jmap (glue, 0.0f, 1.0f, 80.0f, 50.0f));
        highBandComp.setMakeUpGainDecibels (juce::jmap (glue, 0.0f, 1.0f, 0.0f, 2.5f));
    }

    void MultiBandDynamics::process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        if (block.getNumSamples() == 0)
            return;

        auto lowBand = lowMidCrossover.processLowpass (block);
        auto remainder = lowMidCrossover.processHighpass (block);
        auto midBand = midHighCrossover.processLowpass (remainder);
        auto highBand = midHighCrossover.processHighpass (remainder);

        lowBandComp.process (juce::dsp::ProcessContextReplacing<float> (lowBand));
        midBandComp.process (juce::dsp::ProcessContextReplacing<float> (midBand));
        highBandComp.process (juce::dsp::ProcessContextReplacing<float> (highBand));

        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto* dest = block.getChannelPointer (ch);
            const auto* low = lowBand.getChannelPointer (ch);
            const auto* mid = midBand.getChannelPointer (ch);
            const auto* high = highBand.getChannelPointer (ch);

            for (size_t i = 0; i < block.getNumSamples(); ++i)
                dest[i] = low[i] + mid[i] + high[i];
        }
    }
}

