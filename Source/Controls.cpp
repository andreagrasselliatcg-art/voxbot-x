#include "Controls.h"
#include "Presets.h"

namespace
{
    constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;
    constexpr float kEnd   = juce::MathConstants<float>::pi * 2.75f;

    void strokeArc (juce::Graphics& g, juce::Point<float> c, float r, float a0, float a1, float width)
    {
        if (std::abs (a1 - a0) < 0.001f) return;
        juce::Path p;
        p.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
ParamKnob::ParamKnob (UiShared& u, const juce::String& paramId, const juce::String& l, int t, Style s)
    : ui (u), param (u.param (paramId)), target (t), style (s), label (l)
{
    jassert (param != nullptr);
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        norm = param->convertTo0to1 (v);
        repaint();
    });
    attachment->sendInitialUpdate();

    if (target >= 0)
    {
        for (int sl = 0; sl < Params::kSlots; ++sl)
        {
            auto* p = u.param (Params::amountId (sl, target));
            amountParams[(size_t) sl] = p;
            amountAttachments[(size_t) sl] = std::make_unique<juce::ParameterAttachment> (*p, [this, sl] (float v)
            {
                amounts[(size_t) sl] = v;
                repaint();
            });
            amountAttachments[(size_t) sl]->sendInitialUpdate();
        }
    }

    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void ParamKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const bool armed = editingAmount();
    const auto armedCol = armed ? Params::slotColours[(size_t) ui.armedSlot] : juce::Colour();
    const auto col = arcColour();

    // Text: label, or value while hovering/dragging, or depth while assigning
    juce::String text = label;
    if (armed && (hover || dragging))
    {
        const float a = amounts[(size_t) ui.armedSlot];
        text = "M" + juce::String (ui.armedSlot + 1) + " " + (a >= 0 ? "+" : "") + juce::String (juce::roundToInt (a * 100.0f)) + "%";
    }
    else if (hover || dragging) text = param->getCurrentValueAsText();

    const float textH = style == mini ? 12.0f : 14.0f;
    auto textArea = b.removeFromBottom (textH);

    if (style == vslider)
    {
        auto track = b.reduced (b.getWidth() * 0.5f - 3.0f, 6.0f).translated (-3.0f, 0.0f);
        g.setColour (Palette::ledOff);
        g.fillRoundedRectangle (track, 3.0f);
        auto fill = track.withTop (track.getBottom() - track.getHeight() * norm);
        g.setColour (col.withAlpha (0.85f));
        g.fillRoundedRectangle (fill, 3.0f);

        if (armed)
        {
            g.setColour (armedCol.withAlpha (0.35f));
            g.drawRoundedRectangle (track.expanded (5.0f, 3.0f), 4.0f, 1.5f);
        }
        // mod bars
        for (int s = 0; s < Params::kSlots; ++s)
        {
            if (target < 0 || std::abs (amounts[(size_t) s]) < 1.0e-6f || ! ui.slotOn (s)) continue;
            const float y0 = track.getBottom() - track.getHeight() * norm;
            const float y1 = juce::jlimit (track.getY(), track.getBottom(), y0 - track.getHeight() * amounts[(size_t) s]);
            g.setColour (Params::slotColours[(size_t) s]);
            g.fillRect (juce::Rectangle<float> (track.getRight() + 2.0f + 2.5f * (float) s, juce::jmin (y0, y1), 2.0f, std::abs (y1 - y0)));
        }
        const float ty = track.getBottom() - track.getHeight() * norm;
        auto thumb = juce::Rectangle<float> (22.0f, 8.0f).withCentre ({ track.getCentreX(), ty });
        g.setColour (juce::Colour (0xff2c2c33));
        g.fillRoundedRectangle (thumb, 2.5f);
        g.setColour (Palette::textMain);
        g.fillRect (thumb.reduced (thumb.getWidth() * 0.2f, 3.4f));
    }
    else
    {
        const float size = juce::jmin (b.getWidth(), b.getHeight());
        const float R = size * 0.5f;
        const auto c = b.getCentre();
        const float trackR = R * 0.76f;

        if (armed)
        {
            g.setColour (armedCol.withAlpha (0.12f));
            g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (c));
            g.setColour (armedCol.withAlpha (0.55f));
            g.drawEllipse (juce::Rectangle<float> (size - 2.0f, size - 2.0f).withCentre (c), 1.2f);
        }

