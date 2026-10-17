---
name: Macro OSC
description: A dark, flat, type-led instrument that keeps the Braids hardware's colours and segment readout.
colors:
  background: "#0c0d10"
  ink: "#eceef2"
  muted: "#7d838f"
  faint: "#26292f"
  dot: "#202329"
  turquoise: "#34c6cc"
  magenta: "#f2477f"
  display: "#e8fa42"
  chip: "#121418"
  band: "#c6c8ce"
  band-ink: "#111216"
  band-muted: "#5a5e67"
  band-track: "#a4a7af"
typography:
  menu: { fontFamily: Geist Mono, fontWeight: 400, fontSize: "12pt" }
  section: { fontFamily: Geist, fontWeight: 500, fontSize: "22pt" }
  label: { fontFamily: Geist, fontWeight: 500, fontSize: "14pt" }
  value: { fontFamily: Geist Mono, fontWeight: 400, fontSize: "12pt" }
  model-readout: { fontFamily: DSEG14 Modern, fontSize: "28pt" }
layout:
  size: "960 × 640, scalable 75–150% at a fixed aspect ratio"
  corners: "square everywhere"
  margin: "28"
---

# Design System: Macro OSC

## North star

A modern software instrument in the manner of fors: flat, dark, led by type and one hero visual, with no skeuomorphic chrome. Braids survives in its colours (turquoise Timbre/Modulation caps, magenta Color cap), its yellow-green 14-segment model readout, and a silver band standing in for the aluminium faceplate.

## Colour

- Near-black canvas; ink for labels, muted grey for section titles and values.
- Turquoise marks the oscillator macros, filter and modulation; magenta marks Color and the drive section. Each section's sliders fill in its colour.
- Yellow-green is reserved for the model readout and the current model's cell.
- The silver band (pitch and envelope) inverts the scheme: dark ink and dark fills on silver.
- Lit elements (LEDs, the current model cell, the readout) carry a soft glow. Nothing else has shadows, gradients or bevels.
- Every shape has square corners: tracks, buttons, LEDs, menus, tooltips, plot handles, even the grid dots.

## Type

All labels are lowercase. Section titles (osc, drive, filter, mod) are large and muted; parameter labels are ink; values are set in Geist Mono so they don't jitter. The mark is two linked rings, magenta and turquoise: each passes over the other at one crossing, with small gaps cut into the ring beneath. The current preset name follows it in Geist Mono. Menus are set in Geist Mono like the values. Sizes are point heights, matching CSS em sizes.

## Layout

From the top: a header strip (settings menu, mark and preset name on the left, voice count and voice mode on the right); oscillator on the left (readout, model map, waveform, four macros) beside drive and filter on the right; the silver pitch/envelope band across the full width; modulation at the bottom. Whitespace separates sections; there are no dividers or boxes except the readout chip and the band.

The view is designed at 960 × 640 and scaled as a whole by the editor, so every coordinate in `EditorView::resized` is a design unit.

## Components

- **Parameter slider:** a lowercase label, its value, and a square-cornered 6-unit track with a 2 × 12 tick. Bipolar parameters fill from centre. Drag horizontally or vertically; Shift is fine; double-click, Backspace or Delete resets; arrow keys step; Home/End go to the limits. Keyboard focus turns the label turquoise; controls are never boxed for focus, and mouse focus shows nothing.
- **Band graphics:** pitch and detune show their value in a dark block; glide, attack, decay, sustain and release draw their envelope segment at its current size. Fills and 2-unit lines share exact edges, so every segment sits on one baseline; attack and release sit on a full-width baseline, release always falls from the top so its length reads at any sustain level, and glide is a straight ramp because glide is linear in pitch.
- **Model readout:** DSEG14 with the unlit cells visible behind the lit text; click for the three-column model menu. Arrows step through all 47; the grid button opens the menu.
- **Model map:** 47 cells in the nine families of Braids' own model order (the blank-line groups in `braids/settings.h`); click a cell to select it. Only the current family is labelled, or the hovered one while pointing, because single-model families are too narrow for a permanent label.
- **Waveform:** the selected model rendered offline at its base Timbre and Color: two and a half cycles for steady models, the decay from a strike for plucked and percussive ones. It does not show modulation, FM or effects.
- **Effect plots:** drive shows a 100 Hz reference through the current settings (not a live meter); the filter curve is draggable for frequency and resonance. Off shows "bypassed".
- **Settings menu:** presets (save, open, show folder, every preset in `~/Documents/Macro OSC/Presets`, initialise); Audio/MIDI settings in the standalone app only, since a DAW owns its own; interface size (75–150%) and tooltips (both saved with the plug-in state, never in presets); the version.
- **Volume:** the output level in dB. Each voice keeps 9 dB of headroom and resonance is gain-compensated, so chords stay clear of the output limiter.
- **Voices and mode:** eight voice cells (lit = sounding, grey = available, outlined = unused) that set the poly voice count by click or drag; a poly / mono / legato switch. Mono retriggers on every key; legato keeps the envelope and glides between overlapping keys. Both use last-note priority and return to keys still held.
- **Selectors:** a hairline-outlined field with the value and a chevron; menus in the lower half open upwards so every option shows.
- **Drive power:** an LED that switches distortion off, and back on to the last type used.
- **Filter type:** a segmented off / lp / bp / hp switch.
- **Modulation:** the title and slots 1–3 share a line; loop and sync sit below, with the target field at the bottom of the column, level with the slider row. Then (a dot marks a routed slot), target menu, loop and sync LEDs, the curve editor (click to add, drag to move, right-click to remove, down to a single point for a constant level; dashed = after scale and offset; shaded boxes = how far random can move each point), and offset / scale / rate / random sliders. Rate reads in note values when synced. Random moves each point by its own amount, drawn per voice at every note and every loop cycle; on a single point it gives each note its own fixed offset.

## Do and don't

- **Do** keep every sound-shaping parameter on one page.
- **Do** keep the light-on-dark hierarchy: ink for what you touch, muted for what explains it.
- **Do** be honest about previews: the waveform and drive plots are references, not live audio.
- **Don't** add bevels, gradients, drop shadows, rounded corners or focus boxes.
- **Don't** use yellow-green outside the model readout and model map.
- **Don't** introduce knobs; parameters are sliders or direct-manipulation plots.
