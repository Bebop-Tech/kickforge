#pragma once

#include <juce_dsp/juce_dsp.h>

namespace kickforge
{

// EQ 3 bandes entièrement paramétrique (peak). Les coefficients sont
// recalculés en place (formules RBJ, aucune allocation) et seulement quand un
// paramètre effectif change ; le gain est lissé (~20 ms), donc pendant une
// rampe le recalcul se fait par bloc puis s'arrête.
//
// Traite un bloc MONO.
class EqStage
{
public:
    struct BandParameters
    {
        float freqHz = 1000.0f;
        float gainDb = 0.0f;
        float q      = 0.8f;
    };

    struct Parameters
    {
        // Défauts du brief : B1 = 60 Hz +3 dB, B2 = 800 Hz -2 dB, B3 = 6 kHz +1.5 dB
        BandParameters bands[3] { { 60.0f, 3.0f, 0.8f },
                                  { 800.0f, -2.0f, 0.8f },
                                  { 6000.0f, 1.5f, 0.8f } };
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void setParameters (const Parameters& newParams);
    void reset(); // fige les smoothers sur les paramètres courants

    void process (juce::dsp::AudioBlock<float> block);

private:
    void updateBandCoefficients (int band, float freqHz, float gainDb, float q);

    Parameters params;
    double sampleRate = 48000.0;

    juce::dsp::IIR::Filter<float> filters[3];
    juce::SmoothedValue<float> gainSmoothed[3];

    // Dernières valeurs réellement appliquées aux coefficients
    float appliedFreq[3] {};
    float appliedGain[3] {};
    float appliedQ[3] {};
};

} // namespace kickforge
