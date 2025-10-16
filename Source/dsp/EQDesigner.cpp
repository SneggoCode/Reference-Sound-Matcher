#include "EQDesigner.h"

#include <cmath>

namespace reference_tone_matcher
{
    void EQDesigner::prepare (const juce::dsp::ProcessSpec& spec)
    {
        currentSpec = spec;
        isPrepared = true;

        const double minFreq = 80.0;
        const double maxFreq = 16000.0;
        const double ratio = std::pow (maxFreq / minFreq, 1.0 / 15.0);
        bandFrequencies[0] = static_cast<float> (minFreq);
        for (size_t i = 1; i < bandFrequencies.size(); ++i)
            bandFrequencies[i] = static_cast<float> (bandFrequencies[i - 1] * ratio);

        for (auto& channelFilters : filters)
            for (auto& filter : channelFilters)
                filter.prepare (spec);

        reset();
    }

    void EQDesigner::reset() noexcept
    {
        for (auto& channelFilters : filters)
            for (auto& filter : channelFilters)
                filter.reset();
    }

    void EQDesigner::setBandGain (size_t index, float gainDb)
    {
        if (index >= bandGainsDb.size())
            return;

        bandGainsDb[index] = gainDb;
        updateBandCoefficients (index);
    }

    void EQDesigner::process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        jassert (block.getNumChannels() <= filters.size());
        if (! isPrepared)
            return;

        for (size_t ch = 0; ch < static_cast<size_t> (block.getNumChannels()); ++ch)
        {
            auto channelBlock = block.getSingleChannelBlock (ch);
            for (size_t band = 0; band < filters[ch].size(); ++band)
                filters[ch][band].process (juce::dsp::ProcessContextReplacing<float> (channelBlock));
        }
    }

    void EQDesigner::updateBandCoefficients (size_t band)
    {
        if (! isPrepared || band >= bandFrequencies.size())
            return;

        const auto frequency = static_cast<double> (bandFrequencies[band]);
        const auto gainLinear = juce::Decibels::decibelsToGain (bandGainsDb[band]);
        auto coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (currentSpec.sampleRate,
                                                                                frequency,
                                                                                qFactor,
                                                                                gainLinear);
        for (auto& channelFilters : filters)
            channelFilters[band].coefficients = coefficients;
    }
}

