#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "LookAndFeel.h"
#include "WaveformDisplay.h"

namespace kickforge::ui
{

// --- Formatteurs de valeur ---------------------------------------------------

juce::String formatHz (float hz);
juce::String formatMs (float ms);
juce::String formatPercent (float percent);
juce::String formatDb (float db);
juce::String formatSemitones (float st);
juce::String formatSeconds (float seconds);
juce::String formatPlain (float v);

// --- Cadre de section --------------------------------------------------------

// Fond arrondi #1e1e24 + titre. Le contenu est posé par l'éditeur dans
// content(). Un composant "coin" optionnel (ex. toggle ON de l'Osc B) est
// placé en haut à droite.
class SectionPanel : public juce::Component
{
public:
    SectionPanel (const juce::String& title, const juce::String& subtitle = {});

    void setCornerComponent (juce::Component* component); // non possédé
    juce::Rectangle<int> content() const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::String title, subtitle;
    juce::Component* corner = nullptr;
};

// --- Knob avec nom + valeur --------------------------------------------------

class LabelledKnob : public juce::Component
{
public:
    LabelledKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID,
                  const juce::String& displayName, juce::Colour indicatorColour,
                  std::function<juce::String (float)> formatter);

    // Variante sans paramètre APVTS (contrôle purement UI, ex. intervalle de loop)
    LabelledKnob (const juce::String& displayName, juce::Colour indicatorColour,
                  std::function<juce::String (float)> formatter,
                  juce::NormalisableRange<double> range, double defaultValue,
                  std::function<void (float)> onChange);

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void refreshValueText();

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag,
                          juce::Slider::NoTextBox };
    juce::String name;
    juce::String valueText;
    std::function<juce::String (float)> format;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// --- Sélecteur segmenté (paramètre choix) -------------------------------------

class SegmentedControl : public juce::Component
{
public:
    SegmentedControl (juce::RangedAudioParameter& parameter, const juce::StringArray& labels,
                      bool vertical = false);

    void resized() override;

private:
    void select (int index);
    void updateFromValue (float denormalisedValue);

    juce::RangedAudioParameter& parameter;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
    bool isVertical;
};

// --- Vu-mètre de gain reduction ------------------------------------------------

class GainReductionMeter : public juce::Component, private juce::Timer
{
public:
    explicit GainReductionMeter (std::function<float()> gainReductionDbProvider);

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    std::function<float()> provider;
    float displayedDb = 0.0f;
};

// --- Vu-mètre de sortie (segments) ---------------------------------------------

class OutputMeter : public juce::Component, private juce::Timer
{
public:
    explicit OutputMeter (std::function<float()> peakProvider);

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    std::function<float()> provider;
    float displayedPeak = 0.0f;
};

// --- Mini-waveform d'une couche (attack / punch / crunch) -----------------------

// Affiche la silhouette d'une couche calculée par le WaveformDisplay,
// normalisée à son propre pic (on lit la FORME, pas le niveau).
class LayerMiniWaveform : public juce::Component
{
public:
    LayerMiniWaveform (const WaveformDisplay::Silhouette& shapeRef, juce::Colour colour);

    void paint (juce::Graphics& g) override;

private:
    const WaveformDisplay::Silhouette& shape;
    juce::Colour waveColour;
};

// --- Mini-courbe de transfert du clipper ----------------------------------------

class ClipTransferDisplay : public juce::Component
{
public:
    explicit ClipTransferDisplay (juce::AudioProcessorValueTreeState& apvts);

    void paint (juce::Graphics& g) override;

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::ParameterAttachment typeAttachment, ceilingAttachment;
};

} // namespace kickforge::ui
