#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kickforge::ui
{

// Palette de la maquette v3
namespace colours
{
    inline const juce::Colour windowBg  { 0xff17171b };
    inline const juce::Colour section   { 0xff1e1e24 };
    inline const juce::Colour border    { 0xff2c2c32 };
    inline const juce::Colour inset     { 0xff101014 };
    inline const juce::Colour insetLine { 0xff26262c };
    inline const juce::Colour chip      { 0xff2a2a31 };
    inline const juce::Colour chipText  { 0xff8f8f99 };
    inline const juce::Colour accent    { 0xffd85a30 };
    inline const juce::Colour knobFill  { 0xff26262c };
    inline const juce::Colour knobRim   { 0xff3a3a42 };
    inline const juce::Colour value     { 0xffe6e3dc };
    inline const juce::Colour title     { 0xfff0ede6 };
    inline const juce::Colour muted     { 0xff6f6f78 };
    inline const juce::Colour faint     { 0xff55555e };
    inline const juce::Colour onBadgeBg { 0xff3a5c3a };

    // Couleurs d'indicateur par section
    inline const juce::Colour osc    { 0xffe8935f };
    inline const juce::Colour env    { 0xff5dcaa5 };
    inline const juce::Colour drive  { 0xfff09595 };
    inline const juce::Colour filter { 0xffafa9ec };
    inline const juce::Colour fx     { 0xff85b7eb };
    inline const juce::Colour comp   { 0xffefb75a };
    inline const juce::Colour width  { 0xff9fe1cb };
} // namespace colours

// Knobs plats à trait indicateur coloré, chips rectangulaires arrondies.
class KickForgeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KickForgeLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

} // namespace kickforge::ui
