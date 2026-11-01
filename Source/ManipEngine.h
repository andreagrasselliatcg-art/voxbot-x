#pragma once

// VoxBot - X v2 "manipulation" engine.
//
// Chain:  Smear ─► pitch tracker ─┬─► voice 1..4: pitch-synchronous grains
//                                 │      (Pitch, Formant, Smooth Grains, Grain Alternator / Octave)
//                                 │      ─► Harmonic shifter + FM (single-sideband, pitch-locked)
//                                 │      ─► gate envelope (MIDI modes)
//                                 └─► Robot vocoder (delay-compensated)
//         sum ─► Robot blend ─► Detune ─► Stereo ─► wet L/R
//
// The engine has a fixed latency (getLatency()); the host compensates it.

#include "VocoderEngine.h"

namespace vbx
{
enum MidiMode { midiOff = 0, midiMono, midiPoly, midiMonoGated, midiPolyGated };

struct ManipParams
{
    float pitch = 0.0f;        // semitones  -24..24
    float formant = 0.0f;      // semitones  -24..24
    float smooth = 1.0f;       // 0..1
    float harmonics = 0.0f;    // -8..8
    float ratio = 1.0f;        // 0..2
    float fm = 0.0f;           // 0..1
    float alternator = 0.0f;   // semitones 0..24
    float octave = 0.0f;       // -8..8
    float smear = 0.0f;        // 0..1
    float detune = 0.0f;       // 0..1
    float stereo = 0.0f;       // 0..1
    float robot = 0.0f;        // 0..1
    int   robotType = 0;
    int   detect = 1;
    bool  snap = false;
    int   midiMode = midiOff;
    float glideMs = 0.0f;
    bool  midiOnlyGlide = false;
    float gateAttackMs = 10.0f;
    float gateReleaseMs = 150.0f;
};

//==============================================================================
class ManipEngine
{
public:
    static constexpr int kVoices = 4;
    static constexpr int kGrains = 48;

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = (float) sampleRate;
        latency = (int) std::ceil (0.080f * sr);

        ring.prepare (latency * 4 + 8192);
        ringAbs = 0;
        tracker.prepare (sampleRate);
        tracker.setRange (params.detect);

        vocoder.prepare (sampleRate);
        vocoder.setPreset (params.robotType);
        vocDelay.prepare (latency + 16);

        smearBuf.prepare ((int) (sr * 0.6f) + 16);
        smearT = 0; smearLoopLen = 0;

        for (auto& d : detuners) d.prepare (sr);
        for (int i = 0; i < 3; ++i)
        {
            const float ms[3] = { 5.3f, 8.9f, 13.1f };
            allpass[(size_t) i].prepare ((int) (ms[i] * 0.001f * sr) + 4);
            apDelay[(size_t) i] = ms[i] * 0.001f * sr;
        }
        sideHp = 0; sideHpPrev = 0;

        for (auto& v : voices) resetVoice (v);
        voices[0].env = 1.0f;
        noteStackSize = 0;
        orderCounter = 0;

        smooth.robot = params.robot; smooth.detune = params.detune; smooth.stereo = params.stereo;
        smooth.smear = params.smear; smooth.bypass = 0.0f;
        sCoef = 1.0f - std::exp (-1.0f / (0.02f * sr));
    }

    int getLatency() const { return latency; }

    void setParams (const ManipParams& p)
    {
        params = p;
        if (params.snap)
        {
            params.pitch      = std::round (params.pitch);
            params.formant    = std::round (params.formant);
            params.harmonics  = std::round (params.harmonics);
            params.alternator = std::round (params.alternator);
            params.ratio      = std::round (params.ratio * 4.0f) / 4.0f;
        }
        tracker.setRange (params.detect);
        if (params.robotType != vocoder.getPreset()) vocoder.setPreset (params.robotType);
    }

    //--------------------------------------------------------------------------
    void noteOn (int note, float /*velocity*/)
    {
        const int mode = params.midiMode;
        if (mode == midiOff) return;

        if (mode == midiMono || mode == midiMonoGated)
        {
            const bool legato = noteStackSize > 0;
            removeFromStack (note);
            if (noteStackSize < (int) noteStack.size()) noteStack[(size_t) noteStackSize++] = note;
            assignNote (voices[0], note, legato);
        }
        else
        {
            for (auto& v : voices) if (v.note == note) return;
            Voice* target = nullptr;
            for (auto& v : voices) if (v.note < 0 && v.env <= 0.0001f) { target = &v; break; }
            if (! target) for (auto& v : voices) if (v.note < 0) { target = &v; break; }
            if (! target)
            {
                target = &voices[0];
                for (auto& v : voices) if (v.order < target->order) target = &v;
            }
            assignNote (*target, note, false);
        }
    }

