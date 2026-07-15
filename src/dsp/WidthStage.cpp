#include "WidthStage.h"

namespace kickforge
{

namespace
{
constexpr double smoothingSeconds = 0.02;
constexpr double crossoverHz = 150.0;
constexpr double linkwitzRileyQ = 0.5; // LR2 = 2 Butterworth 1er ordre en cascade
} // namespace

void WidthStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    jassert (spec.numChannels == 2);

    // High-pass RBJ à Q = 0.5 : réponse Linkwitz-Riley 2e ordre
    const double w0    = juce::MathConstants<double>::twoPi * crossoverHz / spec.sampleRate;
    const double cosW0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * linkwitzRileyQ);
    const double a0    = 1.0 + alpha;

    hpB0 = ((1.0 + cosW0) / 2.0) / a0;
    hpB1 = (-(1.0 + cosW0)) / a0;
    hpB2 = ((1.0 + cosW0) / 2.0) / a0;
    hpA1 = (-2.0 * cosW0) / a0;
    hpA2 = (1.0 - alpha) / a0;

    widthSmoothed.reset (spec.sampleRate, smoothingSeconds);
    reset();
}

void WidthStage::setParameters (const Parameters& newParams)
{
    params = newParams;
    widthSmoothed.setTargetValue (params.widthPercent * 0.01f);
    safetyCeiling = juce::Decibels::decibelsToGain (params.safetyCeilingDb);
}

void WidthStage::reset()
{
    widthSmoothed.setCurrentAndTargetValue (params.widthPercent * 0.01f);
    hpZ1 = 0.0;
    hpZ2 = 0.0;
}

void WidthStage::process (juce::dsp::AudioBlock<float> block)
{
    jassert (block.getNumChannels() == 2);

    auto* left  = block.getChannelPointer (0);
    auto* right = block.getChannelPointer (1);
    const auto numSamples = block.getNumSamples();

    for (size_t i = 0; i < numSamples; ++i)
    {
        const double mid  = 0.5 * (left[i] + right[i]);
        const double side = 0.5 * (left[i] - right[i]);

        // Le side sous 150 Hz est jeté (sub mono) ; au-dessus il est dosé
        const double sideHp = hpB0 * side + hpZ1;
        hpZ1 = hpB1 * side - hpA1 * sideHp + hpZ2;
        hpZ2 = hpB2 * side - hpA2 * sideHp;

        const double s = sideHp * widthSmoothed.getNextValue();
        left[i]  = juce::jlimit (-safetyCeiling, safetyCeiling, static_cast<float> (mid + s));
        right[i] = juce::jlimit (-safetyCeiling, safetyCeiling, static_cast<float> (mid - s));
    }
}

} // namespace kickforge
