// Polivoks 1982 - an independent, circuit-informed emulation of the Formanta
// Polivoks, built for Akai MPC standalone hardware and desktop VST2 hosts.
//
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// The original is duophonic: VCO I tracks the highest held key, VCO II the
// lowest, and both share one filter, one VCA and one pair of ADSRs. In 1-voice
// mode both oscillators follow the lowest key. Glide is always on VCO I; in
// mono it applies to both. The filter is a two-K140UD12 state-variable design
// (12 dB/oct LP, 6 dB/oct BP). See PolivoksDsp.h for the circuit model.
//
// Extras are deliberately off by default: velocity, pitch bend and adjustable
// key tracking. With them at their defaults the instrument behaves like the
// 1982 panel.

#include "mpc_plugin.h"

#include <cmath>
#include <cstring>

#include "PolivoksDsp.h"

class Polivoks1982 : public mpc::Plugin
{
public:
    enum Params
    {
        kVolume = 0,
        kCutoff, kResonance, kEnvAmount, kLfoToVcf, kFilterMode,
        kVco1Wave, kVco1Oct, kVco2Wave, kVco2Oct, kVco2Detune,
        kVco1Level, kVco2Level, kNoiseLevel, kExtLevel, kXMod,
        kLfoToVco1, kLfoToVco2, kLfoShape, kLfoRate, kLfoToVca,
        kFAttack, kFDecay, kFSustain, kFRelease, kFLoop,
        kAAttack, kADecay, kASustain, kARelease, kALoop,
        kGateHold, kVoiceMode, kGlide, kTune,
        kKeyTrack, kVelocity, kBendRange, kPedal,
        kNumParams
    };

    static mpc::PluginInfo makeInfo()
    {
        mpc::PluginInfo i;
        i.name = "Polivoks 1982";
        i.vendor = "bokontep";
        i.uniqueId = MPC_FOURCC('B', 'p', '4', 'd');   // must match plugin.json "uid"
        i.version = 1200;
        i.isInstrument = true;
        i.numInputs = 0;       // MPC instruments are listed with no audio inputs
        i.numOutputs = 2;
        i.tailSamples = 0;
        return i;
    }

