#include "DriveStage.h"

namespace kickforge
{

namespace
{
constexpr float maxDriveGain = 30.0f;
constexpr double smoothingSeconds = 0.02;

float driveGainFromPercent (float percent)
{
    // Mapping exponentiel 1x -> 30x : progression perceptivement régulière
    return std::pow (maxDriveGain, juce::jlimit (0.0f, 100.0f, percent) / 100.0f);
}

// Repliement triangulaire : identité sur [-1, 1], réflexion au-delà
float foldSample (float x)
{
    x = std::fmod (x + 3.0f, 4.0f);
    if (x < 0.0f)
        x += 4.0f;
    return std::abs (x - 2.0f) - 1.0f;
}

float shapeSample (DriveStage::Type type, float x)
{
    switch (type)
    {
        case DriveStage::Type::soft: return std::tanh (x);
        case DriveStage::Type::hard: return juce::jlimit (-1.0f, 1.0f, x);
        case DriveStage::Type::fold: return foldSample (x);
    }
    return x;
}
} // namespace

void DriveStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    jassert (spec.numChannels == 1);

    oversampling.initProcessing (spec.maximumBlockSize);
    oversampling.reset();

    toneFilter.prepare (spec);
    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    // Le gain est lissé dans le domaine oversamplé (x2)
    gainSmoothed.reset (spec.sampleRate * 2.0, smoothingSeconds);
    toneSmoothed.reset (spec.sampleRate, smoothingSeconds);

    reset();
}

void DriveStage::setParameters (const Parameters& newParams)
{
    params = newParams;
    gainSmoothed.setTargetValue (driveGainFromPercent (params.amountPercent));
    toneSmoothed.setTargetValue (params.toneHz);
}

void DriveStage::reset()
{
    oversampling.reset();
    toneFilter.reset();
    gainSmoothed.setCurrentAndTargetValue (driveGainFromPercent (params.amountPercent));
    toneSmoothed.setCurrentAndTargetValue (params.toneHz);
}

void DriveStage::process (juce::dsp::AudioBlock<float> block)
{
    jassert (block.getNumChannels() == 1);

    auto osBlock = oversampling.processSamplesUp (block);

    auto* data = osBlock.getChannelPointer (0);
    const auto numOsSamples = osBlock.getNumSamples();
    const auto type = params.type;

    for (size_t i = 0; i < numOsSamples; ++i)
        data[i] = shapeSample (type, data[i] * gainSmoothed.getNextValue());

    oversampling.processSamplesDown (block);

    // Tone : mise à jour du cutoff par bloc (valeur lissée), traitement 12 dB/oct
    toneFilter.setCutoffFrequency (toneSmoothed.skip (static_cast<int> (block.getNumSamples())));
    juce::dsp::ProcessContextReplacing<float> context (block);
    toneFilter.process (context);
}

} // namespace kickforge
