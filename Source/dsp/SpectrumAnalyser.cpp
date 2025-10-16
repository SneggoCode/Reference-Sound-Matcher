#include "SpectrumAnalyser.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace reference_tone_matcher
{
    SpectrumAnalyser::SpectrumAnalyser()
    {
        formatManager.registerBasicFormats();
        windowBuffer.setSize (1, fftSize);
        fftScratch.allocate (2 * fftSize, true);
    }

    ReferenceProfile SpectrumAnalyser::analyseFile (const juce::File& file, double targetSampleRate)
    {
        ReferenceProfile profile;

        if (! file.existsAsFile())
            return profile;

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr)
            return profile;

        const int64 numSamples64 = reader->lengthInSamples;
        const int numSamples = static_cast<int> (juce::jlimit<int64> (0, std::numeric_limits<int>::max(), numSamples64));
        if (numSamples <= 0)
            return profile;

        juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), numSamples);
        reader->read (&buffer, 0, numSamples, 0, true, true);

        // Resample if required so that the analysis matches the current processing sample rate.
        const double readerSampleRate = reader->sampleRate;
        const double resampleRatio = (targetSampleRate > 0.0) ? (targetSampleRate / readerSampleRate) : 1.0;

        if (std::abs (resampleRatio - 1.0) > 0.01)
        {
            const int resampledLength = static_cast<int> (std::ceil (numSamples * resampleRatio));
            juce::AudioBuffer<float> resampled (buffer.getNumChannels(), resampledLength);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                juce::LagrangeInterpolator interpolator;
                interpolator.reset();
                const float* src = buffer.getReadPointer (ch);
                float* dst = resampled.getWritePointer (ch);
                int numDone = interpolator.process (resampleRatio, src, dst, resampledLength);
                juce::ignoreUnused (numDone);
            }

            buffer = std::move (resampled);
        }

        profile = buildProfileFromBuffer (buffer, targetSampleRate > 0.0 ? targetSampleRate : readerSampleRate);
        profile.sourceName = file.getFileName();
        profile.isValid = true;

        return profile;
    }

    ReferenceProfile SpectrumAnalyser::buildProfileFromBuffer (juce::AudioBuffer<float>& buffer, double sampleRate) const
    {
        ReferenceProfile profile;
        profile.eqGainsDb = computeAverageBandMagnitudes (buffer, sampleRate);
        profile.spectralSlope = computeSpectralSlope (profile.eqGainsDb);
        profile.transientIntensity = computeTransientIntensity (buffer, sampleRate);

        const float highBandAverage = juce::jlimit (-24.0f, 24.0f, (profile.eqGainsDb[12] + profile.eqGainsDb[13] + profile.eqGainsDb[14] + profile.eqGainsDb[15]) * 0.25f);
        const float midBandAverage = juce::jlimit (-24.0f, 24.0f, (profile.eqGainsDb[6] + profile.eqGainsDb[7] + profile.eqGainsDb[8]) / 3.0f);
        profile.sparkle = juce::jlimit (0.0f, 1.0f, juce::jmap (highBandAverage - midBandAverage, -6.0f, 6.0f, 0.1f, 0.9f));
        profile.bite = juce::jlimit (0.0f, 1.0f, juce::jmap (profile.transientIntensity, 0.1f, 0.8f, 0.2f, 0.9f));
        profile.glue = juce::jlimit (0.0f, 1.0f, juce::jmap (profile.transientIntensity, 0.2f, 0.7f, 0.8f, 0.2f));
        profile.crispAmount = juce::jlimit (0.0f, 1.0f, juce::jmap (profile.sparkle, 0.0f, 1.0f, 0.3f, 1.0f));

        float rms = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            rms += buffer.getRMSLevel (ch, 0, buffer.getNumSamples());

        rms /= static_cast<float> (buffer.getNumChannels());
        profile.rmsLevelDb = juce::Decibels::gainToDecibels (rms + 1.0e-6f);

        return profile;
    }

    std::array<float, 16> SpectrumAnalyser::computeAverageBandMagnitudes (const juce::AudioBuffer<float>& buffer,
                                                                          double sampleRate) const
    {
        std::array<float, 16> bandAveragesDb{};
        bandAveragesDb.fill (0.0f);

        const int totalSamples = buffer.getNumSamples();
        if (totalSamples < fftSize)
            return bandAveragesDb;

        juce::AudioBuffer<float> monoBuffer (1, totalSamples);
        monoBuffer.clear();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            monoBuffer.addFrom (0, 0, buffer, ch, 0, totalSamples, 1.0f / static_cast<float> (buffer.getNumChannels()));

        juce::dsp::WindowingFunction<float> window (fftSize, juce::dsp::WindowingFunction<float>::hann);
        const int hopSize = fftSize / 2;
        const int numHops = juce::jmax (1, 1 + (totalSamples - fftSize) / hopSize);
        if (numHops <= 0)
            return bandAveragesDb;

        std::array<float, 16> energySum{};
        energySum.fill (0.0f);

        float* scratch = fftScratch.getData();

        std::array<double, 17> bandEdgesHz{};
        const double minFreq = 80.0;
        const double maxFreq = 16000.0;
        const double ratio = std::pow (maxFreq / minFreq, 1.0 / 16.0);
        bandEdgesHz[0] = minFreq;
        for (size_t i = 1; i <= 16; ++i)
            bandEdgesHz[i] = bandEdgesHz[i - 1] * ratio;

        for (int hop = 0; hop < numHops; ++hop)
        {
            float* windowData = windowBuffer.getWritePointer (0);
            const float* src = monoBuffer.getReadPointer (0, hop * hopSize);
            std::memcpy (windowData, src, sizeof (float) * fftSize);
            window.multiplyWithWindowingTable (windowData, fftSize);

            std::fill (scratch, scratch + 2 * fftSize, 0.0f);
            std::memcpy (scratch, windowData, sizeof (float) * fftSize);
            fft.performRealOnlyForwardTransform (scratch);

            const int spectrumSize = fftSize / 2;
            for (int band = 0; band < 16; ++band)
            {
                const double lowFreq = bandEdgesHz[band];
                const double highFreq = bandEdgesHz[band + 1];
                const int lowBin = static_cast<int> (juce::jlimit (0.0, static_cast<double> (spectrumSize - 1), std::floor (lowFreq * fftSize / sampleRate)));
                const int highBin = static_cast<int> (juce::jlimit (0.0, static_cast<double> (spectrumSize - 1), std::ceil (highFreq * fftSize / sampleRate)));
                float magnitudeSum = 0.0f;
                const int binCount = juce::jmax (1, highBin - lowBin);

                for (int bin = lowBin; bin < highBin; ++bin)
                {
                    const float real = scratch[bin * 2];
                    const float imag = scratch[bin * 2 + 1];
                    magnitudeSum += std::sqrt (real * real + imag * imag);
                }

                energySum[band] += magnitudeSum / static_cast<float> (binCount);
            }
        }

        const float normalisation = 1.0f / static_cast<float> (numHops);
        float globalAverage = 0.0f;
        for (int band = 0; band < 16; ++band)
        {
            const float average = energySum[band] * normalisation + 1.0e-6f;
            bandAveragesDb[band] = juce::Decibels::gainToDecibels (average);
            globalAverage += bandAveragesDb[band];
        }

        globalAverage /= 16.0f;
        for (float& value : bandAveragesDb)
            value -= globalAverage;

        return bandAveragesDb;
    }

    float SpectrumAnalyser::computeSpectralSlope (const std::array<float, 16>& bandMagnitudesDb) const
    {
        double sumX = 0.0;
        double sumY = 0.0;
        double sumXY = 0.0;
        double sumX2 = 0.0;
        const double n = static_cast<double> (bandMagnitudesDb.size());

        for (size_t i = 0; i < bandMagnitudesDb.size(); ++i)
        {
            const double x = static_cast<double> (i);
            const double y = static_cast<double> (bandMagnitudesDb[i]);
            sumX += x;
            sumY += y;
            sumXY += x * y;
            sumX2 += x * x;
        }

        const double denominator = (n * sumX2 - sumX * sumX);
        if (std::abs (denominator) < 1.0e-6)
            return 0.0f;

        const double slope = (n * sumXY - sumX * sumY) / denominator;
        return static_cast<float> (slope);
    }

    float SpectrumAnalyser::computeTransientIntensity (const juce::AudioBuffer<float>& buffer, double sampleRate) const
    {
        const int numSamples = buffer.getNumSamples();
        const float attackTime = 0.003f;
        const float releaseTime = 0.05f;
        const float attackCoeff = std::exp (-1.0f / (attackTime * static_cast<float> (sampleRate)));
        const float releaseCoeff = std::exp (-1.0f / (releaseTime * static_cast<float> (sampleRate)));

        float peakEnvelope = 0.0f;
        float sustainEnvelope = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float sample = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                sample += std::abs (buffer.getSample (ch, i));

            sample /= static_cast<float> (buffer.getNumChannels());

            if (sample > peakEnvelope)
                peakEnvelope = attackCoeff * peakEnvelope + (1.0f - attackCoeff) * sample;
            else
                peakEnvelope = releaseCoeff * peakEnvelope + (1.0f - releaseCoeff) * sample;

            sustainEnvelope = 0.999f * sustainEnvelope + 0.001f * sample;
        }

        const float ratio = juce::jlimit (0.0f, 1.0f, peakEnvelope / (sustainEnvelope + 1.0e-6f));
        return ratio;
    }
}

