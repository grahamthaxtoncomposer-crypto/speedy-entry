cmake_minimum_required(VERSION 3.22)

project(StaffPaper VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# The name shown in Bitwig and on the window. Change it here only.
set(PLUGIN_DISPLAY_NAME "F--k you, Piano Roll!" CACHE STRING "Plugin name")

option(STAFFPAPER_BUILD_PLUGIN "Build the CLAP / VST3 plugin (downloads JUCE)" ON)
option(STAFFPAPER_BUILD_TESTS "Build unit tests" ON)

# ------------------------------------------------------------------ notation core
add_library(staffpaper_core STATIC
    core/src/Score.cpp
    core/src/Layout.cpp
    core/src/KeyMap.cpp
    core/src/Editor.cpp
    core/src/Playback.cpp
    core/src/Project.cpp
    core/src/MusicXml.cpp)
target_include_directories(staffpaper_core PUBLIC core/include)
if(MSVC)
    target_compile_options(staffpaper_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(staffpaper_core PRIVATE -Wall -Wextra)
endif()

if(STAFFPAPER_BUILD_TESTS)
    enable_testing()
    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE staffpaper_core)
    add_test(NAME core_tests COMMAND core_tests)
endif()

# ------------------------------------------------------------------ plugin
if(STAFFPAPER_BUILD_PLUGIN)
    include(FetchContent)
    # Pass -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE to use a local copy.
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_Declare(clap_juce_extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG 3837f4a6ed2ab0ba46aba69ae5b7041c7c64ce74
        GIT_SUBMODULES_RECURSE ON)
    FetchContent_MakeAvailable(JUCE clap_juce_extensions)

    juce_add_binary_data(staffpaper_fonts
        HEADER_NAME StaffPaperFonts.h
        NAMESPACE StaffPaperFonts
        SOURCES resources/fonts/Bravura.otf)

    juce_add_plugin(StaffPaper
        PRODUCT_NAME "${PLUGIN_DISPLAY_NAME}"
        COMPANY_NAME "Graham Thaxton"
        BUNDLE_ID com.grahamthaxton.staffpaper
        PLUGIN_MANUFACTURER_CODE Gthx
        PLUGIN_CODE StPp
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx Tools
        MICROPHONE_PERMISSION_ENABLED FALSE)

    target_sources(StaffPaper PRIVATE
        plugin/PluginProcessor.cpp
        plugin/PluginEditor.cpp
        plugin/StaffView.cpp
        plugin/ClipView.cpp
        plugin/HouseLook.cpp
        plugin/Glyphs.cpp)
    target_include_directories(StaffPaper PRIVATE plugin)
    target_compile_definitions(StaffPaper PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        PLUGIN_DISPLAY_NAME="${PLUGIN_DISPLAY_NAME}")
    target_link_libraries(StaffPaper
        PRIVATE
            staffpaper_core
            staffpaper_fonts
            juce::juce_audio_utils
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    clap_juce_extensions_plugin(TARGET StaffPaper
        CLAP_ID "com.grahamthaxton.staffpaper"
        CLAP_FEATURES note-effect utility
        CLAP_PROCESS_EVENTS_RESOLUTION 0
        CLAP_USE_JUCE_PARAMETER_RANGES DISCRETE)

    # Offscreen render check used in CI: draws a sample score to a PNG.
    juce_add_console_app(StaffPaperRenderCheck PRODUCT_NAME "StaffPaperRenderCheck")
    target_sources(StaffPaperRenderCheck PRIVATE tools/RenderCheck.cpp plugin/StaffView.cpp plugin/Glyphs.cpp)
    target_include_directories(StaffPaperRenderCheck PRIVATE plugin)
    target_compile_definitions(StaffPaperRenderCheck PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 STAFFPAPER_NO_PROCESSOR=1 PLUGIN_DISPLAY_NAME="${PLUGIN_DISPLAY_NAME}")
    target_link_libraries(StaffPaperRenderCheck PRIVATE staffpaper_core staffpaper_fonts
        juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