        g.setColour (Palette::ledOff);
        strokeArc (g, c, trackR, kStart, kEnd, style == mini ? 2.5f : 3.0f);

        const bool bipolar = param->convertFrom0to1 (0.0f) < 0.0f && param->convertFrom0to1 (1.0f) > 0.0f;
        const float zeroNorm = bipolar ? param->convertTo0to1 (0.0f) : 0.0f;
        const float a0 = kStart + zeroNorm * (kEnd - kStart);
        const float av = kStart + norm * (kEnd - kStart);
        g.setColour (col.withAlpha (0.22f));
        strokeArc (g, c, trackR, a0, av, style == mini ? 5.0f : 7.0f);
        g.setColour (col);
        strokeArc (g, c, trackR, a0, av, style == mini ? 2.5f : 3.0f);

        // Modulation depth rings
        if (target >= 0)
        {
            for (int s = 0; s < Params::kSlots; ++s)
            {
                const float a = amounts[(size_t) s];
                if (std::abs (a) < 1.0e-6f) continue;
                const bool on = ui.slotOn (s);
                const float r = R * 0.86f + (float) s * R * 0.045f;
                const float aEnd = kStart + juce::jlimit (0.0f, 1.0f, norm + a) * (kEnd - kStart);
                g.setColour (Params::slotColours[(size_t) s].withAlpha (on ? 0.95f : 0.3f));
                strokeArc (g, c, r, av, aEnd, 1.6f);
            }

            // live modulated position
            const float live = ui.proc.modulatedNorm[(size_t) target].load();
            if (std::abs (live - norm) > 0.002f)
            {
                const auto p = c.getPointOnCircumference (trackR, kStart + live * (kEnd - kStart));
                g.setColour (juce::Colours::white);
                g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (p));
            }
        }

        const float knobR = R * 0.58f;
        VoxBotLookAndFeel::drawKnobBody (g, c, knobR);
        juce::Path pointer;
        const float pw = juce::jmax (2.0f, knobR * 0.11f);
        pointer.addRoundedRectangle (-pw * 0.5f, -knobR * 0.82f, pw, knobR * 0.40f, pw * 0.5f);
        g.setColour (juce::Colour (0xffe0e0e6));
        g.fillPath (pointer, juce::AffineTransform::rotation (av).translated (c.x, c.y));
    }

    g.setColour (armed ? armedCol : (hover || dragging ? Palette::textMain : Palette::textDim));
    g.setFont (juce::FontOptions (style == knob ? 10.5f : 9.5f, juce::Font::bold));
    g.drawText (text, textArea.expanded (12.0f, 0.0f).toNearestInt(), juce::Justification::centred, false);
}

void ParamKnob::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    if (e.mods.isAltDown()) { mouseDoubleClick (e); return; }

    dragSlot = editingAmount() ? ui.armedSlot : -1;
    if (dragSlot >= 0)
    {
        dragStart = amounts[(size_t) dragSlot];
        amountAttachments[(size_t) dragSlot]->beginGesture();
    }
    else
    {
        dragStart = norm;
        attachment->beginGesture();
    }
}

void ParamKnob::mouseDrag (const juce::MouseEvent& e)
{
    const float sens = e.mods.isShiftDown() ? 0.2f : 1.0f;
    float delta;
    if (style == vslider) delta = -(float) e.getDistanceFromDragStartY() / juce::jmax (40.0f, (float) getHeight() - 26.0f);
    else                  delta = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / 180.0f;
    delta *= sens;

    if (dragSlot >= 0)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, dragStart + delta * 2.0f);
        amountAttachments[(size_t) dragSlot]->setValueAsPartOfGesture (v);
    }
    else
    {
        const float v = juce::jlimit (0.0f, 1.0f, dragStart + delta);
        attachment->setValueAsPartOfGesture (param->convertFrom0to1 (v));
    }
}

