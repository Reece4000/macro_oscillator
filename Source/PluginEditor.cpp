#include "PluginEditor.h"

#include "BinaryData.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace macro_osc
{
namespace
{
const juce::Colour backgroundColour { 0xff062f31 };
const juce::Colour rackPanelColour { 0xff0f7773 };
const juce::Colour boxColour { 0xff083f42 };
const juce::Colour darkScreenColour { 0xff091719 };
const juce::Colour textColour { 0xfffff6e8 };
const juce::Colour mutedTextColour { 0xffbfe3d9 };
const juce::Colour yellowAccent { 0xffffcc1d };
const juce::Colour cyanAccent { 0xff35d4d3 };
const juce::Colour magentaAccent { 0xffd95088 };
const juce::Colour paleAccent { 0xffb7e5dd };

constexpr float kDesignWidth = 1512.0f;
constexpr float kDesignHeight = 504.0f;
constexpr int kEditorWidth = 760;
constexpr int kEditorHeight = 560;
constexpr int kBottomControlBankWidth = 282;
constexpr int kBottomColumnGap = 5;
constexpr std::array<const char*, 30> kUiParameterIds {
    ParamIDs::model,
    ParamIDs::timbre,
    ParamIDs::color,
    ParamIDs::modulation,
    ParamIDs::fmAmount,
    ParamIDs::coarse,
    ParamIDs::fine,
    ParamIDs::portamento,
    ParamIDs::attack,
    ParamIDs::decay,
    ParamIDs::sustain,
    ParamIDs::release,
    ParamIDs::msegDestination[0],
    ParamIDs::msegDestination[1],
    ParamIDs::msegDestination[2],
    ParamIDs::msegAmount[0],
    ParamIDs::msegAmount[1],
    ParamIDs::msegAmount[2],
    ParamIDs::msegOffset[0],
    ParamIDs::msegOffset[1],
    ParamIDs::msegOffset[2],
    ParamIDs::msegRate[0],
    ParamIDs::msegRate[1],
    ParamIDs::msegRate[2],
    ParamIDs::msegLoop[0],
    ParamIDs::msegLoop[1],
    ParamIDs::msegLoop[2],
    ParamIDs::msegSync[0],
    ParamIDs::msegSync[1],
    ParamIDs::msegSync[2]
};

[[nodiscard]] juce::RangedAudioParameter* parameterFor (MacroOscAudioProcessor& processor, const char* parameterId)
{
    return processor.getState().getParameter (parameterId);
}

[[nodiscard]] float rawParameterValue (MacroOscAudioProcessor& processor, const char* parameterId)
{
    if (const auto* value = processor.getState().getRawParameterValue (parameterId))
        return value->load (std::memory_order_relaxed);

    return 0.0f;
}

[[nodiscard]] MsegPoint scaledMsegPoint (const MsegPoint& point, float amountPercent, float offsetPercent)
{
    const float amount = amountPercent * 0.01f;
    const float offset = offsetPercent * 0.01f;
    const float bipolar = (point.y * 2.0f) - 1.0f;
    const float scaled = juce::jlimit (-1.0f, 1.0f, (bipolar * amount) + offset);
    return { point.x, (scaled + 1.0f) * 0.5f };
}

void setParameterNormalised (MacroOscAudioProcessor& processor, const char* parameterId, float normalised)
{
    if (auto* parameter = parameterFor (processor, parameterId))
        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}

void resetParameterToDefault (MacroOscAudioProcessor& processor, const char* parameterId)
{
    if (auto* parameter = parameterFor (processor, parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getDefaultValue());
        parameter->endChangeGesture();
    }
}

[[nodiscard]] juce::Font withHeight (juce::Font font, float height)
{
    return font.withHeight (juce::jmax (6.0f, height));
}

[[nodiscard]] juce::String valueText (float value, int decimals)
{
    if (decimals <= 0)
        return juce::String (juce::roundToInt (value));

    return juce::String (value, decimals);
}

void drawDigitalSlots (juce::Graphics& g,
                       juce::Font font,
                       juce::String text,
                       juce::Rectangle<float> bounds,
                       int slots,
                       juce::juce_wchar padCharacter,
                       juce::Colour colour)
{
    if (slots <= 0)
        return;

    text = text.substring (juce::jmax (0, text.length() - slots)).paddedLeft (padCharacter, slots);
    const float cellWidth = bounds.getWidth() / static_cast<float> (slots);

    g.setColour (colour);
    g.setFont (font);
    for (int i = 0; i < slots; ++i)
    {
        const auto glyph = text.substring (i, i + 1);
        if (glyph == " ")
            continue;

        auto cell = bounds.withX (bounds.getX() + static_cast<float> (i) * cellWidth).withWidth (cellWidth);
        g.drawText (glyph, cell.toNearestInt(), juce::Justification::centred, false);
    }
}

void drawGlassPanel (juce::Graphics& g,
                     juce::Rectangle<float> bounds,
                     juce::Colour base,
                     juce::Colour accent,
                     float radius,
                     bool raised)
{
    if (raised)
    {
        juce::Path shadowPath;
        shadowPath.addRoundedRectangle (bounds, radius);
        juce::DropShadow (juce::Colours::black.withAlpha (0.30f), 9, { 0, 4 }).drawForPath (g, shadowPath);
    }

    juce::ColourGradient fill (base.brighter (0.18f).withAlpha (0.88f), bounds.getX(), bounds.getY(),
                               base.darker (0.42f).withAlpha (0.94f), bounds.getRight(), bounds.getBottom(), false);
    fill.addColour (0.46, base.withAlpha (0.80f));
    g.setGradientFill (fill);
    g.fillRoundedRectangle (bounds, radius);

    const auto shine = bounds.reduced (2.0f).withHeight (bounds.getHeight() * 0.42f);
    juce::ColourGradient highlight (juce::Colours::white.withAlpha (0.14f), shine.getX(), shine.getY(),
                                    juce::Colours::white.withAlpha (0.0f), shine.getX(), shine.getBottom(), false);
    g.setGradientFill (highlight);
    g.fillRoundedRectangle (shine, radius * 0.82f);

    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), radius, 1.0f);
    g.setColour (accent.withAlpha (0.38f));
    g.drawRoundedRectangle (bounds.reduced (0.4f), radius, 1.2f);
}

void drawGlassKnob (juce::Graphics& g,
                    juce::Rectangle<float> knob,
                    juce::Colour accent,
                    float normalised,
                    bool active)
{
    juce::Path knobShadow;
    knobShadow.addEllipse (knob);
    juce::DropShadow (juce::Colours::black.withAlpha (0.30f), 8, { 0, 3 }).drawForPath (g, knobShadow);

    juce::ColourGradient fill (juce::Colour (0xff1c8a81).withAlpha (0.84f), knob.getX(), knob.getY(),
                               darkScreenColour.withAlpha (0.96f), knob.getRight(), knob.getBottom(), false);
    fill.addColour (0.58, boxColour.withAlpha (0.88f));
    g.setGradientFill (fill);
    g.fillEllipse (knob);

    g.setColour (juce::Colours::white.withAlpha (0.20f));
    g.drawEllipse (knob.reduced (1.2f), active ? 2.0f : 1.2f);
    g.setColour (accent.withAlpha (active ? 0.92f : 0.55f));
    g.drawEllipse (knob.reduced (3.8f), active ? 1.8f : 1.0f);

    const auto centre = knob.getCentre();
    const float radius = knob.getWidth() * 0.58f;
    constexpr float startAngle = -2.35f;
    constexpr float endAngle = 2.35f;
    const float valueAngle = startAngle + juce::jlimit (0.0f, 1.0f, normalised) * (endAngle - startAngle);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.24f));
    g.strokePath (backgroundArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, valueAngle, true);
    g.setColour (accent.withAlpha (active ? 1.0f : 0.82f));
    g.strokePath (valueArc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto pointerStart = centre.translated (std::sin (valueAngle) * knob.getWidth() * 0.10f,
                                                -std::cos (valueAngle) * knob.getWidth() * 0.10f);
    const auto pointerEnd = centre.translated (std::sin (valueAngle) * knob.getWidth() * 0.34f,
                                              -std::cos (valueAngle) * knob.getWidth() * 0.34f);
    g.setColour (textColour.withAlpha (0.92f));
    g.drawLine ({ pointerStart, pointerEnd }, 2.2f);
}

