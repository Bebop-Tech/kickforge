#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "dsp/KickVoice.h"
#include "dsp/KickEngine.h"
#include "dsp/DriveStage.h"
#include "dsp/FilterStage.h"
#include "dsp/EqStage.h"
#include "dsp/DistStage.h"
#include "dsp/FxStage.h"
#include "dsp/CompStage.h"
#include "dsp/ClipStage.h"
#include "dsp/WidthStage.h"

#include <array>
#include <cmath>
#include <vector>

using Catch::Approx;
using kickforge::DriveStage;
using kickforge::FilterStage;
using kickforge::KickEngine;
using kickforge::KickVoice;

namespace
{
constexpr double kSampleRate = 48000.0;

// Paramètres pour observer la forme d'onde brute : pitch constant, attack
// quasi nulle, decay très long, pas de punch.
KickVoice::Parameters steadyToneParams (float freqHz, KickVoice::Waveform wave)
{
    KickVoice::Parameters p;
    p.waveform     = wave;
    p.pitchStartHz = freqHz;
    p.pitchEndHz   = freqHz;
    p.attackMs     = 0.1f;
    p.decayMs      = 2000.0f;
    return p;
}

// Amplitude relative d'une composante via Goertzel fenêtré (Hann).
// Non normalisée : à n'utiliser qu'en comparaison relative.
double goertzelMag (const std::vector<float>& x, double freqHz, double sampleRate)
{
    const auto n = x.size();
    const double w     = 2.0 * M_PI * freqHz / sampleRate;
    const double coeff = 2.0 * std::cos (w);

    double s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double hann = 0.5 * (1.0 - std::cos (2.0 * M_PI * static_cast<double> (i)
                                                   / static_cast<double> (n - 1)));
        const double s0 = x[i] * hann + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2));
}

float rmsOf (const std::vector<float>& x, size_t begin, size_t end)
{
    double acc = 0.0;
    for (size_t i = begin; i < end; ++i)
        acc += static_cast<double> (x[i]) * x[i];
    return static_cast<float> (std::sqrt (acc / static_cast<double> (end - begin)));
}

std::vector<float> renderSamples (KickVoice& voice, int numSamples)
{
    std::vector<float> out (static_cast<size_t> (numSamples), 0.0f);
    voice.render (out.data(), numSamples);
    return out;
}

std::vector<float> renderSamples (KickEngine& engine, int numSamples)
{
    std::vector<float> busAP (static_cast<size_t> (numSamples), 0.0f);
    std::vector<float> mix (static_cast<size_t> (numSamples), 0.0f);
    engine.render (busAP.data(), mix.data(), numSamples);
    return mix;
}

std::array<std::vector<float>, 2> renderBuses (KickEngine& engine, int numSamples)
{
    std::vector<float> busAP (static_cast<size_t> (numSamples), 0.0f);
    std::vector<float> mix (static_cast<size_t> (numSamples), 0.0f);
    engine.render (busAP.data(), mix.data(), numSamples);
    return { busAP, mix };
}

// Moteur "neutre" : pas d'attack, pas de crunch, drive du punch transparent
KickEngine::Parameters neutralEngineParams()
{
    KickEngine::Parameters p;
    p.attack.levelPercent = 0.0f;
    p.crunch.levelPercent = 0.0f;
    p.punchDrive = { kickforge::DriveStage::Type::hard, 0.0f, 16000.0f };
    return p;
}
} // namespace

TEST_CASE ("KickVoice est silencieuse avant tout déclenchement", "[KickVoice]")
{
    KickVoice voice;
    voice.prepare (kSampleRate);

    REQUIRE_FALSE (voice.isActive());

    for (float s : renderSamples (voice, 512))
        REQUIRE (s == 0.0f);
}

TEST_CASE ("KickVoice produit du signal après trigger", "[KickVoice]")
{
    KickVoice voice;
    voice.prepare (kSampleRate);
    voice.trigger();

    REQUIRE (voice.isActive());

    float peak = 0.0f;
    for (float s : renderSamples (voice, 512))
        peak = std::max (peak, std::abs (s));

    REQUIRE (peak > 0.1f);
}

TEST_CASE ("la pitch envelope décroît exponentiellement de pitchStart vers pitchEnd", "[KickVoice]")
{
    KickVoice voice;
    voice.prepare (kSampleRate);

    KickVoice::Parameters params;
    params.pitchStartHz = 210.0f;
    params.pitchEndHz   = 48.0f;
    params.sweepTimeMs  = 42.0f;
    params.decayMs      = 2000.0f; // decay long : la voix reste active pendant tout le sweep
    voice.setParameters (params);
    voice.trigger();

    REQUIRE (voice.currentFrequencyHz() == Approx (210.0f).margin (1.0f));

    // À t = tau (= sweepTime), f = fEnd + (fStart - fEnd)/e ≈ 107.6 Hz
    const int tauSamples = static_cast<int> (0.042 * kSampleRate);
    renderSamples (voice, tauSamples);
    const float expectedAtTau = 48.0f + (210.0f - 48.0f) * std::exp (-1.0f);
    REQUIRE (voice.currentFrequencyHz() == Approx (expectedAtTau).margin (2.0f));

    // À t = 10 tau, la fréquence a convergé vers pitchEnd
    renderSamples (voice, 9 * tauSamples);
    REQUIRE (voice.currentFrequencyHz() == Approx (48.0f).margin (0.5f));
}

TEST_CASE ("l'enveloppe d'amplitude monte pendant l'attack puis perd 60 dB au temps de decay", "[KickVoice]")
{
    KickVoice voice;
    voice.prepare (kSampleRate);

    KickVoice::Parameters params;
    params.attackMs = 1.0f;
    params.decayMs  = 340.0f;
    voice.setParameters (params);
    voice.trigger();

    // Fin de l'attack : enveloppe proche de 1
    const int attackSamples = static_cast<int> (0.001 * kSampleRate);
    renderSamples (voice, attackSamples + 1);
    REQUIRE (voice.currentEnvelope() == Approx (1.0f).margin (0.05f));

    // Décroissance monotone après l'attack
    float previous = voice.currentEnvelope();
    for (int i = 0; i < 100; ++i)
    {
        renderSamples (voice, 32);
        REQUIRE (voice.currentEnvelope() <= previous);
        previous = voice.currentEnvelope();
    }

    // À t = decay après l'attack : -60 dB (facteur 0.001), tolérance large
    KickVoice voice2;
    voice2.prepare (kSampleRate);
    voice2.setParameters (params);
    voice2.trigger();
    renderSamples (voice2, attackSamples + static_cast<int> (0.340 * kSampleRate));
    REQUIRE (voice2.currentEnvelope() > 0.0004f);
    REQUIRE (voice2.currentEnvelope() < 0.0025f);
}

TEST_CASE ("la voix se désactive une fois l'enveloppe éteinte", "[KickVoice]")
{
    KickVoice voice;
    voice.prepare (kSampleRate);

    KickVoice::Parameters params;
    params.decayMs = 100.0f;
    voice.setParameters (params);
    voice.trigger();

    // 4x le decay : l'enveloppe est bien sous le seuil d'extinction
    renderSamples (voice, static_cast<int> (0.4 * kSampleRate));
    REQUIRE_FALSE (voice.isActive());

    for (float s : renderSamples (voice, 256))
        REQUIRE (s == 0.0f);
}

TEST_CASE ("la sortie reste finie et bornée avec des paramètres extrêmes", "[KickVoice]")
{
    KickVoice voice;
    voice.prepare (kSampleRate);

    KickVoice::Parameters params;
    params.waveform     = KickVoice::Waveform::saw;
    params.pitchStartHz = 2000.0f;
    params.pitchEndHz   = 25.0f;
    params.sweepTimeMs  = 5.0f;
    params.attackMs     = 0.1f;
    params.decayMs      = 50.0f;
    params.oscBOn       = true;
    params.oscBWave     = KickVoice::Waveform::square;
    params.oscBTuneSemitones = 24.0f;
    params.oscBLevelPercent  = 100.0f;
    voice.setParameters (params);
    voice.trigger();

    for (float s : renderSamples (voice, static_cast<int> (kSampleRate)))
    {
        REQUIRE (std::isfinite (s));
        REQUIRE (std::abs (s) <= 1.5f);
    }
}

TEST_CASE ("le carré est plat, le saw est une rampe, le triangle a une pente constante", "[KickVoice][waveform]")
{
    // Fenêtre d'observation : 2 périodes à 100 Hz, après stabilisation.
    constexpr int skip = 480, window = 960;

    auto renderShape = [] (KickVoice::Waveform wave)
    {
        KickVoice voice;
        voice.prepare (kSampleRate);
        voice.setParameters (steadyToneParams (100.0f, wave));
        voice.trigger();
        auto all = renderSamples (voice, skip + window);
        return std::vector<float> (all.begin() + skip, all.end());
    };

    auto flatness = [] (const std::vector<float>& x)
    {
        float peak = 0.0f;
        for (float s : x)
            peak = std::max (peak, std::abs (s));
        int count = 0;
        for (float s : x)
            if (std::abs (s) > 0.8f * peak)
                ++count;
        return static_cast<float> (count) / static_cast<float> (x.size());
    };

    auto slopeFlatness = [] (const std::vector<float>& x)
    {
        float maxSlope = 0.0f;
        for (size_t i = 1; i < x.size(); ++i)
            maxSlope = std::max (maxSlope, std::abs (x[i] - x[i - 1]));
        int count = 0;
        for (size_t i = 1; i < x.size(); ++i)
            if (std::abs (x[i] - x[i - 1]) > 0.6f * maxSlope)
                ++count;
        return static_cast<float> (count) / static_cast<float> (x.size() - 1);
    };

    // Carré : quasi tout le temps proche de +/- pic
    REQUIRE (flatness (renderShape (KickVoice::Waveform::square)) > 0.8f);

    // Sinus : nettement moins plat qu'un carré
    REQUIRE (flatness (renderShape (KickVoice::Waveform::sine)) < 0.6f);

    // Saw : montée quasi permanente (une seule chute par période)
    const auto saw = renderShape (KickVoice::Waveform::saw);
    int ascending = 0;
    for (size_t i = 1; i < saw.size(); ++i)
        if (saw[i] > saw[i - 1])
            ++ascending;
    REQUIRE (static_cast<float> (ascending) / static_cast<float> (saw.size() - 1) > 0.9f);

    // Triangle : pente constante au signe près (le sinus est bien en dessous)
    REQUIRE (slopeFlatness (renderShape (KickVoice::Waveform::triangle)) > 0.85f);
    REQUIRE (slopeFlatness (renderShape (KickVoice::Waveform::sine)) < 0.7f);
}

