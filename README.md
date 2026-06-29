# Zoo-Tron III

A faithful recreation of the Musitronics **Mu-Tron III** envelope filter as an
AU / VST3 / Standalone plugin, built with [JUCE](https://juce.com) — and tuned
for use as an insert in Universal Audio **LUNA**.

It models the original's analog signal chain rather than approximating it with a
generic auto-wah:

- **Envelope follower** — full-wave rectifier + asymmetric one-pole detector,
  with a tunable **sidechain high-pass** (Contour) so fat low notes don't
  over-trigger the sweep.
- **Vactrol (opto-isolator)** — fast attack, slow *level-dependent* release
  (set by the Attack/Release knobs); the source of the Mu-Tron's vocal,
  rubbery sweep.
- **State-variable filter** — TPT/Cytomic 2-pole SVF with **nonlinear,
  self-limiting resonance** that saturates instead of "swelling" out of control,
  and simultaneous Low Pass / Band Pass / High Pass taps.
- **ADAA drive** — antiderivative anti-aliased `tanh` saturation, 2× oversampled.

<img width="257" height="449" alt="image" src="https://github.com/user-attachments/assets/2f8014fa-fc7b-4fe4-ab4c-a09d65512032" />



## Controls

| Control | What it does |
|---|---|
| **Gain** | Envelope sensitivity — how hard your playing sweeps the filter |
| **Peak** | Resonance / Q (quack); approaches self-oscillation near max |
| **Contour** | Sidechain high-pass on the detector — tightens low-note response |
| **Attack** | How fast the filter opens on a note |
| **Release** | How slowly it closes (keeps the vactrol's level-dependent tail) |
| **Drive** | Anti-aliased analog grit into the output stage |
| **Range** | Low or High frequency sweep band |
| **Mode** | Low Pass / Band Pass / High Pass |
| **Direction** | Up (louder = brighter) or Down (louder = darker) |
| **Output** | Master output trim (±24 dB) |
| **Mix** | Dry/wet (100% = faithful) |

## Live display & presets

A real-time **filter scope** draws the response curve with a playhead that
sweeps along with the envelope as you play. **15 factory presets** (Bootsy,
Garcia, Cosmic Slop, Higher Ground, …) plus **user presets** saved to disk —
click the readout to browse the full list or save the current tone, or use
◀ ▶ to step through the factory patches.

## Build

Requires CMake ≥ 3.22 and a C++20 toolchain (Xcode on macOS). JUCE is fetched
automatically.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Artifacts land in `build/ZooTron_artefacts/`. With `COPY_PLUGIN_AFTER_BUILD`,
the AU and VST3 are also installed to your user plugin folders.

## Validate (macOS)

```sh
auval -v aufx Zt3a Ynme      # Audio Unit validation
```
