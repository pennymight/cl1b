cmake_minimum_required(VERSION 3.22)
project(OptoComp VERSION 0.1.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(OptoComp
    COMPANY_NAME "YourName"
    PLUGIN_MANUFACTURER_CODE Yrnm
    PLUGIN_CODE Opco
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "OptoComp"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(OptoComp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h)

target_compile_definitions(OptoComp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(OptoComp PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
