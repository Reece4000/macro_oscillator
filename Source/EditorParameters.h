#pragma once

#include "PluginProcessor.h"

// Small parameter helpers shared by the editor's controls.
namespace macro_osc::editor
{
[[nodiscard]] inline juce::RangedAudioParameter* parameterFor (MacroOscAudioProcessor& processor, const char* parameterId)
{
    return parameterId != nullptr ? processor.getState().getParameter (parameterId) : nullptr;
}

[[nodiscard]] inline float rawValue (MacroOscAudioProcessor& processor, const char* parameterId)
{
    if (const auto* value = processor.getState().getRawParameterValue (parameterId))
        return value->load (std::memory_order_relaxed);
    return 0.0f;
}

inline void setNormalised (MacroOscAudioProcessor& processor, const char* parameterId, float normalised)
{
    if (auto* parameter = parameterFor (processor, parameterId))
        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
}

inline void setAsGesture (MacroOscAudioProcessor& processor, const char* parameterId, float plainValue)
{
    if (auto* parameter = parameterFor (processor, parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
        parameter->endChangeGesture();
    }
}

inline void resetToDefault (MacroOscAudioProcessor& processor, const char* parameterId)
{
    if (auto* parameter = parameterFor (processor, parameterId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getDefaultValue());
        parameter->endChangeGesture();
    }
}

// Model families for the model strip, in model order. The grouping follows the
// blank lines in Braids' MacroOscillatorShape enum (braids/settings.h); the
// names are ours.
struct ModelFamily
{
    const char* name;
    int count;
};

inline constexpr std::array<ModelFamily, 9> modelFamilies { {
    { "classic", 5 }, { "multi", 12 }, { "formant", 7 }, { "additive", 1 }, { "fm", 3 },
    { "physical", 9 }, { "wavetable", 4 }, { "noise", 5 }, { "digital", 1 }
} };
} // namespace macro_osc::editor
