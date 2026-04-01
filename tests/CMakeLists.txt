add_executable(eni_test_common test_common.c)
target_link_libraries(eni_test_common PRIVATE eni_common)
add_test(NAME eni_common_tests COMMAND eni_test_common)

add_executable(eni_test_min test_min.c)
target_link_libraries(eni_test_min PRIVATE eni_common eni_min)
add_test(NAME eni_min_tests COMMAND eni_test_min)

add_executable(eni_test_framework test_framework.c)
target_link_libraries(eni_test_framework PRIVATE eni_common eni_framework)
add_test(NAME eni_framework_tests COMMAND eni_test_framework)

add_executable(eni_test_providers test_providers.c)
target_link_libraries(eni_test_providers PRIVATE eni_common eni_providers)
add_test(NAME eni_provider_tests COMMAND eni_test_providers)

# --- test_event: Event system ---
add_executable(eni_test_event test_event.c)
target_link_libraries(eni_test_event PRIVATE eni_common)
add_test(NAME eni_event_tests COMMAND eni_test_event)

# --- test_policy: Policy engine ---
add_executable(eni_test_policy test_policy.c)
target_link_libraries(eni_test_policy PRIVATE eni_common)
add_test(NAME eni_policy_tests COMMAND eni_test_policy)

# --- test_neuralink: Neuralink provider ---
add_executable(eni_test_neuralink test_neuralink.c)
target_link_libraries(eni_test_neuralink PRIVATE eni_common eni_providers)
add_test(NAME eni_neuralink_tests COMMAND eni_test_neuralink)

# --- test_filter: Min filter ---
add_executable(eni_test_filter test_filter.c)
target_link_libraries(eni_test_filter PRIVATE eni_common eni_min)
add_test(NAME eni_filter_tests COMMAND eni_test_filter)

# --- test_normalizer: Min normalizer ---
add_executable(eni_test_normalizer test_normalizer.c)
target_link_libraries(eni_test_normalizer PRIVATE eni_common eni_min)
add_test(NAME eni_normalizer_tests COMMAND eni_test_normalizer)

# --- BCI Feature Tests ---

# test_event_feedback: Feedback event types
add_executable(eni_test_event_feedback test_event_feedback.c)
target_link_libraries(eni_test_event_feedback PRIVATE eni_common)
add_test(NAME eni_event_feedback_tests COMMAND eni_test_event_feedback)

# test_provider_eeg: EEG non-invasive provider
if(ENI_PROVIDER_EEG)
    add_executable(eni_test_provider_eeg test_provider_eeg.c)
    target_link_libraries(eni_test_provider_eeg PRIVATE eni_common eni_providers)
    add_test(NAME eni_provider_eeg_tests COMMAND eni_test_provider_eeg)
endif()

# test_dsp: DSP signal processing
if(ENI_BUILD_DSP)
    add_executable(eni_test_dsp test_dsp.c)
    target_link_libraries(eni_test_dsp PRIVATE eni_common)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_dsp PRIVATE m)
    endif()
    add_test(NAME eni_dsp_tests COMMAND eni_test_dsp)
endif()

# test_nn: Neural network inference
if(ENI_BUILD_NN)
    add_executable(eni_test_nn test_nn.c)
    target_link_libraries(eni_test_nn PRIVATE eni_common)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_nn PRIVATE m)
    endif()
    add_test(NAME eni_nn_tests COMMAND eni_test_nn)
endif()

# test_decoder: Neural decoder
if(ENI_BUILD_DECODER)
    add_executable(eni_test_decoder test_decoder.c)
    target_link_libraries(eni_test_decoder PRIVATE eni_common)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_decoder PRIVATE m)
    endif()
    add_test(NAME eni_decoder_tests COMMAND eni_test_decoder)
endif()

# test_stimulator: Simulated stimulator
if(ENI_PROVIDER_STIMULATOR_SIM AND ENI_BUILD_STIMULATOR)
    add_executable(eni_test_stimulator test_stimulator.c)
    target_link_libraries(eni_test_stimulator PRIVATE eni_common eni_providers)
    add_test(NAME eni_stimulator_tests COMMAND eni_test_stimulator)
endif()

# test_stim_safety: Stimulation safety system
if(ENI_BUILD_STIMULATOR)
    add_executable(eni_test_stim_safety test_stim_safety.c)
    target_link_libraries(eni_test_stim_safety PRIVATE eni_common)
    add_test(NAME eni_stim_safety_tests COMMAND eni_test_stim_safety)
endif()

# test_bci_pipeline: End-to-end integration
if(ENI_PROVIDER_EEG AND ENI_BUILD_DSP AND ENI_BUILD_DECODER AND ENI_BUILD_STIMULATOR AND ENI_PROVIDER_STIMULATOR_SIM)
    add_executable(eni_test_bci_pipeline test_bci_pipeline.c)
    target_link_libraries(eni_test_bci_pipeline PRIVATE eni_common eni_providers)
    if(UNIX AND NOT APPLE)
        target_link_libraries(eni_test_bci_pipeline PRIVATE m)
    endif()
    add_test(NAME eni_bci_pipeline_tests COMMAND eni_test_bci_pipeline)
endif()
