#pragma once

#include "StateVariableFilter.h"
#include "EnvelopeFollower.h"
#include "Vactrol.h"
#include "Lfo.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace zt
{
enum class Mode      { LowPass = 0, BandPass, HighPass };
enum class Range     { Low = 0, High };
enum class Direction { Up = 0, Down };
enum class Source    { Envelope = 0, Lfo, Manual };

// --- Antiderivative anti-aliased (ADAA) tanh waveshaper ----------------------
inline float logCoshStable (float x) noexcept
{
    const float a = std::fabs (x);
    return a + std::log1p (std::exp (-2.0f * a)) - 0.6931471805599453f; // - ln 2
}

inline float adaaTanh (float x, float& x1) noexcept
{
    const float dx = x - x1;
    float y;
    if (std::fabs (dx) < 1.0e-4f) y = std::tanh (0.5f * (x + x1));
    else                          y = (logCoshStable (x) - logCoshStable (x1)) / dx;
    x1 = x;
    return y;
}

/*  The full Mu-Tron III control chain:

        input -> [detector HPF] -> rectify/envelope -> [Gain]
              -> vactrol[Attack/Release] ─┐
                          LFO ────────────┤ Source -> cutoff law (+stereo spread)
                          Manual ─────────┘
              -> nonlinear SVF -> [Drive] -> out

    The sweep "driver" (0..1) is whichever Source is selected. Width detunes the
    per-channel cutoff so the filter image widens / moves across the stereo field. */

class MuTronEngine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        env.prepare (sampleRate);
        vactrol.prepare (sampleRate);
        lfo.prepare (sampleRate);

        detHP.reset();
        detHPCoeffs = computeSVFCoeffs (90.0, 0.707, fs);

        states.assign ((size_t) std::max (1, numChannels), SVFState{});
        updateRange();
        updateVactrol();
        reset();
    }

    void reset()
    {
        env.reset();
        vactrol.reset();
        lfo.reset();
        detHP.reset();
        for (auto& s : states) s.reset();
        driveX1[0] = driveX1[1] = 0.0f;
    }

    // ---- per-block parameter setters ----------------------------------------
    void setSensitivity (float gainKnob0to10) noexcept
    {
        const float t = std::clamp (gainKnob0to10 / 10.0f, 0.0f, 1.0f);
        sensitivity = 0.25f + std::pow (t, 1.4f) * 9.0f;
    }

    void setPeak (float peakKnob0to10) noexcept
    {
        const float t = std::clamp (peakKnob0to10 / 10.0f, 0.0f, 1.0f);
        q = 0.7f * std::pow (18.0f / 0.7f, t);
    }

    void setAttack (float ms) noexcept  { attackMs  = ms; updateVactrol(); }
    void setRelease (float ms) noexcept { releaseMs = ms; updateVactrol(); }
    void setDrive (float drive0to10) noexcept { drive = std::clamp (drive0to10 / 10.0f, 0.0f, 1.0f); }
    void setContour (float hz) noexcept { contourHz = hz; detHPCoeffs = computeSVFCoeffs (contourHz, 0.707, fs); }

    void setSource (Source s) noexcept   { source = s; }
    void setLfoRate (float hz) noexcept  { lfo.setRate (hz); }
    void setLfoShape (int s) noexcept    { lfo.setShape (s); }
    void setManual (float pos0to1) noexcept { manualPos = std::clamp (pos0to1, 0.0f, 1.0f); }
    void setWidth (float w0to1) noexcept { spread = std::clamp (w0to1, 0.0f, 1.0f) * 0.25f; }

    void setRange (Range r) noexcept     { range = r; updateRange(); }
    void setMode (Mode m) noexcept       { mode = m; }
    void setDirection (Direction d) noexcept { direction = d; }

    float lastEnvelope() const noexcept { return lastDriver; } // drives the LED + scope motion
    float lastCutoff()   const noexcept { return lastFc; }
    float lastQ()        const noexcept { return q; }

    // ---- audio --------------------------------------------------------------
    void process (float* const* data, int numCh, int numSamples) noexcept
    {
        const int chans = std::min (numCh, (int) states.size());
        const float pre    = 1.0f + drive * 6.0f;
        const float makeup = 1.0f / (1.0f + drive * 1.2f);
        const bool  perCh  = (chans == 2 && spread > 1.0e-4f);

        for (int n = 0; n < numSamples; ++n)
        {
            // Envelope detection always runs (so the LED tracks in every mode).
            float mono = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                mono += data[ch][n];
            mono /= (float) numCh;
            const float detector = detHP.process (detHPCoeffs, mono).hp;
            const float v = vactrol.process (std::clamp (env.process (detector) * sensitivity, 0.0f, 1.0f));

            const float lfoVal = lfo.process();
            const float driver = (source == Source::Envelope) ? v
                               : (source == Source::Lfo)      ? lfoVal
                                                              : manualPos;
            lastDriver = driver;

            const float sweep  = (direction == Direction::Up) ? driver : (1.0f - driver);
            const float fcBase = fcMin * std::pow (2.0f, octaves * sweep);
            lastFc = fcBase;

            SVFCoeffs cShared;
            if (! perCh)
                cShared = computeSVFCoeffs (fcBase, q, fs);

            for (int ch = 0; ch < chans; ++ch)
            {
                const SVFCoeffs c = perCh
                    ? computeSVFCoeffs (fcBase * (ch == 0 ? (1.0f + spread) : (1.0f - spread)), q, fs)
                    : cShared;

                const SVFOutputs o = states[(size_t) ch].processNL (c, data[ch][n], satHeadroom);
                float wet = (mode == Mode::LowPass)  ? o.lp
                          : (mode == Mode::BandPass) ? o.bp
                                                     : o.hp;
                wet = adaaTanh (wet * pre, driveX1[ch]) * makeup;
                data[ch][n] = wet;
            }
        }
    }

private:
    void updateRange() noexcept
    {
        if (range == Range::Low) { fcMin = 80.0f;  octaves = 4.00f; }
        else                     { fcMin = 180.0f; octaves = 4.25f; }
    }

    void updateVactrol() noexcept
    {
        vactrol.setTimes (attackMs, releaseMs * 0.5, releaseMs * 2.0);
    }

    double fs = 44100.0;

    EnvelopeFollower      env;
    Vactrol               vactrol;
    Lfo                   lfo;
    SVFState              detHP;
    SVFCoeffs             detHPCoeffs;
    std::vector<SVFState> states;
    float                 driveX1[2] = { 0.0f, 0.0f };

    float     sensitivity = 4.0f;
    float     q           = 2.0f;
    float     attackMs    = 8.0f;
    float     releaseMs   = 180.0f;
    float     drive       = 0.2f;
    float     contourHz   = 90.0f;
    float     manualPos   = 0.5f;
    float     spread      = 0.0f;
    float     lastDriver  = 0.0f;
    float     lastFc      = 1000.0f;
    float     fcMin       = 180.0f;
    float     octaves     = 4.25f;
    const float satHeadroom = 3.0f;

    Source    source      = Source::Envelope;
    Range     range       = Range::High;
    Mode      mode        = Mode::BandPass;
    Direction direction   = Direction::Up;
};

} // namespace zt
