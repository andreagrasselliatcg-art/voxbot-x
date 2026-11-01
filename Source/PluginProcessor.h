#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "VocoderEngine.h"

namespace ParamIDs
{
    inline constexpr const char* preset = "preset";
    inline constexpr const char* mix    = "mix";
    inline constexpr const char* output = "output";
}

class VoxBotAudioProcessor : public juce::AudioProcessor
{
public:
    VoxBotAudioProcessor();
    ~VoxBotAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VoxBot - X"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    // Host-visible programs mirror the robot presets.
    int getNumPrograms() override { return vbx::kNumPresets; }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    std::atomic<float>* presetParam = nullptr;
    std::atomic<float>* mixParam    = nullptr;
    std::atomic<float>* outputParam = nullptr;

    vbx::VocoderEngine engine;
    juce::AudioBuffer<float> monoIn, wetBuf;
    juce::SmoothedValue<float> mixSmoothed, gainSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxBotAudioProcessor)
};