    Polivoks1982() : mpc::Plugin(makeInfo())
    {
        using namespace mpc;

        const std::vector<std::string> waves = { "Triangle", "Saw", "Square", "Pulse 1", "Pulse 2" };
        const std::vector<std::string> octaves = { "32'", "16'", "8'", "4'", "2'" };

        addParam(floatParam("volume",      "Volume",      -40.0f, 6.0f, -10.0f, "dB", 1));
        addParam(logParam  ("cutoff",      "Cutoff",      100.0f, 10000.0f, 3000.0f, "Hz", 0));
        addParam(floatParam("resonance",   "Resonance",   0.0f, 100.0f, 0.0f, "%", 0));
        addParam(floatParam("env_amount",  "Env Amount",  0.0f, 100.0f, 0.0f, "%", 0));
        addParam(floatParam("lfo_vcf",     "LFO>VCF",     0.0f, 100.0f, 0.0f, "%", 0));
        addParam(choiceParam("filter_mode", "Filter Mode", { "LP", "BP" }, 0));

        addParam(choiceParam("vco1_wave",  "VCO1 Wave", waves, 1));
        addParam(choiceParam("vco1_oct",   "VCO1 Oct",  octaves, 2));
        addParam(choiceParam("vco2_wave",  "VCO2 Wave", waves, 1));
        addParam(choiceParam("vco2_oct",   "VCO2 Oct",  octaves, 2));
        addParam(floatParam("vco2_detune", "VCO2 Detune", -4.0f, 4.0f, 0.0f, "st", 2));
        addParam(floatParam("vco1_level",  "VCO1 Level", 0.0f, 100.0f, 60.0f, "%", 0));
        addParam(floatParam("vco2_level",  "VCO2 Level", 0.0f, 100.0f, 0.0f, "%", 0));
        addParam(floatParam("noise_level", "Noise",      0.0f, 100.0f, 0.0f, "%", 0));
        addParam(floatParam("ext_level",   "Ext Input",  0.0f, 100.0f, 0.0f, "%", 0));
        addParam(floatParam("xmod",        "X-MOD",      0.0f, 100.0f, 0.0f, "%", 0));

        addParam(floatParam("lfo_vco1",    "LFO>VCO1",   0.0f, 100.0f, 0.0f, "%", 0));
        addParam(floatParam("lfo_vco2",    "LFO>VCO2",   0.0f, 100.0f, 0.0f, "%", 0));
        addParam(choiceParam("lfo_shape",  "LFO Shape", { "Triangle", "Square", "S&H", "Noise" }, 0));
        addParam(logParam  ("lfo_rate",    "LFO Rate",   0.1f, 30.0f, 5.0f, "Hz", 2));
        addParam(floatParam("lfo_vca",     "LFO>VCA",    0.0f, 100.0f, 0.0f, "%", 0));

        addParam(logParam  ("f_attack",    "Filt Attack",  4.0f, 10000.0f, 10.0f, "ms", 0));
        addParam(logParam  ("f_decay",     "Filt Decay",   4.0f, 10000.0f, 300.0f, "ms", 0));
        addParam(floatParam("f_sustain",   "Filt Sustain", 0.0f, 100.0f, 30.0f, "%", 0));
        addParam(logParam  ("f_release",   "Filt Release", 4.0f, 10000.0f, 300.0f, "ms", 0));
        addParam(boolParam ("f_loop",      "Filt Loop", false));

        addParam(logParam  ("a_attack",    "Amp Attack",  4.0f, 10000.0f, 5.0f, "ms", 0));
        addParam(logParam  ("a_decay",     "Amp Decay",   4.0f, 10000.0f, 300.0f, "ms", 0));
        addParam(floatParam("a_sustain",   "Amp Sustain", 0.0f, 100.0f, 80.0f, "%", 0));
        addParam(logParam  ("a_release",   "Amp Release", 4.0f, 10000.0f, 300.0f, "ms", 0));
        addParam(boolParam ("a_loop",      "Amp Loop", false));

        addParam(boolParam ("gate_hold",   "Gate On", false));
        addParam(choiceParam("voice_mode", "Voice Mode", { "1 Voice", "2 Voice" }, 1));
        addParam(floatParam("glide",       "Glide", 0.0f, 2000.0f, 0.0f, "ms", 0, 0.5f));
        addParam(floatParam("tune",        "Master Tune", -2.0f, 2.0f, 0.0f, "st", 2));

        addParam(floatParam("key_track",   "Key Track", 0.0f, 100.0f, 62.0f, "%", 0));
        addParam(floatParam("velocity",    "Velocity",  0.0f, 100.0f, 0.0f, "%", 0));
        addParam(intParam  ("bend_range",  "Bend Range", 0, 12, 0, "st"));
        addParam(floatParam("pedal",       "Pedal", 0.0f, 100.0f, 0.0f, "%", 0));

        if (numParams() != kNumParams)
            mpc::log("Polivoks1982: parameter count mismatch (%d vs %d)", numParams(), (int)kNumParams);

        auto program = [this](const char* name, std::initializer_list<std::pair<int, float>> overrides)
        {
            std::vector<float> v((size_t)numParams());
            for (int i = 0; i < numParams(); ++i) v[(size_t)i] = param(i).def;
            for (const auto& o : overrides) v[(size_t)o.first] = o.second;
            addProgram(name, std::move(v));
        };

        // The only patch the factory ever printed: VCO I saw at 32', mixer 3/6,
        // filter open, amplifier sustain at maximum.
        program("Factory Test", {
            { kVco1Wave, 1 }, { kVco1Oct, 0 }, { kVco1Level, 50 },
            { kVco2Level, 0 }, { kNoiseLevel, 0 }, { kExtLevel, 0 },
            { kCutoff, 10000 }, { kResonance, 0 }, { kEnvAmount, 0 },
            { kFAttack, 4 }, { kFDecay, 300 }, { kFSustain, 100 }, { kFRelease, 4 },
            { kAAttack, 4 }, { kADecay, 300 }, { kASustain, 100 }, { kARelease, 4 },
            { kVolume, -10 }
        });

        program("Monster Bass", {
            { kVco1Wave, 1 }, { kVco1Oct, 1 }, { kVco1Level, 100 },
            { kVco2Wave, 1 }, { kVco2Oct, 1 }, { kVco2Detune, 0.12f }, { kVco2Level, 95 },
            { kCutoff, 250 }, { kResonance, 45 }, { kEnvAmount, 75 },
            { kFAttack, 4 }, { kFDecay, 180 }, { kFSustain, 0 }, { kFRelease, 150 },
            { kAAttack, 4 }, { kADecay, 350 }, { kASustain, 65 }, { kARelease, 120 },
            { kVolume, -12 }
        });

        program("Scream", {
            { kVco1Wave, 1 }, { kVco1Oct, 2 }, { kVco1Level, 80 },
            { kVco2Wave, 1 }, { kVco2Oct, 2 }, { kVco2Detune, -0.1f }, { kVco2Level, 60 },
            { kCutoff, 700 }, { kResonance, 100 }, { kEnvAmount, 25 },
            { kFAttack, 10 }, { kFDecay, 800 }, { kFSustain, 55 }, { kFRelease, 500 },
            { kAAttack, 5 }, { kADecay, 500 }, { kASustain, 80 }, { kARelease, 400 },
            { kLfoShape, 0 }, { kLfoRate, 0.25f }, { kLfoToVcf, 25 },
            { kVolume, -14 }
        });

        program("Duo Lead", {
            { kVco1Wave, 1 }, { kVco1Oct, 2 }, { kVco1Level, 85 },
            { kVco2Wave, 1 }, { kVco2Oct, 2 }, { kVco2Detune, 0.2f }, { kVco2Level, 75 },
            { kCutoff, 2200 }, { kResonance, 25 }, { kEnvAmount, 45 },
            { kFAttack, 4 }, { kFDecay, 450 }, { kFSustain, 45 }, { kFRelease, 350 },
            { kAAttack, 8 }, { kADecay, 450 }, { kASustain, 80 }, { kARelease, 300 },
            { kVoiceMode, 1 }, { kGlide, 280 }, { kVolume, -10 }
        });

        program("Trill Chord", {
            { kVco1Wave, 1 }, { kVco1Oct, 2 }, { kVco1Level, 85 },
            { kVco2Wave, 1 }, { kVco2Oct, 2 }, { kVco2Detune, 3.0f }, { kVco2Level, 75 },
            { kCutoff, 3200 }, { kResonance, 20 }, { kEnvAmount, 20 },
            { kAAttack, 4 }, { kADecay, 300 }, { kASustain, 100 }, { kARelease, 200 },
            { kLfoShape, 1 }, { kLfoRate, 5.7f }, { kLfoToVco1, 20 }, { kLfoToVco2, 20 },
            { kVolume, -10 }
        });

        program("Looping Wobble", {
            { kVco1Wave, 1 }, { kVco1Oct, 1 }, { kVco1Level, 65 },
            { kVco2Wave, 1 }, { kVco2Oct, 1 }, { kVco2Detune, 0.15f }, { kVco2Level, 55 },
            { kCutoff, 700 }, { kResonance, 55 }, { kEnvAmount, 55 },
            { kFAttack, 180 }, { kFDecay, 180 }, { kFSustain, 0 }, { kFRelease, 200 }, { kFLoop, 1 },
            { kAAttack, 5 }, { kADecay, 500 }, { kASustain, 100 }, { kARelease, 300 },
            { kVolume, -12 }
        });

        program("Steam Train", {
            { kVco1Wave, 1 }, { kVco1Oct, 0 }, { kVco1Level, 50 },
            { kVco2Level, 0 }, { kNoiseLevel, 65 },
            { kCutoff, 600 }, { kResonance, 80 }, { kEnvAmount, 20 },
            { kLfoShape, 2 }, { kLfoRate, 8.0f }, { kLfoToVcf, 60 }, { kLfoToVco1, 25 },
            { kFAttack, 120 }, { kFDecay, 180 }, { kFSustain, 0 }, { kFRelease, 200 }, { kFLoop, 1 },
            { kAAttack, 5 }, { kADecay, 500 }, { kASustain, 100 }, { kARelease, 300 },
            { kVolume, -14 }
        });

        program("FM Growl", {
            { kVco1Wave, 1 }, { kVco1Oct, 2 }, { kVco1Level, 85 },
            { kVco2Wave, 0 }, { kVco2Oct, 1 }, { kVco2Level, 50 },
            { kXMod, 55 },
            { kCutoff, 1400 }, { kResonance, 30 }, { kEnvAmount, 45 },
            { kFAttack, 4 }, { kFDecay, 300 }, { kFSustain, 30 }, { kFRelease, 250 },
            { kAAttack, 4 }, { kADecay, 300 }, { kASustain, 85 }, { kARelease, 250 },
            { kVolume, -12 }
        });

        // Eight more, covering the classic Polivoks recipes: dual-saw and
        // pulse basses, brass, cello, band-pass lead, looping filter, wind and
        // sample-and-hold beacon.
        program("Dual Saw Bass", {
            { kVco1Wave, 1 }, { kVco1Oct, 1 }, { kVco1Level, 88 },
            { kVco2Wave, 1 }, { kVco2Oct, 1 }, { kVco2Detune, 0.10f }, { kVco2Level, 84 },
            { kCutoff, 420 }, { kResonance, 30 }, { kEnvAmount, 55 },
            { kFAttack, 4 }, { kFDecay, 260 }, { kFSustain, 0 }, { kFRelease, 160 },
            { kAAttack, 4 }, { kADecay, 600 }, { kASustain, 72 }, { kARelease, 140 },
            { kVoiceMode, 0 }, { kVolume, -11 }
        });

        program("Pulse Bass", {
            { kVco1Wave, 3 }, { kVco1Oct, 0 }, { kVco1Level, 100 },
            { kVco2Wave, 4 }, { kVco2Oct, 1 }, { kVco2Detune, -0.08f }, { kVco2Level, 68 },
            { kCutoff, 330 }, { kResonance, 26 }, { kEnvAmount, 48 },
            { kFAttack, 4 }, { kFDecay, 190 }, { kFSustain, 0 }, { kFRelease, 120 },
            { kAAttack, 4 }, { kADecay, 380 }, { kASustain, 76 }, { kARelease, 110 },
            { kVoiceMode, 0 }, { kVolume, -12 }
        });

        program("Soviet Brass", {
            { kVco1Wave, 1 }, { kVco1Oct, 2 }, { kVco1Level, 82 },
            { kVco2Wave, 1 }, { kVco2Oct, 2 }, { kVco2Detune, 0.18f }, { kVco2Level, 82 },
            { kCutoff, 900 }, { kResonance, 22 }, { kEnvAmount, 52 },
            { kFAttack, 130 }, { kFDecay, 900 }, { kFSustain, 38 }, { kFRelease, 420 },
            { kAAttack, 55 }, { kADecay, 900 }, { kASustain, 86 }, { kARelease, 360 },
            { kLfoShape, 0 }, { kLfoRate, 5.2f }, { kLfoToVcf, 8 },
            { kVoiceMode, 1 }, { kVolume, -12 }
        });

        program("Cello Pad", {
            { kVco1Wave, 1 }, { kVco1Oct, 2 }, { kVco1Level, 76 },
            { kVco2Wave, 0 }, { kVco2Oct, 2 }, { kVco2Detune, 0.12f }, { kVco2Level, 54 },
            { kCutoff, 650 }, { kResonance, 18 }, { kEnvAmount, 28 },
            { kFAttack, 340 }, { kFDecay, 1500 }, { kFSustain, 44 }, { kFRelease, 900 },
            { kAAttack, 280 }, { kADecay, 1500 }, { kASustain, 86 }, { kARelease, 800 },
            { kLfoShape, 0 }, { kLfoRate, 4.6f }, { kLfoToVcf, 10 }, { kLfoToVco1, 4 },
            { kVoiceMode, 1 }, { kVolume, -13 }
        });

        program("Bandpass Lead", {
            { kVco1Wave, 2 }, { kVco1Oct, 2 }, { kVco1Level, 90 },
            { kVco2Wave, 1 }, { kVco2Oct, 2 }, { kVco2Detune, 0.15f }, { kVco2Level, 60 },
            { kCutoff, 2400 }, { kResonance, 65 }, { kEnvAmount, 40 }, { kFilterMode, 1 },
            { kFAttack, 15 }, { kFDecay, 700 }, { kFSustain, 30 }, { kFRelease, 350 },
            { kAAttack, 8 }, { kADecay, 450 }, { kASustain, 80 }, { kARelease, 300 },
            { kLfoShape, 0 }, { kLfoRate, 5.4f }, { kLfoToVca, 16 },
            { kGlide, 260 }, { kVoiceMode, 0 }, { kVolume, -12 }
        });

        program("Rhythmic Filter", {
            { kVco1Wave, 1 }, { kVco1Oct, 1 }, { kVco1Level, 80 },
            { kVco2Wave, 1 }, { kVco2Oct, 1 }, { kVco2Detune, 0.10f }, { kVco2Level, 70 },
            { kCutoff, 450 }, { kResonance, 60 }, { kEnvAmount, 75 },
            { kFAttack, 8 }, { kFDecay, 220 }, { kFSustain, 0 }, { kFRelease, 220 }, { kFLoop, 1 },
            { kAAttack, 5 }, { kADecay, 300 }, { kASustain, 100 }, { kARelease, 300 },
            { kLfoShape, 2 }, { kLfoRate, 0.4f }, { kLfoToVcf, 15 },
            { kGateHold, 1 }, { kVoiceMode, 0 }, { kVolume, -13 }
        });

        program("Wind", {
            { kVco1Level, 0 }, { kVco2Level, 0 }, { kNoiseLevel, 85 },
            { kCutoff, 900 }, { kResonance, 55 }, { kFilterMode, 1 },
            { kLfoShape, 3 }, { kLfoRate, 0.8f }, { kLfoToVcf, 60 },
            { kFAttack, 900 }, { kFDecay, 5000 }, { kFSustain, 100 }, { kFRelease, 3500 },
            { kAAttack, 1200 }, { kADecay, 5000 }, { kASustain, 100 }, { kARelease, 3500 },
            { kGateHold, 1 }, { kVoiceMode, 0 }, { kVolume, -18 }
        });

        program("Sputnik", {
            { kVco1Wave, 0 }, { kVco1Oct, 3 }, { kVco1Level, 80 },
            { kVco2Level, 0 },
            { kCutoff, 3000 }, { kResonance, 15 }, { kEnvAmount, 20 },
            { kLfoShape, 1 }, { kLfoRate, 6.5f }, { kLfoToVco1, 25 }, { kLfoToVcf, 15 },
            { kFAttack, 4 }, { kFDecay, 400 }, { kFSustain, 100 }, { kFRelease, 600 },
            { kAAttack, 4 }, { kADecay, 400 }, { kASustain, 100 }, { kARelease, 600 },
            { kGateHold, 1 }, { kVoiceMode, 0 }, { kVolume, -16 }
        });

        // The host shows program 0's name on a fresh instance; make the
        // initial parameter state match it rather than the declaration
        // defaults.
        setProgram(0);
    }

