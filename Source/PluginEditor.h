#pragma once

#include <array>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

/**
    ReferenceProfileView renders the 16 band EQ curve currently applied by the processor.
*/
class ReferenceProfileView : public juce::Component,
                             private juce::Timer
{
public:
    explicit ReferenceProfileView (ReferenceToneMatcherAudioProcessor& proc);

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    ReferenceToneMatcherAudioProcessor& processor;
    std::array<float, 16> cachedGains{};
};

/**
    Provides the graphical user interface for the ReferenceToneMatcher plug-in.
    It displays the analysis controls, EQ bands and enhancement parameters.
*/
class ReferenceToneMatcherAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit ReferenceToneMatcherAudioProcessorEditor (ReferenceToneMatcherAudioProcessor&);
    ~ReferenceToneMatcherAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void configureSlider (juce::Slider& slider, juce::Slider::SliderStyle style, const juce::String& suffix = {});
    void updateProfileLabel();

    ReferenceToneMatcherAudioProcessor& processor;

    juce::TextButton loadButton { "Referenz laden" };
    juce::Label profileLabel;

    std::array<juce::Slider, 16> bandSliders;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 16> bandAttachments;

    juce::Slider crispSlider;
    juce::Slider sparkleSlider;
    juce::Slider biteSlider;
    juce::Slider glueSlider;
    juce::Slider wetSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> crispAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sparkleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> biteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glueAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetAttachment;

    ReferenceProfileView profileView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceToneMatcherAudioProcessorEditor)
};

