#pragma once

// VoxBot - X vocoder engine (no JUCE dependency, so it can be tested offline).
//
// Signal flow (mono):
//   voice ──┬─► pitch tracker (YIN) ──► carrier synth (saw / square / noise, up to 3 voices)
//           │                                   │
//           └─► 20 analysis bands ──► envelopes ─┴─► 20 synthesis bands (formant-shifted)
//                                                     │
//           voice high-pass (sibilance) ──────────────┴─► wet out

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace vbx
{
constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

//==============================================================================
struct Preset
{
    const char* name;
    uint32_t    colour;     // ARGB, used for LEDs + face
    int         wave;       // 0 = saw, 1 = square, 2 = noise only
    int         pitchMode;  // 0 = fixed note, 1 = follow voice, 2 = follow + snap to semitone
    float       fixedHz;
    int         numVoices;
    float       semis[3];
    float       gains[3];
    float       formant;    // synthesis band shift (1 = natural, <1 deeper, >1 smaller)
    float       glideMs;
    float       noise;      // extra noise in the carrier (breath)
    float       ringHz;     // 0 = off
    float       releaseMs;  // band envelope release
    int         crush;      // 1 = 8-bit style sample/bit reduction
};

constexpr int kNumPresets = 8;

inline const std::array<Preset, kNumPresets>& presets()
{
    static const std::array<Preset, kNumPresets> p {{
        //  name        colour       wave mode  fixHz  nV  semis               gains              form  glide noise ring  rel  crush
        { "ROBOT",     0xff19e3ff,  0,   0,    110.f, 1, { 0,   0,  0 },    { 1,   0,   0 },    1.00f,  0.f, 0.03f,  0.f, 22.f, 0 },
        { "ANDROID",   0xff35ff8a,  0,   1,    0.f,   1, { 0,   0,  0 },    { 1,   0,   0 },    1.00f, 12.f, 0.04f,  0.f, 25.f, 0 },
        { "CHOIR BOT", 0xffffd23a,  0,   2,    0.f,   3, { 0,   7, 12 },    { 1, .6f, .45f },   1.00f, 25.f, 0.03f,  0.f, 35.f, 0 },
        { "TITAN",     0xffff4a3a,  0,   1,    0.f,   2, {-12,-24,  0 },    { 1, .5f,  0 },     0.72f, 15.f, 0.02f,  0.f, 30.f, 0 },
        { "ALIEN",     0xffb46bff,  0,   1,    0.f,   2, { 12, 19,  0 },    { 1, .35f, 0 },     1.38f, 10.f, 0.03f, 38.f, 20.f, 0 },
        { "GHOST",     0xffd9e4ff,  2,   1,    0.f,   1, { 0,   0,  0 },    { 1,   0,   0 },    1.05f,  0.f, 0.00f,  0.f, 45.f, 0 },
        { "DISCO",     0xffff4fd8,  1,   2,    0.f,   2, { 0,  12,  0 },    { 1, .35f, 0 },     1.00f,  4.f, 0.02f,  0.f, 18.f, 0 },
        { "8-BIT",     0xffff9a1a,  1,   2,    0.f,   1, { 0,   0,  0 },    { 1,   0,   0 },    1.10f,  0.f, 0.00f,  0.f, 15.f, 1 },
    }};
    return p;
}

//==============================================================================
// Topology-preserving-transform state variable filter (Cytomic / Simper).
struct SVF
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;

    void set (float fc, float q, float sr)
    {
        fc = std::clamp (fc, 20.0f, 0.45f * sr);
        const float g = std::tan (kPi * fc / sr);
        k  = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline void tick (float v0, float& bp, float& lp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        bp = v1; lp = v2;
    }

    inline float bandpass (float v0) { float b, l; tick (v0, b, l); return k * b; } // unity peak
    inline float lowpass  (float v0) { float b, l; tick (v0, b, l); return l; }
    inline float highpass (float v0) { float b, l; tick (v0, b, l); return v0 - k * b - l; }

    void reset() { ic1 = ic2 = 0; }
};

//==============================================================================
// YIN pitch tracker running on a decimated copy of the voice.
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        decim  = std::max (1, (int) std::lround (sampleRate / 11025.0));
        dsr    = (float) sampleRate / (float) decim;
        tauMin = std::max (2, (int) std::floor (dsr / 1100.0f));
        tauMax = (int) std::ceil (dsr / 60.0f);
        integ  = std::max (256, tauMax * 2);
        winLen = integ + tauMax + 2;
        hop    = std::max (32, (int) (dsr * 0.006f));

        ring.assign ((size_t) winLen, 0.0f);
        frame.assign ((size_t) winLen, 0.0f);
        cmnd.assign ((size_t) tauMax + 2, 1.0f);
        aa.set (2200.0f, 0.707f, (float) sampleRate);
        aa2.set (2200.0f, 0.707f, (float) sampleRate);
        aa.reset(); aa2.reset();
        writePos = 0; decimCount = 0; hopCount = 0;
        hz = 0; voiced = false;
    }

    inline void push (float x)
    {
        x = aa2.lowpass (aa.lowpass (x));
        if (++decimCount < decim) return;
        decimCount = 0;

        ring[(size_t) writePos] = x;
        writePos = (writePos + 1) % winLen;

        if (++hopCount >= hop) { hopCount = 0; analyse(); }
    }

    float hz = 0.0f;
    bool  voiced = false;