TEST_CASE ("le saw PolyBLEP ne laisse pas d'alias mesurable à pitchStart max", "[KickVoice][polyblep]")
{
    // Saw tenu à 1900 Hz : les harmoniques 14/15 (26.6/28.5 kHz) replient à
    // 21.4/19.5 kHz, l'harmonique 20 (38 kHz) replie en plein aigu audible à
    // 10 kHz. Sans PolyBLEP ces alias sortent entre 5 et 7 % du fondamental
    // (~-24 dB) ; un PolyBLEP 2 points doit les tenir sous 3 % (-30 dB).
    KickVoice voice;
    voice.prepare (kSampleRate);
    voice.setParameters (steadyToneParams (1900.0f, KickVoice::Waveform::saw));
    voice.trigger();

    renderSamples (voice, 480);
    const auto x = renderSamples (voice, 8192);

    const double fundamental = goertzelMag (x, 1900.0, kSampleRate);
    REQUIRE (fundamental > 0.0);
    REQUIRE (goertzelMag (x, 19500.0, kSampleRate) < 0.03 * fundamental);
    REQUIRE (goertzelMag (x, 21400.0, kSampleRate) < 0.03 * fundamental);
    REQUIRE (goertzelMag (x, 10000.0, kSampleRate) < 0.03 * fundamental);
}

TEST_CASE ("l'Osc B à +12 st enrichit l'octave sans détruire le fondamental", "[KickVoice][oscB]")
{
    auto renderTone = [] (bool oscBOn)
    {
        KickVoice voice;
        voice.prepare (kSampleRate);
        auto p = steadyToneParams (100.0f, KickVoice::Waveform::sine);
        p.oscBOn            = oscBOn;
        p.oscBWave          = KickVoice::Waveform::sine;
        p.oscBTuneSemitones = 12.0f;
        p.oscBLevelPercent  = 50.0f;
        voice.setParameters (p);
        voice.trigger();
        renderSamples (voice, 480);
        return renderSamples (voice, 4800);
    };

    const auto without = renderTone (false);
    const auto with    = renderTone (true);

    const double fundWithout = goertzelMag (without, 100.0, kSampleRate);
    const double fundWith    = goertzelMag (with, 100.0, kSampleRate);
    const double octaveWith  = goertzelMag (with, 200.0, kSampleRate);

    // L'octave apparaît nettement...
    REQUIRE (octaveWith > 0.2 * fundWith);
    REQUIRE (octaveWith > 5.0 * goertzelMag (without, 200.0, kSampleRate));

    // ... et le fondamental (le sub) reste largement présent
    REQUIRE (fundWith > 0.6 * fundWithout);
}

TEST_CASE ("KickEngine déclenche la voix sur noteOn", "[KickEngine]")
{
    KickEngine engine;
    engine.prepare ({ kSampleRate, 512, 1 });

    for (float s : renderSamples (engine, 256))
        REQUIRE (s == 0.0f);

    engine.noteOn();
    float peak = 0.0f;
    for (float s : renderSamples (engine, 512))
        peak = std::max (peak, std::abs (s));

    REQUIRE (peak > 0.1f);
}

TEST_CASE ("le retrigger coupe la note avec un fade court, sans clic", "[KickEngine]")
{
    KickEngine engine;
    engine.prepare ({ kSampleRate, 512, 1 });

    // Transitoire coupé et drive transparent : ce test ne vise que la
    // discontinuité de coupure de la note précédente.
    engine.setParameters (neutralEngineParams());
    engine.reset();

    engine.noteOn();
    auto before = renderSamples (engine, 4800); // 100 ms dans la note

    engine.noteOn(); // retrigger pendant que la voix est active
    auto after = renderSamples (engine, 4800);

    // Pas de discontinuité : le saut entre échantillons consécutifs reste
    // borné par ce qu'un sinus grave + fade de 2 ms peut produire.
    float maxJump  = 0.0f;
    float previous = before.back();
    for (float s : after)
    {
        maxJump  = std::max (maxJump, std::abs (s - previous));
        previous = s;
    }
    REQUIRE (maxJump < 0.1f);
}

TEST_CASE ("le retrigger relance bien un nouveau kick", "[KickEngine]")
{
    KickEngine engine;
    engine.prepare ({ kSampleRate, 512, 1 });

    auto engineParams = neutralEngineParams();
    engineParams.punch.decayMs = 200.0f;
    engine.setParameters (engineParams);

    engine.noteOn();
    renderSamples (engine, static_cast<int> (0.150 * kSampleRate)); // enveloppe bien entamée

    engine.noteOn();
    // Après le fade (~2 ms) + une nouvelle attack, l'enveloppe doit être repartie fort
    renderSamples (engine, static_cast<int> (0.010 * kSampleRate));

    float peak = 0.0f;
    for (float s : renderSamples (engine, 2048))
        peak = std::max (peak, std::abs (s));

    REQUIRE (engine.isActive());
    REQUIRE (peak > 0.5f);
}

// ---------------------------------------------------------------------------
// Étape 4 : DriveStage et FilterStage
// ---------------------------------------------------------------------------

namespace
{
std::vector<float> makeSine (double freqHz, float amplitude, int numSamples)
{
    std::vector<float> out (static_cast<size_t> (numSamples));
    for (int i = 0; i < numSamples; ++i)
        out[static_cast<size_t> (i)] =
            amplitude * static_cast<float> (std::sin (2.0 * M_PI * freqHz * i / kSampleRate));
    return out;
}

// Passe un signal dans un étage par blocs de 512, après un warm-up qui laisse
// le smoothing des paramètres converger.
template <typename Stage>
std::vector<float> processThrough (Stage& stage, std::vector<float> signal)
{
    constexpr int block = 512;

    auto warmup = signal; // même contenu : les smoothers convergent sur du signal réel
    for (auto* data : { &warmup, &signal })
        for (size_t pos = 0; pos < data->size(); pos += block)
        {
            const auto n = std::min (static_cast<size_t> (block), data->size() - pos);
            float* channels[] = { data->data() + pos };
            juce::dsp::AudioBlock<float> monoBlock (channels, 1, n);
            stage.process (monoBlock);
        }
    return signal;
}

// (les étages juce::dsp ne sont ni copiables ni déplaçables : passage par référence)
template <typename Stage>
void prepareStage (Stage& stage, const typename Stage::Parameters& params,
                   juce::uint32 numChannels = 1)
{
    stage.prepare ({ kSampleRate, 512, numChannels });
    stage.setParameters (params);
    stage.reset();
}
}

TEST_CASE ("le drive hard à 0 % est quasi transparent", "[DriveStage]")
{
    DriveStage stage;
    prepareStage (stage, { DriveStage::Type::hard, 0.0f, 16000.0f });

    const auto in  = makeSine (1000.0, 0.5f, 8192);
    const auto out = processThrough (stage, in);

    REQUIRE (rmsOf (out, 512, out.size()) == Approx (rmsOf (in, 512, in.size())).epsilon (0.1));
    REQUIRE (goertzelMag (out, 3000.0, kSampleRate) < 0.02 * goertzelMag (out, 1000.0, kSampleRate));
}

TEST_CASE ("le drive soft à 100 % sature sans dépasser le plein niveau", "[DriveStage]")
{
    DriveStage stage;
    prepareStage (stage, { DriveStage::Type::soft, 100.0f, 16000.0f });

    const auto out = processThrough (stage, makeSine (1000.0, 1.0f, 8192));

    // Borné à +/-1 à 96 kHz ; le downsampling bande-limite le signal et le
    // ringing de Gibbs peut faire dépasser les crêtes (~1.3 max). Le plafond
    // strict est le rôle du clipper final (étape 6).
    float peak = 0.0f;
    for (float s : out)
        peak = std::max (peak, std::abs (s));
    REQUIRE (peak <= 1.35f);

    // Harmoniques impaires bien présentes
    REQUIRE (goertzelMag (out, 3000.0, kSampleRate) > 0.1 * goertzelMag (out, 1000.0, kSampleRate));
}

TEST_CASE ("le drive hard à 100 % écrase le sinus en quasi-carré", "[DriveStage]")
{
    DriveStage stage;
    prepareStage (stage, { DriveStage::Type::hard, 100.0f, 16000.0f });

    const auto out = processThrough (stage, makeSine (1000.0, 1.0f, 8192));

    float peak = 0.0f;
    for (float s : out)
        peak = std::max (peak, std::abs (s));

    // Seuil à 0.7 x pic : le pic est gonflé par le ringing de Gibbs alors que
    // le plateau du carré reste à ~1.0.
    int flat = 0;
    for (float s : out)
        if (std::abs (s) > 0.7f * peak)
            ++flat;
    REQUIRE (static_cast<float> (flat) / static_cast<float> (out.size()) > 0.7f);
}

TEST_CASE ("le wavefolder replie le signal sans dépasser le plein niveau", "[DriveStage]")
{
    DriveStage stage;
    prepareStage (stage, { DriveStage::Type::fold, 80.0f, 16000.0f });

    const auto out = processThrough (stage, makeSine (1000.0, 1.0f, 8192));

    float peak = 0.0f;
    for (float s : out)
        peak = std::max (peak, std::abs (s));
    REQUIRE (peak <= 1.35f); // borné à 96 kHz, ringing de Gibbs toléré
    REQUIRE (rmsOf (out, 512, out.size()) > 0.2f);

    // Le repliement crée un contenu harmonique riche
    const double fund = goertzelMag (out, 1000.0, kSampleRate);
    REQUIRE (goertzelMag (out, 3000.0, kSampleRate) + goertzelMag (out, 5000.0, kSampleRate) > 0.1 * fund);
}

