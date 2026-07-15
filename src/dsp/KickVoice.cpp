#include "KickVoice.h"

#include <algorithm>
#include <cmath>

namespace kickforge
{

namespace
{
constexpr double twoPi = 6.283185307179586476925286766559;
constexpr double silenceThreshold = 1.0e-4; // -80 dB : seuil d'extinction de la voix
constexpr double minus60dB = 1.0e-3;

// Correction PolyBLEP classique : lisse la discontinuité sur 2 échantillons
// autour du wrap de phase. t et dt en phase normalisée [0, 1).
double polyBlep (double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

double shapeSample (KickVoice::Waveform wave, double t, double dt)
{
    switch (wave)
    {
        case KickVoice::Waveform::sine:
            return std::sin (twoPi * t);

        case KickVoice::Waveform::triangle:
            // Naïf (toléré sans anti-aliasing par le brief), démarre à 0 comme le sinus
            if (t < 0.25)
                return 4.0 * t;
            if (t < 0.75)
                return 2.0 - 4.0 * t;
            return 4.0 * t - 4.0;

        case KickVoice::Waveform::square:
        {
            double value = t < 0.5 ? 1.0 : -1.0;
            value += polyBlep (t, dt);
            double tShifted = t + 0.5;
            if (tShifted >= 1.0)
                tShifted -= 1.0;
            value -= polyBlep (tShifted, dt);
            return value;
        }

        case KickVoice::Waveform::saw:
            return 2.0 * t - 1.0 - polyBlep (t, dt);
    }
    return 0.0;
}

double advancePhase (double phase, double dt)
{
    phase += dt;
    if (phase >= 1.0)
        phase -= 1.0;
    return phase;
}
} // namespace

void KickVoice::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    active     = false;
    ampEnv     = 0.0;
    phaseA     = 0.0;
    phaseB     = 0.0;
}

void KickVoice::setParameters (const Parameters& newParams)
{
    params = newParams;
}

void KickVoice::trigger()
{
    waveA      = params.waveform;
    waveB      = params.oscBWave;
    pitchStart = params.pitchStartHz;
    pitchEnd   = params.pitchEndHz;

    oscBActive = params.oscBOn;
    oscBRatio  = std::exp2 (params.oscBTuneSemitones / 12.0);
    oscBLevel  = params.oscBLevelPercent * 0.01;
    mixScale   = oscBActive ? 1.0 / (1.0 + oscBLevel) : 1.0;

    const double tauSamples = std::max (1.0, params.sweepTimeMs * 0.001 * sampleRate);
    pitchState = 1.0;
    pitchMult  = std::exp (-1.0 / tauSamples);

    attackSamplesLeft = std::max (1, static_cast<int> (std::lround (params.attackMs * 0.001 * sampleRate)));
    attackInc         = 1.0 / attackSamplesLeft;

    const double decaySamples = std::max (1.0, params.decayMs * 0.001 * sampleRate);
    decayMult = std::pow (minus60dB, 1.0 / decaySamples);

    phaseA = 0.0;
    phaseB = 0.0;
    ampEnv = 0.0;
    releasing = false;
    active = true;
}

void KickVoice::beginQuickRelease (int numSamples)
{
    if (! active)
        return;

    releasing         = true;
    attackSamplesLeft = 0;
    releaseStep       = ampEnv / std::max (1, numSamples);
}

float KickVoice::currentFrequencyHz() const
{
    if (! active)
        return 0.0f;

    return static_cast<float> (pitchEnd + (pitchStart - pitchEnd) * pitchState);
}

// Avance pitch env et amp env ; renvoie l'échantillon final pour un mix d'oscillateurs donné.
float KickVoice::advanceEnvelopes (double oscMix)
{
    const double out = oscMix * ampEnv;

    if (releasing)
    {
        ampEnv -= releaseStep;
        if (ampEnv <= 0.0)
        {
            ampEnv    = 0.0;
            releasing = false;
            active    = false;
        }
    }
    else if (attackSamplesLeft > 0)
    {
        --attackSamplesLeft;
        ampEnv = std::min (1.0, ampEnv + attackInc);
    }
    else
    {
        ampEnv *= decayMult;
        if (ampEnv < silenceThreshold)
        {
            ampEnv = 0.0;
            active = false;
        }
    }

    return static_cast<float> (out);
}

template <bool withOscB>
void KickVoice::renderLoop (float* dest, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        if (! active)
        {
            dest[i] = 0.0f;
            continue;
        }

        const double freq = pitchEnd + (pitchStart - pitchEnd) * pitchState;
        pitchState *= pitchMult;

        const double dtA = freq / sampleRate;
        double oscMix = shapeSample (waveA, phaseA, dtA);
        phaseA = advancePhase (phaseA, dtA);

        if constexpr (withOscB)
        {
            // dt plafonné sous Nyquist pour rester stable aux tunes extrêmes
            const double dtB = std::min (0.45, dtA * oscBRatio);
            oscMix += oscBLevel * shapeSample (waveB, phaseB, dtB);
            phaseB = advancePhase (phaseB, dtB);
            oscMix *= mixScale;
        }

        dest[i] = advanceEnvelopes (oscMix);
    }
}

float KickVoice::renderNextSample()
{
    float sample = 0.0f;
    render (&sample, 1);
    return sample;
}

void KickVoice::render (float* dest, int numSamples)
{
    // Branchement Osc B au niveau de la voix, pas par échantillon (contrainte du brief)
    if (oscBActive)
        renderLoop<true> (dest, numSamples);
    else
        renderLoop<false> (dest, numSamples);
}

} // namespace kickforge
