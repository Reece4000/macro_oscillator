#include "DSP/BraidsVoice.h"

#include <cmath>

#include <juce_core/juce_core.h>

namespace macro_osc
{
namespace
{
constexpr float kMacroModRateHz = 5.0f;
constexpr float kMacroModDepth = 0.5f;
constexpr float kMaxFmDepthSemitones = 24.0f;
constexpr float kVoiceSilenceThreshold = 1.0e-5f;

[[nodiscard]] double wrapPhase (double phase) noexcept
{
    const double wrapped = std::fmod (phase, juce::MathConstants<double>::twoPi);
    return wrapped < 0.0 ? wrapped + juce::MathConstants<double>::twoPi : wrapped;
}
} // namespace

BraidsVoice::BraidsVoice()
{
    oscillator.Init();
}

void BraidsVoice::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    sampleRatePitchOffsetSemitones = static_cast<float> (12.0 * std::log2 (kBraidsNativeSampleRate / sampleRate));
}

void BraidsVoice::setParameters (const BraidsVoiceParameters& newParameters)
{
    parameters = newParameters;
}

bool BraidsVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<BraidsSound*> (sound) != nullptr;
}

void BraidsVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheelPosition)
{
    currentMidiNote = juce::jlimit (0, 127, midiNoteNumber);
    targetPitchSemitones = static_cast<float> (currentMidiNote);
    currentPitchSemitones = parameters.portamentoSeconds > 1.0e-5f && hasPreviousTargetPitch
        ? previousTargetPitchSemitones
        : targetPitchSemitones;
    previousTargetPitchSemitones = targetPitchSemitones;
    hasPreviousTargetPitch = true;
    currentVelocity = juce::jlimit (0.0f, 1.0f, velocity);
    msegPhases.fill (0.0);
    fmPhase = 0.0;
    modPhase = 0.0;
    carryValid = false;
    pitchWheelMoved (pitchWheelPosition);
    oscillator.Strike();
    envelopeStage = EnvelopeStage::attack;
    envelopeValue = parameters.attackSeconds <= 1.0e-5f ? 1.0f : 0.0f;
    releaseStartValue = 0.0f;
    if (envelopeValue >= 1.0f)
        envelopeStage = EnvelopeStage::decay;
}

void BraidsVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff && parameters.releaseSeconds > 0.001f && envelopeValue > kVoiceSilenceThreshold)
    {
        releaseStartValue = envelopeValue;
        envelopeStage = EnvelopeStage::release;
        return;
    }

    envelopeStage = EnvelopeStage::idle;
    envelopeValue = 0.0f;
    releaseStartValue = 0.0f;
    clearCurrentNote();
}

void BraidsVoice::pitchWheelMoved (int newPitchWheelValue)
{
    const float normalised = (static_cast<float> (juce::jlimit (0, 16383, newPitchWheelValue)) - 8192.0f) / 8192.0f;
    pitchBendSemitones = juce::jlimit (-1.0f, 1.0f, normalised) * kPitchBendRangeSemitones;
}

void BraidsVoice::controllerMoved (int, int)
{
}

void BraidsVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    int rendered = 0;
    while (rendered < numSamples)
    {
        const int chunk = juce::jmin (kRenderBlockSize, numSamples - rendered);
        const auto values = currentModulatedValues();
        const float fmWave = values.fmAmount > 1.0e-5f ? std::sin (static_cast<float> (fmPhase)) : 0.0f;
        const float modWave = values.modulation > 1.0e-5f ? std::sin (static_cast<float> (modPhase)) : 0.0f;
        const float fmPitchOffset = fmWave * values.fmAmount * kMaxFmDepthSemitones;
        const float timbreOffset = modWave * values.modulation * kMacroModDepth;

        oscillator.set_shape (static_cast<braids::MacroOscillatorShape> (values.model));
        renderOscillatorChunk (chunk,
                               values.pitchOffsetSemitones + fmPitchOffset,
                               values.timbre + timbreOffset,
                               values.color - timbreOffset);

        bool audible = false;
        for (int i = 0; i < chunk; ++i)
        {
            const float env = nextEnvelopeSample();
            float sample = static_cast<float> (renderBuffer[static_cast<size_t> (i)]) / 32768.0f;
            sample *= env * currentVelocity * values.level;

            if (! std::isfinite (sample))
                sample = 0.0f;
            sample = std::tanh (juce::jlimit (-4.0f, 4.0f, sample));
            audible = audible || std::abs (sample) > kVoiceSilenceThreshold || env > kVoiceSilenceThreshold;

            const int outputIndex = startSample + rendered + i;
            if (outputBuffer.getNumChannels() == 1)
            {
                outputBuffer.addSample (0, outputIndex, sample);
            }
            else
            {
                outputBuffer.addSample (0, outputIndex, sample);
                outputBuffer.addSample (1, outputIndex, sample);
                for (int channel = 2; channel < outputBuffer.getNumChannels(); ++channel)
                    outputBuffer.addSample (channel, outputIndex, sample);
            }
        }

        advanceModulators (chunk, values.fmAmount, values.modulation);
        rendered += chunk;

        if (! envelopeActive() || ! audible)
        {
            envelopeStage = EnvelopeStage::idle;
            envelopeValue = 0.0f;
            clearCurrentNote();
            break;
        }
    }
}

