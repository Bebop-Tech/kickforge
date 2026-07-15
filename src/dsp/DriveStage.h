#pragma once

#include <juce_dsp/juce_dsp.h>

namespace kickforge
{

// Étage de drive : waveshaper (soft clip tanh / hard clip / wavefolder) avec
// pré-gain 1x-30x (mapping exponentiel du knob), traité en oversampling x2
// (polyphase IIR, latence négligeable), suivi d'un filtre "tone" low-pass
// 12 dB/oct au rythme d'échantillonnage de base.
//
// Traite un bloc MONO. Tout est pré-alloué dans prepare().
class DriveStage
{
public:
    enum class Type : int
    {
        soft = 0,
        hard,
        fold
    };

    struct Parameters
    {
        Type type           = Type::hard;
        float amountPercent = 65.0f;
        float toneHz        = 2400.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void setParameters (const Parameters& newParams);
    void reset(); // fige les smoothers sur les paramètres courants

    float getLatencyInSamples() const { return oversampling.getLatencyInSamples(); }

    void process (juce::dsp::AudioBlock<float> block);

private:
    Parameters params;

    juce::dsp::Oversampling<float> oversampling {
        1, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
    };
    juce::dsp::StateVariableTPTFilter<float> toneFilter;

    juce::SmoothedValue<float> gainSmoothed { 1.0f };   // avance au rythme oversamplé
    juce::SmoothedValue<float> toneSmoothed { 2400.0f };
};

} // namespace kickforge
