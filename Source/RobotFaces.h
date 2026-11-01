#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace RobotFaces
{
    /** Draws the neon robot face for a preset, centred inside `area` (square). */
    void draw (juce::Graphics& g, juce::Rectangle<float> area, int presetIndex, juce::Colour colour);
}
