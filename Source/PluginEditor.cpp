#include "PluginEditor.h"
#include "Presets.h"

using namespace Params;

namespace
{
    constexpr int editorW = 1100;
    constexpr int editorH = 760;

    const juce::Rectangle<int> facePanel   { 16, 72, 290, 300 };
    const juce::Rectangle<int> pitchPanel  { 318, 72, 250, 150 };
    const juce::Rectangle<int> harmPanel   { 580, 72, 260, 150 };
    const juce::Rectangle<int> altPanel    { 852, 72, 232, 150 };
    const juce::Rectangle<int> fxPanel     { 318, 232, 250, 140 };
    const juce::Rectangle<int> robotPanel  { 580, 232, 160, 140 };
    const juce::Rectangle<int> midiPanel   { 752, 232, 200, 140 };
    const juce::Rectangle<int> masterPanel { 964, 232, 120, 140 };
    constexpr int slotsY = 384, slotW = 258, slotH = 362;

    void smallLabel (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, juce::Justification j = juce::Justification::centredLeft)
    {
        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawText (t, r, j);
    }
}

//==============================================================================
ModSlotPanel::ModSlotPanel (UiShared& u, int s)
    : ui (u), slot (s), col (slotColours[(size_t) s]),
      on (u, slotId (s, "on"), "ON"),
      type (u, slotId (s, "type")),
      meta (u, slotId (s, "meta"), "VALUE"),
      polarity (u, slotId (s, "metaPol"), { "+", "-", "B" }),
      steps (u, slotId (s, "metaSteps")),
      midiSrc (u, slotId (s, "midiSrc")),
      stepEditor (u, s),
      sync (u, slotId (s, "seqSync"), "SYNC"),
      retrig (u, slotId (s, "seqRetrig"), "RETRIG"),
      rate (u, slotId (s, "seqRate"), "RATE"),
      division (u, slotId (s, "seqDiv"), "RATE"),
      shape (u, slotId (s, "seqShape"), "SHAPE"),
      folAtk (u, slotId (s, "folAtk"), "ATTACK"),
      folRel (u, slotId (s, "folRel"), "RELEASE"),
      adsrA (u, slotId (s, "adsrA"), "A"),
      adsrD (u, slotId (s, "adsrD"), "D"),
      adsrS (u, slotId (s, "adsrS"), "S"),
      adsrR (u, slotId (s, "adsrR"), "R")
{
    on.colour = col; polarity.colour = col; sync.colour = col; retrig.colour = col;
    assign.colour = col;
    for (auto* k : { &meta, &rate, &division, &shape, &folAtk, &folRel, &adsrA, &adsrD, &adsrS, &adsrR })
    {
        k->colour = col;
        addChildComponent (*k);
    }

    addAndMakeVisible (on);
    addAndMakeVisible (type);
    addAndMakeVisible (assign);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &polarity, &steps, &midiSrc, &stepEditor, &patPrev, &patNext, &sync, &retrig })
        addChildComponent (c);

    assign.onClick = [this] { if (onAssign) onAssign(); };
    patPrev.colour = patNext.colour = col;
    patPrev.onClick = [this] { ui.proc.loadPattern (slot, (ui.proc.getPattern (slot) + 100) % 101); repaint(); };
    patNext.onClick = [this] { ui.proc.loadPattern (slot, (ui.proc.getPattern (slot) + 1) % 101); repaint(); };

    for (int o = 0; o < kSlots; ++o)
    {
        if (o == slot) continue;
        auto k = std::make_unique<ParamKnob> (ui, crossId (slot, o), "M" + juce::String (o + 1), -1, ParamKnob::mini);
        k->colour = slotColours[(size_t) o];
        addAndMakeVisible (*k);
        cross.push_back (std::move (k));
    }

    refresh();
}

