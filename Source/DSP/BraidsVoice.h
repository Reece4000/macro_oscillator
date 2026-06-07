#pragma once

#include "DSP/Mseg.h"
#include "braids/macro_oscillator.h"

#include <array>

#include <juce_audio_basics/juce_audio_basics.h>

namespace macro_osc
{
struct BraidsVoiceParameters
{
    int model {};
    float timbre { 0.5f };
    float color { 0.5f };
    float modulation { 0.0f };
    float fmAmount { 0.0f };
    float fmRatio { 1.0f };
    float level { 0.8f };
    float coarseSemitones {};
    float fineSemitones {};
    float portamentoSeconds {};
    float attackSeconds { 0.005f };
    float decaySeconds { 0.25f };
    float sustainLevel { 0.8f };
    float releaseSeconds { 0.25f };

    struct MsegSlot
    {
        int destination {};
        float amountPercent { 50.0f };
        float offsetPercent {};
        float rateSeconds { 1.0f };
        bool loop { true };
        const MsegShape* shape {};
    };

    std::array<MsegSlot, kMsegSlotCount> msegSlots;
};

struct BraidsModulatedValues
{
    int model {};
    float timbre {};
    float color {};
    float modulation {};
    float fmAmount {};
    float level {};
    float pitchOffsetSemitones {};
};

class BraidsSound final : public juce::SynthesiserSound
{
public:
    [[nodiscard]] bool appliesToNote (int) override { return true; }
    [[nodiscard]] bool appliesToChannel (int) override { return true; }
};

class BraidsVoice final : public juce::SynthesiserVoice
{
public:
    BraidsVoice();

    void prepare (double newSampleRate);
    void setParameters (const BraidsVoiceParameters& newParameters);

    [[nodiscard]] bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int controllerNumber, int newControllerValue) override;
    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

private:
    static constexpr int kRenderBlockSize = 24;
    static constexpr double kBraidsNativeSampleRate = 96000.0;
    static constexpr float kPitchBendRangeSemitones = 2.0f;
    using LinearSmoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    enum class EnvelopeStage : std::uint8_t
    {
        idle,
        attack,
        decay,
        sustain,
        release
    };

    struct SmoothedParameterSnapshot
    {
        float modulation { 0.0f };
        float fmAmount { 0.0f };
        float fmRatio { 1.0f };
        float level { 0.8f };
        float coarseSemitones {};
        float fineSemitones {};
        float portamentoSeconds {};
        float attackSeconds { 0.005f };
        float decaySeconds { 0.25f };
        float sustainLevel { 0.8f };
        float releaseSeconds { 0.25f };

        struct MsegSlot
        {
            float amountPercent { 50.0f };
            float offsetPercent {};
            float rateSeconds { 1.0f };
        };

        std::array<MsegSlot, kMsegSlotCount> msegSlots;
    };

    struct ParameterSmoothers
    {
        LinearSmoother modulation;
        LinearSmoother fmAmount;
        LinearSmoother fmRatio;
        LinearSmoother level;
        LinearSmoother coarseSemitones;
        LinearSmoother fineSemitones;
        LinearSmoother portamentoSeconds;
        LinearSmoother attackSeconds;
        LinearSmoother decaySeconds;
        LinearSmoother sustainLevel;
        LinearSmoother releaseSeconds;

        struct MsegSlot
        {
            LinearSmoother amountPercent;
            LinearSmoother offsetPercent;
            LinearSmoother rateSeconds;
        };

        std::array<MsegSlot, kMsegSlotCount> msegSlots;
    };

    braids::MacroOscillator oscillator;
    BraidsVoiceParameters parameters;
    ParameterSmoothers parameterSmoothers;
    SmoothedParameterSnapshot smoothedParameters;
    EnvelopeStage envelopeStage { EnvelopeStage::idle };
    float envelopeValue {};
    float releaseStartValue {};
    double sampleRate { 44100.0 };
    float sampleRatePitchOffsetSemitones {};
    float currentVelocity {};
    int currentMidiNote { 60 };
    float currentPitchSemitones { 60.0f };
    float targetPitchSemitones { 60.0f };
    float previousTargetPitchSemitones { 60.0f };
    float pitchBendSemitones {};
    std::array<double, kMsegSlotCount> msegPhases {};
    double fmPhase {};
    double modPhase {};
    std::array<int16_t, kRenderBlockSize> renderBuffer {};
    std::array<int16_t, kRenderBlockSize> oscillatorTempBuffer {};
    std::array<std::uint8_t, kRenderBlockSize> syncBuffer {};
    int16_t carrySample {};
    bool carryValid {};
    bool hasPreviousTargetPitch {};

    void resetParameterSmoothers();
    void setParameterSmootherTargets() noexcept;
    void advanceSmoothedParameters (int numSamples) noexcept;
    [[nodiscard]] float nextEnvelopeSample() noexcept;
    [[nodiscard]] bool envelopeActive() const noexcept;
    void renderOscillatorChunk (int numSamples, float pitchOffsetSemitones, float timbre, float color);
    void updateOscillatorParameters (float pitchOffsetSemitones, float timbre, float color);
    void advanceModulators (int numSamples, float fmAmount, float modulation) noexcept;
    [[nodiscard]] BraidsModulatedValues currentModulatedValues() const noexcept;
    [[nodiscard]] float currentMsegWave (int slotIndex) const noexcept;
    [[nodiscard]] float carrierFrequencyHz() const noexcept;
    [[nodiscard]] static float targetDelta (float base, float minimum, float maximum, float wave) noexcept;
};
} // namespace macro_osc
