#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

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

    const juce::String getName() const override { return "EmoBoard"; }
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

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    struct ChannelState
    {
        float env = 0.0f;
        float toneLp = 0.0f;
        float fbLp = 0.0f;
        float delayCur = 0.0f;
        std::vector<float> chorusBuf, delayBuf;
        int chorusW = 0, delayW = 0;
    };

    ChannelState state[2];
    float lfoPhase = 0.0f;
    double sr = 44100.0;
    juce::Reverb reverb;

    static float readInterp (const std::vector<float>& buf, int writePos, float delaySamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EmoBoardProcessor)
};