float BraidsVoice::nextEnvelopeSample() noexcept
{
    const float sustain = juce::jlimit (0.0f, 1.0f, parameters.sustainLevel);

    switch (envelopeStage)
    {
        case EnvelopeStage::idle:
            envelopeValue = 0.0f;
            return 0.0f;

        case EnvelopeStage::attack:
        {
            const float attack = juce::jlimit (0.0f, 10.0f, parameters.attackSeconds);
            if (attack <= 1.0e-5f)
            {
                envelopeValue = 1.0f;
                envelopeStage = EnvelopeStage::decay;
                return envelopeValue;
            }

            envelopeValue += 1.0f / static_cast<float> (attack * sampleRate);
            if (envelopeValue >= 1.0f)
            {
                envelopeValue = 1.0f;
                envelopeStage = EnvelopeStage::decay;
            }
            return envelopeValue;
        }

        case EnvelopeStage::decay:
        {
            const float decay = juce::jlimit (0.0f, 10.0f, parameters.decaySeconds);
            if (decay <= 1.0e-5f)
            {
                envelopeValue = sustain;
                envelopeStage = EnvelopeStage::sustain;
                return envelopeValue;
            }

            envelopeValue -= (1.0f - sustain) / static_cast<float> (decay * sampleRate);
            if (envelopeValue <= sustain)
            {
                envelopeValue = sustain;
                envelopeStage = EnvelopeStage::sustain;
            }
            return envelopeValue;
        }

        case EnvelopeStage::sustain:
            envelopeValue = sustain;
            return envelopeValue;

        case EnvelopeStage::release:
        {
            const float release = juce::jlimit (0.0f, 10.0f, parameters.releaseSeconds);
            if (release <= 1.0e-5f)
            {
                envelopeValue = 0.0f;
                envelopeStage = EnvelopeStage::idle;
                return 0.0f;
            }

            envelopeValue -= releaseStartValue / static_cast<float> (release * sampleRate);
            if (envelopeValue <= kVoiceSilenceThreshold)
            {
                envelopeValue = 0.0f;
                envelopeStage = EnvelopeStage::idle;
            }
            return envelopeValue;
        }
    }

    return 0.0f;
}

bool BraidsVoice::envelopeActive() const noexcept
{
    return envelopeStage != EnvelopeStage::idle;
}

void BraidsVoice::renderOscillatorChunk (int numSamples, float pitchOffsetSemitones, float timbre, float color)
{
    if (numSamples <= 0)
        return;

    int copied = 0;
    if (carryValid)
    {
        renderBuffer[0] = carrySample;
        carrySample = 0;
        carryValid = false;
        copied = 1;
    }

    const int remaining = numSamples - copied;
    if (remaining <= 0)
        return;

    const bool needsPadding = (remaining & 1) != 0;
    const int renderSize = juce::jlimit (2, kRenderBlockSize, remaining + (needsPadding ? 1 : 0));
    std::fill (syncBuffer.begin(), syncBuffer.end(), static_cast<std::uint8_t> (0));

    updateOscillatorParameters (pitchOffsetSemitones, timbre, color);
    oscillator.Render (syncBuffer.data(), oscillatorTempBuffer.data(), static_cast<size_t> (renderSize));
    std::copy_n (oscillatorTempBuffer.begin(), remaining, renderBuffer.begin() + copied);

    if (needsPadding)
    {
        carrySample = oscillatorTempBuffer[static_cast<size_t> (remaining)];
        carryValid = true;
    }
}

void BraidsVoice::updateOscillatorParameters (float pitchOffsetSemitones, float timbre, float color)
{
    const float portamento = juce::jlimit (0.0f, 2.0f, parameters.portamentoSeconds);
    if (portamento <= 1.0e-5f)
    {
        currentPitchSemitones = targetPitchSemitones;
    }
    else
    {
        const float coefficient = 1.0f - std::exp (static_cast<float> (-8.0 / (sampleRate * static_cast<double> (portamento))));
        currentPitchSemitones += (targetPitchSemitones - currentPitchSemitones) * coefficient;
    }

    const float semitones = currentPitchSemitones
        + parameters.coarseSemitones
        + parameters.fineSemitones
        + pitchBendSemitones
        + sampleRatePitchOffsetSemitones
        + pitchOffsetSemitones;

    const int pitch = juce::jlimit (0, 32767, juce::roundToInt (semitones * 128.0f));
    oscillator.set_pitch (static_cast<int16_t> (pitch));
    oscillator.set_parameters (
        static_cast<int16_t> (juce::jlimit (0, 32767, juce::roundToInt (juce::jlimit (0.0f, 1.0f, timbre) * 32767.0f))),
        static_cast<int16_t> (juce::jlimit (0, 32767, juce::roundToInt (juce::jlimit (0.0f, 1.0f, color) * 32767.0f))));
}