void drawMiniCurve (juce::Graphics& g,
                    const std::vector<MsegPoint>& points,
                    juce::Rectangle<float> bounds,
                    juce::Colour colour,
                    float amountPercent,
                    float offsetPercent)
{
    if (points.empty())
        return;

    const auto graph = bounds.reduced (0.0f, 2.0f);
    const auto first = scaledMsegPoint (points.front(), amountPercent, offsetPercent);
    juce::Path path;
    path.startNewSubPath (graph.getX() + first.x * graph.getWidth(),
                          graph.getBottom() - first.y * graph.getHeight());
    for (size_t i = 1; i < points.size(); ++i)
    {
        const auto point = scaledMsegPoint (points[i], amountPercent, offsetPercent);
        path.lineTo (graph.getX() + point.x * graph.getWidth(),
                     graph.getBottom() - point.y * graph.getHeight());
    }

    g.setColour (colour.withAlpha (0.14f));
    auto fill = path;
    fill.lineTo (graph.getRight(), graph.getBottom());
    fill.lineTo (graph.getX(), graph.getBottom());
    fill.closeSubPath();
    g.fillPath (fill);

    g.setColour (colour.withAlpha (0.92f));
    g.strokePath (path, juce::PathStrokeType (2.0f));
}
} // namespace

DevicePanel::DevicePanel (MacroOscAudioProcessor& processor, juce::Image backing, EditorFonts editorFonts)
    : audioProcessor (processor),
      backgroundImage (std::move (backing)),
      fonts (std::move (editorFonts))
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
    setBufferedToImage (true);
}

void DevicePanel::paint (juce::Graphics& g)
{
    const auto bounds = imageBounds();
    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, bounds, juce::RectanglePlacement::fillDestination);
    else
        g.fillAll (rackPanelColour);

    paintModelDisplay (g);
    paintMacroValue (g, "TIMBRE", ParamIDs::timbre, 0, cyanAccent);
    paintMacroValue (g, "COLOR", ParamIDs::color, 1, magentaAccent);
    paintMacroValue (g, "MODULATION", ParamIDs::modulation, 2, cyanAccent);
    paintMacroValue (g, "FM", ParamIDs::fmAmount, 3, paleAccent);
}

void DevicePanel::mouseDown (const juce::MouseEvent& event)
{
    activeDrag = targetForPosition (event.position);
    if (activeDrag.parameterId == nullptr)
        return;

    if (auto* parameter = parameterFor (audioProcessor, activeDrag.parameterId))
    {
        dragStartNormalised = parameter->getValue();
        dragStartPosition = event.position;
        dragging = true;
        beginGesture (activeDrag.parameterId);
    }
}

void DevicePanel::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging || activeDrag.parameterId == nullptr)
        return;

    const float pixels = (dragStartPosition.y - event.position.y)
        + ((event.position.x - dragStartPosition.x) * 0.35f);
    setParameterNormalised (audioProcessor,
                            activeDrag.parameterId,
                            dragStartNormalised + (pixels / activeDrag.pixelsPerUnit));
    repaint();
}

void DevicePanel::mouseUp (const juce::MouseEvent&)
{
    if (dragging && activeDrag.parameterId != nullptr)
        endGesture (activeDrag.parameterId);

    dragging = false;
    activeDrag = {};
}

void DevicePanel::mouseMove (const juce::MouseEvent& event)
{
    setMouseCursor (targetForPosition (event.position).parameterId != nullptr
                        ? juce::MouseCursor::UpDownResizeCursor
                        : juce::MouseCursor::NormalCursor);
}

void DevicePanel::mouseExit (const juce::MouseEvent&)
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void DevicePanel::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (dragging && activeDrag.parameterId != nullptr)
        endGesture (activeDrag.parameterId);

    dragging = false;
    activeDrag = targetForPosition (event.position);

    if (activeDrag.parameterId != nullptr)
        resetParameterToDefault (audioProcessor, activeDrag.parameterId);

    activeDrag = {};
    repaint();
}

juce::Rectangle<float> DevicePanel::imageBounds() const
{
    auto area = getLocalBounds().toFloat();
    const float ratio = backgroundImage.isValid()
        ? static_cast<float> (backgroundImage.getWidth()) / static_cast<float> (backgroundImage.getHeight())
        : 3.0f;

    const float candidateHeight = area.getWidth() / ratio;
    if (candidateHeight <= area.getHeight())
        return area.withSizeKeepingCentre (area.getWidth(), candidateHeight);

    return area.withSizeKeepingCentre (area.getHeight() * ratio, area.getHeight());
}

juce::Rectangle<float> DevicePanel::designRect (float x, float y, float w, float h) const
{
    const auto bounds = imageBounds();
    return {
        bounds.getX() + (x / kDesignWidth) * bounds.getWidth(),
        bounds.getY() + (y / kDesignHeight) * bounds.getHeight(),
        (w / kDesignWidth) * bounds.getWidth(),
        (h / kDesignHeight) * bounds.getHeight()
    };
}

DevicePanel::DragTarget DevicePanel::targetForPosition (juce::Point<float> position) const
{
    if (designRect (354.0f, 21.0f, 1069.0f, 269.0f).contains (position)
        || designRect (78.0f, 81.0f, 173.0f, 137.0f).contains (position))
        return { ParamIDs::model, 260.0f };

    constexpr std::array<float, 4> xs { 355.0f, 643.0f, 930.0f, 1218.0f };
    constexpr std::array<const char*, 4> ids {
        ParamIDs::timbre, ParamIDs::color, ParamIDs::modulation, ParamIDs::fmAmount
    };

    for (size_t i = 0; i < xs.size(); ++i)
        if (designRect (xs[i], 346.0f, 210.0f, 113.0f).contains (position))
            return { ids[i], 150.0f };

    return {};
}

float DevicePanel::parameterValue (const char* parameterId) const
{
    return rawParameterValue (audioProcessor, parameterId);
}

void DevicePanel::beginGesture (const char* parameterId)
{
    if (auto* parameter = parameterFor (audioProcessor, parameterId))
        parameter->beginChangeGesture();
}

void DevicePanel::endGesture (const char* parameterId)
{
    if (auto* parameter = parameterFor (audioProcessor, parameterId))
        parameter->endChangeGesture();
}

void DevicePanel::paintModelDisplay (juce::Graphics& g)
{
    const auto modelNames = braidsModelNames();
    const int modelIndex = juce::jlimit (0, modelNames.size() - 1, juce::roundToInt (parameterValue (ParamIDs::model)));
    const bool active = dragging && activeDrag.parameterId == ParamIDs::model;

    const auto badge = designRect (101.0f, 110.0f, 122.0f, 78.0f);
    drawDigitalSlots (g,
                      withHeight (fonts.digital, badge.getHeight() * 0.86f),
                      juce::String (modelIndex),
                      badge.reduced (2.0f),
                      2,
                      '0',
                      yellowAccent);

    auto screen = designRect (428.0f, 76.0f, 960.0f, 122.0f);
    const int pitch = juce::jmax (3, juce::roundToInt (screen.getHeight() * 0.08f));
    g.setColour (yellowAccent.withAlpha (active ? 0.06f : 0.04f));
    for (float y = screen.getY(); y < screen.getBottom(); y += static_cast<float> (pitch))
        for (float x = screen.getX(); x < screen.getRight(); x += static_cast<float> (pitch))
            g.fillRect (juce::Rectangle<float> (x + 1.0f, y + 1.0f, 1.0f, 1.0f));

    g.setColour (active ? yellowAccent.brighter (0.35f) : yellowAccent);
    g.setFont (withHeight (fonts.digital, screen.getHeight() * 0.92f));
    g.drawFittedText (modelDisplayText(), screen.toNearestInt(),
                      juce::Justification::centred, 1);

    const auto dashArea = designRect (386.0f, 246.0f, 1002.0f, 20.0f);
    const int dashCount = modelNames.size();
    const int activeDash = dashCount > 1 ? juce::roundToInt ((static_cast<float> (modelIndex) / static_cast<float> (dashCount - 1)) * static_cast<float> (dashCount - 1)) : 0;
    const float gap = juce::jmax (1.0f, dashArea.getWidth() / 180.0f);
    const float dashWidth = (dashArea.getWidth() - (static_cast<float> (dashCount - 1) * gap)) / static_cast<float> (dashCount);
    g.setColour (yellowAccent.withAlpha (0.18f));
    for (int i = 0; i < dashCount; ++i)
        g.fillRoundedRectangle (dashArea.getX() + static_cast<float> (i) * (dashWidth + gap),
                                dashArea.getY(),
                                dashWidth,
                                dashArea.getHeight(),
                                1.0f);

    g.setColour (yellowAccent.withAlpha (0.78f));
    for (int i = 0; i <= activeDash; ++i)
        g.fillRoundedRectangle (dashArea.getX() + static_cast<float> (i) * (dashWidth + gap),
                                dashArea.getY(),
                                dashWidth,
                                dashArea.getHeight(),
                                1.0f);
}

