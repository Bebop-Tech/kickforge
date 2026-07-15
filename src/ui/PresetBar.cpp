#include "PresetBar.h"

namespace kickforge::ui
{

PresetBar::PresetBar (juce::AudioProcessorValueTreeState& apvts)
    : genreControl (*apvts.getParameter ("genre"),
                    { "Techno", "Hard techno", "Hardstyle", "EDM", "Trance" })
{
    addAndMakeVisible (genreControl);
}

void PresetBar::resized()
{
    genreControl.setBounds (getLocalBounds());
}

} // namespace kickforge::ui
