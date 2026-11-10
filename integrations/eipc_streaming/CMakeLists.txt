add_library(eni_eipc_stream STATIC src/eipc_stream.c)
target_include_directories(eni_eipc_stream PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(eni_eipc_stream PUBLIC eni_common)
# A missing #include must fail the build with GCC and Clang; GCC before 14 only warns.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(eni_eipc_stream PRIVATE -Werror=implicit-function-declaration)
endif()