void DevicePanel::paintMacroValue (juce::Graphics& g, const char* label, const char* parameterId, int column, juce::Colour accent)
{
    juce::ignoreUnused (accent);
    constexpr std::array<float, 4> xs { 355.0f, 643.0f, 930.0f, 1218.0f };
    const float x = xs[static_cast<size_t> (column)];
    const bool active = dragging && activeDrag.parameterId == parameterId;
    const auto labelArea = designRect (x - 12.0f, 308.0f, 234.0f, 33.0f);
    const auto valueArea = designRect (x + 18.0f, 368.0f, 174.0f, 65.0f);

    g.setColour (textColour);
    g.setFont (withHeight (fonts.title, labelArea.getHeight() * 0.76f));
    g.drawFittedText (label, labelArea.toNearestInt(), juce::Justification::centred, 1);

    const int display = juce::roundToInt (juce::jlimit (0.0f, 1.0f, parameterValue (parameterId)) * 127.0f);
    drawDigitalSlots (g,
                      withHeight (fonts.digital, valueArea.getHeight() * 0.92f),
                      juce::String (display),
                      valueArea.reduced (2.0f),
                      3,
                      '0',
                      active ? yellowAccent.brighter (0.35f) : yellowAccent);
}

juce::String DevicePanel::modelDisplayText() const
{
    const auto names = braidsModelNames();
    const int index = juce::jlimit (0, names.size() - 1, juce::roundToInt (parameterValue (ParamIDs::model)));
    return names[index].replaceCharacter ('/', '-').replaceCharacter ('S', '5').substring (0, 10);
}

RackValueBox::RackValueBox (MacroOscAudioProcessor& processor, EditorFonts editorFonts)
    : audioProcessor (processor),
      fonts (std::move (editorFonts))
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setBufferedToImage (true);
}

void RackValueBox::configure (const juce::String& labelText,
                              const char* parameter,
                              juce::Colour accent,
                              const juce::String& unitText,
                              int decimals,
                              float displayMultiplier)
{
    label = labelText;
    parameterId = parameter;
    accentColour = accent;
    unit = unitText;
    decimalPlaces = decimals;
    multiplier = displayMultiplier;
    repaint();
}

void RackValueBox::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float normalised = parameterId != nullptr && parameterFor (audioProcessor, parameterId) != nullptr
        ? parameterFor (audioProcessor, parameterId)->getValue()
        : 0.0f;

    const bool knobMode = bounds.getHeight() >= 58.0f;
    if (knobMode)
    {
        if (dragging)
        {
            g.setColour (accentColour.withAlpha (0.08f));
            g.fillRoundedRectangle (bounds.reduced (3.0f), 6.0f);
        }

        constexpr float labelTopPadding = 7.0f;
        constexpr float labelHeight = 16.0f;
        constexpr float labelToKnobPadding = 8.0f;
        constexpr float valueHeight = 18.0f;
        constexpr float bottomPadding = 7.0f;

        const auto labelArea = bounds.withY (bounds.getY() + labelTopPadding)
                                      .withHeight (labelHeight)
                                      .reduced (5.0f, 0.0f)
                                      .toNearestInt();
        g.setFont (withHeight (fonts.label, 12.0f));
        g.setColour (accentColour);
        g.drawFittedText (label, labelArea, juce::Justification::centred, 1);

        const float availableKnobHeight = bounds.getHeight()
            - labelTopPadding
            - labelHeight
            - labelToKnobPadding
            - valueHeight
            - bottomPadding;
        const float knobSize = juce::jmin (bounds.getWidth() * 0.68f, availableKnobHeight);
        const auto knobArea = juce::Rectangle<float> (knobSize, knobSize)
            .withCentre ({ bounds.getCentreX(),
                           bounds.getY() + labelTopPadding + labelHeight + labelToKnobPadding + knobSize * 0.5f });
        drawGlassKnob (g, knobArea, accentColour, normalised, dragging);

        auto valueArea = bounds.withTop (knobArea.getBottom() + 3.0f).reduced (4.0f, 0.0f).toNearestInt();
        g.setFont (withHeight (fonts.label, 12.5f));
        g.setColour (textColour);
        const auto value = unit.isNotEmpty() ? displayValue() + " " + unit : displayValue();
        g.drawFittedText (value, valueArea, juce::Justification::centred, 1);
        return;
    }

    drawGlassPanel (g, bounds, boxColour, accentColour, 4.0f, false);

    auto area = getLocalBounds().reduced (8, 2);
    auto nameArea = area.removeFromLeft (juce::roundToInt (area.getWidth() * 0.33f));
    auto unitArea = area.removeFromRight (juce::roundToInt (area.getWidth() * 0.22f));
    g.setFont (withHeight (fonts.label, 15.0f));
    g.setColour (accentColour);
    g.drawFittedText (label, nameArea, juce::Justification::centredLeft, 1);

    g.setFont (withHeight (fonts.label, 12.0f));
    g.drawFittedText (unit, unitArea, juce::Justification::centredRight, 1);

    g.setFont (withHeight (fonts.label, 18.0f));
    g.setColour (textColour);
    g.drawFittedText (displayValue(), area, juce::Justification::centred, 1);

    g.setColour (accentColour);
    g.fillRect (bounds.withY (bounds.getBottom() - 4.0f).withHeight (3.0f).withWidth (bounds.getWidth() * normalised));
}

void RackValueBox::mouseDown (const juce::MouseEvent& event)
{
    if (parameterId == nullptr)
        return;

    if (auto* parameter = parameterFor (audioProcessor, parameterId))
    {
        dragStartNormalised = parameter->getValue();
        dragStartPosition = event.position;
        dragging = true;
        beginGesture();
    }
}

void RackValueBox::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging || parameterId == nullptr)
        return;

    const float pixels = (dragStartPosition.y - event.position.y)
        + ((event.position.x - dragStartPosition.x) * 0.35f);
    setParameterNormalised (audioProcessor, parameterId, dragStartNormalised + (pixels / 180.0f));
    repaint();
}

void RackValueBox::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        endGesture();
    dragging = false;
}

void RackValueBox::mouseDoubleClick (const juce::MouseEvent&)
{
    if (dragging)
        endGesture();

    dragging = false;

    if (parameterId != nullptr)
        resetParameterToDefault (audioProcessor, parameterId);

    repaint();
}

float RackValueBox::parameterValue() const
{
    return parameterId != nullptr ? rawParameterValue (audioProcessor, parameterId) : 0.0f;
}

void RackValueBox::setParameterValue (float value)
{
    if (parameterId != nullptr)
    {
        if (auto* parameter = parameterFor (audioProcessor, parameterId))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }
}

