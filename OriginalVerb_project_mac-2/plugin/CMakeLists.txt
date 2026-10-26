cmake_minimum_required(VERSION 3.22)
project(OriginalVerb VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt)
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(OriginalVerb
    COMPANY_NAME "OriginalDSP"
    PLUGIN_MANUFACTURER_CODE Orig
    PLUGIN_CODE Ovrb
    AU_MAIN_TYPE kAudioUnitType_Effect
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "OriginalVerb"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE)
target_sources(OriginalVerb PRIVATE Source/PluginProcessor.cpp)
target_include_directories(OriginalVerb PRIVATE ../core)
target_compile_definitions(OriginalVerb PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(OriginalVerb PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
