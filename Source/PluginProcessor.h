#pragma once

#include <JuceHeader.h>
#include <array>
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

    // Read by the editor: vactrol "light" level (0..1) and the live filter state
    // (current cutoff Hz + Q) that drives the response-curve display.
    std::atomic<float> envelopeOut  { 0.0f };
    std::atomic<float> cutoffOut    { 1000.0f };
    std::atomic<float> resonanceOut { 2.0f };

    // Spectrum-analyzer feed: the audio thread fills a FIFO with the output
    // signal; the editor pulls a block and runs the FFT for the scope.
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder; // 2048
    std::atomic<bool> fftReady { false };
    const float* fftBlock() const noexcept { return fftLatest.data(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    zt::MuTronEngine engine;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> dryBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassSmoothed;

    juce::RangedAudioParameter* bypassParam = nullptr;

    std::array<float, (size_t) fftSize> fftFifo {};
    std::array<float, (size_t) fftSize> fftLatest {};
    int fifoIndex = 0;
    inline void pushSampleForFFT (float s) noexcept
    {
        if (fifoIndex == fftSize)
        {
            if (! fftReady.load())
            {
                std::copy (fftFifo.begin(), fftFifo.end(), fftLatest.begin());
                fftReady.store (true);
            }
            fifoIndex = 0;
        }
        fftFifo[(size_t) fifoIndex++] = s;
    }

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
    std::atomic<float>* pContour   = nullptr;
    std::atomic<float>* pSource    = nullptr;
    std::atomic<float>* pLfoRate   = nullptr;
    std::atomic<float>* pLfoShape  = nullptr;
    std::atomic<float>* pLfoSync   = nullptr;
    std::atomic<float>* pWidth     = nullptr;
    juce::RangedAudioParameter* rateParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZooTronAudioProcessor)
};
