#include "PluginEditor.h"

ReferenceProfileView::ReferenceProfileView (ReferenceToneMatcherAudioProcessor& proc)
    : processor (proc)
{
    cachedGains.fill (0.0f);
    startTimerHz (15);
}

void ReferenceProfileView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.fillAll (juce::Colours::transparentBlack);

    g.setColour (juce::Colours::white.withAlpha (0.1f));
    for (int i = 0; i <= 4; ++i)
    {
        auto y = juce::jmap (static_cast<float> (i), 0.0f, 4.0f, area.getBottom(), area.getY());
        g.drawHorizontalLine (static_cast<int> (y), area.getX(), area.getRight());
    }

    juce::Path curve;
    const float width = area.getWidth();
    const float height = area.getHeight();

    for (size_t i = 0; i < cachedGains.size(); ++i)
    {
        const float x = area.getX() + (width / static_cast<float> (cachedGains.size() - 1)) * static_cast<float> (i);
        const float normalised = juce::jmap (cachedGains[i], -12.0f, 12.0f, 1.0f, 0.0f);
        const float y = area.getY() + height * normalised;

        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    g.setColour (juce::Colours::white);
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}

void ReferenceProfileView::timerCallback()
{
    for (size_t i = 0; i < cachedGains.size(); ++i)
    {
        if (const auto* value = processor.getValueTreeState().getRawParameterValue ("band" + juce::String (static_cast<int> (i + 1))))
            cachedGains[i] = *value;
    }

    repaint();
}

ReferenceToneMatcherAudioProcessorEditor::ReferenceToneMatcherAudioProcessorEditor (ReferenceToneMatcherAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), profileView (p)
{
    setSize (940, 540);
    setResizable (false, false);

    profileLabel.setJustificationType (juce::Justification::centredLeft);
    profileLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (profileLabel);

    loadButton.onClick = [this]
    {
        juce::FileChooser chooser ("Referenzdatei wählen", juce::File(), "*.wav;*.flac;*.mp3");
        if (chooser.browseForFileToOpen())
        {
            auto result = chooser.getResult();
            if (processor.analyseReferenceFile (result))
                updateProfileLabel();
        }
    };
    addAndMakeVisible (loadButton);

    auto& state = processor.getValueTreeState();

    for (size_t i = 0; i < bandSliders.size(); ++i)
    {
        configureSlider (bandSliders[i], juce::Slider::LinearVertical, " dB");
        bandSliders[i].setRange (-12.0, 12.0, 0.01);
        addAndMakeVisible (bandSliders[i]);

        auto paramID = "band" + juce::String (static_cast<int> (i + 1));
        bandAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, bandSliders[i]);
    }

    configureSlider (crispSlider, juce::Slider::LinearHorizontal);
    configureSlider (sparkleSlider, juce::Slider::LinearHorizontal);
    configureSlider (biteSlider, juce::Slider::LinearHorizontal);
    configureSlider (glueSlider, juce::Slider::LinearHorizontal);
    configureSlider (wetSlider, juce::Slider::LinearHorizontal);
    crispSlider.setRange (0.0, 1.0, 0.001);
    sparkleSlider.setRange (0.0, 1.0, 0.001);
    biteSlider.setRange (0.0, 1.0, 0.001);
    glueSlider.setRange (0.0, 1.0, 0.001);
    wetSlider.setRange (0.0, 1.0, 0.001);

    crispSlider.setTextValueSuffix (" Crisp");
    sparkleSlider.setTextValueSuffix (" Sparkle");
    biteSlider.setTextValueSuffix (" Bite");
    glueSlider.setTextValueSuffix (" Glue");
    wetSlider.setTextValueSuffix (" Wet");

    addAndMakeVisible (crispSlider);
    addAndMakeVisible (sparkleSlider);
    addAndMakeVisible (biteSlider);
    addAndMakeVisible (glueSlider);
    addAndMakeVisible (wetSlider);

    crispAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, "crispAmount", crispSlider);
    sparkleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, "sparkle", sparkleSlider);
    biteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, "bite", biteSlider);
    glueAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, "glue", glueSlider);
    wetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, "wet", wetSlider);

    addAndMakeVisible (profileView);

    updateProfileLabel();
}

ReferenceToneMatcherAudioProcessorEditor::~ReferenceToneMatcherAudioProcessorEditor() = default;

void ReferenceToneMatcherAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (16, 20, 26));

    juce::Rectangle<int> header = getLocalBounds().removeFromTop (60);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (24.0f, juce::Font::bold));
    g.drawText ("ReferenceToneMatcher", header.reduced (20, 10), juce::Justification::centredLeft, false);
}

void ReferenceToneMatcherAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop (60);
    loadButton.setBounds (header.removeFromRight (200).reduced (20, 15));
    profileLabel.setBounds (header.reduced (200, 15));

    auto profileArea = bounds.removeFromTop (140).reduced (20, 10);
    profileView.setBounds (profileArea);

    auto sliderArea = bounds.removeFromTop (220).reduced (20, 10);
    const int sliderWidth = sliderArea.getWidth() / static_cast<int> (bandSliders.size());
    for (size_t i = 0; i < bandSliders.size(); ++i)
    {
        auto x = static_cast<int> (i) * sliderWidth;
        bandSliders[i].setBounds (sliderArea.getX() + x, sliderArea.getY(), sliderWidth - 4, sliderArea.getHeight());
    }

    auto bottomArea = bounds.reduced (20, 10);
    const int rowHeight = 40;
    crispSlider.setBounds (bottomArea.removeFromTop (rowHeight));
    sparkleSlider.setBounds (bottomArea.removeFromTop (rowHeight));
    biteSlider.setBounds (bottomArea.removeFromTop (rowHeight));
    glueSlider.setBounds (bottomArea.removeFromTop (rowHeight));
    wetSlider.setBounds (bottomArea.removeFromTop (rowHeight));
}

void ReferenceToneMatcherAudioProcessorEditor::configureSlider (juce::Slider& slider,
                                                                juce::Slider::SliderStyle style,
                                                                const juce::String& suffix)
{
    slider.setSliderStyle (style);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    slider.setColour (juce::Slider::trackColourId, juce::Colour::fromRGB (60, 180, 255));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setTextValueSuffix (suffix);
}

void ReferenceToneMatcherAudioProcessorEditor::updateProfileLabel()
{
    auto profile = processor.getCurrentProfile();
    if (profile.isValid)
        profileLabel.setText ("Profil geladen: " + profile.sourceName, juce::dontSendNotification);
    else
        profileLabel.setText ("Profil geladen: keines", juce::dontSendNotification);
}

