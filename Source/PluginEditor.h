#pragma once
#include "PluginProcessor.h"

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};

class SoundMasterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SoundMasterAudioProcessorEditor (SoundMasterAudioProcessor&);
    ~SoundMasterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void updateLabel();

    SoundMasterAudioProcessor& proc;
    KnobLookAndFeel lnf;
    juce::Slider knob;
    juce::Label valueLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundMasterAudioProcessorEditor)
};
