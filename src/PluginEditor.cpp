#include "PluginEditor.h"

using namespace kickforge::ui;

namespace
{
juce::RangedAudioParameter& param (juce::AudioProcessorValueTreeState& apvts,
                                   const char* id)
{
    auto* p = apvts.getParameter (id);
    jassert (p != nullptr);
    return *p;
}
} // namespace

void KickForgeAudioProcessorEditor::PlayButton::paintButton (juce::Graphics& g,
                                                             bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f);
    auto colour = colours::accent;
    if (! isEnabled())
        colour = colour.withAlpha (0.35f);
    else if (down)
        colour = colour.brighter (0.15f);
    else if (highlighted)
        colour = colour.brighter (0.08f);

    g.setColour (colour);
    g.fillEllipse (area);

    juce::Path triangle;
    const auto c = area.getCentre();
    const float r = area.getWidth() * 0.2f;
    triangle.addTriangle (c.x - r * 0.6f, c.y - r, c.x - r * 0.6f, c.y + r,
                          c.x + r * 1.1f, c.y);
    g.setColour (juce::Colours::white.withAlpha (isEnabled() ? 1.0f : 0.5f));
    g.fillPath (triangle);
}

KickForgeAudioProcessorEditor::KickForgeAudioProcessorEditor (KickForgeAudioProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor (p),
      presetBar (p.getApvts()),
      waveform (p.getApvts()),
      attackPanel ("Attack", "click"),
      punchPanel ("Punch"),
      crunchPanel ("Crunch", "queue saturée"),
      oscAPanel ("Osc A", "corps"),
      oscBPanel ("Osc B", "couche"),
      envPanel ("Enveloppe"),
      drivePanel ("Drive", "punch"),
      filterPanel ("Filtre"),
      eqPanel (juce::String::fromUTF8 ("EQ paramétrique")),
      distPanel ("Distorsion"),
      fxPanel ("FX"),
      compPanel ("Compresseur"),
      clipPanel ("Clipper"),
      outPanel ("Sortie"),
      waveASelect (param (p.getApvts(), "waveform"), { "Sin", "Tri", "Sqr", "Saw" }),
      waveBSelect (param (p.getApvts(), "oscBWave"), { "Sin", "Tri", "Sqr", "Saw" }),
      crunchWaveSelect (param (p.getApvts(), "crunchWave"), { "Sin", "Tri" }),
      crunchDriveSelect (param (p.getApvts(), "crunchDriveType"), { "Soft", "Hard", "Fold" }),
      driveSelect (param (p.getApvts(), "driveType"), { "Soft", "Hard", "Fold" }),
      filterSelect (param (p.getApvts(), "filterType"), { "LP", "HP", "BP" }),
      distSelect (param (p.getApvts(), "distType"), { "Tube", "Fuzz", "Bit" }),
      clipSelect (param (p.getApvts(), "clipType"), { "Soft", "Hard" }, true),
      pitchStartKnob (p.getApvts(), "pitchStart", "P. start", colours::osc, formatHz),
      pitchEndKnob (p.getApvts(), "pitchEnd", "P. end", colours::osc, formatHz),
      sweepKnob (p.getApvts(), "sweepTime", "Sweep", colours::osc, formatMs),
      tuneKnob (p.getApvts(), "oscBTune", "Tune", colours::osc, formatSemitones),
      levelKnob (p.getApvts(), "oscBLevel", "Level", colours::osc, formatPercent),
      attackKnob (p.getApvts(), "attack", "Attack", colours::env, formatMs),
      decayKnob (p.getApvts(), "decay", "Decay", colours::env, formatMs),
      atkLevelKnob (p.getApvts(), "atkLevel", "Level", colours::comp, formatPercent),
      atkDecayKnob (p.getApvts(), "atkDecay", "Decay", colours::comp, formatMs),
      atkToneKnob (p.getApvts(), "atkTone", "Tone", colours::comp, formatHz),
      punchLevelKnob (p.getApvts(), "punchLevel", "Level", colours::osc, formatPercent),
      crunchLevelKnob (p.getApvts(), "crunchLevel", "Level", colours::drive, formatPercent),
      crunchTuneKnob (p.getApvts(), "crunchTune", "Tune", colours::drive, formatSemitones),
      crunchAttackKnob (p.getApvts(), "crunchAttack", "Attack", colours::drive, formatMs),
      crunchDecayKnob (p.getApvts(), "crunchDecay", "Decay", colours::drive, formatMs),
      crunchDriveAmountKnob (p.getApvts(), "crunchDriveAmount", "Amount", colours::drive, formatPercent),
      crunchDriveToneKnob (p.getApvts(), "crunchDriveTone", "Tone", colours::drive, formatHz),
      driveAmountKnob (p.getApvts(), "driveAmount", "Amount", colours::drive, formatPercent),
      driveToneKnob (p.getApvts(), "driveTone", "Tone", colours::drive, formatHz),
      cutoffKnob (p.getApvts(), "filterCutoff", "Cutoff", colours::filter, formatHz),
      resoKnob (p.getApvts(), "filterReso", "Reso", colours::filter, formatPercent),
      distAmountKnob (p.getApvts(), "distAmount", "Amount", colours::drive, formatPercent),
      distMixKnob (p.getApvts(), "distMix", "Mix", colours::drive, formatPercent),
      reverbKnob (p.getApvts(), "reverbMix", "Reverb", colours::fx, formatPercent),
      chorusKnob (p.getApvts(), "chorusMix", "Chorus", colours::fx, formatPercent),
      compKnob (p.getApvts(), "compAmount", "Comp", colours::comp, formatPercent),
      compAttackKnob (p.getApvts(), "compAttack", "Attack", colours::comp, formatMs),
      ceilingKnob (p.getApvts(), "clipCeiling", "Ceiling", colours::drive, formatDb),
      widthKnob (p.getApvts(), "width", "Width", colours::width, formatPercent),
      gainKnob (p.getApvts(), "outputGain", "Gain", colours::width, formatDb),
      loopIntervalKnob ("Interval", colours::width, formatSeconds,
                        juce::NormalisableRange<double> (0.2, 2.0, 0.0, 0.5), 1.0,
                        [&p] (float seconds) { p.setLoopIntervalSeconds (seconds); }),
      attackMini (waveform.attackShape(), colours::comp),
      punchMini (waveform.punchShape(), colours::osc),
      crunchMini (waveform.crunchShape(), colours::drive),
      eqCurve (p.getApvts()),
      grMeter ([&p] { return p.getGainReductionDb(); }),
      outputMeter ([&p] { return p.getOutputPeak(); }),
      clipTransfer (p.getApvts())
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (presetBar);
    addAndMakeVisible (waveform);

    for (auto* panel : { &attackPanel, &punchPanel, &crunchPanel, &oscAPanel, &oscBPanel,
                         &envPanel, &drivePanel, &filterPanel, &eqPanel, &distPanel, &fxPanel,
                         &compPanel, &clipPanel, &outPanel })
        addAndMakeVisible (panel);

    for (auto* control : { &waveASelect, &waveBSelect, &crunchWaveSelect, &crunchDriveSelect,
                           &driveSelect, &filterSelect, &distSelect, &clipSelect })
        addAndMakeVisible (control);

    oscBOnButton.setClickingTogglesState (true);
    oscBOnButton.setColour (juce::TextButton::buttonOnColourId, colours::onBadgeBg);
    oscBOnButton.setColour (juce::TextButton::textColourOnId, colours::env);
    oscBOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.getApvts(), "oscBOn", oscBOnButton);
    oscBPanel.setCornerComponent (&oscBOnButton);

    for (auto* knob : { &pitchStartKnob, &pitchEndKnob, &sweepKnob, &tuneKnob, &levelKnob,
                        &attackKnob, &decayKnob, &atkLevelKnob, &atkDecayKnob, &atkToneKnob,
                        &punchLevelKnob, &crunchLevelKnob, &crunchTuneKnob, &crunchAttackKnob,
                        &crunchDecayKnob, &crunchDriveAmountKnob, &crunchDriveToneKnob,
                        &driveAmountKnob, &driveToneKnob,
                        &cutoffKnob, &resoKnob, &distAmountKnob, &distMixKnob, &reverbKnob,
                        &chorusKnob, &compKnob, &compAttackKnob, &ceilingKnob, &widthKnob,
                        &gainKnob })
        addAndMakeVisible (knob);

    for (auto* mini : { &attackMini, &punchMini, &crunchMini })
        addAndMakeVisible (mini);
    waveform.onShapesUpdated = [this]
    {
        attackMini.repaint();
        punchMini.repaint();
        crunchMini.repaint();
    };

    addAndMakeVisible (eqCurve);
    addAndMakeVisible (grMeter);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (clipTransfer);

    playButton.onClick = [this] { processor.triggerPlay(); };

    loopButton.setClickingTogglesState (true);
    loopButton.onClick = [this] { processor.setLoopEnabled (loopButton.getToggleState()); };
    addAndMakeVisible (loopButton);
    addAndMakeVisible (loopIntervalKnob);
    exportButton.onClick = [this] { launchExport(); };
    addAndMakeVisible (playButton);
    addAndMakeVisible (exportButton);

    randomButton.onClick = [this] { processor.randomizeAroundActivePreset(); };
    addAndMakeVisible (randomButton);

    setResizable (false, false);
    setSize (760, 841);
}

