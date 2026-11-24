// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 bokontep

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include "BinaryData.h"

#include <array>
#include <cmath>

namespace
{
using namespace juce;

const Colour kIvory { 0xfff1e7d6 };
const Colour kMuted { 0xff9c9488 };
const Colour kRed { 0xffd24a2a };
const Colour kPanelBlack { 0xff121110 };
const Colour kBarLine { 0xff3a3530 };

Font labelFont(float height, bool bold = false)
{
    return Font(height, bold ? Font::bold : Font::plain);
}

// ---------------------------------------------------------------------------
// Filmstrip knob. The artwork is the same 127-frame strip used by the MPC
// skin, so both versions of the instrument look like one product.
// ---------------------------------------------------------------------------
class FilmstripSlider : public Slider
{
public:
    FilmstripSlider(const Image& stripImage, int frameCount, RangedAudioParameter& parameter)
        : strip(stripImage), frames(frameCount), param(parameter)
    {
        setSliderStyle(RotaryHorizontalVerticalDrag);
        setTextBoxStyle(NoTextBox, false, 0, 0);
        setWantsKeyboardFocus(true);
        setMouseDragSensitivity(160);
        setScrollWheelEnabled(true);
    }

    void paint(Graphics& g) override
    {
        if (!strip.isValid() || frames < 2)
        {
            Slider::paint(g);
            return;
        }

        const int frameH = strip.getHeight() / frames;
        const int frame = jlimit(0, frames - 1,
                                 (int)std::lround(param.getValue() * (float)(frames - 1)));
        g.setImageResamplingQuality(Graphics::highResamplingQuality);
        g.drawImage(strip, 0, 0, getWidth(), getHeight(),
                    0, frame * frameH, strip.getWidth(), frameH);

        if (hasKeyboardFocus(false))
        {
            g.setColour(kRed.withAlpha(0.75f));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f, 1.5f);
        }
    }

    void mouseDown(const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (auto* choice = dynamic_cast<AudioParameterChoice*>(&param))
            {
                showChoiceMenu(choice);
                return;
            }
        }
        Slider::mouseDown(e);
    }

private:
    void showChoiceMenu(AudioParameterChoice* choice)
    {
        PopupMenu menu;
        for (int i = 0; i < choice->choices.size(); ++i)
            menu.addItem(i + 1, choice->choices[i], true, i == choice->getIndex());

        Component::SafePointer<FilmstripSlider> safe(this);
        menu.showMenuAsync(PopupMenu::Options().withTargetComponent(this),
                           [safe, choice](int result)
                           {
                               if (safe == nullptr || result <= 0)
                                   return;
                               choice->setValueNotifyingHost(
                                   choice->convertTo0to1((float)(result - 1)));
                           });
    }

    Image strip;
    int frames = 127;
    RangedAudioParameter& param;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilmstripSlider)
};

// ---------------------------------------------------------------------------
// A knob plus its name/value column.
// ---------------------------------------------------------------------------
class FilmstripControl : public Component
{
public:
    FilmstripControl(PolivoksAudioProcessor& processor, int paramIndex, bool bipolar)
        : index(paramIndex)
    {
        auto& state = processor.parameters();
        param = state.getParameter(kParams[(size_t)index].id);
        jassert(param != nullptr);

        const Image strip = bipolar
            ? ImageCache::getFromMemory(BinaryData::knob_bi_png, BinaryData::knob_bi_pngSize)
            : ImageCache::getFromMemory(BinaryData::knob_uni_png, BinaryData::knob_uni_pngSize);

        slider = std::make_unique<FilmstripSlider>(strip, 127, *param);
        addAndMakeVisible(*slider);
        slider->setBounds(8, 8, 90, 90);

        nameLabel.setText(kParams[(size_t)index].name, dontSendNotification);
        nameLabel.setFont(labelFont(22.0f));
        nameLabel.setColour(Label::textColourId, kMuted);
        nameLabel.setJustificationType(Justification::centredLeft);
        nameLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(nameLabel);
        nameLabel.setBounds(110, 10, 158, 34);

        valueLabel.setFont(labelFont(27.0f));
        valueLabel.setColour(Label::textColourId, kIvory);
        valueLabel.setJustificationType(Justification::centredLeft);
        valueLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(valueLabel);
        valueLabel.setBounds(110, 44, 158, 36);

        sliderAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            state, kParams[(size_t)index].id, *slider);

