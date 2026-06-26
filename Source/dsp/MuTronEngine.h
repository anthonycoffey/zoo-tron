#pragma once

#include "StateVariableFilter.h"
#include "EnvelopeFollower.h"
#include "Vactrol.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace zt
{
enum class Mode      { LowPass = 0, BandPass, HighPass };
enum class Range     { Low = 0, High };
enum class Direction { Up = 0, Down };

/*  The full Mu-Tron III control chain:

        input -> rectifier/envelope -> [Gain] -> vactrol -> cutoff law -> SVF

    The envelope is detected from the channel sum (the real pedal is mono, so a
    single control voltage sweeps both channels coherently). Cutoff coefficients
    are computed once per sample and shared across channels. Processing is done
    in place on whatever block it's handed — at the host rate, or oversampled. */

class MuTronEngine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        env.prepare (sampleRate);
        vactrol.prepare (sampleRate);

        states.assign ((size_t) std::max (1, numChannels), SVFState{});
        updateRange();
        reset();
    }

    void reset()
    {
        env.reset();
        vactrol.reset();
        for (auto& s : states)
            s.reset();
    }

    // ---- per-block parameter setters ----------------------------------------
    void setSensitivity (float gainKnob0to10) noexcept
    {
        const float t = std::clamp (gainKnob0to10 / 10.0f, 0.0f, 1.0f);
        sensitivity = 0.25f + std::pow (t, 1.4f) * 9.0f;   // ~0.25 .. ~9.25
    }

    void setPeak (float peakKnob0to10) noexcept
    {
        const float t = std::clamp (peakKnob0to10 / 10.0f, 0.0f, 1.0f);
        q = 0.7f * std::pow (18.0f / 0.7f, t);             // Q ~0.7 .. ~18
    }

    void setRange (Range r) noexcept     { range = r; updateRange(); }
    void setMode (Mode m) noexcept       { mode = m; }
    void setDirection (Direction d) noexcept { direction = d; }

    float lastEnvelope() const noexcept { return vactrol.current(); }

    // ---- audio --------------------------------------------------------------
    void process (float* const* data, int numCh, int numSamples) noexcept
    {
        const int chans = std::min (numCh, (int) states.size());

        for (int n = 0; n < numSamples; ++n)
        {
            // Mono-sum detector so L/R sweep together.
            float mono = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                mono += data[ch][n];
            mono /= (float) numCh;

            const float e     = env.process (mono);
            const float cv    = std::clamp (e * sensitivity, 0.0f, 1.0f);
            const float v     = vactrol.process (cv);
            const float sweep = (direction == Direction::Up) ? v : (1.0f - v);
            const float fc    = fcMin * std::pow (2.0f, octaves * sweep);

            const SVFCoeffs c = computeSVFCoeffs (fc, q, fs);

            for (int ch = 0; ch < chans; ++ch)
            {
                const SVFOutputs o = states[(size_t) ch].process (c, data[ch][n]);
                float wet = (mode == Mode::LowPass)  ? o.lp
                          : (mode == Mode::BandPass) ? o.bp
                                                     : o.hp;
                wet = std::tanh (wet * 1.1f) * 0.95f;   // op-amp warmth, tames self-osc
                data[ch][n] = wet;
            }
        }
    }

private:
    void updateRange() noexcept
    {
        if (range == Range::Low) { fcMin = 80.0f;  octaves = 4.00f; }  //  ~80 Hz .. ~1.3 kHz
        else                     { fcMin = 180.0f; octaves = 4.25f; }  // ~180 Hz .. ~3.4 kHz
    }

    double fs = 44100.0;

    EnvelopeFollower      env;
    Vactrol               vactrol;
    std::vector<SVFState> states;

    float     sensitivity = 4.0f;
    float     q           = 2.0f;
    float     fcMin       = 180.0f;
    float     octaves     = 4.25f;
    Range     range       = Range::High;
    Mode      mode        = Mode::BandPass;
    Direction direction   = Direction::Up;
};

} // namespace zt
