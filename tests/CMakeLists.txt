# This is an input file for the CMake makefile generator

# See notes in root directory, where this is added via add_subdirectory


#
# We set up tests to run with Catch test framework:
#
#   https://github.com/philsquared/Catch
#
# Catch itself is set up as an external, CMake-managed dependency in the root
# ext/catch/ directory. This dependency is included in the main CMake file. A
# CATCH_INCLUDE_DIR property is inherited, to allow us to include the single
# Catch header file.
#

include_directories(${CATCH_INCLUDE_DIR})

# These rigid warnings are too strict for building with Catch.
remove_cxx_flag("-Wctor-dtor-privacy")
remove_cxx_flag("-Wredundant-decls")
remove_cxx_flag("-Wswitch-default")


#
# These tests do not require an evaluator, and simply test Ren values
#

set(
    VALUE_TESTS

    isolated-test.cpp

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

if(DEFINED RUNTIME)

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

add_test(run-test-rencpp test-rencpp)
