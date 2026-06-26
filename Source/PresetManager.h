#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

/*  A factory preset = a named snapshot of the front-panel parameters. Loading a
    preset just sets the APVTS parameters (so the knobs follow via their
    attachments and the result stays fully tweakable). The "current preset" index
    lives in the APVTS state tree, so it survives save/restore, and a preset reads
    as "edited" (shown with a *) the moment any value drifts from the snapshot. */

struct ZooPreset
{
    juce::String name;
    int   range, mode, direction; // choice indices: range 0=Lo/1=Hi, mode 0=LP/1=BP/2=HP, dir 0=Up/1=Down
    float gain, peak, output, mix;
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s)
    {
        if (! apvts.state.hasProperty (indexProp))
            apvts.state.setProperty (indexProp, 0, nullptr); // boots on "Classic Funk"
    }

    int getNumPresets() const { return (int) presets.size(); }

    juce::String getName (int i) const
    {
        return juce::isPositiveAndBelow (i, getNumPresets()) ? presets[(size_t) i].name
                                                             : juce::String ("Manual");
    }

    const std::vector<ZooPreset>& all() const { return presets; }

    int currentIndex() const { return (int) apvts.state.getProperty (indexProp, -1); }

    bool isDirty() const
    {
        const int i = currentIndex();
        if (! juce::isPositiveAndBelow (i, getNumPresets())) return false;
        const auto& p = presets[(size_t) i];
        auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
        return (int) v ("range")     != p.range
            || (int) v ("mode")      != p.mode
            || (int) v ("direction") != p.direction
            || std::abs (v ("gain")   - p.gain)   > 0.02f
            || std::abs (v ("peak")   - p.peak)   > 0.02f
            || std::abs (v ("output") - p.output) > 0.05f
            || std::abs (v ("mix")    - p.mix)    > 0.05f;
    }

    juce::String displayName() const
    {
        const int i = currentIndex();
        if (! juce::isPositiveAndBelow (i, getNumPresets())) return "Manual";
        return presets[(size_t) i].name + (isDirty() ? " *" : "");
    }

    void load (int i)
    {
        if (! juce::isPositiveAndBelow (i, getNumPresets())) return;
        const auto& p = presets[(size_t) i];
        setP ("range",     (float) p.range);
        setP ("mode",      (float) p.mode);
        setP ("direction", (float) p.direction);
        setP ("gain",   p.gain);
        setP ("peak",   p.peak);
        setP ("output", p.output);
        setP ("mix",    p.mix);
        apvts.state.setProperty (indexProp, i, nullptr);
    }

    void next() { load ((juce::jmax (0, currentIndex()) + 1) % getNumPresets()); }
    void prev() { load ((juce::jmax (0, currentIndex()) - 1 + getNumPresets()) % getNumPresets()); }

private:
    void setP (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    juce::AudioProcessorValueTreeState& apvts;
    const juce::Identifier indexProp { "presetIndex" };

    //                name            range mode dir  gain  peak  out    mix
    const std::vector<ZooPreset> presets {
        { "Classic Funk",   1, 1, 0,  5.5f, 6.0f, 0.0f, 100.0f }, // the signature quack
        { "Bootsy",         0, 1, 0,  6.0f, 5.5f, 0.0f, 100.0f }, // deep bass funk
        { "Garcia",         1, 0, 0,  4.5f, 4.0f, 0.0f, 100.0f }, // smooth guitar lead
        { "Quack Attack",   1, 1, 0,  7.5f, 8.0f, 0.0f, 100.0f }, // aggressive, near self-osc
        { "Cosmic Slop",    0, 1, 1,  5.5f, 6.0f, 0.0f, 100.0f }, // down-sweep bass
        { "Higher Ground",  1, 1, 0,  6.5f, 7.0f, 0.0f, 100.0f }, // clav / keys funk
        { "Vocal Wah",      1, 0, 0,  5.0f, 5.0f, 0.0f, 100.0f }, // talky low-pass
        { "Sub Sweep",      0, 0, 0,  5.0f, 3.5f, 0.0f, 100.0f }, // round low filter
        { "Synth Sweep",    1, 2, 0,  5.5f, 6.0f, 0.0f, 100.0f }, // bright high-pass
        { "Down & Out",     1, 1, 1,  5.0f, 6.0f, 0.0f, 100.0f }, // reverse sweep
    };
};