    void noteOff (int note)
    {
        const int mode = params.midiMode;
        if (mode == midiMono || mode == midiMonoGated)
        {
            removeFromStack (note);
            if (voices[0].note == note)
            {
                if (noteStackSize > 0) assignNote (voices[0], noteStack[(size_t) noteStackSize - 1], true);
                else voices[0].note = -1;
            }
        }
        else
        {
            for (auto& v : voices) if (v.note == note) v.note = -1;
        }
    }

    void allNotesOff()
    {
        noteStackSize = 0;
        for (auto& v : voices) v.note = -1;
    }

    bool anyNoteHeld() const
    {
        for (auto& v : voices) if (v.note >= 0) return true;
        return false;
    }

    float getDetectedHz() const { return tracker.hz; }
    bool  isVoiced() const { return tracker.voiced; }

    //--------------------------------------------------------------------------
    void process (const float* in, float* outL, float* outR, int n)
    {
        const auto& P = params;
        const bool midiOn = P.midiMode != midiOff;
        const bool gated  = P.midiMode == midiMonoGated || P.midiMode == midiPolyGated;
        const bool anyNote = anyNoteHeld();

        // Robot vocoder follows the held MIDI note if there is one.
        vocoder.setExternalPitch (midiOn && voices[0].note >= 0
                                  ? noteHz (voices[0].note) * semisToRatio (P.pitch) : 0.0f);

        const float glideCoef = P.glideMs <= 0.5f ? 1.0f : 1.0f - std::exp (-1.0f / (0.001f * P.glideMs * sr));
        const float atkStep = 1.0f / std::max (1.0f, (gated ? P.gateAttackMs : 4.0f) * 0.001f * sr);
        const float relStep = 1.0f / std::max (1.0f, (gated ? P.gateReleaseMs : 40.0f) * 0.001f * sr);

        // Which voices are wanted?
        for (int vi = 0; vi < kVoices; ++vi)
        {
            auto& v = voices[(size_t) vi];
            bool want;
            if (! midiOn)                           want = vi == 0;
            else if (v.note >= 0)                   want = true;
            else if (! gated && ! anyNote && vi == 0) want = true;   // plays the original melody
            else                                    want = false;
            v.envTarget = want ? 1.0f : 0.0f;

            // absolute (MIDI note) or relative (pitch knob) mode
            const bool abs = midiOn && v.note >= 0;
            if (abs != v.absMode)
            {
                const float fin = std::max (40.0f, tracker.hz);
                if (abs) v.logCur = std::log (fin * std::exp (v.logCur));      // ratio -> Hz
                else     v.logCur = std::log (std::exp (v.logCur) / fin);      // Hz -> ratio
                v.absMode = abs;
                if (v.env <= 0.0001f) v.glideNow = false;
            }
            v.logTarget = abs ? std::log (noteHz (v.note) * semisToRatio (P.pitch))
                              : std::log (semisToRatio (P.pitch));
        }

        // Transparent bypass for voice 1 when nothing would change the sound.
        const bool neutral = ! midiOn && std::abs (P.pitch) < 0.01f && std::abs (P.formant) < 0.01f
                             && P.alternator < 0.01f;
        const float bypassTarget = neutral ? 1.0f : 0.0f;

        for (int i = 0; i < n; ++i)
        {
            smooth.robot  += sCoef * (P.robot  - smooth.robot);
            smooth.detune += sCoef * (P.detune - smooth.detune);
            smooth.stereo += sCoef * (P.stereo - smooth.stereo);
            smooth.smear  += sCoef * (P.smear  - smooth.smear);
            smooth.bypass += sCoef * (bypassTarget - smooth.bypass);

            // ---- Smear (granular loops), before everything else -----------------
            float x = processSmear (in[i]);

            ring.push (x);
            ++ringAbs;
            tracker.push (x);

            // ---- Robot vocoder (zero latency) -> delay to align -------------------
            float voc = 0.0f;
            if (P.robot > 0.0001f || smooth.robot > 0.0001f)
                vocoder.process (&x, &voc, 1);
            vocDelay.push (voc);
            const float vocAligned = vocDelay.read ((float) latency + 1.0f);

            // ---- Voices --------------------------------------------------------
            const float dryAligned = ring.read ((float) latency + 1.0f);
            float wet = 0.0f;
            for (int vi = 0; vi < kVoices; ++vi)
            {
                auto& v = voices[(size_t) vi];
                if (v.envTarget <= 0.0f && v.env <= 0.0f) continue;

                v.env += v.envTarget > v.env ? std::min (atkStep, v.envTarget - v.env)
                                             : std::max (-relStep, v.envTarget - v.env);

                const float gc = (v.glideNow || (! midiOn && ! P.midiOnlyGlide)) ? glideCoef : 1.0f;
                v.logCur += gc * (v.logTarget - v.logCur);

                float y = renderGrains (v);
                if (vi == 0) y = y * (1.0f - smooth.bypass) + dryAligned * smooth.bypass;
                y = harmonicsAndFm (v, y);
                wet += y * v.env;
            }

            // ---- Robot blend, Detune, Stereo -------------------------------------
            wet = wet * (1.0f - smooth.robot) + vocAligned * smooth.robot;

            float l = wet, r = wet;
            if (smooth.detune > 0.0005f)
            {
                const float cents = 4.0f + 26.0f * smooth.detune;
                const float up   = detuners[0].process (wet, semisToRatio ( cents / 100.0f));
                const float down = detuners[1].process (wet, semisToRatio (-cents / 100.0f));
                const float d = smooth.detune;
                const float norm = 1.0f / (1.0f + 0.35f * d);
                l = (wet + 0.8f * d * up) * norm;
                r = (wet + 0.8f * d * down) * norm;
            }

            if (smooth.stereo > 0.0005f)
            {
                const float m = 0.5f * (l + r);
                float s = 0.5f * (l - r);
                float dcor = m;
                for (int a = 0; a < 3; ++a) dcor = allpassTick (a, dcor);
                // keep the low end mono
                const float hp = 0.985f * (sideHp + dcor - sideHpPrev);
                sideHpPrev = dcor; sideHp = hp;
                s += smooth.stereo * 0.9f * hp;
                l = m + s; r = m - s;
            }

            outL[i] = l; outR[i] = r;
        }
    }

private:
    //--------------------------------------------------------------------------
    struct Grain
    {
        bool  active = false;
        double start = 0.0;   // absolute source position of the first sample
        float rate = 1.0f;    // source samples per output sample (formant)
        float len = 1.0f;     // output length in samples
        float age = 0.0f;
        float gain = 1.0f;
    };

