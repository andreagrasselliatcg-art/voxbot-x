#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Palette
{
    const juce::Colour bgTop     { 0xff1c1c21 };
    const juce::Colour bgBottom  { 0xff0a0a0c };
    const juce::Colour panel     { 0xff151519 };
    const juce::Colour panelLine { 0xff2a2a31 };
    const juce::Colour textMain  { 0xffe8e8ee };
    const juce::Colour textDim   { 0xff7a7a85 };
    const juce::Colour ledOff    { 0xff212128 };
}

class VoxBotLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoxBotLookAndFeel();

    // ComboBox / popup styling
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    // Shared drawing helpers
    static juce::Colour presetColour (int index);
    static void drawKnobBody (juce::Graphics&, juce::Point<float> centre, float radius);
    static void drawFaceKnob (juce::Graphics&, juce::Rectangle<float> bounds, int presetIndex, int numPresets);
    static void drawPanel (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title, juce::Colour accent);
};
