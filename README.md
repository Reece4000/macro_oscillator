# Macro OSC

Macro OSC is a compact software synthesizer built around the Mutable Instruments Braids macro oscillator engine. It brings Braids-style oscillator models into a focused JUCE instrument with direct pitch, timbre, color, modulation, FM, envelope, and multi-stage modulation controls. Choose an oscillator model, shape its core tone from the main panel, then use the MSEG modulators to add movement.

![Macro OSC user interface](ui.png)

## Highlights

- Braids macro oscillator DSP with all 47 models, shown as a live waveform of the selected model.
- VST3 plugin and standalone app targets.
- A flat, dark 960 × 640 interface, scalable from 75% to 150%, that keeps the Braids colours: turquoise and magenta controls and a yellow-green 14-segment model readout.
- Poly (1–8 voices), mono and legato modes, with constant-time glide.
- Direct control over model, timbre, color, modulation amount, FM amount, pitch, detune, glide, and ADSR envelope shape.
- Lodo's distortion bank: 19 modes with Amount, Mix and Output controls.
- Lodo's low-pass, band-pass and high-pass filter with Frequency and Q controls.
- Three MSEG slots with editable curves, loop and tempo sync, scale, offset, rate and per-note random, targeting oscillator controls, level, and every filter/distortion parameter, including type.
- Presets, an output volume, and a clean output stage: per-voice headroom, resonance gain compensation and a transparent limiter.

## Interface

The header holds the settings menu, the current preset, output volume, the voice count and the poly / mono / legato switch. The settings menu saves, opens and lists presets (stored in `~/Documents/Macro OSC/Presets`), sets the interface size and tooltips, and in the standalone app opens the audio and MIDI settings.

The oscillator section shows the model index and name. Choose a model from the menu, step through all 47 with the arrows, or click a cell in the model map below, where models are grouped in Braids' own families. The waveform shows the selected model at its base Timbre and Color. Timbre, Color, Modulation and FM sit beneath it. Every control is a slider: drag in any direction, hold Shift for fine adjustment, use the arrow keys, or double-click or press Backspace to reset.

The silver band holds pitch, detune and glide beside the amplitude envelope, each drawn at its current size. Times read in milliseconds or seconds.

The modulation section has slots 1–3, loop and sync, and a target menu beside a wide curve editor. Click to add a point, drag to move it, and right-click to remove it. Remove points down to a single one for a constant level per note. The dashed line shows the curve after scale and offset. Random moves each point by its own amount, drawn per voice at every note and every loop cycle. Shaded boxes show its range, and on a single point it gives each note its own fixed offset (for example, random pitch per note).

Distortion and filter run independently per voice after the amplitude envelope,
in Lodo's distortion-then-filter order. Both default to Off. Distortion Amount,
Mix and Output cover 0–100%; Output affects only the wet signal, with unity gain
at 50%. Filter Frequency spans 20–20,000 Hz and Q spans 1–30. Frequency modulation
uses a logarithmic range. Lodo's inactive Shelf control is omitted.

The distortion display renders a 100 Hz reference through the selected DSP,
showing its input and processed waveform. The filter display shows
its frequency response; drag its handle horizontally for Frequency and vertically
for Q. Both displays read “bypassed” when their effect is off. These displays show base settings; MSEG modulation is applied per voice. Hover for details.

Choose an effect parameter in any MSEG destination menu to modulate it. Type
changes use Lodo's short crossfades; continuous controls are smoothed. Existing
presets retain their modulation destinations and load with the new effects
bypassed.

In poly mode, up to eight voices rotate through available slots. Fresh notes
reset oscillator history, MIDI is handled at exact sample positions, and
stealing a busy voice crossfades its outgoing sound over 5 ms. Sustain and note
releases retain their normal MIDI behavior. Mono and legato play the newest key
and return to keys still held; mono retriggers the envelope, legato does not.
Glide takes its set time whatever the interval, starting from the current pitch.
In poly mode it follows the last played note on the same MIDI channel. Playback
restart clears held notes and their history.

Each voice keeps about 9 dB of headroom and filter resonance is gain-compensated,
so chords stay clean; a limiter and soft clipper at the output catch the rest and
leave audio below −1 dBFS untouched.

When using VCV Host, pitch and gate must arrive together. If V/OCT passes
through a quantizer but GATE connects directly, Host can trigger the previous
pitch on that polyphonic channel. Put a VCV VCA at 100% gain in the gate path
(signal input, CV unpatched), or connect both signals directly. See the
[Rack timing reproduction](Tests/Diagnostics/RackCvTiming.md).

## Formats

Macro OSC builds as:

- VST3 instrument plugin
- Standalone desktop app

## Download

The current macOS build is available from the
[v0.2.0 release](https://github.com/Reece4000/macro_oscillator/releases/tag/v0.2.0):

- `MacroOSC-macOS-arm64-v0.2.0-vst3.zip`: unzip it and copy `Macro OSC.vst3`
  to `~/Library/Audio/Plug-Ins/VST3/`.
- `MacroOSC-macOS-arm64-v0.2.0-standalone.zip`: unzip it and open `Macro OSC.app`.

These builds are for Apple Silicon Macs and are ad-hoc signed.

## Development

Macro OSC is a CMake-based JUCE project. Configure it with a JUCE checkout, then build the generated project for your platform:

```sh
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --config Debug
```

Build products are written under the local `build/` directory.

DSP regressions can be run with:

```sh
cmake --build build --target MacroOscDspTests MacroOscVst3Tests
ctest --test-dir build --output-on-failure
```

The tests cover all 47 Braids models, deterministic initialization, arbitrary
host block sizes at 44.1/48/96 kHz, all distortion modes, filter response,
effect modulation, preset compatibility, MIDI timing, voice reuse and stealing,
sustain, glide timing, mono and legato, voice limits, output safety, strike
models (plucked, bowed, bell, drum), MSEG random and single-point shapes,
presets, editor size, and control gestures. The VST3 test loads the
built bundle and checks overlapping keyboard notes and silence after release
through the actual plugin interface. Braids continuity/initialization
fixes are ported from Lodo commit `5993c99` and the matching Taperage changes.
This project contains no Plaits engine, so its resampler fix does not apply.

## License

Except where noted below, Macro OSC source code and first-party assets are
licensed under the GNU Affero General Public License version 3 or later. See
`LICENSE`.

Third-party source and asset notices are listed in `THIRD_PARTY_NOTICES.md`.
That includes the Mutable Instruments Braids/stmlib sources under
`Source/ThirdParty/Mutable/`, the bundled Geist and DSEG fonts, and the external
JUCE dependency.
