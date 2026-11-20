# Development tool (not part of the release): renders the editor to PNG without a DAW.
juce_add_console_app(skjald_ui_preview PRODUCT_NAME "skjald_ui_preview")
target_sources(skjald_ui_preview PRIVATE
    UIPreview.cpp
    ${CMAKE_SOURCE_DIR}/Source/Plugin/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/Source/Plugin/PluginEditor.cpp
    ${CMAKE_SOURCE_DIR}/Source/Plugin/Parameters.cpp
    ${CMAKE_SOURCE_DIR}/Source/Plugin/ABComparison.cpp
    ${CMAKE_SOURCE_DIR}/Source/Presets/PresetManager.cpp
    ${CMAKE_SOURCE_DIR}/Source/Presets/FactoryPresets.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/CustomLookAndFeel.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Common/Controls.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/MainWindow.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/PresetBrowser.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Analyzer/SpectrumAnalyzer.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Equalizer/EQGraph.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Equalizer/EQTab.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/Maximizer/MaximizerTab.cpp
    ${CMAKE_SOURCE_DIR}/Source/UI/MeterComponents/Meters.cpp)
target_include_directories(skjald_ui_preview PRIVATE ${CMAKE_SOURCE_DIR}/Source)
target_compile_definitions(skjald_ui_preview PRIVATE
    JucePlugin_Name="SKJALD" JucePlugin_VersionString="${PROJECT_VERSION}"
    JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(skjald_ui_preview PRIVATE skjald_dsp
    juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
