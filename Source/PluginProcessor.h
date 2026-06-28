#pragma once

#include <JuceHeader.h>
#include "dsp/MuTronEngine.h"
#include "PresetManager.h"

class ZooTronAudioProcessor : public juce::AudioProcessor
{
public:
    ZooTronAudioProcessor();
    ~ZooTronAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Zoo-Tron III"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presetManager { apvts };

    // Read by the editor's LED meter (vactrol "light" level, 0..1).
    std::atomic<float> envelopeOut { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    zt::MuTronEngine engine;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> dryBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassSmoothed;

    juce::RangedAudioParameter* bypassParam = nullptr;

    // Cached raw parameter pointers.
    std::atomic<float>* pGain      = nullptr;
    std::atomic<float>* pPeak      = nullptr;
    std::atomic<float>* pRange     = nullptr;
    std::atomic<float>* pMode      = nullptr;
    std::atomic<float>* pDirection = nullptr;
    std::atomic<float>* pOutput    = nullptr;
    std::atomic<float>* pMix       = nullptr;
    std::atomic<float>* pBypass    = nullptr;
    std::atomic<float>* pAttack    = nullptr;
    std::atomic<float>* pRelease   = nullptr;
    std::atomic<float>* pDrive     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZooTronAudioProcessor)
};
