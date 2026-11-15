#include "PluginEditor.h"

Pedal::Pedal (juce::AudioProcessorValueTreeState& apvts,
              const juce::String& t,
              const juce::String& onId,
              const std::vector<std::pair<juce::String, juce::String>>& knobDefs,
              juce::Colour c)
    : title (t), colour (c)
{
    addAndMakeVisible (toggle);
    toggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    toggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::white);
    toggleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, onId, toggle);

    for (auto& def : knobDefs)
    {
        auto k = std::make_unique<Knob>();
        k->slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        k->slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        k->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xfff2e8cf));
        k->slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        k->label.setText (def.second, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setColour (juce::Label::textColourId, juce::Colours::white);
        k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, def.first, k->slider);
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        knobs.push_back (std::move (k));
    }
}

void Pedal::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (b, 10.0f, 2.0f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText (title, getLocalBounds().reduced (14, 10).removeFromTop (26).withTrimmedRight (50),
                juce::Justification::centredLeft);
}

void Pedal::resized()
{
    auto a = getLocalBounds().reduced (10);
    auto head = a.removeFromTop (34);
    toggle.setBounds (head.removeFromRight (54));
    for (auto& k : knobs)
    {
        auto cell = a.removeFromTop (88);
        k->label.setBounds (cell.removeFromTop (14));
        k->slider.setBounds (cell);
    }
}

EmoBoardEditor::EmoBoardEditor (EmoBoardProcessor& p) : AudioProcessorEditor (&p)
{
    auto& a = p.apvts;
    using KD = std::vector<std::pair<juce::String, juce::String>>;

    pedals.push_back (std::make_unique<Pedal> (a, "COMP", "comp_on",
        KD { { "comp_amt", "Amount" } }, juce::Colour (0xff3b6d8c)));
    pedals.push_back (std::make_unique<Pedal> (a, "DRIVE", "drv_on",
        KD { { "drv_gain", "Gain" }, { "drv_tone", "Tone" }, { "drv_level", "Level" } },
        juce::Colour (0xff9c4f3d)));
    pedals.push_back (std::make_unique<Pedal> (a, "CHORUS", "cho_on",
        KD { { "cho_rate", "Rate" }, { "cho_depth", "Depth" }, { "cho_mix", "Mix" } },
        juce::Colour (0xff4a7c59)));
    pedals.push_back (std::make_unique<Pedal> (a, "DELAY", "dly_on",
        KD { { "dly_time", "Time" }, { "dly_fb", "Feedback" }, { "dly_mix", "Mix" } },
        juce::Colour (0xff7a5c8f)));
    pedals.push_back (std::make_unique<Pedal> (a, "REVERB", "rev_on",
        KD { { "rev_size", "Size" }, { "rev_mix", "Mix" } }, juce::Colour (0xff8a7a3d)));

    for (auto& pd : pedals) addAndMakeVisible (*pd);
    setSize (780, 360);
}

void EmoBoardEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1b18));
    g.setColour (juce::Colour (0xfff2e8cf));
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("EMOBOARD  -  midwest emo pedalboard", 16, 8, 600, 30, juce::Justification::centredLeft);
}

void EmoBoardEditor::resized()
{
    auto a = getLocalBounds().withTrimmedTop (44).reduced (8, 4);
    const int w = a.getWidth() / (int) pedals.size();
    for (auto& pd : pedals)
        pd->setBounds (a.removeFromLeft (w));
}