KickForgeAudioProcessorEditor::~KickForgeAudioProcessorEditor()
{
    processor.setLoopEnabled (false); // la pré-écoute ne survit pas à l'éditeur
    setLookAndFeel (nullptr);
}

void KickForgeAudioProcessorEditor::launchExport()
{
    const int genre = juce::jlimit (0, kickforge::presets::numGenres - 1,
                                    static_cast<int> (std::lround (
                                        processor.getApvts().getRawParameterValue ("genre")->load())));
    const auto defaultName = "kickforge-"
                             + juce::String (kickforge::presets::genrePresets[genre].name)
                                   .toLowerCase().replaceCharacter (' ', '-')
                             + ".wav";

    fileChooser = std::make_unique<juce::FileChooser> (
        "Exporter le kick en WAV",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (defaultName),
        "*.wav");

    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file == juce::File())
                return;

            const auto params = processor.currentChainParameters();
            const auto buffer = kickforge::WavExporter::renderKick (
                params, kickforge::WavExporter::lengthSecondsFor (params));

            if (! kickforge::WavExporter::writeWavFile (buffer,
                                                        file.withFileExtension ("wav")))
                juce::AlertWindow::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon, "Export WAV",
                    juce::String::fromUTF8 ("Impossible d'écrire le fichier :\n")
                        + file.getFullPathName());
        });
}

void KickForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::windowBg);

    // Header
    auto header = getLocalBounds().removeFromTop (46);
    g.setColour (colours::section);
    g.fillRect (header);
    g.setColour (colours::border);
    g.drawHorizontalLine (header.getBottom() - 1, 0.0f, static_cast<float> (getWidth()));

    auto titleArea = header.reduced (18, 0);
    g.setFont (juce::Font (juce::FontOptions (17.0f)));
    g.setColour (colours::title);
    g.drawText ("KICKFORGE", titleArea, juce::Justification::centredLeft);

    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.setColour (colours::muted);
    g.drawText ("by Bebop Tech", titleArea.withTrimmedLeft (118),
                juce::Justification::centredLeft);
}

void KickForgeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header : barre de presets à droite
    auto header = area.removeFromTop (46);
    presetBar.setBounds (header.reduced (18, 9).removeFromRight (392));

    area.reduce (18, 0);
    area.removeFromTop (12);
    waveform.setBounds (area.removeFromTop (96));

    auto layoutRow = [&area] (std::initializer_list<std::pair<juce::Component*, float>> panels,
                              int height)
    {
        area.removeFromTop (9);
        auto row = area.removeFromTop (height);
        float totalWeight = 0.0f;
        for (const auto& [component, weight] : panels)
            totalWeight += weight;

        const int gap = 9;
        int remaining = row.getWidth() - gap * (static_cast<int> (panels.size()) - 1);
        float consumedWeight = 0.0f;
        int consumedPixels = 0;

        for (const auto& [component, weight] : panels)
        {
            consumedWeight += weight;
            const int right = juce::roundToInt (remaining * consumedWeight / totalWeight);
            component->setBounds (row.removeFromLeft (right - consumedPixels));
            row.removeFromLeft (gap);
            consumedPixels = right;
        }
    };

    layoutRow ({ { &attackPanel, 1.0f }, { &punchPanel, 0.55f }, { &crunchPanel, 2.1f } }, 132);
    layoutRow ({ { &oscAPanel, 1.25f }, { &oscBPanel, 1.0f }, { &envPanel, 1.0f } }, 132);
    layoutRow ({ { &drivePanel, 1.0f }, { &filterPanel, 1.0f }, { &eqPanel, 1.4f } }, 132);
    layoutRow ({ { &distPanel, 1.1f }, { &fxPanel, 1.0f }, { &compPanel, 1.0f } }, 132);
    layoutRow ({ { &clipPanel, 1.0f }, { &outPanel, 1.6f } }, 104);

    auto knobRow = [] (juce::Rectangle<int> row,
                       std::initializer_list<kickforge::ui::LabelledKnob*> knobs)
    {
        const int w = row.getWidth() / static_cast<int> (knobs.size());
        for (auto* knob : knobs)
            knob->setBounds (row.removeFromLeft (w).reduced (2, 0));
    };

    // Attack
    {
        auto content = attackPanel.content() + attackPanel.getPosition();
        attackMini.setBounds (content.removeFromTop (22));
        content.removeFromTop (4);
        knobRow (content, { &atkLevelKnob, &atkDecayKnob, &atkToneKnob });
    }
    // Punch (level : le corps se règle dans Osc A/B et Enveloppe)
    {
        auto content = punchPanel.content() + punchPanel.getPosition();
        punchMini.setBounds (content.removeFromTop (22));
        content.removeFromTop (4);
        knobRow (content, { &punchLevelKnob });
    }
    // Crunch
    {
        auto content = crunchPanel.content() + crunchPanel.getPosition();
        auto selectors = content.removeFromTop (22);
        crunchWaveSelect.setBounds (selectors.removeFromLeft (86));
        selectors.removeFromLeft (8);
        crunchDriveSelect.setBounds (selectors.removeFromLeft (140));
        selectors.removeFromLeft (8);
        crunchMini.setBounds (selectors);
        content.removeFromTop (4);
        knobRow (content, { &crunchLevelKnob, &crunchTuneKnob, &crunchAttackKnob,
                            &crunchDecayKnob, &crunchDriveAmountKnob, &crunchDriveToneKnob });
    }
    // Osc A
    {
        auto content = oscAPanel.content() + oscAPanel.getPosition();
        waveASelect.setBounds (content.removeFromTop (22));
        content.removeFromTop (4);
        knobRow (content, { &pitchStartKnob, &pitchEndKnob, &sweepKnob });
    }
    // Osc B
    {
        auto content = oscBPanel.content() + oscBPanel.getPosition();
        waveBSelect.setBounds (content.removeFromTop (22));
        content.removeFromTop (4);
        knobRow (content, { &tuneKnob, &levelKnob });
    }
    // Enveloppe
    {
        auto content = envPanel.content() + envPanel.getPosition();
        content.removeFromTop (26);
        knobRow (content, { &attackKnob, &decayKnob });
    }
    // Drive
    {
        auto content = drivePanel.content() + drivePanel.getPosition();
        driveSelect.setBounds (content.removeFromTop (22));
        content.removeFromTop (4);
        knobRow (content, { &driveAmountKnob, &driveToneKnob });
    }
    // Filtre
    {
        auto content = filterPanel.content() + filterPanel.getPosition();
        filterSelect.setBounds (content.removeFromTop (22));
        content.removeFromTop (4);
        knobRow (content, { &cutoffKnob, &resoKnob });
    }
    // EQ
    eqCurve.setBounds (eqPanel.content() + eqPanel.getPosition());
    // Distorsion
    {
        auto content = distPanel.content() + distPanel.getPosition();
        distSelect.setBounds (content.removeFromTop (22));
        content.removeFromTop (4);
        knobRow (content, { &distAmountKnob, &distMixKnob });
    }
    // FX
    {
        auto content = fxPanel.content() + fxPanel.getPosition();
        content.removeFromTop (26);
        knobRow (content, { &reverbKnob, &chorusKnob });
    }
    // Compresseur
    {
        auto content = compPanel.content() + compPanel.getPosition();
        grMeter.setBounds (content.removeFromBottom (16));
        knobRow (content, { &compKnob, &compAttackKnob });
    }
    // Clipper
    {
        auto content = clipPanel.content() + clipPanel.getPosition();
        clipSelect.setBounds (content.removeFromLeft (58).reduced (0, 8));
        content.removeFromLeft (6);
        clipTransfer.setBounds (content.removeFromRight (56).withSizeKeepingCentre (56, 46));
        ceilingKnob.setBounds (content.reduced (4, 0));
    }
    // Sortie
    {
        auto content = outPanel.content() + outPanel.getPosition();
        auto playCluster = content.removeFromLeft (46);
        playButton.setBounds (playCluster.removeFromTop (playCluster.getHeight() - 18)
                                  .withSizeKeepingCentre (32, 32));
        loopButton.setBounds (playCluster.reduced (1, 0));
        content.removeFromLeft (4);
        loopIntervalKnob.setBounds (content.removeFromLeft (56));
        widthKnob.setBounds (content.removeFromLeft (56));
        gainKnob.setBounds (content.removeFromLeft (56));
        content.removeFromLeft (8);

        auto column = content.reduced (0, 2);
        outputMeter.setBounds (column.removeFromTop (9));
        column.removeFromTop (6);
        auto buttonsRow = column.removeFromTop (20);
        randomButton.setBounds (buttonsRow.removeFromLeft (buttonsRow.getWidth() / 2 - 3));
        buttonsRow.removeFromLeft (6);
        exportButton.setBounds (buttonsRow);
    }
}
