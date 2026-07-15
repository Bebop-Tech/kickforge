#include "FxStage.h"

namespace kickforge
{

namespace
{
// Caractère fixe du chorus en v1/v2 (seul le mix est exposé)
constexpr float chorusRateHz      = 1.2f;
constexpr float chorusDepth       = 0.3f;
constexpr float chorusCentreDelay = 7.0f;
constexpr float chorusFeedback    = 0.0f;

// Reverb en send : WET ONLY. juce::Reverb applique wetScaleFactor = 3 en
// interne (héritage freeverb) : on compense pour un niveau de send unitaire.
juce::Reverb::Parameters reverbParamsFor (float mix01)
{
    juce::Reverb::Parameters p;
    p.roomSize   = 0.55f;
    p.damping    = 0.5f;
    p.width      = 1.0f;
    p.wetLevel   = mix01 / 3.0f;
    p.dryLevel   = 0.0f;
    p.freezeMode = 0.0f;
    return p;
}
} // namespace

void FxStage::prepare (const juce::dsp::ProcessSpec& spec)
{
    jassert (spec.numChannels == 2);

    chorus.prepare (spec);
    chorus.setRate (chorusRateHz);
    chorus.setDepth (chorusDepth);
    chorus.setCentreDelay (chorusCentreDelay);
    chorus.setFeedback (chorusFeedback);

    reverb.prepare (spec);
    reverbScratch.setSize (2, static_cast<int> (spec.maximumBlockSize));

    reset();
}

void FxStage::setParameters (const Parameters& newParams)
{
    params = newParams;
}

void FxStage::reset()
{
    // Cibles d'abord : les reset() JUCE snappent leurs smoothers internes
    chorus.setMix (params.chorusMixPercent * 0.01f);
    reverb.setParameters (reverbParamsFor (params.reverbMixPercent * 0.01f));
    chorus.reset();
    reverb.reset();
}

void FxStage::process (juce::dsp::AudioBlock<float> mainStereo, const float* busAP,
                       size_t numSamples)
{
    jassert (mainStereo.getNumChannels() == 2);
    jassert (mainStereo.getNumSamples() == numSamples);

    // Chorus global sur le bus principal
    chorus.setMix (params.chorusMixPercent * 0.01f);
    juce::dsp::ProcessContextReplacing<float> mainContext (mainStereo);
    chorus.process (mainContext);

    // Send de reverb : busAP dupliqué en stéréo, wet only, sommé au principal.
    // On traite même à mix 0 pour que la queue vive/meure naturellement.
    reverb.setParameters (reverbParamsFor (params.reverbMixPercent * 0.01f));

    reverbScratch.copyFrom (0, 0, busAP, static_cast<int> (numSamples));
    reverbScratch.copyFrom (1, 0, busAP, static_cast<int> (numSamples));

    juce::dsp::AudioBlock<float> wetBlock (reverbScratch);
    auto wet = wetBlock.getSubBlock (0, numSamples);
    juce::dsp::ProcessContextReplacing<float> wetContext (wet);
    reverb.process (wetContext);

    mainStereo.add (wet);
}

} // namespace kickforge
