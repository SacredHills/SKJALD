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
