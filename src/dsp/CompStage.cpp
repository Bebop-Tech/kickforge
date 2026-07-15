#include "CompStage.h"

namespace kickforge
{

namespace
{
constexpr double smoothingSeconds = 0.02;
constexpr float autoReleaseMs = 80.0f;
constexpr float maxThresholdDb = -30.0f;
constexpr float maxRatio = 8.0f;

float thresholdFor (float amount01) { return maxThresholdDb * amount01; }
float ratioFor (float amount01) { return 1.0f + (maxRatio - 1.0f) * amount01; }

// Compensation approximative : la moitié de la réduction attendue pour un
// signal proche de 0 dB.
float makeupDbFor (float amount01)
{
    const float ratio = ratioFor (amount01);
    return 0.5f * (-thresholdFor (amount01)) * (1.0f - 1.0f / ratio);
}
} // namespace

void CompStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    compressor.prepare (spec);
    compressor.setRelease (autoReleaseMs);
    makeupLinear.reset (spec.sampleRate, smoothingSeconds);
    reset();
}

void CompStage::setParameters (const Parameters& newParams)
{
    params = newParams;
    const float amount01 = juce::jlimit (0.0f, 100.0f, params.amountPercent) * 0.01f;

    compressor.setThreshold (thresholdFor (amount01));
    compressor.setRatio (ratioFor (amount01));
    compressor.setAttack (params.attackMs);
    makeupLinear.setTargetValue (juce::Decibels::decibelsToGain (makeupDbFor (amount01)));
}

void CompStage::reset()
{
    compressor.reset();
    const float amount01 = juce::jlimit (0.0f, 100.0f, params.amountPercent) * 0.01f;
    makeupLinear.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (makeupDbFor (amount01)));
    gainReductionDb.store (0.0f, std::memory_order_relaxed);
}

void CompStage::process (juce::dsp::AudioBlock<float> block)
{
    const auto numChannels = block.getNumChannels();
    const auto numSamples  = block.getNumSamples();

    auto peakOf = [&block, numChannels, numSamples]
    {
        float peak = 0.0f;
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            const auto* data = block.getChannelPointer (ch);
            for (size_t i = 0; i < numSamples; ++i)
                peak = juce::jmax (peak, std::abs (data[i]));
        }
        return peak;
    };

    const float peakIn = peakOf();

    juce::dsp::ProcessContextReplacing<float> context (block);
    compressor.process (context);

    // GR mesurée avant makeup, crête à crête sur le bloc
    const float peakOut = peakOf();
    const float gr = (peakIn > 1.0e-4f && peakOut > 1.0e-6f)
                         ? juce::jmax (0.0f, juce::Decibels::gainToDecibels (peakIn)
                                                 - juce::Decibels::gainToDecibels (peakOut))
                         : 0.0f;
    gainReductionDb.store (gr, std::memory_order_relaxed);

    for (size_t i = 0; i < numSamples; ++i)
    {
        const float gain = makeupLinear.getNextValue();
        for (size_t ch = 0; ch < numChannels; ++ch)
            block.getChannelPointer (ch)[i] *= gain;
    }
}

} // namespace kickforge
