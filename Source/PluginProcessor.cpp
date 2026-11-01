#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace Params;

//==============================================================================
VoxBotAudioProcessor::VoxBotAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VoxBotX", Params::createLayout())
{
    auto get = [this] (const juce::String& id)
    {
        auto* p = apvts.getParameter (id);
        jassert (p != nullptr);
        return p;
    };

    for (int t = 0; t < kNumTargets; ++t) targetParams[(size_t) t] = get (targets()[(size_t) t].id);
    pRobotType = get (robotType);  pDetect = get (detect);    pSnap = get (snap);
    pMidiMode  = get (midiMode);   pGlide = get (glide);      pMidiGlide = get (midiOnlyGlide);
    pGateAtk   = get (gateAttack); pGateRel = get (gateRelease); pOutput = get (output);

    for (int s = 0; s < kSlots; ++s)
    {
        auto& sp = slotParams[(size_t) s];
        sp.on = get (slotId (s, "on"));             sp.type = get (slotId (s, "type"));
        sp.meta = get (slotId (s, "meta"));         sp.metaPol = get (slotId (s, "metaPol"));
        sp.metaSteps = get (slotId (s, "metaSteps")); sp.midiSrc = get (slotId (s, "midiSrc"));
        sp.seqSync = get (slotId (s, "seqSync"));   sp.seqRetrig = get (slotId (s, "seqRetrig"));
        sp.seqRate = get (slotId (s, "seqRate"));   sp.seqDiv = get (slotId (s, "seqDiv"));
        sp.seqShape = get (slotId (s, "seqShape"));
        sp.folAtk = get (slotId (s, "folAtk"));     sp.folRel = get (slotId (s, "folRel"));
        sp.adsrA = get (slotId (s, "adsrA"));       sp.adsrD = get (slotId (s, "adsrD"));
        sp.adsrS = get (slotId (s, "adsrS"));       sp.adsrR = get (slotId (s, "adsrR"));
        for (int t = 0; t < kNumTargets; ++t) sp.amount[(size_t) t] = get (amountId (s, t));
        for (int o = 0; o < kSlots; ++o) sp.cross[(size_t) o] = o == s ? nullptr : get (crossId (s, o));

        const int defaultPattern[kSlots] = { 6, 4, 2, 0 };
        float steps[kSeqSteps];
        generatePattern (defaultPattern[s], steps, 1);
        for (int i = 0; i < kSeqSteps; ++i) seqSteps[(size_t) s][(size_t) i].store (steps[i]);
        patternIndex[(size_t) s].store (defaultPattern[s]);
    }

    // Start on the first factory preset.
    applyPreset (0);
    for (int t = 0; t < kNumTargets; ++t) modulatedNorm[(size_t) t].store (targetParams[(size_t) t]->getValue());
}

static float plain (juce::RangedAudioParameter* p) { return p->convertFrom0to1 (p->getValue()); }

//==============================================================================
bool VoxBotAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VoxBotAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    latency = engine.getLatency();
    setLatencySamples (latency);

    dryDelay.setSize (2, latency + 1);
    dryDelay.clear();
    dryWrite = 0;

    mixCurrent  = plain (targetParams[tMix]) * 0.01f;
    gainCurrent = juce::Decibels::decibelsToGain (plain (pOutput));

    held.fill (false);
    heldCount = 0;
    for (auto& st : slotState) st = SlotState();
}

//==============================================================================
void VoxBotAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        if (! held[(size_t) n]) { held[(size_t) n] = true; ++heldCount; }
        midiNote = (float) (n - 60) / 12.0f;
        midiVelocity = m.getFloatVelocity();
        lastNote.store (n);
        engine.noteOn (n, midiVelocity);
        for (auto& st : slotState) { st.adsrStage = 1; st.retrigPending = true; }
    }
    else if (m.isNoteOff())
    {
        const int n = m.getNoteNumber();
        if (held[(size_t) n]) { held[(size_t) n] = false; --heldCount; }
        engine.noteOff (n);
        if (heldCount == 0)
        {
            for (auto& st : slotState) if (st.adsrStage != 0) st.adsrStage = 4;
            lastNote.store (-1);
        }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        held.fill (false);
        heldCount = 0;
        engine.allNotesOff();
        for (auto& st : slotState) if (st.adsrStage != 0) st.adsrStage = 4;
        lastNote.store (-1);
    }
    else if (m.isPitchWheel())       midiPitchWheel = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isController() && m.getControllerNumber() == 1) midiModWheel = (float) m.getControllerValue() / 127.0f;
    else if (m.isChannelPressure())  midiAftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())       midiAftertouch = (float) m.getAfterTouchValue() / 127.0f;

    notesHeld.store (heldCount);
}

