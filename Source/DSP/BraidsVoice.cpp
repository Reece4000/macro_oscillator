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
constexpr double kCustomParameterSmoothTimeSeconds = 0.01;
constexpr float kMinimumSmoothedEnvelopeSeconds = 0.001f;

[[nodiscard]] double wrapPhase (double phase) noexcept
{
    const double wrapped = std::fmod (phase, juce::MathConstants<double>::twoPi);
    return wrapped < 0.0 ? wrapped + juce::MathConstants<double>::twoPi : wrapped;
}
} // namespace

BraidsVoice::BraidsVoice()
{
    oscillator.Init();
    resetParameterSmoothers();
}

void BraidsVoice::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    sampleRatePitchOffsetSemitones = static_cast<float> (12.0 * std::log2 (kBraidsNativeSampleRate / sampleRate));
    resetParameterSmoothers();
}

void BraidsVoice::setParameters (const BraidsVoiceParameters& newParameters)
{
    parameters = newParameters;
    if (isVoiceActive())
        setParameterSmootherTargets();
}

void BraidsVoice::resetParameterSmoothers()
{
    auto reset = [this] (LinearSmoother& smoother, float value)
    {
        smoother.reset (sampleRate, kCustomParameterSmoothTimeSeconds);
        smoother.setCurrentAndTargetValue (value);
    };

    reset (parameterSmoothers.modulation, parameters.modulation);
    reset (parameterSmoothers.fmAmount, parameters.fmAmount);
    reset (parameterSmoothers.fmRatio, parameters.fmRatio);
    reset (parameterSmoothers.level, parameters.level);
    reset (parameterSmoothers.coarseSemitones, parameters.coarseSemitones);
    reset (parameterSmoothers.fineSemitones, parameters.fineSemitones);
    reset (parameterSmoothers.portamentoSeconds, parameters.portamentoSeconds);
    reset (parameterSmoothers.attackSeconds, parameters.attackSeconds);
    reset (parameterSmoothers.decaySeconds, parameters.decaySeconds);
    reset (parameterSmoothers.sustainLevel, parameters.sustainLevel);
    reset (parameterSmoothers.releaseSeconds, parameters.releaseSeconds);

    smoothedParameters.modulation = parameters.modulation;
    smoothedParameters.fmAmount = parameters.fmAmount;
    smoothedParameters.fmRatio = parameters.fmRatio;
    smoothedParameters.level = parameters.level;
    smoothedParameters.coarseSemitones = parameters.coarseSemitones;
    smoothedParameters.fineSemitones = parameters.fineSemitones;
    smoothedParameters.portamentoSeconds = parameters.portamentoSeconds;
    smoothedParameters.attackSeconds = parameters.attackSeconds;
    smoothedParameters.decaySeconds = parameters.decaySeconds;
    smoothedParameters.sustainLevel = parameters.sustainLevel;
    smoothedParameters.releaseSeconds = parameters.releaseSeconds;

    for (int i = 0; i < kMsegSlotCount; ++i)
    {
        const auto& slot = parameters.msegSlots[static_cast<size_t> (i)];
        auto& smoothers = parameterSmoothers.msegSlots[static_cast<size_t> (i)];
        auto& smoothed = smoothedParameters.msegSlots[static_cast<size_t> (i)];

        reset (smoothers.amountPercent, slot.amountPercent);
        reset (smoothers.offsetPercent, slot.offsetPercent);
        reset (smoothers.rateSeconds, slot.rateSeconds);

        smoothed.amountPercent = slot.amountPercent;
        smoothed.offsetPercent = slot.offsetPercent;
        smoothed.rateSeconds = slot.rateSeconds;
    }
}

void BraidsVoice::setParameterSmootherTargets() noexcept
{
    parameterSmoothers.modulation.setTargetValue (parameters.modulation);
    parameterSmoothers.fmAmount.setTargetValue (parameters.fmAmount);
    parameterSmoothers.fmRatio.setTargetValue (parameters.fmRatio);
    parameterSmoothers.level.setTargetValue (parameters.level);
    parameterSmoothers.coarseSemitones.setTargetValue (parameters.coarseSemitones);
    parameterSmoothers.fineSemitones.setTargetValue (parameters.fineSemitones);
    parameterSmoothers.portamentoSeconds.setTargetValue (parameters.portamentoSeconds);
    parameterSmoothers.attackSeconds.setTargetValue (parameters.attackSeconds);
    parameterSmoothers.decaySeconds.setTargetValue (parameters.decaySeconds);
    parameterSmoothers.sustainLevel.setTargetValue (parameters.sustainLevel);
    parameterSmoothers.releaseSeconds.setTargetValue (parameters.releaseSeconds);

    for (int i = 0; i < kMsegSlotCount; ++i)
    {
        const auto& slot = parameters.msegSlots[static_cast<size_t> (i)];
        auto& smoothers = parameterSmoothers.msegSlots[static_cast<size_t> (i)];

        smoothers.amountPercent.setTargetValue (slot.amountPercent);
        smoothers.offsetPercent.setTargetValue (slot.offsetPercent);
        smoothers.rateSeconds.setTargetValue (slot.rateSeconds);
    }
}

