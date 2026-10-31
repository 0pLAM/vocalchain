#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>

class RapChainProcessor : public juce::AudioProcessor
{
public:
    RapChainProcessor();
    ~RapChainProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RapChain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameter + Presets
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::StringArray presetNames();
    void applyPreset (int index);

    juce::AudioProcessorValueTreeState apvts;

    // Pegel fuer die Anzeige (linear, Spitzenwert, wird vom Editor abgeholt)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f }, compReductionDb { 0.0f };

private:
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  on    (const char* id) const { return param (id) > 0.5f; }

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    double sr = 44100.0;

    Filter hpf1[2], hpf2[2];
    Filter eqLow[2], eqMid[2], eqAir[2];
    Filter dsDetect[2];

    juce::dsp::Compressor<float> compressor;
    juce::dsp::Reverb reverb;
    juce::dsp::Limiter<float> limiter;

    // Delay (eigene Ringpuffer, Ping-Pong)
    std::vector<float> delayBuf[2];
    int delayWrite = 0;
    float delayLp[2] { 0.0f, 0.0f };
    juce::SmoothedValue<float> delayTimeSmooth;

    // De-Esser Huellkurve
    float dsEnv = 0.0f;

    juce::AudioBuffer<float> reverbBuf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RapChainProcessor)
};
