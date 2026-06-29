#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

/*  Presets = named parameter snapshots. Factory presets live in the array below;
    user presets are saved as .xml next to the plugin's app-data folder and merged
    into the same list. The "current" selection lives in the APVTS state tree
    (factory index, or -1 + a name for a user preset) so it survives save/restore,
    and a factory preset shows "*" the moment a value drifts from its snapshot. */

struct ZooPreset
{
    juce::String name;
    int   range, mode, direction; // range 0=Lo/1=Hi, mode 0=LP/1=BP/2=HP, dir 0=Up/1=Down
    float gain, peak, attack, release, drive, contour, output, mix;
};

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s)
    {
        userDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                      .getChildFile ("Application Support")
                      .getChildFile ("Zoo-Tron III")
                      .getChildFile ("Presets");
        userDir.createDirectory();

        if (! apvts.state.hasProperty (indexProp))
            apvts.state.setProperty (indexProp, 0, nullptr); // boots on "Classic Funk"
    }

    int getNumPresets() const { return (int) presets.size(); }

    juce::String getName (int i) const
    {
        return juce::isPositiveAndBelow (i, getNumPresets()) ? presets[(size_t) i].name : juce::String();
    }

    juce::Array<juce::File> userPresetFiles() const
    {
        auto files = userDir.findChildFiles (juce::File::findFiles, false, "*.xml");
        files.sort();
        return files;
    }

    int currentIndex() const { return (int) apvts.state.getProperty (indexProp, -1); }

    bool isDirty() const
    {
        const int i = currentIndex();
        if (! juce::isPositiveAndBelow (i, getNumPresets())) return false; // user / manual: not tracked
        const auto& p = presets[(size_t) i];
        auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
        return (int) v ("range")     != p.range
            || (int) v ("mode")      != p.mode
            || (int) v ("direction") != p.direction
            || std::abs (v ("gain")    - p.gain)    > 0.02f
            || std::abs (v ("peak")    - p.peak)    > 0.02f
            || std::abs (v ("attack")  - p.attack)  > 0.2f
            || std::abs (v ("release") - p.release) > 1.0f
            || std::abs (v ("drive")   - p.drive)   > 0.02f
            || std::abs (v ("contour") - p.contour) > 1.0f
            || std::abs (v ("output")  - p.output)  > 0.05f
            || std::abs (v ("mix")     - p.mix)     > 0.05f;
    }

    juce::String displayName() const
    {
        const int i = currentIndex();
        if (juce::isPositiveAndBelow (i, getNumPresets()))
            return presets[(size_t) i].name + (isDirty() ? " *" : "");
        const juce::String user = apvts.state.getProperty (nameProp, "").toString();
        return user.isNotEmpty() ? user : "Manual";
    }

    void load (int i)
    {
        if (! juce::isPositiveAndBelow (i, getNumPresets())) return;
        const auto& p = presets[(size_t) i];
        setP ("range",     (float) p.range);
        setP ("mode",      (float) p.mode);
        setP ("direction", (float) p.direction);
        setP ("gain",    p.gain);
        setP ("peak",    p.peak);
        setP ("attack",  p.attack);
        setP ("release", p.release);
        setP ("drive",   p.drive);
        setP ("contour", p.contour);
        setP ("output",  p.output);
        setP ("mix",     p.mix);
        apvts.state.setProperty (indexProp, i, nullptr);
        apvts.state.setProperty (nameProp, "", nullptr);
    }

    void loadUserPreset (const juce::File& file)
    {
        if (auto xml = juce::XmlDocument::parse (file))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            if (tree.isValid())
            {
                apvts.replaceState (tree);
                apvts.state.setProperty (indexProp, -1, nullptr);
                apvts.state.setProperty (nameProp, file.getFileNameWithoutExtension(), nullptr);
            }
        }
    }

    void saveUserPreset (const juce::String& rawName)
    {
        const auto name = juce::File::createLegalFileName (rawName.trim());
        if (name.isEmpty()) return;

        auto state = apvts.copyState();
        state.setProperty (indexProp, -1, nullptr);
        state.setProperty (nameProp, name, nullptr);
        if (auto xml = state.createXml())
            xml->writeTo (userDir.getChildFile (name + ".xml"));

        apvts.state.setProperty (indexProp, -1, nullptr);
        apvts.state.setProperty (nameProp, name, nullptr);
    }

    // Steps through factory presets only (the ◀ ▶ arrows).
    void next() { load ((juce::jmax (0, currentIndex()) + 1) % getNumPresets()); }
    void prev() { load ((juce::jmax (0, currentIndex()) - 1 + getNumPresets()) % getNumPresets()); }

private:
    void setP (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::File userDir;
    const juce::Identifier indexProp { "presetIndex" };
    const juce::Identifier nameProp  { "presetName" };

    //                name           rng md dir  gain  peak  atk    rel    drv   ctr    out    mix
    const std::vector<ZooPreset> presets {
        { "Classic Funk",  1, 1, 0,  5.5f, 6.0f,  8.0f, 180.0f, 2.0f,  90.0f, 0.0f, 100.0f },
        { "Bootsy",        0, 1, 0,  6.0f, 5.5f, 12.0f, 260.0f, 3.0f,  70.0f, 0.0f, 100.0f },
        { "Garcia",        1, 0, 0,  4.5f, 4.0f,  6.0f, 150.0f, 1.5f,  90.0f, 0.0f, 100.0f },
        { "Quack Attack",  1, 1, 0,  7.5f, 8.0f,  3.0f,  90.0f, 4.0f, 120.0f, 0.0f, 100.0f },
        { "Cosmic Slop",   0, 1, 1,  5.5f, 6.0f, 14.0f, 300.0f, 3.0f,  70.0f, 0.0f, 100.0f },
        { "Higher Ground", 1, 1, 0,  6.5f, 7.0f,  5.0f, 120.0f, 3.5f, 110.0f, 0.0f, 100.0f },
        { "Vocal Wah",     1, 0, 0,  5.0f, 5.0f, 10.0f, 200.0f, 2.0f,  90.0f, 0.0f, 100.0f },
        { "Sub Sweep",     0, 0, 0,  5.0f, 3.5f, 12.0f, 240.0f, 1.5f,  60.0f, 0.0f, 100.0f },
        { "Synth Sweep",   1, 2, 0,  5.5f, 6.0f,  6.0f, 160.0f, 3.0f, 100.0f, 0.0f, 100.0f },
        { "Down & Out",    1, 1, 1,  5.0f, 6.0f,  9.0f, 220.0f, 2.5f,  90.0f, 0.0f, 100.0f },
        { "Mu-Bass",       0, 0, 0,  5.5f, 4.0f, 10.0f, 220.0f, 2.5f,  70.0f, 0.0f, 100.0f },
        { "Talk Box",      1, 1, 0,  6.0f, 8.5f,  4.0f, 130.0f, 3.0f, 100.0f, 0.0f, 100.0f },
        { "Filth",         1, 1, 0,  7.0f, 7.0f,  3.0f, 100.0f, 6.0f, 110.0f, 0.0f, 100.0f },
        { "Slow Motion",   1, 0, 0,  5.0f, 5.0f, 30.0f, 400.0f, 1.5f,  80.0f, 0.0f, 100.0f },
        { "Spit",          1, 2, 0,  6.5f, 7.0f,  2.0f,  80.0f, 3.0f, 120.0f, 0.0f, 100.0f },
    };
};
