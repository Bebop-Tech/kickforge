#pragma once

#include <juce_dsp/juce_dsp.h>

#include <atomic>

namespace kickforge
{

// Compresseur macro : le knob "Comp" (0-100 %) pilote simultanément
// threshold (0 -> -30 dB), ratio (1:1 -> 8:1) et makeup compensatoire.
// Attack exposé, release auto (~80 ms). La gain reduction courante est
// exposée à l'UI via un atomic (mesure crête in/out par bloc).
//
// Traite un bloc STÉRÉO.
class CompStage
{
public:
    struct Parameters
    {
        float amountPercent = 35.0f;
        float attackMs      = 8.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void setParameters (const Parameters& newParams);
    void reset();

    void process (juce::dsp::AudioBlock<float> block);

    // Lisible depuis n'importe quel thread (vu-mètre GR de l'UI)
    float getGainReductionDb() const { return gainReductionDb.load (std::memory_order_relaxed); }

private:
    Parameters params;

    juce::dsp::Compressor<float> compressor;
    juce::SmoothedValue<float> makeupLinear { 1.0f };
    std::atomic<float> gainReductionDb { 0.0f };
};

} // namespace kickforge
