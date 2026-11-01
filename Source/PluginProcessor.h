#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ManipEngine.h"
#include "Params.h"

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
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset.load(); }
    void setCurrentProgram (int index) override { applyPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Presets (message thread)
    void applyPreset (int index);
    void initPatch();

    // Sequencer steps (thread-safe)
    float getSeqStep (int slot, int step) const { return seqSteps[(size_t) slot][(size_t) step].load(); }
    void  setSeqStep (int slot, int step, float value);
    void  loadPattern (int slot, int pattern);       // 0..99, 100 = random every cycle
    int   getPattern (int slot) const { return patternIndex[(size_t) slot].load(); }
    static void generatePattern (int pattern, float* steps, uint32_t seed);

    //==========================================================================
    // Live values for the UI
    std::array<std::atomic<float>, Params::kNumTargets> modulatedNorm {};
    std::array<std::atomic<float>, Params::kSlots> slotOutput {};
    std::array<std::atomic<float>, Params::kSlots> seqPosition {};   // step index + phase
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<int>   lastNote { -1 };
    std::atomic<int>   notesHeld { 0 };
    std::atomic<float> detectedHz { 0.0f };
    std::atomic<int>   currentPreset { 0 };

    juce::AudioProcessorValueTreeState apvts;

private:
    void handleMidi (const juce::MidiMessage& m);
    void updateModulation (int numSamples, double ppqAtChunk, double bpm, bool playing, float chunkPeak);

    // Cached parameter pointers
    std::array<juce::RangedAudioParameter*, Params::kNumTargets> targetParams {};
    juce::RangedAudioParameter *pRobotType, *pDetect, *pSnap, *pMidiMode, *pGlide, *pMidiGlide,
                               *pGateAtk, *pGateRel, *pOutput;

    struct SlotParams
    {
        juce::RangedAudioParameter *on, *type, *meta, *metaPol, *metaSteps, *midiSrc,
                                   *seqSync, *seqRetrig, *seqRate, *seqDiv, *seqShape,
                                   *folAtk, *folRel, *adsrA, *adsrD, *adsrS, *adsrR;
        std::array<juce::RangedAudioParameter*, Params::kNumTargets> amount {};
        std::array<juce::RangedAudioParameter*, Params::kSlots> cross {};
    };
    std::array<SlotParams, Params::kSlots> slotParams {};

    struct SlotState
    {
        double seqPos = 0.0;
        int    lastStep = -1;
        float  follower = 0.0f;
        float  adsr = 0.0f;
        int    adsrStage = 0;   // 0 idle, 1 attack, 2 decay, 3 sustain, 4 release
        float  raw = 0.0f;
        bool   retrigPending = false;
    };
    std::array<SlotState, Params::kSlots> slotState {};

    std::array<std::array<std::atomic<float>, Params::kSeqSteps>, Params::kSlots> seqSteps {};
    std::array<std::atomic<int>, Params::kSlots> patternIndex {};
    uint32_t rng = 12345u;

    // MIDI state for modulation
    std::array<bool, 128> held {};
    int   heldCount = 0;
    float midiNote = 0.0f, midiVelocity = 0.0f, midiPitchWheel = 0.0f, midiModWheel = 0.0f, midiAftertouch = 0.0f;

    vbx::ManipEngine engine;
    vbx::ManipParams engineParams;
    double sr = 44100.0;

    juce::AudioBuffer<float> dryDelay;
    int dryWrite = 0, latency = 0;
    float mixCurrent = 1.0f, gainCurrent = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxBotAudioProcessor)
};
