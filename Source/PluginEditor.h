#pragma once

#include "PluginProcessor.h"

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_extra/juce_gui_extra.h>

namespace macro_osc
{
struct EditorFonts
{
    juce::Font digital { juce::FontOptions (28.0f) };
    juce::Font label { juce::FontOptions (14.0f) };
    juce::Font title { juce::FontOptions (22.0f) };
};

class DevicePanel final : public juce::Component
{
public:
    DevicePanel (MacroOscAudioProcessor& processor, juce::Image backing, EditorFonts fonts);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    struct DragTarget
    {
        const char* parameterId {};
        float pixelsPerUnit { 120.0f };
    };

    MacroOscAudioProcessor& audioProcessor;
    juce::Image backgroundImage;
    EditorFonts fonts;
    DragTarget activeDrag;
    float dragStartNormalised {};
    juce::Point<float> dragStartPosition;
    bool dragging {};

    [[nodiscard]] juce::Rectangle<float> imageBounds() const;
    [[nodiscard]] juce::Rectangle<float> designRect (float x, float y, float w, float h) const;
    [[nodiscard]] DragTarget targetForPosition (juce::Point<float> position) const;
    [[nodiscard]] float parameterValue (const char* parameterId) const;
    void beginGesture (const char* parameterId);
    void endGesture (const char* parameterId);
    void paintModelDisplay (juce::Graphics& g);
    void paintMacroValue (juce::Graphics& g, const char* label, const char* parameterId, int column, juce::Colour accent);
    [[nodiscard]] juce::String modelDisplayText() const;
};

class RackValueBox final : public juce::Component
{
public:
    RackValueBox (MacroOscAudioProcessor& processor, EditorFonts fonts);

    void configure (const juce::String& labelText,
                    const char* parameter,
                    juce::Colour accent,
                    const juce::String& unitText,
                    int decimals,
                    float displayMultiplier = 1.0f);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    MacroOscAudioProcessor& audioProcessor;
    EditorFonts fonts;
    juce::String label;
    juce::String unit;
    const char* parameterId {};
    juce::Colour accentColour { 0xffffc928 };
    int decimalPlaces {};
    float multiplier { 1.0f };
    float dragStartNormalised {};
    juce::Point<float> dragStartPosition;
    bool dragging {};

    [[nodiscard]] float parameterValue() const;
    void setParameterValue (float value);
    void beginGesture();
    void endGesture();
    [[nodiscard]] juce::String displayValue() const;
};

class MsegSlotStrip final : public juce::Component
{
public:
    MsegSlotStrip (MacroOscAudioProcessor& processor, EditorFonts fonts);

    void setActiveSlot (int slot);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;

    std::function<void(int)> onSlotSelected;

private:
    MacroOscAudioProcessor& audioProcessor;
    EditorFonts fonts;
    int activeSlot {};

    void paintSlot (juce::Graphics& g, juce::Rectangle<float> bounds, int slot);
};

class MsegCurveEditor final : public juce::Component
{
public:
    MsegCurveEditor (MacroOscAudioProcessor& processor, EditorFonts fonts);

    void setActiveSlot (int slot);
    void setPoints (std::vector<MsegPoint> newPoints);
    [[nodiscard]] bool isEditing() const noexcept { return editing; }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

private:
    enum class DragMode
    {
        none,
        curvePoint,
        offset,
        scale,
        duration
    };

    MacroOscAudioProcessor& audioProcessor;
    EditorFonts fonts;
    std::vector<MsegPoint> points;
    int activeSlot {};
    int selectedIndex { -1 };
    bool editing {};
    DragMode dragMode { DragMode::none };

    [[nodiscard]] juce::Rectangle<float> graphBounds() const;
    [[nodiscard]] juce::Rectangle<float> offsetSliderBounds() const;
    [[nodiscard]] juce::Rectangle<float> scaleSliderBounds() const;
    [[nodiscard]] juce::Rectangle<float> durationSliderBounds() const;
    [[nodiscard]] juce::Rectangle<float> offsetLabelBounds() const;
    [[nodiscard]] juce::Rectangle<float> scaleLabelBounds() const;
    [[nodiscard]] juce::Point<float> pointToScreen (const MsegPoint& point) const;
    [[nodiscard]] MsegPoint screenToPoint (juce::Point<float> position) const;
    [[nodiscard]] int findNearestPoint (juce::Point<float> position) const;
    [[nodiscard]] DragMode sliderAtPosition (juce::Point<float> position) const;
    [[nodiscard]] const char* parameterForDragMode (DragMode mode) const;
    [[nodiscard]] float normalisedParameterValue (const char* parameterId) const;
    void setSliderFromPosition (DragMode mode, juce::Point<float> position);
    void beginParameterGesture (DragMode mode);
    void endParameterGesture (DragMode mode);
    bool deletePointNear (juce::Point<float> position);
    void commit (bool notifyEditor);
};

class MacroOscAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::ChangeListener,
                                           private juce::AudioProcessorValueTreeState::Listener,
                                           private juce::AsyncUpdater
{
public:
    explicit MacroOscAudioProcessorEditor (MacroOscAudioProcessor&);
    ~MacroOscAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboBoxAttachment = MacroOscAudioProcessor::APVTS::ComboBoxAttachment;
    using ButtonAttachment = MacroOscAudioProcessor::APVTS::ButtonAttachment;

    MacroOscAudioProcessor& audioProcessor;
    EditorFonts fonts;
    DevicePanel devicePanel;
    RackValueBox pitchBox;
    RackValueBox detuneBox;
    RackValueBox portaBox;
    RackValueBox attackBox;
    RackValueBox decayBox;
    RackValueBox sustainBox;
    RackValueBox releaseBox;
    MsegSlotStrip msegSlots;
    MsegCurveEditor msegEditor;
    juce::ComboBox destinationBox;
    juce::ToggleButton loopButton;
    std::unique_ptr<ComboBoxAttachment> destinationAttachment;
    std::unique_ptr<ButtonAttachment> loopAttachment;
    int activeMsegSlot {};

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;
    void setActiveMsegSlot (int slot);
    void configureMsegAttachments();
    void styleMsegControls();
    [[nodiscard]] static EditorFonts loadFonts();
};
} // namespace macro_osc
