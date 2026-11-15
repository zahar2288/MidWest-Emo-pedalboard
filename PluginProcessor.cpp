#include "PluginProcessor.h"
#include "PluginEditor.h"

using Range = juce::NormalisableRange<float>;
using PID = juce::ParameterID;

juce::AudioProcessorValueTreeState::ParameterLayout EmoBoardProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto flt = [&] (const char* id, const char* name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { id, 1 }, name, Range (lo, hi), def));
    };
    auto onoff = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<juce::AudioParameterBool> (PID { id, 1 }, name, def));
    };

    onoff ("comp_on", "Comp On", true);
    flt ("comp_amt", "Comp Amount", 0.f, 1.f, 0.4f);

    onoff ("drv_on", "Drive On", true);
    flt ("drv_gain", "Drive Gain", 0.f, 1.f, 0.25f);
    flt ("drv_tone", "Drive Tone", 0.f, 1.f, 0.5f);
    flt ("drv_level", "Drive Level", 0.f, 1.f, 0.6f);

    onoff ("cho_on", "Chorus On", true);
    flt ("cho_rate", "Chorus Rate", 0.1f, 5.f, 0.6f);
    flt ("cho_depth", "Chorus Depth", 0.f, 1.f, 0.5f);
    flt ("cho_mix", "Chorus Mix", 0.f, 1.f, 0.4f);

    onoff ("dly_on", "Delay On", true);
    flt ("dly_time", "Delay Time (ms)", 60.f, 900.f, 380.f);
    flt ("dly_fb", "Delay Feedback", 0.f, 0.9f, 0.35f);
    flt ("dly_mix", "Delay Mix", 0.f, 1.f, 0.25f);

    onoff ("rev_on", "Reverb On", true);
    flt ("rev_size", "Reverb Size", 0.f, 1.f, 0.6f);
    flt ("rev_mix", "Reverb Mix", 0.f, 1.f, 0.3f);

    return { p.begin(), p.end() };
}

EmoBoardProcessor::EmoBoardProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

bool EmoBoardProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto in = layouts.getMainInputChannelSet();
    auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void EmoBoardProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& s : state)
    {
        s = ChannelState();
        s.chorusBuf.assign ((size_t) (sr * 0.05) + 4, 0.0f);
        s.delayBuf.assign ((size_t) (sr * 1.2) + 4, 0.0f);
    }
    lfoPhase = 0.0f;
    reverb.setSampleRate (sr);
    reverb.reset();
}

float EmoBoardProcessor::readInterp (const std::vector<float>& buf, int w, float delay)
{
    const int len = (int) buf.size();
    float rp = (float) w - delay;
    while (rp < 0.0f) rp += (float) len;
    int i0 = (int) rp;
    float fr = rp - (float) i0;
    i0 %= len;
    int i1 = (i0 + 1) % len;
    return buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) i1] * fr;
}

void EmoBoardProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = nCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const bool compOn = P ("comp_on") > 0.5f, drvOn = P ("drv_on") > 0.5f,
               choOn = P ("cho_on") > 0.5f, dlyOn = P ("dly_on") > 0.5f,
               revOn = P ("rev_on") > 0.5f;

    // Compressor
    const float cAmt = P ("comp_amt");
    const float thr = -12.0f - cAmt * 18.0f;
    const float makeup = cAmt * 9.0f;
    const float atk = std::exp (-1.0f / (0.005f * (float) sr));
    const float rel = std::exp (-1.0f / (0.12f * (float) sr));

    // Drive
    const float g = P ("drv_gain");
    const float pre = 1.0f + g * g * 40.0f;
    const float dLevel = P ("drv_level") * 1.5f;
    const float toneFc = 600.0f + P ("drv_tone") * 6000.0f;
    const float toneA = std::exp (-juce::MathConstants<float>::twoPi * toneFc / (float) sr);

    // Chorus
    const float cRate = P ("cho_rate"), cDepth = P ("cho_depth"), cMix = P ("cho_mix");

    // Delay
    const float dTarget = P ("dly_time") * 0.001f * (float) sr;
    const float dFb = P ("dly_fb"), dMix = P ("dly_mix");
    const float fbA = std::exp (-juce::MathConstants<float>::twoPi * 3000.0f / (float) sr);

    const float lfoInc = cRate / (float) sr;
    const float twoPi = juce::MathConstants<float>::twoPi;

    for (int ch = 0; ch < nCh; ++ch)
    {
        float* x = buffer.getWritePointer (ch);
        auto& s = state[ch];
        float ph = lfoPhase + 0.5f * (float) ch; // opposite phase on right channel
        const int cLen = (int) s.chorusBuf.size();
        const int dLen = (int) s.delayBuf.size();

        for (int i = 0; i < n; ++i)
        {
            float v = x[i];

            if (compOn)
            {
                float lvl = std::abs (v);
                float coeff = lvl > s.env ? atk : rel;
                s.env = lvl + coeff * (s.env - lvl);
                float db = juce::Decibels::gainToDecibels (s.env + 1.0e-6f);
                float over = db - thr;
                float gainDb = over > 0.0f ? -over * 0.75f : 0.0f; // ratio 4:1
                v *= juce::Decibels::decibelsToGain (gainDb + makeup);
            }

            if (drvOn)
            {
                v = std::tanh (v * pre) * 0.7f;
                s.toneLp = (1.0f - toneA) * v + toneA * s.toneLp;
                v = s.toneLp * dLevel;
            }

            if (choOn)
            {
                s.chorusBuf[(size_t) s.chorusW] = v;
                ph -= std::floor (ph);
                float d = (0.012f + 0.005f * cDepth * std::sin (twoPi * ph)) * (float) sr;
                float wet = readInterp (s.chorusBuf, s.chorusW, d);
                s.chorusW = (s.chorusW + 1) % cLen;
                ph += lfoInc;
                v = v + wet * cMix;
            }

            if (dlyOn)
            {
                s.delayCur += 0.001f * (dTarget - s.delayCur);
                float wet = readInterp (s.delayBuf, s.delayW, juce::jmax (1.0f, s.delayCur));
                s.fbLp = (1.0f - fbA) * wet + fbA * s.fbLp;
                s.delayBuf[(size_t) s.delayW] = v + s.fbLp * dFb;
                s.delayW = (s.delayW + 1) % dLen;
                v = v + wet * dMix;
            }

            x[i] = v;
        }
    }
    lfoPhase += lfoInc * (float) n;
    lfoPhase -= std::floor (lfoPhase);

    if (revOn)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + P ("rev_size") * 0.7f;
        rp.damping = 0.5f;
        rp.wetLevel = P ("rev_mix") * 0.5f;
        rp.dryLevel = 1.0f;
        rp.width = 1.0f;
        reverb.setParameters (rp);
        if (nCh == 2)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
        else if (nCh == 1)
            reverb.processMono (buffer.getWritePointer (0), n);
    }
}

juce::AudioProcessorEditor* EmoBoardProcessor::createEditor()
{
    return new EmoBoardEditor (*this);
}

void EmoBoardProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EmoBoardProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EmoBoardProcessor();
}
