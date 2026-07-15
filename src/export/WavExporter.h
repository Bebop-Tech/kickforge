#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "../dsp/KickEngine.h"
#include "../dsp/FilterStage.h"
#include "../dsp/EqStage.h"
#include "../dsp/DistStage.h"
#include "../dsp/FxStage.h"
#include "../dsp/CompStage.h"
#include "../dsp/ClipStage.h"
#include "../dsp/WidthStage.h"

namespace kickforge
{

// Rendu offline du kick complet -> WAV 48 kHz / 24 bits stéréo.
// Tout se passe sur le message thread avec des instances DSP dédiées
// (jamais celles du thread audio). Durée = enveloppe la plus longue
// (punch ou crunch) + queue de reverb, max 3 s.
class WavExporter
{
public:
    struct ChainParameters
    {
        KickEngine::Parameters engine; // couches attack/punch/crunch + drive du punch
        FilterStage::Parameters filter;
        EqStage::Parameters eq;
        DistStage::Parameters dist;
        FxStage::Parameters fx;
        CompStage::Parameters comp;
        ClipStage::Parameters clip;
        WidthStage::Parameters width;
        float outputGainDb = -1.0f;
    };

    static double lengthSecondsFor (const ChainParameters& params);
    static juce::AudioBuffer<float> renderKick (const ChainParameters& params,
                                                double lengthSeconds);
    static bool writeWavFile (const juce::AudioBuffer<float>& buffer, const juce::File& file);
};

} // namespace kickforge