private:
    void analyse()
    {
        double energy = 0;
        for (int i = 0; i < winLen; ++i)
        {
            const float s = ring[(size_t) ((writePos + i) % winLen)];
            frame[(size_t) i] = s;
            energy += (double) s * s;
        }

        const float rms = (float) std::sqrt (energy / winLen);
        if (rms < 0.004f) { voiced = false; return; }

        float running = 0.0f;
        cmnd[0] = 1.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float d = 0.0f;
            const float* a = frame.data();
            const float* b = frame.data() + tau;
            for (int j = 0; j < integ; ++j) { const float df = a[j] - b[j]; d += df * df; }
            running += d;
            cmnd[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.18f)
            {
                while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0) { voiced = false; return; }

        float t = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float s0 = cmnd[(size_t) best - 1], s1 = cmnd[(size_t) best], s2 = cmnd[(size_t) best + 1];
            const float den = s0 + s2 - 2.0f * s1;
            if (std::abs (den) > 1e-9f) t += 0.5f * (s0 - s2) / den;
        }

        hz = dsr / t;
        voiced = true;
    }

    SVF aa, aa2;
    int decim = 4, tauMin = 10, tauMax = 184, integ = 368, winLen = 560, hop = 64;
    float dsr = 11025.0f;
    int writePos = 0, decimCount = 0, hopCount = 0;
    std::vector<float> ring, frame, cmnd;
};

//==============================================================================
class VocoderEngine
{
public:
    static constexpr int kBands = 20;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        tracker.prepare (sampleRate);

        const float fLo = 110.0f, fHi = 7800.0f;
        const float r   = std::pow (fHi / fLo, 1.0f / (kBands - 1));
        bandQ = 0.75f * std::sqrt (r) / (r - 1.0f);

        for (int b = 0; b < kBands; ++b)
        {
            auto& band = bands[(size_t) b];
            band.fc = fLo * std::pow (r, (float) b);
            for (auto& f : band.mod) { f.set (band.fc, bandQ, sr); f.reset(); }
            for (auto& f : band.car) f.reset();
            band.modEnv = band.carEnv = 0.0f;
        }

        sibilance.set (5500.0f, 0.707f, sr); sibilance.reset();
        dcBlock = 0; dcPrev = 0;

        envAttack = coef (2.0f);
        carAttack = coef (4.0f);
        carRelease = coef (30.0f);
        voicingCoef = coef (12.0f);

        phases = { 0, 0.33f, 0.66f };
        logHz = std::log (130.0f);
        voicing = 0;
        ringPhase = 0;
        crushCount = 0; crushHold = 0;
        currentFormant = -1.0f;

