#pragma once

#include <cmath>

// Maths pures de l'UI (sans dépendance JUCE, testables unitairement) :
// axe de fréquence logarithmique et réponse en dB d'une bande peak RBJ
// (mêmes formules que EqStage) pour tracer la courbe d'EQ.
namespace kickforge::ui
{

inline float freqToNorm (float freqHz, float minHz, float maxHz)
{
    return std::log (freqHz / minHz) / std::log (maxHz / minHz);
}

inline float normToFreq (float norm, float minHz, float maxHz)
{
    return minHz * std::pow (maxHz / minHz, norm);
}

// Magnitude en dB d'un peak filter RBJ à la fréquence f (biquad évalué sur
// le cercle unité).
inline float peakBandDbAt (float freqHz, float centreHz, float gainDb, float q, double sampleRate)
{
    const double A     = std::pow (10.0, gainDb / 40.0);
    const double w0    = 2.0 * 3.14159265358979323846 * centreHz / sampleRate;
    const double cosW0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);

    const double a0 = 1.0 + alpha / A;
    const double b0 = (1.0 + alpha * A) / a0;
    const double b1 = (-2.0 * cosW0) / a0;
    const double b2 = (1.0 - alpha * A) / a0;
    const double a1 = (-2.0 * cosW0) / a0;
    const double a2 = (1.0 - alpha / A) / a0;

    const double w    = 2.0 * 3.14159265358979323846 * freqHz / sampleRate;
    const double cosW = std::cos (w), cos2W = std::cos (2.0 * w);

    const double num = b0 * b0 + b1 * b1 + b2 * b2
                       + 2.0 * (b0 * b1 + b1 * b2) * cosW + 2.0 * b0 * b2 * cos2W;
    const double den = 1.0 + a1 * a1 + a2 * a2
                       + 2.0 * (a1 + a1 * a2) * cosW + 2.0 * a2 * cos2W;

    return static_cast<float> (10.0 * std::log10 (num / den));
}

} // namespace kickforge::ui
