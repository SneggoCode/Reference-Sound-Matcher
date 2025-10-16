#pragma once

#include <array>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/ReferenceProfile.h"
#include "dsp/SpectrumAnalyser.h"
#include "dsp/EQDesigner.h"
#include "dsp/Exciter.h"
#include "dsp/TransientDesigner.h"
#include "dsp/MultiBandDynamics.h"

/**
    ReferenceToneMatcherAudioProcessor orchestrates the DSP chain for the ReferenceToneMatcher plug-in.
    It manages parameter state, handles reference profile analysis and processes incoming audio blocks.
*/
class ReferenceToneMatcherAudioProcessor  : public juce::AudioProcessor,
                                            private juce::ValueTree::Listener
{
public:
    ReferenceToneMatcherAudioProcessor();
    ~ReferenceToneMatcherAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return parameters; }

    bool analyseReferenceFile (const juce::File& file);
    reference_tone_matcher::ReferenceProfile getCurrentProfile() const;

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState parameters;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateProcessingFromParameters();
    void parameterChanged (const juce::String& parameterID, float newValue);
    void valueTreePropertyChanged (juce::ValueTree& treeWhosePropertyHasChanged,
                                   const juce::Identifier& property) override;

    void updateWetDryBufferSize (int samplesPerBlock);

    juce::AudioBuffer<float> dryBuffer;
    std::array<float, 16> lastEqValues{};

    reference_tone_matcher::SpectrumAnalyser analyser;
    reference_tone_matcher::ReferenceProfile currentProfile;
    reference_tone_matcher::EQDesigner eqDesigner;
    reference_tone_matcher::TransientDesigner transientDesigner;
    reference_tone_matcher::Exciter exciter;
    reference_tone_matcher::MultiBandDynamics dynamics;

    std::atomic<bool> profileReady { false };
    float sampleRate = 44100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceToneMatcherAudioProcessor)
};