        setPreset (presetIndex);
        updateFormant (true);
    }

    void setPreset (int index)
    {
        presetIndex = std::clamp (index, 0, kNumPresets - 1);
        p = &presets()[(size_t) presetIndex];
        envRelease = coef (p->releaseMs);
        glideCoef  = p->glideMs <= 0.0f ? 1.0f : coef (p->glideMs);
    }

    int getPreset() const { return presetIndex; }

    // Processes a block of mono voice into a mono wet (vocoded) signal.
    void process (const float* in, float* out, int n)
    {
        updateFormant (false);

        for (int i = 0; i < n; ++i)
        {
            const float x = in[i];
            tracker.push (x);

            // ---- carrier pitch -------------------------------------------------
            float targetHz = p->pitchMode == 0 ? p->fixedHz : tracker.hz;
            if (targetHz < 40.0f) targetHz = std::exp (logHz);  // hold last pitch
            if (p->pitchMode == 2)
                targetHz = 440.0f * std::pow (2.0f, std::round (12.0f * std::log2 (targetHz / 440.0f)) / 12.0f);

            const float targetLog = std::log (targetHz);
            if (p->pitchMode == 0 || tracker.voiced)
                logHz += glideCoef * (targetLog - logHz);

            voicing += voicingCoef * ((tracker.voiced ? 1.0f : 0.0f) - voicing);

            // ---- carrier synth -------------------------------------------------
            const float f0 = std::exp (logHz);
            float osc = 0.0f;
            if (p->wave != 2)
            {
                for (int v = 0; v < p->numVoices; ++v)
                {
                    const float f  = f0 * std::pow (2.0f, p->semis[v] / 12.0f);
                    const float dt = std::min (f / sr, 0.45f);
                    float& ph = phases[(size_t) v];
                    float s;
                    if (p->wave == 0) s = 2.0f * ph - 1.0f - blep (ph, dt);
                    else
                    {
                        s = (ph < 0.5f ? 1.0f : -1.0f) + blep (ph, dt);
                        float ph2 = ph + 0.5f; if (ph2 >= 1.0f) ph2 -= 1.0f;
                        s -= blep (ph2, dt);
                    }
                    osc += s * p->gains[v];
                    ph += dt; if (ph >= 1.0f) ph -= 1.0f;
                }
            }

            const float nz = noise();
            float carrier;
            if (p->wave == 2) carrier = nz;
            else              carrier = osc * (0.35f + 0.65f * voicing)
                                      + nz * ((1.0f - voicing) * 0.8f + p->noise);

            // ---- filter bank -----------------------------------------------------
            float wet = 0.0f;
            for (auto& band : bands)
            {
                const float m  = band.mod[1].bandpass (band.mod[0].bandpass (x));
                const float c  = band.car[1].bandpass (band.car[0].bandpass (carrier));
                const float am = std::abs (m), ac = std::abs (c);

                band.modEnv += (am > band.modEnv ? envAttack : envRelease) * (am - band.modEnv);
                band.carEnv += (ac > band.carEnv ? carAttack : carRelease) * (ac - band.carEnv);

                wet += c * band.modEnv / (band.carEnv + 1e-4f);
            }

            wet *= makeup;

            // Keep consonants ("s", "t") intelligible.
            wet += 0.55f * sibilance.highpass (x);

            if (p->ringHz > 0.0f)
            {
                ringPhase += p->ringHz / sr; if (ringPhase >= 1.0f) ringPhase -= 1.0f;
                wet *= 0.45f + 0.55f * std::sin (kTwoPi * ringPhase);
                wet *= 1.3f;
            }

            if (p->crush)
            {
                const int hold = std::max (1, (int) (sr / 9000.0f));
                if (++crushCount >= hold) { crushCount = 0; crushHold = std::round (wet * 24.0f) / 24.0f; }
                wet = crushHold;
            }

            // DC blocker
            const float y = wet - dcPrev + 0.995f * dcBlock;
            dcPrev = wet; dcBlock = y;

            out[i] = y;
        }
    }

    // Exposed for calibration in tests.
    float makeup = 1.6f;

private:
    struct Band
    {
        float fc = 1000.0f;
        std::array<SVF, 2> mod, car;
        float modEnv = 0.0f, carEnv = 0.0f;
    };

    float coef (float ms) const { return 1.0f - std::exp (-1.0f / (0.001f * ms * sr)); }

    static inline float blep (float t, float dt)
    {
        if (t < dt)        { t /= dt;          return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    inline float noise()
    {
        rng = rng * 1664525u + 1013904223u;
        return ((float) (rng >> 9) / 4194304.0f - 1.0f) * 0.6f;
    }

    void updateFormant (bool force)
    {
        const float target = p->formant;
        if (currentFormant < 0.0f || force) currentFormant = target;
        else currentFormant += 0.25f * (target - currentFormant);   // glide over a few blocks

        if (! force && std::abs (currentFormant - lastAppliedFormant) < 1e-4f) return;
        lastAppliedFormant = currentFormant;

        for (auto& band : bands)
            for (auto& f : band.car) f.set (band.fc * currentFormant, bandQ, sr);
    }

    float sr = 44100.0f;
    PitchTracker tracker;
    std::array<Band, kBands> bands;
    SVF sibilance;
    const Preset* p = &presets()[0];
    int presetIndex = 0;

    float bandQ = 3.0f;
    float envAttack = 0, envRelease = 0, carAttack = 0, carRelease = 0, voicingCoef = 0, glideCoef = 1;
    std::array<float, 3> phases { 0, 0, 0 };
    float logHz = 4.8f, voicing = 0, ringPhase = 0;
    float currentFormant = -1.0f, lastAppliedFormant = -1.0f;
    int crushCount = 0; float crushHold = 0;
    float dcBlock = 0, dcPrev = 0;
    uint32_t rng = 22222u;
};

} // namespace vbx
