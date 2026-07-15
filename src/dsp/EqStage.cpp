#include "EqStage.h"

namespace kickforge
{

namespace
{
constexpr double smoothingSeconds = 0.02;
constexpr float epsilon = 1.0e-3f;
} // namespace

void EqStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    jassert (spec.numChannels == 1);
    sampleRate = spec.sampleRate;

    for (int band = 0; band < 3; ++band)
    {
        // Allocation unique du bloc de coefficients (5 valeurs, ordre 2) ;
        // ensuite tout se recalcule en place sur le thread audio.
        filters[band].coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, params.bands[band].freqHz, params.bands[band].q,
            juce::Decibels::decibelsToGain (params.bands[band].gainDb));
        filters[band].prepare (spec);

        gainSmoothed[band].reset (spec.sampleRate, smoothingSeconds);
    }

    reset();
}

void EqStage::setParameters (const Parameters& newParams)
{
    params = newParams;
    for (int band = 0; band < 3; ++band)
        gainSmoothed[band].setTargetValue (params.bands[band].gainDb);
}

void EqStage::reset()
{
    for (int band = 0; band < 3; ++band)
    {
        filters[band].reset();
        gainSmoothed[band].setCurrentAndTargetValue (params.bands[band].gainDb);
        updateBandCoefficients (band, params.bands[band].freqHz,
                                params.bands[band].gainDb, params.bands[band].q);
    }
}

// Peak EQ (RBJ Audio EQ Cookbook), écrit en place dans le bloc de
// coefficients existant : aucune allocation.
void EqStage::updateBandCoefficients (int band, float freqHz, float gainDb, float q)
{
    const double A     = std::pow (10.0, gainDb / 40.0);
    const double w0    = juce::MathConstants<double>::twoPi
                         * juce::jlimit (10.0, sampleRate * 0.47, static_cast<double> (freqHz))
                         / sampleRate;
    const double cosW0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * juce::jmax (0.05, static_cast<double> (q)));

    const double b0 = 1.0 + alpha * A;
    const double b1 = -2.0 * cosW0;
    const double b2 = 1.0 - alpha * A;
    const double a0 = 1.0 + alpha / A;
    const double a1 = -2.0 * cosW0;
    const double a2 = 1.0 - alpha / A;

    auto& c = filters[band].coefficients->coefficients; // [b0, b1, b2, a1, a2] normalisés
    c.setUnchecked (0, static_cast<float> (b0 / a0));
    c.setUnchecked (1, static_cast<float> (b1 / a0));
    c.setUnchecked (2, static_cast<float> (b2 / a0));
    c.setUnchecked (3, static_cast<float> (a1 / a0));
    c.setUnchecked (4, static_cast<float> (a2 / a0));

    appliedFreq[band] = freqHz;
    appliedGain[band] = gainDb;
    appliedQ[band]    = q;
}

void EqStage::process (juce::dsp::AudioBlock<float> block)
{
    jassert (block.getNumChannels() == 1);
    const auto numSamples = static_cast<int> (block.getNumSamples());

    for (int band = 0; band < 3; ++band)
    {
        const float gainNow = gainSmoothed[band].skip (numSamples);
        const auto& target  = params.bands[band];

        if (std::abs (gainNow - appliedGain[band]) > epsilon
            || std::abs (target.freqHz - appliedFreq[band]) > epsilon
            || std::abs (target.q - appliedQ[band]) > epsilon)
        {
            updateBandCoefficients (band, target.freqHz, gainNow, target.q);
        }

        juce::dsp::ProcessContextReplacing<float> context (block);
        filters[band].process (context);
    }
}

} // namespace kickforge
