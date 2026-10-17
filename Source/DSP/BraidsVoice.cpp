#include "DSP/BraidsVoice.h"

#include <cmath>
#include <utility>

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
// Each voice sits about 9 dB below full scale after its effects, so a full
// chord stays clear of the output limiter instead of driving it constantly.
constexpr float kVoiceHeadroom = 0.355f;

[[nodiscard]] double wrapPhase (double phase) noexcept
{
    const double wrapped = std::fmod (phase, juce::MathConstants<double>::twoPi);
    return wrapped < 0.0 ? wrapped + juce::MathConstants<double>::twoPi : wrapped;
}
} // namespace

BraidsVoice::BraidsVoice()
{
    msegRandom.setSeedRandomly();
    oscillator.Init();
    resetParameterSmoothers();
}

void BraidsVoice::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    sampleRatePitchOffsetSemitones = static_cast<float> (12.0 * std::log2 (kBraidsNativeSampleRate / sampleRate));
    resetParameterSmoothers();
    effects.prepare (sampleRate);
    renderReadIndex = kRenderBlockSize;
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

void BraidsVoice::drawMsegOffsets (int slotIndex) noexcept
{
    for (auto& offset : msegOffsets[static_cast<size_t> (slotIndex)])
        offset = { msegRandom.nextFloat() * 2.0f - 1.0f, msegRandom.nextFloat() * 2.0f - 1.0f };
}

void BraidsVoice::beginGlide (float targetPitch)
{
    // Glide takes the set time whatever the interval, from wherever the pitch is.
    targetPitchSemitones = targetPitch;
    const float portamento = smoothedParameters.portamentoSeconds;
    if (portamento <= 1.0e-5f)
    {
        currentPitchSemitones = targetPitchSemitones;
        glideRateSemitonesPerSecond = 0.0f;
        return;
    }
    glideRateSemitonesPerSecond = std::abs (targetPitchSemitones - currentPitchSemitones) / portamento;
}

void BraidsVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheelPosition)
{
    if (std::exchange (legatoPending, false) && envelopeStage != EnvelopeStage::idle)
    {
        // Legato: keep the envelope, oscillator and modulation running.
        currentMidiNote = juce::jlimit (0, 127, midiNoteNumber);
        if (nextGlideStartPitch)
            currentPitchSemitones = *nextGlideStartPitch;
        nextGlideStartPitch.reset();
        beginGlide (static_cast<float> (currentMidiNote));
        pitchWheelMoved (pitchWheelPosition);
        return;
    }
    if (! keepStealTailOnStart)
        stealTailLength = stealTailPosition = 0;
    keepStealTailOnStart = false;
    resetParameterSmoothers();
    currentMidiNote = juce::jlimit (0, 127, midiNoteNumber);
    currentPitchSemitones = nextGlideStartPitch.value_or (static_cast<float> (currentMidiNote));
    nextGlideStartPitch.reset();
    beginGlide (static_cast<float> (currentMidiNote));
    currentVelocity = juce::jlimit (0.0f, 1.0f, velocity);
    msegPhases.fill (0.0);
    for (int slot = 0; slot < kMsegSlotCount; ++slot)
        drawMsegOffsets (slot);
    fmPhase = 0.0;
    modPhase = 0.0;
    renderReadIndex = kRenderBlockSize;
    pitchWheelMoved (pitchWheelPosition);
    oscillator.Init();
    oscillator.Strike();
    effects.reset();
    envelopeStage = EnvelopeStage::attack;
    envelopeValue = smoothedParameters.attackSeconds <= 1.0e-5f ? 1.0f : 0.0f;
    releaseStartValue = 0.0f;
    if (envelopeValue >= 1.0f)
        envelopeStage = EnvelopeStage::decay;
}

