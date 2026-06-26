# Zoo-Tron III

A faithful recreation of the Musitronics **Mu-Tron III** envelope filter as a
VST3 / AU / Standalone plugin, built with [JUCE](https://juce.com).

It models the original's analog signal chain rather than approximating it with a
generic auto-wah:

- **Envelope follower** — full-wave rectifier + asymmetric one-pole detector.
- **Vactrol (opto-isolator)** — fast attack, slow *level-dependent* release;
  the source of the Mu-Tron's vocal, rubbery sweep.
- **State-variable filter** — TPT/Cytomic 2-pole SVF, stable at high resonance,
  with simultaneous Low Pass / Band Pass / High Pass taps.
- **Op-amp character** — gentle `tanh` saturation, 2× oversampled.

<img width="257" height="449" alt="image" src="https://github.com/user-attachments/assets/2f8014fa-fc7b-4fe4-ab4c-a09d65512032" />



## Controls

| Control | What it does |
|---|---|
| **Gain** | Envelope sensitivity — how hard your playing sweeps the filter |
| **Peak** | Resonance / Q (quack); approaches self-oscillation near max |
| **Range** | Low or High frequency sweep band |
| **Mode** | Low Pass / Band Pass / High Pass |
| **Direction** | Up (louder = brighter) or Down (louder = darker) |
| **Output** | Master output trim (±24 dB) |
| **Mix** | Dry/wet (100% = faithful) |

## Build

Requires CMake ≥ 3.22 and a C++17 toolchain (Xcode on macOS). JUCE is fetched
automatically.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Artifacts land in `build/ZooTron_artefacts/`. With `COPY_PLUGIN_AFTER_BUILD`,
the AU and VST3 are also installed to your user plugin folders.

## Validate (macOS)

```sh
auval -v aufx Zt3a Coff      # Audio Unit validation
```