//==============================================================================
static float seqShapeValue (int shape, float cur, float next, float t)
{
    switch (shape)
    {
        case 0:  return cur + (next - cur) * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t)); // rounded
        case 1:  return cur + (next - cur) * t;                                                              // lines
        case 2:  return cur;                                                                                 // steps
        case 3:  return cur * (1.0f - t);                                                                    // drops
        default: { const float d = 1.0f - t; return cur * d * d * d; }                                       // curved drops
    }
}

void VoxBotAudioProcessor::updateModulation (int numSamples, double ppq, double bpm, bool playing, float chunkPeak)
{
    const float dt = (float) numSamples / (float) sr;

    for (int s = 0; s < kSlots; ++s)
    {
        auto& sp = slotParams[(size_t) s];
        auto& st = slotState[(size_t) s];
        float out = 0.0f;

        switch ((int) plain (sp.type))
        {
            case srcMeta:
            {
                float v = plain (sp.meta);
                const int steps = stepCounts[juce::jlimit (0, 12, (int) plain (sp.metaSteps))];
                if (steps >= 2) v = std::round (v * (float) (steps - 1)) / (float) (steps - 1);
                const int pol = (int) plain (sp.metaPol);
                out = pol == 0 ? v : pol == 1 ? -v : 2.0f * v - 1.0f;
                break;
            }
            case srcMidi:
            {
                switch ((int) plain (sp.midiSrc))
                {
                    case 0:  out = midiNote; break;
                    case 1:  out = midiVelocity; break;
                    case 2:  out = midiPitchWheel; break;
                    case 3:  out = midiModWheel; break;
                    default: out = midiAftertouch; break;
                }
                break;
            }
            case srcSequencer:
            {
                const bool sync   = plain (sp.seqSync) > 0.5f;
                const bool retrig = plain (sp.seqRetrig) > 0.5f;
                if (retrig && st.retrigPending) st.seqPos = 0.0;
                st.retrigPending = false;

                if (sync)
                {
                    const double stepBeats = divBeats[juce::jlimit (0, 11, (int) plain (sp.seqDiv))];
                    if (playing && ! retrig) st.seqPos = ppq / stepBeats;
                    else                     st.seqPos += (double) dt * (bpm / 60.0) / stepBeats;
                }
                else st.seqPos += (double) dt * (double) plain (sp.seqRate);

                double wrapped = std::fmod (st.seqPos, (double) kSeqSteps);
                if (wrapped < 0) wrapped += kSeqSteps;
                const int step = (int) wrapped;
                const float t = (float) (wrapped - step);

                // "rndm" pattern: new random sequence every cycle
                if (st.lastStep > step && patternIndex[(size_t) s].load() >= 100)
                {
                    float steps[kSeqSteps];
                    generatePattern (100, steps, rng = rng * 1664525u + 1013904223u);
                    for (int i = 0; i < kSeqSteps; ++i) seqSteps[(size_t) s][(size_t) i].store (steps[i]);
                }
                st.lastStep = step;

                const float cur  = seqSteps[(size_t) s][(size_t) step].load();
                const float next = seqSteps[(size_t) s][(size_t) ((step + 1) % kSeqSteps)].load();
                const float shape = juce::jlimit (0.0f, 4.0f, plain (sp.seqShape));
                const int sh0 = (int) shape;
                const float shf = shape - (float) sh0;
                out = seqShapeValue (sh0, cur, next, t);
                if (shf > 0.0f && sh0 < 4) out += shf * (seqShapeValue (sh0 + 1, cur, next, t) - out);

                seqPosition[(size_t) s].store ((float) wrapped);
                break;
            }
            case srcFollower:
            {
                const float atk = std::max (0.0001f, plain (sp.folAtk) * 0.001f);
                const float rel = std::max (0.001f,  plain (sp.folRel) * 0.001f);
                const float coef = 1.0f - std::exp (-dt / (chunkPeak > st.follower ? atk : rel));
                st.follower += coef * (chunkPeak - st.follower);
                const float db = juce::Decibels::gainToDecibels (st.follower, -60.0f);
                out = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
                break;
            }
            default: // ADSR
            {
                const float a = plain (sp.adsrA) * 0.001f, d = plain (sp.adsrD) * 0.001f;
                const float su = plain (sp.adsrS) * 0.01f, r = plain (sp.adsrR) * 0.001f;
                switch (st.adsrStage)
                {
                    case 1: st.adsr += dt / std::max (0.001f, a); if (st.adsr >= 1.0f) { st.adsr = 1.0f; st.adsrStage = 2; } break;
                    case 2: st.adsr -= dt / std::max (0.001f, d) * (1.0f - su); if (st.adsr <= su) { st.adsr = su; st.adsrStage = 3; } break;
                    case 3: st.adsr = su; break;
                    case 4: st.adsr -= dt / std::max (0.001f, r); if (st.adsr <= 0.0f) { st.adsr = 0.0f; st.adsrStage = 0; } break;
                    default: st.adsr = 0.0f; break;
                }
                out = st.adsr;
                break;
            }
        }

        st.raw = plain (sp.on) > 0.5f ? out : 0.0f;
    }

    // Cross-modulation: other slots scale this slot's depth.
    std::array<float, kSlots> scaled {};
    for (int s = 0; s < kSlots; ++s)
    {
        float scale = 1.0f;
        for (int o = 0; o < kSlots; ++o)
            if (o != s) scale += plain (slotParams[(size_t) s].cross[(size_t) o]) * slotState[(size_t) o].raw;
        scaled[(size_t) s] = slotState[(size_t) s].raw * juce::jlimit (0.0f, 2.0f, scale);
        slotOutput[(size_t) s].store (scaled[(size_t) s]);
    }

    // Final values
    std::array<float, kNumTargets> v {};
    for (int t = 0; t < kNumTargets; ++t)
    {
        float norm = targetParams[(size_t) t]->getValue();
        for (int s = 0; s < kSlots; ++s)
            if (std::abs (scaled[(size_t) s]) > 1.0e-9f)
                norm += plain (slotParams[(size_t) s].amount[(size_t) t]) * scaled[(size_t) s];
        norm = juce::jlimit (0.0f, 1.0f, norm);
        modulatedNorm[(size_t) t].store (norm);
        v[(size_t) t] = targetParams[(size_t) t]->convertFrom0to1 (norm);
    }

    auto& e = engineParams;
    e.pitch = v[tPitch];            e.formant = v[tFormant];        e.smooth = v[tSmooth] * 0.01f;
    e.harmonics = v[tHarmonics];    e.ratio = v[tRatio];            e.fm = v[tFm] * 0.01f;
    e.alternator = v[tAlternator];  e.octave = v[tOctave];
    e.smear = v[tSmear] * 0.01f;    e.detune = v[tDetune] * 0.01f;  e.stereo = v[tStereo] * 0.01f;
    e.robot = v[tRobot] * 0.01f;
    e.robotType = (int) plain (pRobotType);
    e.detect = (int) plain (pDetect);
    e.snap = plain (pSnap) > 0.5f;
    e.midiMode = (int) plain (pMidiMode);
    e.glideMs = plain (pGlide);
    e.midiOnlyGlide = plain (pMidiGlide) > 0.5f;
    e.gateAttackMs = plain (pGateAtk);
    e.gateReleaseMs = plain (pGateRel);
    engine.setParams (e);
}

