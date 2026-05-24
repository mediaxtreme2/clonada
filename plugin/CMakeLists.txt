cmake_minimum_required(VERSION 3.22)

project(Clonada VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch JUCE
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Fetch cppzmq (header-only ZeroMQ C++ bindings)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ZMQ libzmq)
endif()

if(NOT ZMQ_FOUND)
    # Fallback: build libzmq from source
    FetchContent_Declare(
        libzmq
        GIT_REPOSITORY https://github.com/zeromq/libzmq.git
        GIT_TAG v4.3.5
        GIT_SHALLOW TRUE
    )
    set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WITH_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(libzmq)
    set(ZMQ_LIBRARIES libzmq-static)
    set(ZMQ_INCLUDE_DIRS ${libzmq_SOURCE_DIR}/include ${libzmq_BINARY_DIR})
endif()

FetchContent_Declare(
    cppzmq
    GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
    GIT_TAG v4.10.0
    GIT_SHALLOW TRUE
)
set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cppzmq)

juce_add_plugin(Clonada
    COMPANY_NAME "mediaXtreme"
    BUNDLE_ID "com.mediaxtreme.clonada"
    PLUGIN_MANUFACTURER_CODE Mxtm
    PLUGIN_CODE Clon
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Clonada"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Fx" "Tools"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
)

target_sources(Clonada PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/RingBuffer.h
    Source/ZmqBridge.cpp
    Source/Parameters.h
)

target_compile_definitions(Clonada PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_include_directories(Clonada PRIVATE
    ${ZMQ_INCLUDE_DIRS}
    ${cppzmq_SOURCE_DIR}
)

target_link_libraries(Clonada
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        ${ZMQ_LIBRARIES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
