// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep
//
// VST3 wrapper for the Polivoks 1982 engine. The DSP and parameter values are
// shared with the MPC/VST2 build; this wrapper adds a real desktop editor and
// VST3 presets.

#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <vector>

#include "PolivoksDsp.h"
#include "PolivoksParams.h"

using namespace polivoks;

class PolivoksAudioProcessor : public juce::AudioProcessor
{
public:
    PolivoksAudioProcessor();
    ~PolivoksAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Polivoks 1982"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorValueTreeState& parameters() { return apvts; }
    const ParamSpec& paramSpec(int index) const { return kParams[index]; }
    float plainParameter(int index) const;

    static juce::String formatPlainValue(int index, float value);

    // Fired when the host or editor selects a different built-in program.
    juce::ChangeBroadcaster programChanged;

private:
    struct Program
    {
        juce::String name;
        std::array<float, kNumParams> values {};
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void createPrograms();

    float p(int index) const { return raw[(size_t)index]->load(std::memory_order_relaxed); }
    int pi(int index) const { return (int)std::lround(p(index)); }
    bool pb(int index) const { return p(index) > 0.5f; }

    void readSettings();
    void applySettings();
    void updateTargets();
    void triggerEnvelopes();
    void releaseEnvelopes();
    void handleMidi(const juce::MidiMessage& msg);
    void renderRange(float* outL, float* outR, int from, int to);
    float renderSample(float ext);

    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>*, kNumParams> raw {};
    std::vector<Program> programs;
    int currentProgramIndex = 0;

    static constexpr int kCtrlInterval = 16;

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

    float baseSr_ = 44100.0f;
    float osr_ = 176400.0f;

    plv::PolivoksVco vco1_, vco2_;
    plv::PolivoksFilter filter_;
    plv::PolivoksLfo lfo_;
    plv::PolivoksEnvelope filtEnv_, ampEnv_;
    mpc::dsp::Biquad dec1_, dec2_;
    plv::DcBlocker dc_;

    mpc::dsp::Xorshift32 noise_ { 0x1234567u };
    mpc::dsp::Xorshift32 filterSeed_ { 0x9e3779b9u };

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PolivoksAudioProcessor)
};