        valueAttachment = std::make_unique<ParameterAttachment>(
            *param, [this](float) { refreshValue(); });
        valueAttachment->sendInitialUpdate();

        setInterceptsMouseClicks(false, true);
    }

    void refreshValue()
    {
        if (param != nullptr)
            valueLabel.setText(param->getCurrentValueAsText(), dontSendNotification);
    }

private:
    int index = 0;
    RangedAudioParameter* param = nullptr;
    std::unique_ptr<FilmstripSlider> slider;
    Label nameLabel;
    Label valueLabel;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
    std::unique_ptr<ParameterAttachment> valueAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilmstripControl)
};

// ---------------------------------------------------------------------------
// White paddle switch.
// ---------------------------------------------------------------------------
class PaddleButton : public Button
{
public:
    PaddleButton(const Image& onImage, const Image& offImage)
        : Button("paddle"), onArt(onImage), offArt(offImage)
    {
        setClickingTogglesState(false);
        setWantsKeyboardFocus(false);
        setMouseCursor(MouseCursor::PointingHandCursor);
    }

    void paintButton(Graphics& g, bool highlighted, bool down) override
    {
        const auto& art = getToggleState() ? onArt : offArt;
        if (art.isValid())
        {
            g.setImageResamplingQuality(Graphics::highResamplingQuality);
            g.drawImage(art, getLocalBounds().toFloat());
        }
        else
        {
            g.setColour(kPanelBlack);
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
            g.setColour(kIvory);
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f, 1.0f);
        }

        if (highlighted || down)
        {
            g.setColour(Colours::white.withAlpha(down ? 0.10f : 0.05f));
            g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
        }
        if (hasKeyboardFocus(false))
        {
            g.setColour(kRed.withAlpha(0.8f));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f, 1.5f);
        }
    }

private:
    Image onArt, offArt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PaddleButton)
};

class PaddleControl : public Component
{
public:
    PaddleControl(PolivoksAudioProcessor& processor, int paramIndex)
        : index(paramIndex)
    {
        auto& state = processor.parameters();
        param = state.getParameter(kParams[(size_t)index].id);
        jassert(param != nullptr);

        const Image on = ImageCache::getFromMemory(BinaryData::toggle_on_png,
                                                   BinaryData::toggle_on_pngSize);
        const Image off = ImageCache::getFromMemory(BinaryData::toggle_off_png,
                                                    BinaryData::toggle_off_pngSize);

        button = std::make_unique<PaddleButton>(on, off);
        addAndMakeVisible(*button);
        button->setBounds(186, 18, 84, 68);
        button->onClick = [this]
        {
            if (param == nullptr) return;
            const float next = param->getValue() > 0.5f ? 0.0f : 1.0f;
            param->setValueNotifyingHost(next);
        };

        nameLabel.setText(kParams[(size_t)index].name, dontSendNotification);
        nameLabel.setFont(labelFont(22.0f));
        nameLabel.setColour(Label::textColourId, kMuted);
        nameLabel.setJustificationType(Justification::centredLeft);
        nameLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(nameLabel);
        nameLabel.setBounds(15, 9, 160, 34);

        valueLabel.setFont(labelFont(26.0f));
        valueLabel.setColour(Label::textColourId, kIvory);
        valueLabel.setJustificationType(Justification::centredLeft);
        valueLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(valueLabel);
        valueLabel.setBounds(15, 45, 160, 36);

        attachment = std::make_unique<ParameterAttachment>(
            *param, [this](float) { refresh(); });
        attachment->sendInitialUpdate();

        setInterceptsMouseClicks(false, true);
    }

    void refresh()
    {
        if (param == nullptr) return;
        button->setToggleState(param->getValue() > 0.5f, dontSendNotification);
        valueLabel.setText(param->getCurrentValueAsText(), dontSendNotification);
    }

private:
    int index = 0;
    RangedAudioParameter* param = nullptr;
    std::unique_ptr<PaddleButton> button;
    Label nameLabel;
    Label valueLabel;
    std::unique_ptr<ParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PaddleControl)
};

