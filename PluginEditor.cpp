#include "PluginEditor.h"

namespace
{
    const juce::Colour kBgTop   (0xff1c1f1d);
    const juce::Colour kBgBot   (0xff131514);
    const juce::Colour kCard    (0xff252826);
    const juce::Colour kCream   (0xffe9e2cc);
    const juce::Colour kMuted   (0xff8e8a78);
    const juce::Colour kAccent  (0xffe0742b);
    const juce::Colour kSage    (0xffa3b087);
    const juce::Colour kKnob    (0xff3a3d3a);

    juce::Font makeFont (const char* name, float size, int style, float kerning = 0.0f)
    {
        juce::Font f (juce::FontOptions (name, size, style));
        if (kerning != 0.0f)
            f.setExtraKerningFactor (kerning);
        return f;
    }

    juce::Point<float> polar (juce::Point<float> c, float angle, float radius)
    {
        return { c.x + std::sin (angle) * radius, c.y - std::cos (angle) * radius };
    }

    void drawLeaf (juce::Graphics& g, juce::Point<float> c, float size, float rot, juce::Colour col)
    {
        juce::Path leaf;
        leaf.startNewSubPath (0.0f, -1.0f);
        leaf.cubicTo (0.95f, -0.55f, 0.85f, 0.55f, 0.0f, 1.0f);
        leaf.cubicTo (-0.85f, 0.55f, -0.95f, -0.55f, 0.0f, -1.0f);
        leaf.closeSubPath();
        const auto t = juce::AffineTransform::scale (size * 0.55f, size).rotated (rot).translated (c.x, c.y);
        leaf.applyTransform (t);
        g.setColour (col);
        g.fillPath (leaf);
        juce::Path vein;
        vein.startNewSubPath (0.0f, -0.8f);
        vein.lineTo (0.0f, 1.3f);
        vein.applyTransform (t);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (vein, juce::PathStrokeType (juce::jmax (0.7f, size * 0.07f)));
    }
}

//==============================================================================
void styleKnob (juce::Slider& s, juce::Colour cap, juce::Colour pointer)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s.setMouseDragSensitivity (200);
    s.setColour (juce::Slider::rotarySliderFillColourId, cap);
    s.setColour (juce::Slider::thumbColourId, pointer);
    s.setPopupDisplayEnabled (true, true, nullptr);
}

//==============================================================================
MidwestLookAndFeel::MidwestLookAndFeel()
{
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xf02a2d2b));
    setColour (juce::BubbleComponent::outlineColourId, kCream.withAlpha (0.25f));
    setColour (juce::TooltipWindow::textColourId, kCream);

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff262826));
    setColour (juce::PopupMenu::textColourId, kCream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff17140f));

    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2d2b));
    setColour (juce::ComboBox::textColourId, kCream);
    setColour (juce::ComboBox::outlineColourId, kCream.withAlpha (0.14f));
    setColour (juce::ComboBox::arrowColourId, kCream.withAlpha (0.8f));

    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2d2b));
    setColour (juce::TextButton::textColourOffId, kCream);
    setColour (juce::TextButton::textColourOnId, kCream);
}

juce::Image MidwestLookAndFeel::knobImage (int S, juce::Colour cap)
{
    for (auto& c : cache)
        if (c.size == S && c.cap == cap)
            return c.img;

    const float sc = 3.0f;
    juce::Image img (juce::Image::ARGB, juce::roundToInt ((float) S * sc), juce::roundToInt ((float) S * sc),
                     true, juce::SoftwareImageType());
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (sc));

    const float R = (float) S * 0.5f;
    const juce::Point<float> c (R, R);
    const float kr = R * 0.66f;

    juce::Path body;
    body.addEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.65f), juce::jmax (3, (int) (R * 0.22f)),
                      { 0, juce::jmax (1, (int) (R * 0.08f)) }).drawForPath (g, body);

    juce::ColourGradient face (cap.brighter (0.5f), c.x - kr * 0.4f, c.y - kr * 0.6f,
                               cap.darker (0.6f), c.x + kr * 0.5f, c.y + kr * 0.8f, true);
    g.setGradientFill (face);
    g.fillPath (body);

    // soft concentric machining on the face
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawEllipse (c.x - kr * 0.78f, c.y - kr * 0.78f, kr * 1.56f, kr * 1.56f, 1.0f);
    g.drawEllipse (c.x - kr * 0.5f, c.y - kr * 0.5f, kr, kr, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawEllipse (c.x - kr + 0.5f, c.y - kr + 0.5f, kr * 2.0f - 1.0f, kr * 2.0f - 1.0f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.2f);

    cache.push_back ({ S, cap, img });
    return img;
}

void MidwestLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float a0, float a1, juce::Slider& s)
{
    const int S = juce::jmin (w, h);
    if (S <= 4) return;

    const auto cap = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto pointer = s.findColour (juce::Slider::thumbColourId);

    const float ox = (float) x + (float) (w - S) * 0.5f;
    const float oy = (float) y + (float) (h - S) * 0.5f;

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (knobImage (S, cap), juce::Rectangle<float> (ox, oy, (float) S, (float) S));

    const float R = (float) S * 0.5f;
    const juce::Point<float> c (ox + R, oy + R);
    const float rr = R * 0.9f;
    const float angle = a0 + pos * (a1 - a0);
    const float thick = juce::jmax (2.0f, R * 0.075f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, rr, rr, 0.0f, a0, a1, true);
    g.setColour (kCream.withAlpha (0.14f));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (pos > 0.004f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, rr, rr, 0.0f, a0, angle, true);
        g.setColour (kAccent);
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (pointer);
    g.drawLine (juce::Line<float> (polar (c, angle, R * 0.2f), polar (c, angle, R * 0.56f)),
                juce::jmax (2.0f, R * 0.07f));
}

//==============================================================================
PowerSwitch::PowerSwitch() : juce::Button ("power")
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PowerSwitch::paintButton (juce::Graphics& g, bool, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();

    g.setColour (on ? kAccent.withAlpha (0.9f) : juce::Colour (0xff151716));
    g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (on ? kAccent.brighter (0.3f).withAlpha (0.6f) : kCream.withAlpha (0.16f));
    g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);

    const float d = b.getHeight() - 4.0f;
    const float x = on ? b.getRight() - d - 2.0f : b.getX() + 2.0f;
    g.setColour (on ? juce::Colour (0xfffff3df) : juce::Colour (0xff6d6a5c));
    g.fillEllipse (x, b.getY() + 2.0f, d, d);
}

//==============================================================================
Module::Module (juce::AudioProcessorValueTreeState& apvts, const juce::String& t,
                const juce::String& onId, const std::vector<KnobDef>& defs)
    : title (t)
{
    for (auto& d : defs)
    {
        auto k = std::make_unique<Knob>();
        styleKnob (k->slider, kKnob, kCream);
        k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, d.id, k->slider);
        addAndMakeVisible (k->slider);
        labels.push_back (d.label);
        knobs.push_back (std::move (k));
    }

    addAndMakeVisible (power);
    power.onStateChange = [this]
    {
        const float a = power.getToggleState() ? 1.0f : 0.38f;
        for (auto& k : knobs)
            k->slider.setAlpha (a);
    };
    powerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, onId, power);
    power.onStateChange();

    setSize (kWidth, kHeight);
}

void Module::resized()
{
    power.setBounds (kWidth - 14 - 34, 15, 34, 18);

    const int n = (int) knobs.size();
    const int slot = 82, top = 48, area = kHeight - top - 10;
    const int start = top + (area - n * slot) / 2;

    labelAreas.clear();
    for (int i = 0; i < n; ++i)
    {
        const int D = 58;
        const int y0 = start + i * slot + 4;
        knobs[(size_t) i]->slider.setBounds ((kWidth - D) / 2, y0, D, D);
        labelAreas.push_back (juce::Rectangle<float> (6.0f, (float) (y0 + D + 1), (float) kWidth - 12.0f, 12.0f));
    }
}

void Module::paint (juce::Graphics& g)
{
    g.setFont (makeFont ("Helvetica Neue", 12.0f, juce::Font::bold, 0.2f));
    g.setColour (kCream);
    g.drawText (title, 14, 12, kWidth - 60, 24, juce::Justification::centredLeft, false);

    g.setColour (kCream.withAlpha (0.08f));
    g.drawLine (12.0f, 42.0f, (float) kWidth - 12.0f, 42.0f, 1.0f);

    g.setFont (makeFont ("Helvetica Neue", 9.5f, juce::Font::plain, 0.16f));
    g.setColour (kMuted);
    for (size_t i = 0; i < labelAreas.size() && i < labels.size(); ++i)
        g.drawText (labels[i], labelAreas[i], juce::Justification::centred, false);
}

//==============================================================================
LevelMeter::LevelMeter (EmoBoardProcessor& p) : proc (p)
{
    startTimerHz (20);
}

int LevelMeter::litSegments() const
{
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    int n = 0;
    for (int i = 0; i < 32; ++i)
        if (db >= -48.0f + 1.6f * (float) i) ++n;
    return n;
}

