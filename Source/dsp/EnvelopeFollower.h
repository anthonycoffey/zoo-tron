#pragma once

#include <cmath>
#include <algorithm>

namespace zt
{
/*  Full-wave rectifier + asymmetric one-pole smoother. This is the first half
    of the Mu-Tron's control path (the rectifier + envelope cap). The slower,
    more characterful lag is added afterwards by the Vactrol model, so the
    times here are deliberately quick — just enough to give a clean envelope. */

class EnvelopeFollower
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        setTimes (3.0, 35.0);
        reset();
    }

    void reset() noexcept { env = 0.0f; }

    void setTimes (double attackMs, double releaseMs) noexcept
    {
        aCoef = coefFor (attackMs);
        rCoef = coefFor (releaseMs);
    }

    inline float process (float x) noexcept
    {
        const float rect = std::fabs (x);
        const float coef = (rect > env) ? aCoef : rCoef;
        env = rect + coef * (env - rect);
        return env;
    }

    float current() const noexcept { return env; }

private:
    inline float coefFor (double ms) const noexcept
    {
        return (float) std::exp (-1.0 / (0.001 * std::max (0.01, ms) * fs));
    }

    double fs = 44100.0;
    float  aCoef = 0.0f, rCoef = 0.0f, env = 0.0f;
};

} // namespace zt