void RackValueBox::beginGesture()
{
    if (parameterId != nullptr)
        if (auto* parameter = parameterFor (audioProcessor, parameterId))
            parameter->beginChangeGesture();
}

void RackValueBox::endGesture()
{
    if (parameterId != nullptr)
        if (auto* parameter = parameterFor (audioProcessor, parameterId))
            parameter->endChangeGesture();
}

juce::String RackValueBox::displayValue() const
{
    return valueText (parameterValue() * multiplier, decimalPlaces);
}

MsegSlotStrip::MsegSlotStrip (MacroOscAudioProcessor& processor, EditorFonts editorFonts)
    : audioProcessor (processor),
      fonts (std::move (editorFonts))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setBufferedToImage (true);
}

void MsegSlotStrip::setActiveSlot (int slot)
{
    activeSlot = juce::jlimit (0, kMsegSlotCount - 1, slot);
    repaint();
}

void MsegSlotStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
    const float gap = 9.0f;
    const float slotWidth = (area.getWidth() - gap * 2.0f) / 3.0f;
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
    {
        auto slotBounds = area.removeFromLeft (slotWidth);
        paintSlot (g, slotBounds, slot);
        area.removeFromLeft (gap);
    }
}

void MsegSlotStrip::mouseDown (const juce::MouseEvent& event)
{
    auto area = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
    const float gap = 9.0f;
    const float slotWidth = (area.getWidth() - gap * 2.0f) / 3.0f;
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
    {
        auto slotBounds = area.removeFromLeft (slotWidth);
        if (slotBounds.contains (event.position))
        {
            if (onSlotSelected)
                onSlotSelected (slot);
            return;
        }
        area.removeFromLeft (gap);
    }
}

void MsegSlotStrip::paintSlot (juce::Graphics& g, juce::Rectangle<float> bounds, int slot)
{
    const bool active = slot == activeSlot;
    auto labelArea = bounds.removeFromTop (17.0f);
    auto tab = bounds.reduced (1.0f, 0.0f).withTrimmedBottom (1.0f);
    juce::ColourGradient fill (active ? juce::Colour (0xff14817d) : juce::Colour (0xff174b4c),
                               tab.getX(),
                               tab.getY(),
                               active ? boxColour : darkScreenColour,
                               tab.getRight(),
                               tab.getBottom(),
                               false);
    fill.addColour (0.42, active ? juce::Colour (0xff0d6d6b) : boxColour.withAlpha (0.82f));
    g.setGradientFill (fill);
    g.fillRoundedRectangle (tab, 2.0f);

    g.setColour (active ? yellowAccent : cyanAccent.withAlpha (0.45f));
    g.drawRoundedRectangle (tab, 2.0f, active ? 2.0f : 1.0f);
    g.setColour (juce::Colours::white.withAlpha (active ? 0.16f : 0.08f));
    g.drawRoundedRectangle (tab.reduced (2.0f), 1.0f, 1.0f);

    g.setFont (withHeight (fonts.label, 14.0f));
    g.setColour (textColour);
    g.drawFittedText ("MSEG " + juce::String (slot + 1), labelArea.toNearestInt().reduced (2, 0),
                      juce::Justification::centredLeft, 1);

    const int destinationIndex = juce::jlimit (0,
                                               msegDestinationNames().size() - 1,
                                               juce::roundToInt (rawParameterValue (audioProcessor,
                                                                                    ParamIDs::msegDestination[static_cast<size_t> (slot)])));
    g.setFont (withHeight (fonts.label, 10.5f));
    g.setColour (active ? yellowAccent : mutedTextColour.withAlpha (0.82f));
    g.drawFittedText (msegDestinationNames()[destinationIndex],
                      labelArea.toNearestInt().reduced (2, 0),
                      juce::Justification::centredRight,
                      1);

    drawMiniCurve (g,
                   audioProcessor.getMsegPoints (slot),
                   tab.withTrimmedTop (1.0f).withTrimmedBottom (1.0f),
                   active ? yellowAccent : cyanAccent,
                   rawParameterValue (audioProcessor, ParamIDs::msegAmount[static_cast<size_t> (slot)]),
                   rawParameterValue (audioProcessor, ParamIDs::msegOffset[static_cast<size_t> (slot)]));
}

MsegCurveEditor::MsegCurveEditor (MacroOscAudioProcessor& processor, EditorFonts editorFonts)
    : audioProcessor (processor),
      fonts (std::move (editorFonts)),
      points (processor.getMsegPoints (0))
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    setBufferedToImage (true);
}

void MsegCurveEditor::setActiveSlot (int slot)
{
    activeSlot = juce::jlimit (0, kMsegSlotCount - 1, slot);
    setPoints (audioProcessor.getMsegPoints (activeSlot));
}

void MsegCurveEditor::setPoints (std::vector<MsegPoint> newPoints)
{
    if (editing)
        return;

    points = MsegShape::sanitize (std::move (newPoints));
    selectedIndex = -1;
    repaint();
}

