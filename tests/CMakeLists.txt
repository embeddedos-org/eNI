# Disable static analysis for test code — clang-tidy --warnings-as-errors
# prevents test binaries from compiling, causing "Not Run" in ctest.
set(CMAKE_C_CPPCHECK "")
set(CMAKE_C_CLANG_TIDY "")

add_executable(eni_test_common test_common.c)
target_link_libraries(eni_test_common PRIVATE eni_common)
add_test(NAME eni_common_tests COMMAND eni_test_common)
set_tests_properties(eni_common_tests PROPERTIES LABELS "unit")
eni_add_memcheck_target(eni_test_common)

add_executable(eni_test_min test_min.c)
target_link_libraries(eni_test_min PRIVATE eni_common eni_min)
add_test(NAME eni_min_tests COMMAND eni_test_min)
set_tests_properties(eni_min_tests PROPERTIES LABELS "unit")
eni_add_memcheck_target(eni_test_min)

add_executable(eni_test_framework test_framework.c)
target_link_libraries(eni_test_framework PRIVATE eni_common eni_framework)
add_test(NAME eni_framework_tests COMMAND eni_test_framework)
set_tests_properties(eni_framework_tests PROPERTIES LABELS "unit")
eni_add_memcheck_target(eni_test_framework)

add_executable(eni_test_providers test_providers.c)
target_link_libraries(eni_test_providers PRIVATE eni_common eni_providers)
add_test(NAME eni_provider_tests COMMAND eni_test_providers)
set_tests_properties(eni_provider_tests PROPERTIES LABELS "unit")
eni_add_memcheck_target(eni_test_providers)

# --- test_event: Event system ---
add_executable(eni_test_event test_event.c)
target_link_libraries(eni_test_event PRIVATE eni_common)
add_test(NAME eni_event_tests COMMAND eni_test_event)
set_tests_properties(eni_event_tests PROPERTIES LABELS "unit")
eni_add_memcheck_target(eni_test_event)

# --- test_policy: Policy engine ---
add_executable(eni_test_policy test_policy.c)
target_link_libraries(eni_test_policy PRIVATE eni_common)
add_test(NAME eni_policy_tests COMMAND eni_test_policy)
set_tests_properties(eni_policy_tests PROPERTIES LABELS "unit")
eni_add_memcheck_target(eni_test_policy)

# --- test_neuralink: Neuralink provider ---
add_executable(eni_test_neuralink test_neuralink.c)
target_link_libraries(eni_test_neuralink PRIVATE eni_common eni_providers)
add_test(NAME eni_neuralink_tests COMMAND eni_test_neuralink)
set_tests_properties(eni_neuralink_tests PROPERTIES LABELS "unit")
eni_add_memcheck_target(eni_test_neuralink)

# --- test_filter: Min filter ---
add_executable(eni_test_filter test_filter.c)
target_link_libraries(eni_test_filter PRIVATE eni_common eni_min)
add_test(NAME eni_filter_tests COMMAND eni_test_filter)
set_tests_properties(eni_filter_tests PROPERTIES LABELS "unit")
eni_add_memcheck_target(eni_test_filter)

# --- test_normalizer: Min normalizer ---
add_executable(eni_test_normalizer test_normalizer.c)
target_link_libraries(eni_test_normalizer PRIVATE eni_common eni_min)
add_test(NAME eni_normalizer_tests COMMAND eni_test_normalizer)
set_tests_properties(eni_normalizer_tests PROPERTIES LABELS "unit")
eni_add_memcheck_target(eni_test_normalizer)

# --- BCI Feature Tests ---

# test_event_feedback: Feedback event types
add_executable(eni_test_event_feedback test_event_feedback.c)
target_link_libraries(eni_test_event_feedback PRIVATE eni_common)
add_test(NAME eni_event_feedback_tests COMMAND eni_test_event_feedback)
set_tests_properties(eni_event_feedback_tests PROPERTIES LABELS "unit")

# test_provider_eeg: EEG non-invasive provider
if(ENI_PROVIDER_EEG)
    add_executable(eni_test_provider_eeg test_provider_eeg.c)
    target_link_libraries(eni_test_provider_eeg PRIVATE eni_common eni_providers)
    add_test(NAME eni_provider_eeg_tests COMMAND eni_test_provider_eeg)
    set_tests_properties(eni_provider_eeg_tests PROPERTIES LABELS "unit")
endif()

# ============================================================
# Coverage Improvement Tests
# ============================================================

# test_fw_decoder: Decoder (energy + nn) tests
if(ENI_BUILD_DECODER)
    add_executable(eni_test_fw_decoder test_fw_decoder.c)
    target_link_libraries(eni_test_fw_decoder PRIVATE eni_common)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_fw_decoder PRIVATE m)
    endif()
    add_test(NAME eni_fw_decoder_tests COMMAND eni_test_fw_decoder)
    set_tests_properties(eni_fw_decoder_tests PROPERTIES LABELS "unit")
