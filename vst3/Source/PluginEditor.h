// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#pragma once

#include <JuceHeader.h>
#include <memory>

class PolivoksAudioProcessor;
class PolivoksEditorContent;

class PolivoksAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::ChangeListener
{
public:
    explicit PolivoksAudioProcessorEditor(PolivoksAudioProcessor&);
    ~PolivoksAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    PolivoksAudioProcessor& proc;
    std::unique_ptr<PolivoksEditorContent> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PolivoksAudioProcessorEditor)
};
