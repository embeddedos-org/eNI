# Valgrind memcheck integration for eNI
# SPDX-License-Identifier: MIT

option(ENI_ENABLE_MEMCHECK "Enable Valgrind memcheck for memory leak detection" OFF)

if(ENI_ENABLE_MEMCHECK)
    find_program(VALGRIND_EXECUTABLE valgrind)

    if(VALGRIND_EXECUTABLE)
        message(STATUS "Valgrind found: ${VALGRIND_EXECUTABLE}")

        # Valgrind suppression file (optional)
        set(ENI_VALGRIND_SUPPRESSION_FILE "${CMAKE_SOURCE_DIR}/valgrind.supp"
            CACHE FILEPATH "Valgrind suppression file")

        # Core memcheck options
        set(VALGRIND_MEMCHECK_OPTIONS
            --tool=memcheck
            --leak-check=full
            --show-leak-kinds=all
            --track-origins=yes
            --trace-children=yes
            --error-exitcode=1
            --errors-for-leak-kinds=definite,possible
            --gen-suppressions=all
            --verbose
        )

        # Add suppression file if it exists
        if(EXISTS "${ENI_VALGRIND_SUPPRESSION_FILE}")
            list(APPEND VALGRIND_MEMCHECK_OPTIONS
                "--suppressions=${ENI_VALGRIND_SUPPRESSION_FILE}")
            message(STATUS "Using Valgrind suppressions: ${ENI_VALGRIND_SUPPRESSION_FILE}")
        endif()

        # Create memcheck target for running tests under Valgrind
        add_custom_target(memcheck
            COMMAND ${CMAKE_COMMAND} -E echo "Running Valgrind memcheck..."
            COMMENT "Running memory leak detection with Valgrind"
        )

        # Function to add memcheck target for a specific executable
        function(eni_add_memcheck_target TARGET_NAME)
            if(TARGET ${TARGET_NAME})
                add_custom_target(memcheck-${TARGET_NAME}
                    COMMAND ${VALGRIND_EXECUTABLE} ${VALGRIND_MEMCHECK_OPTIONS}
                            $<TARGET_FILE:${TARGET_NAME}>
                    DEPENDS ${TARGET_NAME}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    COMMENT "Running Valgrind memcheck on ${TARGET_NAME}"
                )
                add_dependencies(memcheck memcheck-${TARGET_NAME})
            endif()
        endfunction()

        # Function to add memcheck for CTest tests
        function(eni_enable_memcheck_tests)
            set(MEMORYCHECK_COMMAND ${VALGRIND_EXECUTABLE})
            set(MEMORYCHECK_COMMAND_OPTIONS "${VALGRIND_MEMCHECK_OPTIONS}")
            set(MEMORYCHECK_TYPE Valgrind)
            include(CTest)
        endfunction()

        # Helgrind target for thread error detection
        add_custom_target(helgrind
            COMMENT "Running Valgrind helgrind for thread error detection"
        )

        function(eni_add_helgrind_target TARGET_NAME)
            if(TARGET ${TARGET_NAME})
                add_custom_target(helgrind-${TARGET_NAME}
                    COMMAND ${VALGRIND_EXECUTABLE}
                            --tool=helgrind
                            --error-exitcode=1
                            $<TARGET_FILE:${TARGET_NAME}>
                    DEPENDS ${TARGET_NAME}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    COMMENT "Running Valgrind helgrind on ${TARGET_NAME}"
                )
                add_dependencies(helgrind helgrind-${TARGET_NAME})
            endif()
        endfunction()

        # Massif target for heap profiling
        function(eni_add_massif_target TARGET_NAME)
            if(TARGET ${TARGET_NAME})
                add_custom_target(massif-${TARGET_NAME}
                    COMMAND ${VALGRIND_EXECUTABLE}
                            --tool=massif
                            --massif-out-file=${TARGET_NAME}.massif.out
                            $<TARGET_FILE:${TARGET_NAME}>
                    DEPENDS ${TARGET_NAME}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    COMMENT "Running Valgrind massif heap profiler on ${TARGET_NAME}"
                )
            endif()
        endfunction()

        # Cachegrind target for cache profiling
        function(eni_add_cachegrind_target TARGET_NAME)
            if(TARGET ${TARGET_NAME})
                add_custom_target(cachegrind-${TARGET_NAME}
                    COMMAND ${VALGRIND_EXECUTABLE}
                            --tool=cachegrind
                            --cachegrind-out-file=${TARGET_NAME}.cachegrind.out
                            $<TARGET_FILE:${TARGET_NAME}>
                    DEPENDS ${TARGET_NAME}
                    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                    COMMENT "Running Valgrind cachegrind on ${TARGET_NAME}"
                )
            endif()
        endfunction()

    else()
        message(WARNING "Valgrind not found — memcheck disabled. Install with: brew install valgrind (macOS) or apt install valgrind (Linux)")

        # Provide stub functions when Valgrind is not available
        function(eni_add_memcheck_target TARGET_NAME)
            message(STATUS "Skipping memcheck target for ${TARGET_NAME} (Valgrind not found)")
        endfunction()

        function(eni_enable_memcheck_tests)
        endfunction()

        function(eni_add_helgrind_target TARGET_NAME)
        endfunction()

        function(eni_add_massif_target TARGET_NAME)
        endfunction()

        function(eni_add_cachegrind_target TARGET_NAME)
        endfunction()
    endif()
else()
    # Stub functions when memcheck is disabled
    function(eni_add_memcheck_target TARGET_NAME)
    endfunction()

    function(eni_enable_memcheck_tests)
    endfunction()

    function(eni_add_helgrind_target TARGET_NAME)
    endfunction()

    function(eni_add_massif_target TARGET_NAME)
    endfunction()

    function(eni_add_cachegrind_target TARGET_NAME)
    endfunction()
endif()
