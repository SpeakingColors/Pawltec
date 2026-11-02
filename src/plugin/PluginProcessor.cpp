#include "PluginProcessor.h"
#include "../editor/PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout PawltecProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const int v = 1; // parameter version

    auto knob = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, v }, name, NormalisableRange<float> (0.0f, 10.0f, 0.01f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (x, 1); })
                                           .withValueFromStringFunction ([] (const String& s) { return jlimit (0.0f, 10.0f, s.getFloatValue()); })));
    };
    auto freqs = [] (const double* f, int n, bool khz)
    {
        StringArray a;
        for (int i = 0; i < n; ++i) a.add (khz ? String (f[i] / 1000.0, f[i] / 1000.0 == std::floor (f[i] / 1000.0) ? 0 : 1) + " kHz" : String ((int) f[i]) + " Hz");
        return a;
    };
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::lowF, v }, "Low Frequency", freqs (pawl::kLowF, 4, false), 2));
    knob (ids::lowB, "Low Boost", 0.0f);
    knob (ids::lowA, "Low Atten", 0.0f);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::hiF, v }, "High Boost Frequency", freqs (pawl::kHiF, 7, true), 4));
    knob (ids::hiB, "High Boost", 0.0f);
    knob (ids::bw, "High Boost Width", 5.0f);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::attF, v }, "High Atten Frequency", freqs (pawl::kAttF, 3, true), 2));
    knob (ids::hiA, "High Atten", 0.0f);
    knob (ids::tube, "Tube", 0.0f);
    knob (ids::out, "Output", 5.0f);
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::bypass, v }, "Bypass", false));
    return { p.begin(), p.end() };
}

PawltecProcessor::PawltecProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PAWLTEC", createLayout())
{
    const char* all[11] = { ids::lowF, ids::lowB, ids::lowA, ids::hiF, ids::hiB, ids::bw, ids::attF, ids::hiA, ids::tube, ids::out, ids::bypass };
    for (int i = 0; i < 11; ++i) raw[(size_t) i] = apvts.getRawParameterValue (all[i]);
    setLatencySamples (pawl::kLatencySamples);
}

pawl::Params PawltecProcessor::readParams() const
{
    pawl::Params q;
    q.lowF = (int) raw[0]->load();  q.lowB = raw[1]->load();  q.lowA = raw[2]->load();
    q.hiF = (int) raw[3]->load();   q.hiB = raw[4]->load();   q.bw = raw[5]->load();
    q.attF = (int) raw[6]->load();  q.hiA = raw[7]->load();   q.tube = raw[8]->load();
    q.out = raw[9]->load();         q.bypass = raw[10]->load() > 0.5f;
    return q;
}

bool PawltecProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet();
}

var PawltecProcessor::getSetting (const char* key, const var& fallback) const
{
    return apvts.state.getProperty (key, fallback);
}

void PawltecProcessor::setSetting (const char* key, const var& v)
{
    apvts.state.setProperty (key, v, nullptr);
}

void PawltecProcessor::prepareToPlay (double sampleRate, int)
{
    core.prepare (sampleRate);
    core.setParams (readParams());
    setLatencySamples (core.latency());
}

void PawltecProcessor::copyRings (float* inDest, float* outDest) const
{
    const int end = ringPos.load (std::memory_order_acquire);
    for (int i = 0; i < kRing; ++i)
    {
        const int k = (end + i) & (kRing - 1);   // oldest first
        inDest[i] = ringIn[(size_t) k];
        outDest[i] = ringOut[(size_t) k];
    }
}

void PawltecProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int numCh = jmin (2, buffer.getNumChannels());
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh == 0) return;
    const int n = buffer.getNumSamples();

    float* l = buffer.getWritePointer (0);
    float* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    float inPk = 0.0f, outPk = 0.0f;
    int pos = ringPos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float a = l[i], b = r ? r[i] : a;
        inPk = jmax (inPk, std::abs (a), std::abs (b));
        ringIn[(size_t) ((pos + i) & (kRing - 1))] = 0.5f * (a + b);
    }

    core.setParams (readParams());
    core.process (l, r, n);

    for (int i = 0; i < n; ++i)
    {
        const float a = l[i], b = r ? r[i] : a;
        outPk = jmax (outPk, std::abs (a), std::abs (b));
        ringOut[(size_t) ((pos + i) & (kRing - 1))] = 0.5f * (a + b);
    }
    ringPos.store ((pos + n) & (kRing - 1), std::memory_order_release);
    raiseTo (inPeak, inPk);
    raiseTo (outPeak, outPk);
}

AudioProcessorEditor* PawltecProcessor::createEditor()
{
    return new PawltecEditor (*this);
}

void PawltecProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PawltecProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml)); // unknown keys are ignored by APVTS; older states keep loading
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PawltecProcessor(); }