void ModSlotPanel::refresh()
{
    const int t = juce::roundToInt (ui.param (slotId (slot, "type"))->convertFrom0to1 (ui.param (slotId (slot, "type"))->getValue()));
    const bool syncOn = ui.param (slotId (slot, "seqSync"))->getValue() > 0.5f;

    if (t != shownType || syncOn != shownSync)
    {
        shownType = t; shownSync = syncOn;
        meta.setVisible (t == srcMeta); polarity.setVisible (t == srcMeta); steps.setVisible (t == srcMeta);
        midiSrc.setVisible (t == srcMidi);
        for (juce::Component* c : std::initializer_list<juce::Component*> { &stepEditor, &patPrev, &patNext, &sync, &retrig, &shape })
            c->setVisible (t == srcSequencer);
        rate.setVisible (t == srcSequencer && ! syncOn);
        division.setVisible (t == srcSequencer && syncOn);
        folAtk.setVisible (t == srcFollower); folRel.setVisible (t == srcFollower);
        for (auto* k : { &adsrA, &adsrD, &adsrS, &adsrR }) k->setVisible (t == srcAdsr);
    }

    if (t == srcSequencer) stepEditor.repaint();
    repaint (0, 40, getWidth(), getHeight() - 40);
}

void ModSlotPanel::resized()
{
    on.setBounds (10, 9, 36, 22);
    type.setBounds (100, 9, 94, 22);
    assign.setBounds (198, 9, 50, 22);

    meta.setBounds (getWidth() / 2 - 52, 46, 104, 114);
    polarity.setBounds (24, 182, 96, 22);
    steps.setBounds (140, 182, 94, 22);

    midiSrc.setBounds (34, 70, 190, 24);

    stepEditor.setBounds (10, 44, 238, 96);
    patPrev.setBounds (10, 148, 22, 22);
    patNext.setBounds (90, 148, 22, 22);
    sync.setBounds (122, 148, 56, 22);
    retrig.setBounds (184, 148, 64, 22);
    rate.setBounds (40, 178, 74, 82);
    division.setBounds (40, 178, 74, 82);
    shape.setBounds (144, 178, 74, 82);

    folAtk.setBounds (30, 56, 84, 96);
    folRel.setBounds (144, 56, 84, 96);

    for (int i = 0; i < 4; ++i)
        std::array<ParamKnob*, 4> { &adsrA, &adsrD, &adsrS, &adsrR }[(size_t) i]->setBounds (12 + i * 59, 48, 58, 74);

    for (size_t i = 0; i < cross.size(); ++i)
        cross[i]->setBounds (64 + (int) i * 62, 292, 54, 62);
}

void ModSlotPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    VoxBotLookAndFeel::drawPanel (g, b, {}, col);
    const bool active = ui.slotOn (slot);

    // header
    g.setColour (col.withAlpha (active ? 1.0f : 0.35f));
    g.fillRoundedRectangle (b.getX() + 1.0f, b.getY() + 1.0f, b.getWidth() - 2.0f, 3.0f, 1.5f);
    g.setColour (active ? col : Palette::textDim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("MOD " + juce::String (slot + 1), 50, 9, 48, 22, juce::Justification::centred);

    const float out = ui.proc.slotOutput[(size_t) slot].load();

    switch (shownType)
    {
        case srcMeta:
            smallLabel (g, "POLARITY", { 24, 168, 96, 12 });
            smallLabel (g, "STEPS", { 140, 168, 94, 12 });
            break;
        case srcMidi:
        {
            smallLabel (g, "MIDI MESSAGE", { 34, 54, 190, 12 });
            auto bar = juce::Rectangle<float> (34.0f, 130.0f, 190.0f, 14.0f);
            g.setColour (juce::Colour (0xff0b0b0e)); g.fillRoundedRectangle (bar, 3.0f);
            const float cx = bar.getCentreX();
            const float v = juce::jlimit (-1.0f, 1.0f, out);
            g.setColour (col);
            g.fillRect (juce::Rectangle<float> (v >= 0 ? cx : cx + v * bar.getWidth() * 0.5f, bar.getY() + 2.0f,
                                                std::abs (v) * bar.getWidth() * 0.5f, bar.getHeight() - 4.0f));
            g.setColour (Palette::panelLine); g.drawRoundedRectangle (bar, 3.0f, 1.0f);
            const int note = ui.proc.lastNote.load();
            smallLabel (g, note >= 0 ? "NOTE " + juce::MidiMessage::getMidiNoteName (note, true, true, 3) : "NO NOTE",
                        { 34, 150, 190, 14 }, juce::Justification::centred);
            break;
        }
        case srcSequencer:
        {
            const int p = ui.proc.getPattern (slot);
            g.setColour (Palette::textMain);
            g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::bold));
            g.drawText (p >= 100 ? "RNDM" : "P " + juce::String (p).paddedLeft ('0', 2), 32, 148, 58, 22, juce::Justification::centred);
            break;
        }
        case srcFollower:
        {
            auto bar = juce::Rectangle<float> (30.0f, 168.0f, 198.0f, 10.0f);
            g.setColour (juce::Colour (0xff0b0b0e)); g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (col); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, std::abs (out))), 3.0f);
            smallLabel (g, "INPUT LEVEL", { 30, 182, 198, 12 }, juce::Justification::centred);
            break;
        }
        case srcAdsr:
        {
            auto area = juce::Rectangle<float> (16.0f, 134.0f, 226.0f, 110.0f);
            g.setColour (juce::Colour (0xff0b0b0e)); g.fillRoundedRectangle (area, 4.0f);
            auto pv = [this] (const char* n) { auto* p = ui.param (slotId (slot, n)); return p->convertFrom0to1 (p->getValue()); };
            const float a = pv ("adsrA"), d = pv ("adsrD"), su = pv ("adsrS") * 0.01f, r = pv ("adsrR");
            const float total = a + d + r + 300.0f;
            auto in = area.reduced (8.0f, 10.0f);
            auto X = [&] (float ms) { return in.getX() + in.getWidth() * ms / total; };
            auto Y = [&] (float v)  { return in.getBottom() - in.getHeight() * v; };
            juce::Path env;
            env.startNewSubPath (X (0), Y (0));
            env.lineTo (X (a), Y (1));
            env.lineTo (X (a + d), Y (su));
            env.lineTo (X (a + d + 300.0f), Y (su));
            env.lineTo (X (total), Y (0));
            g.setColour (col.withAlpha (0.15f));
            juce::Path fill (env); fill.lineTo (X (0), Y (0)); fill.closeSubPath(); g.fillPath (fill);
            g.setColour (col);
            g.strokePath (env, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ in.getRight() - 4.0f, Y (juce::jlimit (0.0f, 1.0f, out)) }));
            break;
        }
        default: break;
    }

    // Output meter (bipolar)
    auto meterArea = juce::Rectangle<float> (10.0f, 266.0f, b.getWidth() - 20.0f, 6.0f);
    g.setColour (juce::Colour (0xff0b0b0e)); g.fillRoundedRectangle (meterArea, 3.0f);
    const float cx = meterArea.getCentreX();
    const float v = juce::jlimit (-1.0f, 1.0f, out);
    g.setColour (col.withAlpha (active ? 1.0f : 0.3f));
    g.fillRect (juce::Rectangle<float> (v >= 0 ? cx : cx + v * meterArea.getWidth() * 0.5f, meterArea.getY(),
                                        std::abs (v) * meterArea.getWidth() * 0.5f, meterArea.getHeight()));
    g.setColour (Palette::textDim.withAlpha (0.5f));
    g.drawVerticalLine ((int) cx, meterArea.getY() - 1.0f, meterArea.getBottom() + 1.0f);

    smallLabel (g, "CROSS MOD (scales this slot)", { 12, 278, 236, 12 }, juce::Justification::centred);
}

//==============================================================================
VoxBotAudioProcessorEditor::VoxBotAudioProcessorEditor (VoxBotAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), ui (p),
      detect (ui, Params::detect, detectNames),
      snap (ui, Params::snap, "SNAP"),
      robotType (ui, Params::robotType),
      midiMode (ui, Params::midiMode),
      midiGlide (ui, Params::midiOnlyGlide, "M")
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (face);
    for (auto* b : { &prevPreset, &nextPreset, &initButton }) addAndMakeVisible (*b);
    prevPreset.onClick = [this] { face.step (-1); };
    nextPreset.onClick = [this] { face.step (+1); };
    initButton.onClick = [this] { processorRef.initPatch(); };

    addAndMakeVisible (detect);
    addAndMakeVisible (snap);
    addAndMakeVisible (robotType);
    addAndMakeVisible (midiMode);
    addAndMakeVisible (midiGlide);

    for (int t = 0; t < kNumTargets; ++t)
    {
        auto style = t == tSmooth ? ParamKnob::vslider : ParamKnob::knob;
        targetKnobs[(size_t) t] = &addKnob (targets()[(size_t) t].id, targets()[(size_t) t].shortName, t, style);
    }
    glide   = &addKnob (Params::glide, "GLIDE", -1);
    gateAtk = &addKnob (Params::gateAttack, "ATTACK", -1);
    gateRel = &addKnob (Params::gateRelease, "RELEASE", -1);
    output  = &addKnob (Params::output, "OUTPUT", -1);
    output->colour = juce::Colour (0xffd0d0d8);

    for (int s = 0; s < kSlots; ++s)
    {
        auto panel = std::make_unique<ModSlotPanel> (ui, s);
        panel->onAssign = [this, s] { armSlot (ui.armedSlot == s ? -1 : s); };
        addAndMakeVisible (*panel);
        slots.push_back (std::move (panel));
    }

    setSize (editorW, editorH);
    startTimerHz (30);
    timerCallback();
}

