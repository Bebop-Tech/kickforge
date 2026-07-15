#include "SectionPanel.h"

namespace kickforge::ui
{

// --- Formatteurs ---------------------------------------------------------------

juce::String formatHz (float hz)
{
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, 1) + " kHz";
    return juce::String (juce::roundToInt (hz)) + " Hz";
}

juce::String formatMs (float ms)
{
    if (ms < 10.0f)
        return juce::String (ms, 1) + " ms";
    return juce::String (juce::roundToInt (ms)) + " ms";
}

juce::String formatPercent (float percent)
{
    return juce::String (juce::roundToInt (percent)) + " %";
}

juce::String formatDb (float db)
{
    return juce::String (db, 1) + " dB";
}

juce::String formatSemitones (float st)
{
    const int rounded = juce::roundToInt (st);
    return (rounded > 0 ? "+" : "") + juce::String (rounded) + " st";
}

juce::String formatSeconds (float seconds)
{
    return juce::String (seconds, 1) + " s";
}

juce::String formatPlain (float v)
{
    return juce::String (v, 1);
}

// --- SectionPanel ----------------------------------------------------------------

SectionPanel::SectionPanel (const juce::String& titleIn, const juce::String& subtitleIn)
    : title (titleIn.toUpperCase()), subtitle (subtitleIn)
{
}

void SectionPanel::setCornerComponent (juce::Component* component)
{
    corner = component;
    if (corner != nullptr)
        addAndMakeVisible (corner);
    resized();
}

juce::Rectangle<int> SectionPanel::content() const
{
    return getLocalBounds().reduced (11, 9).withTrimmedTop (18);
}

void SectionPanel::paint (juce::Graphics& g)
{
    g.setColour (colours::section);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.setColour (colours::chipText);

    auto titleArea = getLocalBounds().reduced (11, 9).removeFromTop (14);
    g.drawText (title, titleArea, juce::Justification::centredLeft);

    if (subtitle.isNotEmpty())
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (juce::Font (juce::FontOptions (11.0f)), title, 0.0f, 0.0f);
        const int titleWidth = juce::roundToInt (
            glyphs.getBoundingBox (0, -1, true).getWidth());
        g.setColour (colours::faint);
        g.drawText (juce::String::fromUTF8 ("— ") + subtitle,
                    titleArea.withTrimmedLeft (titleWidth + 6),
                    juce::Justification::centredLeft);
    }
}

void SectionPanel::resized()
{
    if (corner != nullptr)
        corner->setBounds (getLocalBounds().reduced (11, 7).removeFromTop (16)
                               .removeFromRight (44));
}

// --- LabelledKnob -----------------------------------------------------------------

LabelledKnob::LabelledKnob (juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& paramID, const juce::String& displayName,
                            juce::Colour indicatorColour,
                            std::function<juce::String (float)> formatter)
    : name (displayName), format (std::move (formatter))
{
    slider.setColour (juce::Slider::thumbColourId, indicatorColour);
    slider.setDoubleClickReturnValue (true, 0.0); // la valeur exacte vient de l'attachment
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, paramID, slider);

    // Double-clic = retour au défaut du paramètre
    if (auto* parameter = apvts.getParameter (paramID))
        slider.setDoubleClickReturnValue (
            true, parameter->convertFrom0to1 (parameter->getDefaultValue()));

    slider.onValueChange = [this] { refreshValueText(); };
    refreshValueText();
}

LabelledKnob::LabelledKnob (const juce::String& displayName, juce::Colour indicatorColour,
                            std::function<juce::String (float)> formatter,
                            juce::NormalisableRange<double> range, double defaultValue,
                            std::function<void (float)> onChange)
    : name (displayName), format (std::move (formatter))
{
    slider.setColour (juce::Slider::thumbColourId, indicatorColour);
    slider.setNormalisableRange (range);
    slider.setValue (defaultValue, juce::dontSendNotification);
    slider.setDoubleClickReturnValue (true, defaultValue);
    addAndMakeVisible (slider);

    slider.onValueChange = [this, change = std::move (onChange)]
    {
        refreshValueText();
        change (static_cast<float> (slider.getValue()));
    };
    refreshValueText();
}

