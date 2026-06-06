#pragma once

#include "DSP/BraidsVoice.h"
#include "DSP/Mseg.h"

#include <array>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

namespace macro_osc
{
namespace ParamIDs
{
static constexpr const char* model = "model";
static constexpr const char* timbre = "timbre";
static constexpr const char* color = "color";
static constexpr const char* modulation = "modulation";
static constexpr const char* fmAmount = "fmAmount";
static constexpr const char* fmRatio = "fmRatio";
static constexpr const char* level = "level";
static constexpr const char* coarse = "coarse";
static constexpr const char* fine = "fine";
static constexpr const char* portamento = "portamento";
static constexpr const char* scale = "scale";
static constexpr const char* attack = "attack";
static constexpr const char* decay = "decay";
static constexpr const char* sustain = "sustain";
static constexpr const char* release = "release";
static constexpr std::array<const char*, kMsegSlotCount> msegDestination {
    "mseg1Destination", "mseg2Destination", "mseg3Destination"
};
static constexpr std::array<const char*, kMsegSlotCount> msegAmount {
    "mseg1Amount", "mseg2Amount", "mseg3Amount"
};
static constexpr std::array<const char*, kMsegSlotCount> msegOffset {
    "mseg1Offset", "mseg2Offset", "mseg3Offset"
};
static constexpr std::array<const char*, kMsegSlotCount> msegRate {
    "mseg1Rate", "mseg2Rate", "mseg3Rate"
};
static constexpr std::array<const char*, kMsegSlotCount> msegLoop {
    "mseg1Loop", "mseg2Loop", "mseg3Loop"
};
} // namespace ParamIDs

[[nodiscard]] juce::StringArray braidsModelNames();

class MacroOscAudioProcessor final : public juce::AudioProcessor,
                                     public juce::ChangeBroadcaster
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    MacroOscAudioProcessor();
    ~MacroOscAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Macro OSC"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    [[nodiscard]] APVTS& getState() noexcept { return parameters; }
    [[nodiscard]] const APVTS& getState() const noexcept { return parameters; }

    [[nodiscard]] std::vector<MsegPoint> getMsegPoints (int slotIndex) const;
    [[nodiscard]] std::shared_ptr<const MsegShape> getMsegShape (int slotIndex) const;
    void setMsegPoints (int slotIndex, std::vector<MsegPoint> points, bool notifyEditor = true);

    static APVTS::ParameterLayout createParameterLayout();

private:
    juce::Synthesiser synth;
    APVTS parameters;
    std::array<std::shared_ptr<const MsegShape>, kMsegSlotCount> currentMsegShapes;

    [[nodiscard]] BraidsVoiceParameters buildVoiceParameters() const;
    [[nodiscard]] float getFloatParameter (const char* id) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroOscAudioProcessor)
};
} // namespace macro_osc
