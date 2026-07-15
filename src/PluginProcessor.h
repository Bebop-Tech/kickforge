#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/KickEngine.h"
#include "dsp/DriveStage.h"
#include "dsp/FilterStage.h"
#include "dsp/EqStage.h"
#include "dsp/DistStage.h"
#include "dsp/FxStage.h"
#include "dsp/CompStage.h"
#include "dsp/ClipStage.h"
#include "dsp/WidthStage.h"
#include "export/WavExporter.h"

class KickForgeAudioProcessor : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::AsyncUpdater
{
public:
    KickForgeAudioProcessor();
    ~KickForgeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; } // queue de reverb

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }

    // Pour les vu-mètres de l'UI
    float getGainReductionDb() const { return compStage.getGainReductionDb(); }
    float getOutputPeak() const { return outputPeak.load (std::memory_order_relaxed); }

    // Bouton Random (étape 8) : ±25 % autour du preset du genre actif.
    // À appeler depuis le message thread uniquement.
    void randomizeAroundActivePreset();

    // Bouton Play : note-on interne consommé par processBlock (lock-free)
    void triggerPlay() { playRequested.store (true, std::memory_order_relaxed); }

    // Loop de pré-écoute (UI) : retrigger périodique, intervalle en secondes
    void setLoopEnabled (bool enabled) { loopEnabled.store (enabled, std::memory_order_relaxed); }
    void setLoopIntervalSeconds (float seconds) { loopIntervalSeconds.store (seconds, std::memory_order_relaxed); }

    // Copie des paramètres courants pour l'export offline (message thread)
    kickforge::WavExporter::ChainParameters currentChainParameters() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Chargement des presets de genre : le listener peut être notifié depuis
    // le thread audio (automation), l'application est différée au message
    // thread via AsyncUpdater.
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void applyGenrePreset (int genreIndex);

    kickforge::DriveStage::Parameters currentDriveParams() const;
    kickforge::FilterStage::Parameters currentFilterParams() const;
    kickforge::EqStage::Parameters currentEqParams() const;
    kickforge::DistStage::Parameters currentDistParams() const;
    kickforge::FxStage::Parameters currentFxParams() const;
    kickforge::CompStage::Parameters currentCompParams() const;
    kickforge::ClipStage::Parameters currentClipParams() const;
    kickforge::WidthStage::Parameters currentWidthParams() const;
    kickforge::KickVoice::Parameters currentVoiceParams() const;
    kickforge::KickEngine::Parameters currentEngineParams() const;

    juce::AudioProcessorValueTreeState apvts;

    kickforge::KickEngine kickEngine;
    juce::AudioBuffer<float> busScratch; // busAP (attack+punch) pour le send reverb
    kickforge::FilterStage filterStage;
    kickforge::EqStage eqStage;
    kickforge::DistStage distStage;
    kickforge::FxStage fxStage;
    kickforge::CompStage compStage;
    kickforge::ClipStage clipStage;
    kickforge::WidthStage widthStage;
    juce::LinearSmoothedValue<float> outputGainLinear;

    // Pointeurs vers les atomics APVTS, résolus une fois au constructeur.
    std::atomic<float>* waveformParam   = nullptr;
    std::atomic<float>* pitchStartParam = nullptr;
    std::atomic<float>* pitchEndParam   = nullptr;
    std::atomic<float>* sweepTimeParam  = nullptr;
    std::atomic<float>* oscBOnParam     = nullptr;
    std::atomic<float>* oscBWaveParam   = nullptr;
    std::atomic<float>* oscBTuneParam   = nullptr;
    std::atomic<float>* oscBLevelParam  = nullptr;
    std::atomic<float>* attackParam     = nullptr;
    std::atomic<float>* decayParam      = nullptr;
    std::atomic<float>* atkLevelParam   = nullptr;
    std::atomic<float>* atkDecayParam   = nullptr;
    std::atomic<float>* atkToneParam    = nullptr;
    std::atomic<float>* punchLevelParam = nullptr;
    std::atomic<float>* crunchLevelParam       = nullptr;
    std::atomic<float>* crunchWaveParam        = nullptr;
    std::atomic<float>* crunchTuneParam        = nullptr;
    std::atomic<float>* crunchAttackParam      = nullptr;
    std::atomic<float>* crunchDecayParam       = nullptr;
    std::atomic<float>* crunchDriveTypeParam   = nullptr;
    std::atomic<float>* crunchDriveAmountParam = nullptr;
    std::atomic<float>* crunchDriveToneParam   = nullptr;
    std::atomic<float>* driveTypeParam    = nullptr;
    std::atomic<float>* driveAmountParam  = nullptr;
    std::atomic<float>* driveToneParam    = nullptr;
    std::atomic<float>* filterTypeParam   = nullptr;
    std::atomic<float>* filterCutoffParam = nullptr;
    std::atomic<float>* filterResoParam   = nullptr;
    std::atomic<float>* eqFreqParams[3] { nullptr, nullptr, nullptr };
    std::atomic<float>* eqGainParams[3] { nullptr, nullptr, nullptr };
    std::atomic<float>* eqQParams[3] { nullptr, nullptr, nullptr };
    std::atomic<float>* distTypeParam   = nullptr;
    std::atomic<float>* distAmountParam = nullptr;
    std::atomic<float>* distMixParam    = nullptr;
    std::atomic<float>* reverbMixParam  = nullptr;
    std::atomic<float>* chorusMixParam  = nullptr;
    std::atomic<float>* compAmountParam  = nullptr;
    std::atomic<float>* compAttackParam  = nullptr;
    std::atomic<float>* clipTypeParam    = nullptr;
    std::atomic<float>* clipCeilingParam = nullptr;
    std::atomic<float>* widthParam       = nullptr;
    std::atomic<float>* outputGainParam = nullptr;

    std::atomic<float> outputPeak { 0.0f };
    std::atomic<bool> playRequested { false };
    std::atomic<bool> loopEnabled { false };
    std::atomic<float> loopIntervalSeconds { 1.0f };
    std::atomic<int> pendingGenre { -1 };
    bool suppressPresetLoad = false; // garde pendant setStateInformation

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickForgeAudioProcessor)
};
