#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>

ReferenceToneMatcherAudioProcessor::ReferenceToneMatcherAudioProcessor()
    : AudioProcessor (BusesProperties()
                         .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "ReferenceToneMatcherParameters", createParameterLayout())
{
    parameters.state.addListener (this);
    lastEqValues.fill (0.0f);
}

ReferenceToneMatcherAudioProcessor::~ReferenceToneMatcherAudioProcessor()
{
    parameters.state.removeListener (this);
}

const juce::String ReferenceToneMatcherAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ReferenceToneMatcherAudioProcessor::acceptsMidi() const { return false; }
bool ReferenceToneMatcherAudioProcessor::producesMidi() const { return false; }
bool ReferenceToneMatcherAudioProcessor::isMidiEffect() const { return false; }
double ReferenceToneMatcherAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int ReferenceToneMatcherAudioProcessor::getNumPrograms() { return 1; }
int ReferenceToneMatcherAudioProcessor::getCurrentProgram() { return 0; }
void ReferenceToneMatcherAudioProcessor::setCurrentProgram (int) {}
const juce::String ReferenceToneMatcherAudioProcessor::getProgramName (int) { return {}; }
void ReferenceToneMatcherAudioProcessor::changeProgramName (int, const juce::String&) {}

void ReferenceToneMatcherAudioProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = static_cast<float> (newSampleRate);

    juce::dsp::ProcessSpec spec { newSampleRate, static_cast<juce::uint32> (samplesPerBlock), static_cast<juce::uint32> (getTotalNumOutputChannels()) };
    eqDesigner.prepare (spec);
    transientDesigner.prepare (spec);
    exciter.prepare (spec);
    dynamics.prepare (spec);

    updateWetDryBufferSize (samplesPerBlock);
    updateProcessingFromParameters();
}

void ReferenceToneMatcherAudioProcessor::releaseResources()
{
    eqDesigner.reset();
    transientDesigner.reset();
    exciter.reset();
    dynamics.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ReferenceToneMatcherAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void ReferenceToneMatcherAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateProcessingFromParameters();

    if (dryBuffer.getNumSamples() < buffer.getNumSamples())
        updateWetDryBufferSize (buffer.getNumSamples());

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, buffer.getNumSamples());

    juce::dsp::AudioBlock<float> block (buffer);

    eqDesigner.process (block);
    transientDesigner.process (block);
    exciter.process (block);
    dynamics.process (block);

    const float wet = *parameters.getRawParameterValue ("wet");
    const float dry = 1.0f - wet;

    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        const auto* dryData = dryBuffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = dryData[i] * dry + data[i] * wet;
    }
}

bool ReferenceToneMatcherAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ReferenceToneMatcherAudioProcessor::createEditor()
{
    return new ReferenceToneMatcherAudioProcessorEditor (*this);
}

void ReferenceToneMatcherAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState())
    {
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void ReferenceToneMatcherAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ValueTree tree = juce::ValueTree::readFromData (data, static_cast<size_t> (sizeInBytes));

    if (tree.isValid())
    {
        parameters.replaceState (tree);
        updateProcessingFromParameters();
    }
}

bool ReferenceToneMatcherAudioProcessor::analyseReferenceFile (const juce::File& file)
{
    auto profile = analyser.analyseFile (file, sampleRate);
    if (! profile.isValid)
        return false;

    currentProfile = profile;
    profileReady.store (true);

    for (size_t band = 0; band < profile.eqGainsDb.size(); ++band)
    {
        auto paramID = "band" + juce::String (static_cast<int> (band + 1));
        if (auto* param = parameters.getParameter (paramID))
        {
            const float value01 = param->convertTo0to1 (profile.eqGainsDb[band]);
            param->setValueNotifyingHost (value01);
        }
    }

    if (auto* sparkleParam = parameters.getParameter ("sparkle"))
        sparkleParam->setValueNotifyingHost (sparkleParam->convertTo0to1 (profile.sparkle));

    if (auto* biteParam = parameters.getParameter ("bite"))
        biteParam->setValueNotifyingHost (biteParam->convertTo0to1 (profile.bite));

    if (auto* glueParam = parameters.getParameter ("glue"))
        glueParam->setValueNotifyingHost (glueParam->convertTo0to1 (profile.glue));

    if (auto* crispParam = parameters.getParameter ("crispAmount"))
        crispParam->setValueNotifyingHost (crispParam->convertTo0to1 (profile.crispAmount));

    return true;
}

reference_tone_matcher::ReferenceProfile ReferenceToneMatcherAudioProcessor::getCurrentProfile() const
{
    return currentProfile;
}

void ReferenceToneMatcherAudioProcessor::updateWetDryBufferSize (int samplesPerBlock)
{
    dryBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock, false, false, true);
    dryBuffer.clear();
}

void ReferenceToneMatcherAudioProcessor::updateProcessingFromParameters()
{
    for (size_t i = 0; i < lastEqValues.size(); ++i)
    {
        const auto* valuePtr = parameters.getRawParameterValue ("band" + juce::String (static_cast<int> (i + 1)));
        if (valuePtr == nullptr)
            continue;

        const float gainDb = *valuePtr;
        if (! juce::approximatelyEqual (gainDb, lastEqValues[i]))
        {
            eqDesigner.setBandGain (i, gainDb);
            lastEqValues[i] = gainDb;
        }
    }

    const float bite = *parameters.getRawParameterValue ("bite");
    const float sparkle = *parameters.getRawParameterValue ("sparkle");
    const float crisp = *parameters.getRawParameterValue ("crispAmount");
    const float glueAmount = *parameters.getRawParameterValue ("glue");

    transientDesigner.setAmount (bite);
    exciter.setAmounts (crisp, sparkle);
    dynamics.setAmount (glueAmount);
}

juce::AudioProcessorValueTreeState::ParameterLayout ReferenceToneMatcherAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.reserve (16 + 5);

    for (int i = 0; i < 16; ++i)
    {
        auto paramID = "band" + juce::String (i + 1);
        params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { paramID, 1 },
                                                                      "Band " + juce::String (i + 1),
                                                                      juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f),
                                                                      0.0f));
    }

    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "wet", 1 },
                                                                   "Wet",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                                   1.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "crispAmount", 1 },
                                                                   "Crisp Amount",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                                   0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "sparkle", 1 },
                                                                   "Sparkle",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                                   0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "bite", 1 },
                                                                   "Bite",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                                   0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "glue", 1 },
                                                                   "Glue",
                                                                   juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                                                                   0.5f));

    return { params.begin(), params.end() };
}

void ReferenceToneMatcherAudioProcessor::parameterChanged (const juce::String&, float)
{
}

void ReferenceToneMatcherAudioProcessor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReferenceToneMatcherAudioProcessor();
}

