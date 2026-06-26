#include "PluginEditor.h"
#include <cmath>

namespace colours
{
    const juce::Colour bodyEdge  { 0xff15112e };
    const juce::Colour face      { 0xff251e4d };
    const juce::Colour faceTop   { 0xff2c2459 };
    const juce::Colour cream     { 0xffe9e1c9 };
    const juce::Colour creamRim  { 0xffb8ad8f };
    const juce::Colour pointer   { 0xff2a2348 };
    const juce::Colour teal      { 0xff7af0d0 };
    const juce::Colour textLav   { 0xffd6cfee };
    const juce::Colour textMute  { 0xff8e87bd };
    const juce::Colour wellDark  { 0xff15112e };
    const juce::Colour wellStroke{ 0xff3a3470 };
    const juce::Colour screenBg  { 0xff0c0a1c };
    const juce::Colour arrowBg   { 0xff2f2860 };
    const juce::Colour batMetal  { 0xffe2e3ea };
    const juce::Colour footRing  { 0xff13102a };
    const juce::Colour footMetal { 0xffc2c6d0 };
    const juce::Colour footCap   { 0xffaeb4c0 };
}

//==============================================================================
ZooLookAndFeel::ZooLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, colours::screenBg);
    setColour (juce::PopupMenu::textColourId, colours::textLav);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::arrowBg);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::teal);
}

void ZooLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const auto c = bounds.getCentre();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle  = startAngle + pos * (endAngle - startAngle);

    const float arcR = radius - 1.5f;
    juce::Path back, val;
    back.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    val.addCentredArc  (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle,    true);
    g.setColour (colours::wellDark);
    g.strokePath (back, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::teal);
    g.strokePath (val, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = radius - 6.0f;
    g.setColour (colours::bodyEdge);
    g.fillEllipse (juce::Rectangle<float> (2 * kr + 4, 2 * kr + 4).withCentre (c));
    g.setColour (colours::cream);
    g.fillEllipse (juce::Rectangle<float> (2 * kr, 2 * kr).withCentre (c));
    g.setColour (colours::creamRim);
    g.drawEllipse (juce::Rectangle<float> (2 * kr, 2 * kr).withCentre (c), 1.5f);

    const float pr = kr - 3.0f;
    const juce::Point<float> tip (c.x + pr * std::sin (angle), c.y - pr * std::cos (angle));
    g.setColour (colours::pointer);
    g.drawLine (c.x, c.y, tip.x, tip.y, 3.0f);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (tip));
}

//==============================================================================
void EnvLED::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float d = juce::jmin (b.getWidth(), b.getHeight());
    auto dot = juce::Rectangle<float> (d * 0.5f, d * 0.5f).withCentre (b.getCentre());

    g.setColour (colours::wellDark);
    g.fillEllipse (dot);

    const float l = juce::jlimit (0.0f, 1.0f, level);
    if (l > 0.001f)
    {
        g.setColour (colours::teal.withAlpha (0.35f * l));
        g.fillEllipse (dot.expanded (d * 0.28f * l));
        g.setColour (colours::teal.withAlpha (0.3f + 0.7f * l));
        g.fillEllipse (dot.reduced (d * 0.08f));
    }
    g.setColour (colours::wellStroke);
    g.drawEllipse (dot, 1.0f);
}

//==============================================================================
PedalSwitch::PedalSwitch (juce::RangedAudioParameter& p, juce::StringArray l,
                          std::vector<int> idx, juce::String name)
    : param (p), labels (std::move (l)), indices (std::move (idx)), switchName (std::move (name)),
      attachment (p, [this] (float v) { applyValue (v); })
{
    attachment.sendInitialUpdate();
}

void PedalSwitch::applyValue (float denorm)
{
    const int idx = (int) std::round (denorm);
    for (size_t i = 0; i < indices.size(); ++i)
        if (indices[i] == idx) { slot = (int) i; break; }
    repaint();
}

void PedalSwitch::mouseDown (const juce::MouseEvent&)
{
    slot = (slot + 1) % (int) indices.size();
    attachment.setValueAsCompleteGesture ((float) indices[(size_t) slot]);
    repaint();
}