void BraidsVoice::stopNote (float, bool allowTailOff)
{
    // A legato start replaces the note in place; JUCE's preceding hard stop is skipped.
    if (legatoPending && envelopeStage != EnvelopeStage::idle)
        return;

    // JUCE can stop the same voice again on pedal-up, another note-off, or a
    // repeated note-on. A release is a one-way transition: restarting its slope
    // would keep old notes sounding well beyond the original key release.
    if (allowTailOff && envelopeStage == EnvelopeStage::release)
        return;

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

void BraidsVoice::captureStealTail()
{
    // Preserve five milliseconds of the outgoing oscillator and effect history.
    // Fixed stack storage also handles another steal during an existing fade.
    const int frames = juce::jlimit (1, kStealFadeCapacity, juce::roundToInt (sampleRate * 0.005));
    std::array<std::array<float, kStealFadeCapacity>, 2> captured {};
    float* channels[] { captured[0].data(), captured[1].data() };
    juce::AudioBuffer<float> buffer (channels, 2, frames);
    renderNextBlock (buffer, 0, frames);
    stealTail = captured;
    stealTailLength = frames;
    stealTailPosition = 0;
    keepStealTailOnStart = true;
}

void BraidsVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const int numChannels = outputBuffer.getNumChannels();
    int rendered = 0;
    while (rendered < numSamples)
    {
        if (renderReadIndex == kRenderBlockSize)
            renderOscillatorBlock();
        const int chunk = juce::jmin (kRenderBlockSize - renderReadIndex, numSamples - rendered);

        const int outputIndex = startSample + rendered;
        auto* left = numChannels > 0 ? outputBuffer.getWritePointer (0, outputIndex) : nullptr;
        auto* right = numChannels > 1 ? outputBuffer.getWritePointer (1, outputIndex) : nullptr;
        for (int i = 0; i < chunk; ++i)
        {
            const float env = nextEnvelopeSample();
            const float level = juce::jlimit (0.0f, 1.5f, parameterSmoothers.level.getNextValue() + modulatedLevelOffset);
            float sample = static_cast<float> (renderBuffer[static_cast<size_t> (renderReadIndex + i)]) / 32768.0f;
            sample *= env * currentVelocity * level;
            sample = juce::jlimit (-1.0f, 1.0f, sample);

            float sampleLeft = sample, sampleRight = sample;
            effects.process (sampleLeft, sampleRight);
            sampleLeft *= kVoiceHeadroom;
            sampleRight *= kVoiceHeadroom;
            if (stealTailPosition < stealTailLength)
            {
                const float fade = static_cast<float> (stealTailPosition) / static_cast<float> (stealTailLength);
                const auto tailIndex = static_cast<size_t> (stealTailPosition++);
                sampleLeft = stealTail[0][tailIndex] * (1.0f - fade) + sampleLeft * fade;
                sampleRight = stealTail[1][tailIndex] * (1.0f - fade) + sampleRight * fade;
            }
            if (left != nullptr)
                left[i] += numChannels == 1 ? 0.5f * (sampleLeft + sampleRight) : sampleLeft;
            if (right != nullptr)
                right[i] += sampleRight;
            for (int channel = 2; channel < numChannels; ++channel)
                outputBuffer.addSample (channel, outputIndex + i, 0.5f * (sampleLeft + sampleRight));
        }

        renderReadIndex += chunk;
        rendered += chunk;

        if (! envelopeActive() && stealTailPosition >= stealTailLength)
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

void BraidsVoice::renderOscillatorBlock()
{
    // All models receive complete 24-sample blocks. Keep the remainder across
    // host callbacks and parameter changes, including single-sample requests.
    // Control and modulation clocks also follow this stream, not host splits.
    advanceSmoothedParameters (kRenderBlockSize);
    const auto values = currentModulatedValues();
    effects.beginBlock (values.effects, kRenderBlockSize);
    modulatedLevelOffset = values.level - smoothedParameters.level;
    const float fmWave = values.fmAmount > 1.0e-5f ? std::sin (static_cast<float> (fmPhase)) : 0.0f;
    const float modWave = values.modulation > 1.0e-5f ? std::sin (static_cast<float> (modPhase)) : 0.0f;
    const float timbreOffset = modWave * values.modulation * kMacroModDepth;
    oscillator.set_shape (static_cast<braids::MacroOscillatorShape> (values.model));
    updateOscillatorParameters (values.pitchOffsetSemitones + fmWave * values.fmAmount * kMaxFmDepthSemitones,
                                values.timbre + timbreOffset,
                                values.color - timbreOffset);
    syncBuffer.fill (0);
    oscillator.Render (syncBuffer.data(), renderBuffer.data(), renderBuffer.size());
    renderReadIndex = 0;
    advanceModulators (kRenderBlockSize, values.fmAmount, values.modulation);
}

void BraidsVoice::updateOscillatorParameters (float pitchOffsetSemitones, float timbre, float color)
{
    if (glideRateSemitonesPerSecond <= 0.0f || smoothedParameters.portamentoSeconds <= 1.0e-5f)
    {
        currentPitchSemitones = targetPitchSemitones;
    }
    else
    {
        // Linear in pitch at the rate set when the glide began; one render block per step.
        const float step = static_cast<float> (glideRateSemitonesPerSecond * kRenderBlockSize / sampleRate);
        currentPitchSemitones += juce::jlimit (-step, step, targetPitchSemitones - currentPitchSemitones);
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
        {
            // Each new cycle gets new random offsets, so a loop never repeats exactly.
            if (phase >= 1.0)
                drawMsegOffsets (i);
            phase -= std::floor (phase);
        }
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
    values.effects = parameters.effects;
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
            case MsegDestination::filterFrequency:
            {
                const float logFrequency = std::log2 (juce::jlimit (20.0f, 20000.0f, values.effects.filterFrequency));
                values.effects.filterFrequency = std::exp2 (logFrequency
                    + targetDelta (logFrequency, std::log2 (20.0f), std::log2 (20000.0f), wave));
                break;
            }
            case MsegDestination::filterQ:
                values.effects.filterQ += targetDelta (values.effects.filterQ, 1.0f, 30.0f, wave);
                break;
            case MsegDestination::distortionAmount:
                values.effects.distortionAmount += targetDelta (values.effects.distortionAmount, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::distortionMix:
                values.effects.distortionMix += targetDelta (values.effects.distortionMix, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::distortionOutput:
                values.effects.distortionOutput += targetDelta (values.effects.distortionOutput, 0.0f, 1.0f, wave);
                break;
            case MsegDestination::filterType:
                values.effects.filterType = juce::jlimit (0, 3, juce::roundToInt (
                    static_cast<float> (values.effects.filterType)
                    + targetDelta (static_cast<float> (values.effects.filterType), 0.0f, 3.0f, wave)));
                break;
            case MsegDestination::distortionType:
                values.effects.distortionType = lodo::engine::fx::clampDistortionType (juce::roundToInt (
                    static_cast<float> (values.effects.distortionType)
                    + targetDelta (static_cast<float> (values.effects.distortionType), 0.0f,
                                   static_cast<float> (lodo::engine::fx::maxDistortionType), wave)));
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

    const float rawWave = slot.shape->evaluateUnipolar (static_cast<float> (msegPhases[static_cast<size_t> (slotIndex)]),
                                                        msegOffsets[static_cast<size_t> (slotIndex)],
                                                        slot.randomPercent * 0.01f) * 2.0f - 1.0f;
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
