#pragma once

namespace kickforge::presets
{

// Valeurs par genre (section 6 du brief v1 + section 5 de la spec v2).
// Charger un preset écrase TOUS les paramètres sauf `genre` et `outputGain`.
//
// Indices des choix : waveform/oscBWave {Sin 0, Tri 1, Sqr 2, Saw 3},
// crunchWave {Sin 0, Tri 1}, driveType/crunchDriveType {Soft 0, Hard 1, Fold 2},
// filterType {LP 0, HP 1, BP 2}, distType {Tube 0, Fuzz 1, Bit 2},
// clipType {Soft 0, Hard 1}.

struct ParamValue
{
    const char* id;
    float value;
};

inline constexpr int numGenres = 5;
inline constexpr int numPresetParams = 48;

struct GenrePreset
{
    const char* name;
    ParamValue values[numPresetParams];
};

// Randomisation "Random" : ±25 % de la plage (en espace normalisé 0..1)
// autour de la valeur du preset actif, bornée. unitRandom est un tirage
// uniforme dans [0, 1].
inline constexpr float randomizedNormalised (float presetNorm, float unitRandom)
{
    const float v = presetNorm + (unitRandom - 0.5f) * 0.5f;
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

inline constexpr GenrePreset genrePresets[numGenres] = {
    { "Techno",
      { { "waveform", 0.0f },     { "pitchStart", 180.0f }, { "pitchEnd", 50.0f },
        { "sweepTime", 35.0f },   { "oscBOn", 0.0f },       { "oscBWave", 2.0f },
        { "oscBTune", 12.0f },    { "oscBLevel", 25.0f },   { "attack", 1.0f },
        { "decay", 300.0f },
        { "atkLevel", 55.0f },    { "atkDecay", 2.0f },     { "atkTone", 4000.0f },
        { "punchLevel", 100.0f },
        { "crunchLevel", 25.0f }, { "crunchWave", 0.0f },   { "crunchTune", 0.0f },
        { "crunchAttack", 40.0f },{ "crunchDecay", 400.0f },{ "crunchDriveType", 0.0f },
        { "crunchDriveAmount", 40.0f }, { "crunchDriveTone", 2500.0f },
        { "driveType", 0.0f },
        { "driveAmount", 25.0f }, { "driveTone", 4000.0f }, { "filterType", 0.0f },
        { "filterCutoff", 20000.0f }, { "filterReso", 20.0f },
        { "eq1Freq", 60.0f },     { "eq1Gain", 2.0f },      { "eq1Q", 0.8f },
        { "eq2Freq", 800.0f },    { "eq2Gain", -1.0f },     { "eq2Q", 0.8f },
        { "eq3Freq", 6000.0f },   { "eq3Gain", 0.0f },      { "eq3Q", 0.8f },
        { "distType", 0.0f },     { "distAmount", 15.0f },  { "distMix", 30.0f },
        { "reverbMix", 8.0f },    { "chorusMix", 0.0f },    { "compAmount", 30.0f },
        { "compAttack", 10.0f },  { "clipType", 0.0f },     { "clipCeiling", -0.5f },
        { "width", 60.0f },       { "keyTrack", 0.0f } } },

    { "Hard Techno",
      { { "waveform", 0.0f },     { "pitchStart", 210.0f }, { "pitchEnd", 48.0f },
        { "sweepTime", 42.0f },   { "oscBOn", 1.0f },       { "oscBWave", 2.0f },
        { "oscBTune", 12.0f },    { "oscBLevel", 15.0f },   { "attack", 1.0f },
        { "decay", 340.0f },
        { "atkLevel", 70.0f },    { "atkDecay", 3.0f },     { "atkTone", 5000.0f },
        { "punchLevel", 100.0f },
        { "crunchLevel", 45.0f }, { "crunchWave", 0.0f },   { "crunchTune", 0.0f },
        { "crunchAttack", 30.0f },{ "crunchDecay", 500.0f },{ "crunchDriveType", 1.0f },
        { "crunchDriveAmount", 65.0f }, { "crunchDriveTone", 3000.0f },
        { "driveType", 1.0f },
        { "driveAmount", 65.0f }, { "driveTone", 2400.0f }, { "filterType", 0.0f },
        { "filterCutoff", 20000.0f }, { "filterReso", 20.0f },
        { "eq1Freq", 60.0f },     { "eq1Gain", 3.0f },      { "eq1Q", 0.8f },
        { "eq2Freq", 800.0f },    { "eq2Gain", -2.0f },     { "eq2Q", 0.8f },
        { "eq3Freq", 6000.0f },   { "eq3Gain", 1.5f },      { "eq3Q", 0.8f },
        { "distType", 2.0f },     { "distAmount", 25.0f },  { "distMix", 35.0f },
        { "reverbMix", 10.0f },   { "chorusMix", 0.0f },    { "compAmount", 45.0f },
        { "compAttack", 8.0f },   { "clipType", 1.0f },     { "clipCeiling", -0.3f },
        { "width", 70.0f },       { "keyTrack", 0.0f } } },

    { "Hardstyle",
      { { "waveform", 0.0f },     { "pitchStart", 320.0f }, { "pitchEnd", 55.0f },
        { "sweepTime", 70.0f },   { "oscBOn", 1.0f },       { "oscBWave", 2.0f },
        { "oscBTune", 12.0f },    { "oscBLevel", 30.0f },   { "attack", 0.5f },
        { "decay", 420.0f },
        { "atkLevel", 85.0f },    { "atkDecay", 4.0f },     { "atkTone", 6000.0f },
        { "punchLevel", 100.0f },
        { "crunchLevel", 80.0f }, { "crunchWave", 0.0f },   { "crunchTune", 0.0f },
        { "crunchAttack", 20.0f },{ "crunchDecay", 700.0f },{ "crunchDriveType", 1.0f },
        { "crunchDriveAmount", 95.0f }, { "crunchDriveTone", 3500.0f },
        { "driveType", 1.0f },
        { "driveAmount", 90.0f }, { "driveTone", 3500.0f }, { "filterType", 0.0f },
        { "filterCutoff", 12000.0f }, { "filterReso", 30.0f },
        { "eq1Freq", 60.0f },     { "eq1Gain", 2.0f },      { "eq1Q", 0.8f },
        { "eq2Freq", 800.0f },    { "eq2Gain", 0.0f },      { "eq2Q", 0.8f },
        { "eq3Freq", 6000.0f },   { "eq3Gain", 3.0f },      { "eq3Q", 0.8f },
        { "distType", 1.0f },     { "distAmount", 40.0f },  { "distMix", 50.0f },
        { "reverbMix", 5.0f },    { "chorusMix", 0.0f },    { "compAmount", 55.0f },
        { "compAttack", 5.0f },   { "clipType", 1.0f },     { "clipCeiling", -0.1f },
        { "width", 80.0f },       { "keyTrack", 0.0f } } },

    { "EDM",
      { { "waveform", 0.0f },     { "pitchStart", 160.0f }, { "pitchEnd", 45.0f },
        { "sweepTime", 25.0f },   { "oscBOn", 0.0f },       { "oscBWave", 2.0f },
        { "oscBTune", 12.0f },    { "oscBLevel", 25.0f },   { "attack", 1.0f },
        { "decay", 250.0f },
        { "atkLevel", 60.0f },    { "atkDecay", 2.0f },     { "atkTone", 5000.0f },
        { "punchLevel", 100.0f },
        { "crunchLevel", 10.0f }, { "crunchWave", 0.0f },   { "crunchTune", 0.0f },
        { "crunchAttack", 50.0f },{ "crunchDecay", 300.0f },{ "crunchDriveType", 0.0f },
        { "crunchDriveAmount", 25.0f }, { "crunchDriveTone", 4000.0f },
        { "driveType", 0.0f },
        { "driveAmount", 10.0f }, { "driveTone", 8000.0f }, { "filterType", 0.0f },
        { "filterCutoff", 20000.0f }, { "filterReso", 10.0f },
        { "eq1Freq", 60.0f },     { "eq1Gain", 3.0f },      { "eq1Q", 0.8f },
        { "eq2Freq", 800.0f },    { "eq2Gain", -3.0f },     { "eq2Q", 0.8f },
        { "eq3Freq", 6000.0f },   { "eq3Gain", 1.0f },      { "eq3Q", 0.8f },
        { "distType", 0.0f },     { "distAmount", 5.0f },   { "distMix", 20.0f },
        { "reverbMix", 12.0f },   { "chorusMix", 5.0f },    { "compAmount", 35.0f },
        { "compAttack", 12.0f },  { "clipType", 0.0f },     { "clipCeiling", -1.0f },
        { "width", 100.0f },      { "keyTrack", 0.0f } } },

    { "Trance",
      { { "waveform", 0.0f },     { "pitchStart", 150.0f }, { "pitchEnd", 50.0f },
        { "sweepTime", 30.0f },   { "oscBOn", 1.0f },       { "oscBWave", 1.0f },
        { "oscBTune", 0.0f },     { "oscBLevel", 20.0f },   { "attack", 2.0f },
        { "decay", 380.0f },
        { "atkLevel", 40.0f },    { "atkDecay", 2.0f },     { "atkTone", 4000.0f },
        { "punchLevel", 100.0f },
        { "crunchLevel", 15.0f }, { "crunchWave", 0.0f },   { "crunchTune", 0.0f },
        { "crunchAttack", 60.0f },{ "crunchDecay", 400.0f },{ "crunchDriveType", 0.0f },
        { "crunchDriveAmount", 20.0f }, { "crunchDriveTone", 3000.0f },
        { "driveType", 0.0f },
        { "driveAmount", 8.0f },  { "driveTone", 6000.0f }, { "filterType", 0.0f },
        { "filterCutoff", 16000.0f }, { "filterReso", 15.0f },
        { "eq1Freq", 60.0f },     { "eq1Gain", 2.0f },      { "eq1Q", 0.8f },
        { "eq2Freq", 800.0f },    { "eq2Gain", -1.0f },     { "eq2Q", 0.8f },
        { "eq3Freq", 6000.0f },   { "eq3Gain", 1.0f },      { "eq3Q", 0.8f },
        { "distType", 0.0f },     { "distAmount", 5.0f },   { "distMix", 15.0f },
        { "reverbMix", 20.0f },   { "chorusMix", 10.0f },   { "compAmount", 25.0f },
        { "compAttack", 15.0f },  { "clipType", 0.0f },     { "clipCeiling", -1.0f },
        { "width", 110.0f },      { "keyTrack", 0.0f } } },
};

} // namespace kickforge::presets