void MsegCurveEditor::paint (juce::Graphics& g)
{
    const auto graph = graphBounds();

    g.setColour (darkScreenColour.withAlpha (0.18f));
    g.fillRoundedRectangle (graph, 2.0f);
    g.setColour (cyanAccent.withAlpha (0.12f));
    g.drawRoundedRectangle (graph.reduced (0.5f), 2.0f, 1.0f);

    g.setColour (juce::Colour (0xff123235).withAlpha (0.82f));
    for (int i = 1; i < 8; ++i)
    {
        const float x = graph.getX() + graph.getWidth() * (static_cast<float> (i) / 8.0f);
        g.drawVerticalLine (juce::roundToInt (x), graph.getY(), graph.getBottom());
    }
    for (int i = 1; i < 4; ++i)
    {
        const float y = graph.getY() + graph.getHeight() * (static_cast<float> (i) / 4.0f);
        g.drawHorizontalLine (juce::roundToInt (y), graph.getX(), graph.getRight());
    }

    g.setColour (yellowAccent.withAlpha (0.25f));
    g.drawHorizontalLine (juce::roundToInt (graph.getCentreY()), graph.getX(), graph.getRight());

    const auto& safePoints = points;
    juce::Path path;
    if (! safePoints.empty())
    {
        path.startNewSubPath (pointToScreen (safePoints.front()));
        for (size_t i = 1; i < safePoints.size(); ++i)
            path.lineTo (pointToScreen (safePoints[i]));

        auto fill = path;
        fill.lineTo (graph.getRight(), graph.getBottom());
        fill.lineTo (graph.getX(), graph.getBottom());
        fill.closeSubPath();

        g.setColour (yellowAccent.withAlpha (0.18f));
        g.fillPath (fill);

        juce::Path appliedPath;
        appliedPath.startNewSubPath (pointToScreen (pointWithScaleAndOffset (safePoints.front())));
        for (size_t i = 1; i < safePoints.size(); ++i)
            appliedPath.lineTo (pointToScreen (pointWithScaleAndOffset (safePoints[i])));

        auto appliedFill = appliedPath;
        appliedFill.lineTo (graph.getRight(), graph.getBottom());
        appliedFill.lineTo (graph.getX(), graph.getBottom());
        appliedFill.closeSubPath();

        g.setColour (paleAccent.withAlpha (0.075f));
        g.fillPath (appliedFill);

        g.setColour (paleAccent.withAlpha (0.46f));
        g.strokePath (appliedPath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (yellowAccent);
        g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    for (size_t i = 0; i < safePoints.size(); ++i)
    {
        const auto p = pointToScreen (safePoints[i]);
        const bool selected = static_cast<int> (i) == selectedIndex;
        const bool hovered = static_cast<int> (i) == hoveredIndex;
        const float size = selected ? 11.0f : (hovered ? 10.0f : 8.0f);
        const auto handle = juce::Rectangle<float> (size, size).withCentre (p);
        if (hovered && ! selected)
        {
            g.setColour (paleAccent.withAlpha (0.22f));
            g.fillEllipse (handle.expanded (3.0f));
        }
        g.setColour (selected ? textColour : (hovered ? paleAccent : yellowAccent));
        g.fillEllipse (handle);
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawEllipse (handle, selected || hovered ? 1.3f : 1.0f);
    }

    const auto paintAxisSlider = [&] (juce::Rectangle<float> rail,
                                      const juce::String& label,
                                      const juce::String& value,
                                      float normalised,
                                      bool horizontal,
                                      bool active,
                                      bool hovered)
    {
        normalised = juce::jlimit (0.0f, 1.0f, normalised);
        const bool highlighted = active || hovered;
        g.setColour (juce::Colours::black.withAlpha (0.34f));
        if (horizontal)
        {
            g.fillRoundedRectangle (rail.withHeight (4.0f).withCentre (rail.getCentre()), 2.0f);
            g.setColour (yellowAccent.withAlpha (highlighted ? 0.98f : 0.72f));
            g.fillRoundedRectangle (rail.withHeight (4.0f).withWidth (rail.getWidth() * normalised)
                                        .withY (rail.getCentreY() - 2.0f),
                                    2.0f);

            const float handleX = rail.getX() + rail.getWidth() * normalised;
            const float handleSize = active ? 11.0f : (hovered ? 10.0f : 9.0f);
            const auto handle = juce::Rectangle<float> (handleSize, handleSize).withCentre ({ handleX, rail.getCentreY() });
            if (hovered && ! active)
            {
                g.setColour (paleAccent.withAlpha (0.20f));
                g.fillEllipse (handle.expanded (3.0f));
            }
            g.setColour (active ? textColour : (hovered ? paleAccent : yellowAccent));
            g.fillEllipse (handle);
            g.setColour (juce::Colours::black.withAlpha (0.62f));
            g.drawEllipse (handle, highlighted ? 1.3f : 1.0f);

            const auto labelArea = rail.withY (rail.getBottom() + 3.0f).withHeight (14.0f);
            g.setFont (withHeight (fonts.label, 10.0f));
            g.setColour (mutedTextColour.withAlpha (0.90f));
            g.drawText (label.toLowerCase() + "   " + value,
                        labelArea.toNearestInt(),
                        juce::Justification::centred,
                        false);
            return;
        }

        const float handleY = rail.getBottom() - rail.getHeight() * normalised;
        g.fillRoundedRectangle (rail.withWidth (4.0f).withCentre (rail.getCentre()), 2.0f);
        g.setColour (yellowAccent.withAlpha (highlighted ? 0.98f : 0.70f));
        g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, rail.getBottom() - handleY)
                                    .withX (rail.getCentreX() - 2.0f)
                                    .withY (handleY),
                                2.0f);

        const float handleSize = active ? 11.0f : (hovered ? 10.0f : 9.0f);
        const auto handle = juce::Rectangle<float> (handleSize, handleSize).withCentre ({ rail.getCentreX(), handleY });
        if (hovered && ! active)
        {
            g.setColour (paleAccent.withAlpha (0.20f));
            g.fillEllipse (handle.expanded (3.0f));
        }
        g.setColour (active ? textColour : (hovered ? paleAccent : yellowAccent));
        g.fillEllipse (handle);
        g.setColour (juce::Colours::black.withAlpha (0.62f));
        g.drawEllipse (handle, highlighted ? 1.3f : 1.0f);

        const bool leftSide = rail.getCentreX() < graph.getCentreX();
        const auto textBounds = leftSide ? offsetLabelBounds() : scaleLabelBounds();
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                         textBounds.getCentreX(),
                                                         textBounds.getCentreY()));
        g.setFont (withHeight (fonts.label, 10.0f));
        g.setColour (mutedTextColour.withAlpha (0.92f));
        g.drawText (label.toLowerCase(), textBounds.toNearestInt(), juce::Justification::centred, false);
        g.restoreState();
    };

    const auto offsetParameterId = ParamIDs::msegOffset[static_cast<size_t> (activeSlot)];
    const auto scaleParameterId = ParamIDs::msegAmount[static_cast<size_t> (activeSlot)];
    const auto durationParameterId = ParamIDs::msegRate[static_cast<size_t> (activeSlot)];
    const bool durationIsSynced = rawParameterValue (audioProcessor,
                                                     ParamIDs::msegSync[static_cast<size_t> (activeSlot)]) >= 0.5f;
    const float durationValue = rawParameterValue (audioProcessor, durationParameterId);
    float durationNormalised = normalisedParameterValue (durationParameterId);
    if (durationIsSynced)
    {
        if (auto* parameter = parameterFor (audioProcessor, durationParameterId))
            durationNormalised = parameter->convertTo0to1 (snapMsegSyncedBeats (durationValue));
    }

    paintAxisSlider (offsetSliderBounds(),
                     "Offset",
                     valueText (rawParameterValue (audioProcessor, offsetParameterId), 1) + "%",
                     normalisedParameterValue (offsetParameterId),
                     false,
                     dragMode == DragMode::offset,
                     hoveredSlider == DragMode::offset);
    paintAxisSlider (scaleSliderBounds(),
                     "Scale",
                     valueText (rawParameterValue (audioProcessor, scaleParameterId), 1) + "%",
                     normalisedParameterValue (scaleParameterId),
                     false,
                     dragMode == DragMode::scale,
                     hoveredSlider == DragMode::scale);
    paintAxisSlider (durationSliderBounds(),
                     "Duration",
                     durationIsSynced ? msegSyncedDurationLabel (durationValue)
                                      : valueText (durationValue, 2) + "s",
                     durationNormalised,
                     true,
                     dragMode == DragMode::duration,
                     hoveredSlider == DragMode::duration);
}

void MsegCurveEditor::mouseDown (const juce::MouseEvent& event)
{
    const auto position = event.position;
    if (event.mods.isRightButtonDown())
    {
        editing = true;
        dragMode = DragMode::curvePoint;
        deletePointNear (position);
        repaint();
        return;
    }

    const int pointIndex = findNearestPoint (position);
    if (pointIndex >= 0 && position.getDistanceFrom (pointToScreen (points[static_cast<size_t> (pointIndex)])) <= 13.0f)
    {
        editing = true;
        dragMode = DragMode::curvePoint;
        selectedIndex = pointIndex;
        repaint();
        return;
    }

    dragMode = sliderAtPosition (position);
    if (dragMode != DragMode::none)
    {
        beginParameterGesture (dragMode);
        setSliderFromPosition (dragMode, position);
        repaint();
        return;
    }

    editing = true;
    dragMode = DragMode::curvePoint;
    selectedIndex = findNearestPoint (position);

    if (selectedIndex < 0)
    {
        points.push_back (screenToPoint (position));
        points = MsegShape::sanitize (std::move (points));
        selectedIndex = findNearestPoint (position);
        commit (false);
    }

    repaint();
}

void MsegCurveEditor::mouseDrag (const juce::MouseEvent& event)
{
    if (dragMode == DragMode::offset || dragMode == DragMode::scale || dragMode == DragMode::duration)
    {
        setSliderFromPosition (dragMode, event.position);
        repaint();
        return;
    }

    if (event.mods.isRightButtonDown())
    {
        deletePointNear (event.position);
        repaint();
        return;
    }

    if (dragMode != DragMode::curvePoint)
        return;

    if (selectedIndex < 0 || selectedIndex >= static_cast<int> (points.size()))
        return;

    auto next = screenToPoint (event.position);
    if (selectedIndex == 0)
    {
        next.x = 0.0f;
    }
    else if (selectedIndex == static_cast<int> (points.size()) - 1)
    {
        next.x = 1.0f;
    }
    else
    {
        const float leftLimit = points[static_cast<size_t> (selectedIndex - 1)].x;
        const float rightLimit = points[static_cast<size_t> (selectedIndex + 1)].x;
        next.x = juce::jlimit (leftLimit, rightLimit, next.x);
    }

    points[static_cast<size_t> (selectedIndex)] = next;
    audioProcessor.setMsegPoints (activeSlot, points, false);
    repaint();
}

