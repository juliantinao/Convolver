cmake_minimum_required(VERSION 3.22)

project(Convolver VERSION 0.2.0 LANGUAGES C CXX)

# JUCE is expected at C:\JUCE — override with -DJUCE_DIR=<path> if needed
set(JUCE_DIR "C:/JUCE" CACHE PATH "Path to the JUCE framework")
add_subdirectory(${JUCE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/JUCE")

# Standalone GUI application.
#
# The version is declared exactly once, on the project() line at the top of this
# file. juce_add_gui_app writes it into the executable's VERSIONINFO resource
# (FileVersion and ProductVersion), and tools/dist/build-installer.ps1 reads it
# back out of the built binary, so the installer script never declares it again.
# Change the version in one place, rebuild, and everything downstream follows.
#
# ICON_BIG points at the same PNG that juce_add_binary_data embeds below, so the
# file icon Windows shows in Explorer and the window/taskbar icons drawn by
# Source/AppIcon.h are guaranteed to come from one piece of artwork.
# ICON_BIG takes the raster master: juceaide cannot build a Windows .ico from an
# SVG (see the comment on juce_add_binary_data below). The SVG of the same
# artwork is what the runtime icons use.
juce_add_gui_app(Convolver
    PRODUCT_NAME       "Convolver"
    COMPANY_NAME       "JulianTinao"
    COMPANY_COPYRIGHT  "(C) 2026 JulianTinao"
    VERSION            "${PROJECT_VERSION}"
    ICON_BIG           "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon/appicon.png"
    NEEDS_CURL         FALSE
    NEEDS_WEB_BROWSER  FALSE)

# Keep the executable's VERSIONINFO resource in step with PROJECT_VERSION.
#
# juce_add_gui_app writes the version into JuceLibraryCode/Info.txt, and
# _juce_add_resources_rc (JUCEUtils.cmake) turns that into
# Convolver_resources.rc -- but its add_custom_command lists the *icon* as its
# only dependency, not Info.txt. A version bump therefore never regenerates the
# .rc: the executable relinks against the stale resource and Convolver.exe keeps
# reporting the previous version, with no error anywhere to explain why.
#
# Dropping the .rc whenever Info.txt is newer than it forces the regeneration.
# This has to happen at configure time, which is also when Info.txt is written.
file(GLOB _convolver_info_file "${CMAKE_CURRENT_BINARY_DIR}/Convolver_artefacts/JuceLibraryCode/Info.txt")
file(GLOB _convolver_rc_files  "${CMAKE_CURRENT_BINARY_DIR}/Convolver_artefacts/JuceLibraryCode/*_resources.rc")

foreach(_info_file IN LISTS _convolver_info_file)
    foreach(_rc_file IN LISTS _convolver_rc_files)
        if(EXISTS "${_rc_file}" AND "${_info_file}" IS_NEWER_THAN "${_rc_file}")
            file(REMOVE "${_rc_file}")
            message(STATUS "Version resource is stale - regenerating ${_rc_file}")
        endif()
    endforeach()
endforeach()

# Generate JuceHeader.h for convenient single-include
juce_generate_juce_header(Convolver)

# Make project headers (MainComponent.h, DarkLookAndFeel.h, etc.) resolvable
# from any build configuration.  JuceHeader.h is already handled by
# juce_generate_juce_header() above.
target_include_directories(Convolver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# Source files
target_sources(Convolver PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/HelpWindow.cpp
    # Dark look-and-feel implementation
    Source/DarkLookAndFeel.cpp
    # Frequency-domain convolution engine
    Source/ConvolutionEngine.cpp)

target_compile_features(Convolver PRIVATE cxx_std_20)

# Statically link the MSVC runtime so the .exe doesn't need
# msvcp140.dll / vcruntime140.dll on the target machine.
#
# This has to be applied to every static library we build ourselves, not just to
# the executable. juce_add_binary_data below creates an independent target, and
# a target left at the CMake default uses the DLL runtime (/MDd, /MD) while
# Convolver uses the static one (/MTd, /MT). The linker then pulls in two
# different C runtimes and reports LNK4098 - which is not merely a noisy
# warning: a binary with two CRTs linked in has undefined behaviour.
if(MSVC)
    set(CONVOLVER_MSVC_RUNTIME "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set_property(TARGET Convolver PROPERTY MSVC_RUNTIME_LIBRARY
        "${CONVOLVER_MSVC_RUNTIME}")
endif()

# Preprocessor definitions required by JUCE
target_compile_definitions(Convolver PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_LOG_ASSERTIONS=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Convolver,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Convolver,JUCE_VERSION>")

# Application artwork, embedded as binary data.
#
# Note that this embeds the *SVG*, while ICON_BIG above takes the *PNG*. That
# split is deliberate, not an oversight:
#
#   * juce_add_binary_data feeds the runtime window/taskbar/dialog icons. Those
#     want vector, so Source/AppIcon.h can rasterise the artwork natively at
#     whatever size each surface needs instead of downscaling a bitmap.
#   * ICON_BIG cannot take an SVG at all: juceaide resolves it to a
#     DrawableComposite, which never calls Component::setBounds(), so its
#     getWidth() reads 0 and every requested icon size is discarded, leaving an
#     empty .ico.
#
# Both files are produced together by tools/icons/make-app-icon.ps1 from one
# geometry definition, so they cannot drift apart.
#
# The header is named explicitly rather than left as the default "BinaryData.h"
# so a future second binary-data target cannot create an ambiguous include.
juce_add_binary_data(ConvolverAssets
    HEADER_NAME "ConvolverAssets.h"
    NAMESPACE   "ConvolverAssets"
    SOURCES     packaging/icon/appicon.svg)

# Same static runtime as Convolver above; see the comment there for why leaving
# this target at the default produces LNK4098.
if(MSVC)
    set_property(TARGET ConvolverAssets PROPERTY MSVC_RUNTIME_LIBRARY
        "${CONVOLVER_MSVC_RUNTIME}")
endif()

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
    ConvolverAssets
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
