#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
// Cream chicken-head knob with a dark pointer and a thin teal value arc.
class ZooLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ZooLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

//==============================================================================
// The optical "lamp" — pulses with the vactrol light level.
class EnvLED : public juce::Component
{
public:
    void setLevel (float l) { level = l; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float level = 0.0f;
};

//==============================================================================
// A 2- or 3-position bat switch bound to a choice parameter.
class PedalSwitch : public juce::Component
{
public:
    PedalSwitch (juce::RangedAudioParameter& p, juce::StringArray labelsTopToBottom,
                 std::vector<int> indicesTopToBottom, juce::String name);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void applyValue (float denorm);
    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    std::vector<int> indices;   // parameter index for each slot, top -> bottom
    juce::String switchName;
    int slot = 0;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// Digital preset readout: ◀  NAME  ▶, click the name for the full list.
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (PresetManager&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setDisplayName (const juce::String& n) { name = n; repaint(); }
private:
    PresetManager& presets;
    juce::String name;
    juce::Rectangle<int> leftArrow, rightArrow, screen;
};

//==============================================================================
// Stomp footswitch with a status LED above it, bound to the bypass parameter.
class Footswitch : public juce::Component
{
public:
    explicit Footswitch (juce::RangedAudioParameter& bypassParam);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void applyValue (float v);
    juce::RangedAudioParameter& param;
    bool bypassed = false;
    juce::ParameterAttachment attachment;
};

//==============================================================================
class ZooTronAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit ZooTronAudioProcessorEditor (ZooTronAudioProcessor&);
    ~ZooTronAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void styleKnob (juce::Slider&);

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    ZooTronAudioProcessor& audioProcessor;
    ZooLookAndFeel lnf;

    juce::Slider gain, peak, output, mix;
    std::unique_ptr<SA> gainAtt, peakAtt, outputAtt, mixAtt;

    std::unique_ptr<PedalSwitch> rangeSw, modeSw, dirSw;
    PresetBar  presetBar;
    Footswitch footswitch;
    EnvLED     led;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZooTronAudioProcessorEditor)
};
