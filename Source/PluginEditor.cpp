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
const juce::Colour rowColour { 0xff0b6765 };
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
constexpr int kBottomColumnGap = 10;
constexpr std::array<const char*, 27> kUiParameterIds {
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
    ParamIDs::msegLoop[2]
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

void drawGlassPanel (juce::Graphics& g,
                     juce::Rectangle<float> bounds,
                     juce::Colour base,
                     juce::Colour accent,
                     float radius,
                     bool raised)
{
    if (raised)
        juce::DropShadow (juce::Colours::black.withAlpha (0.30f), 9, { 0, 4 }).drawForRectangle (g, bounds.toNearestInt());

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
    juce::DropShadow (juce::Colours::black.withAlpha (0.34f), 8, { 0, 3 }).drawForRectangle (g, knob.toNearestInt());

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

void drawMiniCurve (juce::Graphics& g, const std::vector<MsegPoint>& points, juce::Rectangle<float> bounds, juce::Colour colour)
{
    const auto graph = bounds.reduced (8.0f, 7.0f);
    auto safePoints = MsegShape::sanitize (points);
    juce::Path path;
    if (! safePoints.empty())
    {
        path.startNewSubPath (graph.getX() + safePoints.front().x * graph.getWidth(),
                              graph.getBottom() - safePoints.front().y * graph.getHeight());
        for (size_t i = 1; i < safePoints.size(); ++i)
        {
            path.lineTo (graph.getX() + safePoints[i].x * graph.getWidth(),
                         graph.getBottom() - safePoints[i].y * graph.getHeight());
        }
    }

    g.setColour (colour.withAlpha (0.18f));
    auto fill = path;
    fill.lineTo (graph.getRight(), graph.getBottom());
    fill.lineTo (graph.getX(), graph.getBottom());
    fill.closeSubPath();
    g.fillPath (fill);

    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (2.0f));
}
} // namespace

DevicePanel::DevicePanel (MacroOscAudioProcessor& processor, juce::Image backing, EditorFonts editorFonts)
    : audioProcessor (processor),
      backgroundImage (std::move (backing)),
      fonts (std::move (editorFonts))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
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

    const auto badge = designRect (91.0f, 110.0f, 135.0f, 78.0f);
    g.setColour (yellowAccent);
    g.setFont (withHeight (fonts.digital, badge.getHeight() * 0.86f));
    g.drawFittedText (juce::String (modelIndex).paddedLeft ('0', 2), badge.toNearestInt().reduced (2), juce::Justification::centred, 1);

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

    const auto dashArea = designRect (396.0f, 236.0f, 1018.0f, 20.0f);
    const int dashCount = modelNames.size();
    const int activeDash = dashCount > 1 ? juce::roundToInt ((static_cast<float> (modelIndex) / static_cast<float> (dashCount - 1)) * static_cast<float> (dashCount - 1)) : 0;
    const float gap = juce::jmax (1.0f, dashArea.getWidth() / 180.0f);
    const float dashWidth = (dashArea.getWidth() - (static_cast<float> (dashCount - 1) * gap)) / static_cast<float> (dashCount);
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
    const auto valueArea = designRect (x + 34.0f, 374.0f, 142.0f, 65.0f);

    g.setColour (textColour);
    g.setFont (withHeight (fonts.title, labelArea.getHeight() * 0.76f));
    g.drawFittedText (label, labelArea.toNearestInt(), juce::Justification::centred, 1);

    g.setColour (active ? yellowAccent.brighter (0.35f) : yellowAccent);
    g.setFont (withHeight (fonts.digital, valueArea.getHeight() * 0.92f));
    const int display = juce::roundToInt (juce::jlimit (0.0f, 1.0f, parameterValue (parameterId)) * 127.0f);
    g.drawFittedText (juce::String (display), valueArea.toNearestInt().reduced (2), juce::Justification::centred, 1);
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
        drawGlassPanel (g, bounds, boxColour, accentColour, 8.0f, true);

        auto labelArea = getLocalBounds().reduced (5, 3).removeFromTop (16);
        g.setFont (withHeight (fonts.label, 12.0f));
        g.setColour (accentColour);
        g.drawFittedText (label, labelArea, juce::Justification::centred, 1);

        const float knobSize = juce::jmin (bounds.getWidth() * 0.68f, bounds.getHeight() - 42.0f);
        const auto knobArea = juce::Rectangle<float> (knobSize, knobSize)
            .withCentre ({ bounds.getCentreX(), bounds.getY() + 20.0f + knobSize * 0.5f });
        drawGlassKnob (g, knobArea, accentColour, normalised, dragging);

        auto valueArea = bounds.withTop (knobArea.getBottom() + 1.0f).reduced (4.0f, 0.0f).toNearestInt();
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
}

