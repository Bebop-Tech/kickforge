cmake_minimum_required(VERSION 3.22)

project(KickForge VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(cmake/CPM.cmake)
include(cmake/CompilerWarnings.cmake)

CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.14
)

# Cœur DSP en C++ pur (sans dépendance JUCE) : partagé entre le plugin et les tests.
add_library(kickforge_dsp STATIC
    src/dsp/KickVoice.cpp
    src/dsp/AttackLayer.cpp
)
target_include_directories(kickforge_dsp PUBLIC src)
target_compile_features(kickforge_dsp PUBLIC cxx_std_20)
target_link_libraries(kickforge_dsp PRIVATE kickforge_warnings)

juce_add_plugin(KickForge
    COMPANY_NAME "Bebop Tech"
    BUNDLE_ID "com.beboptech.kickforge"
    PLUGIN_MANUFACTURER_CODE Bbop
    PLUGIN_CODE Kfrg
    FORMATS VST3 Standalone
    PRODUCT_NAME "KickForge"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument
)

# Étages DSP qui dépendent de juce::dsp : compilés dans le plugin ET dans les tests.
set(KICKFORGE_JUCE_DSP_SOURCES
    src/dsp/KickEngine.cpp
    src/dsp/CrunchLayer.cpp
    src/dsp/DriveStage.cpp
    src/dsp/FilterStage.cpp
    src/dsp/EqStage.cpp
    src/dsp/DistStage.cpp
    src/dsp/FxStage.cpp
    src/dsp/CompStage.cpp
    src/dsp/ClipStage.cpp
    src/dsp/WidthStage.cpp
    src/export/WavExporter.cpp
)

target_sources(KickForge PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ui/LookAndFeel.cpp
    src/ui/SectionPanel.cpp
    src/ui/PresetBar.cpp
    src/ui/WaveformDisplay.cpp
    src/ui/EqCurveDisplay.cpp
    ${KICKFORGE_JUCE_DSP_SOURCES}
)

target_include_directories(KickForge PRIVATE src)

target_compile_definitions(KickForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(KickForge
    PRIVATE
        kickforge_dsp
        juce::juce_audio_utils
        juce::juce_dsp
        kickforge_warnings
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

option(KICKFORGE_BUILD_TESTS "Build KickForge unit tests" ON)

if(KICKFORGE_BUILD_TESTS)
    enable_testing()

    CPMAddPackage(
        NAME Catch2
        GITHUB_REPOSITORY catchorg/Catch2
        GIT_TAG v3.15.2
    )

    # Console app JUCE : permet de tester les étages juce::dsp hors plugin.
    juce_add_console_app(dsp_tests PRODUCT_NAME "dsp_tests")
    target_sources(dsp_tests PRIVATE
        tests/dsp_tests.cpp
        ${KICKFORGE_JUCE_DSP_SOURCES}
    )
    target_include_directories(dsp_tests PRIVATE src)
    target_compile_definitions(dsp_tests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(dsp_tests PRIVATE
        kickforge_dsp
        juce::juce_dsp
        juce::juce_audio_formats
        Catch2::Catch2WithMain
        juce::juce_recommended_config_flags
    )

    add_test(NAME dsp_tests COMMAND dsp_tests)
endif()
