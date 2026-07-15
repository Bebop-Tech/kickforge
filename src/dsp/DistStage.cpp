#include "DistStage.h"

namespace kickforge
{

namespace
{
constexpr double smoothingSeconds = 0.02;
constexpr double tubeDcBlockHz = 15.0;
constexpr double fuzzHighpassHz = 100.0;
constexpr float tubeBias = 0.1f;

float onePoleCoeff (double freqHz, double sampleRate)
{
    return static_cast<float> (std::exp (-juce::MathConstants<double>::twoPi * freqHz / sampleRate));
}
} // namespace

void DistStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    jassert (spec.numChannels == 1);
    sampleRate = spec.sampleRate;

    wetBuffer.setSize (1, static_cast<int> (spec.maximumBlockSize));

    tubeDcCoeff = onePoleCoeff (tubeDcBlockHz, sampleRate);
    fuzzHpCoeff = onePoleCoeff (fuzzHighpassHz, sampleRate);

    mixSmoothed.reset (spec.sampleRate, smoothingSeconds);
    amountSmoothed.reset (spec.sampleRate, smoothingSeconds);

    reset();
}

void DistStage::setParameters (const Parameters& newParams)
{
    params = newParams;
    mixSmoothed.setTargetValue (params.mixPercent * 0.01f);
    amountSmoothed.setTargetValue (params.amountPercent);
}

void DistStage::reset()
{
    mixSmoothed.setCurrentAndTargetValue (params.mixPercent * 0.01f);
    amountSmoothed.setCurrentAndTargetValue (params.amountPercent);
    tubeDcState = 0.0;
    fuzzHpState = 0.0;
    holdCounter = 0;
    heldSample  = 0.0f;
}

void DistStage::renderWet (const float* dry, float* wet, int numSamples)
{
    const float amount = amountSmoothed.skip (numSamples); // 0-100, lissé par bloc

    switch (params.type)
    {
        case Type::tube:
        {
            // Waveshaper asymétrique doux : le biais crée les harmoniques
            // paires, compensation par 1/tanh(g), DC blocker en sortie.
            const float g    = 1.0f + amount * 0.09f; // 1x -> 10x
            const float bias = std::tanh (g * tubeBias);
            const float comp = 1.0f / std::tanh (juce::jmax (1.0f, g));

            for (int i = 0; i < numSamples; ++i)
            {
                const float shaped = (std::tanh (g * (dry[i] + tubeBias)) - bias) * comp;
                tubeDcState = tubeDcCoeff * tubeDcState + (1.0 - tubeDcCoeff) * shaped;
                wet[i] = shaped - static_cast<float> (tubeDcState);
            }
            break;
        }

        case Type::fuzz:
        {
            // Gain élevé + hard clip + HPF léger, légère compensation
            const float g = 1.0f + amount * 0.49f; // 1x -> 50x

            for (int i = 0; i < numSamples; ++i)
            {
                const float clipped = juce::jlimit (-1.0f, 1.0f, g * dry[i]);
                fuzzHpState = fuzzHpCoeff * fuzzHpState + (1.0 - fuzzHpCoeff) * clipped;
                // Le HPF rebondit sur les fronts (> 1) : re-clip final, fidèle au fuzz
                wet[i] = juce::jlimit (-1.0f, 1.0f,
                                       (clipped - static_cast<float> (fuzzHpState)) * 0.85f);
            }
            break;
        }

        case Type::bitcrush:
        {
            // Réduction de bit depth (16 -> 3 bits) et de sample rate (jusqu'à /16)
            const float bits     = 16.0f - amount * 0.13f;
            const float levels   = std::exp2 (bits - 1.0f);
            const int holdLength = 1 + static_cast<int> (std::lround (amount * 0.15f));

            for (int i = 0; i < numSamples; ++i)
            {
                if (holdCounter <= 0)
                {
                    heldSample  = std::round (dry[i] * levels) / levels;
                    holdCounter = holdLength;
                }
                --holdCounter;
                wet[i] = heldSample;
            }
            break;
        }
    }
}

void DistStage::process (juce::dsp::AudioBlock<float> block)
{
    jassert (block.getNumChannels() == 1);
    const auto numSamples = static_cast<int> (block.getNumSamples());

    auto* dry = block.getChannelPointer (0);
    auto* wet = wetBuffer.getWritePointer (0);

    renderWet (dry, wet, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        dry[i] = (1.0f - mix) * dry[i] + mix * wet[i];
    }
}

} // namespace kickforge