    struct Voice
    {
        std::array<Grain, kGrains> grains;
        float countdown = 0.0f;
        double prevSrc = 0.0;
        bool havePrev = false;
        long long grainCount = 0;

        int note = -1;
        long long order = 0;
        float env = 0.0f, envTarget = 0.0f;
        bool absMode = false, glideNow = false;
        float logCur = 0.0f, logTarget = 0.0f;
        float outHz = 150.0f;

        Hilbert hil;
        float phLo = 0.0f, phHi = 0.0f, fmPhase = 0.0f;
        int lastH0 = 0;
        float hWet = 0.0f;
        float dcPrev = 0.0f, dcState = 0.0f;
    };

    static float noteHz (int note) { return 440.0f * std::exp2 ((float) (note - 69) / 12.0f); }

    void resetVoice (Voice& v)
    {
        for (auto& g : v.grains) g.active = false;
        v.countdown = 0.0f; v.havePrev = false; v.grainCount = 0;
        v.note = -1; v.env = 0.0f; v.envTarget = 0.0f;
        v.absMode = false; v.glideNow = false;
        v.logCur = v.logTarget = 0.0f;
        v.hil.reset(); v.phLo = v.phHi = v.fmPhase = 0.0f; v.lastH0 = 0; v.hWet = 0.0f;
    }

    void assignNote (Voice& v, int note, bool legato)
    {
        const bool wasSilent = v.env <= 0.0001f;
        v.note = note;
        v.order = ++orderCounter;
        v.glideNow = params.glideMs > 0.5f && (! params.midiOnlyGlide || legato) && ! wasSilent;
        if (wasSilent)
        {
            for (auto& g : v.grains) g.active = false;
            v.havePrev = false;
            v.countdown = 0.0f;
        }
    }

    void removeFromStack (int note)
    {
        int w = 0;
        for (int r = 0; r < noteStackSize; ++r)
            if (noteStack[(size_t) r] != note) noteStack[(size_t) w++] = noteStack[(size_t) r];
        noteStackSize = w;
    }

