#include "RobotFaces.h"

namespace
{
    using juce::Path;
    constexpr float pi = juce::MathConstants<float>::pi;

    // Faces are designed in a -1..1 unit square, then mapped to the target area.
    struct Canvas
    {
        juce::Graphics& g;
        juce::AffineTransform t;
        juce::Colour col;
        float scale;

        void glowStroke (const Path& p, float width = 0.06f) const
        {
            const float w = width * scale;
            g.setColour (col.withAlpha (0.10f));
            g.strokePath (p, juce::PathStrokeType (w * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
            g.setColour (col.withAlpha (0.22f));
            g.strokePath (p, juce::PathStrokeType (w * 1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
        }

        void glowFill (const Path& p) const
        {
            g.setColour (col.withAlpha (0.18f));
            g.strokePath (p, juce::PathStrokeType (0.10f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
            g.setColour (col);
            g.fillPath (p, t);
        }
    };

    Path line (float x1, float y1, float x2, float y2)
    {
        Path p; p.startNewSubPath (x1, y1); p.lineTo (x2, y2); return p;
    }

    Path circle (float cx, float cy, float r)
    {
        Path p; p.addEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f); return p;
    }

    Path rrect (float x, float y, float w, float h, float r)
    {
        Path p; p.addRoundedRectangle (x, y, w, h, r); return p;
    }

    void antenna (const Canvas& c, float x1, float y1, float x2, float y2)
    {
        c.glowStroke (line (x1, y1, x2, y2), 0.05f);
        c.glowFill (circle (x2, y2, 0.065f));
    }

    //--------------------------------------------------------------------------
    void robot (const Canvas& c)
    {
        c.glowStroke (rrect (-0.60f, -0.48f, 1.20f, 1.08f, 0.14f));
        antenna (c, 0.0f, -0.48f, 0.0f, -0.74f);
        c.glowStroke (rrect (-0.76f, -0.16f, 0.12f, 0.32f, 0.04f), 0.045f);
        c.glowStroke (rrect ( 0.64f, -0.16f, 0.12f, 0.32f, 0.04f), 0.045f);
        c.glowFill (rrect (-0.38f, -0.24f, 0.24f, 0.22f, 0.03f));
        c.glowFill (rrect ( 0.14f, -0.24f, 0.24f, 0.22f, 0.03f));
        c.glowStroke (rrect (-0.36f, 0.16f, 0.72f, 0.22f, 0.04f), 0.05f);
        for (float x : { -0.18f, 0.0f, 0.18f })
            c.glowStroke (line (x, 0.16f, x, 0.38f), 0.04f);
    }

    void android (const Canvas& c)
    {
        c.glowStroke (rrect (-0.58f, -0.50f, 1.16f, 1.10f, 0.42f));
        antenna (c, -0.28f, -0.46f, -0.44f, -0.76f);
        antenna (c,  0.28f, -0.46f,  0.44f, -0.76f);
        c.glowFill (circle (-0.24f, -0.10f, 0.13f));
        c.glowFill (circle ( 0.24f, -0.10f, 0.13f));
        c.g.setColour (juce::Colours::black.withAlpha (0.55f));
        c.g.fillPath (circle (-0.20f, -0.14f, 0.045f), c.t);
        c.g.fillPath (circle ( 0.28f, -0.14f, 0.045f), c.t);
        Path smile; smile.addCentredArc (0.0f, 0.10f, 0.30f, 0.24f, 0.0f, pi * 0.68f, pi * 1.32f, true);
        c.glowStroke (smile);
    }

    void choirBot (const Canvas& c)
    {
        c.glowStroke (circle (0.0f, 0.02f, 0.62f));
        Path e1; e1.addCentredArc (-0.24f, -0.06f, 0.12f, 0.10f, 0.0f, -pi * 0.5f, pi * 0.5f, true);
        Path e2; e2.addCentredArc ( 0.24f, -0.06f, 0.12f, 0.10f, 0.0f, -pi * 0.5f, pi * 0.5f, true);
        c.glowStroke (e1); c.glowStroke (e2);
        Path mouth; mouth.addEllipse (-0.12f, 0.16f, 0.24f, 0.30f);
        c.glowFill (mouth);
        // music note
        Path head; head.addEllipse (0.60f, -0.52f, 0.14f, 0.10f);
        c.glowFill (head);
        c.glowStroke (line (0.73f, -0.48f, 0.73f, -0.80f), 0.04f);
        c.glowStroke (line (0.73f, -0.80f, 0.86f, -0.72f), 0.04f);
    }

    void titan (const Canvas& c)
    {
        Path head;
        head.startNewSubPath (-0.70f, -0.50f);
        head.lineTo (0.70f, -0.50f);
        head.lineTo (0.58f, 0.60f);
        head.lineTo (-0.58f, 0.60f);
        head.closeSubPath();
        c.glowStroke (head);
        c.glowStroke (line (-0.48f, -0.34f, -0.12f, -0.18f), 0.06f);
        c.glowStroke (line ( 0.48f, -0.34f,  0.12f, -0.18f), 0.06f);
        c.glowFill (rrect (-0.44f, -0.10f, 0.30f, 0.10f, 0.02f));
        c.glowFill (rrect ( 0.14f, -0.10f, 0.30f, 0.10f, 0.02f));
        c.glowStroke (rrect (-0.40f, 0.16f, 0.80f, 0.28f, 0.04f), 0.05f);
        Path teeth;
        teeth.startNewSubPath (-0.40f, 0.30f);
        for (int i = 1; i <= 8; ++i)
            teeth.lineTo (-0.40f + 0.10f * (float) i, (i % 2) ? 0.18f : 0.42f);
        c.glowStroke (teeth, 0.035f);
        c.glowFill (circle (-0.66f, 0.05f, 0.05f));
        c.glowFill (circle ( 0.66f, 0.05f, 0.05f));
    }

    void alien (const Canvas& c)
    {
        Path head;
        head.startNewSubPath (0.0f, 0.62f);
        head.cubicTo (-0.30f, 0.62f, -0.70f, 0.05f, -0.66f, -0.25f);
        head.cubicTo (-0.62f, -0.78f, 0.62f, -0.78f, 0.66f, -0.25f);
        head.cubicTo (0.70f, 0.05f, 0.30f, 0.62f, 0.0f, 0.62f);
        head.closeSubPath();
        c.glowStroke (head);
        Path eye; eye.addEllipse (-0.19f, -0.09f, 0.38f, 0.18f);
        Path l (eye), r (eye);
        l.applyTransform (juce::AffineTransform::rotation (0.45f).translated (-0.27f, -0.02f));
        r.applyTransform (juce::AffineTransform::rotation (-0.45f).translated (0.27f, -0.02f));
        c.glowFill (l); c.glowFill (r);
        c.glowStroke (line (-0.07f, 0.38f, 0.07f, 0.38f), 0.045f);
        antenna (c, -0.24f, -0.60f, -0.40f, -0.86f);
        antenna (c,  0.24f, -0.60f,  0.40f, -0.86f);
    }

    void ghost (const Canvas& c)
    {
        Path body;
        body.startNewSubPath (-0.55f, 0.62f);
        body.lineTo (-0.55f, -0.08f);
        body.addCentredArc (0.0f, -0.08f, 0.55f, 0.55f, 0.0f, -pi * 0.5f, pi * 0.5f, false);
        body.lineTo (0.55f, 0.62f);
        const float xs[] = { 0.55f, 0.3667f, 0.1833f, 0.0f, -0.1833f, -0.3667f, -0.55f };
        for (int i = 0; i < 6; ++i)
        {
            const float mx = 0.5f * (xs[i] + xs[i + 1]);
            body.quadraticTo (mx, (i % 2 == 0) ? 0.38f : 0.80f, xs[i + 1], 0.62f);
        }
        body.closeSubPath();
        c.glowStroke (body);
        Path e1; e1.addEllipse (-0.32f, -0.22f, 0.22f, 0.30f);
        Path e2; e2.addEllipse ( 0.10f, -0.22f, 0.22f, 0.30f);
        c.glowStroke (e1, 0.05f); c.glowStroke (e2, 0.05f);
        c.glowStroke (circle (0.0f, 0.26f, 0.08f), 0.045f);
    }

    void disco (const Canvas& c)
    {
        c.glowStroke (circle (0.0f, 0.0f, 0.64f));
        c.glowStroke (rrect (-0.76f, -0.18f, 0.12f, 0.36f, 0.05f), 0.045f);
        c.glowStroke (rrect ( 0.64f, -0.18f, 0.12f, 0.36f, 0.05f), 0.045f);

        // visor
        Path visor; visor.addRoundedRectangle (-0.50f, -0.22f, 1.00f, 0.36f, 0.16f);
        c.g.setColour (c.col.withAlpha (0.18f));
        c.g.fillPath (visor, c.t);
        c.glowStroke (visor, 0.045f);
        for (int i = 0; i < 7; ++i)
        {
            const float x = -0.36f + 0.12f * (float) i;
            const float h = (i == 3) ? 0.16f : (i == 2 || i == 4) ? 0.12f : 0.07f;
            c.glowFill (rrect (x - 0.035f, -0.04f - h * 0.5f, 0.07f, h, 0.015f));
        }
        Path seam; seam.addCentredArc (0.0f, 0.0f, 0.46f, 0.46f, 0.0f, pi * 0.72f, pi * 1.28f, true);
        c.glowStroke (seam, 0.04f);
    }

    void eightBit (const Canvas& c)
    {
        static const char* sprite[] = {
            "....XX....",
            "....XX....",
            "XXXXXXXXXX",
            "X........X",
            "X.XX..XX.X",
            "X.XX..XX.X",
            "X........X",
            "X.X.XX.X.X",
            "X..X..X..X",
            "XXXXXXXXXX",
        };
        const int rows = 10, cols = 10;
        const float cell = 0.13f, gap = 0.016f;
        const float w = cols * cell, h = rows * cell;
        Path px;
        for (int r = 0; r < rows; ++r)
            for (int col = 0; col < cols; ++col)
                if (sprite[r][col] == 'X')
                    px.addRectangle (-w * 0.5f + col * cell + gap, -h * 0.5f + r * cell + gap,
                                     cell - 2 * gap, cell - 2 * gap);
        c.g.setColour (c.col.withAlpha (0.16f));
        c.g.strokePath (px, juce::PathStrokeType (0.08f * c.scale), c.t);
        c.g.setColour (c.col);
        c.g.fillPath (px, c.t);
    }
}

void RobotFaces::draw (juce::Graphics& g, juce::Rectangle<float> area, int presetIndex, juce::Colour colour)
{
    const float s = area.getWidth() * 0.5f;
    Canvas c { g,
               juce::AffineTransform::scale (s).translated (area.getCentreX(), area.getCentreY()),
               colour, s };

    switch (presetIndex)
    {
        case 0:  robot (c);    break;
        case 1:  android (c);  break;
        case 2:  choirBot (c); break;
        case 3:  titan (c);    break;
        case 4:  alien (c);    break;
        case 5:  ghost (c);    break;
        case 6:  disco (c);    break;
        default: eightBit (c); break;
    }
}
