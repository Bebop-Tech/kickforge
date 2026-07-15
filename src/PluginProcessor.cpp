#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "presets/GenrePresets.h"
#include "presets/StateMigration.h"

namespace
{
// Plage logarithmique : le centre perceptif est la moyenne géométrique des bornes.
juce::NormalisableRange<float> logRange (float min, float max)
{
    juce::NormalisableRange<float> range (min, max);
    range.setSkewForCentre (std::sqrt (min * max));
    return range;
}

juce::NormalisableRange<float> linRange (float min, float max)
{
    return { min, max };
}

const juce::StringArray waveChoices { "Sin", "Tri", "Sqr", "Saw" };
} // namespace

KickForgeAudioProcessor::KickForgeAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "kickforge_params_v2", createParameterLayout())
{
    waveformParam   = apvts.getRawParameterValue ("waveform");
    pitchStartParam = apvts.getRawParameterValue ("pitchStart");
    pitchEndParam   = apvts.getRawParameterValue ("pitchEnd");
    sweepTimeParam  = apvts.getRawParameterValue ("sweepTime");
    oscBOnParam     = apvts.getRawParameterValue ("oscBOn");
    oscBWaveParam   = apvts.getRawParameterValue ("oscBWave");
    oscBTuneParam   = apvts.getRawParameterValue ("oscBTune");
    oscBLevelParam  = apvts.getRawParameterValue ("oscBLevel");
    attackParam     = apvts.getRawParameterValue ("attack");
    decayParam      = apvts.getRawParameterValue ("decay");
    atkLevelParam   = apvts.getRawParameterValue ("atkLevel");
    atkDecayParam   = apvts.getRawParameterValue ("atkDecay");
    atkToneParam    = apvts.getRawParameterValue ("atkTone");
    punchLevelParam = apvts.getRawParameterValue ("punchLevel");
    crunchLevelParam       = apvts.getRawParameterValue ("crunchLevel");
    crunchWaveParam        = apvts.getRawParameterValue ("crunchWave");
    crunchTuneParam        = apvts.getRawParameterValue ("crunchTune");
    crunchAttackParam      = apvts.getRawParameterValue ("crunchAttack");
    crunchDecayParam       = apvts.getRawParameterValue ("crunchDecay");
    crunchDriveTypeParam   = apvts.getRawParameterValue ("crunchDriveType");
    crunchDriveAmountParam = apvts.getRawParameterValue ("crunchDriveAmount");
    crunchDriveToneParam   = apvts.getRawParameterValue ("crunchDriveTone");
    driveTypeParam    = apvts.getRawParameterValue ("driveType");
    driveAmountParam  = apvts.getRawParameterValue ("driveAmount");
    driveToneParam    = apvts.getRawParameterValue ("driveTone");
    filterTypeParam   = apvts.getRawParameterValue ("filterType");
    filterCutoffParam = apvts.getRawParameterValue ("filterCutoff");
    filterResoParam   = apvts.getRawParameterValue ("filterReso");
    for (int band = 0; band < 3; ++band)
    {
        const auto n = juce::String (band + 1);
        eqFreqParams[band] = apvts.getRawParameterValue ("eq" + n + "Freq");
        eqGainParams[band] = apvts.getRawParameterValue ("eq" + n + "Gain");
        eqQParams[band]    = apvts.getRawParameterValue ("eq" + n + "Q");
    }
    distTypeParam   = apvts.getRawParameterValue ("distType");
    distAmountParam = apvts.getRawParameterValue ("distAmount");
    distMixParam    = apvts.getRawParameterValue ("distMix");
    reverbMixParam  = apvts.getRawParameterValue ("reverbMix");
    chorusMixParam  = apvts.getRawParameterValue ("chorusMix");
    compAmountParam  = apvts.getRawParameterValue ("compAmount");
    compAttackParam  = apvts.getRawParameterValue ("compAttack");
    clipTypeParam    = apvts.getRawParameterValue ("clipType");
    clipCeilingParam = apvts.getRawParameterValue ("clipCeiling");
    widthParam       = apvts.getRawParameterValue ("width");
    outputGainParam = apvts.getRawParameterValue ("outputGain");

    apvts.addParameterListener ("genre", this);
}

