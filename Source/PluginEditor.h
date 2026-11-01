#pragma once

#include "PluginProcessor.h"
#include "VoxBotLookAndFeel.h"

/** Small triangular button used to step through presets. */
class PresetArrow : public juce::Component
{
public:
    explicit PresetArrow (bool pointsRight) : right (pointsRight)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void()> onClick;
    juce::Colour colour { 0xffd0d0d8 };

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp    (const juce::MouseEvent& e) override
    {
        if (onClick && getLocalBounds().contains (e.getPosition())) onClick();
    }

private:
    bool right, hover = false;
};

class VoxBotAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit VoxBotAudioProcessorEditor (VoxBotAudioProcessor&);
    ~VoxBotAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void stepPreset (int delta);
    void updateAccent();

    VoxBotAudioProcessor& processorRef;
    VoxBotLookAndFeel lnf;

    juce::Slider presetKnob, mixKnob, outputKnob;
    PresetArrow prevButton { false }, nextButton { true };

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> presetAttachment, mixAttachment, outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxBotAudioProcessorEditor)
};