void BraidsVoice::advanceSmoothedParameters (int numSamples) noexcept
{
    smoothedParameters.modulation = parameterSmoothers.modulation.skip (numSamples);
    smoothedParameters.fmAmount = parameterSmoothers.fmAmount.skip (numSamples);
    smoothedParameters.fmRatio = parameterSmoothers.fmRatio.skip (numSamples);
    smoothedParameters.level = parameterSmoothers.level.getCurrentValue();
    smoothedParameters.coarseSemitones = parameterSmoothers.coarseSemitones.skip (numSamples);
    smoothedParameters.fineSemitones = parameterSmoothers.fineSemitones.skip (numSamples);
    smoothedParameters.portamentoSeconds = parameterSmoothers.portamentoSeconds.skip (numSamples);
    smoothedParameters.attackSeconds = parameterSmoothers.attackSeconds.skip (numSamples);
    smoothedParameters.decaySeconds = parameterSmoothers.decaySeconds.skip (numSamples);
    smoothedParameters.sustainLevel = parameterSmoothers.sustainLevel.skip (numSamples);
    smoothedParameters.releaseSeconds = parameterSmoothers.releaseSeconds.skip (numSamples);

    for (int i = 0; i < kMsegSlotCount; ++i)
    {
        auto& smoothers = parameterSmoothers.msegSlots[static_cast<size_t> (i)];
        auto& smoothed = smoothedParameters.msegSlots[static_cast<size_t> (i)];

        smoothed.amountPercent = smoothers.amountPercent.skip (numSamples);
        smoothed.offsetPercent = smoothers.offsetPercent.skip (numSamples);
        smoothed.rateSeconds = smoothers.rateSeconds.skip (numSamples);
    }
}

bool BraidsVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<BraidsSound*> (sound) != nullptr;
}

void BraidsVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheelPosition)
{
    resetParameterSmoothers();
    currentMidiNote = juce::jlimit (0, 127, midiNoteNumber);
    targetPitchSemitones = static_cast<float> (currentMidiNote);
    currentPitchSemitones = smoothedParameters.portamentoSeconds > 1.0e-5f && hasPreviousTargetPitch
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
    envelopeValue = smoothedParameters.attackSeconds <= 1.0e-5f ? 1.0f : 0.0f;
    releaseStartValue = 0.0f;
    if (envelopeValue >= 1.0f)
        envelopeStage = EnvelopeStage::decay;
}

void BraidsVoice::stopNote (float, bool allowTailOff)
{
    const float release = juce::jlimit (0.0f, 10.0f, smoothedParameters.releaseSeconds);
    if (allowTailOff && release > 1.0e-5f && envelopeValue > kVoiceSilenceThreshold)
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

    const int numChannels = outputBuffer.getNumChannels();
    int rendered = 0;
    while (rendered < numSamples)
    {
        const int chunk = juce::jmin (kRenderBlockSize, numSamples - rendered);
        advanceSmoothedParameters (chunk);
        const auto values = currentModulatedValues();
        const float levelOffset = values.level - smoothedParameters.level;
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
        const int outputIndex = startSample + rendered;
        auto* left = numChannels > 0 ? outputBuffer.getWritePointer (0, outputIndex) : nullptr;
        auto* right = numChannels > 1 ? outputBuffer.getWritePointer (1, outputIndex) : nullptr;
        for (int i = 0; i < chunk; ++i)
        {
            const float env = nextEnvelopeSample();
            const float level = juce::jlimit (0.0f, 1.5f, parameterSmoothers.level.getNextValue() + levelOffset);
            float sample = static_cast<float> (renderBuffer[static_cast<size_t> (i)]) / 32768.0f;
            sample *= env * currentVelocity * level;
            sample = juce::jlimit (-1.0f, 1.0f, sample);
            audible = audible || std::abs (sample) > kVoiceSilenceThreshold || env > kVoiceSilenceThreshold;

            if (left != nullptr)
                left[i] += sample;
            if (right != nullptr)
                right[i] += sample;
            for (int channel = 2; channel < numChannels; ++channel)
                outputBuffer.addSample (channel, outputIndex + i, sample);
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
    const float sustain = juce::jlimit (0.0f, 1.0f, smoothedParameters.sustainLevel);

    switch (envelopeStage)
    {
        case EnvelopeStage::idle:
            envelopeValue = 0.0f;
            return 0.0f;

        case EnvelopeStage::attack:
        {
            const float attack = juce::jlimit (kMinimumSmoothedEnvelopeSeconds, 10.0f, smoothedParameters.attackSeconds);
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
            const float decay = juce::jlimit (kMinimumSmoothedEnvelopeSeconds, 10.0f, smoothedParameters.decaySeconds);
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
            const float release = juce::jlimit (kMinimumSmoothedEnvelopeSeconds, 10.0f, smoothedParameters.releaseSeconds);
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
    std::fill_n (syncBuffer.begin(), static_cast<size_t> (renderSize), static_cast<std::uint8_t> (0));

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
    const float portamento = juce::jlimit (0.0f, 2.0f, smoothedParameters.portamentoSeconds);
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
        + smoothedParameters.coarseSemitones
        + smoothedParameters.fineSemitones
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

        const float seconds = juce::jlimit (0.01f, 64.0f, smoothedParameters.msegSlots[static_cast<size_t> (i)].rateSeconds);
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
            * static_cast<double> (juce::jlimit (0.25f, 16.0f, smoothedParameters.fmRatio));
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
    values.modulation = juce::jlimit (0.0f, 1.0f, smoothedParameters.modulation);
    values.fmAmount = juce::jlimit (0.0f, 1.0f, smoothedParameters.fmAmount);
    values.level = juce::jlimit (0.0f, 1.5f, smoothedParameters.level);
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
    const auto& smoothedSlot = smoothedParameters.msegSlots[static_cast<size_t> (slotIndex)];
    const float scaled = (rawWave * (smoothedSlot.amountPercent * 0.01f)) + (smoothedSlot.offsetPercent * 0.01f);
    return juce::jlimit (-1.0f, 1.0f, scaled);
}

float BraidsVoice::carrierFrequencyHz() const noexcept
{
    const float semitones = currentPitchSemitones
        + smoothedParameters.coarseSemitones
        + smoothedParameters.fineSemitones
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
