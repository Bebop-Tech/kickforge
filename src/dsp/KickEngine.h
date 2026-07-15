#pragma once

#include "KickVoice.h"
#include "AttackLayer.h"
#include "CrunchLayer.h"
#include "DriveStage.h"

namespace kickforge
{

// Orchestrateur v2 : déclenche et mixe les 3 couches du kick.
//
//   busAP = attack + punch (le punch traverse SA DriveStage puis punchLevel ;
//           l'attack est ajouté APRÈS le drive : le transitoire n'est plus
//           écrasé par la saturation du corps)
//   mix   = busAP + crunch (le crunch embarque sa propre distorsion)
//
// busAP alimente le send de reverb (le crunch reste sec). Retrigger
// monophonique avec fade ~2 ms appliqué aux deux bus, mode loop de
// pré-écoute sample-accurate. Tout est pré-alloué dans prepare().
class KickEngine
{
public:
    struct Parameters
    {
        KickVoice::Parameters punch; // punchPercent est ignoré (couche Attack dédiée)
        DriveStage::Parameters punchDrive;
        float punchLevelPercent = 100.0f;
        AttackLayer::Parameters attack;
        CrunchLayer::Parameters crunch;
    };

    void prepare (const juce::dsp::ProcessSpec& monoSpec);
    void setParameters (const Parameters& newParams);
    void reset(); // fige les smoothers du drive sur les paramètres courants
    void noteOn();

    void setLooping (bool shouldLoop);
    void setLoopIntervalSamples (int samples);

    bool isActive() const
    {
        return punchVoice.isActive() || attack.isActive() || crunch.isActive()
               || pendingTrigger;
    }

    float getLatencyInSamples() const { return punchDrive.getLatencyInSamples(); }

    // Remplit busAP et mix (écrase leur contenu).
    void render (float* busAP, float* mix, int numSamples);

private:
    void triggerLayers();
    void renderLayers (float* busAP, float* mix, int numSamples);

    Parameters params;

    KickVoice punchVoice;
    AttackLayer attack;
    CrunchLayer crunch;
    DriveStage punchDrive;

    juce::AudioBuffer<float> punchScratch;
    int maxBlockSize  = 512;
    double punchLevel = 1.0;

    int fadeLengthSamples = 96; // ~2 ms, recalculé dans prepare()
    int fadeSamplesLeft   = 0;
    double fadeGain       = 1.0;
    double fadeStep       = 0.0;
    bool pendingTrigger   = false;

    bool looping            = false;
    int loopIntervalSamples = 48000;
    int loopCounter         = 0;
};

} // namespace kickforge