void LevelMeter::timerCallback()
{
    level = juce::jmax (proc.outPeak.load(), level * 0.86f);
    const int lit = litSegments();
    if (lit != lastLit)
    {
        lastLit = lit;
        repaint();
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    const int segs = 32;
    const int lit = litSegments();
    const float w = (float) getWidth() / (float) segs;
    const float h = (float) getHeight();

    for (int i = 0; i < segs; ++i)
    {
        const juce::Colour base = i < 22 ? kSage : (i < 28 ? juce::Colour (0xffe0b13a) : juce::Colour (0xffd5432a));
        const auto r = juce::Rectangle<float> ((float) i * w + 1.0f, 0.0f, w - 2.0f, h);
        g.setColour (i < lit ? base : base.withAlpha (0.14f));
        g.fillRoundedRectangle (r, 1.5f);
    }
}

//==============================================================================
Board::Board (EmoBoardProcessor& p) : proc (p), meter (p)
{
    auto& a = p.apvts;
    using KD = std::vector<KnobDef>;

    modules.push_back (std::make_unique<Module> (a, "GATE", "gate_on",
        KD { { "gate_thr", "THRESHOLD" }, { "gate_rel", "RELEASE" } }));
    modules.push_back (std::make_unique<Module> (a, "COMPRESSOR", "comp_on",
        KD { { "comp_amt", "SUSTAIN" }, { "comp_atk", "ATTACK" }, { "comp_blend", "BLEND" } }));
    modules.push_back (std::make_unique<Module> (a, "DRIVE", "drv_on",
        KD { { "drv_mode", "MODE" }, { "drv_gain", "GAIN" }, { "drv_tone", "TONE" }, { "drv_level", "LEVEL" } }));
    modules.push_back (std::make_unique<Module> (a, "TREMOLO", "trm_on",
        KD { { "trm_rate", "RATE" }, { "trm_depth", "DEPTH" }, { "trm_shape", "SHAPE" } }));
    modules.push_back (std::make_unique<Module> (a, "CHORUS", "cho_on",
        KD { { "cho_rate", "RATE" }, { "cho_depth", "DEPTH" }, { "cho_mix", "MIX" } }));
    modules.push_back (std::make_unique<Module> (a, "DELAY", "dly_on",
        KD { { "dly_time", "TIME" }, { "dly_fb", "REPEATS" }, { "dly_mix", "MIX" }, { "dly_wob", "WOBBLE" } }));
    modules.push_back (std::make_unique<Module> (a, "REVERB", "rev_on",
        KD { { "rev_size", "SIZE" }, { "rev_damp", "DAMP" }, { "rev_pre", "PRE-DELAY" }, { "rev_mix", "MIX" } }));
    modules.push_back (std::make_unique<Module> (a, "TAPE", "tape_on",
        KD { { "tape_wow", "WOW" }, { "tape_dark", "DARK" }, { "tape_warm", "WARMTH" } }));

    for (auto& m : modules)
        addAndMakeVisible (*m);

    styleKnob (inKnob, kKnob, kCream);
    styleKnob (outKnob, kKnob, kCream);
    inAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "in_gain", inKnob);
    outAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, "out_vol", outKnob);
    addAndMakeVisible (inKnob);
    addAndMakeVisible (outKnob);
    addAndMakeVisible (meter);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (juce::jlimit (0, proc.getNumPrograms() - 1, proc.getCurrentProgram()),
                                    juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (presetBox);
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);

    setSize (kDesignW, kDesignH);
}

void Board::stepPreset (int delta)
{
    const int n = proc.getNumPrograms();
    if (n <= 0) return;
    const int cur = juce::jmax (0, presetBox.getSelectedItemIndex());
    presetBox.setSelectedItemIndex ((cur + delta + n) % n, juce::sendNotification);
}

void Board::resized()
{
    for (size_t i = 0; i < modules.size(); ++i)
        modules[i]->setTopLeftPosition (11 + (int) i * 146, 96);

    prevBtn.setBounds (668, 24, 30, 30);
    presetBox.setBounds (702, 24, 330, 30);
    nextBtn.setBounds (1036, 24, 30, 30);

    inKnob.setBounds (34, 510, 64, 64);
    outKnob.setBounds (1082, 510, 64, 64);
    meter.setBounds (330, 532, 520, 16);

    if (background.isNull())
        renderBackground();
}