KickForgeAudioProcessor::~KickForgeAudioProcessor()
{
    apvts.removeParameterListener ("genre", this);
    cancelPendingUpdate();
}

void KickForgeAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID != "genre" || suppressPresetLoad)
        return;

    pendingGenre.store (juce::jlimit (0, kickforge::presets::numGenres - 1,
                                      static_cast<int> (std::lround (newValue))));
    triggerAsyncUpdate();
}

void KickForgeAudioProcessor::handleAsyncUpdate()
{
    const int genre = pendingGenre.exchange (-1);
    if (genre >= 0)
        applyGenrePreset (genre);
}

void KickForgeAudioProcessor::applyGenrePreset (int genreIndex)
{
    for (const auto& entry : kickforge::presets::genrePresets[genreIndex].values)
    {
        if (auto* parameter = apvts.getParameter (entry.id))
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (entry.value));
        else
            jassertfalse; // id de preset inconnu : GenrePresets.h et l'APVTS divergent
    }
}

void KickForgeAudioProcessor::randomizeAroundActivePreset()
{
    JUCE_ASSERT_MESSAGE_THREAD

    const int genre = juce::jlimit (0, kickforge::presets::numGenres - 1,
                                    static_cast<int> (std::lround (
                                        apvts.getRawParameterValue ("genre")->load())));
    auto& rng = juce::Random::getSystemRandom();

    for (const auto& entry : kickforge::presets::genrePresets[genre].values)
    {
        if (auto* parameter = apvts.getParameter (entry.id))
        {
            const float presetNorm = parameter->convertTo0to1 (entry.value);
            parameter->setValueNotifyingHost (
                kickforge::presets::randomizedNormalised (presetNorm, rng.nextFloat()));
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout KickForgeAudioProcessor::createParameterLayout()
{
    using FloatParam  = juce::AudioParameterFloat;
    using ChoiceParam = juce::AudioParameterChoice;
    using BoolParam   = juce::AudioParameterBool;
    using IntParam    = juce::AudioParameterInt;
    using PID         = juce::ParameterID;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Genre / presets
    layout.add (std::make_unique<ChoiceParam> (
        PID { "genre", 1 }, "Genre",
        juce::StringArray { "Techno", "Hard Techno", "Hardstyle", "EDM", "Trance" }, 1));

    // Osc A
    layout.add (std::make_unique<ChoiceParam> (PID { "waveform", 1 }, "Osc A Forme", waveChoices, 0));
    layout.add (std::make_unique<FloatParam> (PID { "pitchStart", 1 }, "Pitch Start",
                                              logRange (50.0f, 2000.0f), 210.0f));
    layout.add (std::make_unique<FloatParam> (PID { "pitchEnd", 1 }, "Pitch End",
                                              logRange (25.0f, 200.0f), 48.0f));
    layout.add (std::make_unique<FloatParam> (PID { "sweepTime", 1 }, "Sweep",
                                              logRange (5.0f, 300.0f), 42.0f));

    // Osc B
    layout.add (std::make_unique<BoolParam> (PID { "oscBOn", 1 }, "Osc B On", false));
    layout.add (std::make_unique<ChoiceParam> (PID { "oscBWave", 1 }, "Osc B Forme", waveChoices, 2));
    layout.add (std::make_unique<IntParam> (PID { "oscBTune", 1 }, "Osc B Tune", -24, 24, 12));
    layout.add (std::make_unique<FloatParam> (PID { "oscBLevel", 1 }, "Osc B Level",
                                              linRange (0.0f, 100.0f), 25.0f));

    // Enveloppe d'amplitude
    layout.add (std::make_unique<FloatParam> (PID { "attack", 1 }, "Attack",
                                              linRange (0.1f, 20.0f), 1.0f));
    layout.add (std::make_unique<FloatParam> (PID { "decay", 1 }, "Decay",
                                              logRange (50.0f, 2000.0f), 340.0f));

    // Couche Attack (v2)
    layout.add (std::make_unique<FloatParam> (PID { "atkLevel", 2 }, "Attack Level",
                                              linRange (0.0f, 100.0f), 70.0f));
    layout.add (std::make_unique<FloatParam> (PID { "atkDecay", 2 }, "Attack Decay",
                                              logRange (1.0f, 30.0f), 3.0f));
    layout.add (std::make_unique<FloatParam> (PID { "atkTone", 2 }, "Attack Tone",
                                              logRange (500.0f, 16000.0f), 5000.0f));

    // Couche Punch (v2)
    layout.add (std::make_unique<FloatParam> (PID { "punchLevel", 2 }, "Punch Level",
                                              linRange (0.0f, 100.0f), 100.0f));

    // Couche Crunch (v2)
    layout.add (std::make_unique<FloatParam> (PID { "crunchLevel", 2 }, "Crunch Level",
                                              linRange (0.0f, 100.0f), 0.0f));
    layout.add (std::make_unique<ChoiceParam> (PID { "crunchWave", 2 }, "Crunch Forme",
                                               juce::StringArray { "Sin", "Tri" }, 0));
    layout.add (std::make_unique<IntParam> (PID { "crunchTune", 2 }, "Crunch Tune", -12, 12, 0));
    layout.add (std::make_unique<FloatParam> (PID { "crunchAttack", 2 }, "Crunch Attack",
                                              logRange (5.0f, 200.0f), 30.0f));
    layout.add (std::make_unique<FloatParam> (PID { "crunchDecay", 2 }, "Crunch Decay",
                                              logRange (100.0f, 2000.0f), 500.0f));
    layout.add (std::make_unique<ChoiceParam> (PID { "crunchDriveType", 2 }, "Crunch Drive",
                                               juce::StringArray { "Soft", "Hard", "Fold" }, 1));
    layout.add (std::make_unique<FloatParam> (PID { "crunchDriveAmount", 2 }, "Crunch Amount",
                                              linRange (0.0f, 100.0f), 60.0f));
    layout.add (std::make_unique<FloatParam> (PID { "crunchDriveTone", 2 }, "Crunch Tone",
                                              logRange (500.0f, 16000.0f), 3000.0f));

    // Drive (punch)
    layout.add (std::make_unique<ChoiceParam> (PID { "driveType", 1 }, "Drive Type",
                                               juce::StringArray { "Soft", "Hard", "Fold" }, 1));
    layout.add (std::make_unique<FloatParam> (PID { "driveAmount", 1 }, "Drive Amount",
                                              linRange (0.0f, 100.0f), 65.0f));
    layout.add (std::make_unique<FloatParam> (PID { "driveTone", 1 }, "Drive Tone",
                                              logRange (500.0f, 16000.0f), 2400.0f));

    // Filtre multimode
    layout.add (std::make_unique<ChoiceParam> (PID { "filterType", 1 }, "Filtre Type",
                                               juce::StringArray { "LP", "HP", "BP" }, 0));
    layout.add (std::make_unique<FloatParam> (PID { "filterCutoff", 1 }, "Filtre Cutoff",
                                              logRange (20.0f, 20000.0f), 20000.0f));
    layout.add (std::make_unique<FloatParam> (PID { "filterReso", 1 }, "Filtre Reso",
                                              linRange (0.0f, 90.0f), 20.0f));

    // EQ 3 bandes paramétrique
    const float eqFreqDefaults[3] { 60.0f, 800.0f, 6000.0f };
    const float eqGainDefaults[3] { 3.0f, -2.0f, 1.5f };

    for (int band = 0; band < 3; ++band)
    {
        const auto n = juce::String (band + 1);
        layout.add (std::make_unique<FloatParam> (PID { "eq" + n + "Freq", 1 },
                                                  "Bande " + n + " Freq",
                                                  logRange (20.0f, 18000.0f), eqFreqDefaults[band]));
        layout.add (std::make_unique<FloatParam> (PID { "eq" + n + "Gain", 1 },
                                                  "Bande " + n + " Gain",
                                                  linRange (-12.0f, 12.0f), eqGainDefaults[band]));
        layout.add (std::make_unique<FloatParam> (PID { "eq" + n + "Q", 1 },
                                                  "Bande " + n + " Q",
                                                  logRange (0.3f, 6.0f), 0.8f));
    }

    // Distorsion parallèle
    layout.add (std::make_unique<ChoiceParam> (PID { "distType", 1 }, "Dist Type",
                                               juce::StringArray { "Tube", "Fuzz", "Bit" }, 0));
    layout.add (std::make_unique<FloatParam> (PID { "distAmount", 1 }, "Dist Amount",
                                              linRange (0.0f, 100.0f), 20.0f));
    layout.add (std::make_unique<FloatParam> (PID { "distMix", 1 }, "Dist Mix",
                                              linRange (0.0f, 100.0f), 40.0f));

    // FX
    layout.add (std::make_unique<FloatParam> (PID { "reverbMix", 1 }, "Reverb",
                                              linRange (0.0f, 100.0f), 10.0f));
    layout.add (std::make_unique<FloatParam> (PID { "chorusMix", 1 }, "Chorus",
                                              linRange (0.0f, 100.0f), 0.0f));

    // Compresseur
    layout.add (std::make_unique<FloatParam> (PID { "compAmount", 1 }, "Comp",
                                              linRange (0.0f, 100.0f), 35.0f));
    layout.add (std::make_unique<FloatParam> (PID { "compAttack", 1 }, "Comp Attack",
                                              logRange (0.1f, 50.0f), 8.0f));

    // Clipper
    layout.add (std::make_unique<ChoiceParam> (PID { "clipType", 1 }, "Clip Type",
                                               juce::StringArray { "Soft", "Hard" }, 1));
    layout.add (std::make_unique<FloatParam> (PID { "clipCeiling", 1 }, "Ceiling",
                                              linRange (-12.0f, 0.0f), -0.3f));

    // Sortie
    layout.add (std::make_unique<FloatParam> (PID { "width", 1 }, "Width",
                                              linRange (0.0f, 150.0f), 100.0f));
    layout.add (std::make_unique<FloatParam> (PID { "outputGain", 1 }, "Gain",
                                              linRange (-24.0f, 6.0f), -1.0f));
    layout.add (std::make_unique<BoolParam> (PID { "keyTrack", 1 }, "Key Track", false));

    return layout;
}

kickforge::DriveStage::Parameters KickForgeAudioProcessor::currentDriveParams() const
{
    return { static_cast<kickforge::DriveStage::Type> (static_cast<int> (driveTypeParam->load())),
             driveAmountParam->load(),
             driveToneParam->load() };
}

kickforge::FilterStage::Parameters KickForgeAudioProcessor::currentFilterParams() const
{
    return { static_cast<kickforge::FilterStage::Type> (static_cast<int> (filterTypeParam->load())),
             filterCutoffParam->load(),
             filterResoParam->load() };
}

kickforge::EqStage::Parameters KickForgeAudioProcessor::currentEqParams() const
{
    kickforge::EqStage::Parameters p;
    for (int band = 0; band < 3; ++band)
        p.bands[band] = { eqFreqParams[band]->load(),
                          eqGainParams[band]->load(),
                          eqQParams[band]->load() };
    return p;
}

kickforge::DistStage::Parameters KickForgeAudioProcessor::currentDistParams() const
{
    return { static_cast<kickforge::DistStage::Type> (static_cast<int> (distTypeParam->load())),
             distAmountParam->load(),
             distMixParam->load() };
}

kickforge::FxStage::Parameters KickForgeAudioProcessor::currentFxParams() const
{
    return { chorusMixParam->load(), reverbMixParam->load() };
}

kickforge::CompStage::Parameters KickForgeAudioProcessor::currentCompParams() const
{
    return { compAmountParam->load(), compAttackParam->load() };
}

kickforge::ClipStage::Parameters KickForgeAudioProcessor::currentClipParams() const
{
    return { static_cast<kickforge::ClipStage::Type> (static_cast<int> (clipTypeParam->load())),
             clipCeilingParam->load() };
}

kickforge::WidthStage::Parameters KickForgeAudioProcessor::currentWidthParams() const
{
    // La garde de sécurité du width suit le ceiling du clipper
    return { widthParam->load(), clipCeilingParam->load() };
}

kickforge::KickVoice::Parameters KickForgeAudioProcessor::currentVoiceParams() const
{
    kickforge::KickVoice::Parameters p;
    p.waveform          = static_cast<kickforge::KickVoice::Waveform> (
                              static_cast<int> (waveformParam->load()));
    p.pitchStartHz      = pitchStartParam->load();
    p.pitchEndHz        = pitchEndParam->load();
    p.sweepTimeMs       = sweepTimeParam->load();
    p.oscBOn            = oscBOnParam->load() > 0.5f;
    p.oscBWave          = static_cast<kickforge::KickVoice::Waveform> (
                              static_cast<int> (oscBWaveParam->load()));
    p.oscBTuneSemitones = oscBTuneParam->load();
    p.oscBLevelPercent  = oscBLevelParam->load();
    p.attackMs          = attackParam->load();
    p.decayMs           = decayParam->load();
    return p;
}

kickforge::KickEngine::Parameters KickForgeAudioProcessor::currentEngineParams() const
{
    kickforge::KickEngine::Parameters p;
    p.punch      = currentVoiceParams();
    p.punchDrive = currentDriveParams();
    p.punchLevelPercent = punchLevelParam->load();
    p.attack = { atkLevelParam->load(), atkDecayParam->load(), atkToneParam->load() };

    p.crunch.levelPercent  = crunchLevelParam->load();
    p.crunch.wave          = static_cast<kickforge::CrunchLayer::Waveform> (
                                 static_cast<int> (crunchWaveParam->load()));
    p.crunch.tuneSemitones = crunchTuneParam->load();
    p.crunch.attackMs      = crunchAttackParam->load();
    p.crunch.decayMs       = crunchDecayParam->load();
    p.crunch.pitchEndHz    = pitchEndParam->load(); // le crunch suit le pitch de fin
    p.crunch.drive         = { static_cast<kickforge::DriveStage::Type> (
                                   static_cast<int> (crunchDriveTypeParam->load())),
                               crunchDriveAmountParam->load(),
                               crunchDriveToneParam->load() };
    return p;
}

kickforge::WavExporter::ChainParameters KickForgeAudioProcessor::currentChainParameters() const
{
    kickforge::WavExporter::ChainParameters p;
    p.engine       = currentEngineParams();
    p.filter       = currentFilterParams();
    p.eq           = currentEqParams();
    p.dist         = currentDistParams();
    p.fx           = currentFxParams();
    p.comp         = currentCompParams();
    p.clip         = currentClipParams();
    p.width        = currentWidthParams();
    p.outputGainDb = outputGainParam->load();
    return p;
}

void KickForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{


    const juce::dsp::ProcessSpec monoSpec { sampleRate,
                                            static_cast<juce::uint32> (samplesPerBlock), 1 };
    const juce::dsp::ProcessSpec stereoSpec { sampleRate,
                                              static_cast<juce::uint32> (samplesPerBlock), 2 };

    kickEngine.prepare (monoSpec);
    kickEngine.setParameters (currentEngineParams());
    kickEngine.reset();
    busScratch.setSize (1, samplesPerBlock);

    filterStage.prepare (monoSpec);
    filterStage.setParameters (currentFilterParams());
    filterStage.reset();

    eqStage.prepare (monoSpec);
    eqStage.setParameters (currentEqParams());
    eqStage.reset();

    distStage.prepare (monoSpec);
    distStage.setParameters (currentDistParams());
    distStage.reset();

    fxStage.prepare (stereoSpec);
    fxStage.setParameters (currentFxParams());
    fxStage.reset();

    compStage.prepare (stereoSpec);
    compStage.setParameters (currentCompParams());
    compStage.reset();

    clipStage.prepare (stereoSpec);
    clipStage.setParameters (currentClipParams());
    clipStage.reset();

    widthStage.prepare (stereoSpec);
    widthStage.setParameters (currentWidthParams());
    widthStage.reset();

    setLatencySamples (static_cast<int> (std::round (kickEngine.getLatencyInSamples())));

    outputGainLinear.reset (sampleRate, 0.02);
    outputGainLinear.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (outputGainParam->load()));
}

void KickForgeAudioProcessor::releaseResources()
{
}

bool KickForgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void KickForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    if (numSamples == 0)
        return;

    kickEngine.setParameters (currentEngineParams());

    // Loop de pré-écoute : le retrigger sample-accurate vit dans le moteur
    kickEngine.setLoopIntervalSamples (static_cast<int> (
        loopIntervalSeconds.load (std::memory_order_relaxed) * getSampleRate()));
    kickEngine.setLooping (loopEnabled.load (std::memory_order_relaxed));

    // Bouton Play de l'UI : équivalent d'un note-on en tête de bloc
    if (playRequested.exchange (false, std::memory_order_relaxed))
        kickEngine.noteOn();

    // Rendu mono sample-accurate : on rend par segments entre les note-on.
    // Le moteur remplit deux bus : busAP (attack+punch, futur send reverb) et
    // le mix complet dans le canal 0 du buffer.
    auto* mono  = buffer.getWritePointer (0);
    auto* busAP = busScratch.getWritePointer (0);
    int position = 0;

    for (const auto metadata : midiMessages)
    {
        if (! metadata.getMessage().isNoteOn())
            continue;

        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        kickEngine.render (busAP + position, mono + position, eventPos - position);
        position = eventPos;
        kickEngine.noteOn();
    }

    kickEngine.render (busAP + position, mono + position, numSamples - position);

    // Chaîne mono commune : filtre -> EQ -> dist parallèle (le drive du punch
    // vit désormais dans le moteur, par couche)
    filterStage.setParameters (currentFilterParams());
    eqStage.setParameters (currentEqParams());
    distStage.setParameters (currentDistParams());
    fxStage.setParameters (currentFxParams());
    compStage.setParameters (currentCompParams());
    clipStage.setParameters (currentClipParams());
    widthStage.setParameters (currentWidthParams());

    float* monoChannel[] = { mono };
    juce::dsp::AudioBlock<float> monoBlock (monoChannel, 1,
                                            static_cast<size_t> (numSamples));
    filterStage.process (monoBlock);
    eqStage.process (monoBlock);
    distStage.process (monoBlock);

    // Passage en stéréo : FX (chorus -> reverb), c'est ici que la largeur naît
    if (buffer.getNumChannels() > 1)
    {
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

        float* stereoChannels[] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
        juce::dsp::AudioBlock<float> stereoBlock (stereoChannels, 2,
                                                  static_cast<size_t> (numSamples));
        fxStage.process (stereoBlock, busAP, static_cast<size_t> (numSamples));

        // Fin de chaîne : compresseur -> clipper -> width (sub mono < 150 Hz)
        compStage.process (stereoBlock);
        clipStage.process (stereoBlock);
        widthStage.process (stereoBlock);
    }

    outputGainLinear.setTargetValue (
        juce::Decibels::decibelsToGain (outputGainParam->load()));
    outputGainLinear.applyGain (buffer, numSamples);

    outputPeak.store (buffer.getMagnitude (0, numSamples), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* KickForgeAudioProcessor::createEditor()
{
    return new KickForgeAudioProcessorEditor (*this);
}

void KickForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void KickForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        kickforge::presets::migrateStateToV2 (*xml); // no-op si déjà v2

        if (xml->hasTagName (apvts.state.getType()))
        {
            // La restauration change potentiellement `genre` : il ne doit PAS
            // déclencher un chargement de preset qui écraserait l'état restauré.
            suppressPresetLoad = true;
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            cancelPendingUpdate();
            pendingGenre.store (-1);
            suppressPresetLoad = false;
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KickForgeAudioProcessor();
}
