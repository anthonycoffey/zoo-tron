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
    const juce::Colour grid      { 0xff2a2550 };
    const juce::Colour grid0     { 0xff241f48 };
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
void FilterScope::setData (float c, float q, int m)
{
    targetCutoff = juce::jlimit (20.0f, 20000.0f, c);
    targetQ = q;
    mode = m;

    // Glide toward the target — cutoff in the log domain so the playhead moves
    // smoothly across the (log-frequency) display instead of stepping.
    const float lc = std::log (cutoff), lt = std::log (targetCutoff);
    cutoff = std::exp (lc + 0.35f * (lt - lc));
    qv += 0.35f * (targetQ - qv);

    trail[(size_t) trailIdx] = cutoff;
    trailIdx = (trailIdx + 1) % (int) trail.size();
    repaint();
}

void FilterScope::pushSpectrum (const float* block, double sampleRate)
{
    sr = sampleRate;
    std::copy (block, block + fftSize, fftData.begin());
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float norm = 2.0f / (float) fftSize;
    for (int i = 0; i < fftSize / 2; ++i)
    {
        const float mag   = fftData[(size_t) i] * norm;
        const float db    = juce::Decibels::gainToDecibels (mag + 1.0e-9f, -90.0f);
        const float level = juce::jlimit (0.0f, 1.0f, (db + 84.0f) / 84.0f);
        spectrum[(size_t) i] = juce::jmax (level, spectrum[(size_t) i] * 0.82f); // rise fast, fall slow
    }
}