//==============================================================================
void VoxBotAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = getTotalNumInputChannels();
    const int n     = buffer.getNumSamples();
    for (auto ch = numCh; ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, n);
    if (numCh == 0 || n == 0) return;

    double bpm = 120.0, ppqStart = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppqStart = *q;
            playing = pos->getIsPlaying();
        }

    constexpr int kChunk = 32;
    float mono[kChunk], wetL[kChunk], wetR[kChunk];
    float levelPeak = 0.0f;

    auto midiIt = midi.begin();
    const int dryLen = dryDelay.getNumSamples();

    for (int start = 0; start < n; start += kChunk)
    {
        const int len = std::min (kChunk, n - start);

        while (midiIt != midi.end() && (*midiIt).samplePosition < start + len)
        {
            handleMidi ((*midiIt).getMessage());
            ++midiIt;
        }

        float peak = 0.0f;
        for (int i = 0; i < len; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) s += buffer.getSample (ch, start + i);
            s /= (float) numCh;
            mono[i] = s;
            peak = std::max (peak, std::abs (s));
        }
        levelPeak = std::max (levelPeak, peak);

        const double ppq = ppqStart + ((double) start / sr) * (bpm / 60.0);
        updateModulation (len, ppq, bpm, playing, peak);

        engine.process (mono, wetL, wetR, len);

        const float mixTarget  = modulatedNorm[tMix].load();   // 0..1 maps to 0..100 %
        const float gainTarget = juce::Decibels::decibelsToGain (plain (pOutput));
        const float mixStep  = (mixTarget  - mixCurrent)  / (float) len;
        const float gainStep = (gainTarget - gainCurrent) / (float) len;

        for (int i = 0; i < len; ++i)
        {
            mixCurrent += mixStep; gainCurrent += gainStep;
            const float dryG = std::cos (mixCurrent * juce::MathConstants<float>::halfPi) * gainCurrent;
            const float wetG = std::sin (mixCurrent * juce::MathConstants<float>::halfPi) * gainCurrent;
            const int readPos = dryWrite;   // oldest sample == exactly 'latency' samples ago

            for (int ch = 0; ch < numCh; ++ch)
            {
                const int dch = std::min (ch, 1);
                const float in = buffer.getSample (ch, start + i);
                const float delayed = dryDelay.getSample (dch, readPos);
                dryDelay.setSample (dch, readPos, in);
                const float w = numCh == 1 ? 0.5f * (wetL[i] + wetR[i]) : (ch == 0 ? wetL[i] : wetR[i]);
                buffer.setSample (ch, start + i, delayed * dryG + w * wetG);
            }
            if (++dryWrite >= dryLen) dryWrite = 0;
        }
    }

    inputLevel.store (std::max (levelPeak, inputLevel.load() * 0.85f));
    detectedHz.store (engine.isVoiced() ? engine.getDetectedHz() : 0.0f);
}

