#include "PluginEditor.h"

namespace
{
    constexpr int kWidth = 880, kHeight = 470;
    constexpr int kTop = 60, kMargin = 16, kGap = 8, kRowH = 186, kTitleH = 30;
}

RapChainEditor::RapChainEditor (RapChainProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);

    presetBox.addItemList (RapChainProcessor::presetNames(), 1);
    presetBox.setTextWhenNothingSelected ("Preset waehlen ...");
    presetBox.onChange = [this] { proc.applyPreset (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    // Reihe 1
    addSection (0, "INPUT", nullptr, nullptr, {
        { "inGain", "INPUT",   Fmt::db, 1 },
        { "hpf",    "LOW CUT", Fmt::hz, 0 } });

    addSection (0, "EQ", "eqOn", "EQ", {
        { "eqLow",     "LOW",    Fmt::db, 1 },
        { "eqMidFreq", "MID HZ", Fmt::hz, 0 },
        { "eqMid",     "MID",    Fmt::db, 1 },
        { "eqAir",     "AIR",    Fmt::db, 1 } });

    addSection (0, "COMPRESSOR", "compOn", "COMPRESSOR", {
        { "compThresh", "THRESH",  Fmt::db,    1 },
        { "compRatio",  "RATIO",   Fmt::ratio, 1 },
        { "compAtk",    "ATTACK",  Fmt::ms,    1 },
        { "compRel",    "RELEASE", Fmt::ms,    0 },
        { "compMakeup", "MAKEUP",  Fmt::db,    1 } });

    // Reihe 2
    addSection (1, "DE-ESSER", "dsOn", "DE-ESSER", {
        { "dsThresh", "THRESH", Fmt::db, 1 },
        { "dsFreq",   "FREQ",   Fmt::hz, 0 } });

    addSection (1, "SATURATION", "satOn", "SATURATION", {
        { "satDrive", "DRIVE", Fmt::percent, 0 },
        { "satMix",   "MIX",   Fmt::percent, 0 } });

    addSection (1, "DELAY", "dlyOn", "DELAY", {
        { "dlyTime", "TIME",     Fmt::ms,      0 },
        { "dlyFb",   "FEEDBACK", Fmt::percent, 0 },
        { "dlyMix",  "MIX",      Fmt::percent, 0 } });

    addSection (1, "REVERB", "revOn", "REVERB", {
        { "revSize", "SIZE", Fmt::percent, 0 },
        { "revDamp", "DAMP", Fmt::percent, 0 },
        { "revMix",  "MIX",  Fmt::percent, 0 } });

    addSection (1, "OUTPUT", "limOn", "LIMITER", {
        { "outGain", "OUTPUT", Fmt::db, 1 } });

    setSize (kWidth, kHeight);
    startTimerHz (30);
}

RapChainEditor::~RapChainEditor()
{
    setLookAndFeel (nullptr);
}

void RapChainEditor::addSection (int row, const juce::String& title, const char* onId,
                                 const char* toggleText, std::vector<KnobSpec> specs)
{
    auto s = std::make_unique<Section>();
    s->title = title;
    s->row   = row;

    if (onId != nullptr)
    {
        s->toggle = std::make_unique<juce::ToggleButton> (toggleText);
        s->toggle->setMouseCursor (juce::MouseCursor::PointingHandCursor);
        s->toggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, onId, *s->toggle);
        addAndMakeVisible (*s->toggle);
    }

    for (auto& spec : specs)
    {
        auto k = std::make_unique<Knob>();

        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 16);
        k->slider.setColour (juce::Slider::textBoxTextColourId,       rc::text);
        k->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        k->slider.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        k->slider.setColour (juce::Slider::textBoxHighlightColourId,  rc::accent.withAlpha (0.4f));
        k->slider.setNumDecimalPlacesToDisplay (spec.decimals);

        k->attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, spec.id, k->slider);

        const auto fmt = spec.fmt;
        const int  dec = spec.decimals;
        k->slider.textFromValueFunction = [fmt, dec] (double v)
        {
            switch (fmt)
            {
                case Fmt::db:      return juce::String (v, dec) + " dB";
                case Fmt::ms:      return juce::String (v, dec) + " ms";
                case Fmt::ratio:   return juce::String (v, dec) + " : 1";
                case Fmt::percent: return juce::String (juce::roundToInt (v * 100.0)) + " %";
                case Fmt::hz:      return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " kHz"
                                                      : juce::String (juce::roundToInt (v)) + " Hz";
            }
            return juce::String (v);
        };
        k->slider.valueFromTextFunction = [fmt] (const juce::String& t)
        {
            double v = t.retainCharacters ("0123456789.-").getDoubleValue();
            if (fmt == Fmt::percent)
                return v / 100.0;
            if (fmt == Fmt::hz && t.containsIgnoreCase ("k"))
                return v * 1000.0;
            return v;
        };

        if (auto* prm = proc.apvts.getParameter (spec.id))
            k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));

        k->slider.updateText();

        k->name.setText (spec.label, juce::dontSendNotification);
        k->name.setColour (juce::Label::textColourId, rc::dim);
        k->name.setJustificationType (juce::Justification::centred);
        k->name.setFont (rc::font (10.5f, true));
        k->name.setInterceptsMouseClicks (false, false);

        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->name);
        s->knobs.push_back (std::move (k));
    }

    sections.push_back (std::move (s));
}