void FilterScope::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colours::screenBg);
    g.fillRoundedRectangle (b, 8.0f);

    auto area = b.reduced (10.0f);
    const float L = area.getX(), R = area.getRight(), T = area.getY(), B = area.getBottom();
    const float W = area.getWidth(), H = area.getHeight();
    const float lmin = std::log10 (20.0f), lmax = std::log10 (20000.0f);

    auto fToX = [&] (float f) { return L + (std::log10 (f) - lmin) / (lmax - lmin) * W; };
    auto xToF = [&] (float xx) { return std::pow (10.0f, lmin + (xx - L) / W * (lmax - lmin)); };
    const float dbMax = 24.0f, dbMin = -30.0f;
    auto dbToY = [&] (float db) { return T + (dbMax - db) / (dbMax - dbMin) * H; };

    const float qc = juce::jmax (0.2f, qv);
    auto magAt = [&] (float w)
    {
        const float denom = std::sqrt ((1.0f - w * w) * (1.0f - w * w) + (w / qc) * (w / qc));
        const float m = (mode == 0) ? 1.0f / denom
                      : (mode == 1) ? (w / qc) / denom
                                    : (w * w) / denom;
        return 20.0f * std::log10 (juce::jmax (1.0e-4f, m));
    };

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (b.reduced (1.5f), 7.0f);
        g.reduceClipRegion (clip);

        // live signal spectrum, riding behind the grid + filter curve
        {
            juce::Path spec;
            spec.startNewSubPath (L, B);
            for (float x = L; x <= R; x += 1.0f)
            {
                const int bin = juce::jlimit (0, fftSize / 2 - 1, (int) std::round (xToF (x) * fftSize / sr));
                spec.lineTo (x, B - spectrum[(size_t) bin] * H);
            }
            spec.lineTo (R, B); spec.closeSubPath();
            juce::ColourGradient sg (juce::Colour (0xff6f68a0).withAlpha (0.55f), 0.0f, B,
                                     juce::Colour (0xff6f68a0).withAlpha (0.08f), 0.0f, T, false);
            g.setGradientFill (sg);
            g.fillPath (spec);
        }

        g.setColour (colours::grid);
        for (float f : { 100.0f, 1000.0f, 10000.0f })
            g.drawLine (fToX (f), T, fToX (f), B, 1.0f);
        g.setColour (colours::grid0);
        for (float db : { 12.0f, 0.0f, -12.0f, -24.0f })
            g.drawLine (L, dbToY (db), R, dbToY (db), 1.0f);

        g.setColour (colours::textMute);
        g.setFont (juce::FontOptions (9.0f));
        g.drawText ("100", (int) fToX (100.0f) - 16, (int) B - 13, 32, 12, juce::Justification::centred);
        g.drawText ("1k",  (int) fToX (1000.0f) - 16, (int) B - 13, 32, 12, juce::Justification::centred);
        g.drawText ("10k", (int) fToX (10000.0f) - 16, (int) B - 13, 32, 12, juce::Justification::centred);

        juce::Path curve;
        bool started = false;
        for (float x = L; x <= R; x += 1.0f) // 1px sampling -> silky line
        {
            const float y = juce::jlimit (T, B, dbToY (magAt (xToF (x) / cutoff)));
            if (! started) { curve.startNewSubPath (x, y); started = true; }
            else             curve.lineTo (x, y);
        }

        juce::Path fill = curve;
        fill.lineTo (R, B); fill.lineTo (L, B); fill.closeSubPath();
        juce::ColourGradient grad (colours::teal.withAlpha (0.30f), area.getCentreX(), T,
                                   colours::teal.withAlpha (0.02f), area.getCentreX(), B, false);
        g.setGradientFill (grad);
        g.fillPath (fill);

        g.setColour (colours::teal.withAlpha (0.16f)); // soft glow underlay
        g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::teal);
        g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // playhead afterglow — fading dots along its recent path
        const float trailY = juce::jlimit (T, B, dbToY (magAt (1.0f)));
        const int tn = (int) trail.size();
        for (int age = tn - 1; age >= 1; --age)
        {
            const int idx = ((trailIdx - 1 - age) % tn + tn) % tn;
            const float tc = trail[(size_t) idx];
            if (tc <= 1.0f) continue;
            const float frac = 1.0f - (float) age / (float) tn;
            const float tx = juce::jlimit (L, R, fToX (tc));
            g.setColour (colours::teal.withAlpha (0.30f * frac));
            const float rr = 1.0f + 3.0f * frac;
            g.fillEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre ({ tx, trailY }));
        }

        const float px = juce::jlimit (L, R, fToX (cutoff));
        juce::ColourGradient ph (colours::teal.withAlpha (0.0f), px - 14.0f, T,
                                 colours::teal.withAlpha (0.0f), px + 14.0f, T, false);
        ph.addColour (0.5, colours::teal.withAlpha (0.18f));
        g.setGradientFill (ph);
        g.fillRect (juce::Rectangle<float> (px - 14.0f, T, 28.0f, H));
        g.setColour (colours::teal.withAlpha (0.55f));
        g.drawLine (px, T, px, B, 1.2f);

        const float py = juce::jlimit (T, B, dbToY (magAt (1.0f)));
        g.setColour (colours::teal.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ px, py }));
        g.setColour (juce::Colour (0xffd6fff4));
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ px, py }));
    }

    g.setColour (colours::wellStroke);
    g.drawRoundedRectangle (b, 8.0f, 1.5f);
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
    const int wellH = 32 + (n - 1) * 8; // taller well for more positions
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

    g.setFont (juce::FontOptions (n >= 4 ? 8.0f : (n == 3 ? 10.0f : 11.0f), juce::Font::bold));
    const int labX = wellX + wellW + 8;
    for (int i = 0; i < n; ++i)
    {
        g.setColour (i == slot ? colours::teal : colours::textMute);
        g.drawText (labels[i], labX, slotY (i) - 7, 44, 14, juce::Justification::centredLeft);
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
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain));
    g.drawText (name.toUpperCase(), screen, juce::Justification::centred);

    auto drawArrow = [&] (juce::Rectangle<int> r, bool left)
    {
        const auto c = r.getCentre().toFloat();
        g.setColour (colours::arrowBg);
        g.fillEllipse (juce::Rectangle<float> (24.0f, 24.0f).withCentre (c));
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
    if (leftArrow.contains (pos))  { presets.prev(); return; }
    if (rightArrow.contains (pos)) { presets.next(); return; }
    if (! screen.contains (pos))   return;

    const int cur = presets.currentIndex();
    const auto userFiles = presets.userPresetFiles();

    juce::PopupMenu m;
    for (int i = 0; i < presets.getNumPresets(); ++i)
        m.addItem (i + 1, presets.getName (i), true, i == cur);

    if (userFiles.size() > 0)
    {
        m.addSeparator();
        for (int i = 0; i < userFiles.size(); ++i)
            m.addItem (1001 + i, userFiles[i].getFileNameWithoutExtension(), true, false);
    }
    m.addSeparator();
    m.addItem (9999, "Save current preset...");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [this, userFiles] (int r)
                     {
                         if (r <= 0) return;
                         if (r == 9999) { showSaveDialog(); return; }
                         if (r >= 1001 && r < 1001 + userFiles.size()) { presets.loadUserPreset (userFiles[r - 1001]); return; }
                         if (r >= 1 && r <= presets.getNumPresets()) presets.load (r - 1);
                     });
}

