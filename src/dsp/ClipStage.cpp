#include "ClipStage.h"

namespace kickforge
{

namespace
{
constexpr double smoothingSeconds = 0.02;
} // namespace

void ClipStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    ceilingLinear.reset (spec.sampleRate, smoothingSeconds);
    reset();
}

void ClipStage::setParameters (const Parameters& newParams)
{
    params = newParams;
    ceilingLinear.setTargetValue (juce::Decibels::decibelsToGain (params.ceilingDb));
}

void ClipStage::reset()
{
    ceilingLinear.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.ceilingDb));
}

void ClipStage::process (juce::dsp::AudioBlock<float> block)
{
    const auto numChannels = block.getNumChannels();
    const auto numSamples  = block.getNumSamples();
    const auto type = params.type;

    for (size_t i = 0; i < numSamples; ++i)
    {
        const float ceiling = ceilingLinear.getNextValue();

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto& sample = block.getChannelPointer (ch)[i];
            sample = type == Type::soft
                         ? ceiling * std::tanh (sample / ceiling)
                         : juce::jlimit (-ceiling, ceiling, sample);
        }
    }
}

} // namespace kickforge
