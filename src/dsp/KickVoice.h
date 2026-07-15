#pragma once

#include <cstdint>

namespace kickforge
{

// Voix monophonique du kick.
//
// Osc A (sin / tri / carré / saw, PolyBLEP sur carré et saw) dont la fréquence
// suit une pitch envelope exponentielle f(t) = fEnd + (fStart - fEnd) * exp(-t / tau),
// avec tau = sweepTime. Osc B optionnel (mêmes formes d'onde) qui suit la même
// pitch envelope transposée de oscBTune demi-tons, mixé avant l'enveloppe
// d'amplitude (mix compensé en gain : (A + level*B) / (1 + level)).
// Amplitude : attack linéaire puis décroissance exponentielle (decay = temps
// pour perdre 60 dB). Punch : burst de bruit filtré (~2 ms) ajouté au tout
// début de la note, indépendant de l'attack.
//
// C++ pur, aucune allocation : utilisable directement sur le thread audio.
// Les paramètres sont figés au moment du trigger() (par note).
class KickVoice
{
public:
    enum class Waveform : int
    {
        sine = 0,
        triangle,
        square,
        saw
    };

    struct Parameters
    {
        Waveform waveform  = Waveform::sine;
        float pitchStartHz = 210.0f;
        float pitchEndHz   = 48.0f;
        float sweepTimeMs  = 42.0f;

        bool oscBOn              = false;
        Waveform oscBWave        = Waveform::square;
        float oscBTuneSemitones  = 12.0f;
        float oscBLevelPercent   = 25.0f;

        float attackMs     = 1.0f;
        float decayMs      = 340.0f;
    };

    void prepare (double newSampleRate);
    void setParameters (const Parameters& newParams);
    void trigger();

    // Coupe la note en rampe linéaire sur ~n échantillons (retrigger sans
    // clic : l'enveloppe meurt AVANT le drive, jamais après).
    void beginQuickRelease (int numSamples);

    bool isActive() const { return active; }
    float currentFrequencyHz() const;
    float currentEnvelope() const { return static_cast<float> (ampEnv); }

    float renderNextSample();
    void render (float* dest, int numSamples);

private:
    template <bool withOscB>
    void renderLoop (float* dest, int numSamples);

    float advanceEnvelopes (double oscMix);

    double sampleRate = 48000.0;
    Parameters params;

    bool active = false;

    // Oscillateurs (phase normalisée dans [0, 1))
    Waveform waveA = Waveform::sine;
    Waveform waveB = Waveform::square;
    double phaseA  = 0.0;
    double phaseB  = 0.0;
    bool oscBActive    = false;
    double oscBRatio   = 1.0; // 2^(tune/12)
    double oscBLevel   = 0.0;
    double mixScale    = 1.0; // 1 / (1 + level) quand l'Osc B est actif

    // Pitch envelope : f = end + (start - end) * pitchState, pitchState : 1 -> 0
    double pitchState = 0.0;
    double pitchMult  = 0.0;
    float  pitchStart = 0.0f;
    float  pitchEnd   = 0.0f;

    // Amp envelope : rampe linéaire d'attack puis décroissance exponentielle
    double ampEnv         = 0.0;
    double attackInc      = 0.0;
    double decayMult      = 0.0;
    int attackSamplesLeft = 0;
    double releaseStep    = 0.0;
    bool releasing        = false;

};

} // namespace kickforge
