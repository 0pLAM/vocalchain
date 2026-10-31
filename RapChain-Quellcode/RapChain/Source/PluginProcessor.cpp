#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using Range = juce::NormalisableRange<float>;

    Range skewed (float lo, float hi, float centre)
    {
        Range r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    struct PresetValue { const char* id; float value; };
    struct Preset      { const char* name; std::vector<PresetValue> values; };

    // Alles, was nicht aufgefuehrt ist, wird auf den Standardwert zurueckgesetzt.
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Init", {} },

            { "Rap Vocal - Clean", {
                { "hpf", 90 }, { "eqLow", -1.5f }, { "eqMidFreq", 3200 }, { "eqMid", 2.0f }, { "eqAir", 3.0f },
                { "compThresh", -20 }, { "compRatio", 4 }, { "compAtk", 6 }, { "compRel", 110 }, { "compMakeup", 4 },
                { "dsThresh", -28 }, { "satDrive", 0.15f }, { "satMix", 0.6f },
                { "dlyMix", 0.0f, }, { "dlyOn", 0 }, { "revSize", 0.3f }, { "revMix", 0.08f } } },

            { "Trap Vocal - Dunkel & Fett", {
                { "hpf", 70 }, { "eqLow", 2.5f }, { "eqMidFreq", 450 }, { "eqMid", -2.5f }, { "eqAir", 1.5f },
                { "compThresh", -24 }, { "compRatio", 6 }, { "compAtk", 4 }, { "compRel", 90 }, { "compMakeup", 7 },
                { "dsThresh", -26 }, { "satDrive", 0.45f }, { "satMix", 0.8f },
                { "dlyTime", 220 }, { "dlyFb", 0.3f }, { "dlyMix", 0.14f },
                { "revSize", 0.45f }, { "revDamp", 0.7f }, { "revMix", 0.12f } } },

            { "Hell & Airy", {
                { "hpf", 110 }, { "eqLow", -2.0f }, { "eqMidFreq", 5000 }, { "eqMid", 2.5f }, { "eqAir", 6.0f },
                { "compThresh", -18 }, { "compRatio", 3 }, { "compAtk", 10 }, { "compRel", 130 }, { "compMakeup", 3 },
                { "dsThresh", -32 }, { "dsFreq", 7500 }, { "satDrive", 0.1f },
                { "dlyMix", 0.1f }, { "revSize", 0.55f }, { "revDamp", 0.4f }, { "revMix", 0.18f } } },

            { "Ad-libs - Echo", {
                { "hpf", 200 }, { "eqLow", -4.0f }, { "eqMidFreq", 1800 }, { "eqMid", 3.0f }, { "eqAir", 3.0f },
                { "compThresh", -22 }, { "compRatio", 5 }, { "compMakeup", 5 },
                { "satDrive", 0.35f },
                { "dlyTime", 310 }, { "dlyFb", 0.5f }, { "dlyMix", 0.35f },
                { "revSize", 0.7f }, { "revMix", 0.25f } } },

            { "Telefon / Lo-Fi", {
                { "hpf", 400 }, { "eqLow", -8.0f }, { "eqMidFreq", 1500 }, { "eqMid", 5.0f }, { "eqAir", -8.0f },
                { "compThresh", -26 }, { "compRatio", 8 }, { "compMakeup", 8 },
                { "satDrive", 0.8f }, { "satMix", 1.0f },
                { "dlyMix", 0.0f }, { "revMix", 0.0f } } },
        };
        return p;
    }

    inline float dbToGain (float db) { return juce::Decibels::decibelsToGain (db); }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout RapChainProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    auto f = [&l] (const char* id, const char* name, Range r, float def)
    {
        l.add (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def));
    };
    auto b = [&l] (const char* id, const char* name, bool def)
    {
        l.add (std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def));
    };

    f ("inGain",     "Input",          Range (-24.0f, 24.0f, 0.1f), 0.0f);
    f ("hpf",        "Low Cut",        skewed (20.0f, 400.0f, 90.0f), 80.0f);

    b ("eqOn",       "EQ On",          true);
    f ("eqLow",      "EQ Low",         Range (-12.0f, 12.0f, 0.1f), 0.0f);
    f ("eqMidFreq",  "EQ Mid Freq",    skewed (200.0f, 6000.0f, 1200.0f), 1000.0f);
    f ("eqMid",      "EQ Mid",         Range (-12.0f, 12.0f, 0.1f), 0.0f);
    f ("eqAir",      "EQ Air",         Range (-12.0f, 12.0f, 0.1f), 0.0f);

    b ("compOn",     "Comp On",        true);
    f ("compThresh", "Comp Threshold", Range (-50.0f, 0.0f, 0.1f), -18.0f);
    f ("compRatio",  "Comp Ratio",     skewed (1.0f, 20.0f, 4.0f), 3.0f);
    f ("compAtk",    "Comp Attack",    skewed (0.1f, 100.0f, 10.0f), 8.0f);
    f ("compRel",    "Comp Release",   skewed (20.0f, 600.0f, 120.0f), 120.0f);
    f ("compMakeup", "Comp Makeup",    Range (0.0f, 24.0f, 0.1f), 0.0f);

    b ("dsOn",       "De-Esser On",    true);
    f ("dsThresh",   "De-Esser Thresh", Range (-50.0f, 0.0f, 0.1f), -26.0f);
    f ("dsFreq",     "De-Esser Freq",  skewed (3000.0f, 10000.0f, 6000.0f), 6500.0f);

    b ("satOn",      "Saturation On",  true);
    f ("satDrive",   "Drive",          Range (0.0f, 1.0f, 0.01f), 0.2f);
    f ("satMix",     "Sat Mix",        Range (0.0f, 1.0f, 0.01f), 1.0f);

    b ("dlyOn",      "Delay On",       true);
    f ("dlyTime",    "Delay Time",     skewed (40.0f, 800.0f, 250.0f), 180.0f);
    f ("dlyFb",      "Delay Feedback", Range (0.0f, 0.9f, 0.01f), 0.25f);
    f ("dlyMix",     "Delay Mix",      Range (0.0f, 1.0f, 0.01f), 0.12f);

    b ("revOn",      "Reverb On",      true);
    f ("revSize",    "Reverb Size",    Range (0.0f, 1.0f, 0.01f), 0.4f);
    f ("revDamp",    "Reverb Damp",    Range (0.0f, 1.0f, 0.01f), 0.5f);
    f ("revMix",     "Reverb Mix",     Range (0.0f, 1.0f, 0.01f), 0.1f);

    b ("limOn",      "Limiter On",     true);
    f ("outGain",    "Output",         Range (-24.0f, 12.0f, 0.1f), 0.0f);

    return l;
}

