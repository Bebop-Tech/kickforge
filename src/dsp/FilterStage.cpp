#include "FilterStage.h"

namespace kickforge
{

namespace
{
constexpr double smoothingSeconds = 0.02;
constexpr float minQ = 0.70710678f; // Butterworth, neutre
constexpr float maxQ = 8.0f;        // plafonné : pas d'auto-oscillation en v1

float qFromResoPercent (float resoPercent)
{
    const float t = juce::jlimit (0.0f, 90.0f, resoPercent) / 90.0f;
    return minQ * std::pow (maxQ / minQ, t);
}

juce::dsp::StateVariableTPTFilterType toJuceType (FilterStage::Type type)
{
    switch (type)
    {
        case FilterStage::Type::lowpass:  return juce::dsp::StateVariableTPTFilterType::lowpass;
        case FilterStage::Type::highpass: return juce::dsp::StateVariableTPTFilterType::highpass;
        case FilterStage::Type::bandpass: return juce::dsp::StateVariableTPTFilterType::bandpass;
    }
    return juce::dsp::StateVariableTPTFilterType::lowpass;
}
} // namespace

void FilterStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    jassert (spec.numChannels == 1);

    sampleRate = spec.sampleRate;
    filter.prepare (spec);

    cutoffSmoothed.reset (spec.sampleRate, smoothingSeconds);
    qSmoothed.reset (spec.sampleRate, smoothingSeconds);

    reset();
}

void FilterStage::setParameters (const Parameters& newParams)
{
    params = newParams;
    cutoffSmoothed.setTargetValue (juce::jmin (params.cutoffHz, maxCutoffHz()));
    qSmoothed.setTargetValue (qFromResoPercent (params.resoPercent));
}

void FilterStage::reset()
{
    filter.reset();
    cutoffSmoothed.setCurrentAndTargetValue (juce::jmin (params.cutoffHz, maxCutoffHz()));
    qSmoothed.setCurrentAndTargetValue (qFromResoPercent (params.resoPercent));

    filter.setType (toJuceType (params.type));
    filter.setCutoffFrequency (cutoffSmoothed.getCurrentValue());
    filter.setResonance (qSmoothed.getCurrentValue());
}

void FilterStage::process (juce::dsp::AudioBlock<float> block)
{
    jassert (block.getNumChannels() == 1);

    const auto numSamples = static_cast<int> (block.getNumSamples());
    filter.setType (toJuceType (params.type));
    filter.setCutoffFrequency (cutoffSmoothed.skip (numSamples));
    filter.setResonance (qSmoothed.skip (numSamples));

    juce::dsp::ProcessContextReplacing<float> context (block);
    filter.process (context);
}

} // namespace kickforge
