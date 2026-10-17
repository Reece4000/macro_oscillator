#include "PluginEditor.h"
#include "EditorParameters.h"

#include "braids/macro_oscillator.h"

namespace macro_osc
{
namespace
{
// Braids renders at 96 kHz in 24-sample blocks; C3 gives a 734-sample period.
constexpr int renderBlock = 24;
constexpr int previewNote = 48;
constexpr int periodSamples = 734;
constexpr int warmUpSamples = 4800;
constexpr int strikeSamples = 12000;

// Models that sound from a strike and decay; these show their whole envelope.
[[nodiscard]] bool isStruck (int model)
{
    return model == 28 || model == 32 || model == 33 || model == 34 || model == 36;
}
}

WaveformDisplay::WaveformDisplay (MacroOscAudioProcessor& processor)
    : audioProcessor (processor)
{
    setInterceptsMouseClicks (true, false);
    setTitle ("Oscillator waveform");
    setTooltip ("The selected model at its base Timbre and Color. Modulation, FM and effects are not shown.");
    refresh();
}

void WaveformDisplay::refresh()
{
    const std::array<float, 3> parameters {
        editor::rawValue (audioProcessor, ParamIDs::model),
        editor::rawValue (audioProcessor, ParamIDs::timbre),
        editor::rawValue (audioProcessor, ParamIDs::color)
    };
    if (parameters == lastParameters)
        return;
    lastParameters = parameters;

    const int model = juce::roundToInt (parameters[0]);
    envelopeView = isStruck (model);
    braids::MacroOscillator oscillator;
    oscillator.Init();
    oscillator.set_shape (static_cast<braids::MacroOscillatorShape> (model));
    oscillator.set_pitch (static_cast<int16_t> (previewNote << 7));
    oscillator.set_parameters (static_cast<int16_t> (juce::jlimit (0.0f, 1.0f, parameters[1]) * 32767.0f),
                               static_cast<int16_t> (juce::jlimit (0.0f, 1.0f, parameters[2]) * 32767.0f));
    oscillator.Strike();

    const int total = envelopeView ? strikeSamples : warmUpSamples + periodSamples * 4;
    std::vector<int16_t> samples (static_cast<size_t> (total));
    std::array<uint8_t, renderBlock> sync {};
    for (int i = 0; i + renderBlock <= total; i += renderBlock)
        oscillator.Render (sync.data(), samples.data() + i, renderBlock);

    // Steady models start on a rising zero crossing and show two and a half cycles.
    int start = 0, length = total;
    if (! envelopeView)
    {
        start = warmUpSamples;
        for (int i = warmUpSamples; i < warmUpSamples + periodSamples; ++i)
            if (samples[static_cast<size_t> (i)] <= 0 && samples[static_cast<size_t> (i + 1)] > 0)
            {
                start = i;
                break;
            }
        length = periodSamples * 5 / 2;
    }

    const int columns = juce::jmax (1, getWidth() > 0 ? getWidth() : 560);
    minimums.assign (static_cast<size_t> (columns), 0.0f);
    maximums.assign (static_cast<size_t> (columns), 0.0f);
    float peak = 1638.0f;
    for (int i = start; i < start + length; ++i)
        peak = juce::jmax (peak, std::abs (static_cast<float> (samples[static_cast<size_t> (i)])));
    for (int column = 0; column < columns; ++column)
    {
        const int from = start + column * length / columns;
        const int to = juce::jmax (from + 1, start + (column + 1) * length / columns);
        float low = 1.0f, high = -1.0f;
        for (int i = from; i < to; ++i)
        {
            const float value = static_cast<float> (samples[static_cast<size_t> (i)]) / peak;
            low = juce::jmin (low, value);
            high = juce::jmax (high, value);
        }
        // A steady wave reads as one line; a strike reads as its filled envelope.
        if (! envelopeView)
            low = high = static_cast<float> (samples[static_cast<size_t> (from)]) / peak;
        minimums[static_cast<size_t> (column)] = low;
        maximums[static_cast<size_t> (column)] = high;
    }
    repaint();
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    ui::drawDotGrid (g, bounds);
    g.setColour (ui::faint);
    g.fillRect (bounds.withSizeKeepingCentre (bounds.getWidth(), 1.0f));
    if (minimums.empty())
        return;

    const float scale = bounds.getHeight() * 0.44f;
    const float step = bounds.getWidth() / static_cast<float> (minimums.size());
    const auto yFor = [&] (float value) { return bounds.getCentreY() - value * scale; };
    if (envelopeView)
    {
        juce::Path envelope;
        envelope.startNewSubPath (0.0f, yFor (maximums.front()));
        for (size_t i = 1; i < maximums.size(); ++i)
            envelope.lineTo (static_cast<float> (i) * step, yFor (maximums[i]));
        for (size_t i = minimums.size(); i-- > 0;)
            envelope.lineTo (static_cast<float> (i) * step, yFor (minimums[i]));
        envelope.closeSubPath();
        g.setColour (ui::turquoise.withAlpha (0.35f));
        g.fillPath (envelope);
        g.setColour (ui::turquoise);
        g.strokePath (envelope, juce::PathStrokeType (1.2f));
        return;
    }
    juce::Path wave;
    wave.startNewSubPath (0.0f, yFor (maximums.front()));
    for (size_t i = 1; i < maximums.size(); ++i)
        wave.lineTo (static_cast<float> (i) * step, yFor (maximums[i]));
    g.setColour (ui::turquoise);
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace macro_osc
