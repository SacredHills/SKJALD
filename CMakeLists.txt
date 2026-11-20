cmake_minimum_required(VERSION 3.22)

# Must be set before project() to take effect on Apple platforms
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

project(SKJALD VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SKJALD_BUILD_PLUGIN "Build the VST3 plugin (requires JUCE)" ON)
option(SKJALD_BUILD_TESTS "Build the automated DSP tests" ON)
option(SKJALD_BUILD_TOOLS "Build the offline WAV processing utility" ON)
option(SKJALD_BUILD_UI_PREVIEW "Build the headless UI preview tool (development)" OFF)
option(SKJALD_COPY_PLUGIN_AFTER_BUILD "Install the VST3 into the user plug-in folder after building" OFF)
set(SKJALD_JUCE_PATH "" CACHE PATH "Path to a local JUCE 8 checkout (downloaded automatically when empty)")
set(SKJALD_JUCE_TAG "8.0.10" CACHE STRING "JUCE git tag used when downloading")

# ---------------------------------------------------------------------------------------
# DSP engine: pure C++20, no JUCE dependency (shared by plugin, tests and offline tool)
# ---------------------------------------------------------------------------------------
add_library(skjald_dsp STATIC
    Source/DSP/Filters/FilterDesign.cpp
    Source/DSP/Equalizer/EQBand.cpp
    Source/DSP/Equalizer/Equalizer.cpp
    Source/DSP/Oversampling/HalfbandStage.cpp
    Source/DSP/Oversampling/Oversampler.cpp
    Source/DSP/Oversampling/PeakInterpolator.cpp
    Source/DSP/Oversampling/TruePeakDetector.cpp
    Source/DSP/Maximizer/Leveler.cpp
    Source/DSP/Maximizer/LimiterCore.cpp
    Source/DSP/Maximizer/TruePeakGuard.cpp
    Source/DSP/Maximizer/TransientDetector.cpp
    Source/DSP/Maximizer/Maximizer.cpp
    Source/Metering/LoudnessMeter.cpp
    Source/Metering/TruePeakMeter.cpp
    Source/Engine/MasteringEngine.cpp
)
target_include_directories(skjald_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)

if(MSVC)
    target_compile_options(skjald_dsp PRIVATE /W4 /permissive- /fp:precise)
    target_compile_definitions(skjald_dsp PUBLIC _USE_MATH_DEFINES NOMINMAX)
else()
    target_compile_options(skjald_dsp PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion
                                              -Wno-float-conversion -Wno-implicit-float-conversion
                                              $<$<CONFIG:Release>:-O3>)
endif()

# ---------------------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------------------
if(SKJALD_BUILD_PLUGIN)
    if(SKJALD_JUCE_PATH)
        add_subdirectory(${SKJALD_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG ${SKJALD_JUCE_TAG}
            GIT_SHALLOW ON)
        FetchContent_MakeAvailable(JUCE)
    endif()
    add_subdirectory(Source/Plugin)
    if(SKJALD_BUILD_UI_PREVIEW)
        add_subdirectory(Tools/UIPreview)
    endif()
endif()

if(SKJALD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

if(SKJALD_BUILD_TOOLS)
    add_subdirectory(Tools/OfflineProcessor)
endif()
