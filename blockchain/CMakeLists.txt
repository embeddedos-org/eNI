# Blockchain module for eNI
# SPDX-License-Identifier: MIT

option(ENI_BC_USE_OPENSSL "Use OpenSSL for cryptographic operations" OFF)

set(ENI_BC_SOURCES
    src/types.c
    src/crypto.c
    src/client.c
    src/provenance.c
    src/consent.c
    src/device.c
    src/sharing.c
    src/blockchain.c
)

add_library(eni_blockchain STATIC ${ENI_BC_SOURCES})

target_include_directories(eni_blockchain
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(eni_blockchain PUBLIC eni_common)

if(ENI_BC_USE_OPENSSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(eni_blockchain PRIVATE OpenSSL::Crypto)
    target_compile_definitions(eni_blockchain PRIVATE ENI_BC_USE_OPENSSL)
endif()

# Compiler warnings
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(eni_blockchain PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

# Install headers
install(DIRECTORY include/eni_bc
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS eni_blockchain
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
