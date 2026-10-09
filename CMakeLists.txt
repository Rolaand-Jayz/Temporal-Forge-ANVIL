# CMakeLists.txt — Temporal Forge Player
# spec 01: C++23, CMake + Ninja, Vulkan, FFmpeg, Qt6/QML, Linux first.
cmake_minimum_required(VERSION 3.24)

project(temporal_forge_player
    VERSION 0.1.0
    LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
# NOTE: TFORGE_SOURCE_ROOT is intentionally NOT defined globally. Baking the
# build-host source path into shipped binaries made runtime FSR4 asset
# resolution host-dependent. Test targets that need the source root define
# it per-target in tests/CMakeLists.txt; runtime resolution is
# host-independent (see src/util/Fsr4Paths.hpp).
# Likewise strip the build-host source prefix from __FILE__ diagnostics so no
# absolute build-machine path is baked into shipped binaries.
add_compile_options("-ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Qt + RHI prefer Ninja for faster, more deterministic builds.
if(NINJA_EXE AND NOT EXISTS "${NINJA_EXE}")
    unset(NINJA_EXE CACHE)
endif()
find_program(NINJA_EXE ninja REQUIRED)

# --- dependencies ---
find_package(PkgConfig REQUIRED)

# FFmpeg (spec: libavformat/libavcodec/libavutil/libswscale/libswresample)
# Version floors: the code uses the ch_layout audio API (AVChannelLayout,
# av_channel_layout_copy in src/media/AudioDecoder.cpp/Demuxer.cpp), which
# arrived in FFmpeg 5.1 (libavcodec 59, libavformat 59, libavutil 57,
# libswscale 6, libswresample 4). README promises FFmpeg >= 5.1 behavior;
# these MAJOR floors enforce that at configure time.
pkg_check_modules(FFMPEG REQUIRED
    libavcodec>=59
    libavformat>=59
    libavutil>=57
    libswscale>=6
    libswresample>=4)

# Vulkan
# FindVulkan is the single authoritative Vulkan dependency: it provides the
# Vulkan::Vulkan imported target (loader library + include dirs).  The
# loader's pkg-config file is NOT consulted, so hosts without vulkan.pc
# still configure and link correctly.  Some runtime-only installations ship
# the loader/ICD without development headers; the bundled public headers
# under external/vulkan_include cover that case (including stale NOTFOUND
# cache entries).
# Some runtime-only Vulkan installations provide the loader and ICD but not
# the development headers.  This repository carries the matching public
# headers needed to compile the player, so use them when CMake cannot find a
# system Vulkan include directory (including a stale NOTFOUND cache entry).
set(TFORGE_BUNDLED_VULKAN_INCLUDE
    "${CMAKE_CURRENT_SOURCE_DIR}/external/vulkan_include")
if(EXISTS "${TFORGE_BUNDLED_VULKAN_INCLUDE}/vulkan/vulkan.h")
  set(Vulkan_INCLUDE_DIR "${TFORGE_BUNDLED_VULKAN_INCLUDE}" CACHE PATH
      "Vulkan include directory" FORCE)
endif()
find_package(Vulkan REQUIRED)

# FSR4 GLSL->SPIR-V shader compilation + embedding.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ShaderCompile.cmake)
tforge_compile_shaders(
    OUT_VAR FSR4_SHADER_FILES
    OUT_DIR ${CMAKE_BINARY_DIR}/shaders/fsr4
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/prepass_pq_eotf.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/yuv_to_fsr_input.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/drm_yuv_to_fsr_input.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/codec_motion_expand.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/conv_dw_dot4.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/conv_pw_dot4.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/conv_pw_coop.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/conv_pw_fp16_direct.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/conv_spatial_fp16_direct.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/residual_block_16_int8_coop.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/conv_upscale_fp16_coop.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/conv_upscale_fp16_scalar_final.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/conv_downscale_fp16_coop.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/conv_downscale_fp16_direct.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/conv_fp16_coop.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/residual_add.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/scatter.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/postpass_composite.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/downscale_rgb10.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/easu.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/bicubic_prefilter.comp
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr4/spd.comp)

# Compile the real AMD FSR1 EASU probe into a separate generated directory.
# It is not linked into the active player path yet: the runtime still needs a
# dedicated sampled-image descriptor set and FSR1 constant generation before
# this can be enabled safely. Building it here prevents the probe from
# drifting or being mistaken for the local spatial fallback.
#
# The probe is opt-in because it requires the FidelityFX SDK headers under
# external/FidelityFX-SDK (host-local, gitignored). When the option is OFF,
# a no-op placeholder shader is compiled to the same generated-header
# contract, because src/render/GpuImageUploader.cpp unconditionally embeds
# fsr1_easu.spv.h and creates its shader module during Vulkan init.
option(TFORGE_ENABLE_FSR1_PROBE
    "Compile the FSR1 EASU research probe (requires FidelityFX SDK under external/FidelityFX-SDK)"
    OFF)
if(TFORGE_ENABLE_FSR1_PROBE)
    if(NOT EXISTS
        "${CMAKE_CURRENT_SOURCE_DIR}/external/FidelityFX-SDK/sdk/include/FidelityFX/ffx_core.h")
        message(FATAL_ERROR
            "TFORGE_ENABLE_FSR1_PROBE=ON requires the AMD FidelityFX SDK headers, "
            "but external/FidelityFX-SDK/sdk/include/FidelityFX/ffx_core.h was not found. "
            "Provision the SDK at external/FidelityFX-SDK/ (host-local, gitignored) so that "
            "external/FidelityFX-SDK/sdk/include/FidelityFX/ffx_core.h exists, or reconfigure "
            "with -DTFORGE_ENABLE_FSR1_PROBE=OFF.")
    endif()
    set(TFORGE_FSR1_SHADER_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/fsr1/fsr1_easu.comp")
    add_compile_definitions(TFORGE_FSR1_PROBE_STUB=0)
else()
    set(TFORGE_FSR1_SHADER_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/stubs/fsr1_easu.comp")
    add_compile_definitions(TFORGE_FSR1_PROBE_STUB=1)
endif()
tforge_compile_shaders(
    OUT_VAR FSR1_SHADER_FILES
    OUT_DIR ${CMAKE_BINARY_DIR}/shaders/fsr1
    SOURCES ${TFORGE_FSR1_SHADER_SOURCE})

# Qt6 (spec: Core/Gui/Quick/Qml/Widgets)
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Quick Qml Widgets ShaderTools)

# --- Qt deployment helpers ---
qt_standard_project_setup(REQUIRES 6.6)

# --- compile options (warnings but not pedantic; external/ is exempt) ---
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-missing-field-initializers
)

# --- gather sources (exclude external/ — the FidelityFX SDK has its own CMake) ---
# Split into a static library (everything except main.cpp) so the test suite
# can link the real engine code without dragging in the app's main(). The
# app target links the library + main.cpp + the QML module.
find_package(Threads REQUIRED)
file(GLOB_RECURSE TFORGE_LIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)
list(FILTER TFORGE_LIB_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/external/.*")
list(FILTER TFORGE_LIB_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/main\\.cpp$")
# ANVIL successor isolation: the successor builds as its own FFmpeg-only
# library and must not be absorbed into the FSR-era player library.
list(FILTER TFORGE_LIB_SOURCES EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/src/anvil/.*")

add_library(temporal_forge_lib STATIC ${TFORGE_LIB_SOURCES} ${FSR4_SHADER_FILES}
    ${FSR1_SHADER_FILES})
target_include_directories(temporal_forge_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/external"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/vulkan_include"
    ${FFMPEG_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS})
target_link_libraries(temporal_forge_lib PUBLIC
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::Widgets
    Vulkan::Vulkan
    Threads::Threads ${CMAKE_DL_LIBS})
foreach(lib ${FFMPEG_LIBRARIES})
    target_link_libraries(temporal_forge_lib PUBLIC ${lib})
endforeach()
target_link_directories(temporal_forge_lib PUBLIC ${FFMPEG_LIBRARY_DIRS})
# Shader headers are generated into ${CMAKE_BINARY_DIR}/shaders/fsr4 by the
# custom shader-compile step. The lib owns the FSR4 harness which #includes
# them, so the shader sources must be part of the lib's build graph.
target_include_directories(temporal_forge_lib PUBLIC
    "${CMAKE_BINARY_DIR}/shaders/fsr4")
target_include_directories(temporal_forge_lib PUBLIC
    "${CMAKE_BINARY_DIR}/shaders/fsr1")

# --- main app target ---
qt_add_executable(temporal_forge_player src/main.cpp)
target_link_libraries(temporal_forge_player PRIVATE temporal_forge_lib)

target_include_directories(temporal_forge_player PRIVATE
    "${CMAKE_BINARY_DIR}/shaders/fsr4"
)
# miniaudio (Linux) dlopens the audio backend; keep its libs actually linked.
target_link_options(temporal_forge_player PRIVATE -Wl,--no-as-needed)

# --- QML module (spec: Qt6 + QML) ---
# The app exposes its own QML module "TemporalForge" so main.cpp can
# loadFromModule. UI C++ bridge types will be added here in later phases.
qt_add_qml_module(temporal_forge_player
    URI TemporalForge
    VERSION 1.0
    QML_FILES
        resources/qml/Main.qml
        resources/qml/IconButton.qml
        resources/qml/LiveHud.qml
        resources/qml/SettingsDialog.qml
        resources/qml/SettingsPane.qml
        resources/qml/SettingsSection.qml
        resources/qml/LabeledSlider.qml
        resources/qml/LabeledToggle.qml
        resources/qml/LabeledRadio.qml
        resources/qml/VideoContextMenu.qml
    RESOURCES
        resources/icons/app.svg
)

# --- install / runtime ---
set_target_properties(temporal_forge_player PROPERTIES
    WIN32_EXECUTABLE FALSE
    MACOSX_BUNDLE FALSE
)

# --- ANVIL successor (non-FSR, Qt-free, Vulkan-free, deterministic) ---
# Source identity is captured at BUILD time, not configure time (review
# 4209766186): provenance captured only at configure goes stale as soon as a
# source file changes or a new revision is checked out and ninja recompiles
# without re-running configure, so a rebuilt binary could report a wrong or
# even contradictorily clean identity. cmake/AnvilProvenance.cmake regenerates
# the provenance translation unit below on every build and rewrites the file
# ONLY when the captured content changes, so an unchanged Git state costs zero
# recompilation.
set(ANVIL_PROVENANCE_DIR "${CMAKE_BINARY_DIR}/anvil_provenance")
set(ANVIL_PROVENANCE_SOURCE "${ANVIL_PROVENANCE_DIR}/AnvilBuildProvenance.cpp")
# Configure-time generation guarantees the translation unit exists before any
# compilation; the anvil_provenance target refreshes it from then on.
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DOUT_FILE=${ANVIL_PROVENANCE_SOURCE}"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AnvilProvenance.cmake"
    RESULT_VARIABLE ANVIL_PROVENANCE_RC)
if(NOT ANVIL_PROVENANCE_RC EQUAL 0 OR NOT EXISTS "${ANVIL_PROVENANCE_SOURCE}")
    message(FATAL_ERROR
        "ANVIL: failed to generate build-time provenance source "
        "${ANVIL_PROVENANCE_SOURCE}")
endif()
set_source_files_properties("${ANVIL_PROVENANCE_SOURCE}" PROPERTIES
    GENERATED TRUE)
# Custom targets are phony in Ninja: anvil_provenance runs on every build.
add_custom_target(anvil_provenance
    COMMAND "${CMAKE_COMMAND}"
        "-DOUT_FILE=${ANVIL_PROVENANCE_SOURCE}"
        "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/AnvilProvenance.cmake"
    BYPRODUCTS "${ANVIL_PROVENANCE_SOURCE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "ANVIL: capturing build-time Git provenance"
    VERBATIM)

file(GLOB ANVIL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/anvil/*.cpp")
# Reused historical decode shell: justified independently of FSR (FFmpeg
# correctness, MV side-data extraction, color metadata). Compiled into the
# successor lib so no Qt/Vulkan dependency is inherited.
list(APPEND ANVIL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media/Demuxer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media/VideoDecoder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/util/Log.cpp")
# Build-time provenance definitions are strong and override the weak
# fallbacks compiled from src/anvil/BuildProvenance.cpp (part of the GLOB).
list(APPEND ANVIL_SOURCES "${ANVIL_PROVENANCE_SOURCE}")
add_library(anvil_lib STATIC ${ANVIL_SOURCES})
target_include_directories(anvil_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(anvil_lib PUBLIC cxx_std_23)
target_compile_definitions(anvil_lib PRIVATE
    ANVIL_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    ANVIL_COMPILER_ID="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
add_dependencies(anvil_lib anvil_provenance)
foreach(lib ${FFMPEG_LIBRARIES})
    target_link_libraries(anvil_lib PUBLIC ${lib})
endforeach()
target_link_directories(anvil_lib PUBLIC ${FFMPEG_LIBRARY_DIRS})
target_include_directories(anvil_lib PUBLIC ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(anvil_lib PUBLIC Threads::Threads)

add_executable(anvil_runner tools/anvil/main.cpp)
target_include_directories(anvil_runner PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(anvil_runner PRIVATE cxx_std_23)
target_link_libraries(anvil_runner PRIVATE anvil_lib)

# ANVIL Lab — Home Field Exhibition tooling + Visual Review Lab server.
# Deliberately FFmpeg-free: evidence processing reads PNM artifacts, drives
# the runner as a subprocess, and keeps the review server portable.
add_library(anvil_lab STATIC
    src/anvil_lab/Image.cpp
    src/anvil_lab/PnmIo.cpp
    src/anvil_lab/Png.cpp
    src/anvil_lab/Json.cpp
    src/anvil_lab/Resize.cpp
    src/anvil_lab/Metrics.cpp
    src/anvil_lab/DiffMap.cpp
    src/anvil_lab/Catalog.cpp
    src/anvil_lab/Baseline.cpp
    src/anvil_lab/Scenes.cpp
    src/anvil_lab/Http.cpp
    src/anvil/Sha256.cpp)
target_include_directories(anvil_lab PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/stb")
target_compile_features(anvil_lab PUBLIC cxx_std_23)
target_link_libraries(anvil_lab PUBLIC Threads::Threads)

add_executable(anvil_exhibit tools/anvil_lab/exhibit_main.cpp)
target_compile_features(anvil_exhibit PRIVATE cxx_std_23)
target_link_libraries(anvil_exhibit PRIVATE anvil_lab)

add_executable(anvil_review_lab tools/anvil_lab/server_main.cpp)
target_compile_features(anvil_review_lab PRIVATE cxx_std_23)
target_link_libraries(anvil_review_lab PRIVATE anvil_lab)

# Deterministic fine-timebase fixture generator (libavformat/libavcodec
# only). The FFmpeg CLI tick-fixture route (settb+setpts with -enc_time_base
# 1/10000000) is version-sensitive: it completes instantly on ffmpeg 9 but
# degenerates into a ~10-minute frame explosion on ubuntu-24.04's ffmpeg 6.1
# (measured in CI run 37741621706), so both the C++ and python tick
# regressions take their fixture from this tool.
add_executable(make_tick_fixture tools/anvil/make_tick_fixture.cpp)
target_compile_features(make_tick_fixture PRIVATE cxx_std_23)
foreach(lib ${FFMPEG_LIBRARIES})
    target_link_libraries(make_tick_fixture PRIVATE ${lib})
endforeach()
target_link_directories(make_tick_fixture PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_include_directories(make_tick_fixture PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_compile_definitions(anvil_runner PRIVATE
    ANVIL_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    ANVIL_COMPILER_ID="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# --- tests ---
enable_testing()
add_subdirectory(tests)

message(STATUS "Temporal Forge Player configured:")
message(STATUS "  FFmpeg: ${FFMPEG_VERSION} (libs: ${FFMPEG_LIBRARIES})")
message(STATUS "  Vulkan: ${Vulkan_VERSION}")
message(STATUS "  Qt6:    ${Qt6_VERSION}")