TEST_CASE ("le tone low-pass adoucit les harmoniques du drive", "[DriveStage]")
{
    const auto in = makeSine (500.0, 1.0f, 16384);

    DriveStage bright;
    prepareStage (bright, { DriveStage::Type::hard, 100.0f, 16000.0f });
    DriveStage dark;
    prepareStage (dark, { DriveStage::Type::hard, 100.0f, 500.0f });

    const auto outBright = processThrough (bright, in);
    const auto outDark   = processThrough (dark, in);

    // Harmonique 9 (4.5 kHz) : très atténuée avec tone à 500 Hz
    const double h9Bright = goertzelMag (outBright, 4500.0, kSampleRate);
    const double h9Dark   = goertzelMag (outDark, 4500.0, kSampleRate);
    REQUIRE (h9Dark < 0.15 * h9Bright);

    // Le fondamental (500 Hz = cutoff) reste substantiel
    REQUIRE (goertzelMag (outDark, 500.0, kSampleRate) > 0.3 * goertzelMag (outBright, 500.0, kSampleRate));
}

TEST_CASE ("le filtre LP à 20 kHz est neutre", "[FilterStage]")
{
    FilterStage stage;
    prepareStage (stage, { FilterStage::Type::lowpass, 20000.0f, 20.0f });

    const auto in  = makeSine (100.0, 0.5f, 8192);
    const auto out = processThrough (stage, in);

    REQUIRE (rmsOf (out, 512, out.size()) == Approx (rmsOf (in, 512, in.size())).epsilon (0.05));
}

TEST_CASE ("le filtre LP coupe le haut et laisse passer le bas", "[FilterStage]")
{
    FilterStage stage;
    prepareStage (stage, { FilterStage::Type::lowpass, 200.0f, 0.0f });

    const auto high = processThrough (stage, makeSine (5000.0, 0.5f, 8192));
    REQUIRE (rmsOf (high, 512, high.size()) < 0.05f * 0.353f); // >26 dB d'atténuation

    stage.reset();
    const auto low = processThrough (stage, makeSine (50.0, 0.5f, 8192));
    REQUIRE (rmsOf (low, 512, low.size()) > 0.85f * 0.353f);
}

TEST_CASE ("le filtre HP coupe le bas et laisse passer le haut", "[FilterStage]")
{
    FilterStage stage;
    prepareStage (stage, { FilterStage::Type::highpass, 1000.0f, 0.0f });

    const auto low = processThrough (stage, makeSine (100.0, 0.5f, 8192));
    REQUIRE (rmsOf (low, 512, low.size()) < 0.05f * 0.353f);

    stage.reset();
    const auto high = processThrough (stage, makeSine (8000.0, 0.5f, 8192));
    REQUIRE (rmsOf (high, 512, high.size()) > 0.85f * 0.353f);
}

TEST_CASE ("le filtre BP laisse passer le centre et atténue les côtés", "[FilterStage]")
{
    FilterStage stage;
    prepareStage (stage, { FilterStage::Type::bandpass, 1000.0f, 20.0f });

    const auto centre = processThrough (stage, makeSine (1000.0, 0.5f, 8192));
    REQUIRE (rmsOf (centre, 512, centre.size()) > 0.6f * 0.353f);

    stage.reset();
    const auto low = processThrough (stage, makeSine (50.0, 0.5f, 8192));
    REQUIRE (rmsOf (low, 512, low.size()) < 0.2f * 0.353f);

    stage.reset();
    const auto high = processThrough (stage, makeSine (12000.0, 0.5f, 8192));
    REQUIRE (rmsOf (high, 512, high.size()) < 0.2f * 0.353f);
}

TEST_CASE ("la résonance crée un pic au cutoff mais reste stable", "[FilterStage]")
{
    FilterStage flat;
    prepareStage (flat, { FilterStage::Type::lowpass, 1000.0f, 0.0f });
    FilterStage peaky;
    prepareStage (peaky, { FilterStage::Type::lowpass, 1000.0f, 90.0f });

    const auto in = makeSine (1000.0, 0.1f, 8192);
    const auto outFlat  = processThrough (flat, in);
    const auto outPeaky = processThrough (peaky, in);

    REQUIRE (rmsOf (outPeaky, 2048, outPeaky.size()) > 1.5f * rmsOf (outFlat, 2048, outFlat.size()));

    // Stabilité à résonance max : bruit large bande, sortie finie et bornée
    KickVoice noiseSource;
    noiseSource.prepare (kSampleRate);
    noiseSource.setParameters (steadyToneParams (55.0f, KickVoice::Waveform::saw));
    noiseSource.trigger();
    std::vector<float> noise (48000);
    noiseSource.render (noise.data(), static_cast<int> (noise.size()));

    peaky.reset();
    const auto outNoise = processThrough (peaky, noise);
    for (float s : outNoise)
    {
        REQUIRE (std::isfinite (s));
        REQUIRE (std::abs (s) <= 10.0f);
    }
}

// ---------------------------------------------------------------------------
// Étape 5 : EqStage, DistStage, FxStage
// ---------------------------------------------------------------------------

namespace
{
// Rapport de niveau d'une sinusoïde après passage dans un étage mono.
template <typename Stage>
float toneRatio (Stage& stage, double freqHz)
{
    stage.reset();
    const auto in  = makeSine (freqHz, 0.25f, 8192);
    const auto out = processThrough (stage, in);
    return rmsOf (out, 1024, out.size()) / rmsOf (in, 1024, in.size());
}

std::array<std::vector<float>, 2> processThroughFx (kickforge::FxStage& fx,
                                                    const std::vector<float>& mainMono,
                                                    const std::vector<float>& busAP)
{
    constexpr size_t block = 512;

    // Warm-up : les gains internes de juce::Reverb sont lissés sans snap
    auto warmLeft = mainMono, warmRight = mainMono;
    std::vector<float> left = mainMono, right = mainMono;

    for (auto [l, r] : { std::pair { &warmLeft, &warmRight }, std::pair { &left, &right } })
        for (size_t pos = 0; pos < mainMono.size(); pos += block)
        {
            const auto n = std::min (block, mainMono.size() - pos);
            float* channels[] = { l->data() + pos, r->data() + pos };
            juce::dsp::AudioBlock<float> stereoBlock (channels, 2, n);
            fx.process (stereoBlock, busAP.data() + pos, n);
        }
    return { left, right };
}

double correlation (const std::vector<float>& a, const std::vector<float>& b,
                    size_t begin, size_t end)
{
    double dotAB = 0.0, dotAA = 0.0, dotBB = 0.0;
    for (size_t i = begin; i < end; ++i)
    {
        dotAB += static_cast<double> (a[i]) * b[i];
        dotAA += static_cast<double> (a[i]) * a[i];
        dotBB += static_cast<double> (b[i]) * b[i];
    }
    return dotAB / std::sqrt (dotAA * dotBB + 1.0e-12);
}
} // namespace

TEST_CASE ("l'EQ à gains nuls est neutre", "[EqStage]")
{
    kickforge::EqStage eq;
    kickforge::EqStage::Parameters p; // défauts du brief...
    for (auto& band : p.bands)
        band.gainDb = 0.0f;           // ... mais tous les gains à 0
    prepareStage (eq, p);

    for (double freq : { 60.0, 800.0, 6000.0 })
        REQUIRE (toneRatio (eq, freq) == Approx (1.0f).epsilon (0.03));
}

TEST_CASE ("la bande 1 booste sa fréquence sans toucher les autres", "[EqStage]")
{
    kickforge::EqStage eq;
    kickforge::EqStage::Parameters p;
    for (auto& band : p.bands)
        band.gainDb = 0.0f;
    p.bands[0] = { 60.0f, 12.0f, 0.8f };
    prepareStage (eq, p);

    REQUIRE (toneRatio (eq, 60.0) == Approx (3.98f).epsilon (0.15));  // +12 dB
    REQUIRE (toneRatio (eq, 800.0) == Approx (1.0f).epsilon (0.12));  // hors bande
}

TEST_CASE ("la bande 2 creuse sa fréquence", "[EqStage]")
{
    kickforge::EqStage eq;
    kickforge::EqStage::Parameters p;
    for (auto& band : p.bands)
        band.gainDb = 0.0f;
    p.bands[1] = { 800.0f, -12.0f, 0.8f };
    prepareStage (eq, p);

    REQUIRE (toneRatio (eq, 800.0) == Approx (0.251f).epsilon (0.15)); // -12 dB
}

TEST_CASE ("le Q contrôle la largeur de la bande", "[EqStage]")
{
    auto ratioAt500 = [] (float q)
    {
        kickforge::EqStage eq;
        kickforge::EqStage::Parameters p;
        for (auto& band : p.bands)
            band.gainDb = 0.0f;
        p.bands[1] = { 1000.0f, 12.0f, q };
        prepareStage (eq, p);
        return toneRatio (eq, 500.0);
    };

    REQUIRE (ratioAt500 (0.3f) > 1.5f);  // bande large : 500 Hz nettement boosté
    REQUIRE (ratioAt500 (6.0f) < 1.15f); // bande étroite : 500 Hz quasi intact
}

TEST_CASE ("la distorsion à mix 0 est transparente", "[DistStage]")
{
    for (auto type : { kickforge::DistStage::Type::tube,
                       kickforge::DistStage::Type::fuzz,
                       kickforge::DistStage::Type::bitcrush })
    {
        kickforge::DistStage dist;
        prepareStage (dist, { type, 100.0f, 0.0f });

        const auto in  = makeSine (200.0, 0.5f, 4096);
        const auto out = processThrough (dist, in);

        for (size_t i = 0; i < in.size(); ++i)
            REQUIRE (out[i] == Approx (in[i]).margin (1.0e-5));
    }
}