    // ----- lifecycle --------------------------------------------------------
    void prepare(double sampleRate, int maxBlockSize) override
    {
        (void)maxBlockSize;
        baseSr_ = (float)sampleRate;
        osr_ = baseSr_ * (float)plv::kOversample;

        vco1_.prepare(osr_);
        vco2_.prepare(osr_);
        filter_.prepare(osr_);
        lfo_.prepare(osr_);
        filtEnv_.prepare(osr_);
        ampEnv_.prepare(osr_);
        dc_.prepare(baseSr_);

        dec1_.setCoeffs(mpc::dsp::BiquadCoeffs::lowpass(osr_, 19000.0f, 0.5411961f));
        dec2_.setCoeffs(mpc::dsp::BiquadCoeffs::lowpass(osr_, 19000.0f, 1.3065630f));

        cutoffCoeff_ = 1.0f - std::exp(-1.0f / (0.0010f * osr_));
        bendCoeff_   = 1.0f - std::exp(-1.0f / (0.0050f * osr_));
        volCoeff_    = 1.0f - std::exp(-1.0f / (0.0200f * baseSr_));

        readSettings();
        applySettings();
        reset();
    }

    void reset() override
    {
        filtEnv_.reset();
        ampEnv_.reset();
        vco1_.reset(0.0f);
        vco2_.reset(0.37f);
        filter_.reset();
        dec1_.reset();
        dec2_.reset();
        dc_.reset();
        lfo_.reset();
        noise_.reseed(0x1234567u);
        filterSeed_.reseed(0x9e3779b9u);

        std::memset(held_, 0, sizeof(held_));
        heldCount_ = 0;
        lastHigh_ = lastLow_ = 60;
        target1_ = target2_ = note1_ = note2_ = 60.0f;
        hasPlayed_ = false;
        gate_ = false;
        lastVel_ = 127;
        velGain_ = 1.0f;
        bend_ = bendTarget_ = 0.0f;
        ctrl_ = 0;

        cutoffSmoothed_ = mpc::dsp::clamp(s_.cutoff, 20.0f, 18000.0f);
        vol_ = volTarget_;
        filter_.setMode((float)s_.filterMode);
        filter_.set(cutoffSmoothed_, s_.resonance);

        lastGateHold_ = getBool(kGateHold);
        lastVoiceMode_ = getInt(kVoiceMode);
    }

