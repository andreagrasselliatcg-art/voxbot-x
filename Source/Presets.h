#pragma once

#include <vector>

// Factory presets: one per robot face. Values are plain parameter values
// (choice/bool parameters use their index). Anything not listed is reset to its default.

namespace Presets
{
    struct Value   { const char* id; float value; };
    struct Pattern { int slot; int pattern; };
    struct Def     { const char* name; std::vector<Value> values; std::vector<Pattern> patterns; };

    // midiMode: 0 Off, 1 Mono, 2 Poly, 3 M.Gated, 4 P.Gated
    // mN_type:  0 Meta, 1 MIDI, 2 Sequencer, 3 Follower, 4 ADSR
    inline const std::vector<Def>& all()
    {
        static const std::vector<Def> p {
            { "ROBOT",
              { { "robot", 100 }, { "robotType", 0 }, { "stereo", 20 },
                { "m1_on", 1 }, { "m1_type", 3 }, { "m1_folAtk", 2 }, { "m1_folRel", 80 }, { "m1_amt_fm", 0.15f } },
              {} },

            { "ANDROID",
              { { "robot", 45 }, { "robotType", 1 }, { "formant", 2 }, { "harmonics", 1 }, { "ratio", 0.5f },
                { "fm", 12 }, { "stereo", 30 } },
              {} },

            { "CHOIR BOT",
              { { "midiMode", 2 }, { "detune", 45 }, { "stereo", 70 }, { "robot", 25 }, { "robotType", 2 },
                { "formant", -1 }, { "glide", 40 } },
              {} },

            { "TITAN",
              { { "pitch", -12 }, { "smooth", 0 }, { "formant", -4 }, { "detune", 15 }, { "robot", 20 },
                { "robotType", 3 }, { "detect", 0 } },
              {} },

            { "ALIEN",
              { { "pitch", 5 }, { "formant", 7 }, { "harmonics", 2 }, { "ratio", 0.75f }, { "fm", 25 },
                { "robot", 20 }, { "robotType", 4 }, { "stereo", 50 },
                { "m2_on", 1 }, { "m2_type", 2 }, { "m2_seqSync", 1 }, { "m2_seqDiv", 3 }, { "m2_seqShape", 0 },
                { "m2_amt_formant", 0.25f } },
              { { 1, 6 } } },

            { "GHOST",
              { { "smear", 45 }, { "formant", 3 }, { "detune", 35 }, { "stereo", 85 }, { "robot", 40 },
                { "robotType", 5 }, { "mix", 85 },
                { "m3_on", 1 }, { "m3_type", 3 }, { "m3_amt_smear", 0.3f } },
              {} },

            { "DISCO",
              { { "midiMode", 1 }, { "glide", 60 }, { "midiGlide", 1 }, { "harmonics", 1 }, { "ratio", 1 },
                { "fm", 8 }, { "robot", 30 }, { "robotType", 6 }, { "stereo", 40 },
                { "m4_on", 1 }, { "m4_type", 4 }, { "m4_adsrA", 5 }, { "m4_adsrD", 250 }, { "m4_adsrS", 30 },
                { "m4_adsrR", 200 }, { "m4_amt_formant", 0.1f } },
              {} },

            { "8-BIT",
              { { "alternator", 12 }, { "octave", 1 }, { "snap", 1 }, { "robot", 35 }, { "robotType", 7 },
                { "m2_on", 1 }, { "m2_type", 2 }, { "m2_seqSync", 1 }, { "m2_seqDiv", 4 }, { "m2_seqShape", 2 },
                { "m2_amt_pitch", 0.25f } },
              { { 1, 4 } } },
        };
        return p;
    }
}