void Board::renderBackground()
{
    const float sc = 2.0f;
    const float fW = (float) kDesignW, fH = (float) kDesignH;
    background = juce::Image (juce::Image::ARGB, juce::roundToInt (fW * sc), juce::roundToInt (fH * sc),
                              true, juce::SoftwareImageType());
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (sc));
    juce::Random rng (777);

    // base
    juce::ColourGradient bg (kBgTop, 0.0f, 0.0f, kBgBot, 0.0f, fH, false);
    g.setGradientFill (bg);
    g.fillAll();

    // faint warm light from the top-left corner
    juce::ColourGradient warm (kAccent.withAlpha (0.10f), 0.0f, 0.0f, juce::Colours::transparentBlack, 520.0f, 0.0f, true);
    g.setGradientFill (warm);
    g.fillRect (0.0f, 0.0f, 640.0f, 300.0f);

    // fine grain
    for (int i = 0; i < 9000; ++i)
    {
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.035f));
        g.fillRect (rng.nextFloat() * fW, rng.nextFloat() * fH, 1.0f, 1.0f);
    }

    // header
    g.setColour (kCream.withAlpha (0.07f));
    g.drawLine (0.0f, 80.0f, fW, 80.0f, 1.0f);
    drawLeaf (g, { 40.0f, 40.0f }, 13.0f, 0.6f, kAccent);

    // module cards + signal flow
    for (int i = 0; i < 8; ++i)
    {
        const auto r = juce::Rectangle<float> ((float) (11 + i * 146), 96.0f, (float) Module::kWidth, (float) Module::kHeight);
        juce::Path p;
        p.addRoundedRectangle (r, 10.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 12, { 0, 5 }).drawForPath (g, p);

        juce::ColourGradient cg (kCard.brighter (0.06f), 0.0f, r.getY(), kCard.darker (0.25f), 0.0f, r.getBottom(), false);
        g.setGradientFill (cg);
        g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (r, 10.0f, 1.0f);

        if (i < 7)
        {
            const float ax = r.getRight() + 5.0f, ay = r.getY() + 24.0f;
            juce::Path arrow;
            arrow.addTriangle (ax - 2.5f, ay - 3.5f, ax - 2.5f, ay + 3.5f, ax + 3.0f, ay);
            g.setColour (kAccent.withAlpha (0.8f));
            g.fillPath (arrow);
        }
    }

    // bottom plate
    {
        const auto plate = juce::Rectangle<float> (11.0f, 496.0f, fW - 22.0f, 112.0f);
        juce::Path p;
        p.addRoundedRectangle (plate, 10.0f);
        g.setColour (juce::Colour (0xff181a19));
        g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (plate.reduced (0.5f), 10.0f, 1.0f);

        g.setColour (juce::Colour (0xff0b0c0b));
        g.fillRoundedRectangle (320.0f, 526.0f, 540.0f, 28.0f, 5.0f);
        g.setColour (kCream.withAlpha (0.08f));
        g.drawRoundedRectangle (320.0f, 526.0f, 540.0f, 28.0f, 5.0f, 1.0f);
    }
}

void Board::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, getLocalBounds().toFloat());

    // wordmark
    g.setFont (makeFont ("Helvetica Neue", 30.0f, juce::Font::bold, 0.32f));
    g.setColour (kCream);
    g.drawText ("MIDWEST", 66, 18, 300, 36, juce::Justification::centredLeft, false);

    g.setFont (makeFont ("Georgia", 13.0f, juce::Font::italic, 0.04f));
    g.setColour (kMuted);
    g.drawText ("guitar processor for songs about leaving", 68, 52, 420, 18, juce::Justification::centredLeft, false);

    g.setFont (makeFont ("Helvetica Neue", 9.5f, juce::Font::bold, 0.2f));
    g.setColour (kMuted);
    g.drawText ("PRESET", 668, 8, 120, 14, juce::Justification::centredLeft, false);

    // bottom labels
    g.setColour (kMuted);
    g.drawText ("INPUT", inKnob.getX() - 10, inKnob.getBottom() + 2, inKnob.getWidth() + 20, 12, juce::Justification::centred, false);
    g.drawText ("OUTPUT", outKnob.getX() - 10, outKnob.getBottom() + 2, outKnob.getWidth() + 20, 12, juce::Justification::centred, false);
    g.drawText ("OUTPUT LEVEL", meter.getX(), meter.getBottom() + 16, meter.getWidth(), 12, juce::Justification::centred, false);

    g.setFont (makeFont ("Georgia", 12.0f, juce::Font::italic));
    g.setColour (kCream.withAlpha (0.35f));
    g.drawText ("gate  /  compress  /  drive  /  tremolo  /  chorus  /  delay  /  reverb  /  tape",
                140, 580, 900, 16, juce::Justification::centred, false);
}

//==============================================================================
EmoBoardEditor::EmoBoardEditor (EmoBoardProcessor& p)
    : AudioProcessorEditor (&p), board (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (board);

    setResizable (true, true);
    setResizeLimits (708, 372, 1770, 930);
    getConstrainer()->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    setSize (kDesignW, kDesignH);
}

EmoBoardEditor::~EmoBoardEditor()
{
    setLookAndFeel (nullptr);
}

void EmoBoardEditor::resized()
{
    const float s = (float) getWidth() / (float) kDesignW;
    board.setBounds (0, 0, kDesignW, kDesignH);
    board.setTransform (juce::AffineTransform::scale (s));
}
