cmake_minimum_required(VERSION 3.22)
project(SoundMaster VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# UWAGA: nazwa targetu "SimpleVerb" zostaje celowo, zeby dzialal ten sam workflow GitHub Actions.
# Plik, ktory powstanie i zobaczysz w Reaperze, nazywa sie "SoundMaster".
juce_add_plugin(SimpleVerb
    COMPANY_NAME "MyCompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Mycm
    PLUGIN_CODE Smst
    FORMATS VST3
    PLUGIN_NAME "SoundMaster"
    PRODUCT_NAME "SoundMaster")

target_sources(SimpleVerb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(SimpleVerb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SimpleVerb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
