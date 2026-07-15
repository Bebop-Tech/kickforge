#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "LookAndFeel.h"

namespace kickforge::ui
{

// Courbe d'EQ interactive : points draggables (freq/gain), bande active
// sélectionnée via les chips B1-B3 ou en cliquant un point, molette = Q de la
// bande active, readout freq/gain/Q en bas.
class EqCurveDisplay : public juce::Component
{
public:
    explicit EqCurveDisplay (juce::AudioProcessorValueTreeState& apvts);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event,
                         const juce::MouseWheelDetails& wheel) override;

private:
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> bandPosition (int band) const;
    void setActiveBand (int band);
    void refreshReadout();

    juce::AudioProcessorValueTreeState& apvts;

    // Valeurs en cache (message thread), mises à jour par les attachments
    float freq[3] {}, gainDb[3] {}, q[3] {};
    std::unique_ptr<juce::ParameterAttachment> freqAttachments[3];
    std::unique_ptr<juce::ParameterAttachment> gainAttachments[3];
    std::unique_ptr<juce::ParameterAttachment> qAttachments[3];

    juce::TextButton bandChips[3]; // textes B1/B2/B3 posés au constructeur
    juce::Label readout;

    int activeBand = 0;
    int draggedBand = -1;
};

} // namespace kickforge::ui