void ParamKnob::mouseUp (const juce::MouseEvent&)
{
    if (dragSlot >= 0) amountAttachments[(size_t) dragSlot]->endGesture();
    else               attachment->endGesture();
    dragSlot = -1;
    dragging = false;
    repaint();
}

void ParamKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (editingAmount()) amountAttachments[(size_t) ui.armedSlot]->setValueAsCompleteGesture (0.0f);
    else                 attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
}

void ParamKnob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = (w.isReversed ? -w.deltaY : w.deltaY) * (e.mods.isShiftDown() ? 0.02f : 0.1f);
    if (editingAmount())
        amountAttachments[(size_t) ui.armedSlot]->setValueAsCompleteGesture (juce::jlimit (-1.0f, 1.0f, amounts[(size_t) ui.armedSlot] + d * 2.0f));
    else
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm + d)));
}

//==============================================================================
ParamToggle::ParamToggle (UiShared& u, const juce::String& paramId, const juce::String& t)
    : ui (u), param (u.param (paramId)), text (t)
{
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { on = v > 0.5f; repaint(); });
    attachment->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ParamToggle::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const auto col = colour.getARGB() == 0 ? ui.accent : colour;
    g.setColour (on ? col.withAlpha (0.22f) : juce::Colour (0xff0d0d10));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (on ? col : Palette::panelLine);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
    g.setColour (on ? col.brighter (0.4f) : Palette::textDim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText (text, b, juce::Justification::centred);
}

void ParamToggle::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()))
        attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

//==============================================================================
ParamSegment::ParamSegment (UiShared& u, const juce::String& paramId, juce::StringArray l)
    : ui (u), param (u.param (paramId)), labels (std::move (l))
{
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { index = juce::roundToInt (v); repaint(); });
    attachment->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ParamSegment::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const auto col = colour.getARGB() == 0 ? ui.accent : colour;
    g.setColour (juce::Colour (0xff0d0d10));
    g.fillRoundedRectangle (b, 4.0f);
    const float w = b.getWidth() / (float) labels.size();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto seg = juce::Rectangle<float> (b.getX() + w * (float) i, b.getY(), w, b.getHeight());
        if (i == index)
        {
            g.setColour (col.withAlpha (0.25f));
            g.fillRoundedRectangle (seg.reduced (1.5f), 3.0f);
        }
        g.setColour (i == index ? col.brighter (0.4f) : Palette::textDim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (labels[i], seg, juce::Justification::centred);
    }
    g.setColour (Palette::panelLine);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
}

void ParamSegment::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    const int i = juce::jlimit (0, labels.size() - 1, (int) ((float) e.x / (float) getWidth() * (float) labels.size()));
    attachment->setValueAsCompleteGesture ((float) i);
}

//==============================================================================
ParamCombo::ParamCombo (UiShared& u, const juce::String& paramId)
{
    auto* p = u.param (paramId);
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
        addItemList (choice->choices, 1);
    attachment = std::make_unique<juce::ComboBoxParameterAttachment> (*p, *this);
}

//==============================================================================
void ClickButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (latched ? colour.withAlpha (0.22f) : juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (hover || latched ? colour.withAlpha (0.9f) : Palette::panelLine);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    g.setColour (hover || latched ? colour : Palette::textMain.withAlpha (0.75f));
    if (arrowDir != 0)
    {
        const auto c = b.getCentre();
        const float s = juce::jmin (b.getWidth(), b.getHeight()) * 0.18f;
        juce::Path tri;
        if (arrowDir > 0) tri.addTriangle (c.x - s * 0.7f, c.y - s, c.x - s * 0.7f, c.y + s, c.x + s, c.y);
        else              tri.addTriangle (c.x + s * 0.7f, c.y - s, c.x + s * 0.7f, c.y + s, c.x - s, c.y);
        g.fillPath (tri);
    }
    else
    {
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (text, b, juce::Justification::centred);
    }
}

//==============================================================================
void FaceSelector::paint (juce::Graphics& g)
{
    VoxBotLookAndFeel::drawFaceKnob (g, getLocalBounds().toFloat(), ui.proc.currentPreset.load(),
                                     (int) Presets::all().size());
}

void FaceSelector::step (int delta)
{
    const int n = (int) Presets::all().size();
    const int next = (ui.proc.currentPreset.load() + delta + n) % n;
    ui.proc.applyPreset (next);
    repaint();
}