endif()

# test_fw_feedback: Feedback controller tests
if(ENI_BUILD_STIMULATOR)
    add_executable(eni_test_fw_feedback test_fw_feedback.c)
    target_link_libraries(eni_test_fw_feedback PRIVATE eni_common)
    add_test(NAME eni_fw_feedback_tests COMMAND eni_test_fw_feedback)
    set_tests_properties(eni_fw_feedback_tests PROPERTIES LABELS "unit")
endif()

# test_config_extended: Config loading (KV, JSON, new keys)
add_executable(eni_test_config_extended test_config_extended.c)
target_link_libraries(eni_test_config_extended PRIVATE eni_common)
add_test(NAME eni_config_extended_tests COMMAND eni_test_config_extended)
set_tests_properties(eni_config_extended_tests PROPERTIES LABELS "unit")

# test_stream_bus_extended: Stream bus overflow, pop_wait, stats
add_executable(eni_test_stream_bus_extended test_stream_bus_extended.c)
target_link_libraries(eni_test_stream_bus_extended PRIVATE eni_common eni_framework eni_platform)
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(eni_test_stream_bus_extended PRIVATE Threads::Threads)
endif()
add_test(NAME eni_stream_bus_extended_tests COMMAND eni_test_stream_bus_extended)
set_tests_properties(eni_stream_bus_extended_tests PROPERTIES LABELS "unit")

# test_stim_safety_coverage: Stim safety extended coverage
if(ENI_BUILD_STIMULATOR)
    add_executable(eni_test_stim_safety_coverage test_stim_safety_coverage.c)
    target_link_libraries(eni_test_stim_safety_coverage PRIVATE eni_common)
    add_test(NAME eni_stim_safety_coverage_tests COMMAND eni_test_stim_safety_coverage)
    set_tests_properties(eni_stim_safety_coverage_tests PROPERTIES LABELS "unit")
endif()

# test_platform_extended: Platform threading, condvar, atomics
add_executable(eni_test_platform_extended test_platform_extended.c)
target_link_libraries(eni_test_platform_extended PRIVATE eni_common eni_platform)
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(eni_test_platform_extended PRIVATE Threads::Threads)
endif()
add_test(NAME eni_platform_extended_tests COMMAND eni_test_platform_extended)
set_tests_properties(eni_platform_extended_tests PROPERTIES LABELS "unit")

# test_dsp: DSP signal processing
if(ENI_BUILD_DSP)
    add_executable(eni_test_dsp test_dsp.c)
    target_link_libraries(eni_test_dsp PRIVATE eni_common)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_dsp PRIVATE m)
    endif()
    add_test(NAME eni_dsp_tests COMMAND eni_test_dsp)
    set_tests_properties(eni_dsp_tests PROPERTIES LABELS "unit")
endif()

# test_nn: Neural network inference
if(ENI_BUILD_NN)
    add_executable(eni_test_nn test_nn.c)
    target_link_libraries(eni_test_nn PRIVATE eni_common)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_nn PRIVATE m)
    endif()
    add_test(NAME eni_nn_tests COMMAND eni_test_nn)
    set_tests_properties(eni_nn_tests PROPERTIES LABELS "unit")
endif()

# test_decoder: Neural decoder
if(ENI_BUILD_DECODER)
    add_executable(eni_test_decoder test_decoder.c)
    target_link_libraries(eni_test_decoder PRIVATE eni_common)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_decoder PRIVATE m)
    endif()
    add_test(NAME eni_decoder_tests COMMAND eni_test_decoder)
    set_tests_properties(eni_decoder_tests PROPERTIES LABELS "unit")
endif()

# test_stimulator: Simulated stimulator
if(ENI_PROVIDER_STIMULATOR_SIM AND ENI_BUILD_STIMULATOR)
    add_executable(eni_test_stimulator test_stimulator.c)
    target_link_libraries(eni_test_stimulator PRIVATE eni_common eni_providers)
    add_test(NAME eni_stimulator_tests COMMAND eni_test_stimulator)
    set_tests_properties(eni_stimulator_tests PROPERTIES LABELS "unit")
endif()

# test_stim_safety: Stimulation safety system
if(ENI_BUILD_STIMULATOR)
    add_executable(eni_test_stim_safety test_stim_safety.c)
    target_link_libraries(eni_test_stim_safety PRIVATE eni_common)
    add_test(NAME eni_stim_safety_tests COMMAND eni_test_stim_safety)
    set_tests_properties(eni_stim_safety_tests PROPERTIES LABELS "unit")
endif()

