#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// Central list of every parameter of VoxBot - X v2.

namespace Params
{
    //--------------------------------------------------------------------------
    // Main controls that can be modulated.
    struct Target
    {
        const char* id;
        const char* name;
        const char* shortName;
        float min, max, def, step;
        const char* unit;
    };

    enum TargetIndex { tPitch = 0, tFormant, tSmooth, tHarmonics, tRatio, tFm, tAlternator, tOctave,
                       tSmear, tDetune, tStereo, tRobot, tMix, kNumTargets };

    inline const std::array<Target, kNumTargets>& targets()
    {
        static const std::array<Target, kNumTargets> t {{
            { "pitch",      "Pitch",          "PITCH",      -24.0f, 24.0f,   0.0f, 0.01f,  "st" },
            { "formant",    "Formant",        "FORMANT",    -24.0f, 24.0f,   0.0f, 0.01f,  "st" },
            { "smooth",     "Smooth Grains",  "SMOOTH",       0.0f, 100.0f, 100.0f, 0.1f,  "%"  },
            { "harmonics",  "Harmonics",      "HARMONICS",   -8.0f,  8.0f,   0.0f, 0.001f, ""   },
            { "ratio",      "Ratio",          "RATIO",        0.0f,  2.0f,   1.0f, 0.001f, ""   },
            { "fm",         "FM",             "FM",           0.0f, 100.0f,  0.0f, 0.1f,   "%"  },
            { "alternator", "Alternator",     "ALTERNATOR",   0.0f, 24.0f,   0.0f, 0.01f,  "st" },
            { "octave",     "Octave",         "OCTAVE",      -8.0f,  8.0f,   0.0f, 1.0f,   ""   },
            { "smear",      "Smear",          "SMEAR",        0.0f, 100.0f,  0.0f, 0.1f,   "%"  },
            { "detune",     "Detune",         "DETUNE",       0.0f, 100.0f,  0.0f, 0.1f,   "%"  },
            { "stereo",     "Stereo",         "STEREO",       0.0f, 100.0f,  0.0f, 0.1f,   "%"  },
            { "robot",      "Robot",          "ROBOT",        0.0f, 100.0f,  0.0f, 0.1f,   "%"  },
            { "mix",        "Dry/Wet",        "DRY/WET",      0.0f, 100.0f, 100.0f, 0.1f,  "%"  },
        }};
        return t;
    }

    //--------------------------------------------------------------------------
    // Global (non-modulated) controls
    inline constexpr const char* robotType     = "robotType";
    inline constexpr const char* detect        = "detect";
    inline constexpr const char* snap          = "snap";
    inline constexpr const char* midiMode      = "midiMode";
    inline constexpr const char* glide         = "glide";
    inline constexpr const char* midiOnlyGlide = "midiGlide";
    inline constexpr const char* gateAttack    = "gateAttack";
    inline constexpr const char* gateRelease   = "gateRelease";
    inline constexpr const char* output        = "output";

    inline const juce::StringArray detectNames   { "LOW", "MID", "HIGH" };
    inline const juce::StringArray midiModeNames { "Off", "Mono", "Poly", "M.Gated", "P.Gated" };

    //--------------------------------------------------------------------------
    // Modulation slots
    constexpr int kSlots = 4;
    constexpr int kSeqSteps = 16;

    enum SourceType { srcMeta = 0, srcMidi, srcSequencer, srcFollower, srcAdsr };

    inline const juce::StringArray sourceNames   { "Meta Knob", "MIDI", "Sequencer", "Follower", "ADSR" };
    inline const juce::StringArray polarityNames { "+", "-", "B" };
    inline const juce::StringArray stepNames     { "-", "2", "3", "4", "5", "6", "8", "12", "16", "24", "32", "64", "128" };
    inline const int stepCounts[] = { 0, 2, 3, 4, 5, 6, 8, 12, 16, 24, 32, 64, 128 };
    inline const juce::StringArray midiSrcNames  { "Note", "Velocity", "Pitch Wheel", "Mod Wheel", "After Touch" };
    inline const juce::StringArray divNames      { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                                   "1/4T", "1/8T", "1/16T", "1/4D", "1/8D", "1/16D" };
    inline const double divBeats[] = { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                       2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75, 0.375 };

    inline juce::String slotId (int slot, const juce::String& name) { return "m" + juce::String (slot + 1) + "_" + name; }
    inline juce::String amountId (int slot, int target)            { return slotId (slot, juce::String ("amt_") + targets()[(size_t) target].id); }
    inline juce::String crossId (int slot, int other)              { return slotId (slot, "x" + juce::String (other + 1)); }

    inline const std::array<juce::Colour, kSlots> slotColours {{
        juce::Colour (0xff19e3ff), juce::Colour (0xffff4fd8), juce::Colour (0xffffd23a), juce::Colour (0xff35ff8a)
    }};

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