void MsegSlotStrip::setActiveSlot (int slot)
{
    activeSlot = juce::jlimit (0, kMsegSlotCount - 1, slot);
    repaint();
}

void MsegSlotStrip::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (0.0f, 2.0f);
    const float gap = 10.0f;
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
    auto area = getLocalBounds().toFloat().reduced (0.0f, 2.0f);
    const float gap = 10.0f;
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
    auto tab = bounds.reduced (1.0f, active ? 0.0f : 3.0f);
    drawGlassPanel (g,
                    tab,
                    active ? juce::Colour (0xff117c7a) : boxColour,
                    active ? cyanAccent : juce::Colour (0xff2db6b2),
                    5.0f,
                    active);

    g.setFont (withHeight (fonts.label, 12.0f));
    g.setColour (textColour);
    auto header = tab.removeFromTop (17.0f);
    g.drawFittedText ("MSEG " + juce::String (slot + 1), header.toNearestInt().reduced (8, 0),
                      juce::Justification::centredLeft, 1);
    const int destinationIndex = juce::jlimit (0,
                                               msegDestinationNames().size() - 1,
                                               juce::roundToInt (rawParameterValue (audioProcessor,
                                                                                    ParamIDs::msegDestination[static_cast<size_t> (slot)])));
    g.setColour (active ? yellowAccent : mutedTextColour);
    g.drawFittedText (msegDestinationNames()[destinationIndex], header.toNearestInt().reduced (8, 0),
                      juce::Justification::centredRight, 1);

    drawMiniCurve (g, audioProcessor.getMsegPoints (slot), tab.reduced (6.0f, 1.0f), active ? yellowAccent : cyanAccent);

    if (active)
    {
        g.setColour (yellowAccent);
        g.fillRect (bounds.withY (bounds.getBottom() - 3.0f).withHeight (3.0f).reduced (8.0f, 0.0f));
    }
}

MsegCurveEditor::MsegCurveEditor (MacroOscAudioProcessor& processor, EditorFonts editorFonts)
    : audioProcessor (processor),
      fonts (std::move (editorFonts)),
      points (processor.getMsegPoints (0))
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
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
    auto bounds = getLocalBounds().toFloat();
    drawGlassPanel (g, bounds, juce::Colour (0xff061f21), cyanAccent, 5.0f, false);

    const auto graph = graphBounds();
    g.setColour (juce::Colour (0xff123235));
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
    drawMiniCurve (g, points, getLocalBounds().toFloat().reduced (6.0f), yellowAccent);

    auto safePoints = MsegShape::sanitize (points);
    for (size_t i = 0; i < safePoints.size(); ++i)
    {
        const auto p = pointToScreen (safePoints[i]);
        const bool selected = static_cast<int> (i) == selectedIndex;
        g.setColour (selected ? textColour : yellowAccent);
        const float size = selected ? 15.0f : 12.0f;
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (p));
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.drawEllipse (juce::Rectangle<float> (size, size).withCentre (p), 1.0f);
    }

    g.setFont (withHeight (fonts.label, 11.0f));
    g.setColour (mutedTextColour.withAlpha (0.72f));
    g.drawFittedText ("click to add, drag to move, right-drag to delete", getLocalBounds().reduced (10, 6),
                      juce::Justification::topRight, 1);
}

