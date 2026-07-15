#include "WaveformDisplay.h"
#include "UiScaling.h"

namespace kickforge::ui
{

namespace
{
constexpr double renderSampleRate = 48000.0;
constexpr int renderBlockSize = 512;
constexpr int numColumns = 320;
} // namespace

WaveformDisplay::WaveformDisplay (juce::AudioProcessorValueTreeState& apvtsIn)
    : apvts (apvtsIn)
{
    const juce::dsp::ProcessSpec monoSpec { renderSampleRate, renderBlockSize, 1 };
    filter.prepare (monoSpec);
    eq.prepare (monoSpec);
    dist.prepare (monoSpec);
    comp.prepare (monoSpec);
    clip.prepare (monoSpec);

    renderBuffer.resize (static_cast<size_t> (renderSampleRate)); // 1 s max
    busScratch.resize (static_cast<size_t> (renderSampleRate));
    punchOnlyScratch.resize (static_cast<size_t> (renderSampleRate));
    layerScratch.resize (static_cast<size_t> (renderSampleRate));

    for (auto* silhouette : { &compositeSilhouette, &attackSilhouette,
                              &punchSilhouette, &crunchSilhouette })
    {
        silhouette->low.resize (numColumns, 0.0f);
        silhouette->high.resize (numColumns, 0.0f);
    }

    // Tous les paramètres influencent le rendu : on lève juste un drapeau.
    for (const auto* parameter : apvts.processor.getParameters())
        if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (parameter))
            listenedParamIDs.add (ranged->paramID);

    for (const auto& id : listenedParamIDs)
        apvts.addParameterListener (id, this);

    startTimerHz (30);
}

WaveformDisplay::~WaveformDisplay()
{
    for (const auto& id : listenedParamIDs)
        apvts.removeParameterListener (id, this);
}

void WaveformDisplay::timerCallback()
{
    if (dirty.exchange (false))
    {
        renderKick();
        repaint();
        if (onShapesUpdated)
            onShapesUpdated();
    }
}

