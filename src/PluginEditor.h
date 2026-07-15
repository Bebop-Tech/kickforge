#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "presets/GenrePresets.h"
#include "ui/LookAndFeel.h"
#include "ui/SectionPanel.h"
#include "ui/PresetBar.h"
#include "ui/WaveformDisplay.h"
#include "ui/EqCurveDisplay.h"

// UI complète (maquette v3) : header + presets, waveform, 4 rangées de
// sections, 760 x 700 non redimensionnable.
class KickForgeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit KickForgeAudioProcessorEditor (KickForgeAudioProcessor& ownerProcessor);
    ~KickForgeAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void launchExport();

    // Bouton play rond
    class PlayButton : public juce::Button
    {
    public:
        PlayButton() : juce::Button ("Play") {}
        void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
    };

    KickForgeAudioProcessor& processor;
    kickforge::ui::KickForgeLookAndFeel lookAndFeel;

    kickforge::ui::PresetBar presetBar;
    kickforge::ui::WaveformDisplay waveform;

    kickforge::ui::SectionPanel attackPanel, punchPanel, crunchPanel, oscAPanel, oscBPanel,
        envPanel, drivePanel, filterPanel, eqPanel, distPanel, fxPanel, compPanel, clipPanel,
        outPanel;

    kickforge::ui::SegmentedControl waveASelect, waveBSelect, crunchWaveSelect,
        crunchDriveSelect, driveSelect, filterSelect, distSelect, clipSelect;

    juce::TextButton oscBOnButton { "ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> oscBOnAttachment;

    kickforge::ui::LabelledKnob pitchStartKnob, pitchEndKnob, sweepKnob, tuneKnob, levelKnob,
        attackKnob, decayKnob, atkLevelKnob, atkDecayKnob, atkToneKnob, punchLevelKnob,
        crunchLevelKnob, crunchTuneKnob, crunchAttackKnob, crunchDecayKnob,
        crunchDriveAmountKnob, crunchDriveToneKnob,
        driveAmountKnob, driveToneKnob, cutoffKnob, resoKnob,
        distAmountKnob, distMixKnob, reverbKnob, chorusKnob, compKnob, compAttackKnob,
        ceilingKnob, widthKnob, gainKnob;

    kickforge::ui::LayerMiniWaveform attackMini, punchMini, crunchMini;
    kickforge::ui::EqCurveDisplay eqCurve;
    kickforge::ui::GainReductionMeter grMeter;
    kickforge::ui::OutputMeter outputMeter;
    kickforge::ui::ClipTransferDisplay clipTransfer;

    PlayButton playButton;
    juce::TextButton loopButton { "Loop" };
    kickforge::ui::LabelledKnob loopIntervalKnob;
    juce::TextButton randomButton { "Random" }, exportButton { "Export" };
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KickForgeAudioProcessorEditor)
};
