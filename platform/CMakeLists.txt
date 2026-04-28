add_library(eni_platform STATIC)

target_include_directories(eni_platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eni_platform PUBLIC eni_common)

if(ENI_PLATFORM_EOS)
    target_sources(eni_platform PRIVATE eos/platform_eos.c)
    target_compile_definitions(eni_platform PUBLIC ENI_PLATFORM_EOS_ENABLED)
elseif(WIN32 OR ENI_PLATFORM_WINDOWS)
    target_sources(eni_platform PRIVATE windows/platform_windows.c)
elseif(APPLE OR ENI_PLATFORM_MACOS)
    target_sources(eni_platform PRIVATE macos/platform_macos.c)
elseif(UNIX OR ENI_PLATFORM_LINUX)
    target_sources(eni_platform PRIVATE linux/platform_linux.c)
else()
    target_sources(eni_platform PRIVATE posix/platform_posix.c)
endif()

# Link threading libraries
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(eni_platform PUBLIC Threads::Threads)
endif()
