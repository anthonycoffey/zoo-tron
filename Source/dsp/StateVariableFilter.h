#pragma once

#include <cmath>
#include <algorithm>

namespace zt
{
inline constexpr double kPi = 3.14159265358979323846;

/*  Topology-preserving-transform (TPT) state variable filter, after
    Zavalishin ("The Art of VA Filter Design") and Andrew Simper's Cytomic
    trapezoidal SVF. Unlike the classic Chamberlin SVF it stays stable and
    accurate right up to Nyquist and at very high resonance — exactly what we
    need for a fast-swept, high-"Peak" envelope filter.

    Coefficients are split from per-channel state so the engine can compute the
    (expensive) tan() prewarp once per sample and share it across channels. */

struct SVFCoeffs
{
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;
};

inline SVFCoeffs computeSVFCoeffs (double cutoffHz, double q, double sampleRate) noexcept
{
    cutoffHz = std::clamp (cutoffHz, 20.0, sampleRate * 0.49); // keep below Nyquist
    q        = std::max (0.05, q);

    const double g     = std::tan (kPi * cutoffHz / sampleRate);
    const double k     = 1.0 / q;                 // damping: small k => high resonance
    const double denom = 1.0 / (1.0 + g * (g + k));

    return { (float) denom,
             (float) (g * denom),
             (float) (g * g * denom),
             (float) k };
}

struct SVFOutputs
{
    float lp, bp, hp;
};

struct SVFState
{
    float ic1eq = 0.0f, ic2eq = 0.0f; // integrator capacitor states

    void reset() noexcept { ic1eq = ic2eq = 0.0f; }

    inline SVFOutputs process (const SVFCoeffs& c, float v0) noexcept
    {
        const float v3 = v0 - ic2eq;
        const float v1 = c.a1 * ic1eq + c.a2 * v3;
        const float v2 = ic2eq + c.a2 * ic1eq + c.a3 * v3;

        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        const float hp = v0 - c.k * v1 - v2;
        return { v2, v1, hp }; // lp, bp, hp
    }

    /*  Nonlinear variant: the integrator capacitor states are soft-clipped each
        sample. Below the headroom it is essentially linear; as resonance rings
        up it saturates instead of blowing up — analog filters self-limit this
        way, and it's what stops a parked resonant peak from "swelling" ugly on
        certain notes. */
    inline SVFOutputs processNL (const SVFCoeffs& c, float v0, float headroom) noexcept
    {
        const float v3 = v0 - ic2eq;
        const float v1 = c.a1 * ic1eq + c.a2 * v3;
        const float v2 = ic2eq + c.a2 * ic1eq + c.a3 * v3;

        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        const float inv = 1.0f / headroom;
        ic1eq = headroom * std::tanh (ic1eq * inv);
        ic2eq = headroom * std::tanh (ic2eq * inv);

        const float hp = v0 - c.k * v1 - v2;
        return { v2, v1, hp };
    }
};

} // namespace zt
