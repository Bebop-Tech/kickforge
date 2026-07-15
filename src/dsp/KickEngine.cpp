#include "KickEngine.h"

#include <cstring>
#include <algorithm>

namespace kickforge
{

void KickEngine::prepare (const juce::dsp::ProcessSpec& monoSpec)
{
    jassert (monoSpec.numChannels == 1);

    punchVoice.prepare (monoSpec.sampleRate);
    attack.prepare (monoSpec.sampleRate);
    crunch.prepare (monoSpec);
    punchDrive.prepare (monoSpec);

    maxBlockSize = static_cast<int> (monoSpec.maximumBlockSize);
    punchScratch.setSize (1, maxBlockSize);

    fadeLengthSamples = juce::jmax (1, static_cast<int> (std::lround (
                                            0.002 * monoSpec.sampleRate)));
    fadeSamplesLeft = 0;
    fadeGain        = 1.0;
    pendingTrigger  = false;
    looping         = false;
    loopCounter     = 0;
    loopIntervalSamples = static_cast<int> (monoSpec.sampleRate); // défaut 1 s
}

void KickEngine::setParameters (const Parameters& newParams)
{
    params = newParams;
    punchVoice.setParameters (params.punch);
    attack.setParameters (params.attack);
    crunch.setParameters (params.crunch);
    punchDrive.setParameters (params.punchDrive);
}

void KickEngine::reset()
{
    punchDrive.reset();
}

void KickEngine::triggerLayers()
{
    punchLevel = params.punchLevelPercent * 0.01;
    punchVoice.trigger();
    attack.trigger();
    crunch.trigger();
    // le drive du punch n'est PAS reset : il tourne en continu, comme la
    // chaîne v1 (les smoothers et l'oversampler gardent leur état)
}

void KickEngine::noteOn()
{
    if (! isActive())
    {
        triggerLayers();
        fadeSamplesLeft = 0;
        fadeGain        = 1.0;
        pendingTrigger  = false;
        return;
    }

    // Couches actives : release rapide des SOURCES (~2 ms, avant leurs
    // drives — c'est ce qui évite le clic), puis retrigger. Un noteOn
    // pendant un fade en cours ne fait que reconfirmer le retrigger engagé.
    if (fadeSamplesLeft == 0)
    {
        fadeSamplesLeft = fadeLengthSamples;
        punchVoice.beginQuickRelease (fadeLengthSamples);
        attack.beginQuickRelease (fadeLengthSamples);
        crunch.beginQuickRelease (fadeLengthSamples);
    }
    pendingTrigger = true;
}

void KickEngine::setLooping (bool shouldLoop)
{
    if (shouldLoop == looping)
        return;

    looping = shouldLoop;
    if (looping)
    {
        noteOn(); // départ immédiat, puis retrigger à chaque intervalle
        loopCounter = 0;
    }
}

void KickEngine::setLoopIntervalSamples (int samples)
{
    loopIntervalSamples = juce::jmax (1, samples);
}

void KickEngine::renderLayers (float* busAP, float* mix, int numSamples)
{
    auto* work = punchScratch.getWritePointer (0);

    for (int pos = 0; pos < numSamples; pos += maxBlockSize)
    {
        const int n = juce::jmin (maxBlockSize, numSamples - pos);

        // Punch : voix -> drive dédié -> level, puis attack ajouté POST-drive
        punchVoice.render (work, n);
        float* channel[] = { work };
        juce::dsp::AudioBlock<float> block (channel, 1, static_cast<size_t> (n));
        punchDrive.process (block);

        for (int i = 0; i < n; ++i)
            busAP[pos + i] = work[i] * static_cast<float> (punchLevel);
        attack.renderAdd (busAP + pos, n);

        std::memcpy (mix + pos, busAP + pos, static_cast<size_t> (n) * sizeof (float));
        crunch.renderAdd (mix + pos, n);
    }
}

void KickEngine::render (float* busAP, float* mix, int numSamples)
{
    int pos = 0;
    while (pos < numSamples)
    {
        // Déclenchement AVANT le calcul du segment (voir le bug v1 : un
        // intervalle raccourci sous le compteur produisait un segment négatif)
        if (looping && loopCounter >= loopIntervalSamples)
        {
            noteOn();
            loopCounter = 0;
        }

        int segment = numSamples - pos;
        if (looping)
            segment = juce::jmin (segment, loopIntervalSamples - loopCounter);
        if (fadeSamplesLeft > 0)
            segment = juce::jmin (segment, fadeSamplesLeft);

        renderLayers (busAP + pos, mix + pos, segment);

        if (fadeSamplesLeft > 0)
        {
            fadeSamplesLeft -= segment;
            if (fadeSamplesLeft == 0 && pendingTrigger)
            {
                triggerLayers();
                pendingTrigger = false;
            }
        }

        if (looping)
            loopCounter += segment;
        pos += segment;
    }
}

} // namespace kickforge
