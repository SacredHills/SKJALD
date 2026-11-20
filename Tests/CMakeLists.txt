add_executable(skjald_dsp_tests
    TestMain.cpp
    DSP/EQTests.cpp
    DSP/LimiterTests.cpp
    DSP/MaximizerFeatureTests.cpp
    DSP/StereoTests.cpp
    DSP/LoudnessTests.cpp
)
target_link_libraries(skjald_dsp_tests PRIVATE skjald_dsp)
target_include_directories(skjald_dsp_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(skjald_dsp_tests PRIVATE /bigobj)
endif()

add_test(NAME skjald_dsp_tests COMMAND skjald_dsp_tests)

# Plug-in level tests (host the built VST3). Requires the plugin target.
if(TARGET SKJALD_VST3)
    juce_add_console_app(skjald_plugin_tests PRODUCT_NAME "skjald_plugin_tests")
    target_sources(skjald_plugin_tests PRIVATE Plugin/PluginHostTests.cpp)
    target_compile_definitions(skjald_plugin_tests PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(skjald_plugin_tests PRIVATE
        juce::juce_audio_processors juce::juce_gui_basics
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_dependencies(skjald_plugin_tests SKJALD_VST3)
    # bundle root (JUCE's LIBRARY_OUTPUT_DIRECTORY points inside the bundle on Linux)
    add_test(NAME skjald_plugin_tests
             COMMAND skjald_plugin_tests "${CMAKE_BINARY_DIR}/Source/Plugin/SKJALD_artefacts/$<CONFIG>/VST3/SKJALD.vst3")
endif()
