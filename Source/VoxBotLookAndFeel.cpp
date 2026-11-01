#include "VoxBotLookAndFeel.h"
#include "RobotFaces.h"
#include "VocoderEngine.h"

juce::Colour VoxBotLookAndFeel::presetColour (int index)
{
    index = juce::jlimit (0, vbx::kNumPresets - 1, index);
    return juce::Colour (vbx::presets()[(size_t) index].colour);
}

void VoxBotLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float start, float end, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    if (s.getProperties()["knobStyle"].toString() == "main")
        drawMainKnob (g, bounds, s, start, end);
    else
        drawSmallKnob (g, bounds, s, pos, start, end);
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

void VoxBotLookAndFeel::drawMainKnob (juce::Graphics& g, juce::Rectangle<float> bounds,
                                      juce::Slider& s, float start, float end)
{
    const auto  c      = bounds.getCentre();
    const float outerR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const int   current = juce::roundToInt (s.getValue());
    const auto  col    = presetColour (current);
    const int   n      = vbx::kNumPresets;

    // ---- Preset LEDs: a short bar of square LEDs per preset -----------------
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

    // ---- Knob ---------------------------------------------------------------
    const float knobR = outerR * 0.74f;
    drawKnobBody (g, c, knobR);

    // Inner screen where the face lives
    const float screenR = knobR * 0.74f;
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.16f), c.x, c.y,
                                             juce::Colours::black.withAlpha (0.0f), c.x + screenR, c.y, true));
    g.fillEllipse (c.x - screenR, c.y - screenR, screenR * 2.0f, screenR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (c.x - screenR, c.y - screenR, screenR * 2.0f, screenR * 2.0f, 2.0f);

    RobotFaces::draw (g, juce::Rectangle<float> (screenR * 1.5f, screenR * 1.5f).withCentre (c).translated (0, screenR * 0.04f),
                      current, col);

    // Pointer: glowing dot on the knob rim
    const float angle = start + (end - start) * (float) current / (float) (n - 1);
    const auto  dot   = c.getPointOnCircumference (knobR * 0.84f, angle);
    g.setColour (col.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (knobR * 0.16f, knobR * 0.16f).withCentre (dot));
    g.setColour (col);
    g.fillEllipse (juce::Rectangle<float> (knobR * 0.08f, knobR * 0.08f).withCentre (dot));
}

void VoxBotLookAndFeel::drawSmallKnob (juce::Graphics& g, juce::Rectangle<float> bounds,
                                       juce::Slider& s, float pos, float start, float end)
{
    const auto  c      = bounds.getCentre();
    const float outerR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  accent = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("accent", (juce::int64) 0xffd0d0d8));
    const float zero   = (float) s.getProperties().getWithDefault ("zeroPos", 0.0);

    const float arcR = outerR * 0.92f;
    juce::Path track; track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (Palette::ledOff);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float zeroAngle = start + zero * (end - start);
    const float valAngle  = start + pos  * (end - start);
    if (std::abs (valAngle - zeroAngle) > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (zeroAngle, valAngle), juce::jmax (zeroAngle, valAngle), true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (val, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float knobR = outerR * 0.74f;
    drawKnobBody (g, c, knobR);

    juce::Path pointer;
    const float pw = knobR * 0.10f;
    pointer.addRoundedRectangle (-pw * 0.5f, -knobR * 0.80f, pw, knobR * 0.38f, pw * 0.5f);
    g.setColour (juce::Colour (0xffe0e0e6));
    g.fillPath (pointer, juce::AffineTransform::rotation (valAngle).translated (c.x, c.y));
}
