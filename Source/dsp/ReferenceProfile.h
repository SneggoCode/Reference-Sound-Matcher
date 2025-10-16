#pragma once

#include <array>
#include <juce_core/juce_core.h>

namespace reference_tone_matcher
{
    /**
        Holds the spectral and dynamic fingerprint extracted from a reference recording.
        The profile is used to initialise processing modules and to populate plug-in parameters.
    */
    struct ReferenceProfile
    {
        std::array<float, 16> eqGainsDb{};   // Target gain per logarithmic band in dB.
        float rmsLevelDb = -18.0f;           // Average loudness estimate.
        float spectralSlope = 0.0f;          // Negative slope -> brighter tonality.
        float transientIntensity = 0.5f;     // Normalised transient index.
        float sparkle = 0.5f;                // Suggested sparkle amount.
        float bite = 0.5f;                   // Suggested transient emphasis.
        float glue = 0.5f;                   // Suggested compression strength.
        float crispAmount = 0.5f;            // Suggested overall crispness.
        juce::String sourceName;             // Name of the analysed file.
        bool isValid = false;                // True when analysis succeeded.
    };
}