void FaceSelector::mouseDrag (const juce::MouseEvent& e)
{
    const float y = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
    accum += y - lastY;
    lastY = y;
    const float stepPx = 30.0f;
    const int n = (int) Presets::all().size();
    while (accum > stepPx)  { accum -= stepPx; if (ui.proc.currentPreset.load() < n - 1) step (+1); }
    while (accum < -stepPx) { accum += stepPx; if (ui.proc.currentPreset.load() > 0)     step (-1); }
}

void FaceSelector::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = w.isReversed ? -w.deltaY : w.deltaY;
    if (d > 0.0f) step (+1); else if (d < 0.0f) step (-1);
}

//==============================================================================
void StepEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const auto col = Params::slotColours[(size_t) slot];
    g.setColour (juce::Colour (0xff0b0b0e));
    g.fillRoundedRectangle (b, 4.0f);

    const float midY = b.getCentreY();
    g.setColour (Palette::panelLine);
    for (float q : { 0.25f, 0.75f }) g.drawHorizontalLine ((int) (b.getY() + b.getHeight() * q), b.getX() + 2, b.getRight() - 2);
    g.setColour (Palette::textDim.withAlpha (0.5f));
    g.drawHorizontalLine ((int) midY, b.getX() + 2, b.getRight() - 2);

    const float w = b.getWidth() / (float) Params::kSeqSteps;
    const float pos = ui.proc.seqPosition[(size_t) slot].load();
    const bool running = ui.slotOn (slot);
    const int playing = running ? (int) pos : -1;

    for (int i = 0; i < Params::kSeqSteps; ++i)
    {
        const float v = ui.proc.getSeqStep (slot, i);
        auto cell = juce::Rectangle<float> (b.getX() + w * (float) i, b.getY(), w, b.getHeight()).reduced (1.5f, 3.0f);
        if (i % 4 == 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.025f));
            g.fillRect (cell.withWidth (w * 4.0f - 3.0f));
        }
        const float y = midY - v * (cell.getHeight() * 0.5f);
        auto bar = juce::Rectangle<float> (cell.getX(), juce::jmin (y, midY), cell.getWidth(), std::abs (y - midY));
        g.setColour (col.withAlpha (i == playing ? 1.0f : 0.55f));
        g.fillRect (bar);
        g.fillRect (cell.getX(), y - 1.0f, cell.getWidth(), 2.0f);
    }

    g.setColour (Palette::panelLine);
    g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
}

void StepEditor::edit (const juce::MouseEvent& e, bool interpolate)
{
    const float w = (float) getWidth() / (float) Params::kSeqSteps;
    const int step = juce::jlimit (0, Params::kSeqSteps - 1, (int) ((float) e.x / w));
    float v = juce::jlimit (-1.0f, 1.0f, 1.0f - 2.0f * ((float) e.y - 3.0f) / ((float) getHeight() - 6.0f));

    if (e.mods.isCommandDown())    v = 0.0f;
    else if (e.mods.isShiftDown()) v = std::round (v * 4.0f) / 4.0f;
    else if (e.mods.isAltDown())   v = std::round (v * 12.0f) / 12.0f;

    if (interpolate && lastStep >= 0 && lastStep != step)
    {
        const int dir = step > lastStep ? 1 : -1;
        for (int i = lastStep + dir; i != step; i += dir)
        {
            const float t = (float) (i - lastStep) / (float) (step - lastStep);
            ui.proc.setSeqStep (slot, i, lastValue + t * (v - lastValue));
        }
    }
    ui.proc.setSeqStep (slot, step, v);
    lastStep = step; lastValue = v;
    repaint();
}

void StepEditor::mouseDown (const juce::MouseEvent& e) { lastStep = -1; edit (e, false); }
void StepEditor::mouseDrag (const juce::MouseEvent& e) { edit (e, true); }

void StepEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const float w = (float) getWidth() / (float) Params::kSeqSteps;
    ui.proc.setSeqStep (slot, juce::jlimit (0, Params::kSeqSteps - 1, (int) ((float) e.x / w)), 0.0f);
    repaint();
}