    // ----- audio ------------------------------------------------------------
    void process(mpc::ProcessContext& ctx) override
    {
        readSettings();
        applySettings();

        int pos = 0;
        for (int e = 0; e < ctx.numMidiEvents; ++e)
        {
            const mpc::MidiEvent& ev = ctx.midi[e];
            const int at = mpc::dsp::clamp(ev.sampleOffset, 0, ctx.numFrames);
            if (at > pos) { render(ctx, pos, at); pos = at; }
            handleMidi(ev);
        }
        render(ctx, pos, ctx.numFrames);
    }

private:
    static constexpr int kCtrlInterval = 16;   // oversampled samples between pitch/filter updates

    struct Settings
    {
        int vco1Wave = 1, vco2Wave = 1, vco1Oct = 2, vco2Oct = 2;
        int filterMode = 0, lfoShape = 0, voiceMode = 1, bendRange = 0;
        float cutoff = 3000.0f, resonance = 0.0f, envAmount = 0.0f, lfoToVcf = 0.0f;
        float vco2Detune = 0.0f, vco1Level = 0.6f, vco2Level = 0.0f;
        float noiseLevel = 0.0f, extLevel = 0.0f, xmod = 0.0f;
        float lfoToVco1 = 0.0f, lfoToVco2 = 0.0f, lfoRate = 5.0f, lfoToVca = 0.0f;
        float fAttack = 10.0f, fDecay = 300.0f, fSustain = 0.3f, fRelease = 300.0f;
        float aAttack = 5.0f, aDecay = 300.0f, aSustain = 0.8f, aRelease = 300.0f;
        bool fLoop = false, aLoop = false, gateHold = false;
        float glide = 0.0f, tune = 0.0f, keyTrack = 0.62f, velocity = 0.0f, volumeDb = -10.0f, pedal = 0.0f;

