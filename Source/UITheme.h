#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace macro_osc::ui
{
// A dark, flat instrument. Colour comes from the Braids hardware: turquoise
// Timbre/Modulation caps, a magenta Color cap, the yellow-green model readout,
// and the silver faceplate as the band behind pitch and envelope.
inline const juce::Colour background { 0xff0c0d10 };
inline const juce::Colour ink { 0xffeceef2 };
inline const juce::Colour muted { 0xff7d838f };
inline const juce::Colour faint { 0xff26292f };
inline const juce::Colour dot { 0xff202329 };
inline const juce::Colour turquoise { 0xff34c6cc };
inline const juce::Colour magenta { 0xfff2477f };
inline const juce::Colour display { 0xffe8fa42 };
inline const juce::Colour chip { 0xff121418 };
inline const juce::Colour chipLine { 0xff1d2026 };
inline const juce::Colour popup { 0xff16181c };
// The silver band and its printing.
inline const juce::Colour band { 0xffc6c8ce };
inline const juce::Colour bandInk { 0xff111216 };
inline const juce::Colour bandMuted { 0xff5a5e67 };
inline const juce::Colour bandTrack { 0xffa4a7af };

inline constexpr int width = 960;
inline constexpr int height = 640;

struct Fonts
{
    juce::Font sans { juce::FontOptions (14.0f) };
    juce::Font medium { juce::FontOptions (14.0f) };
    juce::Font mono { juce::FontOptions (12.0f) };
    juce::Font segment { juce::FontOptions (28.0f) };
};

// Faint dots on a 12-unit grid: the backdrop for every plot.
inline void drawDotGrid (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour = dot)
{
    g.setColour (colour);
    for (float y = area.getY() + 6.0f; y < area.getBottom(); y += 12.0f)
        for (float x = area.getX() + 6.0f; x < area.getRight(); x += 12.0f)
            g.fillRect (x - 1.0f, y - 1.0f, 2.0f, 2.0f);
}

// A soft halo behind lit elements: LEDs, the current model cell, the readout.
inline void drawGlow (juce::Graphics& g, const juce::Path& shape, juce::Colour colour, int radius = 8)
{
    juce::DropShadow (colour.withAlpha (0.5f), radius, {}).drawForPath (g, shape);
}

inline void drawChevron (juce::Graphics& g, juce::Point<float> centre, juce::Colour colour)
{
    juce::Path chevron;
    chevron.startNewSubPath (centre.translated (-3.0f, -1.5f));
    chevron.lineTo (centre.translated (0.0f, 1.5f));
    chevron.lineTo (centre.translated (3.0f, -1.5f));
    g.setColour (colour);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// DSEG14 lights every segment for '~' and leaves a full-width blank for '!', so
// the unlit cells sit exactly behind the lit characters.
inline void drawSegmentText (juce::Graphics& g, const juce::Font& font, const juce::String& value,
                             juce::Rectangle<float> area, int cells)
{
    const float baseline = area.getCentreY() + (font.getAscent() - font.getDescent()) * 0.5f;
    juce::GlyphArrangement ghost, lit;
    ghost.addLineOfText (font, juce::String::repeatedString ("~", cells), area.getX(), baseline);
    lit.addLineOfText (font, value.toUpperCase().replaceCharacter (' ', '!'), area.getX(), baseline);
    g.setColour (display.withAlpha (0.08f));
    ghost.draw (g);
    juce::Path glyphs;
    lit.createPath (glyphs);
    juce::DropShadow (display.withAlpha (0.35f), 10, {}).drawForPath (g, glyphs);
    g.setColour (display);
    g.fillPath (glyphs);
}

// The mark: two linked rings, magenta over at the top crossing and turquoise
// over at the bottom. Small gaps cut into the ring underneath show the weave.
inline void drawLogo (juce::Graphics& g, juce::Point<float> centre, float radius, float stroke, juce::Colour behind)
{
    const float separation = radius * 0.8f;
    const auto left = centre.translated (-separation * 0.5f, 0.0f);
    const auto right = centre.translated (separation * 0.5f, 0.0f);
    const auto ring = [&] (juce::Point<float> c, juce::Colour colour)
    {
        g.setColour (colour);
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c), stroke);
    };
    ring (left, magenta);
    ring (right, turquoise);
    // Angle, clockwise from twelve o'clock, from the left centre to the top crossing.
    const float crossing = std::atan2 (separation * 0.5f, std::sqrt (radius * radius - separation * separation * 0.25f));
    const auto over = [&] (juce::Point<float> c, float angle, juce::Colour colour)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, angle - 0.55f, angle + 0.55f, true);
        g.setColour (behind);
        g.strokePath (arc, juce::PathStrokeType (stroke + 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        g.setColour (colour);
        g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    };
    over (left, crossing, magenta);
    over (right, juce::MathConstants<float>::pi + crossing, turquoise);
}

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    static constexpr float modelSeam = 74.0f;

    explicit LookAndFeel (Fonts editorFonts) : fonts (std::move (editorFonts))
    {
        setColour (juce::PopupMenu::backgroundColourId, popup);
        setColour (juce::PopupMenu::textColourId, ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, turquoise);
        setColour (juce::PopupMenu::highlightedTextColourId, background);
        setColour (juce::TooltipWindow::backgroundColourId, popup);
        setColour (juce::TooltipWindow::textColourId, ink);
        setColour (juce::TooltipWindow::outlineColourId, faint);
        setColour (juce::ComboBox::textColourId, ink);
        setColour (juce::ComboBox::arrowColourId, muted);
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return isModelSelector (&box) ? fonts.segment : fonts.medium.withPointHeight (15.0f);
    }

    juce::Font getPopupMenuFont() override { return fonts.mono.withPointHeight (12.0f); }

    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox& box, juce::Label&) override
    {
        const bool models = isModelSelector (&box);
        // No item is pinned to the box: every list fits in full, and pinning
        // makes JUCE scroll the menu instead of opening it clear of the box.
        auto options = juce::PopupMenu::Options().withTargetComponent (&box)
            .withInitiallySelectedItem (box.getSelectedId())
            .withMinimumWidth (models ? 450 : juce::jmax (140, box.getWidth()))
            .withMinimumNumColumns (models ? 3 : 1)
            .withMaximumNumColumns (models ? 3 : 1)
            .withStandardItemHeight (24);
        // Menus open inside the scaled editor view, so they follow the window size
        // and stay within the plug-in window in every host. Selectors in the lower
        // half open upwards so the whole list fits without scrolling.
        for (auto* parent = box.getParentComponent(); parent != nullptr; parent = parent->getParentComponent())
            if (parent->getComponentID() == "editor-view")
            {
                const bool lower = parent->getLocalArea (&box, box.getLocalBounds()).getCentreY() > parent->getHeight() / 2;
                return options.withParentComponent (parent)
                              .withPreferredPopupDirection (lower ? juce::PopupMenu::Options::PopupDirection::upwards
                                                                  : juce::PopupMenu::Options::PopupDirection::downwards);
            }
        return options;
    }

    void getIdealPopupMenuItemSizeWithOptions (const juce::String& itemText, bool separator, int,
                                              int& itemWidth, int& itemHeight, const juce::PopupMenu::Options& options) override
    {
        itemWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (getPopupMenuFont(), itemText))
                    + (isModelSelector (options.getTargetComponent()) ? 64 : 40);
        itemHeight = separator ? 9 : 24;
    }

    void drawPopupMenuBackground (juce::Graphics& g, int menuWidth, int menuHeight) override
    {
        g.fillAll (popup);
        g.setColour (faint);
        g.drawRect (0, 0, menuWidth, menuHeight);
    }

    void drawPopupMenuItemWithOptions (juce::Graphics& g, const juce::Rectangle<int>& area,
                                      bool highlighted, const juce::PopupMenu::Item& item,
                                      const juce::PopupMenu::Options& options) override
    {
        if (item.isSeparator)
        {
            g.setColour (faint);
            g.fillRect (area.withSizeKeepingCentre (area.getWidth() - 20, 1));
            return;
        }

        const bool hover = highlighted && item.isEnabled;
        const bool models = isModelSelector (options.getTargetComponent());
        if (hover)
        {
            g.setColour (turquoise);
            g.fillRect (area.reduced (4, 1));
        }
        if (item.isTicked)
        {
            g.setColour (hover ? background : turquoise);
            g.fillRect (static_cast<float> (area.getX() + 10), area.getCentreY() - 3.0f, 6.0f, 6.0f);
        }
        auto label = area.withTrimmedLeft (24).withTrimmedRight (10);
        if (models)
        {
            g.setFont (fonts.mono.withPointHeight (11.0f));
            g.setColour (hover ? background.withAlpha (0.7f) : muted);
            g.drawText (juce::String (item.itemID - 1).paddedLeft ('0', 2), label.removeFromLeft (26),
                        juce::Justification::centredLeft);
        }
        g.setFont (getPopupMenuFont());
        g.setColour ((hover ? background : item.isTicked ? turquoise : ink).withMultipliedAlpha (item.isEnabled ? 1.0f : 0.4f));
        g.drawText (item.text.toLowerCase(), label, juce::Justification::centredLeft);
        if (item.subMenu != nullptr)
        {
            const juce::Point<float> tip (static_cast<float> (area.getRight() - 12), static_cast<float> (area.getCentreY()));
            juce::Path chevron;
            chevron.startNewSubPath (tip.translated (-1.5f, -3.0f));
            chevron.lineTo (tip.translated (1.5f, 0.0f));
            chevron.lineTo (tip.translated (-1.5f, 3.0f));
            g.setColour (hover ? background : muted);
            g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void drawComboBox (juce::Graphics& g, int boxWidth, int boxHeight, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float> (0, 0, static_cast<float> (boxWidth), static_cast<float> (boxHeight));
        const bool focused = box.hasKeyboardFocus (true);
        const bool over = box.isMouseOver (true);
        if (isModelSelector (&box))
        {
            // A flat readout window: two index cells, a fine seam, then the name.
            g.setColour (chip);
            g.fillRect (bounds);
            g.setColour (focused ? turquoise : over ? faint : chipLine);
            g.drawRect (bounds, 1.0f);
            drawSegmentText (g, fonts.segment, juce::String (juce::jmax (0, box.getSelectedId() - 1)).paddedLeft ('0', 2),
                             bounds.withTrimmedLeft (16.0f).withWidth (48.0f), 2);
            g.setColour (faint);
            g.fillRect (juce::Rectangle<float> (1.0f, 24.0f).withCentre ({ modelSeam, bounds.getCentreY() }));
            drawChevron (g, { bounds.getRight() - 18.0f, bounds.getCentreY() }, over ? ink : muted);
            return;
        }
        // Other selectors are a quiet field: a hairline outline, text and a chevron.
        g.setColour (focused ? turquoise : over ? muted.withAlpha (0.6f) : faint);
        g.drawRect (bounds, 1.0f);
        drawChevron (g, { bounds.getRight() - 12.0f, bounds.getCentreY() + 1.0f }, over ? ink : muted);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        if (isModelSelector (&box))
            label.setBounds (juce::roundToInt (modelSeam) + 16, 0,
                             box.getWidth() - juce::roundToInt (modelSeam) - 50, box.getHeight());
        else
            label.setBounds (10, 0, box.getWidth() - 30, box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setBorderSize ({});
    }

    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        auto* box = dynamic_cast<juce::ComboBox*> (label.getParentComponent());
        if (box == nullptr)
        {
            LookAndFeel_V4::drawLabel (g, label);
            return;
        }
        const auto area = label.getLocalBounds().toFloat();
        if (isModelSelector (box))
        {
            const float cell = juce::GlyphArrangement::getStringWidth (fonts.segment, "~");
            drawSegmentText (g, fonts.segment, label.getText(), area, static_cast<int> (area.getWidth() / cell));
            return;
        }
        g.setFont (label.getFont());
        g.setColour (ink);
        g.drawText (label.getText().toLowerCase(), area, box->getJustificationType(), true);
    }

    // Loop and Sync: a small square LED that lights when on.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool over, bool) override
    {
        const bool on = button.getToggleState();
        const auto led = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ 5.0f, static_cast<float> (button.getHeight()) * 0.5f });
        if (on)
        {
            juce::Path shape;
            shape.addRectangle (led);
            drawGlow (g, shape, turquoise);
            g.setColour (turquoise);
            g.fillPath (shape);
        }
        else
        {
            g.setColour (over ? ink : muted);
            g.drawRect (led.reduced (0.75f), 1.5f);
        }
        g.setFont (fonts.medium.withPointHeight (14.0f));
        g.setColour (on || over ? ink : muted);
        g.drawText (button.getButtonText().toLowerCase(), button.getLocalBounds().withTrimmedLeft (17),
                    juce::Justification::centredLeft);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.fillAll (popup);
        g.setColour (faint);
        g.drawRect (0, 0, w, h);
        g.setColour (ink);
        g.setFont (fonts.sans.withPointHeight (12.0f));
        g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 4);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> position, juce::Rectangle<int> parentArea) override
    {
        const auto font = fonts.sans.withPointHeight (12.0f);
        juce::AttributedString attributed;
        attributed.append (text, font);
        juce::TextLayout layout;
        layout.createLayout (attributed, 300.0f);
        const int w = juce::roundToInt (layout.getWidth()) + 18;
        const int h = juce::roundToInt (layout.getHeight()) + 10;
        return juce::Rectangle<int> (position.x > parentArea.getCentreX() ? position.x - (w + 12) : position.x + 12,
                                     position.y > parentArea.getCentreY() ? position.y - (h + 8) : position.y + 18,
                                     w, h).constrainedWithin (parentArea);
    }

    void drawCornerResizer (juce::Graphics& g, int w, int h, bool over, bool dragging) override
    {
        g.setColour ((over || dragging) ? muted : faint);
        for (float inset : { 4.0f, 8.0f })
            g.drawLine (static_cast<float> (w) - inset, static_cast<float> (h) - 2.0f,
                        static_cast<float> (w) - 2.0f, static_cast<float> (h) - inset, 1.2f);
    }

private:
    static bool isModelSelector (const juce::Component* component)
    {
        return component != nullptr && component->getComponentID() == "oscillator-model";
    }
    Fonts fonts;
};
}
