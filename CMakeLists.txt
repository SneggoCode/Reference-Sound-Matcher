cmake_minimum_required(VERSION 3.15)
project(ReferenceToneMatcher VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

if(NOT DEFINED JUCE_TAG)
    set(JUCE_TAG 8.0.1)
endif()

FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${JUCE_TAG}
)

FetchContent_MakeAvailable(juce)

juce_add_plugin(ReferenceToneMatcher
    COMPANY_NAME "Reference DSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE RefM
    PLUGIN_CODE Rtmt
    FORMATS VST3
    PRODUCT_NAME "ReferenceToneMatcher"
)

set(SOURCE_FILES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/dsp/ReferenceProfile.h
    Source/dsp/SpectrumAnalyser.h
    Source/dsp/SpectrumAnalyser.cpp
    Source/dsp/EQDesigner.h
    Source/dsp/EQDesigner.cpp
    Source/dsp/Exciter.h
    Source/dsp/Exciter.cpp
    Source/dsp/TransientDesigner.h
    Source/dsp/TransientDesigner.cpp
    Source/dsp/MultiBandDynamics.h
    Source/dsp/MultiBandDynamics.cpp
)

target_sources(ReferenceToneMatcher PRIVATE ${SOURCE_FILES})

target_link_libraries(ReferenceToneMatcher
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_audio_devices
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_gui_basics
)

target_compile_definitions(ReferenceToneMatcher
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

juce_generate_juce_header(ReferenceToneMatcher)

set_target_properties(ReferenceToneMatcher PROPERTIES
    VS_GLOBAL_VcpkgEnableManifest TRUE
)

