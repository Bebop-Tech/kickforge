#pragma once

#include <juce_dsp/juce_dsp.h>

namespace kickforge
{

// Filtre multimode créatif post-drive : StateVariableTPTFilter (LP/HP/BP),
// 12 dB/oct. La résonance (0-90 %) est mappée exponentiellement sur un Q de
// 0.707 à 8 — plafonné pour rester stable, pas d'auto-oscillation en v1.
//
// Traite un bloc MONO. Tout est pré-alloué dans prepare().
class FilterStage
{
public:
    enum class Type : int
    {
        lowpass = 0,
        highpass,
        bandpass
    };

    struct Parameters
    {
        Type type         = Type::lowpass;
        float cutoffHz    = 20000.0f;
        float resoPercent = 20.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void setParameters (const Parameters& newParams);
    void reset(); // fige les smoothers sur les paramètres courants

    void process (juce::dsp::AudioBlock<float> block);

private:
    float maxCutoffHz() const { return static_cast<float> (sampleRate) * 0.47f; }

    Parameters params;
    double sampleRate = 48000.0;

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> cutoffSmoothed { 20000.0f };
    juce::SmoothedValue<float> qSmoothed { 0.707f };
};

} // namespace kickforge
