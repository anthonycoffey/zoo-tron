#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class ZooLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ZooLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
};

//==============================================================================
class EnvLED : public juce::Component
{
public:
    void setLevel (float l) { level = l; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float level = 0.0f;
};

//==============================================================================
// Live filter-response display: draws the analog magnitude curve for the
// current cutoff / Q / mode, with a playhead at the current cutoff. Updated
// ~30fps from the processor's published filter state, so it animates with the
// sweep as you play.
class FilterScope : public juce::Component
{
public:
    void setData (float cutoffHz, float q, int mode);
    void paint (juce::Graphics&) override;
private:
    float cutoff = 1000.0f, qv = 2.0f;
    int   mode = 1;
};

//==============================================================================
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
    std::vector<int> indices;
    juce::String switchName;
    int slot = 0;
    juce::ParameterAttachment attachment;
};

//==============================================================================
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (PresetManager&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setDisplayName (const juce::String& n) { name = n; repaint(); }
private:
    void showSaveDialog();
    PresetManager& presets;
    juce::String name;
    juce::Rectangle<int> leftArrow, rightArrow, screen;
    std::unique_ptr<juce::AlertWindow> saveDialog;
};

//==============================================================================
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

    FilterScope scope;
    juce::Slider gain, peak, contour, attack, release, drive, output, mix;
    std::unique_ptr<SA> gainAtt, peakAtt, contourAtt, attackAtt, releaseAtt, driveAtt, outputAtt, mixAtt;

    std::unique_ptr<PedalSwitch> rangeSw, modeSw, dirSw;
    PresetBar  presetBar;
    Footswitch footswitch;
    EnvLED     led;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZooTronAudioProcessorEditor)
};
