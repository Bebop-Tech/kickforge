#include "EqCurveDisplay.h"
#include "SectionPanel.h"
#include "UiScaling.h"

namespace kickforge::ui
{

namespace
{
constexpr float minFreq = 20.0f, maxFreq = 18000.0f;
constexpr float maxGainDb = 12.0f;
constexpr float minQ = 0.3f, maxQ = 6.0f;
constexpr int chipRowHeight = 20;
} // namespace

EqCurveDisplay::EqCurveDisplay (juce::AudioProcessorValueTreeState& apvtsIn)
    : apvts (apvtsIn)
{
    for (int band = 0; band < 3; ++band)
    {
        const auto n = juce::String (band + 1);

        freqAttachments[band] = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter ("eq" + n + "Freq"),
            [this, band] (float v) { freq[band] = v; refreshReadout(); });
        gainAttachments[band] = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter ("eq" + n + "Gain"),
            [this, band] (float v) { gainDb[band] = v; refreshReadout(); });
        qAttachments[band] = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter ("eq" + n + "Q"),
            [this, band] (float v) { q[band] = v; refreshReadout(); });

        freqAttachments[band]->sendInitialUpdate();
        gainAttachments[band]->sendInitialUpdate();
        qAttachments[band]->sendInitialUpdate();

        bandChips[band].setButtonText ("B" + n);
        bandChips[band].setClickingTogglesState (false);
        bandChips[band].onClick = [this, band] { setActiveBand (band); };
        bandChips[band].setColour (juce::TextButton::buttonOnColourId, colours::filter);
        bandChips[band].setColour (juce::TextButton::textColourOnId,
                                   juce::Colour (0xff26215c));
        addAndMakeVisible (bandChips[band]);
    }

    readout.setFont (juce::Font (juce::FontOptions (10.0f)));
    readout.setColour (juce::Label::textColourId, colours::chipText);
    readout.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (readout);

    setActiveBand (0);
}

juce::Rectangle<float> EqCurveDisplay::plotArea() const
{
    return getLocalBounds().withTrimmedBottom (chipRowHeight + 4).toFloat();
}

juce::Point<float> EqCurveDisplay::bandPosition (int band) const
{
    const auto plot = plotArea().reduced (6.0f, 6.0f);
    return { plot.getX() + plot.getWidth() * freqToNorm (freq[band], minFreq, maxFreq),
             plot.getCentreY() - plot.getHeight() * 0.5f * (gainDb[band] / maxGainDb) };
}

void EqCurveDisplay::setActiveBand (int band)
{
    activeBand = band;
    for (int i = 0; i < 3; ++i)
        bandChips[i].setToggleState (i == band, juce::dontSendNotification);
    refreshReadout();
}

void EqCurveDisplay::refreshReadout()
{
    const auto sep = juce::String::fromUTF8 ("  ·  ");
    readout.setText ("Freq " + formatHz (freq[activeBand])
                         + sep + "Gain " + (gainDb[activeBand] > 0 ? "+" : "")
                         + juce::String (gainDb[activeBand], 1) + " dB"
                         + sep + "Q " + juce::String (q[activeBand], 1),
                     juce::dontSendNotification);
    repaint();
}

void EqCurveDisplay::resized()
{
    auto row = getLocalBounds().removeFromBottom (chipRowHeight);
    for (auto& chip : bandChips)
    {
        chip.setBounds (row.removeFromLeft (32).reduced (0, 2));
        row.removeFromLeft (4);
    }
    readout.setBounds (row);
}

void EqCurveDisplay::paint (juce::Graphics& g)
{
    const auto plot = plotArea();
    g.setColour (colours::inset);
    g.fillRoundedRectangle (plot, 5.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (plot.reduced (0.5f), 5.0f, 1.0f);

    const auto inner = plot.reduced (6.0f, 6.0f);
    g.setColour (colours::insetLine);
    g.drawHorizontalLine (juce::roundToInt (inner.getCentreY()), plot.getX(), plot.getRight());

    // Courbe de réponse cumulée
    juce::Path curve;
    constexpr int numPoints = 96;
    for (int i = 0; i < numPoints; ++i)
    {
        const float f = normToFreq (i / static_cast<float> (numPoints - 1), minFreq, maxFreq);
        float db = 0.0f;
        for (int band = 0; band < 3; ++band)
            db += peakBandDbAt (f, freq[band], gainDb[band], q[band], 48000.0);

        const float x = inner.getX() + inner.getWidth() * i / (numPoints - 1);
        const float y = inner.getCentreY()
                        - inner.getHeight() * 0.5f * juce::jlimit (-1.0f, 1.0f, db / maxGainDb);
        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }
    g.setColour (colours::filter);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // Points des bandes
    for (int band = 0; band < 3; ++band)
    {
        const auto p = bandPosition (band);
        if (band == activeBand)
        {
            g.setColour (colours::filter);
            g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
            g.setColour (colours::title);
            g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 1.5f);
        }
        else
        {
            g.setColour (colours::faint);
            g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
        }
    }
}

void EqCurveDisplay::mouseDown (const juce::MouseEvent& event)
{
    if (! plotArea().contains (event.position))
        return;

    int nearest = -1;
    float bestDistance = 16.0f;
    for (int band = 0; band < 3; ++band)
    {
        const float d = bandPosition (band).getDistanceFrom (event.position);
        if (d < bestDistance)
        {
            bestDistance = d;
            nearest = band;
        }
    }

    if (nearest >= 0)
    {
        setActiveBand (nearest);
        draggedBand = nearest;
        freqAttachments[draggedBand]->beginGesture();
        gainAttachments[draggedBand]->beginGesture();
    }
}

void EqCurveDisplay::mouseDrag (const juce::MouseEvent& event)
{
    if (draggedBand < 0)
        return;

    const auto inner = plotArea().reduced (6.0f, 6.0f);
    const float norm = juce::jlimit (0.0f, 1.0f,
                                     (event.position.x - inner.getX()) / inner.getWidth());
    const float gain = juce::jlimit (-maxGainDb, maxGainDb,
                                     (inner.getCentreY() - event.position.y)
                                         / (inner.getHeight() * 0.5f) * maxGainDb);

    freqAttachments[draggedBand]->setValueAsPartOfGesture (normToFreq (norm, minFreq, maxFreq));
    gainAttachments[draggedBand]->setValueAsPartOfGesture (gain);
}

void EqCurveDisplay::mouseUp (const juce::MouseEvent&)
{
    if (draggedBand >= 0)
    {
        freqAttachments[draggedBand]->endGesture();
        gainAttachments[draggedBand]->endGesture();
        draggedBand = -1;
    }
}

void EqCurveDisplay::mouseWheelMove (const juce::MouseEvent&,
                                     const juce::MouseWheelDetails& wheel)
{
    const float factor = wheel.deltaY > 0 ? 1.12f : 1.0f / 1.12f;
    qAttachments[activeBand]->setValueAsCompleteGesture (
        juce::jlimit (minQ, maxQ, q[activeBand] * factor));
}

} // namespace kickforge::ui
