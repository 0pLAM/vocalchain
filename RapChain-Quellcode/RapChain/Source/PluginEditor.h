#pragma once

#include "PluginProcessor.h"
#include <memory>
#include <vector>

namespace rc
{
    inline const juce::Colour bg     { 0xff0c0c0e };
    inline const juce::Colour panel  { 0xff151518 };
    inline const juce::Colour line   { 0xff26262c };
    inline const juce::Colour text   { 0xffeeeeee };
    inline const juce::Colour dim    { 0xff85858f };
    inline const juce::Colour accent { 0xffff4d3d };

    inline juce::Font font (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
}

//==============================================================================
class RapChainLook : public juce::LookAndFeel_V4
{
public:
    RapChainLook()
    {
        setColour (juce::Slider::textBoxTextColourId,       rc::text);
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId,  rc::accent.withAlpha (0.4f));
        setColour (juce::Label::textColourId,               rc::dim);
        setColour (juce::ComboBox::backgroundColourId,      rc::panel);
        setColour (juce::ComboBox::textColourId,            rc::text);
        setColour (juce::ComboBox::outlineColourId,         rc::line);
        setColour (juce::ComboBox::arrowColourId,           rc::dim);
        setColour (juce::PopupMenu::backgroundColourId,     rc::panel);
        setColour (juce::PopupMenu::textColourId,           rc::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, rc::accent.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, rc::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float d  = juce::jmin (area.getWidth(), area.getHeight());
        auto  c  = area.getCentre();
        const float r  = d * 0.5f - 3.0f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        g.setColour (rc::panel.brighter (0.08f));
        g.fillEllipse (c.x - r + 5, c.y - r + 5, 2 * (r - 5), 2 * (r - 5));

        juce::Path track;
        track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
        g.setColour (rc::line);
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, angle, true);
        g.setColour (rc::accent);
        g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path tick;
        tick.addRoundedRectangle (-1.5f, -(r - 4.0f), 3.0f, r * 0.5f, 1.5f);
        g.setColour (rc::text);
        g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const float dot = 9.0f;
        auto dotArea = juce::Rectangle<float> (r.getX(), r.getCentreY() - dot / 2, dot, dot);
        if (b.getToggleState())
        {
            g.setColour (rc::accent.withAlpha (0.35f));
            g.fillEllipse (dotArea.expanded (3.0f));
            g.setColour (rc::accent);
            g.fillEllipse (dotArea);
        }
        else
        {
            g.setColour (rc::dim);
            g.drawEllipse (dotArea.reduced (0.5f), 1.4f);
        }
        g.setColour (b.getToggleState() ? rc::text : rc::dim);
        g.setFont (rc::font (12.0f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (dot + 8.0f).toNearestInt(),
                    juce::Justification::centredLeft);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (rc::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (rc::line);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);

        juce::Path arrow;
        const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return rc::font (13.0f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (12, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    juce::Font getLabelFont (juce::Label& l) override { return l.getFont(); }
};

//==============================================================================
class RapChainEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RapChainEditor (RapChainProcessor&);
    ~RapChainEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class Fmt { db, hz, ms, ratio, percent };

    struct KnobSpec { const char* id; const char* label; Fmt fmt; int decimals; };

    struct Knob
    {
        juce::Slider slider;
        juce::Label  name;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    struct Section
    {
        juce::String title;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAttach;
        std::vector<std::unique_ptr<Knob>> knobs;
        juce::Rectangle<int> bounds;
        int row = 0;
    };

    void addSection (int row, const juce::String& title, const char* onId, const char* toggleText,
                     std::vector<KnobSpec> specs);
    void timerCallback() override;

    RapChainProcessor& proc;
    RapChainLook look;

    juce::ComboBox presetBox;
    std::vector<std::unique_ptr<Section>> sections;

    float inLevel = 0.0f, outLevel = 0.0f, grDb = 0.0f;
    juce::Rectangle<int> meterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RapChainEditor)
};
