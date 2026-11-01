#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Palette
{
    const juce::Colour bgTop     { 0xff1c1c21 };
    const juce::Colour bgBottom  { 0xff0a0a0c };
    const juce::Colour panelLine { 0xff2a2a31 };
    const juce::Colour textMain  { 0xffe8e8ee };
    const juce::Colour textDim   { 0xff7a7a85 };
    const juce::Colour ledOff    { 0xff212128 };
}

class VoxBotLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    static juce::Colour presetColour (int index);
    static void drawKnobBody (juce::Graphics&, juce::Point<float> centre, float radius);

private:
    void drawMainKnob  (juce::Graphics&, juce::Rectangle<float>, juce::Slider&, float start, float end);
    void drawSmallKnob (juce::Graphics&, juce::Rectangle<float>, juce::Slider&, float pos, float start, float end);
};