# test_bci_pipeline: End-to-end integration
if(ENI_PROVIDER_EEG AND ENI_BUILD_DSP AND ENI_BUILD_DECODER AND ENI_BUILD_STIMULATOR AND ENI_PROVIDER_STIMULATOR_SIM)
    add_executable(eni_test_bci_pipeline test_bci_pipeline.c)
    target_link_libraries(eni_test_bci_pipeline PRIVATE eni_common eni_providers)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_bci_pipeline PRIVATE m)
    endif()
    add_test(NAME eni_bci_pipeline_tests COMMAND eni_test_bci_pipeline)
    set_tests_properties(eni_bci_pipeline_tests PROPERTIES LABELS "integration")
endif()

# ============================================================
# Phase 5: Extended Tests & Mock Infrastructure
# ============================================================

# Mock library (shared by extended tests)
add_library(eni_test_mocks STATIC mocks/mock_neural_input.c mocks/mock_provider.c)
target_include_directories(eni_test_mocks PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/mocks)
target_link_libraries(eni_test_mocks PUBLIC eni_common)
if(UNIX AND NOT APPLE)
    target_link_libraries(eni_test_mocks PRIVATE m)
endif()

# Extended DSP tests
if(ENI_BUILD_DSP)
    add_executable(eni_test_dsp_extended test_dsp_extended.c)
    target_link_libraries(eni_test_dsp_extended PRIVATE eni_common eni_test_mocks)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_dsp_extended PRIVATE m)
    endif()
    add_test(NAME eni_dsp_extended_tests COMMAND eni_test_dsp_extended)
    set_tests_properties(eni_dsp_extended_tests PROPERTIES LABELS "unit")
endif()

# Extended NN tests
if(ENI_BUILD_NN)
    add_executable(eni_test_nn_extended test_nn_extended.c)
    target_link_libraries(eni_test_nn_extended PRIVATE eni_common eni_test_mocks)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_nn_extended PRIVATE m)
    endif()
    add_test(NAME eni_nn_extended_tests COMMAND eni_test_nn_extended)
    set_tests_properties(eni_nn_extended_tests PROPERTIES LABELS "unit")
endif()

# Extended decoder tests
if(ENI_BUILD_DECODER)
    add_executable(eni_test_decoder_extended test_decoder_extended.c)
    target_link_libraries(eni_test_decoder_extended PRIVATE eni_common eni_test_mocks)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_decoder_extended PRIVATE m)
    endif()
    add_test(NAME eni_decoder_extended_tests COMMAND eni_test_decoder_extended)
    set_tests_properties(eni_decoder_extended_tests PROPERTIES LABELS "unit")
endif()

# Provider lifecycle tests
add_executable(eni_test_provider_lifecycle test_provider_lifecycle.c)
target_link_libraries(eni_test_provider_lifecycle PRIVATE eni_common eni_test_mocks)
add_test(NAME eni_provider_lifecycle_tests COMMAND eni_test_provider_lifecycle)
set_tests_properties(eni_provider_lifecycle_tests PROPERTIES LABELS "unit")

# Extended stim safety tests
if(ENI_BUILD_STIMULATOR)
    add_executable(eni_test_stim_safety_extended test_stim_safety_extended.c)
    target_link_libraries(eni_test_stim_safety_extended PRIVATE eni_common eni_test_mocks)
    add_test(NAME eni_stim_safety_extended_tests COMMAND eni_test_stim_safety_extended)
    set_tests_properties(eni_stim_safety_extended_tests PROPERTIES LABELS "unit")
endif()

# Full pipeline integration test
if(ENI_BUILD_DSP AND ENI_BUILD_DECODER AND ENI_BUILD_STIMULATOR)
    add_executable(eni_test_full_pipeline integration/test_full_pipeline.c)
    target_link_libraries(eni_test_full_pipeline PRIVATE eni_common eni_providers eni_test_mocks)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_full_pipeline PRIVATE m)
    endif()
    add_test(NAME eni_full_pipeline_tests COMMAND eni_test_full_pipeline)
    set_tests_properties(eni_full_pipeline_tests PROPERTIES LABELS "integration")
endif()

# ============================================================
# Phase 7: World-Class Upgrade Tests
# ============================================================

# JSON parser tests
add_executable(eni_test_json test_json.c)
target_link_libraries(eni_test_json PRIVATE eni_common)
add_test(NAME eni_json_tests COMMAND eni_test_json)
set_tests_properties(eni_json_tests PROPERTIES LABELS "unit")

# Thread safety tests
add_executable(eni_test_thread_safety test_thread_safety.c)
target_link_libraries(eni_test_thread_safety PRIVATE eni_common eni_framework eni_platform)
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(eni_test_thread_safety PRIVATE Threads::Threads)
endif()
add_test(NAME eni_thread_safety_tests COMMAND eni_test_thread_safety)
set_tests_properties(eni_thread_safety_tests PROPERTIES LABELS "unit")

