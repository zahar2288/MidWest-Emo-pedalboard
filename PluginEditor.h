#pragma once
#include "PluginProcessor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <vector>

constexpr int kDesignW = 1180;
constexpr int kDesignH = 620;

struct KnobDef
{
    juce::String id, label;
};

void styleKnob (juce::Slider& s, juce::Colour cap, juce::Colour pointer);

//==============================================================================
// Knob bodies are rendered once into cached images; only the value arc and
// pointer are drawn live, which keeps the UI cheap.
class MidwestLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MidwestLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

private:
    struct Cached
    {
        int size;
        juce::Colour cap;
        juce::Image img;
    };
    std::vector<Cached> cache;
    juce::Image knobImage (int size, juce::Colour cap);
};

//==============================================================================
class PowerSwitch : public juce::Button
{
public:
    PowerSwitch();
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
};

//==============================================================================
class Module : public juce::Component
{
public:
    Module (juce::AudioProcessorValueTreeState& apvts, const juce::String& title,
            const juce::String& onId, const std::vector<KnobDef>& knobDefs);

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 136;
    static constexpr int kHeight = 384;

private:
    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    juce::String title;
    PowerSwitch power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAtt;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<juce::String> labels;
    std::vector<juce::Rectangle<float>> labelAreas;
};

//==============================================================================
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter (EmoBoardProcessor& p);
    ~LevelMeter() override { stopTimer(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    int litSegments() const;

    EmoBoardProcessor& proc;
    float level = 0.0f;
    int lastLit = -1;
};

//==============================================================================
class Board : public juce::Component
{
public:
    explicit Board (EmoBoardProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void renderBackground();
    void stepPreset (int delta);

    EmoBoardProcessor& proc;
    juce::Image background;
    std::vector<std::unique_ptr<Module>> modules;

    juce::Slider inKnob, outKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inAtt, outAtt;
    LevelMeter meter;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
};

//==============================================================================
class EmoBoardEditor : public juce::AudioProcessorEditor
{
public:
    explicit EmoBoardEditor (EmoBoardProcessor&);
    ~EmoBoardEditor() override;
    void resized() override;

private:
    MidwestLookAndFeel laf;
    Board board;
};
