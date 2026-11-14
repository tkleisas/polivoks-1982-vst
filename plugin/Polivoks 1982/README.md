# Polivoks 1982

An independent, circuit-informed emulation of the 1982 Formanta Polivoks, by
**bokontep**. It runs as an Akai MPC standalone instrument (MPC OS 3.10.1+,
External Plugin Support) and as a desktop **VST2** instrument.

“Polivoks” / “Поливокс” is the name of the original instrument and is used here
descriptively. This project is not affiliated with Formanta, the Vector
enterprise, Akai Professional, inMusic, or the original designers. The plugin
code is **GPL-3.0-or-later** licensed. The MPC Plugin SDK used to
build it is MIT-licensed; see `LICENSES/MPC-Plugin-SDK-MIT.txt` in the repository.

## What it is

The original is **duophonic**, not polyphonic: two oscillators share one
filter, one VCA and one pair of envelopes. This plugin follows that
architecture.

- **2-voice mode:** VCO I takes the highest held key, VCO II the lowest. One
  filter, VCA and envelope pair process both. Glide always acts on VCO I; VCO II
  jumps to its note.
- **1-voice mode:** both VCOs follow the lowest held key; glide acts on both.
- **VCOs:** free-running (no phase reset on note-on), five waveforms (triangle,
  saw, 45% square, 20% pulse, 8.8% pulse), footages 32'–2', master tune ±2
  semitones (±12%), VCO II detune ±4 semitones (±25%), exponential X-MOD from
  VCO II into VCO I. The triangle uses the measured 52% rise and a small peak
  spike. Pulse widths use PolyBLEP edges.
- **Mixer:** unity-gain VCO I, VCO II, noise and external-input levels. The sum
  is deliberately unbuffered: high levels overload the filter input, as on the
  original.
- **Filter:** a four-times-oversampled model of the two-K140UD12 state-variable
  filter. 12 dB/oct low-pass and 6 dB/oct band-pass, cutoff 100 Hz–10 kHz,
  exponential cutoff CV, fixed 0.62 keyboard tracking, saturating integrators
  (resonance compression), input overload, a low-cutoff output-stage step, and
  self-oscillation that tracks the cutoff knob.
- **Envelopes:** separate filter and amplifier ADSRs with exponential segments.
  Each has the original **periodic** mode: with sustain at zero it loops A+D
  while the gate is held, restarting on the key. The amplifier has the Gate On
  (drone) switch.
- **LFO:** one global modulator with triangle (bipolar), positive square,
  stepped sample-and-hold and low-frequency noise; depth controls for VCO I,
  VCO II, VCF and VCA. The square is positive-only, so pitch trills go upward.
- **Master:** tune, glide, volume, voice mode, output AC coupling at 10 Hz.

With the **extras** at their defaults, the instrument behaves like the 1982
panel. The extras page adds:

| Extra | Default | Behaviour |
|---|---|---|
| Key Track | 62% | The original fixed value; adjustable for calibration/DAW use. |
| Velocity | 0% | Off = no velocity response, as on the original. |
| Bend Range | 0 st | Off = pitch bend ignored, as on the original. |
| Pedal | 0% | Approximates the external filter pedal; 100% opens the cutoff by five octaves. |

## MPC pages

The MPC skin is generated from `ui.json` (five function-key tabs):

| Page | Controls |
|---|---|
| **VOICE** | Cutoff, Resonance, Env Amount, Volume; VCO I wave/octave/level; VCO II wave/octave/detune/level; Noise, Ext Input, X-MOD, Voice Mode, Glide |
| **FILTER** | Cutoff, Resonance, Env Amount, LFO>VCF; filter ADSR; LP/BP; envelope loop |
| **AMP** | Amp ADSR; LFO>VCA; Gate On; Amp Loop; Glide, Volume |
| **LFO** | Shape, Rate, LFO>VCO I, LFO>VCO II, LFO>VCF, LFO>VCA |
| **MORE** | Master Tune, Key Track, Velocity, Bend Range, Pedal |

The desktop VST2 has **no custom editor**: the host shows its generic parameter
list. The MPC touch UI is the JSON skin in the plugin folder.

## Programs

1. **Factory Test** — the only patch printed in the 1982 owner's manual: VCO I
   saw at 32', mixer 3/6, filter open, amplifier sustain at maximum.
2. **Monster Bass** — two detuned 16' saws, hot mixer, low cutoff and a fast
   filter envelope.
3. **Scream** — high resonance with a slow filter LFO.
4. **Duo Lead** — 2-voice, portamento on VCO I, detuned VCO II.
5. **Trill Chord** — positive square LFO on both VCOs, VCO II a minor third up.
6. **Looping Wobble** — periodic filter envelope, sustain at zero.
7. **Steam Train** — noise, sample-and-hold filter modulation and looping
   envelopes.
8. **FM Growl** — VCO II triangle cross-modulating VCO I.

## Build

From this repository's root, run `./build.sh` (it downloads the pinned MPC
Plugin SDK and produces both release assets). The equivalent manual steps from
the MPC Plugin SDK root are:

```bash
# MPC folder (native test build + ARM32 + ARM64 + skins + validation)
python3 tools/build.py "plugins/Polivoks 1982"

# Desktop VST2 for the computer running the build
python3 tools/build.py "plugins/Polivoks 1982" --target desktop
```

If Zig is not on `PATH`, the SDK fetches it on first build, or pass
`--zig /path/to/zig`. The MPC output is:

