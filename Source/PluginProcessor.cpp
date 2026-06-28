#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    constexpr auto gain      = "gain";
    constexpr auto peak      = "peak";
    constexpr auto range     = "range";
    constexpr auto mode      = "mode";
    constexpr auto direction = "direction";
    constexpr auto output    = "output";
    constexpr auto mix       = "mix";
    constexpr auto bypass    = "bypass";
    constexpr auto attack    = "attack";
    constexpr auto release   = "release";
    constexpr auto drive     = "drive";
}

ZooTronAudioProcessor::ZooTronAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pGain      = apvts.getRawParameterValue (ids::gain);
    pPeak      = apvts.getRawParameterValue (ids::peak);
    pRange     = apvts.getRawParameterValue (ids::range);
    pMode      = apvts.getRawParameterValue (ids::mode);
    pDirection = apvts.getRawParameterValue (ids::direction);
    pOutput    = apvts.getRawParameterValue (ids::output);
    pMix       = apvts.getRawParameterValue (ids::mix);
    pBypass    = apvts.getRawParameterValue (ids::bypass);
    pAttack    = apvts.getRawParameterValue (ids::attack);
    pRelease   = apvts.getRawParameterValue (ids::release);
    pDrive     = apvts.getRawParameterValue (ids::drive);
    bypassParam = apvts.getParameter (ids::bypass);
}

juce::AudioProcessorValueTreeState::ParameterLayout
ZooTronAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::gain, 1 }, "Gain",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.5f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::peak, 1 }, "Peak",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 6.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::range, 1 }, "Range",
        StringArray { "Low", "High" }, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::mode, 1 }, "Mode",
        StringArray { "Low Pass", "Band Pass", "High Pass" }, 1));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ids::direction, 1 }, "Direction",
        StringArray { "Up", "Down" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::output, 1 }, "Output",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { ids::bypass, 1 }, "Bypass", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::attack, 1 }, "Attack",
        NormalisableRange<float> (1.0f, 50.0f, 0.1f, 0.4f), 8.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::release, 1 }, "Release",
        NormalisableRange<float> (20.0f, 600.0f, 1.0f, 0.45f), 180.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ids::drive, 1 }, "Drive",
        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 2.0f));

    return layout;
}

void ZooTronAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jmax (1, getTotalNumOutputChannels());

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, 1, // 1 stage = 2x
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples ((int) oversampler->getLatencyInSamples());

    engine.prepare (sampleRate * 2.0, numCh); // engine runs on the oversampled stream

    dryBuffer.setSize (numCh, samplesPerBlock);

    outGainSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    bypassSmoothed.reset (sampleRate, 0.015);
    outGainSmoothed.setCurrentAndTargetValue (1.0f);
    mixSmoothed.setCurrentAndTargetValue (1.0f);
    bypassSmoothed.setCurrentAndTargetValue (pBypass->load() >= 0.5f ? 1.0f : 0.0f);
}

bool ZooTronAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

void ZooTronAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Push current parameter values into the engine.
    engine.setSensitivity (pGain->load());
    engine.setPeak (pPeak->load());
    engine.setRange (pRange->load() < 0.5f ? zt::Range::Low : zt::Range::High);
    engine.setMode (static_cast<zt::Mode> ((int) pMode->load()));
    engine.setDirection (pDirection->load() < 0.5f ? zt::Direction::Up : zt::Direction::Down);
    engine.setAttack (pAttack->load());
    engine.setRelease (pRelease->load());
    engine.setDrive (pDrive->load());

    outGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    mixSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, pMix->load() * 0.01f));
    bypassSmoothed.setTargetValue (pBypass->load() >= 0.5f ? 1.0f : 0.0f);

    // Stash the dry signal for the wet/dry mix.
    dryBuffer.makeCopyOf (buffer, true);

    // --- wet path: 2x oversampled filter + saturation -----------------------
    juce::dsp::AudioBlock<float> block (buffer);
    auto osBlock = oversampler->processSamplesUp (block);

    float* chans[2] = { nullptr, nullptr };
    const int osCh = (int) juce::jmin ((size_t) 2, osBlock.getNumChannels());
    for (int ch = 0; ch < osCh; ++ch)
        chans[ch] = osBlock.getChannelPointer ((size_t) ch);

    engine.process (chans, osCh, (int) osBlock.getNumSamples());
    oversampler->processSamplesDown (block);

    // --- wet/dry mix + output trim + bypass crossfade -----------------------
    for (int n = 0; n < numSamples; ++n)
    {
        const float g  = outGainSmoothed.getNextValue();
        const float wf = mixSmoothed.getNextValue();
        const float b  = bypassSmoothed.getNextValue(); // 0 = engaged, 1 = bypassed

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float dry = dryBuffer.getSample (ch, n);
            const float wet = buffer.getSample (ch, n);
            const float fx  = (dry * (1.0f - wf) + wet * wf) * g;
            buffer.setSample (ch, n, fx * (1.0f - b) + dry * b);
        }
    }

    envelopeOut.store (engine.lastEnvelope());
}

juce::AudioProcessorEditor* ZooTronAudioProcessor::createEditor()
{
    return new ZooTronAudioProcessorEditor (*this);
}

void ZooTronAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        juce::MemoryOutputStream mos (destData, true);
        state.writeToStream (mos);
    }
}

void ZooTronAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZooTronAudioProcessor();
}