TEST_CASE ("le tube crée des harmoniques paires (asymétrie)", "[DistStage]")
{
    kickforge::DistStage dist;
    prepareStage (dist, { kickforge::DistStage::Type::tube, 80.0f, 100.0f });

    const auto out = processThrough (dist, makeSine (200.0, 0.5f, 8192));

    const double h1 = goertzelMag (out, 200.0, kSampleRate);
    const double h2 = goertzelMag (out, 400.0, kSampleRate);
    REQUIRE (h2 > 0.02 * h1);
}

TEST_CASE ("le fuzz clippe fort et allège le bas de sa branche wet", "[DistStage]")
{
    kickforge::DistStage dist;
    prepareStage (dist, { kickforge::DistStage::Type::fuzz, 100.0f, 100.0f });

    const auto out = processThrough (dist, makeSine (200.0, 0.5f, 8192));

    float peak = 0.0f;
    for (float s : out)
        peak = std::max (peak, std::abs (s));
    REQUIRE (peak <= 1.1f);

    // Signature d'un hard clip : harmoniques impaires fortes (h3 = 1/3 du
    // fondamental pour un carré idéal ; le HPF léger réduit un peu le tout)
    const double h1 = goertzelMag (out, 200.0, kSampleRate);
    const double h3 = goertzelMag (out, 600.0, kSampleRate);
    REQUIRE (h3 > 0.15 * h1);

    // Le HPF léger (100 Hz) atténue le bas dans la branche wet
    dist.reset();
    const auto low = processThrough (dist, makeSine (50.0, 0.5f, 8192));
    REQUIRE (rmsOf (low, 1024, low.size()) < 0.6f * rmsOf (out, 1024, out.size()));
}

TEST_CASE ("le bitcrush réduit la résolution temporelle", "[DistStage]")
{
    kickforge::DistStage dist;
    prepareStage (dist, { kickforge::DistStage::Type::bitcrush, 100.0f, 100.0f });

    const auto out = processThrough (dist, makeSine (500.0, 0.8f, 8192));

    // Réduction de sample rate : beaucoup d'échantillons consécutifs identiques
    int held = 0;
    for (size_t i = 1; i < out.size(); ++i)
        if (out[i] == out[i - 1])
            ++held;
    REQUIRE (static_cast<float> (held) / static_cast<float> (out.size() - 1) > 0.5f);
}

TEST_CASE ("la branche wet est compensée en gain", "[DistStage]")
{
    const auto in = makeSine (150.0, 0.7f, 8192);
    const float rmsIn = rmsOf (in, 1024, in.size());

    for (auto type : { kickforge::DistStage::Type::tube,
                       kickforge::DistStage::Type::fuzz,
                       kickforge::DistStage::Type::bitcrush })
    {
        kickforge::DistStage dist;
        prepareStage (dist, { type, 60.0f, 100.0f });

        const auto out = processThrough (dist, in);
        const float rmsOut = rmsOf (out, 1024, out.size());

        REQUIRE (rmsOut > rmsIn / 3.0f);
        REQUIRE (rmsOut < rmsIn * 3.0f);
    }
}

TEST_CASE ("les FX à mix 0 sont transparents", "[FxStage]")
{
    kickforge::FxStage fx;
    prepareStage (fx, { 0.0f, 0.0f }, 2);

    const auto in = makeSine (100.0, 0.5f, 8192);
    const auto [left, right] = processThroughFx (fx, in, in);

    std::vector<float> diff (in.size());
    for (size_t i = 0; i < in.size(); ++i)
        diff[i] = left[i] - in[i];
    REQUIRE (rmsOf (diff, 0, diff.size()) < 1.0e-3f);

    for (size_t i = 0; i < in.size(); ++i)
        REQUIRE (left[i] == Approx (right[i]).margin (1.0e-6));
}

TEST_CASE ("la reverb laisse une queue stéréo décorrélée", "[FxStage]")
{
    kickforge::FxStage fx;
    prepareStage (fx, { 0.0f, 60.0f }, 2);

    // Burst de 100 ms puis silence : la queue doit persister
    auto in = makeSine (100.0, 0.8f, 4800);
    in.resize (48000, 0.0f);
    const auto [left, right] = processThroughFx (fx, in, in);

    const size_t tailBegin = 14400, tailEnd = 38400; // 0.3 s -> 0.8 s
    REQUIRE (rmsOf (left, tailBegin, tailEnd) > 1.0e-3f);
    REQUIRE (correlation (left, right, tailBegin, tailEnd) < 0.98);

    for (float s : left)
        REQUIRE (std::isfinite (s));
}

TEST_CASE ("le chorus module le signal sans le dénaturer", "[FxStage]")
{
    kickforge::FxStage fx;
    prepareStage (fx, { 60.0f, 0.0f }, 2);

    const std::vector<float> silentBus (24000, 0.0f);
    const auto in = makeSine (400.0, 0.5f, 24000);
    const auto [left, right] = processThroughFx (fx, in, silentBus);

    std::vector<float> diff (in.size());
    for (size_t i = 0; i < in.size(); ++i)
        diff[i] = left[i] - in[i];

    REQUIRE (rmsOf (diff, 4800, diff.size()) > 0.01f); // ça module

    // Sans exploser ni s'éteindre (l'interférence dry+wet peut aller
    // jusqu'à ~2x le niveau selon la phase, c'est attendu)
    const float rmsIn = rmsOf (in, 4800, in.size());
    REQUIRE (rmsOf (left, 4800, left.size()) > 0.3f * rmsIn);
    REQUIRE (rmsOf (left, 4800, left.size()) < 2.0f * rmsIn);

    for (float s : left)
        REQUIRE (std::isfinite (s));
}

// ---------------------------------------------------------------------------
// Étape 6 : CompStage, ClipStage, WidthStage
// ---------------------------------------------------------------------------

namespace
{
// Passe un couple L/R dans un étage stéréo par blocs de 512, avec warm-up.
template <typename Stage>
std::array<std::vector<float>, 2> processStereo (Stage& stage,
                                                 const std::vector<float>& inLeft,
                                                 const std::vector<float>& inRight)
{
    constexpr size_t block = 512;
    auto warmLeft = inLeft, warmRight = inRight;
    auto left = inLeft, right = inRight;

    for (auto [l, r] : { std::pair { &warmLeft, &warmRight }, std::pair { &left, &right } })
        for (size_t pos = 0; pos < l->size(); pos += block)
        {
            const auto n = std::min (block, l->size() - pos);
            float* channels[] = { l->data() + pos, r->data() + pos };
            juce::dsp::AudioBlock<float> stereoBlock (channels, 2, n);
            stage.process (stereoBlock);
        }
    return { left, right };
}

std::vector<float> sideOf (const std::vector<float>& l, const std::vector<float>& r)
{
    std::vector<float> side (l.size());
    for (size_t i = 0; i < l.size(); ++i)
        side[i] = 0.5f * (l[i] - r[i]);
    return side;
}
} // namespace

TEST_CASE ("le compresseur à 0 % est transparent", "[CompStage]")
{
    kickforge::CompStage comp;
    prepareStage (comp, { 0.0f, 8.0f }, 2);

    const auto in = makeSine (100.0, 0.8f, 8192);
    const auto [left, right] = processStereo (comp, in, in);

    REQUIRE (rmsOf (left, 1024, left.size())
             == Approx (rmsOf (in, 1024, in.size())).epsilon (0.03));
}

TEST_CASE ("le compresseur réduit la dynamique et expose sa gain reduction", "[CompStage]")
{
    auto gainFor = [] (float amplitude, float& grDbOut)
    {
        kickforge::CompStage comp;
        prepareStage (comp, { 70.0f, 8.0f }, 2);
        const auto in = makeSine (100.0, amplitude, 16384);
        const auto [left, right] = processStereo (comp, in, in);
        grDbOut = comp.getGainReductionDb();
        return rmsOf (left, 8192, left.size()) / rmsOf (in, 8192, in.size());
    };

    float grLoud = 0.0f, grQuiet = 0.0f;
    const float gainLoud  = gainFor (0.9f, grLoud);
    const float gainQuiet = gainFor (0.02f, grQuiet); // sous le threshold (-21 dB)

    // Le fort est plus comprimé que le faible : la dynamique se resserre
    REQUIRE (gainLoud < 0.8f * gainQuiet);

    // GR meter : réduction franche sur le signal fort, quasi nulle sur le faible
    REQUIRE (grLoud > 2.0f);
    REQUIRE (grQuiet < 1.0f);
}

TEST_CASE ("la gain reduction retombe à zéro sur le silence", "[CompStage]")
{
    kickforge::CompStage comp;
    prepareStage (comp, { 100.0f, 1.0f }, 2);

    const std::vector<float> silence (8192, 0.0f);
    processStereo (comp, silence, silence);

    REQUIRE (comp.getGainReductionDb() == Approx (0.0f).margin (0.1f));
}

TEST_CASE ("le clipper hard plafonne exactement au ceiling", "[ClipStage]")
{
    kickforge::ClipStage clip;
    prepareStage (clip, { kickforge::ClipStage::Type::hard, -6.0f }, 2);

    const auto in = makeSine (100.0, 1.0f, 8192);
    const auto [left, right] = processStereo (clip, in, in);

    const float ceiling = juce::Decibels::decibelsToGain (-6.0f);
    float peak = 0.0f;
    for (float s : left)
        peak = std::max (peak, std::abs (s));

    REQUIRE (peak <= ceiling * 1.001f);
    REQUIRE (peak >= ceiling * 0.98f); // le signal atteint bien le plafond
}

TEST_CASE ("le clipper soft reste sous le ceiling et transparent à bas niveau", "[ClipStage]")
{
    kickforge::ClipStage clip;
    prepareStage (clip, { kickforge::ClipStage::Type::soft, -0.3f }, 2);

    const auto loud = makeSine (100.0, 1.5f, 8192);
    const auto [loudL, loudR] = processStereo (clip, loud, loud);
    const float ceiling = juce::Decibels::decibelsToGain (-0.3f);
    for (float s : loudL)
        REQUIRE (std::abs (s) <= ceiling * 1.001f);

    clip.reset();
    const auto quiet = makeSine (100.0, 0.1f, 8192);
    const auto [quietL, quietR] = processStereo (clip, quiet, quiet);
    REQUIRE (rmsOf (quietL, 1024, quietL.size())
             == Approx (rmsOf (quiet, 1024, quiet.size())).epsilon (0.03));
}

