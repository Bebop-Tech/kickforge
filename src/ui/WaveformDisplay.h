#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../dsp/KickEngine.h"
#include "../dsp/FilterStage.h"
#include "../dsp/EqStage.h"
#include "../dsp/DistStage.h"
#include "../dsp/CompStage.h"
#include "../dsp/ClipStage.h"
#include "LookAndFeel.h"

#include <atomic>
#include <vector>

namespace kickforge::ui
{

// Rendu de la forme d'onde du kick + courbe de pitch en pointillés, et
// silhouettes séparées des 3 couches (attack/punch/crunch) superposées en
// couleur — également consommées par les mini-vues des panneaux de couche.
//
// Contrainte section 9 : le rendu se fait sur le message thread à partir
// d'une COPIE des paramètres (les atomics APVTS), avec des instances DSP
// dédiées — jamais en lisant l'état du thread audio. Throttlé à ~30 fps via
// un timer + drapeau dirty (les listeners de paramètres peuvent être
// notifiés depuis n'importe quel thread : ils ne font que lever le drapeau).
class WaveformDisplay : public juce::Component,
                        private juce::Timer,
                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit WaveformDisplay (juce::AudioProcessorValueTreeState& apvts);
    ~WaveformDisplay() override;

    void paint (juce::Graphics& g) override;

    // Silhouette min/max par colonne d'affichage
    struct Silhouette
    {
        std::vector<float> low, high;
        float peak = 0.0f;
    };

    const Silhouette& attackShape() const { return attackSilhouette; }
    const Silhouette& punchShape() const { return punchSilhouette; }
    const Silhouette& crunchShape() const { return crunchSilhouette; }

    // Appelé sur le message thread après chaque nouveau rendu
    std::function<void()> onShapesUpdated;

private:
    void timerCallback() override;
    void parameterChanged (const juce::String&, float) override { dirty.store (true); }
    void renderKick();

    juce::AudioProcessorValueTreeState& apvts;
    juce::StringArray listenedParamIDs;
    std::atomic<bool> dirty { true };

    // Instances DSP dédiées à l'affichage (jamais celles du thread audio).
    // Le moteur est recréé à chaque rendu (message thread : les allocations
    // sont permises) pour repartir d'un état vierge.
    std::unique_ptr<KickEngine> engine;
    FilterStage filter;
    EqStage eq;
    DistStage dist;
    CompStage comp;
    ClipStage clip;

    std::vector<float> renderBuffer;
    std::vector<float> busScratch;
    std::vector<float> punchOnlyScratch;
    std::vector<float> layerScratch;

    // Silhouettes (composite post-chaîne + couches pré-chaîne) et courbe de pitch
    Silhouette compositeSilhouette, attackSilhouette, punchSilhouette, crunchSilhouette;
    float pitchStartHz = 210.0f, pitchEndHz = 48.0f, sweepTimeMs = 42.0f;
    double displaySeconds = 0.5;
};

} // namespace kickforge::ui
