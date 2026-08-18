cmake_minimum_required(VERSION 3.22)
project(GuitarDSPPro VERSION 0.1.0 LANGUAGES CXX)

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
    message(FATAL_ERROR
        "JUCE was not found. Set -DGUITARDSP_JUCE_PATH=/path/to/JUCE, "
        "add a JUCE submodule, or clone JUCE to ~/JUCE.")
endif()

juce_add_gui_app(GuitarDSPPro
    PRODUCT_NAME "GuitarDSP-Pro"
)

target_sources(GuitarDSPPro PRIVATE
    src/Main.cpp
    src/MainComponent.cpp

    engine/AudioEngine.cpp
    engine/SignalChain.cpp

    dsp/SafetyLimiter.cpp
    dsp/LevelMeter.cpp

    dsp/amp/AmpStage.cpp
    dsp/amp/AmpEngine.cpp

    ui/MainView.cpp
    ui/NavigationBar.cpp
    ui/pages/AmpPage.cpp
    ui/pages/PedalPage.cpp
    ui/pages/CabPage.cpp
    ui/pages/EffectsPage.cpp
    ui/pages/SettingsPage.cpp
)

target_include_directories(GuitarDSPPro PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(GuitarDSPPro PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GuitarDSPPro,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GuitarDSPPro,JUCE_VERSION>"
)

target_link_libraries(GuitarDSPPro PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(GuitarDSPPro PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
