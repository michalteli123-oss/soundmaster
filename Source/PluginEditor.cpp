#include "PluginEditor.h"

void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                        juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (10.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float trackR = radius - 6.0f;

    // tło toru
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff2a2f3a));
    g.strokePath (track, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // aktywny łuk
    if (sliderPos > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, rotaryStartAngle, angle, true);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff00e5ff), centre.x - trackR, centre.y,
                                                 juce::Colour (0xffff2d95), centre.x + trackR, centre.y, false));
        g.strokePath (val, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // korpus pokrętła
    const float bodyR = radius - 28.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff434a59), centre.x, centre.y - bodyR,
                                             juce::Colour (0xff12151b), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour (0xff0b0d11));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 2.5f);

    // wskaźnik
    juce::Path pointer;
    pointer.addRoundedRectangle (-3.0f, -bodyR + 8.0f, 6.0f, bodyR * 0.40f, 3.0f);
    g.setColour (juce::Colours::white);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

SoundMasterAudioProcessorEditor::SoundMasterAudioProcessorEditor (SoundMasterAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), attachment (p.apvts, "soundmaster", knob)
{
    setSize (420, 490);

    knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                              juce::MathConstants<float>::pi * 2.75f, true);
    knob.setLookAndFeel (&lnf);
    knob.setDoubleClickReturnValue (true, 50.0);
    knob.onValueChange = [this] { updateLabel(); };
    addAndMakeVisible (knob);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (juce::Font (juce::FontOptions (38.0f, juce::Font::bold)));
    valueLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (valueLabel);

    updateLabel();
}

SoundMasterAudioProcessorEditor::~SoundMasterAudioProcessorEditor()
{
    knob.setLookAndFeel (nullptr);
}

void SoundMasterAudioProcessorEditor::updateLabel()
{
    valueLabel.setText (juce::String (juce::roundToInt (knob.getValue())) + " %", juce::dontSendNotification);
}

void SoundMasterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1f27), 0.0f, 0.0f,
                                             juce::Colour (0xff090b0f), 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText ("SOUNDMASTER", juce::Rectangle<int> (0, 22, getWidth(), 36), juce::Justification::centred, false);

    g.setColour (juce::Colour (0xff00e5ff));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::plain)));
    g.drawText ("VOCAL CHAIN", juce::Rectangle<int> (0, 58, getWidth(), 18), juce::Justification::centred, false);

    g.setColour (juce::Colour (0xff7d8594));
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::plain)));
    g.drawText ("EQ  |  DE-ESSER  |  COMPRESSOR  |  SATURATION  |  REVERB",
                juce::Rectangle<int> (0, 452, getWidth(), 18), juce::Justification::centred, false);
}

void SoundMasterAudioProcessorEditor::resized()
{
    knob.setBounds (60, 90, 300, 300);
    valueLabel.setBounds (110, 396, 200, 48);
}