void MsegCurveEditor::mouseUp (const juce::MouseEvent& event)
{
    if (dragMode == DragMode::offset || dragMode == DragMode::scale || dragMode == DragMode::duration)
    {
        endParameterGesture (dragMode);
        dragMode = DragMode::none;
        updateHoverTarget (event.position);
        repaint();
        return;
    }

    editing = false;
    if (dragMode == DragMode::curvePoint)
        commit (true);
    dragMode = DragMode::none;
    updateHoverTarget (event.position);
    repaint();
}

void MsegCurveEditor::mouseMove (const juce::MouseEvent& event)
{
    updateHoverTarget (event.position);
}

void MsegCurveEditor::mouseExit (const juce::MouseEvent&)
{
    if (hoveredIndex != -1 || hoveredSlider != DragMode::none)
    {
        hoveredIndex = -1;
        hoveredSlider = DragMode::none;
        repaint();
    }
}

void MsegCurveEditor::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto slider = sliderAtPosition (event.position);
    if (slider != DragMode::none)
    {
        if (const auto* parameterId = parameterForDragMode (slider))
            resetParameterToDefault (audioProcessor, parameterId);

        repaint();
    }
}

juce::Rectangle<float> MsegCurveEditor::graphBounds() const
{
    return getLocalBounds().toFloat()
        .reduced (4.0f, 3.0f)
        .withTrimmedLeft (26.0f)
        .withTrimmedRight (26.0f)
        .withTrimmedBottom (32.0f);
}

juce::Rectangle<float> MsegCurveEditor::offsetSliderBounds() const
{
    const auto graph = graphBounds();
    return { graph.getX() - 18.0f, graph.getY(), 8.0f, graph.getHeight() };
}

juce::Rectangle<float> MsegCurveEditor::scaleSliderBounds() const
{
    const auto graph = graphBounds();
    return { graph.getRight() + 10.0f, graph.getY(), 8.0f, graph.getHeight() };
}

juce::Rectangle<float> MsegCurveEditor::durationSliderBounds() const
{
    const auto graph = graphBounds();
    const auto leftSlider = offsetSliderBounds();
    const auto rightSlider = scaleSliderBounds();
    const float x = leftSlider.getCentreX();
    const float right = rightSlider.getCentreX();
    return { x, graph.getBottom() + 6.0f, right - x, 8.0f };
}

juce::Rectangle<float> MsegCurveEditor::offsetLabelBounds() const
{
    const auto rail = offsetSliderBounds();
    const float centreX = rail.getX() - 8.0f;
    return { centreX - rail.getHeight() * 0.5f,
             rail.getCentreY() - 6.5f,
             rail.getHeight(),
             13.0f };
}

juce::Rectangle<float> MsegCurveEditor::scaleLabelBounds() const
{
    const auto rail = scaleSliderBounds();
    const float centreX = rail.getRight() + 8.0f;
    return { centreX - rail.getHeight() * 0.5f,
             rail.getCentreY() - 6.5f,
             rail.getHeight(),
             13.0f };
}

juce::Point<float> MsegCurveEditor::pointToScreen (const MsegPoint& point) const
{
    const auto graph = graphBounds();
    return {
        graph.getX() + point.x * graph.getWidth(),
        graph.getBottom() - point.y * graph.getHeight()
    };
}

MsegPoint MsegCurveEditor::screenToPoint (juce::Point<float> position) const
{
    const auto graph = graphBounds();
    return {
        juce::jlimit (0.0f, 1.0f, (position.x - graph.getX()) / graph.getWidth()),
        juce::jlimit (0.0f, 1.0f, 1.0f - ((position.y - graph.getY()) / graph.getHeight()))
    };
}

MsegPoint MsegCurveEditor::pointWithScaleAndOffset (const MsegPoint& point) const
{
    return scaledMsegPoint (
        point,
        rawParameterValue (audioProcessor, ParamIDs::msegAmount[static_cast<size_t> (activeSlot)]),
        rawParameterValue (audioProcessor, ParamIDs::msegOffset[static_cast<size_t> (activeSlot)]));
}

int MsegCurveEditor::findNearestPoint (juce::Point<float> position) const
{
    int nearest = -1;
    float nearestDistance = 24.0f;

    for (size_t i = 0; i < points.size(); ++i)
    {
        const float distance = position.getDistanceFrom (pointToScreen (points[i]));
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = static_cast<int> (i);
        }
    }

    return nearest;
}

juce::Rectangle<float> MsegCurveEditor::sliderHandleBounds (DragMode mode) const
{
    const bool horizontal = mode == DragMode::duration;
    const auto rail = horizontal ? durationSliderBounds()
                                 : (mode == DragMode::offset ? offsetSliderBounds() : scaleSliderBounds());
    const float normalised = sliderDisplayNormalised (mode);

    if (horizontal)
    {
        const float handleX = rail.getX() + rail.getWidth() * normalised;
        return juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ handleX, rail.getCentreY() });
    }

    const float handleY = rail.getBottom() - rail.getHeight() * normalised;
    return juce::Rectangle<float> (11.0f, 11.0f).withCentre ({ rail.getCentreX(), handleY });
}

MsegCurveEditor::DragMode MsegCurveEditor::sliderHandleAtPosition (juce::Point<float> position) const
{
    if (sliderHandleBounds (DragMode::offset).expanded (5.0f).contains (position))
        return DragMode::offset;

    if (sliderHandleBounds (DragMode::scale).expanded (5.0f).contains (position))
        return DragMode::scale;

    if (sliderHandleBounds (DragMode::duration).expanded (5.0f).contains (position))
        return DragMode::duration;

    return DragMode::none;
}

MsegCurveEditor::DragMode MsegCurveEditor::sliderAtPosition (juce::Point<float> position) const
{
    auto offsetHit = offsetSliderBounds().expanded (6.0f, 4.0f);
    offsetHit.setRight (offsetSliderBounds().getRight() + 2.0f);
    if (offsetHit.contains (position))
        return DragMode::offset;

    auto scaleHit = scaleSliderBounds().expanded (6.0f, 4.0f);
    scaleHit.setLeft (scaleSliderBounds().getX() - 2.0f);
    if (scaleHit.contains (position))
        return DragMode::scale;

    if (durationSliderBounds().expanded (4.0f, 7.0f).contains (position))
        return DragMode::duration;

    return DragMode::none;
}

const char* MsegCurveEditor::parameterForDragMode (DragMode mode) const
{
    switch (mode)
    {
        case DragMode::offset:
            return ParamIDs::msegOffset[static_cast<size_t> (activeSlot)];
        case DragMode::scale:
            return ParamIDs::msegAmount[static_cast<size_t> (activeSlot)];
        case DragMode::duration:
            return ParamIDs::msegRate[static_cast<size_t> (activeSlot)];
        case DragMode::none:
        case DragMode::curvePoint:
            break;
    }

    return nullptr;
}

float MsegCurveEditor::normalisedParameterValue (const char* parameterId) const
{
    if (parameterId != nullptr)
        if (auto* parameter = parameterFor (audioProcessor, parameterId))
            return parameter->getValue();

    return 0.0f;
}

float MsegCurveEditor::sliderDisplayNormalised (DragMode mode) const
{
    const auto* parameterId = parameterForDragMode (mode);
    if (parameterId == nullptr)
        return 0.0f;

    if (mode == DragMode::duration
        && rawParameterValue (audioProcessor, ParamIDs::msegSync[static_cast<size_t> (activeSlot)]) >= 0.5f)
    {
        if (auto* parameter = parameterFor (audioProcessor, parameterId))
            return parameter->convertTo0to1 (snapMsegSyncedBeats (rawParameterValue (audioProcessor, parameterId)));
    }

    return normalisedParameterValue (parameterId);
}

