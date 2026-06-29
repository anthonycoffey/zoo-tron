// Offline sanity checks for the Zoo-Tron DSP core (no JUCE required).
//   clang++ -std=c++17 -O2 -I ../Source engine_test.cpp -o engine_test && ./engine_test
#include "dsp/MuTronEngine.h"
#include <cstdio>
#include <cmath>
#include <vector>

static double rms (const std::vector<float>& v, int from)
{
    double s = 0.0; int n = 0;
    for (int i = from; i < (int) v.size(); ++i) { s += (double) v[i] * v[i]; ++n; }
    return std::sqrt (s / std::max (1, n));
}

// Steady-state gain (dB) of one SVF tap at frequency f, cutoff fc, Q.
static double svfGainDb (int tap, double f, double fc, double q, double fs)
{
    const auto c = zt::computeSVFCoeffs (fc, q, fs);
    zt::SVFState st;
    const int N = (int) (fs * 0.5);
    std::vector<float> out (N);
    for (int n = 0; n < N; ++n)
    {
        const float x = (float) std::sin (2.0 * zt::kPi * f * n / fs);
        const auto o = st.process (c, x);
        out[n] = (tap == 0 ? o.lp : tap == 1 ? o.bp : o.hp);
    }
    const double g = rms (out, N / 2) / 0.70710678; // input RMS of unit sine
    return 20.0 * std::log10 (std::max (1e-9, g));
}

