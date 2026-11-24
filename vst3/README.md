# Polivoks 1982 — desktop VST3

A VST3 instrument for Linux and Windows, built with JUCE 7.0.12. It is the
desktop counterpart of the MPC/VST2 plugin and shares the same circuit-informed
DSP engine, parameters and 16 built-in programs.

The difference is the editor: instead of the host's generic parameter list, this
build carries the MPC panel into the DAW. The four-page **VCO / FILTER / AMP /
LFO** layout uses the same raised black plates, Cyrillic section lettering,
film-strip knobs and white paddle switches as the MPC skin.

## Features

- Duophonic/one-voice Polivoks engine: two free-running VCOs, five waveforms,
  K140UD12-style 4x-oversampled state-variable filter, looping filter/amp
  envelopes, four-shape LFO, X-MOD, glide, pitch bend and the 1982 keyboard
  tracking/velocity/pedal extras.
- Four-page custom editor with live values and mouse editing.
- Right-click a waveform/octave/LFO-shape knob for a choice menu.
- 16 factory programs in the host preset menu:
  Factory Test, Monster Bass, Scream, Duo Lead, Trill Chord, Looping Wobble,
  Steam Train, FM Growl, Dual Saw Bass, Pulse Bass, Soviet Brass, Cello Pad,
  Bandpass Lead, Rhythmic Filter, Wind, Sputnik.
- Resizable editor with a fixed 1280:684 aspect ratio.
- A Standalone target is built alongside the VST3 for quick testing; the
  release package contains the VST3.

## Install

**Linux:** copy `Polivoks 1982.vst3` into `~/.vst3/`.

**Windows:** copy `Polivoks 1982.vst3` into
`C:\Program Files\Common Files\VST3\`.

Then rescan plugins in the DAW. The plugin is an instrument and accepts MIDI;
it has no audio input.

## Build

Requirements: CMake 3.22+, a C++17 compiler, and the platform JUCE dependencies
(X11/freetype/ALSA on Linux). JUCE 7.0.12 is fetched automatically by CMake on
the first build; pass `-DJUCE_DIR=/path/to/JUCE` to use a local checkout.

Linux/macOS:

```bash
vst3/build.sh
```

Windows (PowerShell, Visual Studio 2022):

```powershell
vst3\build.ps1
```

The VST3 bundle is written to:

```
vst3/build/Polivoks1982_artefacts/Release/VST3/Polivoks 1982.vst3
```

Package a release zip with:

```bash
python3 vst3/package.py --version 1.2.0
```

## How it is put together

- `Source/PluginProcessor.*` — JUCE parameter tree, programs, MIDI/duophony and
  the audio engine.
- `Source/PluginEditor.*` — the panel, film-strip knobs, paddle switches and
  page/preset bar.
- `Source/PolivoksParams.h` — the 39 parameters, in the same order and with the
  same ranges/defaults as the MPC/VST2 build.
- `Source/dsp/` — the three small MIT DSP helpers from the MPC Plugin SDK
  (`math.h`, `biquad.h`, `noise.h`); the Polivoks-specific DSP is the shared
  `../plugin/Polivoks 1982/src/PolivoksDsp.h`.
- The background/knob/switch PNGs and `ui.json` are embedded from the MPC skin
  at build time, so there is one source of visual truth.

The MPC/VST2 plugin remains the hardware target. This VST3 is for desktop DAWs.

## Licence

SPDX-License-Identifier: GPL-3.0-or-later. Copyright (C) 2026 bokontep.

- JUCE 7.0.12 — GPLv3 (or commercial); this project uses the GPLv3 option.
- VST3 SDK bundled with JUCE — Steinberg dual licence, used under GPLv3.
- The three DSP helper headers in `Source/dsp/` are MIT, from the inMusic MPC
  Plugin SDK.
- “VST” is a trademark of Steinberg Media Technologies GmbH. “Polivoks” is used
  descriptively; this project is not affiliated with Formanta, Vector, Akai
  Professional, inMusic, or the original designers.
