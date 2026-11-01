#pragma once

#include "Controls.h"

//==============================================================================
/** One modulation slot: source selector, source-specific controls, cross-mod. */
class ModSlotPanel : public juce::Component
{
public:
    ModSlotPanel (UiShared&, int slot);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                // called by the editor timer

    std::function<void()> onAssign;
    void setArmed (bool a) { assign.latched = a; assign.repaint(); }

private:
    UiShared& ui;
    int slot;
    juce::Colour col;
    int shownType = -1;
    bool shownSync = true;

    ParamToggle on;
    ParamCombo type;
    ClickButton assign { "ASSIGN" };

    // Meta
    ParamKnob meta;
    ParamSegment polarity;
    ParamCombo steps;
    // MIDI
    ParamCombo midiSrc;
    // Sequencer
    StepEditor stepEditor;
    ClickButton patPrev { "", -1 }, patNext { "", +1 };
    ParamToggle sync, retrig;
    ParamKnob rate, division, shape;
    // Follower
    ParamKnob folAtk, folRel;
    // ADSR
    ParamKnob adsrA, adsrD, adsrS, adsrR;
    // Cross modulation
    std::vector<std::unique_ptr<ParamKnob>> cross;
};

//==============================================================================
class VoxBotAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxBotAudioProcessorEditor (VoxBotAudioProcessor&);
    ~VoxBotAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void armSlot (int slot);
    ParamKnob& addKnob (const juce::String& id, const juce::String& label, int target, ParamKnob::Style style = ParamKnob::knob);

    VoxBotAudioProcessor& processorRef;
    VoxBotLookAndFeel lnf;
    UiShared ui;

    FaceSelector face { ui };
    ClickButton prevPreset { "", -1 }, nextPreset { "", +1 }, initButton { "INIT" };
    ParamSegment detect;
    ParamToggle snap;

    std::vector<std::unique_ptr<ParamKnob>> knobs;
    std::array<ParamKnob*, Params::kNumTargets> targetKnobs {};

    ParamCombo robotType, midiMode;
    ParamToggle midiGlide;
    ParamKnob *glide = nullptr, *gateAtk = nullptr, *gateRel = nullptr, *output = nullptr;

    std::vector<std::unique_ptr<ModSlotPanel>> slots;

    int shownPreset = -1;
    bool shownGated = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxBotAudioProcessorEditor)
};
