cmake_minimum_required(VERSION 3.21)

project(Cullami VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DPF_LIBRARIES OFF CACHE BOOL "" FORCE)
set(DPF_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(dpf)

set(CULLAMI_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(CULLAMI_WAV_HEADER "${CULLAMI_GENERATED_DIR}/CullamiWavData.hpp")

add_custom_command(
    OUTPUT "${CULLAMI_WAV_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CULLAMI_GENERATED_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -DINPUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        -DOUTPUT="${CULLAMI_WAV_HEADER}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedWavs.cmake"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/1.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/2.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/3.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/4.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/5 cuffia.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedWavs.cmake")

add_custom_target(cullami-wav-data DEPENDS "${CULLAMI_WAV_HEADER}")

dpf_add_plugin(Cullami
    TARGETS vst3 clap
    UI_TYPE opengl
    FILES_DSP src/CullamiPlugin.cpp
    FILES_UI src/CullamiUI.cpp)

add_dependencies(Cullami-dsp cullami-wav-data)
target_include_directories(Cullami PUBLIC src "${CULLAMI_GENERATED_DIR}")
