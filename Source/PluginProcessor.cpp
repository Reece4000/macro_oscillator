#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace macro_osc
{
namespace
{
constexpr double kDcBlockerCutoffHz = 20.0;

juce::NormalisableRange<float> skewedRange (float start, float end, float interval, float skew)
{
    return { start, end, interval, skew };
}

[[nodiscard]] juce::String msegPointsPropertyName (int slotIndex)
{
    return "msegPoints" + juce::String (juce::jlimit (0, kMsegSlotCount - 1, slotIndex) + 1);
}
} // namespace

juce::StringArray braidsModelNames()
{
    return {
        "CSAW", "MORPH", "SAW/SQR", "SINE/TRI", "BUZZ",
        "SQR SUB", "SAW SUB", "SQR SYNC", "SAW SYNC",
        "TRI SAW", "TRI SQR", "TRIANGLE", "TRI SINE", "RING MOD",
        "SAW SWARM", "SAW COMB", "TOY",
        "LP FILTER", "PK FILTER", "BP FILTER", "HP FILTER",
        "VOSIM", "VOWEL", "VOWEL FOF", "HARMONICS",
        "FM", "FB FM", "CHAOS FM",
        "PLUCKED", "BOWED", "BLOWN", "FLUTED",
        "BELL", "DRUM", "KICK", "CYMBAL", "SNARE",
        "WAVETABLE", "WAVE MAP", "WAVE LINE", "WAVE PARA",
        "FIL. NOISE", "TWIN PEAKS", "CLK NOISE", "CLOUD", "PARTICLE", "DIGI MOD"
    };
}

MacroOscAudioProcessor::MacroOscAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MacroOscState", createParameterLayout())
{
    retainedMsegShapes.reserve (128);
    cacheRawParameterPointers();

    synth.addSound (new BraidsSound());
    for (auto& voice : braidsVoices)
    {
        voice = new BraidsVoice();
        synth.addVoice (voice);
    }
    synth.setNoteStealingEnabled (true);

    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        setMsegPoints (slot, MsegShape::defaultPoints(), false);
}

void MacroOscAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (auto* voice : braidsVoices)
        voice->prepare (sampleRate);

    if (sampleRate > 0.0)
        dcBlockerCoefficient = static_cast<float> (std::exp (-2.0 * juce::MathConstants<double>::pi * kDcBlockerCutoffHz / sampleRate));
    else
        dcBlockerCoefficient = 0.995f;

    resetOutputDcBlocker();
}

void MacroOscAudioProcessor::releaseResources()
{
}

bool MacroOscAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

void MacroOscAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    updateHostTempo();
    const auto voiceParameters = buildVoiceParameters();
    for (auto* voice : braidsVoices)
        voice->setParameters (voiceParameters);

    synth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
    applyOutputDcBlocker (buffer);
}

juce::AudioProcessorEditor* MacroOscAudioProcessor::createEditor()
{
    return new MacroOscAudioProcessorEditor (*this);
}

void MacroOscAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        state.setProperty (msegPointsPropertyName (slot), MsegShape::serialise (getMsegPoints (slot)), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MacroOscAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != parameters.state.getType())
        return;

    std::array<juce::String, kMsegSlotCount> encodedPoints;
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
    {
        encodedPoints[static_cast<size_t> (slot)] = state.getProperty (msegPointsPropertyName (slot)).toString();
        if (encodedPoints[static_cast<size_t> (slot)].isEmpty() && slot == 0)
            encodedPoints[static_cast<size_t> (slot)] = state.getProperty ("msegPoints").toString();
    }

    parameters.replaceState (state);
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        setMsegPoints (slot, MsegShape::parse (encodedPoints[static_cast<size_t> (slot)]), false);

    sendChangeMessage();
}

std::vector<MsegPoint> MacroOscAudioProcessor::getMsegPoints (int slotIndex) const
{
    if (auto shape = getMsegShape (slotIndex))
        return shape->points;

    return MsegShape::defaultPoints();
}

std::shared_ptr<const MsegShape> MacroOscAudioProcessor::getMsegShape (int slotIndex) const
{
    const int clampedSlot = juce::jlimit (0, kMsegSlotCount - 1, slotIndex);
    return std::atomic_load_explicit (&currentMsegShapes[static_cast<size_t> (clampedSlot)], std::memory_order_acquire);
}

const MsegShape* MacroOscAudioProcessor::getAudioMsegShape (int slotIndex) const noexcept
{
    const int clampedSlot = juce::jlimit (0, kMsegSlotCount - 1, slotIndex);
    return currentAudioMsegShapes[static_cast<size_t> (clampedSlot)].load (std::memory_order_acquire);
}

