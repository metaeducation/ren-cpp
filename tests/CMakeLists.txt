# This is an input file for the CMake makefile generator

# See notes in root directory, where this is added via add_subdirectory



#
# These tests do not require an evaluator, and simply test Ren values
#

set(
    VALUE_TESTS

    literals-test.cpp
    cast-test.cpp
    block-test.cpp
    assign-test.cpp
    form-test.cpp
    iterator-test.cpp
)

#
# These tests should work with any evaluator, assuming there is one
#

if(NOT RUNTIME STREQUAL "none")

    set(
        EVALUATOR_TESTS

        apply-test.cpp
        context-test.cpp
        function-test.cpp
    )

endif()


#
# These tests can call methods on the runtime object that are specific to
# the evaluator in use.
#

if(RUNTIME STREQUAL "red")

    set(RUNTIME_TESTS red-test.cpp)

elseif(RUNTIME STREQUAL "rebol")

    set(RUNTIME_TESTS rebol-test.cpp)

endif()


#
# Make one big executable...
#

add_executable(
    test-rencpp

    main.cpp
    ${VALUE_TESTS}
    ${EVALUATOR_TESTS}
    ${RUNTIME_TESTS}
)

target_link_libraries(test-rencpp RenCpp)

add_test(run-test all-tests)
