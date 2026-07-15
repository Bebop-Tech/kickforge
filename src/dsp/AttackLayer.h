#pragma once

#include <cstdint>

namespace kickforge
{

// Couche ATTACK (v2) : burst de bruit blanc déterministe -> low-pass one-pole
// (atkTone) -> décroissance exponentielle (atkDecay = temps pour perdre 60 dB).
// Généralisation du transitoire "punch" v1 : à réglages équivalents
// (tone 5 kHz, decay = 0.7 ms * ln(1000)), la sortie est identique.
//
// C++ pur, aucune allocation. Les paramètres sont figés au trigger().
// renderAdd() AJOUTE au buffer : les couches se mixent sur un bus.
class AttackLayer
{
public:
    struct Parameters
    {
        float levelPercent = 70.0f;
        float decayMs      = 3.0f;
        float toneHz       = 5000.0f;
    };

    void prepare (double newSampleRate);
    void setParameters (const Parameters& newParams);
    void trigger();
    void beginQuickRelease (int numSamples);

    bool isActive() const { return active; }
    void renderAdd (float* dest, int numSamples);

private:
    double sampleRate = 48000.0;
    Parameters params;

    bool active   = false;
    double env    = 0.0;
    double envMult = 0.0;
    double level  = 0.0;
    double lpState = 0.0;
    double lpCoeff = 0.0;
    uint32_t noiseState = 0x2545f491u;
    double releaseStep  = 0.0;
    bool releasing      = false;
};

} // namespace kickforge