VoxBotAudioProcessorEditor::~VoxBotAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

ParamKnob& VoxBotAudioProcessorEditor::addKnob (const juce::String& id, const juce::String& label, int target, ParamKnob::Style style)
{
    knobs.push_back (std::make_unique<ParamKnob> (ui, id, label, target, style));
    addAndMakeVisible (*knobs.back());
    return *knobs.back();
}

void VoxBotAudioProcessorEditor::armSlot (int slot)
{
    ui.armedSlot = slot;
    for (int s = 0; s < kSlots; ++s) slots[(size_t) s]->setArmed (s == slot);
    for (auto& k : knobs) k->repaint();
    repaint();
}

void VoxBotAudioProcessorEditor::timerCallback()
{
    const int preset = processorRef.currentPreset.load();
    if (preset != shownPreset)
    {
        shownPreset = preset;
        ui.accent = VoxBotLookAndFeel::presetColour (preset);
        for (auto* c : std::initializer_list<juce::Component*> { &detect, &snap, &midiGlide }) c->repaint();
        for (auto& k : knobs) k->repaint();
        face.repaint();
        repaint();
    }

    const int mode = juce::roundToInt (ui.param (Params::midiMode)->convertFrom0to1 (ui.param (Params::midiMode)->getValue()));
    const bool gated = mode == vbx::midiMonoGated || mode == vbx::midiPolyGated;
    if (gated != shownGated)
    {
        shownGated = gated;
        gateAtk->setVisible (gated);
        gateRel->setVisible (gated);
    }

    for (auto* k : targetKnobs) k->repaint();
    for (auto& s : slots) s->refresh();
    repaint (980, 12, 104, 50);
}

void VoxBotAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto accent = ui.accent;

    g.setGradientFill (juce::ColourGradient (Palette::bgTop, 0.0f, 0.0f, Palette::bgBottom, 0.0f, bounds.getHeight(), false));
    g.fillAll();
    g.setColour (juce::Colours::white.withAlpha (0.012f));
    for (int yy = 0; yy < getHeight(); yy += 3) g.drawHorizontalLine (yy, 0.0f, bounds.getWidth());

    // ---- Header -----------------------------------------------------------------
    g.setColour (Palette::textMain);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("VOXBOT", 24, 12, 130, 32, juce::Justification::centredLeft);
    g.setColour (accent);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    auto badge = juce::Rectangle<float> (156.0f, 19.0f, 34.0f, 17.0f);
    g.drawRoundedRectangle (badge, 3.0f, 1.2f);
    g.drawText ("- X", badge, juce::Justification::centred);
    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("TUNE TALKS - v2.0", 25, 44, 200, 14, juce::Justification::centredLeft);

    auto nameBox = juce::Rectangle<float> (394.0f, 18.0f, 230.0f, 30.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (nameBox, 5.0f);
    g.setColour (accent.withAlpha (0.5f));
    g.drawRoundedRectangle (nameBox, 5.0f, 1.0f);
    g.setColour (accent);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::bold));
    g.drawText (processorRef.getProgramName (shownPreset), nameBox, juce::Justification::centred);

    smallLabel (g, "PITCH DETECT", { 760, 6, 140, 12 });

    // Audio / MIDI indicator
    {
        const float lvl = processorRef.inputLevel.load();
        const float db = juce::Decibels::gainToDecibels (lvl, -60.0f);
        auto meter = juce::Rectangle<float> (984.0f, 22.0f, 60.0f, 8.0f);
        g.setColour (juce::Colour (0xff0b0b0e)); g.fillRoundedRectangle (meter, 2.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f)), 2.0f);
        const bool midiOn = processorRef.notesHeld.load() > 0;
        g.setColour (midiOn ? accent : Palette::ledOff);
        g.fillEllipse (1052.0f, 20.0f, 12.0f, 12.0f);
        const float hz = processorRef.detectedHz.load();
        smallLabel (g, "IN", { 984, 8, 30, 12 });
        smallLabel (g, "MIDI", { 1046, 8, 40, 12 });
        smallLabel (g, hz > 0.0f ? juce::String (juce::roundToInt (hz)) + " Hz" : "-- Hz", { 984, 34, 100, 14 });
    }

    // ---- Panels -------------------------------------------------------------------
    VoxBotLookAndFeel::drawPanel (g, facePanel.toFloat(), "ROBOT - PRESET", accent);
    VoxBotLookAndFeel::drawPanel (g, pitchPanel.toFloat(), "PITCH SHIFTER", accent);
    VoxBotLookAndFeel::drawPanel (g, harmPanel.toFloat(), "HARMONIC SHIFTER", accent);
    VoxBotLookAndFeel::drawPanel (g, altPanel.toFloat(), "GRAIN ALTERNATOR", accent);
    VoxBotLookAndFeel::drawPanel (g, fxPanel.toFloat(), "EFFECTS", accent);
    VoxBotLookAndFeel::drawPanel (g, robotPanel.toFloat(), "ROBOT VOCODER", accent);
    VoxBotLookAndFeel::drawPanel (g, midiPanel.toFloat(), "MIDI", accent);
    VoxBotLookAndFeel::drawPanel (g, masterPanel.toFloat(), "MASTER", accent);

    smallLabel (g, "TYPE", { 668, 262, 64, 12 });
    smallLabel (g, "MODE", { 764, 262, 100, 12 });
    smallLabel (g, "MIDI GLIDE", { 878, 262, 66, 12 });

    if (ui.armedSlot >= 0)
    {
        const auto c = slotColours[(size_t) ui.armedSlot];
        g.setColour (c);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("ASSIGN MOD " + juce::String (ui.armedSlot + 1) + ": drag any knob to set its depth - double-click to clear",
                    318, 374, 766, 12, juce::Justification::centredLeft);
    }
}

void VoxBotAudioProcessorEditor::resized()
{
    face.setBounds (31, 104, 260, 260);
    prevPreset.setBounds (360, 18, 30, 30);
    nextPreset.setBounds (628, 18, 30, 30);
    initButton.setBounds (668, 18, 52, 30);
    detect.setBounds (760, 20, 140, 28);
    snap.setBounds (910, 20, 56, 28);

    targetKnobs[tPitch]->setBounds (330, 104, 84, 106);
    targetKnobs[tSmooth]->setBounds (414, 104, 48, 106);
    targetKnobs[tFormant]->setBounds (462, 104, 84, 106);

    targetKnobs[tHarmonics]->setBounds (592, 104, 78, 106);
    targetKnobs[tRatio]->setBounds (672, 104, 78, 106);
    targetKnobs[tFm]->setBounds (752, 104, 78, 106);

    targetKnobs[tAlternator]->setBounds (870, 104, 96, 106);
    targetKnobs[tOctave]->setBounds (972, 104, 96, 106);

    targetKnobs[tSmear]->setBounds (328, 262, 76, 96);
    targetKnobs[tDetune]->setBounds (406, 262, 76, 96);
    targetKnobs[tStereo]->setBounds (484, 262, 76, 96);

    targetKnobs[tRobot]->setBounds (588, 262, 76, 96);
    robotType.setBounds (668, 276, 64, 24);

    midiMode.setBounds (764, 276, 108, 22);
    midiGlide.setBounds (878, 276, 64, 22);
    glide->setBounds (764, 302, 58, 66);
    gateAtk->setBounds (826, 302, 58, 66);
    gateRel->setBounds (888, 302, 58, 66);

    targetKnobs[tMix]->setBounds (970, 268, 54, 88);
    output->setBounds (1026, 268, 54, 88);

    for (int s = 0; s < kSlots; ++s)
        slots[(size_t) s]->setBounds (16 + s * (slotW + 12), slotsY + 6, slotW, slotH);
}