void MacroOscAudioProcessor::cacheRawParameterPointers()
{
    rawParameters.model = parameters.getRawParameterValue (ParamIDs::model);
    rawParameters.timbre = parameters.getRawParameterValue (ParamIDs::timbre);
    rawParameters.color = parameters.getRawParameterValue (ParamIDs::color);
    rawParameters.modulation = parameters.getRawParameterValue (ParamIDs::modulation);
    rawParameters.fmAmount = parameters.getRawParameterValue (ParamIDs::fmAmount);
    rawParameters.fmRatio = parameters.getRawParameterValue (ParamIDs::fmRatio);
    rawParameters.level = parameters.getRawParameterValue (ParamIDs::level);
    rawParameters.coarse = parameters.getRawParameterValue (ParamIDs::coarse);
    rawParameters.fine = parameters.getRawParameterValue (ParamIDs::fine);
    rawParameters.portamento = parameters.getRawParameterValue (ParamIDs::portamento);
    rawParameters.attack = parameters.getRawParameterValue (ParamIDs::attack);
    rawParameters.decay = parameters.getRawParameterValue (ParamIDs::decay);
    rawParameters.sustain = parameters.getRawParameterValue (ParamIDs::sustain);
    rawParameters.release = parameters.getRawParameterValue (ParamIDs::release);

    for (int slot = 0; slot < kMsegSlotCount; ++slot)
    {
        const auto index = static_cast<size_t> (slot);
        rawParameters.msegDestination[index] = parameters.getRawParameterValue (ParamIDs::msegDestination[index]);
        rawParameters.msegAmount[index] = parameters.getRawParameterValue (ParamIDs::msegAmount[index]);
        rawParameters.msegOffset[index] = parameters.getRawParameterValue (ParamIDs::msegOffset[index]);
        rawParameters.msegRate[index] = parameters.getRawParameterValue (ParamIDs::msegRate[index]);
        rawParameters.msegLoop[index] = parameters.getRawParameterValue (ParamIDs::msegLoop[index]);
        rawParameters.msegSync[index] = parameters.getRawParameterValue (ParamIDs::msegSync[index]);
    }
}

void MacroOscAudioProcessor::resetOutputDcBlocker() noexcept
{
    dcBlockerPreviousInput.fill (0.0f);
    dcBlockerPreviousOutput.fill (0.0f);
}

void MacroOscAudioProcessor::applyOutputDcBlocker (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto channels = juce::jmin (buffer.getNumChannels(), static_cast<int> (dcBlockerPreviousInput.size()));
    const auto samples = buffer.getNumSamples();

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer (channel);
        auto previousInput = dcBlockerPreviousInput[static_cast<size_t> (channel)];
        auto previousOutput = dcBlockerPreviousOutput[static_cast<size_t> (channel)];

        for (int sample = 0; sample < samples; ++sample)
        {
            const auto input = data[sample];
            const auto output = input - previousInput + dcBlockerCoefficient * previousOutput;
            data[sample] = output;
            previousInput = input;
            previousOutput = output;
        }

        dcBlockerPreviousInput[static_cast<size_t> (channel)] = previousInput;
        dcBlockerPreviousOutput[static_cast<size_t> (channel)] = previousOutput;
    }
}

void MacroOscAudioProcessor::updateHostTempo() noexcept
{
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
            {
                const float tempo = static_cast<float> (*bpm);
                if (std::isfinite (tempo) && tempo > 0.0f)
                    currentTempoBpm.store (juce::jlimit (20.0f, 300.0f, tempo), std::memory_order_relaxed);
            }
        }
    }
}

void MacroOscAudioProcessor::setMsegPoints (int slotIndex, std::vector<MsegPoint> points, bool notifyEditor)
{
    const int clampedSlot = juce::jlimit (0, kMsegSlotCount - 1, slotIndex);
    auto shape = std::make_shared<MsegShape>();
    shape->points = MsegShape::sanitize (std::move (points));
    if (notifyEditor)
        parameters.state.setProperty (msegPointsPropertyName (clampedSlot), MsegShape::serialise (shape->points), nullptr);

    auto publishedShape = std::static_pointer_cast<const MsegShape> (shape);
    retainedMsegShapes.push_back (publishedShape);
    std::atomic_store_explicit (&currentMsegShapes[static_cast<size_t> (clampedSlot)],
                                publishedShape,
                                std::memory_order_release);
    currentAudioMsegShapes[static_cast<size_t> (clampedSlot)].store (publishedShape.get(), std::memory_order_release);
    if (notifyEditor)
        sendChangeMessage();
}

