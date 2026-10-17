# Macro OSC

**The Mutable Instruments Braids macro oscillator, reborn as a polyphonic software synth.**

All 47 Braids models, from virtual-analog saws to physical-modelled strings, formant voices, FM and granular noise, with up to eight voices, a full effects chain and drawable modulation that never plays the same way twice.

[![Latest release](https://img.shields.io/github/v/release/Reece4000/macro_oscillator?label=download&color=34c6cc)](https://github.com/Reece4000/macro_oscillator/releases/latest)
![VST3 and Standalone](https://img.shields.io/badge/format-VST3%20%7C%20Standalone-f2477f)
![macOS Apple Silicon](https://img.shields.io/badge/macOS-Apple%20Silicon-e8fa42)
[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-7d838f)](LICENSE)

![Macro OSC, playing the Attack Bass preset](ui.png)

## Why Macro OSC

Braids is one of the most loved Eurorack oscillators ever made: one module, 47 ways to make sound, each shaped by just two knobs, Timbre and Color. Macro OSC runs the original Braids DSP inside a modern instrument and adds what the module never had:

- **Polyphony.** Play chords across up to 8 voices, or switch to mono or legato for leads and basses.
- **See what you hear.** A live waveform of the selected model redraws as you move Timbre and Color.
- **Movement built in.** Three drawable MSEG modulators can target almost anything, from pitch and model to every filter and distortion setting.
- **Controlled randomness.** MSEG random nudges every point differently on every note and every loop, giving analog-style drift, evolving pads and per-note detune.
- **A proper effects chain per voice.** 19 distortion modes (including 13 Airwindows algorithms) and a resonant low-, band- and high-pass filter.

## Download

**[Get the latest release](https://github.com/Reece4000/macro_oscillator/releases/latest)**: macOS, Apple Silicon.

- **Plug-in:** unzip `MacroOSC-macOS-arm64-v0.2.0-vst3.zip` and copy `Macro OSC.vst3` to `~/Library/Audio/Plug-Ins/VST3/`.
- **Standalone app:** unzip `MacroOSC-macOS-arm64-v0.2.0-standalone.zip` and open `Macro OSC.app`. Pick your audio and MIDI devices from the menu (☰).

The builds are ad-hoc signed, so macOS may refuse to open them the first time. Allow them in **System Settings → Privacy & Security → Open Anyway**, or clear the download flag in Terminal:

```sh
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Macro OSC.vst3" "/path/to/Macro OSC.app"
```

Other platforms can [build from source](#build-from-source).

## Presets

Four presets come with the repo, each showing off a different side of the engine:

| Preset | Model | What it does |
|---|---|---|
| **Attack Bass** | SINE/TRI, mono | A two-octave-down mono bass through Grit distortion: a tempo-synced filter pluck, restless drive and a burst of FM on every attack. |
| **Airy Bursts** | FLUTED | Breathy, blown tones with Tape saturation. Timbre and filter drift on their own loops, and a short FM burst marks each note. |
| **Sad Boi** | BUZZ | A slow-attack, long-release pad through Stack distortion. Tempo-synced timbre and filter swells take a different shape on every note. |
| **Unstable Tines** | DRUM | Struck metallic tines with long decays and Heat saturation. A touch of random pitch drift keeps every note slightly out of true. |

**To install them,** download `MacroOSC-presets.zip` from the [latest release](https://github.com/Reece4000/macro_oscillator/releases/latest), or the [`Presets/`](Presets) folder, and copy the `.mosc` files to `~/Documents/Macro OSC/Presets`. They then appear in **☰ → Presets**. Save your own from the same menu.

## Features

**Oscillator**
- All 47 Braids models with Timbre, Color, Modulation and FM, running the original Braids DSP.
- A yellow-green 14-segment readout, plus a map of every model grouped by Braids' own families: one click to jump anywhere.

**Voices**
- Poly with 1–8 voices, mono, or legato with last-note priority.
- Constant-time glide, pitch, detune and an ADSR envelope.

**Effects (per voice)**
- 19 distortion modes with Amount, Mix and Output. The response preview shows a 100 Hz reference through the current settings.
- A resonant low-, band- or high-pass filter. Drag its response curve directly.

**Modulation**
- Three MSEG slots with loop and tempo sync.
- Scale, offset, rate and random controls on each slot.
- 14 destinations, including model, pitch, level and every filter and distortion parameter.
- Draw any curve, or reduce it to a single point for a fixed random offset on each note.

**Workflow**
- Presets.
- A resizable interface (75–150%).
- An output volume control.
- Clean gain staging: per-voice headroom, resonance compensation and a transparent output limiter, so big chords don't clip.

## Playing tips

- **Timbre and Color are everything.** Each model maps them differently. On PLUCKED, Timbre sets the decay and Color the pluck position. On the FM models, they set the modulation index and ratio.
- **Every control is a slider.** Drag in any direction, hold Shift for fine moves, use the arrow keys, and double-click to reset.
- **Drift:** route an MSEG to pitch with a small scale and some random for slow, analog-style wander.
- **Random per note:** right-click MSEG points until one is left, then raise random. Each note gets its own fixed offset.

## Build from source

Macro OSC is a CMake project built on [JUCE](https://juce.com). With a JUCE checkout:

```sh
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --config Release
```

Build products are written under `build/`. Run the tests with:

```sh
cmake --build build --target MacroOscDspTests MacroOscVst3Tests
ctest --test-dir build --output-on-failure
```

The tests cover:
- all 47 Braids models, including deterministic initialization and strike models (plucked, bowed, bell, drum);
- arbitrary host block sizes at 44.1, 48 and 96 kHz;
- every distortion mode, filter response and effect modulation;
- MIDI timing, voice allocation, sustain, glide timing, and mono and legato;
- output safety, MSEG random, presets and editor sizing.

The VST3 test loads the built bundle and plays it through the real plug-in interface.

## How it works

<details>
<summary>Signal flow, voices and host notes</summary>

Each voice runs a Braids macro oscillator, then the amplitude envelope, then distortion and filter in Lodo's distortion-then-filter order. Both effects default to Off. Distortion Output affects only the wet signal, with unity gain at 50%. Filter Frequency spans 20–20,000 Hz and Q spans 1–30; frequency modulation uses a logarithmic range. Effect type changes crossfade; continuous controls are smoothed. The distortion and filter displays show base settings; MSEG modulation is applied per voice.

In poly mode, voices rotate through free slots. Fresh notes reset oscillator history, MIDI is handled at exact sample positions, and stealing a busy voice crossfades it out over 5 ms. Mono and legato play the newest key and return to keys still held. Mono retriggers the envelope; legato does not. Glide takes its set time whatever the interval, starting from the current pitch.

Each voice keeps about 9 dB of headroom, and filter resonance is gain-compensated. A limiter and soft clipper at the output catch anything that's left, and leave audio below −1 dBFS untouched.

When using VCV Host, pitch and gate must arrive together. If V/OCT passes through a quantizer but GATE connects directly, Host can trigger the previous pitch on that polyphonic channel. Put a VCV VCA at 100% gain in the gate path (signal input, CV unpatched), or connect both signals directly. See the [Rack timing reproduction](Tests/Diagnostics/RackCvTiming.md).

Braids continuity and initialization fixes are ported from Lodo commit `5993c99` and the matching Taperage changes.

</details>

## Credits

- **[Mutable Instruments Braids](https://pichenettes.github.io/mutable-instruments-documentation/modules/braids/)** by Émilie Gillet: the oscillator DSP (MIT).
- **[Airwindows](https://www.airwindows.com)** by Chris Johnson: 13 of the distortion algorithms (MIT), adapted via Lodo.
- **Lodo:** the distortion bank, filter and their control profiles.
- **[JUCE](https://juce.com)**: the plug-in framework.
- **[Geist](https://vercel.com/font)** and **[DSEG](https://www.keshikan.net/fonts-e.html)**: the fonts (SIL OFL).

## License

Except where noted, Macro OSC source code and first-party assets are licensed under the GNU Affero General Public License version 3 or later. See [`LICENSE`](LICENSE). Third-party notices, covering Braids/stmlib, Airwindows, the fonts and JUCE, are in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

If Macro OSC makes something you like, a ⭐ helps other people find it.
