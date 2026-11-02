#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../plugin/PluginProcessor.h"

class Face;

// Plugin window. The faceplate is a fixed 1200 x 588 drawing that is scaled to the window size;
// the window keeps its proportions and can be dragged from any edge or corner.
class PawltecEditor : public juce::AudioProcessorEditor
{
public:
    explicit PawltecEditor (PawltecProcessor&);
    ~PawltecEditor() override;
    void resized() override;
    void resetSize();
    void openDialogForTesting (const juce::String& name); // used by tools/snapshot.cpp
    void tickForTesting (int frames);                       // runs the display refresh without a timer

    static constexpr int kW = 1200, kH = 588;

private:
    PawltecProcessor& proc;
    std::unique_ptr<Face> face;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableBorderComponent> border;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PawltecEditor)
};
