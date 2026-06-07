cmake_minimum_required(VERSION 3.22)

project(MacroOsc VERSION 0.1.0 LANGUAGES C CXX)

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(JUCE_DIR "/Users/reecemitchell/Programming/JUCE" CACHE PATH "Path to a JUCE checkout")
if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE_DIR must point to a JUCE checkout containing CMakeLists.txt")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

juce_add_binary_data(MacroOscAssets
    SOURCES
        Source/Assets/macro_oscillator_backing.png
        Source/Assets/DSEG14Modern-Regular.ttf
        Source/Assets/Inter-Bold.ttf
        Source/Assets/Inter-SemiBold.ttf)

set(MUTABLE_BRAIDS_SOURCES
    Source/ThirdParty/Mutable/braids/analog_oscillator.cc
    Source/ThirdParty/Mutable/braids/digital_oscillator.cc
    Source/ThirdParty/Mutable/braids/macro_oscillator.cc
    Source/ThirdParty/Mutable/braids/resources.cc
    Source/ThirdParty/Mutable/stmlib/dsp/units.cc
    Source/ThirdParty/Mutable/stmlib/utils/random.cc)

if (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
    set_source_files_properties(${MUTABLE_BRAIDS_SOURCES}
        PROPERTIES
            COMPILE_OPTIONS "-w")
endif()

juce_add_plugin(MacroOsc
    COMPANY_NAME "Reece Mitchell"
    PRODUCT_NAME "Macro OSC"
    BUNDLE_ID "com.reecemitchell.macroosc"
    PLUGIN_MANUFACTURER_CODE Rmco
    PLUGIN_CODE Mosc
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(MacroOsc
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/BraidsVoice.cpp
        ${MUTABLE_BRAIDS_SOURCES})

target_include_directories(MacroOsc
    PRIVATE
        Source)

target_include_directories(MacroOsc
    SYSTEM PRIVATE
        Source/ThirdParty/Mutable)

target_compile_definitions(MacroOsc
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MacroOsc
    PRIVATE
        MacroOscAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (TARGET MacroOsc_Standalone)
    target_sources(MacroOsc_Standalone
        PRIVATE
            Source/StandaloneApp.cpp)

    target_compile_definitions(MacroOsc_Standalone
        PRIVATE
            JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
endif()
