# Static analysis integration (cppcheck + clang-tidy + scan-build)
# SPDX-License-Identifier: MIT

option(ENI_ENABLE_CPPCHECK   "Enable cppcheck static analysis"   OFF)
option(ENI_ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
option(ENI_ENABLE_SCAN_BUILD "Enable Clang scan-build analyzer"  OFF)
option(ENI_ENABLE_INFER      "Enable Facebook Infer analyzer"    OFF)

# Compiler warning flags for memory safety
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wformat=2
        -Wformat-security
        -Wnull-dereference
        -Wstack-protector
        -Wstrict-overflow=2
        -Warray-bounds
        -Wimplicit-fallthrough
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wcast-qual
        -Wcast-align
        -Wwrite-strings
        -Wpointer-arith
    )

    # Memory safety specific warnings
    add_compile_options(
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wold-style-definition
        -Wvla
        -Wshadow
    )

    # Additional GCC-specific warnings
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        add_compile_options(
            -Wlogical-op
            -Wduplicated-cond
            -Wduplicated-branches
            -Wrestrict
            -Wstringop-overflow=4
            -Walloc-zero
        )
    endif()

    # Additional Clang-specific warnings
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        add_compile_options(
            -Wcomma
            -Wassign-enum
            -Wbad-function-cast
            -Wfloat-conversion
            -Wshorten-64-to-32
            -Wnullable-to-nonnull-conversion
        )
    endif()
endif()

if(ENI_ENABLE_CPPCHECK)
    find_program(CPPCHECK cppcheck)
    if(CPPCHECK)
        set(CMAKE_C_CPPCHECK
            ${CPPCHECK}
            --enable=warning,performance,portability,style,information
            --suppress=missingIncludeSystem
            --suppress=unusedFunction
            --inline-suppr
            --std=c11
            --quiet
            --error-exitcode=1
            --force
            # Memory safety checks
            --check-level=exhaustive
            --bug-hunting
        )
        message(STATUS "cppcheck enabled: ${CPPCHECK}")
    else()
        message(WARNING "cppcheck not found — static analysis disabled")
    endif()
endif()

if(ENI_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY clang-tidy)
    if(CLANG_TIDY)
        set(CMAKE_C_CLANG_TIDY
            ${CLANG_TIDY}
            -checks=-*,
                bugprone-*,
                cert-*,
                clang-analyzer-*,
                performance-*,
                portability-*,
                readability-braces-around-statements,
                readability-inconsistent-declaration-parameter-name,
                readability-misleading-indentation,
                readability-misplaced-array-index,
                readability-redundant-*,
                misc-definitions-in-headers,
                misc-misplaced-const,
                misc-redundant-expression,
                misc-static-assert,
                misc-unconventional-assign-operator,
                misc-unused-*,
                modernize-use-nullptr,
                concurrency-*,
                -bugprone-easily-swappable-parameters
            --warnings-as-errors=bugprone-*,cert-*,clang-analyzer-*
            --header-filter=.*
        )
        message(STATUS "clang-tidy enabled: ${CLANG_TIDY}")
    else()
        message(WARNING "clang-tidy not found — static analysis disabled")
    endif()
endif()

if(ENI_ENABLE_SCAN_BUILD)
    find_program(SCAN_BUILD scan-build)
    if(SCAN_BUILD)
        add_custom_target(scan-build
            COMMAND ${SCAN_BUILD}
                    -analyze-headers
                    -enable-checker alpha.core.BoolAssignment
                    -enable-checker alpha.core.CastSize
                    -enable-checker alpha.core.CastToStruct
                    -enable-checker alpha.core.IdenticalExpr
                    -enable-checker alpha.core.PointerArithm
                    -enable-checker alpha.core.PointerSub
                    -enable-checker alpha.core.SizeofPtr
                    -enable-checker alpha.core.TestAfterDivZero
                    -enable-checker alpha.security.ArrayBoundV2
                    -enable-checker alpha.security.MallocOverflow
                    -enable-checker alpha.security.ReturnPtrRange
                    -enable-checker alpha.security.taint.TaintPropagation
                    -enable-checker alpha.unix.BlockInCriticalSection
                    -enable-checker alpha.unix.Chroot
                    -enable-checker alpha.unix.PthreadLock
                    -enable-checker alpha.unix.SimpleStream
                    -enable-checker alpha.unix.Stream
                    -enable-checker alpha.unix.cstring.BufferOverlap
                    -enable-checker alpha.unix.cstring.NotNullTerminated
                    -enable-checker alpha.unix.cstring.OutOfBounds
                    --use-cc=${CMAKE_C_COMPILER}
                    -o ${CMAKE_BINARY_DIR}/scan-build-report
                    ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running Clang scan-build static analyzer"
        )
        message(STATUS "scan-build enabled: ${SCAN_BUILD}")
    else()
        message(WARNING "scan-build not found — Clang static analyzer disabled")
    endif()
endif()

if(ENI_ENABLE_INFER)
    find_program(INFER infer)
    if(INFER)
        add_custom_target(infer
            COMMAND ${INFER} run --compilation-database ${CMAKE_BINARY_DIR}/compile_commands.json
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Running Facebook Infer static analyzer"
        )

        add_custom_target(infer-report
            COMMAND ${INFER} report
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Generating Infer report"
            DEPENDS infer
        )
        message(STATUS "Facebook Infer enabled: ${INFER}")
    else()
        message(WARNING "Facebook Infer not found — install with: brew install infer")
    endif()
endif()

# Combined static analysis target
add_custom_target(static-analysis
    COMMENT "Running all enabled static analysis tools"
)

if(ENI_ENABLE_SCAN_BUILD AND TARGET scan-build)
    add_dependencies(static-analysis scan-build)
endif()

if(ENI_ENABLE_INFER AND TARGET infer)
    add_dependencies(static-analysis infer)
endif()
