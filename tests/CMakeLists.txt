# tests/CMakeLists.txt — declare the focused contract tests for the player.
# Upstream: the real src library and small test fixtures. Downstream: ctest
# executables that validate math, configuration, media lifetime, backend
# contracts, and optional GPU diagnostics without changing runtime code.
#
# tests/CMakeLists.txt
# Headless unit tests. Run with: ctest --output-on-failure
#
# Tests link against the same util/ headers used by the app; for tests that
# only need headers (FsrTargetMath, Jitter, UpscaleTypes) we compile each
# test as a standalone executable with just the required sources.

function(tforge_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/external/vulkan_include")
    target_compile_features(${name} PRIVATE cxx_std_23)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

tforge_add_test(preset_math_tests
    SOURCES preset_math_tests.cpp
            ../src/backend/UpscaleTypes.cpp)

tforge_add_test(quality_lab_config_tests
    SOURCES quality_lab_config_tests.cpp
            ../src/config/QualityLabConfig.cpp
            ../src/util/Fsr4Paths.cpp)
target_link_libraries(quality_lab_config_tests PRIVATE Qt6::Core)
target_compile_definitions(quality_lab_config_tests PRIVATE
    TFORGE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# Host-independent FSR4 asset-resolution contract (native packs + weight
# blobs). Hermetic: env overrides + temp dirs, no machine paths.
tforge_add_test(fsr4_paths_contract_tests
    SOURCES fsr4_paths_contract_tests.cpp
            ../src/util/Fsr4Paths.cpp)

tforge_add_test(playlist_eos_tests
    SOURCES playlist_eos_tests.cpp
            ../src/core/PlaylistAdvancement.cpp)

tforge_add_test(jitter_tests
    SOURCES jitter_tests.cpp)

# Synthetic-jitter sampling/reporting boundary test. Native passthrough must
# remain spatially unchanged because it has no matching FSR jitter metadata.
tforge_add_test(synthetic_jitter_contract_audit_tests
    SOURCES synthetic_jitter_contract_audit_tests.cpp)
target_compile_definitions(synthetic_jitter_contract_audit_tests PRIVATE
    TFORGE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
set_tests_properties(synthetic_jitter_contract_audit_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

tforge_add_test(frame_identity_tests
    SOURCES frame_identity_tests.cpp
            ../src/backend/UpscaleTypes.cpp)
target_compile_definitions(frame_identity_tests PRIVATE
    TFORGE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

tforge_add_test(temporal_frame_continuity_tests
    SOURCES temporal_frame_continuity_tests.cpp)

tforge_add_test(backend_tests
    SOURCES backend_tests.cpp
            ../src/backend/UpscaleTypes.cpp
            ../src/backend/NullBackend.cpp
            ../src/backend/SpatialFallbackBackend.cpp
            ../src/backend/Fsr23SdkBackend.cpp
            ../src/backend/Fsr4Int8Backend.cpp
            ../src/backend/Fsr3FallbackBackend.cpp
            ../src/backend/BackendSelector.cpp
            ../src/backend/Fsr4PostpassParams.cpp
            ../src/render/Fsr4DispatchHarness.cpp
            ../src/util/Fsr4Paths.cpp
            ../src/render/fsr4/Fsr4ConvSteps.cpp
            ../src/render/fsr4/Fsr4Memory.cpp
            ../src/util/Log.cpp)
target_include_directories(backend_tests PRIVATE "${CMAKE_BINARY_DIR}/shaders/fsr4")
target_link_libraries(backend_tests PRIVATE Vulkan::Vulkan)
add_dependencies(backend_tests temporal_forge_player)

tforge_add_test(sidebuffer_tests
    SOURCES sidebuffer_tests.cpp
            ../src/render/SideBufferSynth.cpp)

# Cheap codec-seeded motion estimator tests. The estimator is intentionally
# tested independently of Vulkan/FSR so direction, bounded search, confidence,
# and scene-cut fail-closed behavior remain diagnosable.
tforge_add_test(motion_estimator_tests
    SOURCES motion_estimator_tests.cpp
            ../src/motion/MotionEstimator.cpp)

# FSR4 RE weight-blob loader test (no GPU needed; validates the RE dataset).
tforge_add_test(fsr4_weight_tests
    SOURCES fsr4_weight_tests.cpp
            ../src/backend/WeightBlob.cpp
            ../src/util/Log.cpp)
set_tests_properties(fsr4_weight_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    SKIP_RETURN_CODE 77)

# M1 postpass contract test. This stays CPU-only: it validates the recovered
# byte region, host decode rules, and source-level shader consumption evidence.
tforge_add_test(fsr4_postpass_contract_tests
    SOURCES fsr4_postpass_contract_tests.cpp
            ../src/backend/Fsr4PostpassParams.cpp)
set_tests_properties(fsr4_postpass_contract_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# M2 temporal resource-graph contract test. Runtime GPU validation is a later
# part of the milestone because the existing live harness test is disabled.
tforge_add_test(fsr4_temporal_contract_tests
    SOURCES fsr4_temporal_contract_tests.cpp)
set_tests_properties(fsr4_temporal_contract_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# FSR4 residual/skip output contract. The recovered model applies an FP8
# CopySat boundary after each residual addition; this CPU-only test keeps the
# Vulkan shader implementation aligned with that source evidence.
tforge_add_test(fsr4_residual_quantization_contract_tests
    SOURCES fsr4_residual_quantization_contract_tests.cpp)
target_compile_definitions(fsr4_residual_quantization_contract_tests PRIVATE
    TFORGE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
set_tests_properties(fsr4_residual_quantization_contract_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# M3 causal codec-motion boundary test.
tforge_add_test(fsr4_motion_contract_tests
    SOURCES fsr4_motion_contract_tests.cpp)
set_tests_properties(fsr4_motion_contract_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# M4 decode-to-upload color contract test. It runs before implementation work
# is accepted and keeps metadata, chroma phase, and high-bit-depth handling
# from regressing behind a passing shader compile.
tforge_add_test(color_metadata_contract_tests
    SOURCES color_metadata_contract_tests.cpp
            ../src/render/upload/YuvConstants.cpp)
set_tests_properties(color_metadata_contract_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# M5 decoded-video jitter policy test. This runs the public side-buffer policy
# boundary without starting the disabled live GPU harness.
tforge_add_test(jitter_policy_tests
    SOURCES jitter_policy_tests.cpp
            ../src/render/SideBufferSynth.cpp
            ../src/util/Log.cpp)
set_tests_properties(jitter_policy_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# FSR4 tensor-map parser test (no GPU needed).
tforge_add_test(fsr4_tensormap_tests
    SOURCES fsr4_tensormap_tests.cpp
            ../src/backend/Fsr4TensorMap.cpp
            ../src/util/Log.cpp)
set_tests_properties(fsr4_tensormap_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    SKIP_RETURN_CODE 77)

# Live Vulkan capability handoff contract. The test is source-level because
# constructing PlaybackEngine requires the full Qt/media runtime; it still
# catches the failure mode where startup loses FP16/INT8 capability flags.
tforge_add_test(gpu_capability_handoff_contract_tests
    SOURCES gpu_capability_handoff_contract_tests.cpp)
target_compile_definitions(gpu_capability_handoff_contract_tests PRIVATE
    TFORGE_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
set_tests_properties(gpu_capability_handoff_contract_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Standalone GPU capability probe (not a ctest — run manually; needs a GPU).
tforge_add_test(gpu_probe
    SOURCES gpu_probe.cpp
            ../src/backend/GpuCapabilityProbe.cpp
            ../src/util/Log.cpp)
target_link_libraries(gpu_probe PRIVATE Vulkan::Vulkan)
# Not run as part of ctest (it's an interactive diagnostic).
set_tests_properties(gpu_probe PROPERTIES DISABLED ON)

# Cooperative-matrix mode dumper (diagnostic).
tforge_add_test(cm_dump
    SOURCES cm_dump.cpp)
target_link_libraries(cm_dump PRIVATE Vulkan::Vulkan)
set_tests_properties(cm_dump PROPERTIES DISABLED ON)

# FSR4 dispatch harness + proof runner integration test (P-B/P-E). Live AMD GPU.
tforge_add_test(fsr4_harness_tests
    SOURCES fsr4_harness_tests.cpp
            ../src/backend/GpuCapabilityProbe.cpp
            ../src/backend/Fsr4PostpassParams.cpp
            ../src/backend/WeightBlob.cpp
            ../src/backend/Fsr4ProofRunner.cpp
            ../src/render/Fsr4DispatchHarness.cpp
            ../src/util/Fsr4Paths.cpp
            ../src/render/fsr4/Fsr4ConvSteps.cpp
            ../src/render/fsr4/Fsr4Memory.cpp
            ../src/util/Log.cpp)
target_link_libraries(fsr4_harness_tests PRIVATE Vulkan::Vulkan)
target_include_directories(fsr4_harness_tests PRIVATE "${CMAKE_BINARY_DIR}/shaders/fsr4")
# Ensure the embedded SPIR-V header is generated before compiling.
add_dependencies(fsr4_harness_tests temporal_forge_player)
set_tests_properties(fsr4_harness_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DISABLED ON)

# Real YUV-upload jitter contract probe. It is kept out of normal CTest because
# it needs a live Vulkan device and generated shader assets; run manually on a
# capable GPU to verify that fixed positive jitter produces a directional
# change in the actual RGB10/A2 model-color image.
tforge_add_test(jitter_gpu_contract_tests
    SOURCES jitter_gpu_contract_tests.cpp)
target_link_libraries(jitter_gpu_contract_tests PRIVATE temporal_forge_lib)
add_dependencies(jitter_gpu_contract_tests temporal_forge_player)
set_tests_properties(jitter_gpu_contract_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DISABLED ON)

# --- integration test (needs FFmpeg) ---
find_program(FFMPEG_BIN ffmpeg)
# Generate a deterministic sample clip as a test fixture.
if(FFMPEG_BIN)
    set(SAMPLE_MP4 "${CMAKE_CURRENT_BINARY_DIR}/sample.mp4")
    add_custom_command(
        OUTPUT ${SAMPLE_MP4}
        COMMAND ${FFMPEG_BIN} -y -loglevel error
            -f lavfi -i "testsrc2=size=640x360:rate=30:duration=5"
            -f lavfi -i "sine=frequency=440:sample_rate=48000:duration=5"
            -c:v libx264 -pix_fmt yuv420p -color_range tv -colorspace bt709
            -c:a aac ${SAMPLE_MP4}
        DEPENDS ""
        COMMENT "Generating sample.mp4 test fixture"
        VERBATIM)
    add_custom_target(tforge_sample_mp4 DEPENDS ${SAMPLE_MP4})
endif()

# media_pipeline_tests exercises the real demux+decode path.
pkg_check_modules(FFMPEG_TEST IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample)
if(FFMPEG_TEST_FOUND)
    tforge_add_test(media_pipeline_tests
        SOURCES media_pipeline_tests.cpp
                ../src/media/Demuxer.cpp
                ../src/media/VideoDecoder.cpp
                ../src/media/AudioDecoder.cpp
                ../src/util/Log.cpp)
    target_link_libraries(media_pipeline_tests PRIVATE PkgConfig::FFMPEG_TEST)
    if(TARGET tforge_sample_mp4)
        add_dependencies(media_pipeline_tests tforge_sample_mp4)
    endif()
    # Run from the build/tests dir where sample.mp4 is generated; the test
    # source returns 77 when the fixture is unavailable (deterministic skip).
    set_tests_properties(media_pipeline_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "TFORGE_SAMPLE=${CMAKE_CURRENT_BINARY_DIR}/sample.mp4"
        SKIP_RETURN_CODE 77)
endif()

# file_switch_tests exercises the PlaybackEngine's openUrl/close cycle to
# verify that switching files mid-playback no longer deadlocks. It links
# against temporal_forge_lib so it reuses the real engine code without
# pulling in main.cpp or the QML module.
if(TARGET temporal_forge_lib AND TARGET tforge_sample_mp4)
    add_executable(file_switch_tests file_switch_tests.cpp)
    target_link_libraries(file_switch_tests PRIVATE temporal_forge_lib)
    target_compile_features(file_switch_tests PRIVATE cxx_std_23)
    add_dependencies(file_switch_tests temporal_forge_player tforge_sample_mp4)
    add_test(NAME file_switch_tests COMMAND file_switch_tests
        "${CMAKE_CURRENT_BINARY_DIR}/sample.mp4"
        "${CMAKE_CURRENT_BINARY_DIR}/sample.mp4")
    # The test source returns 77 when the fixture is missing (deterministic
    # skip instead of a hard failure on machines without ffmpeg).
    set_tests_properties(file_switch_tests PROPERTIES SKIP_RETURN_CODE 77)
endif()

# M-05 device-request planner contract test. Hermetic: pure functions over
# fabricated extension/feature availability; no GPU or Vulkan calls.
tforge_add_test(vulkan_capability_plan_tests
    SOURCES vulkan_capability_plan_tests.cpp
            ../src/render/VulkanContext.cpp
            ../src/util/Log.cpp)
target_link_libraries(vulkan_capability_plan_tests PRIVATE Vulkan::Vulkan)

# --- ANVIL successor tests (Qt-free, FFmpeg-only) ---
pkg_check_modules(FFMPEG_ANVIL IMPORTED_TARGET libavformat libavcodec libavutil libswscale)

function(anvil_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_compile_features(${name} PRIVATE cxx_std_23)
    target_compile_definitions(${name} PRIVATE
        ANVIL_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
        ANVIL_COMPILER_ID="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    if(FFMPEG_ANVIL_FOUND)
        target_link_libraries(${name} PRIVATE PkgConfig::FFMPEG_ANVIL)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

anvil_add_test(anvil_core_tests
    SOURCES anvil_core_tests.cpp
            ../src/anvil/Core.cpp
            ../src/anvil/JsonWriter.cpp
            ../src/anvil/ColorMeta.cpp
            ../src/anvil/Manifest.cpp
            ../src/anvil/BuildProvenance.cpp
            ../src/anvil/Oracle.cpp
            ../src/anvil/Pnm.cpp
            ../src/anvil/Reconstruct.cpp
            ../src/anvil/Sha256.cpp)
# Build-time provenance (review 4209766186): link the GENERATED strong
# definitions too so detectGitSha()/detectGitDirty() are exercised through
# the build_generated tier alongside the weak fallbacks (strong wins at
# link time; the pair must coexist without duplicate-symbol errors).
set(ANVIL_PROVENANCE_SOURCE "${CMAKE_BINARY_DIR}/anvil_provenance/AnvilBuildProvenance.cpp")
target_sources(anvil_core_tests PRIVATE "${ANVIL_PROVENANCE_SOURCE}")
set_source_files_properties("${ANVIL_PROVENANCE_SOURCE}" PROPERTIES GENERATED TRUE)
add_dependencies(anvil_core_tests anvil_provenance)
# git-provenance regression runs inside the checkout so detectGitSha() can
# be compared against `git rev-parse HEAD`.
set_tests_properties(anvil_core_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
if(NOT FFMPEG_ANVIL_FOUND)
    set_tests_properties(anvil_core_tests PROPERTIES DISABLED ON)
endif()

# Coarse/refinement resolution-hierarchy regression (review 4209751753):
# the even-lattice coarse prior plus +/-1 residual refinement must change a
# deliberately imperfect coarse field on odd true displacements and leave
# even/boundary optima unchanged.
anvil_add_test(anvil_refinement_tests
    SOURCES anvil_refinement_tests.cpp
            ../src/anvil/Core.cpp
            ../src/anvil/Reconstruct.cpp)
if(NOT FFMPEG_ANVIL_FOUND)
    set_tests_properties(anvil_refinement_tests PROPERTIES DISABLED ON)
endif()

# Sample-geometry estimation contract: the fixture knows the true sampling
# grid offset, the estimator sees only image planes. Covers the estimated
# arm's accuracy, zero-offset, fail-closed inputs, the flow-application sign
# proof, determinism, and codec-MV independence.
anvil_add_test(anvil_geometry_estimate_tests
    SOURCES anvil_geometry_estimate_tests.cpp
            ../src/anvil/BuildProvenance.cpp
            ../src/anvil/Core.cpp
            ../src/anvil/JsonWriter.cpp
            ../src/anvil/ColorMeta.cpp
            ../src/anvil/Manifest.cpp
            ../src/anvil/Oracle.cpp
            ../src/anvil/Pnm.cpp
            ../src/anvil/Reconstruct.cpp
            ../src/anvil/SampleGeometryEstimate.cpp
            ../src/anvil/Sha256.cpp)
if(NOT FFMPEG_ANVIL_FOUND)
    set_tests_properties(anvil_geometry_estimate_tests PROPERTIES DISABLED ON)
endif()

anvil_add_test(anvil_codec_tests
    SOURCES anvil_codec_tests.cpp
            ../src/anvil/CodecProbe.cpp)
if(NOT FFMPEG_ANVIL_FOUND)
    set_tests_properties(anvil_codec_tests PROPERTIES DISABLED ON)
endif()

# Side-info normalization stage (review 4209763208): the raw -> normalized
# prior boundary must be an explicit, independently bypassable stage (pack
# capability D), not an internal step of the codec correspondence arm.
# Pure CPU; Core.cpp is linked because the stage delegates to normalizeCodecMv.
anvil_add_test(anvil_side_info_tests
    SOURCES anvil_side_info_tests.cpp
            ../src/anvil/Core.cpp
            ../src/anvil/SideInfoNormalize.cpp)
if(NOT FFMPEG_ANVIL_FOUND)
    set_tests_properties(anvil_side_info_tests PROPERTIES DISABLED ON)
endif()

# anvil_output_backend_tests: the replaceable/bypassable output stage boundary
# (execution pack capability D). "pnm" must reproduce the runner's former
# inline serialization byte-for-byte; "null" is the zero-artifact bypass.
anvil_add_test(anvil_output_backend_tests
    SOURCES anvil_output_backend_tests.cpp
            ../src/anvil/OutputBackend.cpp
            ../src/anvil/Pnm.cpp
            ../src/anvil/Reconstruct.cpp)
if(NOT FFMPEG_ANVIL_FOUND)
    set_tests_properties(anvil_output_backend_tests PROPERTIES DISABLED ON)
endif()

# anvil_runner_tests: end-to-end CLI on a synthetic deterministic clip.
if(FFMPEG_ANVIL_FOUND AND FFMPEG_BIN)
    add_executable(anvil_runner_tests anvil_runner_tests.cpp)
    target_include_directories(anvil_runner_tests PRIVATE "${CMAKE_SOURCE_DIR}/src")
    target_compile_features(anvil_runner_tests PRIVATE cxx_std_23)
    target_link_libraries(anvil_runner_tests PRIVATE anvil_lib)
    add_dependencies(anvil_runner_tests anvil_runner)
    set(ANVIL_CLIP "${CMAKE_CURRENT_BINARY_DIR}/anvil_fixture.mp4")
    set(ANVIL_CLIP_HDR "${CMAKE_CURRENT_BINARY_DIR}/anvil_fixture_hdr.mp4")
    set(ANVIL_CLIP_NOMETA "${CMAKE_CURRENT_BINARY_DIR}/anvil_fixture_nometa.mp4")
    add_custom_command(
        OUTPUT ${ANVIL_CLIP}
        COMMAND ${FFMPEG_BIN} -y -loglevel error
            -f lavfi -i "testsrc2=size=64x64:rate=10:duration=1"
            -c:v libx264 -pix_fmt yuv420p
            -x264-params "keyint=10:bframes=0:colorprim=bt709:transfer=bt709:colorrange=tv:colormatrix=bt709"
            ${ANVIL_CLIP}
        DEPENDS ""
        COMMENT "Generating anvil_fixture.mp4 (SDR, full color metadata)"
        VERBATIM)
    add_custom_command(
        OUTPUT ${ANVIL_CLIP_HDR}
        COMMAND ${FFMPEG_BIN} -y -loglevel error
            -f lavfi -i "testsrc2=size=64x64:rate=10:duration=1"
            -c:v libx264 -pix_fmt yuv420p10le
            -x264-params "keyint=10:bframes=0:colorprim=bt2020:transfer=smpte2084:colorrange=tv:colormatrix=bt2020nc"
            ${ANVIL_CLIP_HDR}
        DEPENDS ""
        COMMENT "Generating anvil_fixture_hdr.mp4 (PQ transfer, no conversion)"
        VERBATIM)
    add_custom_command(
        OUTPUT ${ANVIL_CLIP_NOMETA}
        COMMAND ${FFMPEG_BIN} -y -loglevel error
            -f lavfi -i "testsrc2=size=64x64:rate=10:duration=1"
            -c:v libx264 -pix_fmt yuv420p -x264-params "keyint=10:bframes=0"
            ${ANVIL_CLIP_NOMETA}
        DEPENDS ""
        COMMENT "Generating anvil_fixture_nometa.mp4 (unspecified color metadata)"
        VERBATIM)
    add_custom_target(anvil_fixture DEPENDS ${ANVIL_CLIP} ${ANVIL_CLIP_HDR}
        ${ANVIL_CLIP_NOMETA})
    add_dependencies(anvil_runner_tests anvil_fixture)
    add_test(NAME anvil_runner_tests COMMAND anvil_runner_tests
        $<TARGET_FILE:anvil_runner> "${ANVIL_CLIP}" "${ANVIL_CLIP_HDR}"
        "${ANVIL_CLIP_NOMETA}")
    set_tests_properties(anvil_runner_tests PROPERTIES
        SKIP_RETURN_CODE 77)
elseif(NOT FFMPEG_BIN)
    message(STATUS "ANVIL: ffmpeg binary not found; anvil_runner_tests disabled")
endif()

# anvil_timestamp_tests: native-timestamp identity (review 4209783084).
# Unit tests for exact tick selection plus a decode-level regression on a
# fine-timebase fixture (mp4 track timescale 10 MHz) containing two frames
# whose adjacent frames sit 5 ticks (0.5 us) apart and collapse to the same
# microsecond after av_rescale_q. Fixture expectations are derived at runtime
# from ffprobe, never hardcoded.
find_program(FFPROBE_BIN ffprobe)
if(FFMPEG_ANVIL_FOUND AND FFMPEG_BIN)
    set(ANVIL_TICK_CLIP "${CMAKE_CURRENT_BINARY_DIR}/anvil_tick_fixture.mp4")
    # Fixture via make_tick_fixture (libavformat, version-proof): the CLI
    # route with settb/setpts + -enc_time_base 1/10000000 explodes on older
    # ffmpeg (ubuntu-24.04 / 6.1); see the root CMakeLists note.
    add_custom_command(
        OUTPUT ${ANVIL_TICK_CLIP}
        COMMAND $<TARGET_FILE:make_tick_fixture>
                ${ANVIL_TICK_CLIP} 5 10000000 8
        DEPENDS make_tick_fixture
        COMMENT "Generating anvil_tick_fixture.mp4 (10 MHz timebase, 0.5 us tick spacing)"
        VERBATIM)
    add_custom_target(anvil_tick_fixture DEPENDS ${ANVIL_TICK_CLIP})
    anvil_add_test(anvil_timestamp_tests
        SOURCES anvil_timestamp_tests.cpp
                ../src/media/Demuxer.cpp
                ../src/media/VideoDecoder.cpp
                ../src/util/Log.cpp)
    add_dependencies(anvil_timestamp_tests anvil_tick_fixture)
    # The C++ test shells out to ffprobe for runtime-derived truth.
    if(FFPROBE_BIN)
        set(ANVIL_FFPROBE_VAL "${FFPROBE_BIN}")
    else()
        set(ANVIL_FFPROBE_VAL "ffprobe")
    endif()
    set_tests_properties(anvil_timestamp_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "ANVIL_TICK_FIXTURE=${ANVIL_TICK_CLIP};ANVIL_FFPROBE=${ANVIL_FFPROBE_VAL}"
        SKIP_RETURN_CODE 77)
endif()

# Regression for review 4209766186 (configure-time provenance staleness):
# configures a detached worktree at HEAD, mutates source and Git state, and
# rebuilds WITHOUT re-running configure; the manifest must never claim the
# stale clean identity (git_dirty_hash must appear on dirty rebuilds and the
# sha must advance on commit). Deterministic skip (exit 77) when git/cmake/
# ninja/ffmpeg/python3 are unavailable.
find_program(ANVIL_PYTHON3 python3)
if(ANVIL_PYTHON3 AND FFMPEG_ANVIL_FOUND AND FFMPEG_BIN)
    add_test(
        NAME anvil_provenance_rebuild_regression
        COMMAND "${ANVIL_PYTHON3}"
                "${CMAKE_CURRENT_SOURCE_DIR}/anvil_provenance_rebuild_regression.py"
                "${CMAKE_SOURCE_DIR}")
    set_tests_properties(anvil_provenance_rebuild_regression PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 1200
        # The regression materializes a full worktree of the source tree; keep
        # its scratch on the same filesystem as the build. On hosts where /tmp
        # is a small or restricted tmpfs, checking ~30k tracked files into it
        # fails ("unable to write file" -> "Could not reset index"), which
        # would masquerade as a provenance defect.
        ENVIRONMENT "TMPDIR=${CMAKE_BINARY_DIR}")
elseif(NOT ANVIL_PYTHON3)
    message(STATUS "ANVIL: python3 not found; anvil_provenance_rebuild_regression disabled")
endif()