void MsegCurveEditor::updateHoverTarget (juce::Point<float> position)
{
    if (dragMode != DragMode::none)
        return;

    int nextHoveredIndex = -1;
    const int pointIndex = findNearestPoint (position);
    if (pointIndex >= 0 && position.getDistanceFrom (pointToScreen (points[static_cast<size_t> (pointIndex)])) <= 13.0f)
        nextHoveredIndex = pointIndex;

    const auto nextHoveredSlider = nextHoveredIndex >= 0 ? DragMode::none : sliderHandleAtPosition (position);
    if (nextHoveredIndex == hoveredIndex && nextHoveredSlider == hoveredSlider)
        return;

    hoveredIndex = nextHoveredIndex;
    hoveredSlider = nextHoveredSlider;

    if (hoveredIndex >= 0 || hoveredSlider != DragMode::none)
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor (juce::MouseCursor::CrosshairCursor);

    repaint();
}

void MsegCurveEditor::setSliderFromPosition (DragMode mode, juce::Point<float> position)
{
    const auto* parameterId = parameterForDragMode (mode);
    if (parameterId == nullptr)
        return;

    const bool horizontal = mode == DragMode::duration;
    const auto rail = horizontal ? durationSliderBounds()
                                 : (mode == DragMode::offset ? offsetSliderBounds() : scaleSliderBounds());
    const float normalised = juce::jlimit (0.0f,
                                           1.0f,
                                           horizontal ? (position.x - rail.getX()) / rail.getWidth()
                                                      : 1.0f - ((position.y - rail.getY()) / rail.getHeight()));

    if (mode == DragMode::duration
        && rawParameterValue (audioProcessor, ParamIDs::msegSync[static_cast<size_t> (activeSlot)]) >= 0.5f)
    {
        if (auto* parameter = parameterFor (audioProcessor, parameterId))
            setParameterNormalised (audioProcessor, parameterId,
                                    parameter->convertTo0to1 (snapMsegSyncedBeats (parameter->convertFrom0to1 (normalised))));
        return;
    }

    setParameterNormalised (audioProcessor, parameterId, normalised);
}

void MsegCurveEditor::beginParameterGesture (DragMode mode)
{
    if (const auto* parameterId = parameterForDragMode (mode))
        if (auto* parameter = parameterFor (audioProcessor, parameterId))
            parameter->beginChangeGesture();
}

void MsegCurveEditor::endParameterGesture (DragMode mode)
{
    if (const auto* parameterId = parameterForDragMode (mode))
        if (auto* parameter = parameterFor (audioProcessor, parameterId))
            parameter->endChangeGesture();
}

bool MsegCurveEditor::deletePointNear (juce::Point<float> position)
{
    const int index = findNearestPoint (position);
    if (index <= 0 || index >= static_cast<int> (points.size()) - 1)
        return false;

    points.erase (points.begin() + index);
    selectedIndex = -1;
    audioProcessor.setMsegPoints (activeSlot, points, false);
    return true;
}

void MsegCurveEditor::commit (bool notifyEditor)
{
    if (notifyEditor)
        points = MsegShape::sanitize (std::move (points));

    audioProcessor.setMsegPoints (activeSlot, points, notifyEditor);
}

MacroOscAudioProcessorEditor::MacroOscAudioProcessorEditor (MacroOscAudioProcessor& processorRef)
    : AudioProcessorEditor (&processorRef),
      audioProcessor (processorRef),
      fonts (loadFonts()),
      devicePanel (processorRef,
                   juce::ImageCache::getFromMemory (BinaryData::macro_oscillator_backing_png,
                                                     BinaryData::macro_oscillator_backing_pngSize),
                   fonts),
      pitchBox (processorRef, fonts),
      detuneBox (processorRef, fonts),
      portaBox (processorRef, fonts),
      attackBox (processorRef, fonts),
      decayBox (processorRef, fonts),
      sustainBox (processorRef, fonts),
      releaseBox (processorRef, fonts),
      msegSlots (processorRef, fonts),
      msegEditor (processorRef, fonts)
{
    addAndMakeVisible (devicePanel);
    addAndMakeVisible (pitchBox);
    addAndMakeVisible (detuneBox);
    addAndMakeVisible (portaBox);
    addAndMakeVisible (attackBox);
    addAndMakeVisible (decayBox);
    addAndMakeVisible (sustainBox);
    addAndMakeVisible (releaseBox);
    addAndMakeVisible (msegSlots);
    addAndMakeVisible (msegEditor);
    addAndMakeVisible (destinationBox);
    addAndMakeVisible (loopButton);
    addAndMakeVisible (syncButton);

    pitchBox.configure ("Pitch", ParamIDs::coarse, yellowAccent, "st", 0);
    detuneBox.configure ("Detune", ParamIDs::fine, yellowAccent, "ct", 1, 100.0f);
    portaBox.configure ("Porta", ParamIDs::portamento, yellowAccent, "s", 3);
    attackBox.configure ("A", ParamIDs::attack, cyanAccent, "%", 1, 10.0f);
    decayBox.configure ("D", ParamIDs::decay, cyanAccent, "%", 1, 10.0f);
    sustainBox.configure ("S", ParamIDs::sustain, cyanAccent, "%", 1, 100.0f);
    releaseBox.configure ("R", ParamIDs::release, cyanAccent, "%", 1, 10.0f);

    styleMsegControls();
    destinationBox.addMouseListener (this, true);
    loopButton.addMouseListener (this, true);
    syncButton.addMouseListener (this, true);
    msegSlots.onSlotSelected = [this] (int slot) { setActiveMsegSlot (slot); };
    setActiveMsegSlot (0);

    audioProcessor.addChangeListener (this);
    for (auto* parameterId : kUiParameterIds)
        audioProcessor.getState().addParameterListener (parameterId, this);

    startTimerHz (30);

    setResizable (false, false);
    setSize (kEditorWidth, kEditorHeight);
}

MacroOscAudioProcessorEditor::~MacroOscAudioProcessorEditor()
{
    stopTimer();
    for (auto* parameterId : kUiParameterIds)
        audioProcessor.getState().removeParameterListener (parameterId, this);

    destinationBox.removeMouseListener (this);
    loopButton.removeMouseListener (this);
    syncButton.removeMouseListener (this);
    audioProcessor.removeChangeListener (this);
}

void MacroOscAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    auto area = getLocalBounds().toFloat().reduced (8.0f);
    drawGlassPanel (g, area, rackPanelColour.withAlpha (0.70f), cyanAccent, 8.0f, false);

    auto content = getLocalBounds().reduced (14);
    const int deviceHeight = juce::roundToInt (static_cast<float> (content.getWidth()) / 3.0f);
    content.removeFromTop (deviceHeight);
    content.removeFromTop (8);
    auto controlsArea = content.removeFromLeft (kBottomControlBankWidth);
    content.removeFromLeft (kBottomColumnGap);
    auto msegArea = content;

    const auto paintMinimalSectionBox = [&] (juce::Rectangle<int> sectionArea, juce::Colour accent)
    {
        auto box = sectionArea.toFloat().reduced (1.0f);
        g.setColour (accent.withAlpha (0.055f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (accent.withAlpha (0.16f));
        g.drawRoundedRectangle (box, 5.0f, 1.0f);
    };

    paintMinimalSectionBox (msegArea, cyanAccent);

    auto controlsSections = controlsArea;
    constexpr int groupHeaderHeight = 22;
    constexpr int groupGap = 8;
    const int groupRowHeight = (controlsSections.getHeight() - groupGap - (groupHeaderHeight * 2)) / 2;
    const auto paintGroupSection = [&] (juce::Rectangle<int> sectionArea, const juce::String& title, juce::Colour accent)
    {
        paintMinimalSectionBox (sectionArea, accent);

        auto titleArea = sectionArea.removeFromTop (groupHeaderHeight);
        g.setFont (withHeight (fonts.title, 14.0f));
        g.setColour (textColour);
        g.drawFittedText (title, titleArea.reduced (6, 0), juce::Justification::centredLeft, 1);

        g.setColour (accent.withAlpha (0.26f));
        g.fillRoundedRectangle (titleArea.toFloat().withY (titleArea.getBottom() - 1).withHeight (1.0f).reduced (7.0f, 0.0f),
                                0.5f);
    };

    paintGroupSection (controlsSections.removeFromTop (groupHeaderHeight + groupRowHeight), "PITCH", yellowAccent);
    controlsSections.removeFromTop (groupGap);
    paintGroupSection (controlsSections, "ENV.", cyanAccent);
}

void MacroOscAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (14);

    const int deviceHeight = juce::roundToInt (static_cast<float> (area.getWidth()) / 3.0f);
    devicePanel.setBounds (area.removeFromTop (deviceHeight));
    area.removeFromTop (8);

    auto bottomArea = area;
    auto controlsArea = bottomArea.removeFromLeft (kBottomControlBankWidth);
    bottomArea.removeFromLeft (kBottomColumnGap);
    auto msegArea = bottomArea;

    auto controlsInner = controlsArea.reduced (10, 0);
    constexpr int groupHeaderHeight = 22;
    constexpr int groupGap = 8;
    const int groupRowHeight = (controlsInner.getHeight() - groupGap - (groupHeaderHeight * 2)) / 2;
    controlsInner.removeFromTop (groupHeaderHeight);
    auto pitchRow = controlsInner.removeFromTop (groupRowHeight).reduced (5, 2);
    const auto layoutKnobRow = [] (juce::Rectangle<int> row, const auto& boxes)
    {
        constexpr int gap = 8;
        const int count = static_cast<int> (boxes.size());
        const int cellWidth = (row.getWidth() - gap * (count - 1)) / count;
        for (auto* box : boxes)
        {
            box->setBounds (row.removeFromLeft (cellWidth));
            row.removeFromLeft (gap);
        }
    };

    layoutKnobRow (pitchRow, std::array<RackValueBox*, 3> { &pitchBox, &detuneBox, &portaBox });

    controlsInner.removeFromTop (groupGap);
    controlsInner.removeFromTop (groupHeaderHeight);
    auto envRow = controlsInner.reduced (5, 2);
    layoutKnobRow (envRow, std::array<RackValueBox*, 4> { &attackBox, &decayBox, &sustainBox, &releaseBox });

    constexpr int msegTabHeight = 66;
    auto tabRow = msegArea.removeFromTop (msegTabHeight).reduced (6, 0);
    msegSlots.setBounds (tabRow);

    auto msegBody = msegArea.withTrimmedTop (3).reduced (10, 8);
    auto header = msegBody.removeFromTop (29);
    constexpr int destinationWidth = 126;
    constexpr int toggleWidth = 70;
    constexpr int controlGap = 18;
    constexpr int headerWidth = destinationWidth + (toggleWidth * 2) + (controlGap * 2);
    auto headerGroup = juce::Rectangle<int> (headerWidth, 28).withCentre ({ header.getCentreX(), header.getCentreY() });
    destinationBox.setBounds (headerGroup.removeFromLeft (destinationWidth).reduced (1, 0));
    headerGroup.removeFromLeft (controlGap);
    loopButton.setBounds (headerGroup.removeFromLeft (toggleWidth));
    headerGroup.removeFromLeft (controlGap);
    syncButton.setBounds (headerGroup.removeFromLeft (toggleWidth));
    msegBody.removeFromTop (2);
    msegEditor.setBounds (msegBody);
}

void MacroOscAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! msegEditor.isEditing())
        msegEditor.setPoints (audioProcessor.getMsegPoints (activeMsegSlot));

    msegSlots.repaint();
}

void MacroOscAudioProcessorEditor::parameterChanged (const juce::String&, float)
{
    parameterUiDirty.store (true, std::memory_order_relaxed);
}

void MacroOscAudioProcessorEditor::timerCallback()
{
    if (! parameterUiDirty.exchange (false, std::memory_order_relaxed))
        return;

    devicePanel.repaint();
    pitchBox.repaint();
    detuneBox.repaint();
    portaBox.repaint();
    attackBox.repaint();
    decayBox.repaint();
    sustainBox.repaint();
    releaseBox.repaint();
    msegEditor.repaint();
    msegSlots.repaint();
    destinationBox.repaint();
    loopButton.repaint();
    syncButton.repaint();
}

void MacroOscAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto editorPosition = getLocalPoint (event.eventComponent, event.getPosition());

    if (destinationBox.getBounds().contains (editorPosition))
    {
        resetParameterToDefault (audioProcessor, ParamIDs::msegDestination[static_cast<size_t> (activeMsegSlot)]);
        destinationBox.repaint();
        msegSlots.repaint();
        return;
    }

    if (loopButton.getBounds().contains (editorPosition))
    {
        resetParameterToDefault (audioProcessor, ParamIDs::msegLoop[static_cast<size_t> (activeMsegSlot)]);
        loopButton.repaint();
        msegSlots.repaint();
        return;
    }

    if (syncButton.getBounds().contains (editorPosition))
    {
        resetParameterToDefault (audioProcessor, ParamIDs::msegSync[static_cast<size_t> (activeMsegSlot)]);
        syncButton.repaint();
        msegEditor.repaint();
        msegSlots.repaint();
    }
}

void MacroOscAudioProcessorEditor::setActiveMsegSlot (int slot)
{
    activeMsegSlot = juce::jlimit (0, kMsegSlotCount - 1, slot);
    msegSlots.setActiveSlot (activeMsegSlot);
    msegEditor.setActiveSlot (activeMsegSlot);
    configureMsegAttachments();
    resized();
}

void MacroOscAudioProcessorEditor::configureMsegAttachments()
{
    destinationAttachment.reset();
    loopAttachment.reset();
    syncAttachment.reset();
    destinationAttachment = std::make_unique<ComboBoxAttachment> (
        audioProcessor.getState(),
        ParamIDs::msegDestination[static_cast<size_t> (activeMsegSlot)],
        destinationBox);
    loopAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getState(),
        ParamIDs::msegLoop[static_cast<size_t> (activeMsegSlot)],
        loopButton);
    syncAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getState(),
        ParamIDs::msegSync[static_cast<size_t> (activeMsegSlot)],
        syncButton);
}

void MacroOscAudioProcessorEditor::styleMsegControls()
{
    destinationBox.addItemList (msegDestinationNames(), 1);
    destinationBox.setJustificationType (juce::Justification::centred);
    destinationBox.setColour (juce::ComboBox::backgroundColourId, darkScreenColour.withAlpha (0.34f));
    destinationBox.setColour (juce::ComboBox::outlineColourId, yellowAccent.withAlpha (0.0f));
    destinationBox.setColour (juce::ComboBox::textColourId, yellowAccent);
    destinationBox.setColour (juce::ComboBox::arrowColourId, textColour.withAlpha (0.88f));
    destinationBox.setColour (juce::PopupMenu::backgroundColourId, boxColour);
    destinationBox.setColour (juce::PopupMenu::textColourId, textColour);

    const auto styleToggle = [] (juce::ToggleButton& button, const juce::String& text)
    {
        button.setButtonText (text);
        button.setColour (juce::ToggleButton::textColourId, textColour);
        button.setColour (juce::ToggleButton::tickColourId, yellowAccent);
        button.setColour (juce::ToggleButton::tickDisabledColourId, mutedTextColour);
    };

    styleToggle (loopButton, "Loop");
    styleToggle (syncButton, "Sync");
}

EditorFonts MacroOscAudioProcessorEditor::loadFonts()
{
    auto dseg = juce::Typeface::createSystemTypefaceFor (BinaryData::DSEG14ModernRegular_ttf,
                                                         BinaryData::DSEG14ModernRegular_ttfSize);
    auto interBold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf,
                                                              BinaryData::InterBold_ttfSize);
    auto interSemi = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf,
                                                              BinaryData::InterSemiBold_ttfSize);

    EditorFonts result;
    result.digital = juce::Font (juce::FontOptions (dseg).withHeight (28.0f));
    result.label = juce::Font (juce::FontOptions (interSemi).withHeight (14.0f));
    result.title = juce::Font (juce::FontOptions (interBold).withHeight (22.0f));
    return result;
}
} // namespace macro_osc