juce::StringArray RapChainProcessor::presetNames()
{
    juce::StringArray n;
    for (auto& p : presets())
        n.add (p.name);
    return n;
}

void RapChainProcessor::applyPreset (int index)
{
    const auto& all = presets();
    if (index < 0 || index >= (int) all.size())
        return;

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& v : all[(size_t) index].values)
        if (auto* rp = apvts.getParameter (v.id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v.value));
}

//==============================================================================
RapChainProcessor::RapChainProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RAPCHAIN", createLayout())
{
}

bool RapChainProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void RapChainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const auto nCh = (juce::uint32) juce::jmax (1, getTotalNumInputChannels());
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, nCh };

    for (int c = 0; c < 2; ++c)
    {
        hpf1[c].reset(); hpf2[c].reset();
        eqLow[c].reset(); eqMid[c].reset(); eqAir[c].reset();
        dsDetect[c].reset();
        delayLp[c] = 0.0f;
        delayBuf[c].assign ((size_t) (sampleRate * 1.0) + 8, 0.0f);

        hpf1[c].coefficients  = Coeffs::makeHighPass (sr, 80.0f);
        hpf2[c].coefficients  = Coeffs::makeHighPass (sr, 80.0f);
        eqLow[c].coefficients = Coeffs::makeLowShelf (sr, 120.0f, 0.7f, 1.0f);
        eqMid[c].coefficients = Coeffs::makePeakFilter (sr, 1000.0f, 1.0f, 1.0f);
        eqAir[c].coefficients = Coeffs::makeHighShelf (sr, 10000.0f, 0.7f, 1.0f);
        dsDetect[c].coefficients = Coeffs::makeLowPass (sr, 6500.0f);
    }
    delayWrite = 0;
    dsEnv = 0.0f;
    delayTimeSmooth.reset (sampleRate, 0.05);
    delayTimeSmooth.setCurrentAndTargetValue (0.18f);

    compressor.prepare (spec);
    reverb.prepare (spec);
    limiter.prepare (spec);
    limiter.setThreshold (-0.3f);
    limiter.setRelease (80.0f);
    reverbBuf.setSize ((int) nCh, samplesPerBlock);
}