MacroOscAudioProcessor::APVTS::ParameterLayout MacroOscAudioProcessor::createParameterLayout()
{
    APVTS::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::model, 1 }, "Model", braidsModelNames(), 34));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::timbre, 1 }, "Timbre", juce::NormalisableRange<float> { 0.0f, 1.0f }, 64.0f / 127.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::color, 1 }, "Color", juce::NormalisableRange<float> { 0.0f, 1.0f }, 64.0f / 127.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::modulation, 1 }, "Modulation", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::fmAmount, 1 }, "FM", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::fmRatio, 1 }, "FM Ratio", juce::NormalisableRange<float> { 0.25f, 16.0f, 0.001f, 0.35f }, 1.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::level, 1 }, "Level", juce::NormalisableRange<float> { 0.0f, 1.5f, 0.001f }, 0.8f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::coarse, 1 }, "Pitch", juce::NormalisableRange<float> { -36.0f, 36.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::fine, 1 }, "Detune", juce::NormalisableRange<float> { -1.0f, 1.0f, 0.001f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::portamento, 1 }, "Portamento", skewedRange (0.0f, 2.0f, 0.001f, 0.35f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::scale, 1 }, "Scale", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::attack, 1 }, "Attack", skewedRange (0.0f, 10.0f, 0.001f, 0.35f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::decay, 1 }, "Decay", skewedRange (0.0f, 10.0f, 0.001f, 0.35f), 1.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::sustain, 1 }, "Sustain", juce::NormalisableRange<float> { 0.0f, 1.0f }, 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::release, 1 }, "Release", skewedRange (0.0f, 10.0f, 0.001f, 0.35f), 0.35f));

    for (int slot = 0; slot < kMsegSlotCount; ++slot)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::msegDestination[static_cast<size_t> (slot)], 1 },
            "MSEG " + juce::String (slot + 1) + " Destination",
            msegDestinationNames(),
            slot == 0 ? static_cast<int> (MsegDestination::timbre) : static_cast<int> (MsegDestination::off)));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::msegAmount[static_cast<size_t> (slot)], 1 },
            "MSEG " + juce::String (slot + 1) + " Amount",
            juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f },
            50.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::msegOffset[static_cast<size_t> (slot)], 1 },
            "MSEG " + juce::String (slot + 1) + " Offset",
            juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f },
            0.0f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::msegRate[static_cast<size_t> (slot)], 1 },
            "MSEG " + juce::String (slot + 1) + " Rate",
            skewedRange (0.05f, 16.0f, 0.001f, 0.35f),
            1.0f));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::msegLoop[static_cast<size_t> (slot)], 1 },
            "MSEG " + juce::String (slot + 1) + " Loop",
            true));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::msegSync[static_cast<size_t> (slot)], 1 },
            "MSEG " + juce::String (slot + 1) + " Sync",
            false));
    }

    return layout;
}

BraidsVoiceParameters MacroOscAudioProcessor::buildVoiceParameters() const
{
    BraidsVoiceParameters result;
    result.model = juce::roundToInt (getFloatParameter (rawParameters.model));
    result.timbre = getFloatParameter (rawParameters.timbre);
    result.color = getFloatParameter (rawParameters.color);
    result.modulation = getFloatParameter (rawParameters.modulation);
    result.fmAmount = getFloatParameter (rawParameters.fmAmount);
    result.fmRatio = getFloatParameter (rawParameters.fmRatio);
    result.level = getFloatParameter (rawParameters.level);
    result.coarseSemitones = getFloatParameter (rawParameters.coarse);
    result.fineSemitones = getFloatParameter (rawParameters.fine);
    result.portamentoSeconds = getFloatParameter (rawParameters.portamento);
    result.attackSeconds = getFloatParameter (rawParameters.attack);
    result.decaySeconds = getFloatParameter (rawParameters.decay);
    result.sustainLevel = getFloatParameter (rawParameters.sustain);
    result.releaseSeconds = getFloatParameter (rawParameters.release);

    for (int slot = 0; slot < kMsegSlotCount; ++slot)
    {
        const auto index = static_cast<size_t> (slot);
        auto& target = result.msegSlots[static_cast<size_t> (slot)];
        target.destination = juce::roundToInt (getFloatParameter (rawParameters.msegDestination[index]));
        target.amountPercent = getFloatParameter (rawParameters.msegAmount[index]);
        target.offsetPercent = getFloatParameter (rawParameters.msegOffset[index]);
        const float rateValue = getFloatParameter (rawParameters.msegRate[index]);
        const bool syncDuration = getFloatParameter (rawParameters.msegSync[index]) >= 0.5f;
        target.rateSeconds = syncDuration
            ? msegSyncedBeatsToSeconds (rateValue, currentTempoBpm.load (std::memory_order_relaxed))
            : rateValue;
        target.loop = getFloatParameter (rawParameters.msegLoop[index]) >= 0.5f;
        target.shape = getAudioMsegShape (slot);
    }

    return result;
}

float MacroOscAudioProcessor::getFloatParameter (const std::atomic<float>* value) noexcept
{
    if (value != nullptr)
        return value->load (std::memory_order_relaxed);

    return 0.0f;
}
} // namespace macro_osc

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new macro_osc::MacroOscAudioProcessor();
}
