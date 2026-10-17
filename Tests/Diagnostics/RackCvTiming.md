# VCV Rack keyboard pitch timing

The Rack autosave inspected on 2026-10-08 routes MPKmini2 MIDI through MIDI-CV
(eight channels, Rotate), with pitch through ML Quantum into Host V/OCT and gate
directly from MIDI-CV into Host GATE. The paths have different cable counts.

[Rack adds one sample per cable](https://vcvrack.com/manual/VoltageStandards#Timing).
[Host creates notes from pitch at the rising gate](https://vcvrack.com/Host).
When a channel rotates back into use, its old, released pitch is still present
when gate rises; the newly quantized pitch arrives one sample later.

This diagnostic runs Rack 2.6.6's actual SDK engine and MIDI parser, loading the
installed ML Quantum 2.0.3 and VCV VCA 2.6.4 modules. It measures the pitch at
each rising gate. It does **not** execute VCV Host's proprietary VST conversion.
No audio device, hardware MIDI device, GUI, or user patch modification is used.

The separate audio test below also verifies the actual Host conversion and
Macro OSC VST3 playback.

On macOS ARM64, with the official Rack SDK unzipped:

```sh
RACK_SDK=/path/to/Rack-SDK
clang++ -std=c++17 -O2 -Wno-deprecated-declarations \
  -I"$RACK_SDK/include" -I"$RACK_SDK/dep/include" \
  Tests/Diagnostics/RackCvTiming.cpp -L"$RACK_SDK" -lRack \
  -o build/RackCvTiming
DYLD_LIBRARY_PATH="$RACK_SDK" build/RackCvTiming \
  "$HOME/Library/Application Support/Rack2" original
DYLD_LIBRARY_PATH="$RACK_SDK" build/RackCvTiming \
  "$HOME/Library/Application Support/Rack2" aligned
DYLD_LIBRARY_PATH="$RACK_SDK" build/RackCvTiming \
  "$HOME/Library/Application Support/Rack2" direct
```

Use the SDK's `libRack.dylib`, not the Pro app's internal library. The standalone
harness uses internal engine construction APIs that aren't compatible with Pro's
additional application context. The diagnostic loads modules but never widgets.

`original` must exit 1: on the ninth press, MIDI note 67 sees released note 48;
69 sees 50; 72 sees 52, and so on. `aligned` must exit 0: it puts a VCV VCA at
100% gain in the gate path, so both paths have two cables. `direct` must also
exit 0: pitch and gate each use one cable. An optional final channel count of
`1` gives the smaller reproduction: each press sees the preceding pitch.

For the patch fix, connect MIDI-CV GATE to the VCA's **signal input**, leave its
CV input unpatched, set gain to 100%, and connect its output to Host GATE. Keep
MIDI-CV V/OCT → Quantum → Host V/OCT. Do not filter valid MIDI notes in the
synth to compensate for CV timing before the MIDI conversion.

## Actual Host audio regression

With VCV Rack Pro, licensed VCV Host, ML Quantum, and Fundamental installed:

```sh
python3 Tests/Diagnostics/rack_host_timing.py \
  --sdk /path/to/Rack-SDK \
  --plugin "build/MacroOsc_artefacts/Release/VST3/Macro OSC.vst3"
```

The runner builds `RackHostFixture.cpp`, copies the required installed modules
into temporary Rack user folders, and starts three headless Rack processes.
It does not use the normal Rack autosave, change the installed modules, or
connect to an audio or MIDI device. The temporary folders are deleted on exit.
Optional `--app` and `--user` arguments override the normal macOS locations.

The fixture plays and releases sixteen distinct notes through eight rotating
MIDI-CV channels, covering reuse of every channel. It records the actual Host
2.0.4 output with Macro OSC loaded at 48 kHz and a 256-sample Host buffer. A
deterministic SINE/TRI patch makes a sample comparison possible.

The assertions require audible, finite output and all three outcomes:

- Original Quantum pitch/direct gate wiring differs from direct keyboard audio.
- With a unity-gain VCA in the gate path, audio matches the direct reference to
  within 1e-6 V, allowing exactly one sample for the extra cable.
- Every released note becomes silent before the next key press.

This verifies the CV-to-MIDI-to-audio path rather than assuming Host's behavior
from its documentation. Hardware MIDI transport and overlapping live chords
are outside this fixture; the regular DSP and hosted VST3 tests cover overlapping
notes, release, voice stealing, multiple channels, sample rates, and block sizes.