        float vco1OctSemi = 0.0f, vco2OctSemi = 0.0f;
        float xmodOct = 0.0f, lfoVco1Semis = 0.0f, lfoVco2Semis = 0.0f, lfoVcfOct = 0.0f;
        float envAmountOct = 0.0f, cutoffOctBase = 4.9068906f;
        float velocityAmt = 0.0f;
    } s_;

    void readSettings()
    {
        s_.vco1Wave = getInt(kVco1Wave);
        s_.vco1Oct = getInt(kVco1Oct);
        s_.vco2Wave = getInt(kVco2Wave);
        s_.vco2Oct = getInt(kVco2Oct);
        s_.filterMode = getInt(kFilterMode);
        s_.lfoShape = getInt(kLfoShape);
        s_.voiceMode = getInt(kVoiceMode);
        s_.bendRange = getInt(kBendRange);

        s_.cutoff = get(kCutoff);
        s_.resonance = get(kResonance) * 0.01f;
        s_.envAmount = get(kEnvAmount) * 0.01f;
        s_.lfoToVcf = get(kLfoToVcf) * 0.01f;
        s_.vco2Detune = get(kVco2Detune);
        s_.vco1Level = get(kVco1Level) * 0.01f;
        s_.vco2Level = get(kVco2Level) * 0.01f;
        s_.noiseLevel = get(kNoiseLevel) * 0.01f;
        s_.extLevel = get(kExtLevel) * 0.01f;
        s_.xmod = get(kXMod) * 0.01f;
        s_.lfoToVco1 = get(kLfoToVco1) * 0.01f;
        s_.lfoToVco2 = get(kLfoToVco2) * 0.01f;
        s_.lfoRate = get(kLfoRate);
        s_.lfoToVca = get(kLfoToVca) * 0.01f;

        s_.fAttack = get(kFAttack);
        s_.fDecay = get(kFDecay);
        s_.fSustain = get(kFSustain) * 0.01f;
        s_.fRelease = get(kFRelease);
        s_.fLoop = getBool(kFLoop);
        s_.aAttack = get(kAAttack);
        s_.aDecay = get(kADecay);
        s_.aSustain = get(kASustain) * 0.01f;
        s_.aRelease = get(kARelease);
        s_.aLoop = getBool(kALoop);

        s_.gateHold = getBool(kGateHold);
        s_.glide = get(kGlide);
        s_.tune = get(kTune);
        s_.keyTrack = get(kKeyTrack) * 0.01f;
        s_.velocity = get(kVelocity) * 0.01f;
        s_.volumeDb = get(kVolume);
        s_.pedal = get(kPedal) * 0.01f;

        s_.vco1OctSemi = (float)(s_.vco1Oct - 2) * 12.0f;
        s_.vco2OctSemi = (float)(s_.vco2Oct - 2) * 12.0f;
        s_.xmodOct = s_.xmod * 5.0f;
        s_.lfoVco1Semis = s_.lfoToVco1 * 12.0f;
        s_.lfoVco2Semis = s_.lfoToVco2 * 12.0f;
        s_.lfoVcfOct = s_.lfoToVcf * 5.0f;
        s_.envAmountOct = s_.envAmount * 6.64f;
        s_.cutoffOctBase = std::log2(mpc::dsp::clamp(s_.cutoff, 100.0f, 10000.0f) * 0.01f);
        s_.velocityAmt = s_.velocity;
    }