//==============================================================================
void RapChainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n     = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    // ---- Input ----
    buffer.applyGain (dbToGain (param ("inGain")));
    inPeak.store (buffer.getMagnitude (0, n));

    // ---- Low Cut (24 dB/Okt) ----
    {
        const float fc = param ("hpf");
        for (int c = 0; c < numCh; ++c)
        {
            hpf1[c].coefficients = Coeffs::makeHighPass (sr, fc, 0.54f);
            hpf2[c].coefficients = Coeffs::makeHighPass (sr, fc, 1.31f);
            auto* x = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                x[i] = hpf2[c].processSample (hpf1[c].processSample (x[i]));
        }
    }

    // ---- EQ ----
    if (on ("eqOn"))
    {
        const float low = dbToGain (param ("eqLow"));
        const float mid = dbToGain (param ("eqMid"));
        const float air = dbToGain (param ("eqAir"));
        const float mf  = param ("eqMidFreq");

        for (int c = 0; c < numCh; ++c)
        {
            eqLow[c].coefficients = Coeffs::makeLowShelf (sr, 120.0f, 0.7f, low);
            eqMid[c].coefficients = Coeffs::makePeakFilter (sr, mf, 1.0f, mid);
            eqAir[c].coefficients = Coeffs::makeHighShelf (sr, 10000.0f, 0.7f, air);

            auto* x = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                x[i] = eqAir[c].processSample (eqMid[c].processSample (eqLow[c].processSample (x[i])));
        }
    }

    // ---- Kompressor ----
    float grDb = 0.0f;
    if (on ("compOn"))
    {
        const float before = buffer.getMagnitude (0, n);

        compressor.setThreshold (param ("compThresh"));
        compressor.setRatio (param ("compRatio"));
        compressor.setAttack (param ("compAtk"));
        compressor.setRelease (param ("compRel"));

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        compressor.process (ctx);

        const float after = buffer.getMagnitude (0, n);
        if (before > 1.0e-5f && after > 1.0e-6f)
            grDb = juce::jmax (0.0f, juce::Decibels::gainToDecibels (before / after));

        buffer.applyGain (dbToGain (param ("compMakeup")));
    }
    compReductionDb.store (grDb);

    // ---- De-Esser (frequenzselektiv, Seitenkette ab Eckfrequenz) ----
    if (on ("dsOn"))
    {
        const float thr = dbToGain (param ("dsThresh"));
        const float fc  = param ("dsFreq");
        const float releaseCoef = std::exp (-1.0f / (0.04f * (float) sr));

        for (int c = 0; c < numCh; ++c)
            dsDetect[c].coefficients = Coeffs::makeLowPass (sr, fc, 0.707f);

        for (int i = 0; i < n; ++i)
        {
            float hi[2] { 0.0f, 0.0f };
            float level = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                // Hochband = Signal minus Tiefpass: ergibt immer exakt das Originalsignal, wenn nicht reduziert wird
                const float in = buffer.getReadPointer (c)[i];
                hi[c] = in - dsDetect[c].processSample (in);
                level = juce::jmax (level, std::abs (hi[c]));
            }
            dsEnv = juce::jmax (level, dsEnv * releaseCoef);

            float g = 1.0f;
            if (dsEnv > thr)
                g = juce::jmax (0.25f, std::pow (thr / dsEnv, 0.75f));

            for (int c = 0; c < numCh; ++c)
                buffer.getWritePointer (c)[i] -= hi[c] * (1.0f - g);
        }
    }

    // ---- Saettigung ----
    if (on ("satOn"))
    {
        const float drive = param ("satDrive");
        const float mix   = param ("satMix");
        const float g     = 1.0f + drive * 6.0f;

        for (int c = 0; c < numCh; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float wet = std::tanh (g * x[i]) / g;
                x[i] = x[i] + mix * (wet - x[i]);
            }
        }
    }

    // ---- Delay (Ping-Pong) ----
    {
        const bool  active = on ("dlyOn");
        const float mix    = active ? param ("dlyMix") : 0.0f;
        const float fb     = param ("dlyFb");
        const float lpA    = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 4500.0f / (float) sr);
        const int   len    = (int) delayBuf[0].size();

        delayTimeSmooth.setTargetValue (param ("dlyTime") * 0.001f);

        for (int i = 0; i < n; ++i)
        {
            const float delaySamples = juce::jlimit (1.0f, (float) (len - 4), delayTimeSmooth.getNextValue() * (float) sr);

            float rp = (float) delayWrite - delaySamples;
            if (rp < 0.0f) rp += (float) len;
            const int   i0   = (int) rp;
            const int   i1   = (i0 + 1) % len;
            const float frac = rp - (float) i0;

            float d[2];
            for (int c = 0; c < 2; ++c)
            {
                const float s = delayBuf[c][(size_t) i0] * (1.0f - frac) + delayBuf[c][(size_t) i1] * frac;
                delayLp[c] += lpA * (s - delayLp[c]);
                d[c] = delayLp[c];
            }

            float mono = 0.0f;
            for (int c = 0; c < numCh; ++c)
                mono += buffer.getReadPointer (c)[i];
            mono /= (float) numCh;
            if (! active) mono = 0.0f;

            delayBuf[0][(size_t) delayWrite] = mono + fb * d[1];
            delayBuf[1][(size_t) delayWrite] = fb * d[0];
            delayWrite = (delayWrite + 1) % len;

            if (numCh == 1)
                buffer.getWritePointer (0)[i] += mix * 0.5f * (d[0] + d[1]);
            else
                for (int c = 0; c < 2; ++c)
                    buffer.getWritePointer (c)[i] += mix * d[c];
        }
    }

    // ---- Reverb ----
    if (on ("revOn") && param ("revMix") > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = 0.25f + 0.7f * param ("revSize");
        rp.damping    = param ("revDamp");
        rp.wetLevel   = 0.33f;
        rp.dryLevel   = 0.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);

        reverbBuf.setSize (buffer.getNumChannels(), n, false, false, true);
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            reverbBuf.copyFrom (c, 0, buffer, c, 0, n);

        juce::dsp::AudioBlock<float> block (reverbBuf);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);

        const float mix = param ("revMix");
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            buffer.addFrom (c, 0, reverbBuf, c, 0, n, mix);
    }

    // ---- Output + Limiter ----
    buffer.applyGain (dbToGain (param ("outGain")));

    if (on ("limOn"))
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        limiter.process (ctx);
    }

    outPeak.store (buffer.getMagnitude (0, n));
}

//==============================================================================
juce::AudioProcessorEditor* RapChainProcessor::createEditor()
{
    return new RapChainEditor (*this);
}

void RapChainProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RapChainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RapChainProcessor();
}