//==============================================================================
void VoxBotAudioProcessor::setSeqStep (int slot, int step, float value)
{
    if (slot < 0 || slot >= kSlots || step < 0 || step >= kSeqSteps) return;
    seqSteps[(size_t) slot][(size_t) step].store (juce::jlimit (-1.0f, 1.0f, value));
}

void VoxBotAudioProcessor::generatePattern (int pattern, float* steps, uint32_t seed)
{
    const int type = pattern % 10;
    const int variant = (pattern / 10) % 10;
    const float amp = 1.0f - 0.07f * (float) variant;
    uint32_t r = seed * 2654435761u + (uint32_t) pattern * 40503u + 1u;
    auto rnd = [&r] { r = r * 1664525u + 1013904223u; return (float) (r >> 8) / 16777216.0f; };

    static const int majorArp[16] = { 0, 4, 7, 12, 7, 4, 0, -5, 0, 4, 7, 12, 16, 12, 7, 4 };
    static const int minorArp[16] = { 0, 3, 7, 10, 12, 10, 7, 3, 0, -2, 0, 3, 7, 12, 15, 12 };

    for (int i = 0; i < kSeqSteps; ++i)
    {
        const int k = (i + variant) % kSeqSteps;
        float v = 0.0f;
        if (pattern >= 100) v = std::round ((rnd() * 2.0f - 1.0f) * 12.0f) / 12.0f;
        else switch (type)
        {
            case 0:  v = -1.0f + 2.0f * (float) k / 15.0f; break;
            case 1:  v =  1.0f - 2.0f * (float) k / 15.0f; break;
            case 2:  v = k < 8 ? -1.0f + (float) k / 3.5f : 1.0f - (float) (k - 8) / 3.5f; break;
            case 3:  v = (k % 2 == 0) ? 1.0f : -1.0f; break;
            case 4:  v = (float) majorArp[k] / 12.0f; break;
            case 5:  v = (k % 4 == 0) ? 1.0f : 0.0f; break;
            case 6:  v = std::sin (juce::MathConstants<float>::twoPi * (float) k / 16.0f); break;
            case 7:  v = rnd() * 2.0f - 1.0f; break;
            case 8:  v = (float) minorArp[k] / 12.0f; break;
            default: v = (float) (k % 8) / 12.0f; break;
        }
        steps[i] = juce::jlimit (-1.0f, 1.0f, pattern >= 100 ? v : v * (type == 4 || type == 8 || type == 9 ? 1.0f : amp));
    }
}

