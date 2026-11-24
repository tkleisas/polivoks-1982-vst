// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstring>

PolivoksAudioProcessor::PolivoksAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        raw[(size_t)i] = apvts.getRawParameterValue(kParams[i].id);
        jassert(raw[(size_t)i] != nullptr);
    }

    createPrograms();
    setCurrentProgram(0);
}

PolivoksAudioProcessor::~PolivoksAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout PolivoksAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& spec = kParams[i];
        const juce::ParameterID pid { spec.id, 1 };

        if (spec.kind == ParamKind::Choice)
        {
            juce::StringArray choices;
            for (int c = 0; c < spec.numChoices; ++c)
                choices.add(spec.choices[c]);
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                pid, spec.name, choices, (int)std::lround(spec.def)));
        }
        else if (spec.kind == ParamKind::Bool)
        {
            layout.add(std::make_unique<juce::AudioParameterBool>(
                pid, spec.name, spec.def > 0.5f));
        }
        else
        {
            juce::NormalisableRange<float> range(spec.min, spec.max, spec.interval);
            if (spec.logarithmic && spec.min > 0.0f && spec.max > spec.min)
                range.setSkewForCentre(std::sqrt(spec.min * spec.max));

            auto attrs = juce::AudioParameterFloatAttributes()
                             .withStringFromValueFunction([i](float value, int)
                             {
                                 return formatPlainValue(i, value);
                             })
                             .withLabel("");
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                pid, spec.name, range, spec.def, attrs));
        }
    }

    return layout;
}

juce::String PolivoksAudioProcessor::formatPlainValue(int index, float value)
{
    switch (index)
    {
        case kVolume:
            return juce::String(value, 1) + " dB";

        case kCutoff:
            return value >= 1000.0f ? juce::String(value / 1000.0f, 2) + " kHz"
                                    : juce::String(value, 0) + " Hz";

        case kLfoRate:
            return juce::String(value, 2) + " Hz";

        case kVco2Detune:
        case kTune:
            return juce::String(value, 2) + " st";

        case kGlide:
        case kFAttack:
        case kFDecay:
        case kFRelease:
        case kAAttack:
        case kADecay:
        case kARelease:
            return (value >= 1000.0f) ? juce::String(value / 1000.0f, 2) + " s"
                                      : juce::String(value, 0) + " ms";

        case kBendRange:
            return juce::String(value, 0) + " st";

        case kResonance:
        case kEnvAmount:
        case kLfoToVcf:
        case kVco1Level:
        case kVco2Level:
        case kNoiseLevel:
        case kExtLevel:
        case kXMod:
        case kLfoToVco1:
        case kLfoToVco2:
        case kLfoToVca:
        case kFSustain:
        case kASustain:
        case kKeyTrack:
        case kVelocity:
        case kPedal:
            return juce::String(value, 0) + " %";

        default:
            return juce::String(value, 0);
    }
}

float PolivoksAudioProcessor::plainParameter(int index) const
{
    if (index < 0 || index >= kNumParams || raw[(size_t)index] == nullptr)
        return defaultPlain(index);
    return raw[(size_t)index]->load(std::memory_order_relaxed);
}

int PolivoksAudioProcessor::getNumPrograms()
{
    return (int)programs.size();
}

int PolivoksAudioProcessor::getCurrentProgram()
{
    return currentProgramIndex;
}

const juce::String PolivoksAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < (int)programs.size())
        return programs[(size_t)index].name;
    return {};
}

void PolivoksAudioProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= (int)programs.size())
        return;

    currentProgramIndex = index;
    const auto& values = programs[(size_t)index].values;
    for (int i = 0; i < kNumParams; ++i)
    {
        if (auto* param = apvts.getParameter(kParams[i].id))
            param->setValueNotifyingHost(param->convertTo0to1(values[(size_t)i]));
    }
    programChanged.sendChangeMessage();
}

void PolivoksAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    if (index >= 0 && index < (int)programs.size() && newName.isNotEmpty())
        programs[(size_t)index].name = newName;
}

void PolivoksAudioProcessor::createPrograms()
{
    auto program = [this](const char* name, std::initializer_list<std::pair<int, float>> overrides)
    {
        Program pr;
        pr.name = name;
        for (int i = 0; i < kNumParams; ++i)
            pr.values[(size_t)i] = defaultPlain(i);
        for (const auto& o : overrides)
            pr.values[(size_t)o.first] = o.second;
        programs.push_back(std::move(pr));
    };

    // Keep this list in step with plugin/Polivoks 1982/src/Polivoks1982.cpp.
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
}

void PolivoksAudioProcessor::prepareToPlay(double sampleRate, int)
{
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

void PolivoksAudioProcessor::releaseResources()
{
}

void PolivoksAudioProcessor::reset()
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

    lastGateHold_ = pb(kGateHold);
    lastVoiceMode_ = pi(kVoiceMode);
}

bool PolivoksAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (!layouts.getMainInputChannelSet().isDisabled())
        return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void PolivoksAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n <= 0 || buffer.getNumChannels() <= 0)
        return;

    readSettings();
    applySettings();

    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    int pos = 0;
    for (const auto meta : midiMessages)
    {
        const int off = juce::jlimit(0, n, (int)meta.samplePosition);
        if (off > pos)
        {
            renderRange(outL, outR, pos, off);
            pos = off;
        }
        handleMidi(meta.getMessage());
    }
    if (pos < n)
        renderRange(outL, outR, pos, n);
}