void BraidsVoice::advanceModulators (int numSamples, float fmAmount, float modulation) noexcept
{
    for (int i = 0; i < kMsegSlotCount; ++i)
    {
        const auto& slot = parameters.msegSlots[static_cast<size_t> (i)];
        if (slot.destination == static_cast<int> (MsegDestination::off) || slot.shape == nullptr)
            continue;

        const float seconds = juce::jlimit (0.01f, 64.0f, slot.rateSeconds);
        auto& phase = msegPhases[static_cast<size_t> (i)];
        phase += static_cast<double> (numSamples) / (sampleRate * static_cast<double> (seconds));

        if (slot.loop)
            phase -= std::floor (phase);
        else
            phase = juce::jlimit (0.0, 1.0, phase);
    }

    if (fmAmount > 1.0e-5f)
    {
        const double fmFrequency = static_cast<double> (carrierFrequencyHz())
            * static_cast<double> (juce::jlimit (0.25f, 16.0f, parameters.fmRatio));
        fmPhase = wrapPhase (fmPhase + (juce::MathConstants<double>::twoPi
                                        * fmFrequency
                                        * static_cast<double> (numSamples)
                                        / sampleRate));
    }

    if (modulation > 1.0e-5f)
    {
        modPhase = wrapPhase (modPhase + (juce::MathConstants<double>::twoPi
                                          * static_cast<double> (kMacroModRateHz)
                                          * static_cast<double> (numSamples)
                                          / sampleRate));
    }
}

BraidsModulatedValues BraidsVoice::currentModulatedValues() const noexcept
{
    const int maxModel = static_cast<int> (braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META);
    BraidsModulatedValues values;
    float model = static_cast<float> (juce::jlimit (0, maxModel, parameters.model));
    values.timbre = juce::jlimit (0.0f, 1.0f, parameters.timbre);
    values.color = juce::jlimit (0.0f, 1.0f, parameters.color);
    values.modulation = juce::jlimit (0.0f, 1.0f, parameters.modulation);
    values.fmAmount = juce::jlimit (0.0f, 1.0f, parameters.fmAmount);
    values.level = juce::jlimit (0.0f, 1.5f, parameters.level);
    values.pitchOffsetSemitones = 0.0f;

    for (int i = 0; i < kMsegSlotCount; ++i)
    {
        const auto& slot = parameters.msegSlots[static_cast<size_t> (i)];
        const float wave = currentMsegWave (i);

        switch (static_cast<MsegDestination> (slot.destination))
        {
            case MsegDestination::pitch:
                values.pitchOffsetSemitones += targetDelta (0.0f, -36.0f, 36.0f, wave);
                break;
            case MsegDestination::model:
                model += targetDelta (model, 0.0f, static_cast<float> (maxModel), wave);
                break;
            case MsegDestination::timbre:
                values.timbre += targetDelta (values.timbre, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::color:
                values.color += targetDelta (values.color, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::modulation:
                values.modulation += targetDelta (values.modulation, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::fm:
                values.fmAmount += targetDelta (values.fmAmount, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::level:
                values.level += targetDelta (values.level, 0.0f, 1.5f, wave);
                break;
            case MsegDestination::off:
                break;
        }
    }

    values.model = juce::jlimit (0, maxModel, juce::roundToInt (model));
    values.timbre = juce::jlimit (0.0f, 1.0f, values.timbre);
    values.color = juce::jlimit (0.0f, 1.0f, values.color);
    values.modulation = juce::jlimit (0.0f, 1.0f, values.modulation);
    values.fmAmount = juce::jlimit (0.0f, 1.0f, values.fmAmount);
    values.level = juce::jlimit (0.0f, 1.5f, values.level);
    values.pitchOffsetSemitones = juce::jlimit (-36.0f, 36.0f, values.pitchOffsetSemitones);
    return values;
}

float BraidsVoice::currentMsegWave (int slotIndex) const noexcept
{
    if (slotIndex < 0 || slotIndex >= kMsegSlotCount)
        return 0.0f;

    const auto& slot = parameters.msegSlots[static_cast<size_t> (slotIndex)];
    if (slot.destination == static_cast<int> (MsegDestination::off) || slot.shape == nullptr)
        return 0.0f;

    const float rawWave = slot.shape->evaluateBipolar (static_cast<float> (msegPhases[static_cast<size_t> (slotIndex)]));
    const float scaled = (rawWave * (slot.amountPercent * 0.01f)) + (slot.offsetPercent * 0.01f);
    return juce::jlimit (-1.0f, 1.0f, scaled);
}

float BraidsVoice::carrierFrequencyHz() const noexcept
{
    const float semitones = currentPitchSemitones
        + parameters.coarseSemitones
        + parameters.fineSemitones
        + pitchBendSemitones;
    return 440.0f * std::pow (2.0f, (semitones - 69.0f) / 12.0f);
}

float BraidsVoice::targetDelta (float base, float minimum, float maximum, float wave) noexcept
{
    const float clampedBase = juce::jlimit (minimum, maximum, base);
    if (wave >= 0.0f)
        return wave * (maximum - clampedBase);

    return wave * (clampedBase - minimum);
}
} // namespace macro_osc
