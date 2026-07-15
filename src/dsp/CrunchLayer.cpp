#include "CrunchLayer.h"
#include <algorithm>

namespace kickforge
{

namespace
{
constexpr double silenceThreshold = 1.0e-4; // -80 dB
constexpr double minus60dB = 1.0e-3;

double oscSample (CrunchLayer::Waveform wave, double t)
{
    if (wave == CrunchLayer::Waveform::sine)
        return std::sin (juce::MathConstants<double>::twoPi * t);

    // Triangle naïf démarrant à 0 (fréquences basses : pas d'anti-aliasing requis)
    if (t < 0.25)
        return 4.0 * t;
    if (t < 0.75)
        return 2.0 - 4.0 * t;
    return 4.0 * t - 4.0;
}
} // namespace

void CrunchLayer::prepare (const juce::dsp::ProcessSpec& spec)
{
    jassert (spec.numChannels == 1);
    sampleRate   = spec.sampleRate;
    maxBlockSize = static_cast<int> (spec.maximumBlockSize);

    scratch.setSize (1, maxBlockSize);
    drive.prepare (spec);

    active = false;
    env    = 0.0;
    phase  = 0.0;
}

void CrunchLayer::setParameters (const Parameters& newParams)
{
    params = newParams;
    drive.setParameters (params.drive);
}

void CrunchLayer::trigger()
{
    wave  = params.wave;
    level = params.levelPercent * 0.01;

    const double freq = params.pitchEndHz * std::exp2 (params.tuneSemitones / 12.0);
    phaseInc = freq / sampleRate;
    phase    = 0.0;

    attackSamplesLeft = juce::jmax (1, static_cast<int> (std::lround (
                                            params.attackMs * 0.001 * sampleRate)));
    attackInc = 1.0 / attackSamplesLeft;

    const double decaySamples = juce::jmax (1.0, params.decayMs * 0.001 * sampleRate);
    decayMult = std::pow (minus60dB, 1.0 / decaySamples);

    env = 0.0;
    releasing = false;
    drive.reset(); // note fraîche : état du waveshaper/tone remis à zéro
    active = level > 0.0;
}

void CrunchLayer::beginQuickRelease (int numSamples)
{
    if (! active)
        return;
    releasing         = true;
    attackSamplesLeft = 0;
    releaseStep       = env / juce::jmax (1, numSamples);
}

void CrunchLayer::renderAdd (float* dest, int numSamples)
{
    if (! active)
        return;

    auto* work = scratch.getWritePointer (0);

    for (int pos = 0; pos < numSamples && active; pos += maxBlockSize)
    {
        const int n = juce::jmin (maxBlockSize, numSamples - pos);

        for (int i = 0; i < n; ++i)
        {
            work[i] = static_cast<float> (oscSample (wave, phase) * env);

            phase += phaseInc;
            if (phase >= 1.0)
                phase -= 1.0;

            if (releasing)
            {
                env -= releaseStep;
                if (env <= 0.0)
                {
                    env       = 0.0;
                    releasing = false;
                    active    = false;
                    for (int j = i + 1; j < n; ++j)
                        work[j] = 0.0f;
                    break;
                }
            }
            else if (attackSamplesLeft > 0)
            {
                --attackSamplesLeft;
                env = juce::jmin (1.0, env + attackInc);
            }
            else
            {
                env *= decayMult;
                if (env < silenceThreshold)
                {
                    env = 0.0;
                    active = false;
                    // le reste du sous-bloc est déjà rempli : on le laisse
                    // traverser le drive puis on s'arrête
                    for (int j = i + 1; j < n; ++j)
                        work[j] = 0.0f;
                    break;
                }
            }
        }

        float* channel[] = { work };
        juce::dsp::AudioBlock<float> block (channel, 1, static_cast<size_t> (n));
        drive.process (block);

        for (int i = 0; i < n; ++i)
            dest[pos + i] += work[i] * static_cast<float> (level);
    }
}

} // namespace kickforge