// ---------------------------------------------------------------------------
// One 1280 x 628 panel page. The background art is the MPC skin background;
// the control positions are the same 4 x 4 slots used on the hardware.
// ---------------------------------------------------------------------------
class PageComponent : public Component
{
public:
    PageComponent(PolivoksAudioProcessor& processor, const var& page, const Image& backgroundImage)
        : background(backgroundImage)
    {
        setSize(1280, 628);
        setOpaque(true);
        setInterceptsMouseClicks(false, true);

        if (auto* controls = page.getProperty("controls", var()).getArray())
        {
            for (int i = 0; i < controls->size(); ++i)
            {
                const auto control = (*controls)[i];
                if (control.isString())
                    continue;

                const auto name = control.getProperty("param", var()).toString();
                const int paramIndex = paramIndexFromName(name.toRawUTF8());
                if (paramIndex < 0)
                    continue;

                const auto widget = control.getProperty("widget", var()).toString();
                const bool isSwitch = widget == "switch";
                const bool bipolar = control.getProperty("polarity", var()).toString() == "bipolar"
                                  || kParams[(size_t)paramIndex].bipolar;

                const int col = i % 4;
                const int row = i / 4;
                const int x = 23 + col * 320;
                const int y = 95 + row * 130;

                std::unique_ptr<Component> child;
                if (isSwitch)
                    child = std::make_unique<PaddleControl>(processor, paramIndex);
                else
                    child = std::make_unique<FilmstripControl>(processor, paramIndex, bipolar);

                child->setBounds(x, y, 278, 107);
                addAndMakeVisible(*child);
                children.push_back(std::move(child));
            }
        }
    }

    void paint(Graphics& g) override
    {
        if (background.isValid())
        {
            g.setImageResamplingQuality(Graphics::highResamplingQuality);
            g.drawImage(background, getLocalBounds().toFloat());
        }
        else
        {
            g.fillAll(kPanelBlack);
        }
    }

private:
    Image background;
    std::vector<std::unique_ptr<Component>> children;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PageComponent)
};

// ---------------------------------------------------------------------------
// Flat bottom-bar tab.
// ---------------------------------------------------------------------------
class PanelTab : public Button
{
public:
    explicit PanelTab(const String& label) : Button(label)
    {
        setClickingTogglesState(false);
        setWantsKeyboardFocus(false);
        setMouseCursor(MouseCursor::PointingHandCursor);
    }

    void paintButton(Graphics& g, bool highlighted, bool down) override
    {
        const bool active = getToggleState();
        if (highlighted || down)
        {
            g.setColour(Colours::white.withAlpha(down ? 0.10f : 0.05f));
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.0f), 4.0f);
        }

        g.setColour(active ? kIvory : kMuted);
        g.setFont(labelFont(17.0f, active));
        g.drawText(getButtonText(), getLocalBounds().withTrimmedBottom(4),
                   Justification::centred, false);

        if (active)
        {
            g.setColour(kRed);
            g.fillRect(getWidth() / 2 - 22, getHeight() - 4, 44, 2);
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PanelTab)
};

} // namespace

// ---------------------------------------------------------------------------
// Editor content
// ---------------------------------------------------------------------------
class PolivoksEditorContent : public Component
{
public:
    explicit PolivoksEditorContent(PolivoksAudioProcessor& processor)
        : proc(processor)
    {
        setOpaque(true);

        const var json = JSON::parse(String::fromUTF8(BinaryData::ui_json,
                                                      BinaryData::ui_jsonSize));
        auto* pageArray = json.getProperty("pages", var()).getArray();

        static const std::array<const char*, 4> names { "VCO", "FILTER", "AMP", "LFO" };
        const std::array<std::pair<const char*, int>, 4> backgrounds
        {{
            { BinaryData::bg_vco_png, BinaryData::bg_vco_pngSize },
            { BinaryData::bg_filter_png, BinaryData::bg_filter_pngSize },
            { BinaryData::bg_amp_png, BinaryData::bg_amp_pngSize },
            { BinaryData::bg_lfo_png, BinaryData::bg_lfo_pngSize },
        }};

        for (int i = 0; i < 4; ++i)
        {
            const var page = (pageArray != nullptr && pageArray->size() > i)
                                 ? (*pageArray)[i] : var();
            const Image bg = ImageCache::getFromMemory(backgrounds[(size_t)i].first,
                                                       backgrounds[(size_t)i].second);
            pages[(size_t)i] = std::make_unique<PageComponent>(proc, page, bg);
            addAndMakeVisible(*pages[(size_t)i]);
            pages[(size_t)i]->setBounds(0, 0, 1280, 628);

            tabs[(size_t)i] = std::make_unique<PanelTab>(names[(size_t)i]);
            addAndMakeVisible(*tabs[(size_t)i]);
            tabs[(size_t)i]->onClick = [this, i] { showPage(i); };
        }

        presetLabel.setText("PRESET", dontSendNotification);
        presetLabel.setFont(labelFont(10.0f, true));
        presetLabel.setColour(Label::textColourId, kMuted);
        presetLabel.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(presetLabel);

        for (int i = 0; i < proc.getNumPrograms(); ++i)
            presetBox.addItem(proc.getProgramName(i), i + 1);
        presetBox.setSelectedId(proc.getCurrentProgram() + 1, dontSendNotification);
        presetBox.setColour(ComboBox::backgroundColourId, Colour(0xff1a1816));
        presetBox.setColour(ComboBox::textColourId, kIvory);
        presetBox.setColour(ComboBox::outlineColourId, Colour(0xff5a544a));
        presetBox.setColour(ComboBox::arrowColourId, kRed);
        presetBox.setColour(ComboBox::focusedOutlineColourId, kRed);
        presetBox.onChange = [this]
        {
            const int id = presetBox.getSelectedId();
            if (id > 0)
                proc.setCurrentProgram(id - 1);
        };
        addAndMakeVisible(presetBox);

        editionLabel.setText("MPC-STYLE PANEL  /  VST3", dontSendNotification);
        editionLabel.setFont(labelFont(10.0f, true));
        editionLabel.setColour(Label::textColourId, kMuted.withAlpha(0.75f));
        editionLabel.setJustificationType(Justification::centredRight);
        addAndMakeVisible(editionLabel);

        showPage(0);
        setSize(1280, 684);
    }