void LabelledKnob::refreshValueText()
{
    valueText = format (static_cast<float> (slider.getValue()));
    repaint();
}

void LabelledKnob::resized()
{
    auto area = getLocalBounds();
    auto knobArea = area.removeFromTop (area.getHeight() - 24);
    const int size = juce::jmin (knobArea.getWidth(), knobArea.getHeight());
    slider.setBounds (knobArea.withSizeKeepingCentre (size, size));
}

void LabelledKnob::paint (juce::Graphics& g)
{
    auto textArea = getLocalBounds().removeFromBottom (24);

    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (colours::chipText);
    g.drawText (name, textArea.removeFromTop (12), juce::Justification::centred);

    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.setColour (colours::value);
    g.drawText (valueText, textArea, juce::Justification::centred);
}

// --- SegmentedControl ----------------------------------------------------------------

SegmentedControl::SegmentedControl (juce::RangedAudioParameter& parameterIn,
                                    const juce::StringArray& labels, bool vertical)
    : parameter (parameterIn),
      attachment (parameterIn, [this] (float v) { updateFromValue (v); }),
      isVertical (vertical)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* button = buttons.add (new juce::TextButton (labels[i]));
        button->setClickingTogglesState (false);
        button->onClick = [this, i] { select (i); };
        addAndMakeVisible (button);
    }

    attachment.sendInitialUpdate();
}

void SegmentedControl::select (int index)
{
    attachment.setValueAsCompleteGesture (static_cast<float> (index));
}

void SegmentedControl::updateFromValue (float denormalisedValue)
{
    const int index = juce::roundToInt (denormalisedValue);
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
}

void SegmentedControl::resized()
{
    auto area = getLocalBounds();
    const int count = buttons.size();
    if (count == 0)
        return;

    const int gap = 4;
    if (isVertical)
    {
        const int h = (area.getHeight() - gap * (count - 1)) / count;
        for (int i = 0; i < count; ++i)
        {
            buttons[i]->setBounds (area.removeFromTop (h));
            area.removeFromTop (gap);
        }
    }
    else
    {
        const int w = (area.getWidth() - gap * (count - 1)) / count;
        for (int i = 0; i < count; ++i)
        {
            buttons[i]->setBounds (area.removeFromLeft (w));
            area.removeFromLeft (gap);
        }
    }
}

// --- GainReductionMeter -----------------------------------------------------------------

GainReductionMeter::GainReductionMeter (std::function<float()> gainReductionDbProvider)
    : provider (std::move (gainReductionDbProvider))
{
    startTimerHz (30);
}

void GainReductionMeter::timerCallback()
{
    const float target = provider();
    // Montée immédiate, retombée douce
    displayedDb = target > displayedDb ? target : displayedDb * 0.85f;
    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();

    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.setColour (colours::chipText);
    g.drawText ("GR", area.removeFromLeft (18), juce::Justification::centredLeft);

    auto textArea = area.removeFromRight (46);
    g.setColour (colours::value);
    g.drawText (displayedDb > 0.05f ? juce::String ("-") + juce::String (displayedDb, 1) + " dB"
                                    : juce::String ("0 dB"),
                textArea, juce::Justification::centredRight);

    auto barArea = area.reduced (4, 0).withSizeKeepingCentre (area.getWidth() - 8, 5).toFloat();
    g.setColour (colours::chip);
    g.fillRoundedRectangle (barArea, 3.0f);

    const float fraction = juce::jlimit (0.0f, 1.0f, displayedDb / 12.0f);
    g.setColour (colours::comp);
    g.fillRoundedRectangle (barArea.removeFromLeft (barArea.getWidth() * fraction), 3.0f);
}

// --- OutputMeter -------------------------------------------------------------------------

OutputMeter::OutputMeter (std::function<float()> peakProvider)
    : provider (std::move (peakProvider))
{
    startTimerHz (30);
}