TEST_CASE ("le width force le sub en mono et élargit le haut", "[WidthStage]")
{
    // Mid : 60 Hz. Side : 40 Hz (sub, doit disparaître) et 1 kHz (doit être dosé).
    const int n = 16384;
    std::vector<float> left (n), right (n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / kSampleRate;
        const float mid    = 0.5f * static_cast<float> (std::sin (2.0 * M_PI * 60.0 * t));
        const float side40 = 0.3f * static_cast<float> (std::sin (2.0 * M_PI * 40.0 * t));
        const float side1k = 0.4f * static_cast<float> (std::sin (2.0 * M_PI * 1000.0 * t));
        left[static_cast<size_t> (i)]  = mid + side40 + side1k;
        right[static_cast<size_t> (i)] = mid - side40 - side1k;
    }

    SECTION ("width 150 % : sub mono, haut élargi x1.5")
    {
        kickforge::WidthStage width;
        prepareStage (width, { 150.0f }, 2);
        const auto [outL, outR] = processStereo (width, left, right);
        const auto side = sideOf (outL, outR);

        REQUIRE (goertzelMag (side, 40.0, kSampleRate) < 0.1 * 0.3 * side.size() / 2);
        REQUIRE (goertzelMag (side, 1000.0, kSampleRate)
                 == Approx (1.5 * goertzelMag (sideOf (left, right), 1000.0, kSampleRate)).epsilon (0.1));

        // Le mid (60 Hz) est intact
        std::vector<float> midOut (outL.size());
        for (size_t i = 0; i < outL.size(); ++i)
            midOut[i] = 0.5f * (outL[i] + outR[i]);
        std::vector<float> midIn (left.size());
        for (size_t i = 0; i < left.size(); ++i)
            midIn[i] = 0.5f * (left[i] + right[i]);
        REQUIRE (goertzelMag (midOut, 60.0, kSampleRate)
                 == Approx (goertzelMag (midIn, 60.0, kSampleRate)).epsilon (0.05));
    }

    SECTION ("width 0 % : mono total, corrélation L/R = 1")
    {
        kickforge::WidthStage width;
        prepareStage (width, { 0.0f }, 2);
        const auto [outL, outR] = processStereo (width, left, right);

        REQUIRE (correlation (outL, outR, 1024, outL.size()) > 0.999);
    }

    SECTION ("width 100 % : stéréo native au-dessus du crossover")
    {
        kickforge::WidthStage width;
        prepareStage (width, { 100.0f }, 2);
        const auto [outL, outR] = processStereo (width, left, right);
        const auto side = sideOf (outL, outR);

        REQUIRE (goertzelMag (side, 1000.0, kSampleRate)
                 == Approx (goertzelMag (sideOf (left, right), 1000.0, kSampleRate)).epsilon (0.1));

        // Corrélation sous 150 Hz : on isole le sub du side, il doit être quasi nul
        REQUIRE (goertzelMag (side, 40.0, kSampleRate) < 0.1 * 0.3 * side.size() / 2);
    }
}

// ---------------------------------------------------------------------------
// Étape 7 : presets par genre + randomisation
// ---------------------------------------------------------------------------

#include "presets/GenrePresets.h"

#include <set>
#include <string>

namespace
{
float presetValue (const kickforge::presets::GenrePreset& preset, const std::string& id)
{
    for (const auto& entry : preset.values)
        if (id == entry.id)
            return entry.value;
    FAIL ("paramètre absent du preset : " + id);
    return 0.0f;
}
} // namespace

TEST_CASE ("les 5 presets couvrent tous les paramètres sauf genre et outputGain", "[presets]")
{
    using namespace kickforge::presets;

    REQUIRE (numGenres == 5);

    std::set<std::string> reference;
    for (const auto& entry : genrePresets[0].values)
        reference.insert (entry.id);

    // 50 paramètres APVTS v2 - genre - outputGain = 48
    REQUIRE (reference.size() == static_cast<size_t> (numPresetParams));
    REQUIRE (numPresetParams == 48);
    REQUIRE (reference.count ("genre") == 0);
    REQUIRE (reference.count ("outputGain") == 0);
    REQUIRE (reference.count ("keyTrack") == 1);

    for (int g = 1; g < numGenres; ++g)
    {
        std::set<std::string> ids;
        for (const auto& entry : genrePresets[g].values)
            ids.insert (entry.id);
        REQUIRE (ids == reference);
    }
}

TEST_CASE ("les valeurs des presets correspondent à la section 6 du brief", "[presets]")
{
    using namespace kickforge::presets;

    const auto& techno    = genrePresets[0];
    const auto& hardstyle = genrePresets[2];
    const auto& trance    = genrePresets[4];

    REQUIRE (std::string (hardstyle.name) == "Hardstyle");

    // Hardstyle : le kick le plus agressif
    REQUIRE (presetValue (hardstyle, "pitchStart") == 320.0f);
    REQUIRE (presetValue (hardstyle, "sweepTime") == 70.0f);
    REQUIRE (presetValue (hardstyle, "atkLevel") == 85.0f);
    REQUIRE (presetValue (hardstyle, "driveType") == 1.0f);   // Hard
    REQUIRE (presetValue (hardstyle, "driveAmount") == 90.0f);
    REQUIRE (presetValue (hardstyle, "distType") == 1.0f);    // Fuzz
    REQUIRE (presetValue (hardstyle, "clipCeiling") == -0.1f);
    REQUIRE (presetValue (hardstyle, "width") == 80.0f);
    REQUIRE (presetValue (hardstyle, "oscBOn") == 1.0f);
    REQUIRE (presetValue (hardstyle, "oscBLevel") == 30.0f);

    // Techno : soft, oscB off
    REQUIRE (presetValue (techno, "oscBOn") == 0.0f);
    REQUIRE (presetValue (techno, "driveType") == 0.0f);      // Soft
    REQUIRE (presetValue (techno, "pitchStart") == 180.0f);
    REQUIRE (presetValue (techno, "width") == 60.0f);

    // Trance : oscB triangle à l'unisson, FX généreux
    REQUIRE (presetValue (trance, "oscBWave") == 1.0f);       // Tri
    REQUIRE (presetValue (trance, "oscBTune") == 0.0f);
    REQUIRE (presetValue (trance, "reverbMix") == 20.0f);
    REQUIRE (presetValue (trance, "chorusMix") == 10.0f);
}

TEST_CASE ("la randomisation reste dans ±25 % autour du preset, bornée à [0, 1]", "[presets]")
{
    using kickforge::presets::randomizedNormalised;

    // Autour du milieu : la fenêtre complète est accessible
    for (float u : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        const float v = randomizedNormalised (0.5f, u);
        REQUIRE (v >= 0.25f);
        REQUIRE (v <= 0.75f);
    }
    REQUIRE (randomizedNormalised (0.5f, 0.0f) == Approx (0.25f));
    REQUIRE (randomizedNormalised (0.5f, 1.0f) == Approx (0.75f));
    REQUIRE (randomizedNormalised (0.5f, 0.5f) == Approx (0.5f));

    // Près des bords : clampé
    REQUIRE (randomizedNormalised (0.1f, 0.0f) == Approx (0.0f));
    REQUIRE (randomizedNormalised (0.95f, 1.0f) == Approx (1.0f));
}

// ---------------------------------------------------------------------------
// Étape 8 : maths de l'UI (axe log, réponse de bande peak pour la courbe EQ)
// ---------------------------------------------------------------------------

#include "ui/UiScaling.h"

TEST_CASE ("l'axe de fréquence log est une bijection bornée", "[ui]")
{
    using namespace kickforge::ui;

    REQUIRE (freqToNorm (20.0f, 20.0f, 18000.0f) == Approx (0.0f).margin (1e-4));
    REQUIRE (freqToNorm (18000.0f, 20.0f, 18000.0f) == Approx (1.0f).margin (1e-4));

    // Moyenne géométrique au centre
    const float centre = std::sqrt (20.0f * 18000.0f);
    REQUIRE (freqToNorm (centre, 20.0f, 18000.0f) == Approx (0.5f).margin (1e-3));

    for (float f : { 20.0f, 60.0f, 800.0f, 6000.0f, 18000.0f })
        REQUIRE (normToFreq (freqToNorm (f, 20.0f, 18000.0f), 20.0f, 18000.0f)
                 == Approx (f).epsilon (1e-3));
}

TEST_CASE ("la réponse d'une bande peak culmine au centre et s'annule au loin", "[ui]")
{
    using kickforge::ui::peakBandDbAt;

    // Au centre : le gain demandé
    REQUIRE (peakBandDbAt (1000.0f, 1000.0f, 6.0f, 1.0f, 48000.0) == Approx (6.0f).margin (0.3));
    REQUIRE (peakBandDbAt (60.0f, 60.0f, -12.0f, 0.8f, 48000.0) == Approx (-12.0f).margin (0.3));

    // Loin du centre : quasi plat
    REQUIRE (peakBandDbAt (20.0f, 1000.0f, 6.0f, 1.0f, 48000.0) == Approx (0.0f).margin (0.3));
    REQUIRE (peakBandDbAt (15000.0f, 1000.0f, 6.0f, 1.0f, 48000.0) == Approx (0.0f).margin (0.5));

    // Gain nul : identité partout
    for (float f : { 30.0f, 500.0f, 8000.0f })
        REQUIRE (peakBandDbAt (f, 1000.0f, 0.0f, 1.0f, 48000.0) == Approx (0.0f).margin (0.01));

    // Un Q serré resserre la bande
    const float wideAt500   = peakBandDbAt (500.0f, 1000.0f, 12.0f, 0.3f, 48000.0);
    const float narrowAt500 = peakBandDbAt (500.0f, 1000.0f, 12.0f, 6.0f, 48000.0);
    REQUIRE (narrowAt500 < 0.5f * wideAt500);
}

