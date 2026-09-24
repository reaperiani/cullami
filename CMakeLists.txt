cmake_minimum_required(VERSION 3.21)

project(Cullami VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(DPF_LIBRARIES OFF CACHE BOOL "" FORCE)
set(DPF_EXAMPLES OFF CACHE BOOL "" FORCE)

# DPF does not validate its Khronos downloads, so fetch pinned copies before
# configuring DPF to avoid silently compiling an HTTP error response.
if(MSVC)
    set(KHRONOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dpf/khronos")
    file(MAKE_DIRECTORY "${KHRONOS_DIR}/GL" "${KHRONOS_DIR}/KHR")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/e8f7cd0e35ac8d6f5667a021ff83d04b1fec41ef/api/GL/glext.h"
        "${KHRONOS_DIR}/GL/glext.h"
        EXPECTED_HASH SHA256=d7b6f15b1bb209e08bd6ada36983f8bb40429e66c24083aa2474d55205d97047
        TLS_VERIFY ON)
    file(DOWNLOAD
        "https://raw.githubusercontent.com/KhronosGroup/EGL-Registry/57b4876de0f33677ece92dd9de0ef105ce69139d/api/KHR/khrplatform.h"
        "${KHRONOS_DIR}/KHR/khrplatform.h"
        EXPECTED_HASH SHA256=7b1e01aaa7ad8f6fc34b5c7bdf79ebf5189bb09e2c4d2e79fc5d350623d11e83
        TLS_VERIFY ON)
endif()

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
