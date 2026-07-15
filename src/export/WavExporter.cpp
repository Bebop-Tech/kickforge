#include "WavExporter.h"

namespace kickforge
{

namespace
{
constexpr double exportSampleRate = 48000.0;
constexpr int exportBlockSize = 512;
constexpr double maxLengthSeconds = 3.0;
constexpr double reverbTailSeconds = 2.0;   // queue de la reverb v1 (roomSize fixe)
constexpr double dryReleaseSeconds = 0.15;  // marge après l'extinction des couches
} // namespace

double WavExporter::lengthSecondsFor (const ChainParameters& params)
{
    const double punchSeconds = params.engine.punch.decayMs / 1000.0;
    const double crunchSeconds =
        params.engine.crunch.levelPercent > 0.0f
            ? (params.engine.crunch.attackMs + params.engine.crunch.decayMs) / 1000.0
            : 0.0;
    const double tail = params.fx.reverbMixPercent > 0.0f ? reverbTailSeconds
                                                          : dryReleaseSeconds;
    return juce::jmin (maxLengthSeconds,
                       juce::jmax (punchSeconds, crunchSeconds) + tail);
}

juce::AudioBuffer<float> WavExporter::renderKick (const ChainParameters& params,
                                                  double lengthSeconds)
{
    const int numSamples = juce::roundToInt (lengthSeconds * exportSampleRate);
    juce::AudioBuffer<float> buffer (2, numSamples);
    buffer.clear();

    const juce::dsp::ProcessSpec monoSpec { exportSampleRate, exportBlockSize, 1 };
    const juce::dsp::ProcessSpec stereoSpec { exportSampleRate, exportBlockSize, 2 };

    KickEngine engine;
    FilterStage filter;
    EqStage eq;
    DistStage dist;
    FxStage fx;
    CompStage comp;
    ClipStage clip;
    WidthStage width;

    engine.prepare (monoSpec);
    engine.setParameters (params.engine);
    engine.reset();

    filter.prepare (monoSpec);
    filter.setParameters (params.filter);
    filter.reset();
    eq.prepare (monoSpec);
    eq.setParameters (params.eq);
    eq.reset();
    dist.prepare (monoSpec);
    dist.setParameters (params.dist);
    dist.reset();
    fx.prepare (stereoSpec);
    fx.setParameters (params.fx);
    fx.reset();
    comp.prepare (stereoSpec);
    comp.setParameters (params.comp);
    comp.reset();
    clip.prepare (stereoSpec);
    clip.setParameters (params.clip);
    clip.reset();
    width.prepare (stereoSpec);
    width.setParameters (params.width);
    width.reset();

    engine.noteOn();
    const float outputGain = juce::Decibels::decibelsToGain (params.outputGainDb);

    juce::AudioBuffer<float> busAP (1, exportBlockSize);
    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);

    for (int pos = 0; pos < numSamples; pos += exportBlockSize)
    {
        const int n = juce::jmin (exportBlockSize, numSamples - pos);

        // busAP = attack + punch : c'est lui qui nourrit le send de reverb
        engine.render (busAP.getWritePointer (0), left + pos, n);

        float* monoChannel[] = { left + pos };
        juce::dsp::AudioBlock<float> monoBlock (monoChannel, 1, static_cast<size_t> (n));
        filter.process (monoBlock);
        eq.process (monoBlock);
        dist.process (monoBlock);

        juce::FloatVectorOperations::copy (right + pos, left + pos, n);
        float* stereoChannels[] = { left + pos, right + pos };
        juce::dsp::AudioBlock<float> stereoBlock (stereoChannels, 2,
                                                  static_cast<size_t> (n));
        fx.process (stereoBlock, busAP.getReadPointer (0), static_cast<size_t> (n));
        comp.process (stereoBlock);
        clip.process (stereoBlock);
        width.process (stereoBlock);

        stereoBlock.multiplyBy (outputGain);
    }

    return buffer;
}

bool WavExporter::writeWavFile (const juce::AudioBuffer<float>& buffer, const juce::File& file)
{
    file.deleteFile();
    auto stream = file.createOutputStream();
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.get(), exportSampleRate,
                                static_cast<unsigned int> (buffer.getNumChannels()),
                                24, {}, 0));
    if (writer == nullptr)
        return false;

    stream.release(); // le writer possède désormais le stream
    return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

} // namespace kickforge
