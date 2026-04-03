# Disable static analysis for test code — clang-tidy --warnings-as-errors
# prevents test binaries from compiling, causing "Not Run" in ctest.
set(CMAKE_C_CPPCHECK "")
set(CMAKE_C_CLANG_TIDY "")

# Disable static analysis for test code — clang-tidy --warnings-as-errors
# prevents test binaries from compiling, causing "Not Run" in ctest.
set(CMAKE_C_CPPCHECK "")
set(CMAKE_C_CLANG_TIDY "")

add_executable(eni_test_common test_common.c)
target_link_libraries(eni_test_common PRIVATE eni_common)
add_test(NAME eni_common_tests COMMAND eni_test_common)
set_tests_properties(eni_common_tests PROPERTIES LABELS "unit")

add_executable(eni_test_min test_min.c)
target_link_libraries(eni_test_min PRIVATE eni_common eni_min)
add_test(NAME eni_min_tests COMMAND eni_test_min)
set_tests_properties(eni_min_tests PROPERTIES LABELS "unit")

add_executable(eni_test_framework test_framework.c)
target_link_libraries(eni_test_framework PRIVATE eni_common eni_framework)
add_test(NAME eni_framework_tests COMMAND eni_test_framework)
set_tests_properties(eni_framework_tests PROPERTIES LABELS "unit")

add_executable(eni_test_providers test_providers.c)
target_link_libraries(eni_test_providers PRIVATE eni_common eni_providers)
add_test(NAME eni_provider_tests COMMAND eni_test_providers)
set_tests_properties(eni_provider_tests PROPERTIES LABELS "unit")

# --- test_event: Event system ---
add_executable(eni_test_event test_event.c)
target_link_libraries(eni_test_event PRIVATE eni_common)
add_test(NAME eni_event_tests COMMAND eni_test_event)
set_tests_properties(eni_event_tests PROPERTIES LABELS "unit")

# --- test_policy: Policy engine ---
add_executable(eni_test_policy test_policy.c)
target_link_libraries(eni_test_policy PRIVATE eni_common)
add_test(NAME eni_policy_tests COMMAND eni_test_policy)
set_tests_properties(eni_policy_tests PROPERTIES LABELS "unit")

# --- test_neuralink: Neuralink provider ---
add_executable(eni_test_neuralink test_neuralink.c)
target_link_libraries(eni_test_neuralink PRIVATE eni_common eni_providers)
add_test(NAME eni_neuralink_tests COMMAND eni_test_neuralink)
set_tests_properties(eni_neuralink_tests PROPERTIES LABELS "unit")

# --- test_filter: Min filter ---
add_executable(eni_test_filter test_filter.c)
target_link_libraries(eni_test_filter PRIVATE eni_common eni_min)
add_test(NAME eni_filter_tests COMMAND eni_test_filter)
set_tests_properties(eni_filter_tests PROPERTIES LABELS "unit")

# --- test_normalizer: Min normalizer ---
add_executable(eni_test_normalizer test_normalizer.c)
target_link_libraries(eni_test_normalizer PRIVATE eni_common eni_min)
add_test(NAME eni_normalizer_tests COMMAND eni_test_normalizer)
set_tests_properties(eni_normalizer_tests PROPERTIES LABELS "unit")

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
