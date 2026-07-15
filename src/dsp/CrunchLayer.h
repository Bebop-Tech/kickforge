#pragma once

#include "DriveStage.h"

namespace kickforge
{

// Couche CRUNCH (v2) : la queue saturée qui tient.
// Oscillateur dédié (sin/tri) à fréquence FIXE = pitchEnd * 2^(tune/12)
// (pas de sweep : la queue est déjà posée), enveloppe attack lent + long
// decay (-60 dB au decay), poussé dans SA propre DriveStage (waveshaper +
// tone, oversampling x2). Le level s'applique POST-drive : il dose le mix
// sans changer le caractère de la saturation (c'est le rôle de amount).
//
// renderAdd() AJOUTE au buffer. Tout est pré-alloué dans prepare().
// Les paramètres d'oscillateur/enveloppe sont figés au trigger() ; ceux du
// drive sont lissés en continu par la DriveStage.
class CrunchLayer
{
public:
    enum class Waveform : int
    {
        sine = 0,
        triangle
    };

    struct Parameters
    {
        float levelPercent  = 0.0f;
        Waveform wave       = Waveform::sine;
        float tuneSemitones = 0.0f;
        float attackMs      = 30.0f;
        float decayMs       = 500.0f;
        float pitchEndHz    = 48.0f; // suit le pitchEnd du punch
        DriveStage::Parameters drive { DriveStage::Type::hard, 60.0f, 3000.0f };
    };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void setParameters (const Parameters& newParams);
    void trigger();
    void beginQuickRelease (int numSamples);

    bool isActive() const { return active; }
    void renderAdd (float* dest, int numSamples);

private:
    Parameters params;
    double sampleRate = 48000.0;
    int maxBlockSize  = 512;

    juce::AudioBuffer<float> scratch;
    DriveStage drive;

    bool active = false;
    Waveform wave = Waveform::sine;
    double phase = 0.0;
    double phaseInc = 0.0;
    double level = 0.0;

    double env = 0.0;
    double attackInc = 0.0;
    double decayMult = 0.0;
    int attackSamplesLeft = 0;
    double releaseStep = 0.0;
    bool releasing     = false;
};

} // namespace kickforge
