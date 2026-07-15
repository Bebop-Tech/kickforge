#pragma once

#include <juce_dsp/juce_dsp.h>

namespace kickforge
{

// Largeur stéréo mid/side avec crossover à 150 Hz (Linkwitz-Riley 2e ordre) :
// le side n'est conservé et dosé qu'au-dessus du crossover, le sub reste mono
// quel que soit le réglage. 0 % = mono total, 100 % = stéréo native (haut du
// spectre), 150 % = élargi.
//
// Traite un bloc STÉRÉO.
class WidthStage
{
public:
    struct Parameters
    {
        float widthPercent = 100.0f;

        // Garde de sécurité : le crossover peut faire ré-émerger des crêtes
        // au-dessus du ceiling du clipper (~0.2 dB) ; on re-clampe ici pour
        // que le ceiling tienne jusqu'à la sortie du width.
        float safetyCeilingDb = 0.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void setParameters (const Parameters& newParams);
    void reset();

    void process (juce::dsp::AudioBlock<float> block);

private:
    Parameters params;
    juce::SmoothedValue<float> widthSmoothed { 1.0f };
    float safetyCeiling = 1.0f;

    // Biquad high-pass LR2 (RBJ, Q = 0.5) sur le canal side, forme directe II transposée
    double hpB0 = 1.0, hpB1 = 0.0, hpB2 = 0.0, hpA1 = 0.0, hpA2 = 0.0;
    double hpZ1 = 0.0, hpZ2 = 0.0;
};

} // namespace kickforge
