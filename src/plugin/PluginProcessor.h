#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../core/Pawltec.h"
#include <array>
#include <atomic>

// Stable parameter IDs. Never rename these (saved projects and automation use them).
namespace ids
{
    inline constexpr const char* lowF = "lowF";
    inline constexpr const char* lowB = "lowB";
    inline constexpr const char* lowA = "lowA";
    inline constexpr const char* hiF = "hiF";
    inline constexpr const char* hiB = "hiB";
    inline constexpr const char* bw = "bw";
    inline constexpr const char* attF = "attF";
    inline constexpr const char* hiA = "hiA";
    inline constexpr const char* tube = "tube";
    inline constexpr const char* out = "out";
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* limit = "limit";   // added in 0.1.1: safety limiter at 0 dBFS, on by default
}

// Plugin-wide settings kept in the saved state (not presets, not automatable).
namespace setting
{
    inline constexpr const char* look = "look";       // "A", "B", "C"
    inline constexpr const char* tips = "tips";       // info bubbles
    inline constexpr const char* power = "power";     // power saving redraw
    inline constexpr const char* scale = "scale";     // window scale, 1.0 = 100 %
    inline constexpr const char* preset = "preset";   // name of the loaded preset
}

class PawltecProcessor : public juce::AudioProcessor
{
public:
    PawltecProcessor();
    ~PawltecProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pawltec"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (ids::bypass); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Meter and analysis data for the editor (written on the audio thread, read by the editor timer).
    static constexpr int kRing = 4096;
    std::atomic<float> inPeak { 0.0f };      // linear peak since the editor last read it (exchange to 0), input
    std::atomic<float> outPeak { 0.0f };     // same for the output
    void copyRings (float* inDest, float* outDest) const;   // the last kRing samples of the mono sum, before and after

    // Settings stored in the state tree.
    juce::var getSetting (const char* key, const juce::var& fallback) const;
    void setSetting (const char* key, const juce::var& v);
    static void raiseTo (std::atomic<float>& a, float v) { float c = a.load(); while (v > c && ! a.compare_exchange_weak (c, v)) {} }

    pawl::Params readParams() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    pawl::Engine core;
    std::array<std::atomic<float>*, 12> raw {};
    std::array<float, kRing> ringIn {}, ringOut {};
    std::atomic<int> ringPos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PawltecProcessor)
};
