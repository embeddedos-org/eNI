add_library(eni_common STATIC
    src/types.c
    src/event.c
    src/log.c
    src/config.c
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

if(UNIX AND NOT APPLE)
    target_link_libraries(eni_common PRIVATE m)
endif()
