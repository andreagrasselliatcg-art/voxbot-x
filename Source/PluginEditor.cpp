#include "PluginEditor.h"

namespace
{
    constexpr int editorW = 460;
    constexpr int editorH = 590;

    const juce::Rectangle<int> presetKnobArea { 70, 98, 320, 320 };
    const juce::Rectangle<int> presetNameArea { 140, 432, 180, 36 };
    const juce::Rectangle<int> mixKnobArea    { 44, 488, 76, 76 };
    const juce::Rectangle<int> outputKnobArea { editorW - 120, 488, 76, 76 };

    constexpr float rotaryStart = juce::MathConstants<float>::pi * 1.25f;
    constexpr float rotaryEnd   = juce::MathConstants<float>::pi * 2.75f;
}

//==============================================================================
void PresetArrow::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (hover ? colour.withAlpha (0.8f) : Palette::panelLine);
    g.drawRoundedRectangle (b, 5.0f, 1.0f);

    const auto c = b.getCentre();
    const float s = b.getHeight() * 0.18f;
    juce::Path tri;
    if (right) tri.addTriangle (c.x - s * 0.7f, c.y - s, c.x - s * 0.7f, c.y + s, c.x + s, c.y);
    else       tri.addTriangle (c.x + s * 0.7f, c.y - s, c.x + s * 0.7f, c.y + s, c.x - s, c.y);
    g.setColour (hover ? colour : Palette::textMain.withAlpha (0.75f));
    g.fillPath (tri);
}

//==============================================================================
VoxBotAudioProcessorEditor::VoxBotAudioProcessorEditor (VoxBotAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lnf);

    for (auto* s : { &presetKnob, &mixKnob, &outputKnob })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (rotaryStart, rotaryEnd, true);
        addAndMakeVisible (*s);
    }

    presetKnob.getProperties().set ("knobStyle", "main");
    presetKnob.setMouseDragSensitivity (360);
    presetKnob.onValueChange = [this] { updateAccent(); repaint(); };

    mixKnob.getProperties().set ("knobStyle", "small");
    mixKnob.getProperties().set ("zeroPos", 0.0);
    mixKnob.setDoubleClickReturnValue (true, 100.0);
    mixKnob.onValueChange = [this] { repaint (mixKnobArea.expanded (30, 30)); };

    outputKnob.getProperties().set ("knobStyle", "small");
    outputKnob.getProperties().set ("zeroPos", 24.0 / 36.0);
    outputKnob.getProperties().set ("accent", (juce::int64) 0xffd0d0d8);
    outputKnob.setDoubleClickReturnValue (true, 0.0);
    outputKnob.onValueChange = [this] { repaint (outputKnobArea.expanded (30, 30)); };

    presetAttachment = std::make_unique<Attachment> (processorRef.apvts, ParamIDs::preset, presetKnob);
    mixAttachment    = std::make_unique<Attachment> (processorRef.apvts, ParamIDs::mix,    mixKnob);
    outputAttachment = std::make_unique<Attachment> (processorRef.apvts, ParamIDs::output, outputKnob);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (+1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    updateAccent();
    setSize (editorW, editorH);
}

VoxBotAudioProcessorEditor::~VoxBotAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void VoxBotAudioProcessorEditor::stepPreset (int delta)
{
    const int n = vbx::kNumPresets;
    const int next = (juce::roundToInt (presetKnob.getValue()) + delta + n) % n;
    presetKnob.setValue ((double) next, juce::sendNotificationSync);
}

void VoxBotAudioProcessorEditor::updateAccent()
{
    const auto col = VoxBotLookAndFeel::presetColour (juce::roundToInt (presetKnob.getValue()));
    mixKnob.getProperties().set ("accent", (juce::int64) col.getARGB());
    prevButton.colour = nextButton.colour = col;
    mixKnob.repaint();
    prevButton.repaint();
    nextButton.repaint();
}

void VoxBotAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const int  current = juce::roundToInt (presetKnob.getValue());
    const auto accent  = VoxBotLookAndFeel::presetColour (current);

    // ---- Background ---------------------------------------------------------
    g.setGradientFill (juce::ColourGradient (Palette::bgTop, 0.0f, 0.0f,
                                             Palette::bgBottom, 0.0f, bounds.getHeight(), false));
    g.fillAll();

    g.setColour (juce::Colours::white.withAlpha (0.012f));
    for (int yy = 0; yy < getHeight(); yy += 3)
        g.drawHorizontalLine (yy, 0.0f, bounds.getWidth());

    // Soft halo of the current robot colour behind the knob
    const auto kc = presetKnobArea.toFloat().getCentre();
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.10f), kc.x, kc.y,
                                             accent.withAlpha (0.0f), kc.x + 230.0f, kc.y, true));
    g.fillEllipse (juce::Rectangle<float> (460.0f, 460.0f).withCentre (kc));

    g.setColour (Palette::panelLine);
    g.drawRoundedRectangle (bounds.reduced (8.0f), 10.0f, 1.0f);

    // ---- Header -------------------------------------------------------------
    g.setColour (Palette::textMain);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("VOXBOT", juce::Rectangle<int> (0, 24, editorW, 34), juce::Justification::centred);

    g.setColour (accent);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    auto badge = juce::Rectangle<float> (editorW * 0.5f - 18.0f, 60.0f, 36.0f, 17.0f);
    g.drawRoundedRectangle (badge, 3.0f, 1.2f);
    g.drawText ("- X", badge, juce::Justification::centred);

    // ---- Preset name display -----------------------------------------------
    auto nameBox = presetNameArea.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (nameBox, 5.0f);
    g.setColour (accent.withAlpha (0.45f));
    g.drawRoundedRectangle (nameBox, 5.0f, 1.0f);
    g.setColour (accent);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::bold));
    g.drawText (vbx::presets()[(size_t) juce::jlimit (0, vbx::kNumPresets - 1, current)].name,
                nameBox, juce::Justification::centred);

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (juce::String (current + 1) + " / " + juce::String (vbx::kNumPresets),
                presetNameArea.withY (presetNameArea.getBottom() + 3).withHeight (13), juce::Justification::centred);

    // ---- Small knob labels ----------------------------------------------------
    auto drawKnobLabels = [&] (juce::Rectangle<int> area, const juce::String& title, const juce::String& value)
    {
        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (title, area.withY (area.getY() - 18).withHeight (14).expanded (20, 0), juce::Justification::centred);
        g.setColour (Palette::textMain);
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.5f, juce::Font::plain));
        g.drawText (value, area.withY (area.getBottom() + 1).withHeight (14).expanded (20, 0), juce::Justification::centred);
    };

    drawKnobLabels (mixKnobArea, "MIX", juce::String (juce::roundToInt (mixKnob.getValue())) + " %");
    const double db = outputKnob.getValue();
    drawKnobLabels (outputKnobArea, "OUTPUT", (db > 0.0 ? "+" : "") + juce::String (db, 1) + " dB");

    g.setColour (Palette::textDim.withAlpha (0.8f));
    g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    g.drawText ("DRY", mixKnobArea.getX() - 22, mixKnobArea.getBottom() - 14, 24, 12, juce::Justification::centredRight);
    g.drawText ("ROBOT", mixKnobArea.getRight() - 4, mixKnobArea.getBottom() - 14, 36, 12, juce::Justification::centredLeft);

    // ---- Manufacturer -----------------------------------------------------------
    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("TUNE TALKS", juce::Rectangle<int> (0, editorH - 54, editorW, 16), juce::Justification::centred);
    g.setColour (Palette::textDim.withAlpha (0.6f));
    g.setFont (juce::FontOptions (9.5f));
    g.drawText ("v1.0", juce::Rectangle<int> (0, editorH - 38, editorW, 12), juce::Justification::centred);
}

void VoxBotAudioProcessorEditor::resized()
{
    presetKnob.setBounds (presetKnobArea);
    mixKnob.setBounds (mixKnobArea);
    outputKnob.setBounds (outputKnobArea);
    prevButton.setBounds (presetNameArea.getX() - 42, presetNameArea.getY(), 36, 36);
    nextButton.setBounds (presetNameArea.getRight() + 6, presetNameArea.getY(), 36, 36);
}