```
plugins/Polivoks 1982/build/bokontep - VST - Polivoks 1982/
```

### MPC installation

1. Copy the whole `bokontep - VST - Polivoks 1982` folder into a folder called
   `VST2 Plugins` at the root of a USB drive or SD card.
2. Put the drive in the MPC (MPC OS 3.10.1 or later).
3. Preferences → Enhancements (Beta): enable **Enable Enhancements** and
   **External Plugin Support**.
4. Add the instrument from **External Plugins** and restart the MPC if the skin
   changed.

### Desktop installation

The `--target desktop` output is `build/desktop/Polivoks 1982/`. On Linux copy
that folder to `~/.vst` (or the host's VST2 folder) and rescan. On macOS the SDK
produces a `.vst` bundle; on Windows a 64-bit `.dll` with `plugin.json` beside
it. The desktop build must be done on the target OS. The SDK produces VST2, not
VST3; some current DAWs no longer load VST2.

## Verification and calibration

`host_sim` pass: 19 checks passed, 0 warnings, 0 failures (39 parameters, 8
programs, state round-trip, random parameter sweep, 32/512-frame blocks,
all-notes-off).

The ARM32 and ARM64 builds validate, and the desktop VST2 loads and renders.
The following were measured from rendered audio during development:

| Check | Result |
|---|---|
| Pulse duties | 45%, 20%, 8.8% |
| Triangle | 52% rise, rounded/peaked top |
| Self-oscillation vs cutoff | 100/440/1760/5000 Hz tracked within 0.2% |
| Fixed key tracking (62%) at cutoff 200 Hz | note 48 → 130.0 Hz, note 60 → 200.0 Hz, note 72 → 307.5 Hz |
| Envelope loop, A=D=100 ms | 5.00 Hz (200 ms), filter loop 5.01 Hz |
| Positive square LFO, 20% depth | 260.9 ↔ 300.0 Hz (≈+2.4 semitones) |
| 1-voice / 2-voice duophony | lowest-note tracking / highest + lowest split verified |
| Glide, 280 ms, 2-voice | VCO I 130.8 → 260.9 Hz; VCO II instant |
| Host-sim CPU, 128-frame block | ≈0.65% average on the development desktop |

The CPU figure is from a desktop build. The SDK's rule of thumb is that a
32-bit MPC is roughly 15–25× slower, so plan for roughly 10–20% of one core
with the four-times-oversampled filter. Test with your MPC's CPU meter and use
one instance if it is tight.

## Sources and accuracy

The model was built from public circuit material, not from another plugin:

- Original Formanta board scans: VCO (U2/U3), VCF (U4), VCA/EG (U5), LFO (U7),
  keyboard/backplane, owner's manual. The scans are not redistributed here;
  public archive: <https://archive.org/details/synthmanual-polivoks-schematics>
  and <https://archive.org/details/synthmanual-polivoks-owners-manual>.
- Marc Bareille's redrawn Polivoks VCF (the clean two-uA776/K140UD12 reference):
  <http://m.bareille.free.fr/modular1/vcf_polivoks/vcf_polivoks.htm>.
- K140UD12 programmable-op-amp data (unity-gain frequency proportional to Iset,
  internal compensation): <https://cabintechglobal.com/pdf/ALFA_RPAR_K140YD1201.pdf>.
- Sound On Sound, “Formanta Polivoks Synthesizer” (architecture, filter,
  duophony, gate switch): <https://www.soundonsound.com/reviews/formanta-polivoks-synthesizer>.
- Monosounds' public Polivoks/Duovox journal (measured pulse widths, triangle
  asymmetry, filter key tracking and calibration approach):
  <https://monosounds.studio/polyvox/history> and
  <https://monosounds.studio/polyvox/why-duovox>.

No code was copied from another emulation. The filter, oscillators and envelopes
are original DSP written for this project.

### Honest limitations

- **No real Polivoks was available.** The model is calibrated to the public
  schematics and published measurements. Unit-to-unit tolerances, exact
  K140UD12 nonlinearities and the original's precise envelope/LFO laws are not
  captured.
- The K140UD12 is modelled behaviourally: saturating integrators, input
  overload, low-cutoff asymmetry and a compensated self-oscillation. It is not
  a transistor-level simulation.
- Envelope times are a 4 ms–10 s logarithmic control fitted to the published
  loop-period measurements; the original panel's exact taper is approximated.
- LFO rate (0.1–30 Hz) and modulation depths are reasonable ranges fitted to
  the published behavior, not factory specifications.
- The external pedal is simplified to a direct cutoff offset. The original has
  a pedal-amount knob plus an external CV.
- This build advertises **0 audio inputs** so the MPC lists it as an instrument;
  the original EXT input path is therefore inactive, and its mixer control is
  hidden from the MPC skin. The parameter remains for state compatibility.
- There is no poly mode, pitch bend or velocity response unless the extras are
  turned up. There is no output-mute/headphone/VU section, no oscillator drift
  model (the original is heater-stabilised), and no 1985 filter-revision switch.
- X-MOD is exponential and uses VCO II's selected waveform; the exact original
  injection point is inferred from the VCO schematic.
- The desktop VST2 has no graphical editor. A custom desktop GUI would be a
  separate wrapper/UI project; the MPC skin is the supported UI here.

If you can provide dry recordings of a real unit (individual waveforms at known
footages, filter sweeps at known settings, self-oscillation, envelope loop
periods, LFO rates), the remaining estimates can be tightened without changing
the architecture.