void VoxBotAudioProcessor::loadPattern (int slot, int pattern)
{
    if (slot < 0 || slot >= kSlots) return;
    pattern = juce::jlimit (0, 100, pattern);
    patternIndex[(size_t) slot].store (pattern);
    float steps[kSeqSteps];
    generatePattern (pattern, steps, (uint32_t) juce::Random::getSystemRandom().nextInt());
    for (int i = 0; i < kSeqSteps; ++i) seqSteps[(size_t) slot][(size_t) i].store (steps[i]);
}

//==============================================================================
int VoxBotAudioProcessor::getNumPrograms() { return (int) Presets::all().size(); }

const juce::String VoxBotAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < (int) Presets::all().size()) return Presets::all()[(size_t) index].name;
    return {};
}

void VoxBotAudioProcessor::initPatch()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    const int defaultPattern[kSlots] = { 6, 4, 2, 0 };
    for (int s = 0; s < kSlots; ++s) loadPattern (s, defaultPattern[s]);
}

void VoxBotAudioProcessor::applyPreset (int index)
{
    const auto& all = Presets::all();
    if (index < 0 || index >= (int) all.size()) return;

    initPatch();
    const auto& def = all[(size_t) index];
    for (auto& v : def.values)
        if (auto* p = apvts.getParameter (v.id))
            p->setValueNotifyingHost (p->convertTo0to1 (v.value));
    for (auto& pat : def.patterns)
        loadPattern (pat.slot, pat.pattern);

    currentPreset.store (index);
    updateHostDisplay (ChangeDetails().withProgramChanged (true));
}

//==============================================================================
void VoxBotAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree extra ("EXTRA");
    extra.setProperty ("preset", currentPreset.load(), nullptr);
    for (int s = 0; s < kSlots; ++s)
    {
        juce::StringArray vals;
        for (int i = 0; i < kSeqSteps; ++i) vals.add (juce::String (seqSteps[(size_t) s][(size_t) i].load(), 4));
        extra.setProperty ("seq" + juce::String (s), vals.joinIntoString (","), nullptr);
        extra.setProperty ("pattern" + juce::String (s), patternIndex[(size_t) s].load(), nullptr);
    }
    state.removeChild (state.getChildWithName ("EXTRA"), nullptr);
    state.appendChild (extra, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VoxBotAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml || ! xml->hasTagName (apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    auto extra = tree.getChildWithName ("EXTRA");
    if (extra.isValid())
    {
        currentPreset.store ((int) extra.getProperty ("preset", 0));
        for (int s = 0; s < kSlots; ++s)
        {
            auto vals = juce::StringArray::fromTokens (extra.getProperty ("seq" + juce::String (s)).toString(), ",", "");
            for (int i = 0; i < kSeqSteps && i < vals.size(); ++i)
                seqSteps[(size_t) s][(size_t) i].store (vals[i].getFloatValue());
            patternIndex[(size_t) s].store ((int) extra.getProperty ("pattern" + juce::String (s), 0));
        }
        tree.removeChild (extra, nullptr);
    }
    apvts.replaceState (tree);
}

juce::AudioProcessorEditor* VoxBotAudioProcessor::createEditor()
{
    return new VoxBotAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxBotAudioProcessor();
}