void WaveformDisplay::renderKick()
{
    auto load = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    KickEngine::Parameters engineParams;
    auto& voiceParams = engineParams.punch;
    voiceParams.waveform     = static_cast<KickVoice::Waveform> (static_cast<int> (load ("waveform")));
    voiceParams.pitchStartHz = load ("pitchStart");
    voiceParams.pitchEndHz   = load ("pitchEnd");
    voiceParams.sweepTimeMs  = load ("sweepTime");
    voiceParams.oscBOn       = load ("oscBOn") > 0.5f;
    voiceParams.oscBWave     = static_cast<KickVoice::Waveform> (static_cast<int> (load ("oscBWave")));
    voiceParams.oscBTuneSemitones = load ("oscBTune");
    voiceParams.oscBLevelPercent  = load ("oscBLevel");
    voiceParams.attackMs     = load ("attack");
    voiceParams.decayMs      = load ("decay");

    engineParams.punchDrive = { static_cast<DriveStage::Type> (static_cast<int> (load ("driveType"))),
                                load ("driveAmount"), load ("driveTone") };
    engineParams.punchLevelPercent = load ("punchLevel");
    engineParams.attack = { load ("atkLevel"), load ("atkDecay"), load ("atkTone") };
    engineParams.crunch.levelPercent  = load ("crunchLevel");
    engineParams.crunch.wave          = static_cast<CrunchLayer::Waveform> (
                                            static_cast<int> (load ("crunchWave")));
    engineParams.crunch.tuneSemitones = load ("crunchTune");
    engineParams.crunch.attackMs      = load ("crunchAttack");
    engineParams.crunch.decayMs       = load ("crunchDecay");
    engineParams.crunch.pitchEndHz    = voiceParams.pitchEndHz;
    engineParams.crunch.drive = { static_cast<DriveStage::Type> (
                                      static_cast<int> (load ("crunchDriveType"))),
                                  load ("crunchDriveAmount"), load ("crunchDriveTone") };

    pitchStartHz = voiceParams.pitchStartHz;
    pitchEndHz   = voiceParams.pitchEndHz;
    sweepTimeMs  = voiceParams.sweepTimeMs;

    filter.setParameters ({ static_cast<FilterStage::Type> (static_cast<int> (load ("filterType"))),
                            load ("filterCutoff"), load ("filterReso") });

    EqStage::Parameters eqParams;
    eqParams.bands[0] = { load ("eq1Freq"), load ("eq1Gain"), load ("eq1Q") };
    eqParams.bands[1] = { load ("eq2Freq"), load ("eq2Gain"), load ("eq2Q") };
    eqParams.bands[2] = { load ("eq3Freq"), load ("eq3Gain"), load ("eq3Q") };
    eq.setParameters (eqParams);

    dist.setParameters ({ static_cast<DistStage::Type> (static_cast<int> (load ("distType"))),
                          load ("distAmount"), load ("distMix") });
    comp.setParameters ({ load ("compAmount"), load ("compAttack") });
    clip.setParameters ({ static_cast<ClipStage::Type> (static_cast<int> (load ("clipType"))),
                          load ("clipCeiling") });

    // Fenêtre adaptée à l'enveloppe la plus longue (punch ou crunch), bornée à 1 s
    const double longestMs = juce::jmax (
        static_cast<double> (voiceParams.decayMs),
        engineParams.crunch.levelPercent > 0.0f
            ? static_cast<double> (engineParams.crunch.attackMs + engineParams.crunch.decayMs)
            : 0.0);
    displaySeconds = juce::jlimit (0.15, 1.0, (longestMs * 1.2 + 50.0) / 1000.0);
    const int numSamples = static_cast<int> (displaySeconds * renderSampleRate);

    engine = std::make_unique<KickEngine>();
    engine->prepare ({ renderSampleRate, renderBlockSize, 1 });
    engine->setParameters (engineParams);
    engine->reset();
    filter.reset();
    eq.reset();
    dist.reset();
    comp.reset();
    clip.reset();
    engine->noteOn();

    // Passe 1 : bus bruts (busScratch = attack+punch, layerScratch = mix brut)
    for (int pos = 0; pos < numSamples; pos += renderBlockSize)
    {
        const int n = juce::jmin (renderBlockSize, numSamples - pos);
        engine->render (busScratch.data() + pos, layerScratch.data() + pos, n);
    }

    // Composite : mix brut copié puis passé dans la chaîne commune
    std::copy (layerScratch.begin(), layerScratch.begin() + numSamples, renderBuffer.begin());
    for (int pos = 0; pos < numSamples; pos += renderBlockSize)
    {
        const int n = juce::jmin (renderBlockSize, numSamples - pos);
        float* channel[] = { renderBuffer.data() + pos };
        juce::dsp::AudioBlock<float> block (channel, 1, static_cast<size_t> (n));
        filter.process (block);
        eq.process (block);
        dist.process (block);
        comp.process (block);
        clip.process (block);
    }

    auto columnize = [numSamples] (const float* signal, Silhouette& out)
    {
        out.peak = 0.0f;
        for (int col = 0; col < numColumns; ++col)
        {
            const int begin = col * numSamples / numColumns;
            const int end   = juce::jmax (begin + 1, (col + 1) * numSamples / numColumns);
            float lo = 0.0f, hi = 0.0f;
            for (int i = begin; i < end; ++i)
            {
                lo = juce::jmin (lo, signal[i]);
                hi = juce::jmax (hi, signal[i]);
            }
            out.low[static_cast<size_t> (col)]  = lo;
            out.high[static_cast<size_t> (col)] = hi;
            out.peak = juce::jmax (out.peak, juce::jmax (-lo, hi));
        }
    };

    columnize (renderBuffer.data(), compositeSilhouette);

    // Passe 2 (déterministe, attack muté) : punch seul dans punchOnlyScratch ;
    // renderBuffer sert de poubelle pour le mix de cette passe.
    auto punchParams = engineParams;
    punchParams.attack.levelPercent = 0.0f;
    auto punchEngine = std::make_unique<KickEngine>();
    punchEngine->prepare ({ renderSampleRate, renderBlockSize, 1 });
    punchEngine->setParameters (punchParams);
    punchEngine->reset();
    punchEngine->noteOn();
    for (int pos = 0; pos < numSamples; pos += renderBlockSize)
    {
        const int n = juce::jmin (renderBlockSize, numSamples - pos);
        punchEngine->render (punchOnlyScratch.data() + pos, renderBuffer.data() + pos, n);
    }
    columnize (punchOnlyScratch.data(), punchSilhouette);

    // attack = busAP - punchSeul ; crunch = mixBrut - busAP (buffers réutilisés)
    for (int i = 0; i < numSamples; ++i)
        punchOnlyScratch[static_cast<size_t> (i)] =
            busScratch[static_cast<size_t> (i)] - punchOnlyScratch[static_cast<size_t> (i)];
    columnize (punchOnlyScratch.data(), attackSilhouette);

    for (int i = 0; i < numSamples; ++i)
        busScratch[static_cast<size_t> (i)] =
            layerScratch[static_cast<size_t> (i)] - busScratch[static_cast<size_t> (i)];
    columnize (busScratch.data(), crunchSilhouette);
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (colours::inset);
    g.fillRoundedRectangle (area, 6.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

    auto plot = area.reduced (4.0f, 3.0f);
    const float midY = plot.getCentreY();

    g.setColour (colours::insetLine);
    g.drawHorizontalLine (juce::roundToInt (midY), plot.getX(), plot.getRight());

    // Composite post-chaîne en référence discrète, puis les 3 couches en
    // silhouettes translucides (jaune attack, orange punch, rouge crunch)
    const float halfHeight = plot.getHeight() * 0.5f - 1.0f;
    auto drawSilhouette = [&g, &plot, midY, halfHeight] (const Silhouette& shape,
                                                         juce::Colour colour)
    {
        juce::Path wave;
        for (int col = 0; col < numColumns; ++col)
        {
            const float x = plot.getX() + plot.getWidth() * col / (numColumns - 1);
            const float yTop = midY - shape.high[static_cast<size_t> (col)] * halfHeight;
            if (col == 0)
                wave.startNewSubPath (x, yTop);
            else
                wave.lineTo (x, yTop);
        }
        for (int col = numColumns - 1; col >= 0; --col)
        {
            const float x = plot.getX() + plot.getWidth() * col / (numColumns - 1);
            wave.lineTo (x, midY - shape.low[static_cast<size_t> (col)] * halfHeight);
        }
        wave.closeSubPath();
        g.setColour (colour);
        g.fillPath (wave);
    };

    drawSilhouette (compositeSilhouette, colours::knobRim.withAlpha (0.55f));
    drawSilhouette (crunchSilhouette, colours::drive.withAlpha (0.45f));
    drawSilhouette (punchSilhouette, colours::osc.withAlpha (0.45f));
    drawSilhouette (attackSilhouette, colours::comp.withAlpha (0.55f));

    // Légende
    {
        const std::pair<const char*, juce::Colour> entries[] = {
            { "attack", colours::comp }, { "punch", colours::osc }, { "crunch", colours::drive }
        };
        float x = plot.getRight() - 190.0f;
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        for (const auto& [label, colour] : entries)
        {
            g.setColour (colour);
            g.fillEllipse (x, plot.getY() + 5.0f, 6.0f, 6.0f);
            g.setColour (colours::chipText);
            g.drawText (label, juce::Rectangle<float> (x + 9.0f, plot.getY(), 48.0f, 16.0f),
                        juce::Justification::centredLeft);
            x += 64.0f;
        }
    }

    // Courbe de pitch en pointillés (échelle log 25 Hz - 2 kHz)
    juce::Path pitch;
    const double tau = sweepTimeMs / 1000.0;
    for (int px = 0; px <= 100; ++px)
    {
        const double t = displaySeconds * px / 100.0;
        const float f = pitchEndHz + (pitchStartHz - pitchEndHz)
                                         * static_cast<float> (std::exp (-t / tau));
        const float x = plot.getX() + plot.getWidth() * px / 100.0f;
        const float y = plot.getY() + 2.0f
                        + (plot.getHeight() - 4.0f)
                              * (1.0f - freqToNorm (f, 25.0f, 2000.0f));
        if (px == 0)
            pitch.startNewSubPath (x, y);
        else
            pitch.lineTo (x, y);
    }

    juce::Path dashed;
    const float dashes[] = { 4.0f, 3.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, pitch, dashes, 2);
    g.setColour (colours::env);
    g.fillPath (dashed);

    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("pitch " + juce::String (juce::roundToInt (pitchStartHz))
                    + juce::String::fromUTF8 (" Hz → ")
                    + juce::String (juce::roundToInt (pitchEndHz)) + " Hz",
                getLocalBounds().reduced (10, 4), juce::Justification::topLeft);
}

} // namespace kickforge::ui