    void applySettings()
    {
        vco1_.setWave(s_.vco1Wave);
        vco2_.setWave(s_.vco2Wave);
        lfo_.setRate(s_.lfoRate);
        lfo_.setShape(s_.lfoShape);
        filter_.setMode((float)s_.filterMode);

        filtEnv_.set(s_.fAttack, s_.fDecay, s_.fSustain, s_.fRelease, s_.fLoop);
        ampEnv_.set(s_.aAttack, s_.aDecay, s_.aSustain, s_.aRelease, s_.aLoop);

        if (s_.glide <= 0.5f)
        {
            glideCoeff_ = 1.0f;
        }
        else
        {
            const float tauMs = s_.glide * (1.0f / 3.0f);
            glideCoeff_ = 1.0f - std::exp(-1.0f / (tauMs * 0.001f * osr_));
        }
        note2Coeff_ = (s_.voiceMode == 1) ? 1.0f : glideCoeff_;

        volTarget_ = mpc::dsp::dbToGain(s_.volumeDb);
        velGain_ = mpc::dsp::lerp(1.0f, (float)lastVel_ * (1.0f / 127.0f), s_.velocityAmt);
        if (s_.bendRange == 0) bendTarget_ = 0.0f;

        // Parameter changes that act like panel switches.
        if (s_.gateHold != lastGateHold_)
        {
            lastGateHold_ = s_.gateHold;
            if (s_.gateHold)
            {
                if (!gate_) { gate_ = true; triggerEnvelopes(); }
            }
            else if (heldCount_ == 0)
            {
                gate_ = false;
                releaseEnvelopes();
            }
        }

        if (s_.voiceMode != lastVoiceMode_)
        {
            lastVoiceMode_ = s_.voiceMode;
            updateTargets();
        }
    }

