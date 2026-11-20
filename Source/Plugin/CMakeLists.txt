juce_add_plugin(SKJALD
    COMPANY_NAME "SacredHills"
    COMPANY_WEBSITE "https://github.com/SacredHills/SKJALD"
    BUNDLE_ID "com.sacredhills.skjald"
    PLUGIN_MANUFACTURER_CODE Scrh
    PLUGIN_CODE Skjd
    FORMATS VST3
    PRODUCT_NAME "SKJALD"
    DESCRIPTION "Mastering EQ and Maximizer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Mastering Dynamics EQ
    COPY_PLUGIN_AFTER_BUILD ${SKJALD_COPY_PLUGIN_AFTER_BUILD})

target_sources(SKJALD PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    Parameters.cpp
    ABComparison.cpp
    ../Presets/PresetManager.cpp
    ../Presets/FactoryPresets.cpp
    ../UI/CustomLookAndFeel.cpp
    ../UI/Common/Controls.cpp
    ../UI/MainWindow.cpp
    ../UI/PresetBrowser.cpp
    ../UI/Analyzer/SpectrumAnalyzer.cpp
    ../UI/Equalizer/EQGraph.cpp
    ../UI/Equalizer/EQTab.cpp
    ../UI/Maximizer/MaximizerTab.cpp
    ../UI/MeterComponents/Meters.cpp)

target_include_directories(SKJALD PRIVATE ${CMAKE_SOURCE_DIR}/Source)

target_compile_definitions(SKJALD PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(SKJALD
    PRIVATE
        skjald_dsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
