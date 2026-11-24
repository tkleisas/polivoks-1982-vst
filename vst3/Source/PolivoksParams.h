// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// Shared parameter table for the VST3 build. The order and plain-value
// semantics match the MPC/VST2 plugin exactly, so the two targets sound the
// same and the same presets can be copied between them.

#pragma once

#include <cstring>

namespace polivoks {

enum ParamIndex : int
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

enum class ParamKind { Float, Choice, Bool };

inline constexpr const char* kWaveNames[] = { "Triangle", "Saw", "Square", "Pulse 1", "Pulse 2" };
inline constexpr const char* kOctaveNames[] = { "32'", "16'", "8'", "4'", "2'" };
inline constexpr const char* kFilterNames[] = { "LP", "BP" };
inline constexpr const char* kLfoShapeNames[] = { "Triangle", "Square", "S&H", "Noise" };
inline constexpr const char* kVoiceNames[] = { "1 Voice", "2 Voice" };

struct ParamSpec
{
    const char* id;
    const char* name;
    ParamKind kind;
    float min;
    float max;
    float def;
    bool logarithmic;
    float interval;
    const char* const* choices;
    int numChoices;
    bool bipolar;
};

inline constexpr ParamSpec kParams[] =
{
    { "volume",      "Volume",      ParamKind::Float, -40.0f, 6.0f,   -10.0f, false, 0.1f, nullptr, 0, false },
    { "cutoff",      "Cutoff",      ParamKind::Float, 100.0f, 10000.0f, 3000.0f, true, 0.0f, nullptr, 0, false },
    { "resonance",   "Resonance",   ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },
    { "env_amount",  "Env Amount",  ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },
    { "lfo_vcf",     "LFO>VCF",     ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },
    { "filter_mode", "Filter Mode", ParamKind::Choice, 0.0f, 1.0f,    0.0f, false, 1.0f, kFilterNames, 2, false },

    { "vco1_wave",   "VCO1 Wave",   ParamKind::Choice, 0.0f, 4.0f,    1.0f, false, 1.0f, kWaveNames, 5, false },
    { "vco1_oct",    "VCO1 Oct",    ParamKind::Choice, 0.0f, 4.0f,    2.0f, false, 1.0f, kOctaveNames, 5, false },
    { "vco2_wave",   "VCO2 Wave",   ParamKind::Choice, 0.0f, 4.0f,    1.0f, false, 1.0f, kWaveNames, 5, false },
    { "vco2_oct",    "VCO2 Oct",    ParamKind::Choice, 0.0f, 4.0f,    2.0f, false, 1.0f, kOctaveNames, 5, false },
    { "vco2_detune", "VCO2 Detune", ParamKind::Float, -4.0f, 4.0f,    0.0f, false, 0.01f, nullptr, 0, true },
    { "vco1_level",  "VCO1 Level",  ParamKind::Float, 0.0f, 100.0f,  60.0f, false, 1.0f, nullptr, 0, false },
    { "vco2_level",  "VCO2 Level",  ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },
    { "noise_level", "Noise",       ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },
    { "ext_level",   "Ext Input",   ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },
    { "xmod",        "X-MOD",       ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },

    { "lfo_vco1",    "LFO>VCO1",    ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },
    { "lfo_vco2",    "LFO>VCO2",    ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },
    { "lfo_shape",   "LFO Shape",   ParamKind::Choice, 0.0f, 3.0f,    0.0f, false, 1.0f, kLfoShapeNames, 4, false },
    { "lfo_rate",    "LFO Rate",    ParamKind::Float, 0.1f, 30.0f,    5.0f, true, 0.01f, nullptr, 0, false },
    { "lfo_vca",     "LFO>VCA",     ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },

    { "f_attack",    "Filt Attack", ParamKind::Float, 4.0f, 10000.0f, 10.0f, true, 0.0f, nullptr, 0, false },
    { "f_decay",     "Filt Decay",  ParamKind::Float, 4.0f, 10000.0f, 300.0f, true, 0.0f, nullptr, 0, false },
    { "f_sustain",   "Filt Sustain",ParamKind::Float, 0.0f, 100.0f,  30.0f, false, 1.0f, nullptr, 0, false },
    { "f_release",   "Filt Release",ParamKind::Float, 4.0f, 10000.0f, 300.0f, true, 0.0f, nullptr, 0, false },
    { "f_loop",      "Filt Loop",   ParamKind::Bool,  0.0f, 1.0f,    0.0f, false, 1.0f, nullptr, 0, false },

    { "a_attack",    "Amp Attack",  ParamKind::Float, 4.0f, 10000.0f, 5.0f, true, 0.0f, nullptr, 0, false },
    { "a_decay",     "Amp Decay",   ParamKind::Float, 4.0f, 10000.0f, 300.0f, true, 0.0f, nullptr, 0, false },
    { "a_sustain",   "Amp Sustain", ParamKind::Float, 0.0f, 100.0f,  80.0f, false, 1.0f, nullptr, 0, false },
    { "a_release",   "Amp Release", ParamKind::Float, 4.0f, 10000.0f, 300.0f, true, 0.0f, nullptr, 0, false },
    { "a_loop",      "Amp Loop",    ParamKind::Bool,  0.0f, 1.0f,    0.0f, false, 1.0f, nullptr, 0, false },

    { "gate_hold",   "Gate On",     ParamKind::Bool,  0.0f, 1.0f,    0.0f, false, 1.0f, nullptr, 0, false },
    { "voice_mode",  "Voice Mode",  ParamKind::Choice, 0.0f, 1.0f,    1.0f, false, 1.0f, kVoiceNames, 2, false },
    { "glide",       "Glide",       ParamKind::Float, 0.0f, 2000.0f, 0.0f, false, 0.5f, nullptr, 0, false },
    { "tune",        "Master Tune", ParamKind::Float, -2.0f, 2.0f,    0.0f, false, 0.01f, nullptr, 0, true },

    { "key_track",   "Key Track",   ParamKind::Float, 0.0f, 100.0f,  62.0f, false, 1.0f, nullptr, 0, false },
    { "velocity",    "Velocity",    ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },
    { "bend_range",  "Bend Range",  ParamKind::Float, 0.0f, 12.0f,    0.0f, false, 1.0f, nullptr, 0, false },
    { "pedal",       "Pedal",       ParamKind::Float, 0.0f, 100.0f,   0.0f, false, 1.0f, nullptr, 0, false },
};

static_assert(sizeof(kParams) / sizeof(kParams[0]) == kNumParams, "parameter table incomplete");

inline int paramIndexFromName(const char* name)
{
    if (name == nullptr) return -1;
    for (int i = 0; i < kNumParams; ++i)
        if (std::strcmp(kParams[i].name, name) == 0) return i;
    return -1;
}

inline float defaultPlain(int index)
{
    return (index >= 0 && index < kNumParams) ? kParams[index].def : 0.0f;
}

} // namespace polivoks
