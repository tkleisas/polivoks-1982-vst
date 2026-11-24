cmake_minimum_required(VERSION 3.22)

project(Polivoks1982VST3 VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# A local JUCE checkout can be supplied with -DJUCE_DIR=...; otherwise the
# pinned release is fetched once by CMake.
set(JUCE_DIR "" CACHE PATH "Optional local JUCE source directory")

if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(POLIVOKS_MPC "${CMAKE_CURRENT_SOURCE_DIR}/../plugin/Polivoks 1982")
set(POLIVOKS_SKINS "${POLIVOKS_MPC}/skins")

if(NOT EXISTS "${POLIVOKS_MPC}/src/PolivoksDsp.h")
    message(FATAL_ERROR "PolivoksDsp.h not found at ${POLIVOKS_MPC}/src")
endif()

juce_add_plugin(Polivoks1982
    COMPANY_NAME "bokontep"
    PRODUCT_NAME "Polivoks 1982"
    PLUGIN_MANUFACTURER_CODE Boko
    PLUGIN_CODE Plv3
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Instrument|Synth"
    BUNDLE_ID "com.bokontep.polivoks1982"
)

juce_generate_juce_header(Polivoks1982)

juce_add_binary_data(PolivoksAssets
    SOURCES
        "${POLIVOKS_SKINS}/bg_vco.png"
        "${POLIVOKS_SKINS}/bg_filter.png"
        "${POLIVOKS_SKINS}/bg_amp.png"
        "${POLIVOKS_SKINS}/bg_lfo.png"
        "${POLIVOKS_SKINS}/knob_uni.png"
        "${POLIVOKS_SKINS}/knob_bi.png"
        "${POLIVOKS_SKINS}/toggle_on.png"
        "${POLIVOKS_SKINS}/toggle_off.png"
        "${POLIVOKS_MPC}/ui.json"
)

target_sources(Polivoks1982 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(Polivoks1982 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_include_directories(Polivoks1982 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${POLIVOKS_MPC}/src"
)

target_link_libraries(Polivoks1982 PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    PolivoksAssets
)