void PedalSwitch::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    const int n = (int) indices.size();
    const int nameH = 18;
    auto content = b.withTrimmedBottom (nameH);

    const int wellW = 16;
    const int wellH = (n == 3 ? 46 : 40);
    const int wellX = content.getX() + 8;
    const int wellY = content.getCentreY() - wellH / 2;

    g.setColour (colours::wellDark);
    g.fillRoundedRectangle ((float) wellX, (float) wellY, (float) wellW, (float) wellH, 8.0f);
    g.setColour (colours::wellStroke);
    g.drawRoundedRectangle ((float) wellX, (float) wellY, (float) wellW, (float) wellH, 8.0f, 1.0f);

    const int pad = 9;
    auto slotY = [&] (int s) { return (n == 1) ? wellY + wellH / 2
                                               : wellY + pad + (int) std::round ((double) (wellH - 2 * pad) * s / (n - 1)); };

    const int batX = wellX + wellW / 2;
    const int by = slotY (slot);
    g.setColour (colours::wellStroke);
    g.drawLine ((float) batX, (float) content.getCentreY(), (float) batX, (float) by, 3.0f);
    g.setColour (colours::batMetal);
    g.fillEllipse ((float) (batX - 6), (float) (by - 6), 12.0f, 12.0f);

    g.setFont (juce::FontOptions (n == 3 ? 10.0f : 11.0f, juce::Font::bold));
    const int labX = wellX + wellW + 8;
    for (int i = 0; i < n; ++i)
    {
        g.setColour (i == slot ? colours::teal : colours::textMute);
        g.drawText (labels[i], labX, slotY (i) - 8, 42, 16, juce::Justification::centredLeft);
    }

    g.setColour (colours::textLav);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (switchName, b.removeFromBottom (nameH), juce::Justification::centred);
}

//==============================================================================
PresetBar::PresetBar (PresetManager& pm) : presets (pm) {}

void PresetBar::resized()
{
    auto b = getLocalBounds();
    leftArrow  = b.removeFromLeft (30);
    rightArrow = b.removeFromRight (30);
    screen     = b.reduced (4, 2);
}

void PresetBar::paint (juce::Graphics& g)
{
    g.setColour (colours::wellStroke);
    g.fillRoundedRectangle (screen.toFloat().expanded (2.0f), 8.0f);
    g.setColour (colours::screenBg);
    g.fillRoundedRectangle (screen.toFloat(), 6.0f);

    g.setColour (colours::teal);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 17.0f, juce::Font::plain));
    g.drawText (name.toUpperCase(), screen, juce::Justification::centred);

    auto drawArrow = [&] (juce::Rectangle<int> r, bool left)
    {
        const auto c = r.getCentre().toFloat();
        g.setColour (colours::arrowBg);
        g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (c));
        juce::Path p;
        if (left) p.addTriangle (c.x - 5, c.y, c.x + 5, c.y - 7, c.x + 5, c.y + 7);
        else      p.addTriangle (c.x + 5, c.y, c.x - 5, c.y - 7, c.x - 5, c.y + 7);
        g.setColour (colours::textLav);
        g.fillPath (p);
    };
    drawArrow (leftArrow, true);
    drawArrow (rightArrow, false);
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();
    if (leftArrow.contains (pos))       presets.prev();
    else if (rightArrow.contains (pos)) presets.next();
    else if (screen.contains (pos))
    {
        juce::PopupMenu m;
        for (int i = 0; i < presets.getNumPresets(); ++i)
            m.addItem (i + 1, presets.getName (i), true, i == presets.currentIndex());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [this] (int r) { if (r > 0) presets.load (r - 1); });
    }
}

//==============================================================================
Footswitch::Footswitch (juce::RangedAudioParameter& p)
    : param (p), attachment (p, [this] (float v) { applyValue (v); })
{
    attachment.sendInitialUpdate();
}

void Footswitch::applyValue (float v) { bypassed = v >= 0.5f; repaint(); }

void Footswitch::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (bypassed ? 0.0f : 1.0f);
}

void Footswitch::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    const bool engaged = ! bypassed;

    const juce::Point<float> ledC ((float) b.getCentreX(), (float) b.getY() + 14.0f);
    if (engaged)
    {
        g.setColour (colours::teal.withAlpha (0.22f));
        g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (ledC));
        g.setColour (colours::teal);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (ledC));
        g.setColour (juce::Colour (0xffd6fff4));
        g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre (ledC.translated (-1.5f, -1.5f)));
    }
    else
    {
        g.setColour (colours::wellDark);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (ledC));
        g.setColour (colours::wellStroke);
        g.drawEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (ledC), 1.0f);
    }

    // Simple stomp: outer ring + metal disc + cap. Sits well below the LED.
    const juce::Point<float> stompC ((float) b.getCentreX(), (float) b.getY() + 74.0f);
    const float r = 24.0f;
    g.setColour (colours::footRing);
    g.fillEllipse (juce::Rectangle<float> (2 * r, 2 * r).withCentre (stompC));
    g.setColour (colours::footMetal);
    g.fillEllipse (juce::Rectangle<float> (2 * r - 7, 2 * r - 7).withCentre (stompC));
    g.setColour (colours::footCap);
    g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (stompC));
    g.setColour (colours::footRing);
    g.drawEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (stompC), 1.5f);

    g.setColour (colours::textMute);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("ENGAGE", b.removeFromBottom (20), juce::Justification::centred);
}