// ---------------------------------------------------------------------------
// Étape 9 : export WAV
// ---------------------------------------------------------------------------

#include "export/WavExporter.h"

TEST_CASE ("la durée d'export = decay + queue de reverb, plafonnée à 3 s", "[export]")
{
    using kickforge::WavExporter;

    WavExporter::ChainParameters p; // défauts : decay 340 ms, reverb 10 %
    REQUIRE (WavExporter::lengthSecondsFor (p) == Approx (0.34 + 2.0).margin (0.01));

    p.fx.reverbMixPercent = 0.0f;
    REQUIRE (WavExporter::lengthSecondsFor (p) < 0.6);
    REQUIRE (WavExporter::lengthSecondsFor (p) >= 0.34);

    p.engine.punch.decayMs = 2000.0f;
    p.fx.reverbMixPercent = 20.0f;
    REQUIRE (WavExporter::lengthSecondsFor (p) == Approx (3.0));

    // La couche crunch compte quand elle est active
    p.engine.punch.decayMs = 200.0f;
    p.fx.reverbMixPercent = 0.0f;
    p.engine.crunch.levelPercent = 50.0f;
    p.engine.crunch.attackMs = 100.0f;
    p.engine.crunch.decayMs  = 800.0f;
    REQUIRE (WavExporter::lengthSecondsFor (p) == Approx (0.9 + 0.15).margin (0.01));
}

TEST_CASE ("le rendu offline produit un kick stéréo fini et non silencieux", "[export]")
{
    using kickforge::WavExporter;

    WavExporter::ChainParameters p;
    const double length = WavExporter::lengthSecondsFor (p);
    const auto buffer = WavExporter::renderKick (p, length);

    REQUIRE (buffer.getNumChannels() == 2);
    REQUIRE (buffer.getNumSamples() == juce::roundToInt (length * 48000.0));

    // Le transitoire est bien au début
    REQUIRE (buffer.getMagnitude (0, 0, 2400) > 0.1f);

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            REQUIRE (std::isfinite (data[i]));
    }
}

TEST_CASE ("le WAV écrit est en 48 kHz / 24 bits stéréo et se relit à l'identique", "[export]")
{
    using kickforge::WavExporter;

    WavExporter::ChainParameters p;
    p.engine.punch.decayMs = 100.0f;
    p.fx.reverbMixPercent = 0.0f; // court : test rapide
    const auto buffer = WavExporter::renderKick (p, WavExporter::lengthSecondsFor (p));

    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("kickforge_test_export.wav");
    file.deleteFile();

    REQUIRE (WavExporter::writeWavFile (buffer, file));

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader (
        format.createReaderFor (file.createInputStream().release(), true));
    REQUIRE (reader != nullptr);
    REQUIRE (reader->sampleRate == 48000.0);
    REQUIRE (reader->bitsPerSample == 24);
    REQUIRE (reader->numChannels == 2);
    REQUIRE (static_cast<int> (reader->lengthInSamples) == buffer.getNumSamples());

    juce::AudioBuffer<float> readBack (2, buffer.getNumSamples());
    REQUIRE (reader->read (&readBack, 0, buffer.getNumSamples(), 0, true, true));
    for (int i = 0; i < buffer.getNumSamples(); i += 37)
        REQUIRE (readBack.getSample (0, i)
                 == Approx (buffer.getSample (0, i)).margin (2.0e-4)); // quantif 24 bits

    file.deleteFile();
}

// ---------------------------------------------------------------------------
// Loop de pré-écoute (demande utilisateur, hors brief)
// ---------------------------------------------------------------------------

TEST_CASE ("activer le loop déclenche immédiatement puis retrigge à l'intervalle exact", "[KickEngine][loop]")
{
    KickEngine engine;
    engine.prepare ({ kSampleRate, 512, 1 });

    auto engineParams = neutralEngineParams();
    engineParams.punch.decayMs = 50.0f; // kick éteint bien avant le prochain trigger
    engine.setParameters (engineParams);

    constexpr int interval = 9600; // 0.2 s
    engine.setLoopIntervalSamples (interval);
    engine.setLooping (true);

    const auto out = renderSamples (engine, interval * 4);

    // Départ immédiat
    REQUIRE (rmsOf (out, 0, 2400) > 0.05f);

    for (int k = 1; k <= 3; ++k)
    {
        const auto t = static_cast<size_t> (k * interval);
        // Juste avant le trigger : silence (l'enveloppe est morte depuis longtemps)
        REQUIRE (std::abs (out[t - 8]) < 1.0e-3f);
        // Juste après : le kick est reparti
        REQUIRE (rmsOf (out, t, t + 2400) > 0.05f);
    }
}

TEST_CASE ("désactiver le loop arrête les retriggers", "[KickEngine][loop]")
{
    KickEngine engine;
    engine.prepare ({ kSampleRate, 512, 1 });

    engine.setParameters ([]
    {
        auto p = neutralEngineParams();
        p.punch.decayMs = 50.0f;
        return p;
    }());
    engine.reset();

    engine.setLoopIntervalSamples (4800);
    engine.setLooping (true);
    renderSamples (engine, 9600);
    engine.setLooping (false);

    // Après l'extinction de la voix en cours, plus rien ne repart
    const auto out = renderSamples (engine, 48000);
    REQUIRE (rmsOf (out, 24000, out.size()) < 1.0e-4f);
}

TEST_CASE ("le loop est inactif par défaut", "[KickEngine][loop]")
{
    KickEngine engine;
    engine.prepare ({ kSampleRate, 512, 1 });
    engine.setParameters (neutralEngineParams());
    engine.reset();
    engine.noteOn();

    // Un seul kick : après 2 s, silence total
    const auto out = renderSamples (engine, 96000);
    REQUIRE (rmsOf (out, 72000, out.size()) < 1.0e-4f);
}

TEST_CASE ("réduire l'intervalle sous le compteur courant retrigge sans corruption", "[KickEngine][loop]")
{
    KickEngine engine;
    engine.prepare ({ kSampleRate, 512, 1 });

    engine.setParameters ([]
    {
        auto p = neutralEngineParams();
        p.punch.decayMs = 50.0f;
        return p;
    }());
    engine.reset();

    engine.setLoopIntervalSamples (9600);
    engine.setLooping (true);
    renderSamples (engine, 5000); // compteur à 5000

    // L'utilisateur tourne le knob : nouvel intervalle plus court que le compteur
    engine.setLoopIntervalSamples (2400);
    const auto out = renderSamples (engine, 9600);

    // Retrigger immédiat (compteur >= intervalle) puis période de 2400
    REQUIRE (rmsOf (out, 0, 1200) > 0.05f);
    for (int k = 1; k <= 3; ++k)
    {
        const auto t = static_cast<size_t> (k * 2400);
        REQUIRE (rmsOf (out, t, t + 1200) > 0.05f);
    }
}

// ---------------------------------------------------------------------------
// Étape 10 : polish — garde au ceiling, anti-clic, anti-zipper
// ---------------------------------------------------------------------------

TEST_CASE ("le width clampe au ceiling de sécurité", "[WidthStage][polish]")
{
    kickforge::WidthStage width;
    kickforge::WidthStage::Parameters p;
    p.widthPercent    = 150.0f;
    p.safetyCeilingDb = -6.0f;
    prepareStage (width, p, 2);

    // Entrée stéréo décorrélée qui dépasse largement le ceiling
    const auto inL = makeSine (400.0, 1.0f, 8192);
    const auto inR = makeSine (700.0, 1.0f, 8192);
    const auto [outL, outR] = processStereo (width, inL, inR);

    const float ceiling = juce::Decibels::decibelsToGain (-6.0f);
    for (size_t i = 0; i < outL.size(); ++i)
    {
        REQUIRE (std::abs (outL[i]) <= ceiling * 1.0001f);
        REQUIRE (std::abs (outR[i]) <= ceiling * 1.0001f);
    }
}

TEST_CASE ("la chaîne complète ne dépasse jamais le ceiling du clipper", "[export][polish]")
{
    using kickforge::WavExporter;

    // Valeurs hardstyle : le cas le plus chaud (ceiling -0.1 dB, drive 90 %)
    WavExporter::ChainParameters p;
    p.engine.punch.pitchStartHz     = 320.0f;
    p.engine.punch.sweepTimeMs      = 70.0f;
    p.engine.punch.oscBOn           = true;
    p.engine.punch.oscBLevelPercent = 30.0f;
    p.engine.attack.levelPercent    = 85.0f;
    p.engine.punchDrive             = { kickforge::DriveStage::Type::hard, 90.0f, 3500.0f };
    p.dist                    = { kickforge::DistStage::Type::fuzz, 40.0f, 50.0f };
    p.comp                    = { 55.0f, 5.0f };
    p.clip                    = { kickforge::ClipStage::Type::hard, -0.1f };
    p.width                   = { 80.0f, -0.1f };
    p.fx.reverbMixPercent     = 5.0f;
    p.outputGainDb            = 0.0f; // pas de marge du gain : le ceiling doit tenir seul

    const auto buffer = WavExporter::renderKick (p, 1.0);
    const float ceiling = juce::Decibels::decibelsToGain (-0.1f);

    REQUIRE (buffer.getMagnitude (0, buffer.getNumSamples()) <= ceiling * 1.0001f);
}

TEST_CASE ("des retriggers rapides n'introduisent aucun clic", "[KickEngine][polish]")
{
    KickEngine engine;
    engine.prepare ({ kSampleRate, 512, 1 });

    engine.setParameters (neutralEngineParams());
    engine.reset();

    // 16 notes à ~60 ms d'écart (kick roll à tempo extrême)
    std::vector<float> out;
    for (int note = 0; note < 16; ++note)
    {
        engine.noteOn();
        const auto chunk = renderSamples (engine, 3000);
        out.insert (out.end(), chunk.begin(), chunk.end());
    }

    float maxJump = 0.0f;
    for (size_t i = 1; i < out.size(); ++i)
        maxJump = std::max (maxJump, std::abs (out[i] - out[i - 1]));
    REQUIRE (maxJump < 0.12f);
}

