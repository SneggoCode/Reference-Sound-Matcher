#pragma once

#include "ReferenceProfile.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

namespace reference_tone_matcher
{
    /**
        Performs FFT based analysis for the reference file and converts results into a ReferenceProfile.
        The analyser is lightweight enough to run on the message thread when a new file is selected.
    */
    class SpectrumAnalyser
    {
    public:
        SpectrumAnalyser();

        [[nodiscard]] ReferenceProfile analyseFile (const juce::File& file, double targetSampleRate);

    private:
        ReferenceProfile buildProfileFromBuffer (juce::AudioBuffer<float>& buffer, double sampleRate) const;
        std::array<float, 16> computeAverageBandMagnitudes (const juce::AudioBuffer<float>& buffer,
                                                            double sampleRate) const;
        float computeSpectralSlope (const std::array<float, 16>& bandMagnitudesDb) const;
        float computeTransientIntensity (const juce::AudioBuffer<float>& buffer, double sampleRate) const;

        juce::AudioFormatManager formatManager;
        static constexpr int fftOrder = 12;      // 4096 point FFT.
        static constexpr int fftSize = 1 << fftOrder;
        mutable juce::dsp::FFT fft { fftOrder };
        mutable juce::AudioBuffer<float> windowBuffer;
        mutable juce::HeapBlock<float> fftScratch;
    };
}