    //--------------------------------------------------------------------------
    float renderGrains (Voice& v)
    {
        v.countdown -= 1.0f;
        if (v.countdown <= 0.0f) spawnGrain (v);

        float out = 0.0f;
        const auto& win = hann();
        for (auto& g : v.grains)
        {
            if (! g.active) continue;
            const float w = win (g.age / g.len);
            const double pos = g.start + (double) (g.age * g.rate);
            out += w * g.gain * readAbs (pos);
            g.age += 1.0f;
            if (g.age >= g.len) g.active = false;
        }
        return out;
    }

    void spawnGrain (Voice& v)
    {
        const auto& P = params;
        const bool voiced = tracker.voiced;
        const float fin = std::max (25.0f, tracker.hz);
        const float tin = voiced ? sr / fin : sr * 0.006f;

        // Base pitch ratio for this voice
        float ratio = v.absMode ? std::exp (v.logCur) / fin : std::exp (v.logCur);

        // Grain alternator / octave
        float polarity = 1.0f;
        if (P.alternator > 0.001f)
        {
            const int oct = std::clamp ((int) std::lround (std::abs (P.octave)), 0, 8);
            // positive: alternate every 2^oct cycles; negative: sub-octave at f0 / 2^|oct|
            const long long blockLen = P.octave < -0.5f ? (1LL << std::max (0, oct - 1)) : (1LL << oct);
            const bool odd = ((v.grainCount / blockLen) & 1) != 0;
            ratio *= semisToRatio (odd ? -P.alternator : P.alternator);
            // Negative octave: alternating grain polarity adds a sub-octave at f0 / 2^|octave|
            if (P.octave < -0.5f && odd) polarity = -0.5f;
        }
        ++v.grainCount;

        ratio = std::clamp (ratio, 0.0625f, 16.0f);
        const float tout = tin / ratio;
        v.outHz = sr / tout;

        const float fr = semisToRatio (P.formant);
        const float baseLen = 2.0f * tin / fr;
        const float smoothLen = std::max (baseLen, 2.0f * tout);
        float len = baseLen + P.smooth * (smoothLen - baseLen);
        const float maxSpan = 2.0f * ((float) latency - tin) - 4.0f;
        if (len * fr > maxSpan) len = maxSpan / fr;
        len = std::max (len, 16.0f);

        const double target = (double) ringAbs - 1.0 - (double) latency;
        double centre = target;
        if (voiced && v.havePrev)
        {
            double m = std::round ((target - v.prevSrc) / (double) tin);
            if (m < 0) m = 0;
            centre = v.prevSrc + m * (double) tin;
            if (std::abs (centre - target) > (double) tin) centre = target;
        }
        v.prevSrc = centre;
        v.havePrev = voiced;

        const float overlap = 2.0f * tout / len;   // <1: grains overlap more than 2x
        const float gain = std::clamp (std::sqrt (overlap), 0.1f, 2.0f);   // keep loudness constant

        for (auto& g : v.grains)
        {
            if (g.active) continue;
            g.active = true;
            g.rate = fr;
            g.len = len;
            g.age = 0.0f;
            g.start = centre - 0.5 * (double) (len * fr);
            g.gain = gain * polarity;
            break;
        }

        v.countdown += tout;
        if (v.countdown < 1.0f) v.countdown = 1.0f;
    }

    inline float readAbs (double pos) const
    {
        // ring.read() takes a delay relative to the write head
        const float delay = (float) ((double) ringAbs - pos);
        if (delay < 1.0f || delay > (float) ring.mask) return 0.0f;
        return ring.read (delay);
    }

