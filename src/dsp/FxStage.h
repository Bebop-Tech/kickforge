#pragma once

#include <juce_dsp/juce_dsp.h>

namespace kickforge
{

// FX v2 : le chorus traite le bus principal (mix complet, dry/wet interne) ;
// la reverb est un SEND alimenté par busAP (attack + punch) uniquement — le
// crunch reste sec. Son wet stéréo est sommé dans le bus principal, avant le
// compresseur (comp/clip/width traitent aussi la queue).
class FxStage
{
public:
    struct Parameters
    {
        float chorusMixPercent = 0.0f;
        float reverbMixPercent = 10.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void setParameters (const Parameters& newParams);
    void reset();

    // mainStereo : bus principal (2 canaux), modifié en place.
    // busAP : send mono (attack + punch), lu seulement.
    void process (juce::dsp::AudioBlock<float> mainStereo, const float* busAP,
                  size_t numSamples);

private:
    Parameters params;

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> reverbScratch; // wet stéréo pré-alloué
};

} // namespace kickforge