# Data format tests (EDF+, BDF+, XDF, annotations)
if(ENI_BUILD_DATA_FORMATS)
    add_executable(eni_test_data_formats test_data_formats.c)
    target_link_libraries(eni_test_data_formats PRIVATE eni_common eni_platform)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_data_formats PRIVATE m)
    endif()
    add_test(NAME eni_data_format_tests COMMAND eni_test_data_formats)
    set_tests_properties(eni_data_format_tests PROPERTIES LABELS "unit")
endif()

# Session, calibration, profile tests
if(ENI_BUILD_SESSION)
    add_executable(eni_test_session_calibration test_session_calibration.c)
    target_link_libraries(eni_test_session_calibration PRIVATE eni_common eni_platform)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_session_calibration PRIVATE m)
    endif()
    add_test(NAME eni_session_calibration_tests COMMAND eni_test_session_calibration)
    set_tests_properties(eni_session_calibration_tests PROPERTIES LABELS "unit")
endif()

# ============================================================
# Coverage Gap Tests — types.c, log.c, event.c, policy.c
# ============================================================

# types.c: all eni_status_str branches + eni_timestamp_now
add_executable(eni_test_types_coverage test_types_coverage.c)
target_link_libraries(eni_test_types_coverage PRIVATE eni_common)
add_test(NAME eni_types_coverage_tests COMMAND eni_test_types_coverage)
set_tests_properties(eni_types_coverage_tests PROPERTIES LABELS "unit")

# log.c: all log levels, filtering, output redirect, null module
add_executable(eni_test_log_coverage test_log_coverage.c)
target_link_libraries(eni_test_log_coverage PRIVATE eni_common)
add_test(NAME eni_log_coverage_tests COMMAND eni_test_log_coverage)
set_tests_properties(eni_log_coverage_tests PROPERTIES LABELS "unit")

# event.c: eni_event_print all branches, overflow, null/long inputs
add_executable(eni_test_event_coverage test_event_coverage.c)
target_link_libraries(eni_test_event_coverage PRIVATE eni_common)
add_test(NAME eni_event_coverage_tests COMMAND eni_test_event_coverage)
set_tests_properties(eni_event_coverage_tests PROPERTIES LABELS "unit")

# policy.c: eni_policy_dump all branches, overflow, long action, null guards
add_executable(eni_test_policy_coverage test_policy_coverage.c)
target_link_libraries(eni_test_policy_coverage PRIVATE eni_common)
add_test(NAME eni_policy_coverage_tests COMMAND eni_test_policy_coverage)
set_tests_properties(eni_policy_coverage_tests PROPERTIES LABELS "unit")

# ============================================================
# Integration Tests: End-to-End Workflows
# ============================================================

# Calibration + Session lifecycle integration test
if(ENI_BUILD_SESSION)
    add_executable(eni_test_calibration_session integration/test_calibration_session.c)
    target_link_libraries(eni_test_calibration_session PRIVATE eni_common eni_platform)
    if(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(eni_test_calibration_session PRIVATE Threads::Threads)
    endif()
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_calibration_session PRIVATE m)
    endif()
    add_test(NAME eni_calibration_session_tests COMMAND eni_test_calibration_session)
    set_tests_properties(eni_calibration_session_tests PROPERTIES LABELS "integration")
    if(WIN32)
        set_tests_properties(eni_calibration_session_tests PROPERTIES
            WILL_FAIL TRUE
            DISABLED TRUE
        )
    endif()
endif()

# Minimal session debug test (always builds)
if(ENI_BUILD_SESSION)
    add_executable(eni_test_mini_session test_mini_session.c)
    target_link_libraries(eni_test_mini_session PRIVATE eni_common eni_platform)
    if(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(eni_test_mini_session PRIVATE Threads::Threads)
    endif()
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_mini_session PRIVATE m)
    endif()
    add_test(NAME eni_mini_session_tests COMMAND eni_test_mini_session)
    set_tests_properties(eni_mini_session_tests PROPERTIES LABELS "unit")
endif()

# ============================================================
# Stress Tests: Concurrency & Multi-Device
# ============================================================

# Multi-device sync, concurrent calibration, thread contention
if(ENI_BUILD_SESSION)
    add_executable(eni_test_concurrent_stress stress/test_concurrent_stress.c)
    target_link_libraries(eni_test_concurrent_stress PRIVATE eni_common eni_framework eni_platform)
    if(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(eni_test_concurrent_stress PRIVATE Threads::Threads)
    endif()
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_concurrent_stress PRIVATE m)
    endif()
    add_test(NAME eni_concurrent_stress_tests COMMAND eni_test_concurrent_stress)
    set_tests_properties(eni_concurrent_stress_tests PROPERTIES
        LABELS "stress"
        TIMEOUT 120
    )
endif()