void PolivoksAudioProcessor::readSettings()
{
    s_.vco1Wave = pi(kVco1Wave);
    s_.vco1Oct = pi(kVco1Oct);
    s_.vco2Wave = pi(kVco2Wave);
    s_.vco2Oct = pi(kVco2Oct);
    s_.filterMode = pi(kFilterMode);
    s_.lfoShape = pi(kLfoShape);
    s_.voiceMode = pi(kVoiceMode);
    s_.bendRange = pi(kBendRange);

    s_.cutoff = p(kCutoff);
    s_.resonance = p(kResonance) * 0.01f;
    s_.envAmount = p(kEnvAmount) * 0.01f;
    s_.lfoToVcf = p(kLfoToVcf) * 0.01f;
    s_.vco2Detune = p(kVco2Detune);
    s_.vco1Level = p(kVco1Level) * 0.01f;
    s_.vco2Level = p(kVco2Level) * 0.01f;
    s_.noiseLevel = p(kNoiseLevel) * 0.01f;
    s_.extLevel = p(kExtLevel) * 0.01f;
    s_.xmod = p(kXMod) * 0.01f;
    s_.lfoToVco1 = p(kLfoToVco1) * 0.01f;
    s_.lfoToVco2 = p(kLfoToVco2) * 0.01f;
    s_.lfoRate = p(kLfoRate);
    s_.lfoToVca = p(kLfoToVca) * 0.01f;

    s_.fAttack = p(kFAttack);
    s_.fDecay = p(kFDecay);
    s_.fSustain = p(kFSustain) * 0.01f;
    s_.fRelease = p(kFRelease);
    s_.fLoop = pb(kFLoop);
    s_.aAttack = p(kAAttack);
    s_.aDecay = p(kADecay);
    s_.aSustain = p(kASustain) * 0.01f;
    s_.aRelease = p(kARelease);
    s_.aLoop = pb(kALoop);

    s_.gateHold = pb(kGateHold);
    s_.glide = p(kGlide);
    s_.tune = p(kTune);
    s_.keyTrack = p(kKeyTrack) * 0.01f;
    s_.velocity = p(kVelocity) * 0.01f;
    s_.volumeDb = p(kVolume);
    s_.pedal = p(kPedal) * 0.01f;

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

void PolivoksAudioProcessor::applySettings()
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

void PolivoksAudioProcessor::updateTargets()
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

void PolivoksAudioProcessor::triggerEnvelopes() { filtEnv_.gateOn(); ampEnv_.gateOn(); }
void PolivoksAudioProcessor::releaseEnvelopes() { filtEnv_.gateOff(); ampEnv_.gateOff(); }

void PolivoksAudioProcessor::handleMidi(const juce::MidiMessage& msg)
{
    if (msg.isNoteOn())
    {
        const int n = juce::jlimit(0, 127, msg.getNoteNumber());
        if (!held_[n]) { held_[n] = true; ++heldCount_; }
        lastVel_ = juce::jlimit(1, 127, (int)msg.getVelocity());
        velGain_ = mpc::dsp::lerp(1.0f, (float)lastVel_ * (1.0f / 127.0f), s_.velocityAmt);
        updateTargets();
        hasPlayed_ = true;
        if (!gate_) { gate_ = true; triggerEnvelopes(); }
    }
    else if (msg.isNoteOff())
    {
        const int n = juce::jlimit(0, 127, msg.getNoteNumber());
        if (held_[n]) { held_[n] = false; --heldCount_; }
        updateTargets();
        if (heldCount_ == 0 && !s_.gateHold)
        {
            gate_ = false;
            releaseEnvelopes();
        }
    }
    else if (msg.isAllNotesOff() || msg.isAllSoundOff())
    {
        std::memset(held_, 0, sizeof(held_));
        heldCount_ = 0;
        updateTargets();
        gate_ = false;
        releaseEnvelopes();
    }
    else if (msg.isPitchWheel())
    {
        const float bend = ((float)msg.getPitchWheelValue() - 8192.0f) * (1.0f / 8192.0f);
        bendTarget_ = bend * (float)s_.bendRange;
    }
}

void PolivoksAudioProcessor::renderRange(float* outL, float* outR, int from, int to)
{
    for (int i = from; i < to; ++i)
    {
        const float out = renderSample(0.0f);
        outL[i] = out;
        if (outR != nullptr) outR[i] = out;
    }
}

float PolivoksAudioProcessor::renderSample(float ext)
{
    float decimated = 0.0f;

    for (int k = 0; k < plv::kOversample; ++k)
    {
        float lfoSigned = 0.0f, lfoUnipolar = 0.0f;
        lfo_.process(lfoSigned, lfoUnipolar);

        const float fenv = filtEnv_.process();
        const float aenv = ampEnv_.process();

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
        mix += filterSeed_.nextBipolar() * 2.0e-5f;

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

void PolivoksAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("program", currentProgramIndex, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void PolivoksAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr) return;

    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return;

    apvts.replaceState(tree);
    const int program = (int)tree.getProperty("program", 0);
    if (program >= 0 && program < (int)programs.size())
        currentProgramIndex = program;
    programChanged.sendChangeMessage();
}

juce::AudioProcessorEditor* PolivoksAudioProcessor::createEditor()
{
    return new PolivoksAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PolivoksAudioProcessor();
}
