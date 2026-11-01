#include "VoxBotLookAndFeel.h"
#include "RobotFaces.h"
#include "VocoderEngine.h"

VoxBotLookAndFeel::VoxBotLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0d0d10));
    setColour (juce::ComboBox::textColourId, Palette::textMain);
    setColour (juce::ComboBox::outlineColourId, Palette::panelLine);
    setColour (juce::ComboBox::arrowColourId, Palette::textDim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141418));
    setColour (juce::PopupMenu::textColourId, Palette::textMain);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2c2c36));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

juce::Font VoxBotLookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::Font (juce::FontOptions (12.0f, juce::Font::bold)); }
juce::Font VoxBotLookAndFeel::getPopupMenuFont()                { return juce::Font (juce::FontOptions (13.0f)); }

void VoxBotLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    auto b = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (box.hasKeyboardFocus (false) || box.isMouseOver (true) ? Palette::textDim : Palette::panelLine);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    const float ax = (float) width - 12.0f, ay = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void VoxBotLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

//==============================================================================
juce::Colour VoxBotLookAndFeel::presetColour (int index)
{
    index = juce::jlimit (0, vbx::kNumPresets - 1, index);
    return juce::Colour (vbx::presets()[(size_t) index].colour);
}

void VoxBotLookAndFeel::drawKnobBody (juce::Graphics& g, juce::Point<float> c, float r)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (c.x - r * 1.04f, c.y - r * 1.0f, r * 2.08f, r * 2.14f);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a41), c.x, c.y - r,
                                             juce::Colour (0xff09090b), c.x, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    const float capR = r * 0.90f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff27272c), c.x - capR * 0.4f, c.y - capR * 0.6f,
                                             juce::Colour (0xff0f0f12), c.x + capR * 0.5f, c.y + capR, true));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.2f);
}

void VoxBotLookAndFeel::drawFaceKnob (juce::Graphics& g, juce::Rectangle<float> bounds, int current, int n)
{
    const float start = juce::MathConstants<float>::pi * 1.25f;
    const float end   = juce::MathConstants<float>::pi * 2.75f;
    const auto  c      = bounds.getCentre();
    const float outerR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  col    = presetColour (current);

    // Preset LEDs: a short bar of square LEDs per preset
    const float ledR    = outerR * 0.90f;
    const float ledSize = outerR * 0.068f;
    for (int i = 0; i < n; ++i)
    {
        const float centreAngle = start + (end - start) * (float) i / (float) (n - 1);
        const auto  ledCol      = presetColour (i);
        const bool  lit         = i == current;

        for (int k = -1; k <= 1; ++k)
        {
            const float a  = centreAngle + (float) k * 0.075f;
            const auto  pc = c.getPointOnCircumference (ledR, a);
            const auto  t  = juce::AffineTransform::rotation (a, pc.x, pc.y);
            auto sq = juce::Rectangle<float> (ledSize, ledSize).withCentre (pc);
            juce::Path led; led.addRoundedRectangle (sq, ledSize * 0.14f);

            if (lit)
            {
                for (int gl = 3; gl >= 1; --gl)
                {
                    juce::Path glow; glow.addRoundedRectangle (sq.expanded (ledSize * 0.3f * (float) gl), ledSize * 0.3f);
                    g.setColour (ledCol.withAlpha (0.08f));
                    g.fillPath (glow, t);
                }
                g.setColour (ledCol);
                g.fillPath (led, t);
                juce::Path hot; hot.addRoundedRectangle (sq.reduced (ledSize * 0.28f), ledSize * 0.08f);
                g.setColour (ledCol.brighter (0.9f).withAlpha (0.7f));
                g.fillPath (hot, t);
            }
            else
            {
                g.setColour (Palette::ledOff.interpolatedWith (ledCol, 0.18f));
                g.fillPath (led, t);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.strokePath (led, juce::PathStrokeType (1.0f), t);
            }
        }
    }

    const float knobR = outerR * 0.74f;
    drawKnobBody (g, c, knobR);

    const float screenR = knobR * 0.74f;
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.16f), c.x, c.y,
                                             juce::Colours::black.withAlpha (0.0f), c.x + screenR, c.y, true));
    g.fillEllipse (c.x - screenR, c.y - screenR, screenR * 2.0f, screenR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (c.x - screenR, c.y - screenR, screenR * 2.0f, screenR * 2.0f, 2.0f);

    RobotFaces::draw (g, juce::Rectangle<float> (screenR * 1.5f, screenR * 1.5f).withCentre (c).translated (0, screenR * 0.04f),
                      current, col);

    const float angle = start + (end - start) * (float) current / (float) (n - 1);
    const auto  dot   = c.getPointOnCircumference (knobR * 0.84f, angle);
    g.setColour (col.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (knobR * 0.16f, knobR * 0.16f).withCentre (dot));
    g.setColour (col);
    g.fillEllipse (juce::Rectangle<float> (knobR * 0.08f, knobR * 0.08f).withCentre (dot));
}

void VoxBotLookAndFeel::drawPanel (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title, juce::Colour accent)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (area.translated (0, 2), 8.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff18181d), area.getX(), area.getY(),
                                             juce::Colour (0xff111114), area.getX(), area.getBottom(), false));
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (Palette::panelLine);
    g.drawRoundedRectangle (area, 8.0f, 1.0f);

    if (title.isNotEmpty())
    {
        g.setColour (accent.withAlpha (0.85f));
        g.fillRoundedRectangle (area.getX() + 12.0f, area.getY() + 11.0f, 3.0f, 10.0f, 1.5f);
        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (title, area.withTrimmedLeft (21).withHeight (32).toNearestInt(), juce::Justification::centredLeft);
    }
}
