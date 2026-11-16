#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <functional>

namespace
{
    using Range = juce::NormalisableRange<float>;
    using PID = juce::ParameterID;
    using Fmt = std::function<juce::String (float, int)>;

    juce::String fmtPct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    juce::String fmtDb  (float v, int) { return juce::String (v, 1) + " dB"; }
    juce::String fmtMs  (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; }
    juce::String fmtHz  (float v, int) { return juce::String (v, 2) + " Hz"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout EmoBoardProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto flt = [&] (const char* id, const char* name, float lo, float hi, float def, Fmt f)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            PID { id, 1 }, name, Range (lo, hi), def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (f))));
    };
    auto onoff = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<juce::AudioParameterBool> (PID { id, 1 }, name, def));
    };

    // master
    flt ("in_gain", "Input Gain", -12.f, 12.f, 0.f, fmtDb);
    flt ("out_vol", "Output Volume", -24.f, 6.f, 0.f, fmtDb);

    // HUSH - noise gate
    onoff ("gate_on", "Hush On", false);
    flt ("gate_thr", "Hush Threshold", -80.f, -20.f, -58.f, fmtDb);
    flt ("gate_rel", "Hush Release", 20.f, 800.f, 160.f, fmtMs);

    // SQUEEZE - compressor
    onoff ("comp_on", "Squeeze On", true);
    flt ("comp_amt", "Squeeze Sustain", 0.f, 1.f, 0.4f, fmtPct);
    flt ("comp_atk", "Squeeze Attack", 1.f, 60.f, 10.f, fmtMs);
    flt ("comp_blend", "Squeeze Blend", 0.f, 1.f, 0.85f, fmtPct);

    // RUST - overdrive / fuzz / boost
    onoff ("drv_on", "Rust On", true);
    p.push_back (std::make_unique<juce::AudioParameterChoice> (PID { "drv_mode", 1 }, "Rust Mode",
                                                               juce::StringArray { "Overdrive", "Fuzz", "Boost" }, 0));
    flt ("drv_gain", "Rust Gain", 0.f, 1.f, 0.25f, fmtPct);
    flt ("drv_tone", "Rust Tone", 0.f, 1.f, 0.5f, fmtPct);
    flt ("drv_level", "Rust Level", 0.f, 1.f, 0.6f, fmtPct);

    // FLICKER - tremolo
    onoff ("trm_on", "Flicker On", false);
    flt ("trm_rate", "Flicker Rate", 0.5f, 10.f, 4.f, fmtHz);
    flt ("trm_depth", "Flicker Depth", 0.f, 1.f, 0.5f, fmtPct);
    flt ("trm_shape", "Flicker Shape", 0.f, 1.f, 0.2f, fmtPct);

    // DRIFT - chorus
    onoff ("cho_on", "Drift On", true);
    flt ("cho_rate", "Drift Rate", 0.1f, 5.f, 0.6f, fmtHz);
    flt ("cho_depth", "Drift Depth", 0.f, 1.f, 0.5f, fmtPct);
    flt ("cho_mix", "Drift Mix", 0.f, 1.f, 0.4f, fmtPct);

    // ECHO HOLLOW - delay
    onoff ("dly_on", "Echo On", true);
    flt ("dly_time", "Echo Time", 60.f, 900.f, 380.f, fmtMs);
    flt ("dly_fb", "Echo Repeats", 0.f, 0.9f, 0.35f, fmtPct);
    flt ("dly_mix", "Echo Mix", 0.f, 1.f, 0.25f, fmtPct);
    flt ("dly_wob", "Echo Wobble", 0.f, 1.f, 0.15f, fmtPct);

    // NOVEMBER - reverb
    onoff ("rev_on", "November On", true);
    flt ("rev_size", "November Size", 0.f, 1.f, 0.6f, fmtPct);
    flt ("rev_damp", "November Damp", 0.f, 1.f, 0.5f, fmtPct);
    flt ("rev_pre", "November Pre-Delay", 0.f, 120.f, 20.f, fmtMs);
    flt ("rev_mix", "November Mix", 0.f, 1.f, 0.3f, fmtPct);

    // CASSETTE - tape
    onoff ("tape_on", "Cassette On", true);
    flt ("tape_wow", "Cassette Wow", 0.f, 1.f, 0.15f, fmtPct);
    flt ("tape_dark", "Cassette Dark", 0.f, 1.f, 0.3f, fmtPct);
    flt ("tape_warm", "Cassette Warmth", 0.f, 1.f, 0.25f, fmtPct);

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

void EmoBoardProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto& s : state)
    {
        s = ChannelState();
        s.chorusBuf.assign ((size_t) (sr * 0.05) + 4, 0.0f);
        s.delayBuf.assign ((size_t) (sr * 1.2) + 4, 0.0f);
        s.tapeBuf.assign ((size_t) (sr * 0.03) + 4, 0.0f);
        s.preBuf.assign ((size_t) (sr * 0.2) + 4, 0.0f);
    }
    trmPhase = lfoPhase = wobPhase = wobPhase2 = tapePhase1 = tapePhase2 = 0.0f;

    prevIn = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("in_gain")->load());
    prevOut = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out_vol")->load());

    reverbBuf.setSize (2, juce::jmax (1, samplesPerBlock));
    reverbBuf.clear();
    reverb.setSampleRate (sr);
    reverb.reset();
}

float EmoBoardProcessor::readInterp (const std::vector<float>& buf, int w, float delay)
{
    const int len = (int) buf.size();
    float rp = (float) w - delay;
    while (rp < 0.0f) rp += (float) len;
    int i0 = (int) rp;
    const float fr = rp - (float) i0;
    i0 %= len;
    const int i1 = (i0 + 1) % len;
    return buf[(size_t) i0] * (1.0f - fr) + buf[(size_t) i1] * fr;
}

void EmoBoardProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    for (int c = nCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    if (nCh == 0 || n == 0) return;

    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float fs = (float) sr;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // ---------------- input gain ----------------
    const float inTarget = juce::Decibels::decibelsToGain (P ("in_gain"));
    for (int ch = 0; ch < nCh; ++ch)
        buffer.applyGainRamp (ch, 0, n, prevIn, inTarget);
    prevIn = inTarget;

    const bool gateOn = P ("gate_on") > 0.5f, compOn = P ("comp_on") > 0.5f,
               drvOn = P ("drv_on") > 0.5f, trmOn = P ("trm_on") > 0.5f,
               choOn = P ("cho_on") > 0.5f, dlyOn = P ("dly_on") > 0.5f,
               revOn = P ("rev_on") > 0.5f, tapeOn = P ("tape_on") > 0.5f;

    // gate
    const float gThr = juce::Decibels::decibelsToGain (P ("gate_thr"));
    const float gRel = std::exp (-1.0f / (P ("gate_rel") * 0.001f * fs));
    const float gAtk = std::exp (-1.0f / (0.001f * fs));
    const float gEnvC = std::exp (-1.0f / (0.01f * fs));

    // compressor
    const float cAmt = P ("comp_amt"), cBlend = P ("comp_blend");
    const float cThr = -10.0f - cAmt * 26.0f;
    const float cMakeup = cAmt * 10.0f;
    const float cAtk = std::exp (-1.0f / (P ("comp_atk") * 0.001f * fs));
    const float cRel = std::exp (-1.0f / (0.15f * fs));

    // drive
    const int mode = (int) std::lround (P ("drv_mode"));
    const float g = P ("drv_gain");
    const float pre = 1.0f + g * g * 40.0f;
    const float fuzzPre = 3.0f + g * 80.0f;
    const float boostPre = 1.0f + g * 3.5f;
    const float dLevel = P ("drv_level") * 1.5f;
    const float toneFc = 600.0f + P ("drv_tone") * 6000.0f;
    const float toneA = std::exp (-twoPi * toneFc / fs);

    // tremolo
    const float tDepth = P ("trm_depth");
    const float tK = 0.1f + P ("trm_shape") * 9.0f;
    const float tTanhK = std::tanh (tK);
    const float tInc = P ("trm_rate") / fs;

    // chorus
    const float cRate = P ("cho_rate"), cDepth = P ("cho_depth"), cMix = P ("cho_mix");
    const float lfoInc = cRate / fs;

    // delay
    const float dTarget = P ("dly_time") * 0.001f * fs;
    const float dFb = P ("dly_fb"), dMix = P ("dly_mix");
    const float dWob = P ("dly_wob") * 0.004f * fs;
    const float fbA = std::exp (-twoPi * 3000.0f / fs);
    const float wobInc = 0.55f / fs, wobInc2 = 1.9f / fs;

    // ---------------- per-channel chain ----------------
    for (int ch = 0; ch < nCh; ++ch)
    {
        float* x = buffer.getWritePointer (ch);
        auto& s = state[ch];
        float ph = lfoPhase + 0.5f * (float) ch;
        float tp = trmPhase;
        float wp = wobPhase + 0.2f * (float) ch;
        float wp2 = wobPhase2 + 0.37f * (float) ch;
        const int cLen = (int) s.chorusBuf.size();
        const int dLen = (int) s.delayBuf.size();

        for (int i = 0; i < n; ++i)
        {
            float v = x[i];

            if (gateOn)
            {
                const float lvl = std::abs (v);
                s.gateEnv = lvl > s.gateEnv ? lvl : lvl + gEnvC * (s.gateEnv - lvl);
                const float target = s.gateEnv > gThr ? 1.0f : 0.0f;
                const float c = target > s.gateG ? gAtk : gRel;
                s.gateG = target + c * (s.gateG - target);
                v *= s.gateG;
            }

            if (compOn)
            {
                const float dry = v;
                const float lvl = std::abs (v);
                const float coeff = lvl > s.env ? cAtk : cRel;
                s.env = lvl + coeff * (s.env - lvl);
                const float dbv = juce::Decibels::gainToDecibels (s.env + 1.0e-6f);
                const float over = dbv - cThr;
                const float gainDb = over > 0.0f ? -over * 0.75f : 0.0f; // 4:1
                const float wet = v * juce::Decibels::decibelsToGain (gainDb + cMakeup);
                v = dry + (wet - dry) * cBlend;
            }

            if (drvOn)
            {
                float y;
                if (mode == 1)
                {
                    const float z = v * fuzzPre + 0.15f;
                    y = (z / (1.0f + std::abs (z) * 1.5f)) * 0.9f;
                }
                else if (mode == 2)
                {
                    y = std::tanh (v * boostPre) * 0.9f;
                }
                else
                {
                    y = std::tanh (v * pre) * 0.7f;
                }

                const float dc = y - s.dcX + 0.995f * s.dcY;
                s.dcX = y;
                s.dcY = dc;

                s.toneLp = (1.0f - toneA) * dc + toneA * s.toneLp;
                v = s.toneLp * dLevel;
            }

            if (trmOn)
            {
                const float sn = std::sin (twoPi * tp);
                const float shaped = std::tanh (tK * sn) / tTanhK;
                const float lfo = 0.5f + 0.5f * shaped;
                v *= 1.0f - tDepth * lfo;
                tp += tInc;
                if (tp >= 1.0f) tp -= 1.0f;
            }

            if (choOn)
            {
                s.chorusBuf[(size_t) s.chorusW] = v;
                ph -= std::floor (ph);
                const float d = (0.012f + 0.005f * cDepth * std::sin (twoPi * ph)) * fs;
                const float wet = readInterp (s.chorusBuf, s.chorusW, d);
                s.chorusW = (s.chorusW + 1) % cLen;
                ph += lfoInc;
                v = v + wet * cMix;
            }

            if (dlyOn)
            {
                s.delayCur += 0.001f * (dTarget - s.delayCur);
                const float mod = dWob * (std::sin (twoPi * wp) + 0.3f * std::sin (twoPi * wp2));
                float d = s.delayCur + mod;
                d = juce::jlimit (1.0f, (float) dLen - 3.0f, d);
                const float wet = readInterp (s.delayBuf, s.delayW, d);
                s.fbLp = (1.0f - fbA) * wet + fbA * s.fbLp;
                s.delayBuf[(size_t) s.delayW] = v + std::tanh (s.fbLp * dFb);
                s.delayW = (s.delayW + 1) % dLen;
                v = v + wet * dMix;
                wp += wobInc;   if (wp >= 1.0f) wp -= 1.0f;
                wp2 += wobInc2; if (wp2 >= 1.0f) wp2 -= 1.0f;
            }

            x[i] = v;
        }
    }

    lfoPhase += lfoInc * (float) n;   lfoPhase -= std::floor (lfoPhase);
    trmPhase += tInc * (float) n;     trmPhase -= std::floor (trmPhase);
    wobPhase += wobInc * (float) n;   wobPhase -= std::floor (wobPhase);
    wobPhase2 += wobInc2 * (float) n; wobPhase2 -= std::floor (wobPhase2);

    // ---------------- NOVEMBER reverb (parallel, with pre-delay) ----------------
    if (revOn)
    {
        if (reverbBuf.getNumSamples() < n || reverbBuf.getNumChannels() < 2)
            reverbBuf.setSize (2, n, false, false, true);

        const int preSamples = (int) (P ("rev_pre") * 0.001f * fs);

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float* in = buffer.getReadPointer (ch);
            float* rb = reverbBuf.getWritePointer (ch);
            auto& s = state[ch];
            const int pLen = (int) s.preBuf.size();
            for (int i = 0; i < n; ++i)
            {
                s.preBuf[(size_t) s.preW] = in[i];
                int r = s.preW - preSamples;
                if (r < 0) r += pLen;
                rb[i] = s.preBuf[(size_t) r];
                s.preW = (s.preW + 1) % pLen;
            }
        }

        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + P ("rev_size") * 0.7f;
        rp.damping = P ("rev_damp");
        rp.wetLevel = 0.33f;
        rp.dryLevel = 0.0f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);

        if (nCh == 2)
            reverb.processStereo (reverbBuf.getWritePointer (0), reverbBuf.getWritePointer (1), n);
        else
            reverb.processMono (reverbBuf.getWritePointer (0), n);

        const float rMix = P ("rev_mix") * 1.3f;
        for (int ch = 0; ch < nCh; ++ch)
            buffer.addFrom (ch, 0, reverbBuf, ch, 0, n, rMix);
    }

    // ---------------- CASSETTE tape (wow/flutter, darkness, warmth) ----------------
    if (tapeOn)
    {
        const float wow = P ("tape_wow");
        const float warm = P ("tape_warm");
        const float dark = P ("tape_dark");
        const float fc = juce::jmin (0.45f * fs, 18000.0f * std::pow (2000.0f / 18000.0f, dark));
        const float lpA = std::exp (-twoPi * fc / fs);
        const float baseD = 0.004f * fs;
        const float depthS = wow * 0.0035f * fs;
        const float inc1 = 0.7f / fs, inc2 = 6.1f / fs;

        for (int ch = 0; ch < nCh; ++ch)
        {
            float* x = buffer.getWritePointer (ch);
            auto& s = state[ch];
            const int tLen = (int) s.tapeBuf.size();
            float t1 = tapePhase1 + 0.13f * (float) ch;
            float t2 = tapePhase2 + 0.29f * (float) ch;
            t1 -= std::floor (t1);
            t2 -= std::floor (t2);

            for (int i = 0; i < n; ++i)
            {
                float v = x[i];

                const float sat = std::tanh (1.8f * v) / 1.8f;
                v = v + warm * (sat - v);

                s.tapeBuf[(size_t) s.tapeW] = v;
                float d = baseD + depthS * (0.8f * std::sin (twoPi * t1) + 0.25f * std::sin (twoPi * t2));
                d = juce::jlimit (1.0f, (float) tLen - 3.0f, d);
                v = readInterp (s.tapeBuf, s.tapeW, d);
                s.tapeW = (s.tapeW + 1) % tLen;
                t1 += inc1; if (t1 >= 1.0f) t1 -= 1.0f;
                t2 += inc2; if (t2 >= 1.0f) t2 -= 1.0f;

                s.tapeLp = (1.0f - lpA) * v + lpA * s.tapeLp;
                x[i] = s.tapeLp;
            }
        }
        tapePhase1 += inc1 * (float) n; tapePhase1 -= std::floor (tapePhase1);
        tapePhase2 += inc2 * (float) n; tapePhase2 -= std::floor (tapePhase2);
    }

    // ---------------- output volume + meter ----------------
    const float outTarget = juce::Decibels::decibelsToGain (P ("out_vol"));
    for (int ch = 0; ch < nCh; ++ch)
        buffer.applyGainRamp (ch, 0, n, prevOut, outTarget);
    prevOut = outTarget;

    float peak = 0.0f;
    for (int ch = 0; ch < nCh; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, n));
    outPeak.store (peak);
}


namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };

    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p = {
            { "Init - Clean Twinkle", {} },

            { "American Football style", {
                { "comp_on", 1 }, { "comp_amt", 0.3f }, { "drv_on", 0 },
                { "cho_on", 1 }, { "cho_rate", 0.5f }, { "cho_depth", 0.4f }, { "cho_mix", 0.3f },
                { "dly_on", 1 }, { "dly_time", 330 }, { "dly_fb", 0.3f }, { "dly_mix", 0.2f },
                { "rev_on", 1 }, { "rev_size", 0.55f }, { "rev_mix", 0.28f },
                { "tape_on", 1 }, { "tape_warm", 0.3f } } },

            { "Cap'n Jazz style", {
                { "comp_amt", 0.25f }, { "drv_on", 1 }, { "drv_mode", 0 }, { "drv_gain", 0.5f }, { "drv_tone", 0.7f }, { "drv_level", 0.65f },
                { "cho_on", 0 }, { "dly_time", 180 }, { "dly_fb", 0.2f }, { "dly_mix", 0.12f },
                { "rev_size", 0.25f }, { "rev_mix", 0.15f }, { "tape_wow", 0.1f } } },

            { "Mineral style", {
                { "comp_amt", 0.45f }, { "drv_gain", 0.45f }, { "drv_tone", 0.4f }, { "drv_level", 0.65f },
                { "cho_mix", 0.2f }, { "dly_time", 420 }, { "dly_fb", 0.45f }, { "dly_mix", 0.3f },
                { "rev_size", 0.8f }, { "rev_damp", 0.6f }, { "rev_pre", 40 }, { "rev_mix", 0.4f } } },

            { "Sunny Day Real Estate style", {
                { "comp_amt", 0.5f }, { "drv_gain", 0.6f }, { "drv_tone", 0.55f }, { "drv_level", 0.7f },
                { "cho_mix", 0.25f }, { "dly_time", 450 }, { "dly_fb", 0.4f }, { "dly_mix", 0.25f },
                { "rev_size", 0.7f }, { "rev_mix", 0.35f } } },

            { "The Promise Ring style", {
                { "drv_gain", 0.3f }, { "drv_tone", 0.8f }, { "cho_rate", 0.8f }, { "cho_mix", 0.3f },
                { "dly_time", 250 }, { "dly_fb", 0.25f }, { "dly_mix", 0.15f },
                { "rev_size", 0.4f }, { "rev_mix", 0.2f }, { "tape_wow", 0.1f }, { "tape_dark", 0.1f } } },

            { "Algernon Cadwallader style", {
                { "comp_amt", 0.35f }, { "drv_gain", 0.55f }, { "drv_tone", 0.75f }, { "drv_level", 0.7f },
                { "cho_on", 0 }, { "dly_time", 200 }, { "dly_fb", 0.2f }, { "dly_mix", 0.1f },
                { "rev_size", 0.3f }, { "rev_mix", 0.15f }, { "tape_on", 0 } } },

            { "Snowing style", {
                { "drv_gain", 0.55f }, { "drv_tone", 0.6f }, { "drv_level", 0.65f },
                { "cho_mix", 0.15f }, { "dly_time", 320 }, { "dly_fb", 0.3f }, { "dly_mix", 0.2f },
                { "rev_size", 0.5f }, { "rev_mix", 0.25f } } },

            { "Foxing style - Ethereal", {
                { "drv_gain", 0.2f }, { "cho_depth", 0.7f }, { "cho_mix", 0.5f },
                { "dly_time", 500 }, { "dly_fb", 0.55f }, { "dly_mix", 0.35f },
                { "rev_size", 0.85f }, { "rev_pre", 50 }, { "rev_mix", 0.45f } } },

            { "Tiny Moving Parts style", {
                { "gate_on", 1 }, { "gate_thr", -55 }, { "comp_amt", 0.3f },
                { "drv_gain", 0.5f }, { "drv_tone", 0.85f }, { "drv_level", 0.7f },
                { "cho_on", 0 }, { "dly_mix", 0.1f }, { "rev_size", 0.3f }, { "rev_mix", 0.2f }, { "tape_on", 0 } } },

            { "Sour Fuzz", {
                { "gate_on", 1 }, { "drv_mode", 1 }, { "drv_gain", 0.45f }, { "drv_tone", 0.45f }, { "drv_level", 0.5f },
                { "cho_on", 0 }, { "dly_mix", 0.15f }, { "rev_mix", 0.25f }, { "tape_dark", 0.4f } } },

            { "Sad Autumn Ambient", {
                { "drv_on", 0 }, { "trm_on", 1 }, { "trm_rate", 3.0f }, { "trm_depth", 0.4f },
                { "cho_depth", 0.8f }, { "cho_mix", 0.5f },
                { "dly_time", 600 }, { "dly_fb", 0.6f }, { "dly_mix", 0.4f }, { "dly_wob", 0.4f },
                { "rev_size", 0.95f }, { "rev_damp", 0.7f }, { "rev_pre", 60 }, { "rev_mix", 0.6f },
                { "tape_wow", 0.4f }, { "tape_dark", 0.5f } } }
        };
        return p;
    }
}

int EmoBoardProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String EmoBoardProcessor::getProgramName (int i)
{
    const auto& ps = presets();
    return (i >= 0 && i < (int) ps.size()) ? juce::String (ps[(size_t) i].name) : juce::String();
}

void EmoBoardProcessor::setCurrentProgram (int i)
{
    const auto& ps = presets();
    if (i < 0 || i >= (int) ps.size()) return;
    currentPreset = i;

    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            r->setValueNotifyingHost (r->getDefaultValue());

    for (auto& kv : ps[(size_t) i].v)
        if (auto* r = apvts.getParameter (kv.first))
            r->setValueNotifyingHost (r->convertTo0to1 (kv.second));
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