    // ----- note handling ----------------------------------------------------
    void updateTargets()
    {
        if (heldCount_ > 0)
        {
            int hi = -1, lo = 128;
            for (int n = 0; n < 128; ++n)
            {
                if (!held_[n]) continue;
                if (n > hi) hi = n;
                if (n < lo) lo = n;
            }
            if (hi >= 0) { lastHigh_ = hi; lastLow_ = lo; }
        }

        if (s_.voiceMode == 0)
        {
            target1_ = (float)lastLow_;
            target2_ = (float)lastLow_;
        }
        else
        {
            target1_ = (float)lastHigh_;
            target2_ = (float)lastLow_;
        }

        if (!hasPlayed_)
        {
            note1_ = target1_;
            note2_ = target2_;
        }
    }

    void triggerEnvelopes() { filtEnv_.gateOn(); ampEnv_.gateOn(); }
    void releaseEnvelopes() { filtEnv_.gateOff(); ampEnv_.gateOff(); }

    void handleMidi(const mpc::MidiEvent& ev)
    {
        if (ev.isNoteOn())
        {
            const int n = mpc::dsp::clamp(ev.note(), 0, 127);
            if (!held_[n]) { held_[n] = true; ++heldCount_; }
            lastVel_ = mpc::dsp::clamp(ev.velocity(), 1, 127);
            velGain_ = mpc::dsp::lerp(1.0f, (float)lastVel_ * (1.0f / 127.0f), s_.velocityAmt);
            updateTargets();
            hasPlayed_ = true;
            if (!gate_) { gate_ = true; triggerEnvelopes(); }
        }
        else if (ev.isNoteOff())
        {
            const int n = mpc::dsp::clamp(ev.note(), 0, 127);
            if (held_[n]) { held_[n] = false; --heldCount_; }
            updateTargets();
            if (heldCount_ == 0 && !s_.gateHold)
            {
                gate_ = false;
                releaseEnvelopes();
            }
        }
        else if (ev.isAllNotesOff())
        {
            std::memset(held_, 0, sizeof(held_));
            heldCount_ = 0;
            updateTargets();
            gate_ = false;
            releaseEnvelopes();
        }
        else if (ev.isPitchBend())
        {
            bendTarget_ = ((float)ev.pitchBend() * (1.0f / 8192.0f)) * (float)s_.bendRange;
        }
    }

    // ----- rendering --------------------------------------------------------
    void render(mpc::ProcessContext& ctx, int from, int to)
    {
        float* outL = ctx.outputs[0];
        float* outR = (ctx.numOutputs > 1) ? ctx.outputs[1] : nullptr;

        for (int i = from; i < to; ++i)
        {
            float ext = 0.0f;
            if (ctx.numInputs > 0 && ctx.inputs != nullptr && ctx.inputs[0] != nullptr)
            {
                ext = ctx.inputs[0][i];
                if (ctx.numInputs > 1 && ctx.inputs[1] != nullptr)
                    ext = 0.5f * (ext + ctx.inputs[1][i]);
                ext = mpc::dsp::clamp(ext, -4.0f, 4.0f);
            }

            const float out = renderSample(ext);
            outL[i] = out;
            if (outR) outR[i] = out;
        }
    }