void PresetBar::showSaveDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name this preset:",
                                                      juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor ("name", "My Tone");
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int r)
    {
        if (r == 1 && saveDialog != nullptr)
        {
            const auto n = saveDialog->getTextEditorContents ("name");
            if (n.isNotEmpty()) presets.saveUserPreset (n);
        }
        saveDialog.reset();
    }), false);
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
    addAndMakeVisible (scope);

    styleKnob (gain); styleKnob (peak); styleKnob (contour);
    styleKnob (attack); styleKnob (release); styleKnob (drive);
    styleKnob (rate); styleKnob (width);
    styleKnob (output); styleKnob (mix);
    gainAtt    = std::make_unique<SA> (p.apvts, "gain",    gain);
    peakAtt    = std::make_unique<SA> (p.apvts, "peak",    peak);
    contourAtt = std::make_unique<SA> (p.apvts, "contour", contour);
    attackAtt  = std::make_unique<SA> (p.apvts, "attack",  attack);
    releaseAtt = std::make_unique<SA> (p.apvts, "release", release);
    driveAtt   = std::make_unique<SA> (p.apvts, "drive",   drive);
    outputAtt  = std::make_unique<SA> (p.apvts, "output",  output);
    mixAtt     = std::make_unique<SA> (p.apvts, "mix",     mix);
    rateAtt    = std::make_unique<SA> (p.apvts, "lfoRate", rate);
    widthAtt   = std::make_unique<SA> (p.apvts, "width",   width);

    rangeSw = std::make_unique<PedalSwitch> (*p.apvts.getParameter ("range"),
                  juce::StringArray { "HI", "LO" }, std::vector<int> { 1, 0 }, "RANGE");
    modeSw  = std::make_unique<PedalSwitch> (*p.apvts.getParameter ("mode"),
                  juce::StringArray { "LP", "BP", "HP" }, std::vector<int> { 0, 1, 2 }, "MODE");
    dirSw   = std::make_unique<PedalSwitch> (*p.apvts.getParameter ("direction"),
                  juce::StringArray { "UP", "DN" }, std::vector<int> { 0, 1 }, "SWEEP");
    addAndMakeVisible (*rangeSw);
    addAndMakeVisible (*modeSw);
    addAndMakeVisible (*dirSw);

    sourceSw = std::make_unique<PedalSwitch> (*p.apvts.getParameter ("source"),
                  juce::StringArray { "ENV", "LFO", "MAN" }, std::vector<int> { 0, 1, 2 }, "SOURCE");
    shapeSw  = std::make_unique<PedalSwitch> (*p.apvts.getParameter ("lfoShape"),
                  juce::StringArray { "SIN", "TRI", "SQR", "S&H" }, std::vector<int> { 0, 1, 2, 3 }, "SHAPE");
    syncSw   = std::make_unique<PedalSwitch> (*p.apvts.getParameter ("lfoSync"),
                  juce::StringArray { "FRE", "SYN" }, std::vector<int> { 0, 1 }, "SYNC");
    addAndMakeVisible (*sourceSw);
    addAndMakeVisible (*shapeSw);
    addAndMakeVisible (*syncSw);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (footswitch);
    addAndMakeVisible (led);

    setSize (600, 650);
    startTimerHz (60);
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
    g.fillRoundedRectangle (b.reduced (6.0f), 26.0f);
    g.setColour (juce::Colour (0xff4a4385));
    g.drawRoundedRectangle (b.reduced (6.0f), 26.0f, 1.5f);

    auto face = b.reduced (14.0f);
    g.setColour (colours::face);
    g.fillRoundedRectangle (face, 20.0f);
    juce::Path band;
    band.addRoundedRectangle (face.getX(), face.getY(), face.getWidth(), 64.0f,
                              20.0f, 20.0f, true, true, false, false);
    g.setColour (colours::faceTop);
    g.fillPath (band);

    auto screw = [&] (float sx, float sy)
    {
        g.setColour (colours::wellStroke);
        g.fillEllipse (sx - 6, sy - 6, 12, 12);
        g.setColour (colours::bodyEdge);
        g.fillEllipse (sx - 2.5f, sy - 2.5f, 5, 5);
    };
    screw (face.getX() + 22, face.getY() + 22);
    screw (face.getRight() - 22, face.getY() + 22);
    screw (face.getX() + 22, face.getBottom() - 22);
    screw (face.getRight() - 22, face.getBottom() - 22);

    g.setColour (colours::cream);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("ZOO-TRON III", 40, 32, 280, 28, juce::Justification::centredLeft);
    g.setColour (colours::textMute);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("envelope filter", 42, 58, 280, 14, juce::Justification::centredLeft);
    g.drawText ("ENV", 538, 52, 44, 12, juce::Justification::centred);

    g.setColour (colours::textLav);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    const char* row1[] = { "GAIN", "PEAK", "CONTOUR", "ATTACK", "RELEASE" };
    const char* row2[] = { "DRIVE", "RATE", "WIDTH", "OUTPUT", "MIX" };
    const int   kx[]   = { 84, 192, 300, 408, 516 };
    for (int i = 0; i < 5; ++i)
    {
        g.drawText (row1[i], kx[i] - 34, 374, 68, 14, juce::Justification::centred);
        g.drawText (row2[i], kx[i] - 34, 462, 68, 14, juce::Justification::centred);
    }
}

