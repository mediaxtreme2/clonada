cmake_minimum_required(VERSION 3.22)

project(Clonada VERSION 1.7.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch JUCE
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# Fetch clap-juce-extensions for CLAP format support
FetchContent_Declare(
    clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(clap-juce-extensions)

# ALWAYS build ZeroMQ from source as a STATIC library
# Never use system/Homebrew ZeroMQ -- it links dynamically and breaks distribution
message(STATUS "Building ZeroMQ from source (static)")
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ENABLE_DRAFTS OFF CACHE BOOL "" FORCE)
set(WITH_TLS OFF CACHE BOOL "" FORCE)
set(WITH_PERF_TOOL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    libzmq
    GIT_REPOSITORY https://github.com/zeromq/libzmq.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(libzmq)
set(ZMQ_LIBRARIES libzmq-static)
set(ZMQ_INCLUDE_DIRS ${libzmq_SOURCE_DIR}/include ${libzmq_BINARY_DIR})

juce_add_plugin(Clonada
    COMPANY_NAME "mediaXtreme"
    BUNDLE_ID "com.mediaxtreme.clonada"
    PLUGIN_MANUFACTURER_CODE Mxtm
    PLUGIN_CODE Clon
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Clonada"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/../installer/clonada-icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/../installer/clonada-icon.png"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx" "Tools"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
)

# Enable CLAP format via clap-juce-extensions
clap_juce_extensions_plugin(
    TARGET Clonada
    CLAP_ID "com.mediaxtreme.clonada"
    CLAP_FEATURES "audio-effect" "voice" "pitch-shift"
    CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 64
)

target_sources(Clonada PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/RingBuffer.h
    Source/ZmqBridge.cpp
    Source/Parameters.h
    Source/LicenseClient.cpp
    Source/LicensePanel.cpp
    Source/EngineLauncher.cpp
    Source/ClonadaLookAndFeel.cpp
    Source/WaveformDisplay.cpp
    Source/PresetManager.cpp
    Source/ModelBrowser.cpp
)

target_compile_definitions(Clonada PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_include_directories(Clonada PRIVATE
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(Clonada
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_cryptography
        ${ZMQ_LIBRARIES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
