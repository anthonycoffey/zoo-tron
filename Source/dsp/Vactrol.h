#pragma once

#include <cmath>
#include <algorithm>

namespace zt
{
/*  Opto-isolator (vactrol) model — the heart of the Mu-Tron's "feel".

    A vactrol is an LED facing a light-dependent resistor (LDR). The cell
    responds asymmetrically: resistance drops *fast* when the LED brightens
    (note attack), but recovers *slowly* when it dims — and that recovery gets
    slower the darker the cell already is. That level-dependent release is what
    gives the Mu-Tron its vocal, rubbery "owww" tail instead of a sterile
    auto-wah snap.

    Implemented as a one-pole smoother whose time constant switches on
    direction and, on release, scales with the current light level. */

class Vactrol
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        setTimes (6.0, 55.0, 240.0);
        reset();
    }

    void reset() noexcept { y = 0.0f; }

    // attackMs: brightening. relBrightMs..relDarkMs: release when bright vs. dark.
    void setTimes (double attackMs, double relBrightMs, double relDarkMs) noexcept
    {
        attackCoef    = coefFor (attackMs);
        relBrightCoef = coefFor (relBrightMs);
        relDarkCoef   = coefFor (relDarkMs);
    }

    inline float process (float target) noexcept
    {
        float coef;

        if (target > y)
        {
            coef = attackCoef;                       // light increasing: snappy
        }
        else
        {
            // Darker cell (small y) recovers more slowly -> coef nearer 1.
            const float t = std::clamp (y, 0.0f, 1.0f);
            coef = relDarkCoef + (relBrightCoef - relDarkCoef) * t;
        }

        y = target + coef * (y - target);
        return y;
    }

    float current() const noexcept { return y; }

private:
    inline float coefFor (double ms) const noexcept
    {
        return (float) std::exp (-1.0 / (0.001 * std::max (0.01, ms) * fs));
    }

    double fs = 44100.0;
    float  attackCoef = 0.0f, relBrightCoef = 0.0f, relDarkCoef = 0.0f, y = 0.0f;
};

} // namespace zt
