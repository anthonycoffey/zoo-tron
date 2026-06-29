#pragma once

#include <cmath>
#include <cstdint>

namespace zt
{
// A small unipolar (0..1) LFO with sine / triangle / square / sample-&-hold
// shapes, used as an alternative sweep source to the envelope follower.
class Lfo
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; reset(); }

    void reset() noexcept { phase = 0.0f; sh = 0.5f; rng = 22222u; }

    void setRate (float hz) noexcept   { inc = (float) (hz / fs); }
    void setShape (int s) noexcept     { shape = s; }

    inline float process() noexcept
    {
        bool wrapped = false;
        phase += inc;
        if (phase >= 1.0f) { phase -= 1.0f; wrapped = true; }

        switch (shape)
        {
            case 0:  return 0.5f + 0.5f * std::sin (2.0f * 3.14159265358979f * phase);     // sine
            case 1:  return phase < 0.5f ? phase * 2.0f : 2.0f - phase * 2.0f;             // triangle
            case 2:  return phase < 0.5f ? 1.0f : 0.0f;                                    // square
            default:                                                                        // sample & hold
                if (wrapped) { rng = rng * 1664525u + 1013904223u; sh = (float) (rng >> 9) / 8388608.0f; }
                return sh;
        }
    }

private:
    double   fs = 48000.0;
    float    inc = 0.0f, phase = 0.0f, sh = 0.5f;
    int      shape = 0;
    uint32_t rng = 22222u;
};

} // namespace zt
