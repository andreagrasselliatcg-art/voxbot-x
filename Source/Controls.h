#pragma once

#include "PluginProcessor.h"
#include "VoxBotLookAndFeel.h"

// Shared UI state: which mod slot is armed for assigning, accent colour, etc.
struct UiShared
{
    explicit UiShared (VoxBotAudioProcessor& p) : proc (p) {}

    VoxBotAudioProcessor& proc;
    int armedSlot = -1;
    juce::Colour accent { 0xff19e3ff };
    std::function<void()> onArmedChanged;

    juce::RangedAudioParameter* param (const juce::String& id) const { return proc.apvts.getParameter (id); }
    bool slotOn (int s) const { return param (Params::slotId (s, "on"))->getValue() > 0.5f; }
};

//==============================================================================
/** Rotary knob / micro-slider bound to a parameter. Main targets also show
    the modulation rings of the 4 slots, and edit a slot's depth while that slot is armed. */
class ParamKnob : public juce::Component
{
public:
    enum Style { knob, vslider, mini };

    ParamKnob (UiShared&, const juce::String& paramId, const juce::String& label, int target = -1, Style = knob);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

    juce::Colour colour { 0 };   // 0 = use accent

private:
    bool editingAmount() const { return target >= 0 && ui.armedSlot >= 0; }
    juce::Colour arcColour() const { return colour.getARGB() == 0 ? ui.accent : colour; }

    UiShared& ui;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    float norm = 0.0f;

    int target;
    Style style;
    juce::String label;

    std::array<juce::RangedAudioParameter*, Params::kSlots> amountParams {};
    std::array<std::unique_ptr<juce::ParameterAttachment>, Params::kSlots> amountAttachments;
    std::array<float, Params::kSlots> amounts {};

    bool hover = false, dragging = false;
    int dragSlot = -1;
    float dragStart = 0.0f;
};

//==============================================================================
class ParamToggle : public juce::Component
{
public:
    ParamToggle (UiShared&, const juce::String& paramId, const juce::String& text);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::Colour colour { 0 };

private:
    UiShared& ui;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool on = false;
    juce::String text;
};

//==============================================================================
class ParamSegment : public juce::Component
{
public:
    ParamSegment (UiShared&, const juce::String& paramId, juce::StringArray labels);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::Colour colour { 0 };

private:
    UiShared& ui;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::StringArray labels;
    int index = 0;
};

//==============================================================================
class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (UiShared&, const juce::String& paramId);
private:
    std::unique_ptr<juce::ComboBoxParameterAttachment> attachment;
};

//==============================================================================
class ClickButton : public juce::Component
{
public:
    ClickButton (juce::String t, int arrow = 0) : text (std::move (t)), arrowDir (arrow)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    std::function<void()> onClick;
    juce::Colour colour { 0xffd0d0d8 };
    bool latched = false;

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (onClick && getLocalBounds().contains (e.getPosition())) onClick();
    }

private:
    juce::String text;
    int arrowDir;   // -1 left, +1 right, 0 text
    bool hover = false;
};

//==============================================================================
/** The big robot-face knob: selects factory presets. */
class FaceSelector : public juce::Component
{
public:
    explicit FaceSelector (UiShared& u) : ui (u) { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { accum = 0.0f; lastY = 0.0f; }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void step (int delta);

private:
    UiShared& ui;
    float accum = 0.0f, lastY = 0.0f;
};

//==============================================================================
/** 16-step sequencer editor. */
class StepEditor : public juce::Component
{
public:
    StepEditor (UiShared& u, int s) : ui (u), slot (s) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void edit (const juce::MouseEvent&, bool interpolate);
    UiShared& ui;
    int slot;
    int lastStep = -1;
    float lastValue = 0.0f;
};