TEST_CASE ("un sweep violent du cutoff ne produit pas de zipper", "[FilterStage][polish]")
{
    kickforge::FilterStage filter;
    prepareStage (filter, { kickforge::FilterStage::Type::lowpass, 20000.0f, 20.0f });

    // Sinus tenu pendant qu'on alterne brutalement la cible du cutoff
    const auto in = makeSine (200.0, 0.7f, 512 * 40);
    std::vector<float> out = in;

    for (int block = 0; block < 40; ++block)
    {
        filter.setParameters ({ kickforge::FilterStage::Type::lowpass,
                                block % 2 == 0 ? 500.0f : 20000.0f, 20.0f });
        float* channel[] = { out.data() + block * 512 };
        juce::dsp::AudioBlock<float> monoBlock (channel, 1, 512);
        filter.process (monoBlock);
    }

    float maxJump = 0.0f;
    for (size_t i = 1; i < out.size(); ++i)
        maxJump = std::max (maxJump, std::abs (out[i] - out[i - 1]));

    // Base : delta max d'un sinus 200 Hz ~ 0.018 ; on tolère la modulation
    // du filtre mais pas les clics francs.
    REQUIRE (maxJump < 0.25f);
}

// ---------------------------------------------------------------------------
// V2 étape 1 : AttackLayer (spec BRIEF-KICKFORGE-V2.md)
// ---------------------------------------------------------------------------

#include "dsp/AttackLayer.h"

TEST_CASE ("l'AttackLayer est silencieux avant trigger et s'éteint après le decay", "[AttackLayer]")
{
    kickforge::AttackLayer attack;
    attack.prepare (kSampleRate);
    attack.setParameters ({ 70.0f, 3.0f, 5000.0f });

    std::vector<float> out (4800, 0.0f);
    attack.renderAdd (out.data(), 4800);
    for (float s : out)
        REQUIRE (s == 0.0f);
    REQUIRE_FALSE (attack.isActive());

    attack.trigger();
    REQUIRE (attack.isActive());
    std::fill (out.begin(), out.end(), 0.0f);
    attack.renderAdd (out.data(), 4800); // 100 ms >> 5x decay
    REQUIRE (rmsOf (out, 0, 96) > 0.02f);
    REQUIRE_FALSE (attack.isActive());
    REQUIRE (rmsOf (out, 2400, out.size()) < 1.0e-6f);
}

TEST_CASE ("atkDecay contrôle la durée du transitoire", "[AttackLayer]")
{
    auto energyAt10ms = [] (float decayMs)
    {
        kickforge::AttackLayer attack;
        attack.prepare (kSampleRate);
        attack.setParameters ({ 100.0f, decayMs, 5000.0f });
        attack.trigger();
        std::vector<float> out (960, 0.0f);
        attack.renderAdd (out.data(), 960);
        return rmsOf (out, 384, 576); // fenêtre 8-12 ms
    };

    REQUIRE (energyAt10ms (30.0f) > 20.0f * energyAt10ms (1.0f));
}

TEST_CASE ("atkTone assombrit le transitoire", "[AttackLayer]")
{
    auto derivativeRms = [] (float toneHz)
    {
        kickforge::AttackLayer attack;
        attack.prepare (kSampleRate);
        attack.setParameters ({ 100.0f, 30.0f, toneHz });
        attack.trigger();
        std::vector<float> out (1440, 0.0f);
        attack.renderAdd (out.data(), 1440);

        std::vector<float> diff (out.size() - 1);
        for (size_t i = 1; i < out.size(); ++i)
            diff[i - 1] = out[i] - out[i - 1];
        return rmsOf (diff, 0, diff.size()) / (rmsOf (out, 0, out.size()) + 1.0e-9f);
    };

    // Le contenu HF relatif (dérivée normalisée) chute nettement avec le tone bas
    REQUIRE (derivativeRms (16000.0f) > 2.0f * derivativeRms (500.0f));
}

// ---------------------------------------------------------------------------
// V2 étape 2 : CrunchLayer (spec BRIEF-KICKFORGE-V2.md)
// ---------------------------------------------------------------------------

#include "dsp/CrunchLayer.h"

namespace
{
kickforge::CrunchLayer::Parameters crunchTestParams()
{
    kickforge::CrunchLayer::Parameters p;
    p.levelPercent  = 100.0f;
    p.wave          = kickforge::CrunchLayer::Waveform::sine;
    p.tuneSemitones = 0.0f;
    p.attackMs      = 20.0f;
    p.decayMs       = 2000.0f; // long : les fenêtres de mesure restent dans du signal vivant
    p.pitchEndHz    = 50.0f;
    p.drive         = { kickforge::DriveStage::Type::hard, 0.0f, 16000.0f }; // transparent
    return p;
}
} // namespace

TEST_CASE ("le CrunchLayer est silencieux avant trigger et s'éteint après le decay", "[CrunchLayer]")
{
    kickforge::CrunchLayer crunch;
    crunch.prepare ({ kSampleRate, 512, 1 });
    auto p = crunchTestParams();
    p.decayMs = 300.0f; // court pour observer l'extinction dans 2 s de rendu
    crunch.setParameters (p);

    std::vector<float> out (4800, 0.0f);
    crunch.renderAdd (out.data(), 4800);
    for (float s : out)
        REQUIRE (s == 0.0f);
    REQUIRE_FALSE (crunch.isActive());

    crunch.trigger();
    REQUIRE (crunch.isActive());
    std::fill (out.begin(), out.end(), 0.0f);
    out.resize (static_cast<size_t> (2.0 * kSampleRate), 0.0f);
    crunch.renderAdd (out.data(), static_cast<int> (out.size()));

    // Présent après l'attack (env ~ -16 a -36 dB sur 0.1-0.2 s avec decay 300 ms)
    REQUIRE (rmsOf (out, 4800, 9600) > 0.02f);
    REQUIRE_FALSE (crunch.isActive()); // éteint bien avant 2 s
    REQUIRE (rmsOf (out, out.size() - 9600, out.size()) < 1.0e-4f);
}

TEST_CASE ("le CrunchLayer est accordé sur pitchEnd, transposable en demi-tons", "[CrunchLayer]")
{
    auto measureFreq = [] (float tuneSemitones)
    {
        kickforge::CrunchLayer crunch;
        crunch.prepare ({ kSampleRate, 512, 1 });
        auto p = crunchTestParams();
        p.tuneSemitones = tuneSemitones;
        crunch.setParameters (p);
        crunch.trigger();

        std::vector<float> out (static_cast<size_t> (kSampleRate), 0.0f);
        crunch.renderAdd (out.data(), static_cast<int> (out.size()));

        // Zéros croissants entre 0.2 s et 0.8 s (enveloppe établie)
        int firstZc = 0, lastZc = 0, count = 0;
        for (size_t i = 9600; i < 38400; ++i)
            if (out[i - 1] < 0.0f && out[i] >= 0.0f)
            {
                if (count == 0)
                    firstZc = static_cast<int> (i);
                lastZc = static_cast<int> (i);
                ++count;
            }
        return kSampleRate * (count - 1) / static_cast<double> (lastZc - firstZc);
    };

    REQUIRE (measureFreq (0.0f) == Approx (50.0).margin (0.5));
    REQUIRE (measureFreq (12.0f) == Approx (100.0).margin (1.0));
}

TEST_CASE ("l'enveloppe du crunch monte lentement puis tient", "[CrunchLayer]")
{
    kickforge::CrunchLayer crunch;
    crunch.prepare ({ kSampleRate, 512, 1 });
    auto p = crunchTestParams();
    p.attackMs = 100.0f;
    crunch.setParameters (p);
    crunch.trigger();

    std::vector<float> out (static_cast<size_t> (kSampleRate / 2), 0.0f);
    crunch.renderAdd (out.data(), static_cast<int> (out.size()));

    // 0-10 ms : à peine parti ; à 100 ms (fin d'attack) : plein niveau
    REQUIRE (rmsOf (out, 0, 480) < 0.3f * rmsOf (out, 4560, 5040));
}

TEST_CASE ("le crunch a sa propre distorsion", "[CrunchLayer]")
{
    auto h3Ratio = [] (float driveAmount)
    {
        kickforge::CrunchLayer crunch;
        crunch.prepare ({ kSampleRate, 512, 1 });
        auto p = crunchTestParams();
        p.drive = { kickforge::DriveStage::Type::hard, driveAmount, 16000.0f };
        crunch.setParameters (p);
        crunch.trigger();

        std::vector<float> out (static_cast<size_t> (kSampleRate), 0.0f);
        crunch.renderAdd (out.data(), static_cast<int> (out.size()));

        const std::vector<float> window (out.begin() + 9600, out.begin() + 26984);
        return goertzelMag (window, 150.0, kSampleRate)
               / goertzelMag (window, 50.0, kSampleRate);
    };

    REQUIRE (h3Ratio (0.0f) < 0.02);  // transparent
    REQUIRE (h3Ratio (90.0f) > 0.1);  // saturé : harmoniques impaires franches
}

TEST_CASE ("renderAdd du crunch est additif et level 0 est neutre", "[CrunchLayer]")
{
    kickforge::CrunchLayer crunch;
    crunch.prepare ({ kSampleRate, 512, 1 });

    // Level 0 : rien n'est ajouté
    auto p = crunchTestParams();
    p.levelPercent = 0.0f;
    crunch.setParameters (p);
    crunch.trigger();
    std::vector<float> untouched (4800, 0.25f);
    crunch.renderAdd (untouched.data(), 4800);
    for (float s : untouched)
        REQUIRE (s == Approx (0.25f).margin (1.0e-9));

    // Level 100 : le contenu existant est préservé (addition, pas écrasement)
    p.levelPercent = 100.0f;
    crunch.setParameters (p);
    crunch.trigger();
    std::vector<float> mixed (4800, 0.25f);
    crunch.renderAdd (mixed.data(), 4800);
    float deviation = 0.0f;
    for (float s : mixed)
        deviation = std::max (deviation, std::abs (s - 0.25f));
    REQUIRE (deviation > 0.01f); // le crunch s'est bien ajouté par-dessus
}

