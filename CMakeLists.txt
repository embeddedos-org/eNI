cmake_minimum_required(VERSION 3.20)
project(ENI
    VERSION 0.3.0
    DESCRIPTION "Neural Interface Adapter — BCI/sensor input integration for EoS AI Layer"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/sanitizers.cmake)
include(cmake/coverage.cmake)
include(cmake/static-analysis.cmake)
include(cmake/memcheck.cmake)

option(ENI_BUILD_MIN       "Build ENI-Min lightweight runtime"       ON)
option(ENI_BUILD_FRAMEWORK "Build ENI-Framework industrial platform" ON)
option(ENI_BUILD_CLI       "Build ENI CLI tool"                      ON)
option(ENI_BUILD_TESTS     "Build unit tests"                        OFF)

option(ENI_EIPC_ENABLED    "Enable EIPC integration" OFF)
option(ENI_BUILD_DSP       "Build DSP signal processing module"      ON)
option(ENI_BUILD_DECODER   "Build neural decoder module"             ON)
option(ENI_BUILD_NN        "Build neural network inference engine"    ON)
option(ENI_BUILD_STIMULATOR "Build stimulator/feedback subsystem"    ON)
option(ENI_BUILD_DATA_FORMATS "Build neural data format I/O (EDF+/BDF+/XDF/ENI)" ON)
option(ENI_BUILD_SESSION     "Build session management and calibration"         ON)

option(ENI_BUILD_SIM         "Build simulation environment"          OFF)
option(ENI_BUILD_BINDINGS_CPP "Build C++ bindings"                   OFF)
option(ENI_BUILD_ROS2        "Build ROS 2 integration"               OFF)
option(ENI_BUILD_ONI         "Build ONI compliance layer"            OFF)
option(ENI_BUILD_TFLITE      "Build TFLite Micro integration"        OFF)
option(ENI_BUILD_EIPC_STREAM "Build EIPC streaming module"           OFF)
option(ENI_ONNX_ENABLED      "Enable ONNX model loading"            OFF)
option(ENI_WIRELESS_ENABLED   "Enable wireless BCI provider"        OFF)

if(ENI_EIPC_ENABLED)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../eipc/sdk/c ${CMAKE_BINARY_DIR}/eipc_c_sdk EXCLUDE_FROM_ALL)
endif()

option(ENI_PLATFORM_EOS   "Enable EoS platform adapter"   OFF)
option(ENI_PLATFORM_LINUX     "Enable Linux platform adapter"     ON)
option(ENI_PLATFORM_WINDOWS   "Enable Windows platform adapter"   OFF)
option(ENI_PLATFORM_CONTAINER "Enable container platform adapter" OFF)

if(WIN32)
    set(ENI_PLATFORM_LINUX OFF)
    set(ENI_PLATFORM_WINDOWS ON)
endif()

add_subdirectory(common)
add_subdirectory(platform)
add_subdirectory(providers)

if(ENI_BUILD_MIN)
    add_subdirectory(min)
endif()

if(ENI_BUILD_FRAMEWORK)
    add_subdirectory(framework)
endif()

if(ENI_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(ENI_BUILD_SIM)
    add_subdirectory(sim)
endif()

if(ENI_BUILD_BINDINGS_CPP)
    add_subdirectory(bindings/cpp)
endif()

if(ENI_BUILD_ROS2 OR ENI_BUILD_ONI OR ENI_BUILD_EIPC_STREAM)
    add_subdirectory(integrations)
endif()

if(ENI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "--------------------------------------------")
message(STATUS "ENI v${PROJECT_VERSION}")
message(STATUS "  ENI-Min:       ${ENI_BUILD_MIN}")
message(STATUS "  ENI-Framework: ${ENI_BUILD_FRAMEWORK}")
message(STATUS "  CLI:           ${ENI_BUILD_CLI}")
message(STATUS "  DSP:           ${ENI_BUILD_DSP}")
message(STATUS "  Decoder:       ${ENI_BUILD_DECODER}")
message(STATUS "  NN:            ${ENI_BUILD_NN}")
message(STATUS "  ONNX:          ${ENI_ONNX_ENABLED}")
message(STATUS "  Stimulator:    ${ENI_BUILD_STIMULATOR}")
message(STATUS "  Data Formats:  ${ENI_BUILD_DATA_FORMATS}")
message(STATUS "  Session/Cal:   ${ENI_BUILD_SESSION}")
message(STATUS "  Simulation:    ${ENI_BUILD_SIM}")
message(STATUS "  C++ Bindings:  ${ENI_BUILD_BINDINGS_CPP}")
message(STATUS "  TFLite Micro:  ${ENI_BUILD_TFLITE}")
message(STATUS "  ROS 2:         ${ENI_BUILD_ROS2}")
message(STATUS "  ONI:           ${ENI_BUILD_ONI}")
message(STATUS "  Wireless BCI:  ${ENI_WIRELESS_ENABLED}")
message(STATUS "  Platform:      eos=${ENI_PLATFORM_EOS} linux=${ENI_PLATFORM_LINUX} win=${ENI_PLATFORM_WINDOWS} container=${ENI_PLATFORM_CONTAINER}")
message(STATUS "  Sanitizers:    asan=${ENI_ENABLE_ASAN} ubsan=${ENI_ENABLE_UBSAN} tsan=${ENI_ENABLE_TSAN} msan=${ENI_ENABLE_MSAN}")
message(STATUS "  Analysis:      cppcheck=${ENI_ENABLE_CPPCHECK} clang-tidy=${ENI_ENABLE_CLANG_TIDY} scan-build=${ENI_ENABLE_SCAN_BUILD}")
message(STATUS "  Memcheck:      ${ENI_ENABLE_MEMCHECK}")
message(STATUS "  Coverage:      ${ENI_ENABLE_COVERAGE}")
message(STATUS "--------------------------------------------")
