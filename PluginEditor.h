#pragma once
#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>

class Pedal : public juce::Component
{
public:
    Pedal (juce::AudioProcessorValueTreeState& apvts,
           const juce::String& title,
           const juce::String& onId,
           const std::vector<std::pair<juce::String, juce::String>>& knobDefs,
           juce::Colour colour);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    juce::String title;
    juce::Colour colour;
    juce::ToggleButton toggle { "ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAtt;
    std::vector<std::unique_ptr<Knob>> knobs;
};

class EmoBoardEditor : public juce::AudioProcessorEditor
{
public:
    explicit EmoBoardEditor (EmoBoardProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    std::vector<std::unique_ptr<Pedal>> pedals;
};
