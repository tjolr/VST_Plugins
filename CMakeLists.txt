cmake_minimum_required(VERSION 3.15)

project(GuitarToBassPlugin VERSION 1.0.0)

# Set minimum macOS version to avoid deprecated API issues
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum OS X deployment version")

# Add compiler flags to suppress warnings for JUCE build
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

# Add JUCE subdirectory
add_subdirectory(JUCE)

# Create the plugin target
juce_add_plugin(GuitarToBassPlugin
    VERSION 1.0.0
    COMPANY_NAME "GuitarToBass"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE GtBs
    PLUGIN_CODE GtBs
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Guitar to Bass")

# Add source files
target_sources(GuitarToBassPlugin
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

# Compiler definitions
target_compile_definitions(GuitarToBassPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

# Link JUCE modules
target_link_libraries(GuitarToBassPlugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Enable testing
enable_testing()

# Find GoogleTest (if available)
find_package(GTest QUIET)

if(GTest_FOUND)
    # Add test executable
    add_executable(GuitarToBassTests
        tests/build_config_test.cpp)
    
    target_link_libraries(GuitarToBassTests
        PRIVATE
            GTest::gtest_main
            GTest::gtest)
    
    # Add tests to CTest
    include(GoogleTest)
    gtest_discover_tests(GuitarToBassTests)
else()
    message(STATUS "GoogleTest not found - skipping unit tests")
endif()