void ZooTronAudioProcessorEditor::resized()
{
    led.setBounds (551, 31, 18, 18);
    presetBar.setBounds (300, 24, 220, 38);
    scope.setBounds (26, 92, 548, 196);

    const int kx[] = { 84, 192, 300, 408, 516 };
    gain.setBounds    (kx[0] - 32, 306, 64, 64);
    peak.setBounds    (kx[1] - 32, 306, 64, 64);
    contour.setBounds (kx[2] - 32, 306, 64, 64);
    attack.setBounds  (kx[3] - 32, 306, 64, 64);
    release.setBounds (kx[4] - 32, 306, 64, 64);
    drive.setBounds   (kx[0] - 32, 394, 64, 64);
    rate.setBounds    (kx[1] - 32, 394, 64, 64);
    width.setBounds   (kx[2] - 32, 394, 64, 64);
    output.setBounds  (kx[3] - 32, 394, 64, 64);
    mix.setBounds     (kx[4] - 32, 394, 64, 64);

    rangeSw->setBounds (29, 490, 70, 84);
    modeSw->setBounds  (99, 490, 70, 84);
    dirSw->setBounds   (169, 490, 70, 84);
    footswitch.setBounds (250, 484, 100, 122);
    sourceSw->setBounds  (361, 490, 70, 84);
    shapeSw->setBounds   (431, 490, 70, 84);
    syncSw->setBounds    (501, 490, 70, 84);
}

void ZooTronAudioProcessorEditor::timerCallback()
{
    led.setLevel (audioProcessor.envelopeOut.load());
    presetBar.setDisplayName (audioProcessor.presetManager.displayName());

    const int mode = (int) audioProcessor.apvts.getRawParameterValue ("mode")->load();
    scope.setData (audioProcessor.cutoffOut.load(), audioProcessor.resonanceOut.load(), mode);

    if (audioProcessor.fftReady.exchange (false))
        scope.pushSpectrum (audioProcessor.fftBlock(), audioProcessor.getSampleRate());
}