    float renderSample(float ext)
    {
        float decimated = 0.0f;

        for (int k = 0; k < plv::kOversample; ++k)
        {
            float lfoSigned = 0.0f, lfoUnipolar = 0.0f;
            lfo_.process(lfoSigned, lfoUnipolar);

            const float fenv = filtEnv_.process();
            const float aenv = ampEnv_.process();

            // Glide on the note number (linear in pitch), not on Hz.
            note1_ += glideCoeff_ * (target1_ - note1_);
            note2_ += note2Coeff_ * (target2_ - note2_);
            bend_ += bendCoeff_ * (bendTarget_ - bend_);

            if (ctrl_ == 0)
            {
                const float pitch1 = note1_ + bend_ + s_.tune + s_.vco1OctSemi + lfoSigned * s_.lfoVco1Semis;
                const float pitch2 = note2_ + bend_ + s_.tune + s_.vco2OctSemi + s_.vco2Detune
                                   + lfoSigned * s_.lfoVco2Semis;
                vco1BaseHz_ = plv::midiToHzFast(pitch1);
                vco2BaseHz_ = plv::midiToHzFast(pitch2);

                const float keyOct = s_.keyTrack * (note1_ - 60.0f) * (1.0f / 12.0f);
                const float modOct = keyOct + s_.envAmountOct * fenv + s_.lfoVcfOct * lfoSigned + s_.pedal * 5.0f;
                const float cutoffHz = mpc::dsp::clamp(100.0f * plv::fastExp2(s_.cutoffOctBase + modOct), 20.0f, 18000.0f);
                cutoffSmoothed_ += cutoffCoeff_ * (cutoffHz - cutoffSmoothed_);
                filter_.set(cutoffSmoothed_, s_.resonance);
            }
            ctrl_++;
            if (ctrl_ >= kCtrlInterval) ctrl_ = 0;

            vco2_.setFrequency(vco2BaseHz_);
            const float v2 = vco2_.process();

            float f1 = vco1BaseHz_;
            if (s_.xmodOct > 0.0f) f1 *= plv::fastExp2(s_.xmodOct * v2);
            vco1_.setFrequency(f1);
            const float v1 = vco1_.process();

            float mix = v1 * s_.vco1Level
                      + v2 * s_.vco2Level
                      + noise_.nextBipolar() * s_.noiseLevel
                      + ext * s_.extLevel;
            mix += filterSeed_.nextBipolar() * 2.0e-5f;   // analogue noise floor; seeds self-oscillation

            const float filtered = filter_.process(mix);

            const float lfoGain = 1.0f - s_.lfoToVca * (1.0f - lfoUnipolar);
            float vca = filtered * (aenv * lfoGain * velGain_);
            vca = mpc::dsp::softClip(vca * 1.1f);

            decimated = dec2_.process(dec1_.process(vca));
        }

        float y = dc_.process(decimated);
        vol_ += volCoeff_ * (volTarget_ - vol_);
        y *= vol_;
        return mpc::dsp::flushDenormal(mpc::dsp::softClip(y));
    }

    // ----- DSP state --------------------------------------------------------
    float baseSr_ = 44100.0f;
    float osr_ = 176400.0f;

    plv::PolivoksVco vco1_, vco2_;
    plv::PolivoksFilter filter_;
    plv::PolivoksLfo lfo_;
    plv::PolivoksEnvelope filtEnv_, ampEnv_;
    mpc::dsp::Biquad dec1_, dec2_;
    plv::DcBlocker dc_;

    mpc::dsp::Xorshift32 noise_{0x1234567u};
    mpc::dsp::Xorshift32 filterSeed_{0x9e3779b9u};

    bool held_[128] = {};
    int heldCount_ = 0;
    int lastHigh_ = 60, lastLow_ = 60;
    float target1_ = 60.0f, target2_ = 60.0f;
    float note1_ = 60.0f, note2_ = 60.0f;
    float vco1BaseHz_ = 261.6256f, vco2BaseHz_ = 261.6256f;
    bool hasPlayed_ = false;
    bool gate_ = false;
    bool lastGateHold_ = false;
    int lastVoiceMode_ = 1;
    int lastVel_ = 127;
    float velGain_ = 1.0f;
    float bend_ = 0.0f, bendTarget_ = 0.0f;

    float glideCoeff_ = 1.0f, note2Coeff_ = 1.0f;
    float bendCoeff_ = 0.01f;
    float cutoffCoeff_ = 0.006f;
    float cutoffSmoothed_ = 3000.0f;
    float vol_ = 0.316f, volTarget_ = 0.316f, volCoeff_ = 0.001f;
    int ctrl_ = 0;
};

MPC_PLUGIN_ENTRY(Polivoks1982)
