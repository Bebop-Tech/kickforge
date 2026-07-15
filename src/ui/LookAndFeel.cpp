#include "LookAndFeel.h"

namespace kickforge::ui
{

KickForgeLookAndFeel::KickForgeLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::windowBg);
    setColour (juce::Label::textColourId, colours::chipText);

    setColour (juce::TextButton::buttonColourId, colours::chip);
    setColour (juce::TextButton::buttonOnColourId, colours::accent);
    setColour (juce::TextButton::textColourOffId, colours::chipText);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);

    setColour (juce::Slider::thumbColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::section);
    setColour (juce::PopupMenu::textColourId, colours::value);
}

void KickForgeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width,
                                             int height, float sliderPos,
                                             float rotaryStartAngle, float rotaryEndAngle,
                                             juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.5f;
    const auto centre = bounds.getCentre();

    g.setColour (colours::knobFill);
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (colours::knobRim);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto tip = centre.getPointOnCircumference (radius * 0.82f, angle);

    juce::Path indicator;
    indicator.startNewSubPath (centre);
    indicator.lineTo (tip);

    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.strokePath (indicator, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
}

void KickForgeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour& backgroundColour,
                                                 bool highlighted, bool down)
{
    auto colour = backgroundColour;
    if (down)
        colour = colour.brighter (0.1f);
    else if (highlighted && ! button.getToggleState())
        colour = colour.brighter (0.06f);

    g.setColour (colour);
    g.fillRoundedRectangle (button.getLocalBounds().toFloat(), 4.0f);
}

juce::Font KickForgeLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::FontOptions (juce::jmin (11.0f, buttonHeight * 0.7f)));
}

} // namespace kickforge::ui
