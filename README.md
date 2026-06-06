# Macro OSC

A JUCE VST3/standalone synth plugin built around the Mutable Instruments Braids macro oscillator DSP vendored from `IDIMide_V2`.

## Build

```sh
cmake -S . -B build
cmake --build build --config Debug -j8
```

By default, `CMakeLists.txt` expects JUCE at:

```sh
/Users/reecemitchell/Programming/JUCE
```

Override it with:

```sh
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
```

## Artifacts

The build creates:

- `build/MacroOsc_artefacts/VST3/Macro OSC.vst3`
- `build/MacroOsc_artefacts/Standalone/Macro OSC.app`

With `COPY_PLUGIN_AFTER_BUILD` enabled, JUCE also copies the VST3 into:

```sh
~/Library/Audio/Plug-Ins/VST3/Macro OSC.vst3
```

## Controls

The main device face uses the IDIMide rack backing image. Drag directly on the model display, Timbre, Color, Modulation, and FM readouts to edit those values. Pitch and envelope controls sit below the device image.

The plugin has three MSEG slots. Click a slot to select it, then edit its destination, amount, offset, rate, loop state, and curve. Click the MSEG graph to add points, drag existing points to move them, and double-click interior points to remove them.

The Mutable DSP license is included at `Source/ThirdParty/Mutable/LICENSE`.
