// Quick objective readout of rendered audio: level + brightness (HF/LF balance)
// and how much the brightness moves over time (the "sweep depth").
//   clang++ -std=c++17 -O2 render.o? no — standalone:  clang++ -std=c++17 -O2 analyze.cpp -o analyze
#include "wav.h"
#include <cstdio>
#include <cmath>
#include <vector>

int main (int argc, char** argv)
{
    printf ("%-34s %8s %8s %10s %10s\n", "file", "peak", "rms", "bright_dB", "sweep_dB");
    for (int a = 1; a < argc; ++a)
    {
        wav::Data d; if (! wav::read (argv[a], d)) { printf ("  (cannot read %s)\n", argv[a]); continue; }
        auto x = wav::toMono (d);
        const double fs = d.sampleRate;
        const double coef = std::exp (-2.0 * 3.14159265 * 800.0 / fs); // one-pole split @800 Hz
        double lp = 0.0, peak = 0.0, sumSq = 0.0;
        double hiEnergyWin = 0.0, loEnergyWin = 0.0; // for windowed brightness movement
        std::vector<float> brightTrack;
        const int win = (int) (fs * 0.05); int wc = 0; double wh = 0, wl = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const double s = x[i];
            lp = s + coef * (lp - s);
            const double hi = s - lp;
            peak = std::max (peak, std::fabs (s));
            sumSq += s * s;
            wh += hi * hi; wl += lp * lp;
            if (++wc >= win) { if (wl > 1e-12 && wh > 1e-12) brightTrack.push_back ((float) (10.0 * std::log10 (wh / wl))); wh = wl = 0; wc = 0; }
            hiEnergyWin += hi * hi; loEnergyWin += lp * lp;
        }
        const double rms = std::sqrt (sumSq / x.size());
        const double bright = 10.0 * std::log10 ((hiEnergyWin + 1e-12) / (loEnergyWin + 1e-12));
        double lo = 1e9, hg = -1e9;
        for (float v : brightTrack) { lo = std::min (lo, (double) v); hg = std::max (hg, (double) v); }
        const double sweep = brightTrack.empty() ? 0.0 : (hg - lo);
        const char* slash = std::strrchr (argv[a], '/');
        printf ("%-34s %8.3f %8.4f %+9.1f %10.1f\n", slash ? slash + 1 : argv[a], peak, rms, bright, sweep);
    }
    return 0;
}