void RapChainEditor::paint (juce::Graphics& g)
{
    g.fillAll (rc::bg);

    // Kopfzeile
    g.setColour (rc::accent);
    g.fillEllipse ((float) kMargin, 24.0f, 8.0f, 8.0f);
    g.setColour (rc::text);
    g.setFont (rc::font (15.0f, true));
    g.drawText ("R A P C H A I N", kMargin + 16, 18, 180, 20, juce::Justification::centredLeft);

    // Pegel
    auto drawMeter = [&g] (juce::Rectangle<int> r, float peak, const char* label)
    {
        g.setColour (rc::dim);
        g.setFont (rc::font (10.0f, true));
        g.drawText (label, r.removeFromLeft (26), juce::Justification::centredLeft);
        auto bar = r.toFloat().withSizeKeepingCentre ((float) r.getWidth(), 6.0f);
        g.setColour (rc::line);
        g.fillRoundedRectangle (bar, 3.0f);
        const float db = juce::Decibels::gainToDecibels (peak, -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (db > -1.0f ? rc::accent : rc::text);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac), 3.0f);
    };
    auto m = meterArea;
    drawMeter (m.removeFromTop (22).withTrimmedTop (4), inLevel,  "IN");
    drawMeter (m.removeFromTop (22).withTrimmedTop (4), outLevel, "OUT");

    // Sektionen
    for (auto& s : sections)
    {
        auto b = s->bounds.toFloat();
        g.setColour (rc::panel);
        g.fillRoundedRectangle (b, 12.0f);
        g.setColour (rc::line);
        g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);

        if (s->toggle == nullptr)
        {
            g.setColour (rc::text);
            g.setFont (rc::font (12.0f, true));
            g.drawText (s->title, s->bounds.withHeight (kTitleH).withTrimmedLeft (14), juce::Justification::centredLeft);
        }

        if (s->title == "COMPRESSOR")
        {
            g.setColour (grDb > 0.5f ? rc::accent : rc::dim);
            g.setFont (rc::font (11.0f, true));
            g.drawText ("GR " + juce::String (grDb, 1) + " dB",
                        s->bounds.withHeight (kTitleH).withTrimmedRight (14), juce::Justification::centredRight);
        }
    }
}

void RapChainEditor::resized()
{
    presetBox.setBounds (220, 14, 260, 28);
    meterArea = juce::Rectangle<int> (kWidth - kMargin - 230, 8, 230, 48);

    for (int row = 0; row < 2; ++row)
    {
        int total = 0;
        int count = 0;
        for (auto& s : sections)
            if (s->row == row) { total += (int) s->knobs.size(); ++count; }

        const int avail = kWidth - 2 * kMargin - (count - 1) * kGap;
        int x = kMargin;
        const int y = kTop + row * (kRowH + kGap);

        for (auto& s : sections)
        {
            if (s->row != row)
                continue;

            const int w = avail * (int) s->knobs.size() / total;
            s->bounds = { x, y, w, kRowH };
            x += w + kGap;

            if (s->toggle != nullptr)
                s->toggle->setBounds (s->bounds.getX() + 14, s->bounds.getY() + 5, 150, kTitleH - 6);

            const int n = (int) s->knobs.size();
            const int pad = 6;
            const int cellW = (w - 2 * pad) / n;
            for (int i = 0; i < n; ++i)
            {
                auto cell = juce::Rectangle<int> (s->bounds.getX() + pad + i * cellW, y + kTitleH, cellW, kRowH - kTitleH - 8);
                s->knobs[(size_t) i]->name.setBounds (cell.removeFromTop (16));
                s->knobs[(size_t) i]->slider.setBounds (cell);
            }
        }
    }
}

void RapChainEditor::timerCallback()
{
    auto fall = [] (float current, float target)
    {
        return target > current ? target : current * 0.85f + target * 0.15f;
    };
    inLevel  = fall (inLevel,  proc.inPeak.load());
    outLevel = fall (outLevel, proc.outPeak.load());
    grDb     = fall (grDb,     proc.compReductionDb.load());
    repaint();
}
