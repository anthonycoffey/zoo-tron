// Render a musical phrase through the Zoo-Tron engine to a WAV, for voicing by ear.
//   clang++ -std=c++17 -O2 -I ../Source render.cpp -o render
//   ./render out.wav --range hi --mode bp --dir up --gain 5.5 --peak 6 --out 0 --mix 100 [--in di.wav]
// With no --in, synthesizes a dynamic funk lick (Karplus-Strong plucks).
#include "dsp/MuTronEngine.h"
#include "wav.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

static const double FS = 48000.0;

// One Karplus-Strong plucked note.
static void pluck (std::vector<float>& buf, int start, double freq, float amp, double seconds)
{
    const int N = std::max (2, (int) std::lround (FS / freq));
    std::vector<float> dl ((size_t) N);
    static uint32_t seed = 22222;
    for (int i = 0; i < N; ++i) { seed = seed * 1664525u + 1013904223u; dl[i] = ((seed >> 9) / 8388608.0f - 1.0f) * amp; }
    const int len = (int) (seconds * FS);
    const float decay = 0.996f;
    for (int i = 0; i < len; ++i)
    {
        const int idx = start + i;
        if (idx >= (int) buf.size()) break;
        const int j = i % N, j2 = (i + 1) % N;
        buf[(size_t) idx] += dl[(size_t) j];
        dl[(size_t) j] = decay * 0.5f * (dl[(size_t) j] + dl[(size_t) j2]);
    }
}

static std::vector<float> synthRiff()
{
    const double bpm = 110.0;
    const int sixteenth = (int) (FS * 60.0 / bpm / 4.0);
    std::vector<float> buf ((size_t) (FS * 7.0), 0.0f);
    // {sixteenth index, freq Hz, amplitude} — accents + ghost notes give touch dynamics.
    struct Ev { int s; double f; float a; };
    const Ev riff[] = {
        {0, 82.41, 1.0f}, {2, 164.8, 0.45f}, {3, 82.41, 0.7f}, {6, 98.0, 0.9f},
        {8, 110.0, 1.0f}, {10, 82.41, 0.5f}, {11, 98.0, 0.8f}, {14, 146.8, 0.9f},
        {16, 82.41, 1.0f}, {18, 164.8, 0.45f}, {19, 82.41, 0.7f}, {22, 98.0, 0.9f},
        {24, 110.0, 1.0f}, {26, 130.8, 0.6f}, {27, 110.0, 0.7f}, {28, 98.0, 0.8f}, {30, 82.41, 0.95f},
    };
    for (const auto& e : riff) pluck (buf, e.s * sixteenth, e.f, e.a, 1.1);
    // Normalize to a clean instrument level (peak ~0.6) so the dry isn't clipped
    // and the envelope follower sees realistic dynamics.
    float pk = 1e-9f;
    for (float s : buf) pk = std::max (pk, std::fabs (s));
    const float norm = 0.6f / pk;
    for (float& s : buf) s *= norm;
    return buf;
}

int main (int argc, char** argv)
{
    if (argc < 2) { printf ("usage: render out.wav [--range lo|hi --mode lp|bp|hp --dir up|down --gain f --peak f --out dB --mix %% --in file.wav]\n"); return 1; }
    std::string outPath = argv[1], inPath;
    float gain = 5.5f, peak = 6.0f, outDb = 0.0f, mix = 100.0f;
    zt::Range range = zt::Range::High; zt::Mode mode = zt::Mode::BandPass; zt::Direction dir = zt::Direction::Up;

    for (int i = 2; i < argc - 1; ++i)
    {
        std::string a = argv[i], v = argv[i + 1];
        if      (a == "--gain") gain = (float) atof (v.c_str());
        else if (a == "--peak") peak = (float) atof (v.c_str());
        else if (a == "--out")  outDb = (float) atof (v.c_str());
        else if (a == "--mix")  mix = (float) atof (v.c_str());
        else if (a == "--in")   inPath = v;
        else if (a == "--range") range = (v == "lo" ? zt::Range::Low : zt::Range::High);
        else if (a == "--mode")  mode = (v == "lp" ? zt::Mode::LowPass : v == "hp" ? zt::Mode::HighPass : zt::Mode::BandPass);
        else if (a == "--dir")   dir = (v == "down" ? zt::Direction::Down : zt::Direction::Up);
    }

    std::vector<float> in;
    if (! inPath.empty()) { wav::Data d; if (! wav::read (inPath, d)) { printf ("cannot read %s\n", inPath.c_str()); return 1; } in = wav::toMono (d); }
    else in = synthRiff();

    zt::MuTronEngine eng; eng.prepare (FS, 1);
    eng.setSensitivity (gain); eng.setPeak (peak);
    eng.setRange (range); eng.setMode (mode); eng.setDirection (dir);

    const float g  = std::pow (10.0f, outDb / 20.0f);
    const float wf = std::min (1.0f, std::max (0.0f, mix * 0.01f));

    std::vector<float> out (in.size());
    const int block = 256;
    float envMin = 1.0f, envMax = 0.0f;
    for (size_t pos = 0; pos < in.size(); pos += block)
    {
        const int n = (int) std::min ((size_t) block, in.size() - pos);
        std::vector<float> work (in.begin() + pos, in.begin() + pos + n);
        float* ch[1] = { work.data() };
        eng.process (ch, 1, n);
        const float e = eng.lastEnvelope();
        envMin = std::min (envMin, e); envMax = std::max (envMax, e);
        for (int i = 0; i < n; ++i)
        {
            const float dry = in[pos + i];
            out[pos + i] = std::max (-1.0f, std::min (1.0f, (dry * (1.0f - wf) + work[i] * wf) * g));
        }
    }
    fprintf (stderr, "    sweep env: %.2f .. %.2f\n", envMin, envMax);

    if (! wav::write (outPath, out.data(), (int) out.size(), 1, (int) FS)) { printf ("write failed\n"); return 1; }
    printf ("wrote %s  (%.1fs)\n", outPath.c_str(), out.size() / FS);
    return 0;
}
