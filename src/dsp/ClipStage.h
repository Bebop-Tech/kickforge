#pragma once

#include <juce_dsp/juce_dsp.h>

namespace kickforge
{

// Clipper final : dernier étage non linéaire, remplace le limiter de
// sécurité. Soft = tanh normalisé au ceiling, hard = clip franc.
// Pas d'oversampling en v1 (à réévaluer si aliasing audible en mode hard).
//
// Traite un bloc STÉRÉO.
class ClipStage
{
public:
    enum class Type : int
    {
        soft = 0,
        hard
    };

    struct Parameters
    {
        Type type       = Type::hard;
        float ceilingDb = -0.3f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void setParameters (const Parameters& newParams);
    void reset();

    void process (juce::dsp::AudioBlock<float> block);

private:
    Parameters params;
    juce::SmoothedValue<float> ceilingLinear { 1.0f };
};

} // namespace kickforge