void MsegCurveEditor::mouseDown (const juce::MouseEvent& event)
{
    editing = true;
    const auto position = event.position;
    if (event.mods.isRightButtonDown())
    {
        deletePointNear (position);
        repaint();
        return;
    }

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
    if (event.mods.isRightButtonDown())
    {
        deletePointNear (event.position);
        repaint();
        return;
    }

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
        const float leftLimit = points[static_cast<size_t> (selectedIndex - 1)].x + 0.0025f;
        const float rightLimit = points[static_cast<size_t> (selectedIndex + 1)].x - 0.0025f;
        next.x = juce::jlimit (leftLimit, rightLimit, next.x);
    }

    points[static_cast<size_t> (selectedIndex)] = next;
    audioProcessor.setMsegPoints (activeSlot, points, false);
    repaint();
}

void MsegCurveEditor::mouseUp (const juce::MouseEvent&)
{
    editing = false;
    commit (true);
    repaint();
}

void MsegCurveEditor::mouseDoubleClick (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
}

juce::Rectangle<float> MsegCurveEditor::graphBounds() const
{
    return getLocalBounds().toFloat().reduced (12.0f, 12.0f);
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
      msegEditor (processorRef, fonts),
      msegAmountBox (processorRef, fonts),
      msegOffsetBox (processorRef, fonts),
      msegRateBox (processorRef, fonts)
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
    addAndMakeVisible (msegAmountBox);
    addAndMakeVisible (msegOffsetBox);
    addAndMakeVisible (msegRateBox);
    addAndMakeVisible (destinationBox);
    addAndMakeVisible (loopButton);

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
    msegSlots.onSlotSelected = [this] (int slot) { setActiveMsegSlot (slot); };
    setActiveMsegSlot (0);

    audioProcessor.addChangeListener (this);
    for (auto* parameterId : kUiParameterIds)
        audioProcessor.getState().addParameterListener (parameterId, this);

    setResizable (false, false);
    setSize (kEditorWidth, kEditorHeight);
}

MacroOscAudioProcessorEditor::~MacroOscAudioProcessorEditor()
{
    cancelPendingUpdate();
    for (auto* parameterId : kUiParameterIds)
        audioProcessor.getState().removeParameterListener (parameterId, this);

    destinationBox.removeMouseListener (this);
    loopButton.removeMouseListener (this);
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

    drawGlassPanel (g, controlsArea.toFloat(), rowColour.darker (0.12f), cyanAccent, 8.0f, true);
    drawGlassPanel (g, msegArea.toFloat(), juce::Colour (0xff042829), cyanAccent, 7.0f, true);

    auto controlsInner = controlsArea.reduced (10, 8);
    const auto paintGroupTitle = [&] (juce::Rectangle<int> titleArea, const juce::String& title, juce::Colour accent)
    {
        g.setColour (accent.withAlpha (0.18f));
        g.fillRoundedRectangle (titleArea.toFloat().reduced (1.0f), 4.0f);
        g.setFont (withHeight (fonts.title, 14.0f));
        g.setColour (textColour);
        g.drawFittedText (title, titleArea.reduced (6, 0), juce::Justification::centredLeft, 1);
    };

    paintGroupTitle (controlsInner.removeFromTop (22), "PITCH", yellowAccent);
    controlsInner.removeFromTop (92);
    controlsInner.removeFromTop (8);
    paintGroupTitle (controlsInner.removeFromTop (22), "ENV.", cyanAccent);
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

    auto controlsInner = controlsArea.reduced (10, 8);
    controlsInner.removeFromTop (22);
    auto pitchRow = controlsInner.removeFromTop (92);
    const auto layoutKnobRow = [] (juce::Rectangle<int> row, const auto& boxes)
    {
        constexpr int gap = 6;
        const int count = static_cast<int> (boxes.size());
        const int cellWidth = (row.getWidth() - gap * (count - 1)) / count;
        for (auto* box : boxes)
        {
            box->setBounds (row.removeFromLeft (cellWidth));
            row.removeFromLeft (gap);
        }
    };

    layoutKnobRow (pitchRow, std::array<RackValueBox*, 3> { &pitchBox, &detuneBox, &portaBox });

    controlsInner.removeFromTop (8);
    controlsInner.removeFromTop (22);
    auto envRow = controlsInner.removeFromTop (92);
    layoutKnobRow (envRow, std::array<RackValueBox*, 4> { &attackBox, &decayBox, &sustainBox, &releaseBox });

    msegArea.reduce (8, 8);
    auto tabRow = msegArea.removeFromTop (44);
    msegSlots.setBounds (tabRow);

    msegArea.removeFromTop (6);
    auto controls = msegArea.removeFromTop (38);
    destinationBox.setBounds (controls.removeFromLeft (132).reduced (4, 3));
    loopButton.setBounds (controls.removeFromLeft (68).reduced (4, 4));
    const int smallWidth = controls.getWidth() / 3;
    msegAmountBox.setBounds (controls.removeFromLeft (smallWidth).reduced (4, 1));
    msegOffsetBox.setBounds (controls.removeFromLeft (smallWidth).reduced (4, 1));
    msegRateBox.setBounds (controls.reduced (4, 1));

    msegArea.removeFromTop (8);
    msegEditor.setBounds (msegArea);
}

void MacroOscAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! msegEditor.isEditing())
        msegEditor.setPoints (audioProcessor.getMsegPoints (activeMsegSlot));

    msegSlots.repaint();
}

void MacroOscAudioProcessorEditor::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate();
}

void MacroOscAudioProcessorEditor::handleAsyncUpdate()
{
    devicePanel.repaint();
    pitchBox.repaint();
    detuneBox.repaint();
    portaBox.repaint();
    attackBox.repaint();
    decayBox.repaint();
    sustainBox.repaint();
    releaseBox.repaint();
    msegAmountBox.repaint();
    msegOffsetBox.repaint();
    msegRateBox.repaint();
    msegSlots.repaint();
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
    }
}

void MacroOscAudioProcessorEditor::setActiveMsegSlot (int slot)
{
    activeMsegSlot = juce::jlimit (0, kMsegSlotCount - 1, slot);
    msegSlots.setActiveSlot (activeMsegSlot);
    msegEditor.setActiveSlot (activeMsegSlot);
    msegAmountBox.configure ("Amt", ParamIDs::msegAmount[static_cast<size_t> (activeMsegSlot)], yellowAccent, "%", 1);
    msegOffsetBox.configure ("Off", ParamIDs::msegOffset[static_cast<size_t> (activeMsegSlot)], yellowAccent, "%", 1);
    msegRateBox.configure ("Rate", ParamIDs::msegRate[static_cast<size_t> (activeMsegSlot)], yellowAccent, "s", 3);
    configureMsegAttachments();
}

void MacroOscAudioProcessorEditor::configureMsegAttachments()
{
    destinationAttachment.reset();
    loopAttachment.reset();
    destinationAttachment = std::make_unique<ComboBoxAttachment> (
        audioProcessor.getState(),
        ParamIDs::msegDestination[static_cast<size_t> (activeMsegSlot)],
        destinationBox);
    loopAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.getState(),
        ParamIDs::msegLoop[static_cast<size_t> (activeMsegSlot)],
        loopButton);
}

void MacroOscAudioProcessorEditor::styleMsegControls()
{
    destinationBox.addItemList (msegDestinationNames(), 1);
    destinationBox.setColour (juce::ComboBox::backgroundColourId, boxColour);
    destinationBox.setColour (juce::ComboBox::outlineColourId, cyanAccent.withAlpha (0.55f));
    destinationBox.setColour (juce::ComboBox::textColourId, textColour);
    destinationBox.setColour (juce::PopupMenu::backgroundColourId, boxColour);
    destinationBox.setColour (juce::PopupMenu::textColourId, textColour);

    loopButton.setButtonText ("Loop");
    loopButton.setColour (juce::ToggleButton::textColourId, textColour);
    loopButton.setColour (juce::ToggleButton::tickColourId, yellowAccent);
    loopButton.setColour (juce::ToggleButton::tickDisabledColourId, mutedTextColour);
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
