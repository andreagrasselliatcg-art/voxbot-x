#include "Params.h"
#include "VocoderEngine.h"

namespace
{
    using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

    juce::NormalisableRange<float> msRange (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi, 0.1f);
        r.setSkewForCentre (centre);
        return r;
    }

    std::unique_ptr<juce::AudioParameterFloat> floatParam (const juce::String& id, const juce::String& name,
                                                           juce::NormalisableRange<float> range, float def,
                                                           const juce::String& unit, int decimals = 1)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def,
            juce::AudioParameterFloatAttributes()
                .withLabel (unit)
                .withStringFromValueFunction ([unit, decimals] (float v, int)
                {
                    auto s = juce::String (v, decimals);
                    if (unit == "st" || unit == "dB") s = (v > 0.0f ? "+" : "") + s;
                    return unit.isEmpty() ? s : s + " " + unit;
                }));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout Params::createLayout()
{
    Layout layout;

    // ---- Main targets ---------------------------------------------------------
    for (auto& t : targets())
    {
        const int decimals = t.step >= 1.0f ? 0 : (t.step >= 0.1f ? 0 : 2);
        layout.add (floatParam (t.id, t.name, juce::NormalisableRange<float> (t.min, t.max, t.step), t.def, t.unit, decimals));
    }

    // ---- Globals --------------------------------------------------------------
    juce::StringArray robotNames;
    for (auto& p : vbx::presets()) robotNames.add (p.name);

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { robotType, 1 }, "Robot Type", robotNames, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { detect, 1 }, "Detect", detectNames, 1));
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { snap, 1 }, "Snap", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { midiMode, 1 }, "MIDI Mode", midiModeNames, 0));
    layout.add (floatParam (glide, "Glide", msRange (0.0f, 2000.0f, 150.0f), 0.0f, "ms", 0));
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { midiOnlyGlide, 1 }, "MIDI Only Glide", false));
    layout.add (floatParam (gateAttack,  "Gate Attack",  msRange (1.0f, 2000.0f, 100.0f), 10.0f,  "ms", 0));
    layout.add (floatParam (gateRelease, "Gate Release", msRange (1.0f, 4000.0f, 300.0f), 150.0f, "ms", 0));
    layout.add (floatParam (output, "Output", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, "dB"));

    // ---- Modulation slots -----------------------------------------------------
    for (int s = 0; s < kSlots; ++s)
    {
        const juce::String n = "Mod " + juce::String (s + 1) + " ";
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("mod" + juce::String (s + 1), "Mod " + juce::String (s + 1), "|");

        group->addChild (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { slotId (s, "on"), 1 }, n + "On", false));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { slotId (s, "type"), 1 }, n + "Source", sourceNames, s == 1 ? srcSequencer : s == 2 ? srcFollower : s == 3 ? srcAdsr : srcMeta));

        group->addChild (floatParam (slotId (s, "meta"), n + "Meta", juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f, "", 3));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { slotId (s, "metaPol"), 1 }, n + "Meta Polarity", polarityNames, 0));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { slotId (s, "metaSteps"), 1 }, n + "Meta Steps", stepNames, 0));

        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { slotId (s, "midiSrc"), 1 }, n + "MIDI Source", midiSrcNames, 0));

        group->addChild (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { slotId (s, "seqSync"), 1 }, n + "Seq Sync", true));
        group->addChild (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { slotId (s, "seqRetrig"), 1 }, n + "Seq Retrig", false));
        group->addChild (floatParam (slotId (s, "seqRate"), n + "Seq Rate", msRange (0.1f, 50.0f, 4.0f), 4.0f, "Hz", 2));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { slotId (s, "seqDiv"), 1 }, n + "Seq Division", divNames, 4));
        group->addChild (floatParam (slotId (s, "seqShape"), n + "Seq Shape", juce::NormalisableRange<float> (0.0f, 4.0f, 0.01f), 2.0f, "", 2));

        group->addChild (floatParam (slotId (s, "folAtk"), n + "Follower Attack",  msRange (0.1f, 500.0f, 30.0f), 5.0f, "ms", 1));
        group->addChild (floatParam (slotId (s, "folRel"), n + "Follower Release", msRange (1.0f, 2000.0f, 200.0f), 120.0f, "ms", 0));

        group->addChild (floatParam (slotId (s, "adsrA"), n + "Attack",  msRange (1.0f, 5000.0f, 300.0f), 10.0f,  "ms", 0));
        group->addChild (floatParam (slotId (s, "adsrD"), n + "Decay",   msRange (1.0f, 5000.0f, 300.0f), 300.0f, "ms", 0));
        group->addChild (floatParam (slotId (s, "adsrS"), n + "Sustain", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f, "%", 0));
        group->addChild (floatParam (slotId (s, "adsrR"), n + "Release", msRange (1.0f, 5000.0f, 300.0f), 400.0f, "ms", 0));

        for (int t = 0; t < kNumTargets; ++t)
            group->addChild (floatParam (amountId (s, t), n + "> " + targets()[(size_t) t].name,
                                         juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, "", 3));

        for (int o = 0; o < kSlots; ++o)
            if (o != s)
                group->addChild (floatParam (crossId (s, o), n + "x Mod " + juce::String (o + 1),
                                             juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f, "", 3));

        layout.add (std::move (group));
    }

    return layout;
}
