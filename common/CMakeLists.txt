add_library(eni_common STATIC
    src/types.c
    src/event.c
    src/log.c
    src/config.c
    src/json.c
    src/policy.c
    src/tool_call.c
    src/provider.c
)

target_include_directories(eni_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(ENI_EIPC_ENABLED)
    target_sources(eni_common PRIVATE src/eipc_bridge.c)
    target_compile_definitions(eni_common PUBLIC ENI_EIPC_ENABLED)
    target_link_libraries(eni_common PUBLIC eipc_c)
endif()

if(ENI_BUILD_DSP)
    target_sources(eni_common PRIVATE src/dsp.c)
    target_compile_definitions(eni_common PUBLIC ENI_HAS_DSP)
endif()

if(ENI_BUILD_NN)
    target_sources(eni_common PRIVATE src/nn.c)
    target_compile_definitions(eni_common PUBLIC ENI_HAS_NN)
endif()

if(ENI_BUILD_DECODER)
    target_sources(eni_common PRIVATE src/decoder.c)
    target_compile_definitions(eni_common PUBLIC ENI_HAS_DECODER)
endif()

if(ENI_BUILD_STIMULATOR)
    target_sources(eni_common PRIVATE src/stim_safety.c src/feedback.c)
    target_compile_definitions(eni_common PUBLIC ENI_HAS_STIMULATOR)
endif()

# Data format modules
if(ENI_BUILD_DATA_FORMATS)
    target_sources(eni_common PRIVATE
        src/data_format.c
        src/edf.c
        src/bdf.c
        src/xdf.c
        src/eni_format.c
        src/annotation.c
        src/recorder.c
        src/player.c
    )
    target_compile_definitions(eni_common PUBLIC ENI_HAS_DATA_FORMATS)
endif()

# Session & calibration modules
if(ENI_BUILD_SESSION)
    target_sources(eni_common PRIVATE
        src/session.c
        src/profile.c
        src/calibration.c
        src/transfer.c
    )
    target_compile_definitions(eni_common PUBLIC ENI_HAS_SESSION)
endif()

# Production hardening modules
target_sources(eni_common PRIVATE
    src/recovery.c
    src/mempool.c
    src/device_manager.c
)

if(UNIX AND NOT APPLE)
    target_link_libraries(eni_common PRIVATE m)
endif()
