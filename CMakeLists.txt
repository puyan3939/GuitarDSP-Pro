cmake_minimum_required(VERSION 3.22)
project(GuitarDSPPro VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GUITARDSP_JUCE_PATH "" CACHE PATH "Path to JUCE checkout")

if (GUITARDSP_JUCE_PATH)
    message(STATUS "Using JUCE from GUITARDSP_JUCE_PATH=${GUITARDSP_JUCE_PATH}")
    add_subdirectory("${GUITARDSP_JUCE_PATH}" JUCE)
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    message(STATUS "Using JUCE submodule at ${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    add_subdirectory(JUCE)
elseif (EXISTS "$ENV{HOME}/JUCE/CMakeLists.txt")
    message(STATUS "Using JUCE from $ENV{HOME}/JUCE")
    add_subdirectory("$ENV{HOME}/JUCE" JUCE)
else()
    message(FATAL_ERROR "JUCE was not found. Set -DGUITARDSP_JUCE_PATH=/path/to/JUCE, add a JUCE submodule, or clone JUCE to ~/JUCE.")
endif()

juce_add_gui_app(GuitarDSPPro PRODUCT_NAME "GuitarDSP-Pro")
juce_generate_juce_header(GuitarDSPPro)

target_sources(GuitarDSPPro PRIVATE
    src/Main.cpp src/MainComponent.cpp
    engine/AudioEngine.cpp engine/SignalChain.cpp
    dsp/SafetyLimiter.cpp dsp/LevelMeter.cpp
    dsp/amp/AmpStage.cpp dsp/amp/AmpEngine.cpp
    hq_preload/dsp/amp/AmpEngineHQ.cpp
    hq_preload/dsp/HQEffectsRack.cpp
    hq_preload/dsp/cab/CabMicEngineHQ.cpp
    ui/MainView.cpp ui/NavigationBar.cpp
    ui/pages/AmpPage.cpp ui/pages/PedalPage.cpp ui/pages/CabPage.cpp
    ui/pages/EffectsPage.cpp ui/pages/PerformancePage.cpp ui/pages/SettingsPage.cpp
)

target_include_directories(GuitarDSPPro PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(GuitarDSPPro PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GuitarDSPPro,JUCE_PRODUCT_NAME>" JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GuitarDSPPro,JUCE_VERSION>")
target_link_libraries(GuitarDSPPro PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra)

juce_add_console_app(GuitarDSPDSPTests PRODUCT_NAME "GuitarDSP-DSP-Tests")
juce_generate_juce_header(GuitarDSPDSPTests)
target_sources(GuitarDSPDSPTests PRIVATE tests/DSPCharacterSmoke.cpp hq_preload/dsp/amp/AmpEngineHQ.cpp hq_preload/dsp/HQEffectsRack.cpp hq_preload/dsp/cab/CabMicEngineHQ.cpp)
target_include_directories(GuitarDSPDSPTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(GuitarDSPDSPTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(GuitarDSPDSPTests PRIVATE juce::juce_dsp juce::juce_audio_basics)

juce_add_console_app(GuitarDSPRoutingTests PRODUCT_NAME "GuitarDSP-Routing-Tests")
juce_generate_juce_header(GuitarDSPRoutingTests)
target_sources(GuitarDSPRoutingTests PRIVATE tests/ParallelRigSmoke.cpp)
target_include_directories(GuitarDSPRoutingTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(GuitarDSPRoutingTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(GuitarDSPRoutingTests PRIVATE juce::juce_dsp juce::juce_audio_basics)

include(CTest)
if (BUILD_TESTING)
    add_test(NAME hq_dsp_smoke COMMAND GuitarDSPDSPTests)
    add_test(NAME parallel_rig_smoke COMMAND GuitarDSPRoutingTests)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(GuitarDSPPro PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(GuitarDSPDSPTests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(GuitarDSPRoutingTests PRIVATE -Wall -Wextra -Wpedantic)
endif()
