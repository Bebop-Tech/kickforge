#include "AttackLayer.h"

#include <cmath>
#include <algorithm>

namespace kickforge
{

namespace
{
constexpr double twoPi = 6.283185307179586476925286766559;
constexpr double minus60dB = 1.0e-3;
constexpr double peakScale = 0.6;      // même niveau que le punch v1
constexpr double ln1000 = 6.9077552789821370520539743640531; // decay -> tau
} // namespace

void AttackLayer::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    active     = false;
    env        = 0.0;
    lpState    = 0.0;
}

void AttackLayer::setParameters (const Parameters& newParams)
{
    params = newParams;
}

void AttackLayer::trigger()
{
    level = params.levelPercent * 0.01 * peakScale;

    const double tauSeconds = params.decayMs / 1000.0 / ln1000;
    envMult = std::exp (-1.0 / (tauSeconds * sampleRate));

    lpCoeff    = std::exp (-twoPi * params.toneHz / sampleRate);
    lpState    = 0.0;
    noiseState = 0x2545f491u; // graine fixe : transitoire reproductible
    env        = level > 0.0 ? 1.0 : 0.0;
    releasing  = false;
    active     = env > 0.0;
}

void AttackLayer::beginQuickRelease (int numSamples)
{
    if (! active)
        return;
    releasing   = true;
    releaseStep = env / std::max (1, numSamples);
}

void AttackLayer::renderAdd (float* dest, int numSamples)
{
    if (! active)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        if (env <= minus60dB)
        {
            active = false;
            return;
        }

        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;
        const double noise = static_cast<double> (noiseState) * (2.0 / 4294967295.0) - 1.0;
        lpState = lpCoeff * lpState + (1.0 - lpCoeff) * noise;

        dest[i] += static_cast<float> (lpState * level * env);

        if (releasing)
        {
            env -= releaseStep;
            if (env <= 0.0)
            {
                env       = 0.0;
                releasing = false;
                active    = false;
                return;
            }
        }
        else
        {
            env *= envMult;
        }
    }
}

} // namespace kickforge
