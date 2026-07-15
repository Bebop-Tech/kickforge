#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "SectionPanel.h"

namespace kickforge::ui
{

// Barre de presets du header : 5 boutons de genre, le genre actif en orange.
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (juce::AudioProcessorValueTreeState& apvts);

    void resized() override;

private:
    SegmentedControl genreControl;
};

} // namespace kickforge::ui
