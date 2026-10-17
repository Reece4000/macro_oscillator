#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace macro_osc
{
class DistortionDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    DistortionDisplay (MacroOscAudioProcessor&, juce::Font);
    void refresh();
    void paint (juce::Graphics&) override;

private:
    MacroOscAudioProcessor& processor;
    juce::Font font;
    NativeDistortionProcessor preview;
    std::array<float, 4> lastParameters { -1.0f, -1.0f, -1.0f, -1.0f };
    std::array<float, 960> waveform {};
    float peak {};
};

class FilterDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    FilterDisplay (MacroOscAudioProcessor&, juce::Font);
    ~FilterDisplay() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    MacroOscAudioProcessor& processor;
    juce::Font font;
    bool dragging {};
    [[nodiscard]] juce::Rectangle<float> plotBounds() const;
    void setFromPosition (juce::Point<float>);
    void endGesture();
};
}