void OutputMeter::timerCallback()
{
    const float target = provider();
    displayedPeak = target > displayedPeak ? target : displayedPeak * 0.9f;
    repaint();
}

void OutputMeter::paint (juce::Graphics& g)
{
    constexpr int numSegments = 7;
    // Seuils en dB par segment
    constexpr float thresholds[numSegments] = { -30.0f, -24.0f, -18.0f, -12.0f,
                                                -6.0f, -3.0f, -0.5f };

    const float levelDb = juce::Decibels::gainToDecibels (displayedPeak, -60.0f);
    auto area = getLocalBounds().toFloat();
    const float gap = 2.0f;
    const float w = (area.getWidth() - gap * (numSegments - 1)) / numSegments;

    for (int i = 0; i < numSegments; ++i)
    {
        const auto segment = juce::Rectangle<float> (area.getX() + i * (w + gap),
                                                     area.getY(), w, area.getHeight());
        const bool lit = levelDb >= thresholds[i];
        auto colour = colours::chip;
        if (lit)
            colour = i < 5 ? colours::env : (i == 5 ? colours::comp : colours::drive);
        g.setColour (colour);
        g.fillRoundedRectangle (segment, 2.0f);
    }
}

// --- LayerMiniWaveform ---------------------------------------------------------------------

LayerMiniWaveform::LayerMiniWaveform (const WaveformDisplay::Silhouette& shapeRef,
                                      juce::Colour colour)
    : shape (shapeRef), waveColour (colour)
{
    setInterceptsMouseClicks (false, false);
}

void LayerMiniWaveform::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (colours::inset);
    g.fillRoundedRectangle (area, 4.0f);

    if (shape.high.empty() || shape.peak < 1.0e-6f)
    {
        g.setColour (colours::faint);
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText ("off", area, juce::Justification::centred);
        return;
    }

    const auto plot = area.reduced (3.0f, 2.0f);
    const float midY = plot.getCentreY();
    const float scale = plot.getHeight() * 0.5f / shape.peak;
    const auto columns = static_cast<int> (shape.high.size());

    juce::Path wave;
    for (int col = 0; col < columns; ++col)
    {
        const float x = plot.getX() + plot.getWidth() * col / (columns - 1);
        const float y = midY - shape.high[static_cast<size_t> (col)] * scale;
        if (col == 0)
            wave.startNewSubPath (x, y);
        else
            wave.lineTo (x, y);
    }
    for (int col = columns - 1; col >= 0; --col)
    {
        const float x = plot.getX() + plot.getWidth() * col / (columns - 1);
        wave.lineTo (x, midY - shape.low[static_cast<size_t> (col)] * scale);
    }
    wave.closeSubPath();
    g.setColour (waveColour.withAlpha (0.8f));
    g.fillPath (wave);
}

// --- ClipTransferDisplay -------------------------------------------------------------------

ClipTransferDisplay::ClipTransferDisplay (juce::AudioProcessorValueTreeState& apvtsIn)
    : apvts (apvtsIn),
      typeAttachment (*apvts.getParameter ("clipType"), [this] (float) { repaint(); }),
      ceilingAttachment (*apvts.getParameter ("clipCeiling"), [this] (float) { repaint(); })
{
}

void ClipTransferDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (colours::inset);
    g.fillRoundedRectangle (area, 4.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (area.reduced (0.5f), 4.0f, 1.0f);

    const bool isSoft = apvts.getRawParameterValue ("clipType")->load() < 0.5f;
    const float ceiling =
        juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("clipCeiling")->load());

    auto plot = area.reduced (5.0f);
    juce::Path curve;
    constexpr float maxIn = 1.4f;

    for (int px = 0; px <= 40; ++px)
    {
        const float in = maxIn * px / 40.0f;
        const float out = isSoft ? ceiling * std::tanh (in / ceiling)
                                 : juce::jmin (in, ceiling);
        const float x = plot.getX() + plot.getWidth() * in / maxIn;
        const float y = plot.getBottom() - plot.getHeight() * out / maxIn;
        if (px == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    g.setColour (colours::drive);
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}

} // namespace kickforge::ui