//==============================================================================
ZooTronAudioProcessorEditor::ZooTronAudioProcessorEditor (ZooTronAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), audioProcessor (p),
      presetBar (p.presetManager),
      footswitch (*p.apvts.getParameter ("bypass"))
{
    setLookAndFeel (&lnf);

    styleKnob (gain); styleKnob (peak); styleKnob (output); styleKnob (mix);
    gainAtt   = std::make_unique<SA> (p.apvts, "gain",   gain);
    peakAtt   = std::make_unique<SA> (p.apvts, "peak",   peak);
    outputAtt = std::make_unique<SA> (p.apvts, "output", output);
    mixAtt    = std::make_unique<SA> (p.apvts, "mix",    mix);

    rangeSw = std::make_unique<PedalSwitch> (*p.apvts.getParameter ("range"),
                  juce::StringArray { "HI", "LO" }, std::vector<int> { 1, 0 }, "RANGE");
    modeSw  = std::make_unique<PedalSwitch> (*p.apvts.getParameter ("mode"),
                  juce::StringArray { "LP", "BP", "HP" }, std::vector<int> { 0, 1, 2 }, "MODE");
    dirSw   = std::make_unique<PedalSwitch> (*p.apvts.getParameter ("direction"),
                  juce::StringArray { "UP", "DN" }, std::vector<int> { 0, 1 }, "SWEEP");
    addAndMakeVisible (*rangeSw);
    addAndMakeVisible (*modeSw);
    addAndMakeVisible (*dirSw);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (footswitch);
    addAndMakeVisible (led);

    setSize (340, 600);
    startTimerHz (30);
}

ZooTronAudioProcessorEditor::~ZooTronAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ZooTronAudioProcessorEditor::styleKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                           juce::MathConstants<float>::pi * 2.8f, true);
    addAndMakeVisible (s);
}

void ZooTronAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    g.setColour (colours::bodyEdge);
    g.fillRoundedRectangle (b.reduced (6.0f), 24.0f);
    g.setColour (juce::Colour (0xff4a4385));
    g.drawRoundedRectangle (b.reduced (6.0f), 24.0f, 1.5f);

    auto face = b.reduced (14.0f);
    g.setColour (colours::face);
    g.fillRoundedRectangle (face, 18.0f);

    juce::Path band;
    band.addRoundedRectangle (face.getX(), face.getY(), face.getWidth(), 70.0f,
                              18.0f, 18.0f, true, true, false, false);
    g.setColour (colours::faceTop);
    g.fillPath (band);

    auto screw = [&] (float sx, float sy)
    {
        g.setColour (colours::wellStroke);
        g.fillEllipse (sx - 6, sy - 6, 12, 12);
        g.setColour (colours::bodyEdge);
        g.fillEllipse (sx - 2.5f, sy - 2.5f, 5, 5);
    };
    screw (face.getX() + 20, face.getY() + 22);
    screw (face.getRight() - 20, face.getY() + 22);
    screw (face.getX() + 20, face.getBottom() - 22);
    screw (face.getRight() - 20, face.getBottom() - 22);

    g.setColour (colours::cream);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("ZOO-TRON III", juce::Rectangle<int> (0, 38, 330, 28), juce::Justification::centred);
    g.setColour (colours::textMute);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("envelope filter", juce::Rectangle<int> (0, 64, 330, 14), juce::Justification::centred);
    g.drawText ("ENV", juce::Rectangle<int> (286, 56, 32, 12), juce::Justification::centred);

    g.setColour (colours::textLav);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("GAIN", 44, 230, 84, 16, juce::Justification::centred);
    g.drawText ("PEAK", 212, 230, 84, 16, juce::Justification::centred);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("OUTPUT", 56, 410, 60, 16, juce::Justification::centred);
    g.drawText ("MIX", 224, 410, 60, 16, juce::Justification::centred);
}

void ZooTronAudioProcessorEditor::resized()
{
    led.setBounds (293, 35, 20, 20);
    presetBar.setBounds (28, 84, 284, 40);

    gain.setBounds (44, 140, 84, 84);
    peak.setBounds (212, 140, 84, 84);

    rangeSw->setBounds (40, 250, 80, 82);
    modeSw->setBounds (130, 250, 80, 82);
    dirSw->setBounds (220, 250, 80, 82);

    output.setBounds (56, 348, 60, 60);
    mix.setBounds (224, 348, 60, 60);

    footswitch.setBounds (110, 424, 120, 140);
}

void ZooTronAudioProcessorEditor::timerCallback()
{
    led.setLevel (audioProcessor.envelopeOut.load());
    presetBar.setDisplayName (audioProcessor.presetManager.displayName());
}
