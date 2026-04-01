add_library(eni_min STATIC
    src/input.c
    src/normalizer.c
    src/filter.c
    src/mapper.c
    src/tool_bridge.c
    src/service.c
)

if(ENI_BUILD_DSP)
    target_sources(eni_min PRIVATE src/signal_processor.c)
endif()

if(ENI_BUILD_DECODER)
    target_sources(eni_min PRIVATE src/decoder.c)
endif()

if(ENI_BUILD_STIMULATOR)
    target_sources(eni_min PRIVATE src/feedback.c)
endif()

target_include_directories(eni_min PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eni_min PUBLIC eni_common)
