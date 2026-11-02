#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Presets use the faceplate units of the mockup: knobs 0..10 and the three frequency switches as indices
// (low 0..3, high boost 0..6, high atten 0..2). Bypass is not part of a preset. Unknown keys are ignored,
// out of range values refuse the preset.
struct Preset
{
    juce::String name, type = "Mix";
    juce::StringArray tags;
    bool factory = false;
    double v[10] { 2, 0, 0, 4, 0, 5, 2, 0, 0, 5 };
};

namespace presets
{
inline constexpr int kCount = 10;
inline const char* const keys[kCount] = { "lowF", "lowB", "lowA", "hiF", "hiB", "bw", "attF", "hiA", "tube", "out" };
inline const double lo[kCount] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
inline const double hi[kCount] = { 3, 10, 10, 6, 10, 10, 2, 10, 10, 10 };
inline const bool isIndex[kCount] = { true, false, false, true, false, false, true, false, false, false };
inline const char* const types[] = { "Start", "Drums", "Bass", "Vocal", "Mix", "Synth", "Master" };
inline constexpr int kNumTypes = 7;

const std::vector<Preset>& factory();
juce::File userFolder();

class Store
{
public:
    Store();
    void reload();                                    // read the user folder
    std::vector<Preset> all() const;                  // factory first, then yours
    const std::vector<Preset>& user() const { return mine; }
    const Preset* find (const juce::String& name) const;
    bool save (const Preset& p, const juce::String& replaceName = {}); // writes the file, replaces a same-named one
    bool remove (const juce::String& name);
    juce::String exportJson (const std::vector<Preset>& list) const;
    int importJson (const juce::String& text);        // returns count, or -1 when invalid
private:
    std::vector<Preset> mine;
};

bool valid (const juce::var& presetObject);
Preset fromVar (const juce::var& o);
juce::var toVar (const Preset& p);
} // namespace presets
