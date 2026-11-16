#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>

class EmoBoardProcessor : public juce::AudioProcessor
{
public:
    EmoBoardProcessor();
    ~EmoBoardProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Midwest"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // peak level of the last processed block (read by the UI meter)
    std::atomic<float> outPeak { 0.0f };

private:
    struct ChannelState
    {
        float gateEnv = 0.0f, gateG = 1.0f;     // noise gate
        float env = 0.0f;                        // compressor envelope
        float toneLp = 0.0f;                     // drive tone filter
        float dcX = 0.0f, dcY = 0.0f;            // drive DC blocker
        float fbLp = 0.0f, delayCur = 0.0f;      // delay feedback filter / smoothed time
        float tapeLp = 0.0f;                     // tape darkness filter
        std::vector<float> chorusBuf, delayBuf, tapeBuf, preBuf;
        int chorusW = 0, delayW = 0, tapeW = 0, preW = 0;
    };

    int currentPreset = 0;
    ChannelState state[2];

    float trmPhase = 0.0f, lfoPhase = 0.0f;
    float wobPhase = 0.0f, wobPhase2 = 0.0f;
    float tapePhase1 = 0.0f, tapePhase2 = 0.0f;
    float prevIn = 1.0f, prevOut = 1.0f;
    double sr = 44100.0;

    juce::Reverb reverb;
    juce::AudioBuffer<float> reverbBuf;

    static float readInterp (const std::vector<float>& buf, int writePos, float delaySamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmoBoardProcessor)
};