    //--------------------------------------------------------------------------
    float harmonicsAndFm (Voice& v, float x)
    {
        const auto& P = params;
        const bool active = std::abs (P.harmonics) > 0.001f || P.fm > 0.001f;
        v.hWet += sCoef * ((active ? 1.0f : 0.0f) - v.hWet);

        float re, im;
        v.hil.tick (x, re, im);
        if (v.hWet < 0.0005f) return x;

        const float f0 = std::clamp (v.outHz, 20.0f, 4000.0f);
        const float h = P.harmonics;
        const int h0 = (int) std::floor (h);
        const float frac = h - (float) h0;

        // Keep phases continuous when the integer step changes.
        if (h0 == v.lastH0 + 1)      { v.phLo = v.phHi; v.phHi = 0.0f; }
        else if (h0 == v.lastH0 - 1) { v.phHi = v.phLo; v.phLo = 0.0f; }
        v.lastH0 = h0;

        const float stepHz = P.ratio * f0;
        v.phLo += kTwoPi * (float) h0 * stepHz / sr;
        v.phHi += kTwoPi * (float) (h0 + 1) * stepHz / sr;
        v.phLo = std::remainder (v.phLo, kTwoPi);
        v.phHi = std::remainder (v.phHi, kTwoPi);

        v.fmPhase += kTwoPi * std::max (0.0f, stepHz) / sr;
        if (v.fmPhase > kTwoPi) v.fmPhase -= kTwoPi;
        const float pm = P.fm * 4.0f * std::sin (v.fmPhase);

        const float aLo = v.phLo + pm, aHi = v.phHi + pm;
        const float yLo = re * std::cos (aLo) + im * std::sin (aLo);   // single-sideband, shifts up for +phase
        const float yHi = re * std::cos (aHi) + im * std::sin (aHi);
        float y = yLo * (1.0f - frac) + yHi * frac;

        // DC blocker: shifting harmonics down can push the fundamental to 0 Hz.
        const float yb = y - v.dcPrev + 0.9995f * v.dcState;
        v.dcPrev = y; v.dcState = yb; y = yb;

        return x * (1.0f - v.hWet) + y * v.hWet;
    }

    //--------------------------------------------------------------------------
    float processSmear (float x)
    {
        smearBuf.push (x);
        const float s = smooth.smear;
        if (params.smear <= 0.0005f && s <= 0.0005f) { smearT = 0; return x; }

        const float interval = sr * (0.03f + 0.47f * s);
        if (smearT <= 0.0f || smearT >= smearInterval)
        {
            smearT = 0.0f;
            smearInterval = interval;
            smearLoopLen = sr * (0.012f + 0.06f * s);
        }

        float y;
        const float fade = sr * 0.003f;
        if (smearT < smearLoopLen) y = x;
        else
        {
            const float t = smearT - smearLoopLen;
            const float posInLoop = std::fmod (t, smearLoopLen);
            // how far back in time the loop sample is: loop captured just before smearT = loopLen
            const float delay = (smearT + 1.0f) - posInLoop;
            float loop = smearBuf.read (std::max (1.0f, delay));
            // short fades at loop boundaries
            const float edge = std::min (posInLoop, smearLoopLen - posInLoop);
            loop *= std::min (1.0f, edge / fade);
            const float xf = std::min (1.0f, t / fade);
            y = x * (1.0f - xf) + loop * xf;
        }
        smearT += 1.0f;

        const float blend = std::min (1.0f, s * 4.0f);
        return x * (1.0f - blend) + y * blend;
    }

    //--------------------------------------------------------------------------
    struct Detuner
    {
        DelayLine d;
        float phase = 0.0f, window = 2000.0f;
        void prepare (float sampleRate) { d.prepare ((int) (sampleRate * 0.06f) + 16); window = sampleRate * 0.045f; phase = 0; }
        float process (float x, float ratio)
        {
            d.push (x);
            phase += (1.0f - ratio) / window;
            if (phase >= 1.0f) phase -= 1.0f;
            if (phase < 0.0f) phase += 1.0f;
            float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
            const float a = d.read (2.0f + phase * window);
            const float b = d.read (2.0f + p2 * window);
            return a * std::sin (kPi * phase) + b * std::sin (kPi * p2);
        }
    };

    float allpassTick (int i, float x)
    {
        auto& dl = allpass[(size_t) i];
        const float g = 0.6f;
        const float delayed = dl.read (apDelay[(size_t) i]);
        const float v = x + g * delayed;
        dl.push (v);
        return delayed - g * v;
    }

    //--------------------------------------------------------------------------
    float sr = 44100.0f;
    int latency = 2048;
    ManipParams params;

    DelayLine ring;
    long long ringAbs = 0;
    PitchTracker tracker;

    VocoderEngine vocoder;
    DelayLine vocDelay;

    DelayLine smearBuf;
    float smearT = 0.0f, smearInterval = 0.0f, smearLoopLen = 0.0f;

    std::array<Detuner, 2> detuners;
    std::array<DelayLine, 3> allpass;
    std::array<float, 3> apDelay {};
    float sideHp = 0.0f, sideHpPrev = 0.0f;

    std::array<Voice, kVoices> voices;
    std::array<int, 16> noteStack {};
    int noteStackSize = 0;
    long long orderCounter = 0;

    struct { float robot = 0, detune = 0, stereo = 0, smear = 0, bypass = 0; } smooth;
    float sCoef = 0.001f;
};

} // namespace vbx
