#pragma once

#include <juce_dsp/juce_dsp.h>

namespace kickforge
{

// Distorsion parallèle : la branche wet traite une copie du signal
// (tube = waveshaper asymétrique doux, fuzz = gain élevé + hard clip + HPF
// léger, bitcrush = réduction de bit depth + sample rate), avec compensation
// de gain approximative pour que le knob Mix soit utilisable sans saut de
// volume. Mix = crossfade linéaire dry/wet.
//
// Traite un bloc MONO. Le buffer wet est pré-alloué dans prepare().
class DistStage
{
public:
    enum class Type : int
    {
        tube = 0,
        fuzz,
        bitcrush
    };

    struct Parameters
    {
        Type type           = Type::tube;
        float amountPercent = 20.0f;
        float mixPercent    = 40.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void setParameters (const Parameters& newParams);
    void reset(); // fige les smoothers sur les paramètres courants

    void process (juce::dsp::AudioBlock<float> block);

private:
    void renderWet (const float* dry, float* wet, int numSamples);

    Parameters params;
    double sampleRate = 48000.0;

    juce::AudioBuffer<float> wetBuffer;
    juce::SmoothedValue<float> mixSmoothed { 0.4f };
    juce::SmoothedValue<float> amountSmoothed { 20.0f };

    // États des filtres one-pole de la branche wet
    double tubeDcState = 0.0;   // DC blocker (l'asymétrie crée du DC)
    double fuzzHpState = 0.0;   // HPF léger post-fuzz
    double tubeDcCoeff = 0.0;
    double fuzzHpCoeff = 0.0;

    // État du bitcrush (sample & hold)
    int holdCounter  = 0;
    float heldSample = 0.0f;
};

} // namespace kickforge