// ---------------------------------------------------------------------------
// V2 étape 3 : orchestration des couches (critères de la spec)
// ---------------------------------------------------------------------------

TEST_CASE ("à couches neutres, le mix v2 est identique à la chaîne v1", "[KickEngine][v2]")
{
    constexpr int total = 4800;

    // Chemin v1 : voix (sans transitoire) -> DriveStage, par blocs de 512
    KickVoice v1Voice;
    v1Voice.prepare (kSampleRate);
    KickVoice::Parameters voiceParams; // défauts hard techno
    v1Voice.setParameters (voiceParams);

    DriveStage v1Drive;
    prepareStage (v1Drive, { DriveStage::Type::hard, 65.0f, 2400.0f });

    v1Voice.trigger();
    std::vector<float> v1Out (total, 0.0f);
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = std::min (512, total - pos);
        v1Voice.render (v1Out.data() + pos, n);
        float* channel[] = { v1Out.data() + pos };
        juce::dsp::AudioBlock<float> block (channel, 1, static_cast<size_t> (n));
        v1Drive.process (block);
    }

    // Chemin v2 : moteur avec attack 0, crunch 0, punchLevel 100, même drive
    KickEngine engine;
    engine.prepare ({ kSampleRate, 512, 1 });
    KickEngine::Parameters ep;
    ep.punch = voiceParams;
    ep.punchDrive = { DriveStage::Type::hard, 65.0f, 2400.0f };
    ep.attack.levelPercent = 0.0f;
    ep.crunch.levelPercent = 0.0f;
    engine.setParameters (ep);
    engine.reset();
    engine.noteOn();
    const auto [busAP, mix] = renderBuses (engine, total);

    for (int i = 0; i < total; ++i)
        REQUIRE (mix[static_cast<size_t> (i)]
                 == Approx (v1Out[static_cast<size_t> (i)]).margin (1.0e-6));
}

TEST_CASE ("l'attack échappe au drive du punch", "[KickEngine][v2]")
{
    // L'attack est ajouté POST-drive : la contribution de la couche doit être
    // exactement le rendu de l'AttackLayer seul, même avec un drive extrême.
    auto renderMix = [] (float atkLevel)
    {
        KickEngine engine;
        engine.prepare ({ kSampleRate, 512, 1 });
        KickEngine::Parameters ep;
        ep.punchDrive = { DriveStage::Type::hard, 100.0f, 16000.0f };
        ep.attack = { atkLevel, static_cast<float> (0.7 * std::log (1000.0)), 5000.0f };
        ep.crunch.levelPercent = 0.0f;
        engine.setParameters (ep);
        engine.reset();
        engine.noteOn();
        return renderSamples (engine, 2400);
    };

    const auto with    = renderMix (70.0f);
    const auto without = renderMix (0.0f);

    kickforge::AttackLayer reference;
    reference.prepare (kSampleRate);
    reference.setParameters ({ 70.0f, static_cast<float> (0.7 * std::log (1000.0)), 5000.0f });
    reference.trigger();
    std::vector<float> expected (2400, 0.0f);
    reference.renderAdd (expected.data(), 2400);

    for (size_t i = 0; i < expected.size(); ++i)
        REQUIRE (with[i] - without[i] == Approx (expected[i]).margin (1.0e-5));
}

TEST_CASE ("le busAP exclut le crunch", "[KickEngine][v2]")
{
    KickEngine engine;
    engine.prepare ({ kSampleRate, 512, 1 });
    auto ep = neutralEngineParams();
    ep.crunch.levelPercent = 80.0f;
    ep.crunch.attackMs     = 10.0f;
    ep.crunch.decayMs      = 2000.0f;
    engine.setParameters (ep);
    engine.reset();
    engine.noteOn();
    const auto [busWith, mixWith] = renderBuses (engine, 9600);

    KickEngine engineDry;
    engineDry.prepare ({ kSampleRate, 512, 1 });
    ep.crunch.levelPercent = 0.0f;
    engineDry.setParameters (ep);
    engineDry.reset();
    engineDry.noteOn();
    const auto [busDry, mixDry] = renderBuses (engineDry, 9600);

    // Le crunch est bien dans le mix...
    std::vector<float> crunchPart (9600);
    for (size_t i = 0; i < crunchPart.size(); ++i)
        crunchPart[i] = mixWith[i] - busWith[i];
    REQUIRE (rmsOf (crunchPart, 4800, crunchPart.size()) > 0.1f);

    // ... et absent du busAP, qui ignore totalement crunchLevel
    for (size_t i = 0; i < busWith.size(); ++i)
        REQUIRE (busWith[i] == Approx (busDry[i]).margin (1.0e-7));
}

// ---------------------------------------------------------------------------
// V2 étape 4 : send de reverb (busAP), crunch sec
// ---------------------------------------------------------------------------

TEST_CASE ("la reverb n'écoute que le busAP", "[FxStage][v2]")
{
    // busAP silencieux : un burst sur le bus principal ne laisse AUCUNE queue
    kickforge::FxStage fx;
    prepareStage (fx, { 0.0f, 80.0f }, 2);

    auto burst = makeSine (200.0, 0.8f, 4800);
    burst.resize (48000, 0.0f);
    const std::vector<float> silence (48000, 0.0f);

    const auto [dryL, dryR] = processThroughFx (fx, burst, silence);
    REQUIRE (rmsOf (dryL, 14400, 38400) < 1.0e-5f); // pas de queue

    // busAP actif, bus principal silencieux : le wet apparaît quand même
    kickforge::FxStage fx2;
    prepareStage (fx2, { 0.0f, 80.0f }, 2);
    const auto [wetL, wetR] = processThroughFx (fx2, silence, burst);
    REQUIRE (rmsOf (wetL, 4800, 38400) > 1.0e-3f); // queue présente
}

TEST_CASE ("chaîne complète : le crunch reste sec, attack+punch nourrissent la reverb", "[export][v2]")
{
    using kickforge::WavExporter;

    // Cas 1 : seulement du crunch (busAP muet), reverb à fond -> aucune queue
    WavExporter::ChainParameters crunchOnly;
    crunchOnly.engine.attack.levelPercent = 0.0f;
    crunchOnly.engine.punchLevelPercent   = 0.0f;
    crunchOnly.engine.crunch.levelPercent = 80.0f;
    crunchOnly.engine.crunch.attackMs     = 10.0f;
    crunchOnly.engine.crunch.decayMs      = 300.0f;
    crunchOnly.fx.reverbMixPercent        = 100.0f;
    crunchOnly.outputGainDb               = 0.0f;

    const auto dryRender = WavExporter::renderKick (crunchOnly, 2.5);
    // Le crunch est éteint vers 0.45 s ; s'il nourrissait la reverb, la
    // fenêtre 0.8-2.0 s contiendrait sa queue.
    float peakTail = 0.0f;
    for (int i = 38400; i < 96000; ++i)
        peakTail = juce::jmax (peakTail, std::abs (dryRender.getSample (0, i)));
    REQUIRE (peakTail < 1.0e-4f);

    // Cas 2 : attack + punch actifs, pas de crunch -> queue de reverb présente
    WavExporter::ChainParameters withPunch;
    withPunch.engine.attack.levelPercent = 85.0f;
    withPunch.engine.crunch.levelPercent = 0.0f;
    withPunch.fx.reverbMixPercent        = 100.0f;
    withPunch.outputGainDb               = 0.0f;

    const auto wetRender = WavExporter::renderKick (withPunch, 2.5);
    float tailRms = 0.0f;
    for (int i = 38400; i < 72000; ++i)
        tailRms += wetRender.getSample (0, i) * wetRender.getSample (0, i);
    tailRms = std::sqrt (tailRms / (72000 - 38400));
    REQUIRE (tailRms > 1.0e-4f);
}

// ---------------------------------------------------------------------------
// V2 étape 5 : migration d'état v1 -> v2
// ---------------------------------------------------------------------------

#include "presets/StateMigration.h"

TEST_CASE ("un état v1 est migré : tag v2, punch -> atkLevel, le reste intact", "[migration]")
{
    juce::XmlElement state ("kickforge_params_v1");
    auto addParam = [&state] (const char* id, double value)
    {
        auto* param = state.createNewChildElement ("PARAM");
        param->setAttribute ("id", id);
        param->setAttribute ("value", value);
    };
    addParam ("punch", 85.0);
    addParam ("pitchStart", 320.0);
    addParam ("driveAmount", 90.0);

    kickforge::presets::migrateStateToV2 (state);

    REQUIRE (state.hasTagName ("kickforge_params_v2"));

    juce::StringArray ids;
    for (auto* child : state.getChildWithTagNameIterator ("PARAM"))
        ids.add (child->getStringAttribute ("id"));
    REQUIRE (ids.contains ("atkLevel"));
    REQUIRE (! ids.contains ("punch"));
    REQUIRE (ids.contains ("pitchStart"));

    for (auto* child : state.getChildWithTagNameIterator ("PARAM"))
        if (child->getStringAttribute ("id") == "atkLevel")
            REQUIRE (child->getDoubleAttribute ("value") == Approx (85.0));
}

TEST_CASE ("un état v2 traverse la migration sans modification", "[migration]")
{
    juce::XmlElement state ("kickforge_params_v2");
    auto* param = state.createNewChildElement ("PARAM");
    param->setAttribute ("id", "atkLevel");
    param->setAttribute ("value", 42.0);

    kickforge::presets::migrateStateToV2 (state);

    REQUIRE (state.hasTagName ("kickforge_params_v2"));
    REQUIRE (state.getNumChildElements() == 1);
    REQUIRE (state.getFirstChildElement()->getDoubleAttribute ("value") == Approx (42.0));
}