int main()
{
    const double fs = 48000.0;
    bool ok = true;

    printf ("== SVF frequency response (fc=1000 Hz, Q=2) ==\n");
    const char* names[3] = { "LowPass ", "BandPass", "HighPass" };
    double f[3] = { 100.0, 1000.0, 8000.0 };
    for (int tap = 0; tap < 3; ++tap)
    {
        printf ("  %s:", names[tap]);
        for (double ff : f) printf ("  %6.0fHz=%+6.1fdB", ff, svfGainDb (tap, ff, 1000.0, 2.0, fs));
        printf ("\n");
    }
    // Expectations: LP passes 100/attenuates 8k; HP opposite; BP peaks at 1k.
    ok &= svfGainDb (0, 100, 1000, 2, fs) > svfGainDb (0, 8000, 1000, 2, fs) + 20.0;
    ok &= svfGainDb (2, 8000, 1000, 2, fs) > svfGainDb (2, 100, 1000, 2, fs) + 20.0;
    ok &= svfGainDb (1, 1000, 1000, 2, fs) > svfGainDb (1, 100, 1000, 2, fs) + 6.0;
    printf ("  -> shapes %s\n\n", ok ? "OK" : "FAIL");

    printf ("== Vactrol dynamics (fast attack, slow level-dependent release) ==\n");
    zt::Vactrol vac; vac.prepare (fs);
    int riseSamps = -1, fallSamps = -1;
    for (int n = 0; n < (int) (fs * 0.02); ++n) { vac.process (1.0f); if (riseSamps < 0 && vac.current() > 0.9f) riseSamps = n; }
    for (int n = 0; n < (int) (fs * 1.0);  ++n) { vac.process (0.0f); if (fallSamps < 0 && vac.current() < 0.1f) fallSamps = n; }
    printf ("  attack to 0.9: %.1f ms   release to 0.1: %.1f ms\n",
            1000.0 * riseSamps / fs, 1000.0 * fallSamps / fs);
    const bool vacOk = (riseSamps > 0 && fallSamps > riseSamps * 5);
    ok &= vacOk;
    printf ("  -> release slower than attack: %s\n\n", vacOk ? "OK" : "FAIL");

    printf ("== Full engine: louder input sweeps further, output stays finite ==\n");
    auto burstPeak = [&] (float amp)
    {
        zt::MuTronEngine eng; eng.prepare (fs, 2);
        eng.setSensitivity (5.0f); eng.setPeak (5.0f);
        eng.setRange (zt::Range::High); eng.setMode (zt::Mode::BandPass);
        eng.setDirection (zt::Direction::Up);
        const int N = 2048; std::vector<float> L (N), R (N);
        float maxEnv = 0.0f, maxAbs = 0.0f; bool finite = true;
        for (int blk = 0; blk < 24; ++blk)
        {
            for (int n = 0; n < N; ++n)
            {
                const int t = blk * N + n;
                const float decay = std::exp (-t / (fs * 0.15f));
                const float s = amp * decay * std::sin (2.0 * zt::kPi * 110.0 * t / fs);
                L[n] = R[n] = s;
            }
            float* ch[2] = { L.data(), R.data() };
            eng.process (ch, 2, N);
            for (int n = 0; n < N; ++n)
            {
                if (! std::isfinite (L[n]) || ! std::isfinite (R[n])) finite = false;
                maxAbs = std::max (maxAbs, std::fabs (L[n]));
            }
            maxEnv = std::max (maxEnv, eng.lastEnvelope());
        }
        printf ("  amp=%.2f -> peak vactrol=%.3f  peak out=%.3f  finite=%s\n",
                amp, maxEnv, maxAbs, finite ? "yes" : "NO");
        ok &= finite;
        return maxEnv;
    };
    const float quiet = burstPeak (0.1f);
    const float loud  = burstPeak (0.9f);
    const bool sweepOk = loud > quiet;
    ok &= sweepOk;
    printf ("  -> louder opens filter more: %s\n\n", sweepOk ? "OK" : "FAIL");

    printf ("== Nonlinear resonance self-limits (kills runaway swell) ==\n");
    {
        const double fc = 800.0, qHigh = 16.0;
        const auto c = zt::computeSVFCoeffs (fc, qHigh, fs);
        zt::SVFState lin, nl;
        float linPeak = 0.0f, nlPeak = 0.0f; bool nlFinite = true;
        const int N = (int) (fs * 1.0);
        for (int n = 0; n < N; ++n)
        {
            const float x = 0.5f * (float) std::sin (2.0 * zt::kPi * fc * n / fs); // drive AT resonance
            linPeak = std::max (linPeak, std::fabs (lin.process (c, x).bp));
            const float y = nl.processNL (c, x, 3.0f).bp;
            nlPeak = std::max (nlPeak, std::fabs (y));
            if (! std::isfinite (y)) nlFinite = false;
        }
        printf ("  driven at resonance (Q=16): linear peak=%.2f  nonlinear peak=%.2f  finite=%s\n",
                linPeak, nlPeak, nlFinite ? "yes" : "NO");
        const bool nlOk = nlFinite && nlPeak < linPeak * 0.9f;
        ok &= nlOk;
        printf ("  -> nonlinear path stays bounded: %s\n\n", nlOk ? "OK" : "FAIL");
    }

    printf ("== Detector high-pass tames low-note over-trigger ==\n");
    {
        auto envForTone = [&] (double freq)
        {
            zt::MuTronEngine eng; eng.prepare (fs, 1);
            eng.setSensitivity (5.0f); eng.setPeak (5.0f);
            const int N = 2048; std::vector<float> buf ((size_t) N);
            float maxEnv = 0.0f;
            for (int blk = 0; blk < 16; ++blk)
            {
                for (int n = 0; n < N; ++n)
                    buf[(size_t) n] = 0.3f * (float) std::sin (2.0 * zt::kPi * freq * (blk * N + n) / fs);
                float* ch[1] = { buf.data() };
                eng.process (ch, 1, N);
                maxEnv = std::max (maxEnv, eng.lastEnvelope());
            }
            return maxEnv;
        };
        const float lowEnv = envForTone (50.0);
        const float midEnv = envForTone (300.0);
        printf ("  50 Hz env=%.3f   300 Hz env=%.3f  (identical input level)\n", lowEnv, midEnv);
        const bool hpOk = midEnv > lowEnv;
        ok &= hpOk;
        printf ("  -> lows trigger the sweep less than mids: %s\n\n", hpOk ? "OK" : "FAIL");
    }

    printf ("== LFO source sweeps the filter with no playing ==\n");
    {
        zt::MuTronEngine eng; eng.prepare (fs, 1);
        eng.setSource (zt::Source::Lfo); eng.setLfoShape (0); eng.setLfoRate (4.0f);
        eng.setRange (zt::Range::High); eng.setPeak (5.0f);
        const int N = 1024; std::vector<float> buf ((size_t) N);
        float fcLo = 1.0e9f, fcHi = -1.0e9f;
        for (int blk = 0; blk < 48; ++blk) // ~1 s
        {
            for (int n = 0; n < N; ++n)
                buf[(size_t) n] = 0.1f * (float) std::sin (2.0 * zt::kPi * 220.0 * (blk * N + n) / fs);
            float* ch[1] = { buf.data() };
            eng.process (ch, 1, N);
            fcLo = std::min (fcLo, eng.lastCutoff());
            fcHi = std::max (fcHi, eng.lastCutoff());
        }
        printf ("  LFO @4 Hz cutoff range: %.0f .. %.0f Hz\n", fcLo, fcHi);
        const bool lfoOk = fcHi > fcLo * 1.5f;
        ok &= lfoOk;
        printf ("  -> sweeps on its own: %s\n\n", lfoOk ? "OK" : "FAIL");
    }

    printf ("== Stereo width detunes L/R cutoff ==\n");
    {
        zt::MuTronEngine eng; eng.prepare (fs, 2);
        eng.setSource (zt::Source::Manual); eng.setManual (0.6f); eng.setWidth (1.0f);
        eng.setPeak (6.0f); eng.setMode (zt::Mode::BandPass);
        const int N = 2048; std::vector<float> L ((size_t) N), R ((size_t) N);
        float maxDiff = 0.0f;
        for (int blk = 0; blk < 8; ++blk)
        {
            for (int n = 0; n < N; ++n)
            {
                const float s = 0.3f * (float) std::sin (2.0 * zt::kPi * 500.0 * (blk * N + n) / fs);
                L[(size_t) n] = R[(size_t) n] = s;
            }
            float* ch[2] = { L.data(), R.data() };
            eng.process (ch, 2, N);
            for (int n = 0; n < N; ++n)
                maxDiff = std::max (maxDiff, std::fabs (L[(size_t) n] - R[(size_t) n]));
        }
        printf ("  width=100%%, identical mono in: max|L-R|=%.4f\n", maxDiff);
        const bool wOk = maxDiff > 0.01f;
        ok &= wOk;
        printf ("  -> stereo image diverges: %s\n\n", wOk ? "OK" : "FAIL");
    }

    printf ("%s\n", ok ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    return ok ? 0 : 1;
}
