cmake_minimum_required(VERSION 3.22)

project(Clonada VERSION 1.6.3 LANGUAGES C CXX)

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

# Find ZeroMQ - prefer static to avoid DLL dependency issues in plugins
find_package(ZeroMQ CONFIG QUIET)
if(ZeroMQ_FOUND OR TARGET libzmq-static OR TARGET libzmq)
    message(STATUS "Found ZeroMQ via find_package")
    if(TARGET libzmq-static)
        set(ZMQ_LIBRARIES libzmq-static)
    elseif(TARGET libzmq)
        set(ZMQ_LIBRARIES libzmq)
    else()
        set(ZMQ_LIBRARIES ${ZeroMQ_LIBRARIES})
    endif()
    set(ZMQ_INCLUDE_DIRS ${ZeroMQ_INCLUDE_DIRS})
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ZMQ IMPORTED_TARGET libzmq)
    endif()

    if(ZMQ_FOUND)
        message(STATUS "Found ZeroMQ via pkg-config")
        set(ZMQ_LIBRARIES PkgConfig::ZMQ)
    else()
        message(STATUS "Building ZeroMQ from source")
        set(ZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(WITH_DOCS OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
        set(BUILD_STATIC ON CACHE BOOL "" FORCE)
        set(ENABLE_DRAFTS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            libzmq
            GIT_REPOSITORY https://github.com/zeromq/libzmq.git
            GIT_TAG v4.3.6
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(libzmq)
        set(ZMQ_LIBRARIES libzmq-static)
        set(ZMQ_INCLUDE_DIRS ${libzmq_SOURCE_DIR}/include ${libzmq_BINARY_DIR})
    endif()
endif()

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
