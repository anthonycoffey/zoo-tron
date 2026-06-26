cmake_minimum_required(VERSION 3.22)

project(ZooTron VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    # Universal (Apple Silicon + Intel) by default, matching the LM-1 build.
    # For a faster local/dev build, override with -DCMAKE_OSX_ARCHITECTURES=arm64
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")
endif()

# --- Pull JUCE in a reproducible, self-contained way --------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.14
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# --- The plugin ---------------------------------------------------------------
juce_add_plugin(ZooTron
    PLUGIN_MANUFACTURER_CODE  Ynme               # same vendor code as LM-1
    PLUGIN_CODE               Zt3a               # unique per-plugin subtype
    FORMATS                   AU VST3 Standalone
    PRODUCT_NAME              "Zoo-Tron III"
    COMPANY_NAME              "Anthony Coffey"
    BUNDLE_ID                 com.anthonycoffey.zootron
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    VST3_CATEGORIES           Fx Filter
    AU_MAIN_TYPE              kAudioUnitType_Effect)

juce_generate_juce_header(ZooTron)

target_sources(ZooTron PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ZooTron PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(ZooTron
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
