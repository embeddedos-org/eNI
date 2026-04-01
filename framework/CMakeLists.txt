add_library(eni_framework STATIC
    src/provider_manager.c
    src/stream_bus.c
    src/router.c
    src/orchestrator.c
    src/connectors.c
    src/observability.c
    src/service.c
)

if(ENI_BUILD_DSP)
    target_sources(eni_framework PRIVATE src/signal_processor.c)
endif()

if(ENI_BUILD_DECODER)
    target_sources(eni_framework PRIVATE src/decoder.c)
endif()

if(ENI_BUILD_STIMULATOR)
    target_sources(eni_framework PRIVATE src/feedback.c)
endif()

target_include_directories(eni_framework PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eni_framework PUBLIC eni_common eni_min)