    void showPage(int page)
    {
        activePage = jlimit(0, 3, page);
        for (int i = 0; i < 4; ++i)
        {
            pages[(size_t)i]->setVisible(i == activePage);
            tabs[(size_t)i]->setToggleState(i == activePage, dontSendNotification);
        }
        repaint();
    }

    void syncProgram()
    {
        presetBox.setSelectedId(proc.getCurrentProgram() + 1, dontSendNotification);
    }

    void paint(Graphics& g) override
    {
        g.fillAll(kPanelBlack);
        g.setColour(kBarLine);
        g.drawHorizontalLine(628, 0.0f, 1280.0f);
    }

    void resized() override
    {
        if (pages[0] == nullptr)
            return;

        for (int i = 0; i < 4; ++i)
        {
            pages[(size_t)i]->setBounds(0, 0, 1280, 628);
            tabs[(size_t)i]->setBounds(24 + i * 116, 640, 108, 34);
        }

        presetLabel.setBounds(760, 631, 70, 14);
        presetBox.setBounds(760, 647, 300, 27);
        editionLabel.setBounds(1080, 640, 176, 34);
    }

private:
    PolivoksAudioProcessor& proc;
    std::array<std::unique_ptr<PageComponent>, 4> pages;
    std::array<std::unique_ptr<PanelTab>, 4> tabs;
    Label presetLabel;
    ComboBox presetBox;
    Label editionLabel;
    int activePage = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PolivoksEditorContent)
};

// ---------------------------------------------------------------------------
// Editor window
// ---------------------------------------------------------------------------
PolivoksAudioProcessorEditor::PolivoksAudioProcessorEditor(PolivoksAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    content = std::make_unique<PolivoksEditorContent>(proc);
    addAndMakeVisible(*content);

    setSize(1280, 684);
    setResizable(true, true);
    setResizeLimits(800, 428, 2560, 1368);
    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio(1280.0 / 684.0);

    proc.programChanged.addChangeListener(this);
    setWantsKeyboardFocus(true);
}

PolivoksAudioProcessorEditor::~PolivoksAudioProcessorEditor()
{
    proc.programChanged.removeChangeListener(this);
}

void PolivoksAudioProcessorEditor::paint(Graphics& g)
{
    g.fillAll(Colour(0xff0b0a09));
}

void PolivoksAudioProcessorEditor::resized()
{
    const auto area = getLocalBounds().toFloat();
    const float scale = jmin(area.getWidth() / 1280.0f,
                             area.getHeight() / 684.0f);

    content->setBounds(0, 0, 1280, 684);
    content->setTransform(AffineTransform::scale(scale)
                              .translated((area.getWidth() - 1280.0f * scale) * 0.5f,
                                          (area.getHeight() - 684.0f * scale) * 0.5f));
}

void PolivoksAudioProcessorEditor::changeListenerCallback(ChangeBroadcaster* source)
{
    if (source == &proc.programChanged && content != nullptr)
        content->syncProgram();
}
