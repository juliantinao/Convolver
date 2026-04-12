cmake_minimum_required(VERSION 3.22)

project(Convolver VERSION 0.1.0 LANGUAGES CXX)

# JUCE is expected at C:\JUCE — override with -DJUCE_DIR=<path> if needed
set(JUCE_DIR "C:/JUCE" CACHE PATH "Path to the JUCE framework")
add_subdirectory(${JUCE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/JUCE")

# Standalone GUI application
juce_add_gui_app(Convolver
    PRODUCT_NAME       "Convolver"
    COMPANY_NAME       "JulianTinao"
    VERSION            "0.1.0"
    NEEDS_CURL         FALSE
    NEEDS_WEB_BROWSER  FALSE)

# Generate JuceHeader.h for convenient single-include
juce_generate_juce_header(Convolver)

# Source files
target_sources(Convolver PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp)

target_compile_features(Convolver PRIVATE cxx_std_20)

# Preprocessor definitions required by JUCE
target_compile_definitions(Convolver PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_LOG_ASSERTIONS=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Convolver,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Convolver,JUCE_VERSION>")

# Link the JUCE modules needed for batch WAV convolution
#   juce_core            — File I/O, threading, containers
#   juce_events          — Application event loop, timers
#   juce_data_structures — ValueTree, undo manager
#   juce_graphics        — 2-D rendering
#   juce_gui_basics      — Components, windows, look-and-feel
#   juce_gui_extra       — File browser, utility GUI widgets
#   juce_audio_basics    — AudioBuffer, MIDI basics
#   juce_audio_formats   — WAV / FLAC / AIFF reading and writing
#   juce_audio_devices   — Audio hardware I/O (needed by audio_utils)
#   juce_audio_utils     — AudioAppComponent, AudioDeviceSelector
#   juce_dsp             — FFT, Convolution, windowing functions
target_link_libraries(Convolver PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
