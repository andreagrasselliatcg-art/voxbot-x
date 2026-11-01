#include "PluginProcessor.h"
#include "PluginEditor.h"

VoxBotAudioProcessor::VoxBotAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VoxBotX", createParameterLayout())
{
    presetParam = apvts.getRawParameterValue (ParamIDs::preset);
    mixParam    = apvts.getRawParameterValue (ParamIDs::mix);
    outputParam = apvts.getRawParameterValue (ParamIDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout VoxBotAudioProcessor::createParameterLayout()
{
    juce::StringArray names;
    for (auto& p : vbx::presets()) names.add (p.name);

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::preset, 1 }, "Robot", names, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::mix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::output, 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; })));

    return { params.begin(), params.end() };
}

bool VoxBotAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VoxBotAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    engine.setPreset ((int) presetParam->load());

    monoIn.setSize (1, samplesPerBlock);
    wetBuf.setSize (1, samplesPerBlock);

    mixSmoothed.reset (sampleRate, 0.03);
    gainSmoothed.reset (sampleRate, 0.03);
    mixSmoothed.setCurrentAndTargetValue (mixParam->load() * 0.01f);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
}

void VoxBotAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = getTotalNumInputChannels();
    const int n     = buffer.getNumSamples();

    for (auto ch = numCh; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    if (numCh == 0 || n == 0) return;

    if (monoIn.getNumSamples() < n)
    {
        monoIn.setSize (1, n, false, false, true);
        wetBuf.setSize (1, n, false, false, true);
    }

    const int preset = juce::jlimit (0, vbx::kNumPresets - 1, (int) presetParam->load());
    if (preset != engine.getPreset())
        engine.setPreset (preset);

    // Sum the voice to mono for the vocoder.
    auto* mono = monoIn.getWritePointer (0);
    juce::FloatVectorOperations::copy (mono, buffer.getReadPointer (0), n);
    for (int ch = 1; ch < numCh; ++ch)
        juce::FloatVectorOperations::add (mono, buffer.getReadPointer (ch), n);
    if (numCh > 1)
        juce::FloatVectorOperations::multiply (mono, 1.0f / (float) numCh, n);

    auto* wet = wetBuf.getWritePointer (0);
    engine.process (mono, wet, n);

    mixSmoothed.setTargetValue (mixParam->load() * 0.01f);
    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));

    for (int i = 0; i < n; ++i)
    {
        const float m = mixSmoothed.getNextValue();
        const float g = gainSmoothed.getNextValue();
        // Equal-power crossfade between original and robot voice.
        const float dryGain = std::cos (m * juce::MathConstants<float>::halfPi) * g;
        const float wetGain = std::sin (m * juce::MathConstants<float>::halfPi) * g;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[i] = d[i] * dryGain + wet[i] * wetGain;
        }
    }
}

int VoxBotAudioProcessor::getCurrentProgram()
{
    return (int) presetParam->load();
}

void VoxBotAudioProcessor::setCurrentProgram (int index)
{
    if (auto* p = apvts.getParameter (ParamIDs::preset))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, vbx::kNumPresets - 1, index)));
}

const juce::String VoxBotAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < vbx::kNumPresets)
        return vbx::presets()[(size_t) index].name;
    return {};
}

juce::AudioProcessorEditor* VoxBotAudioProcessor::createEditor()
{
    return new VoxBotAudioProcessorEditor (*this);
}

void VoxBotAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VoxBotAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxBotAudioProcessor();
}
