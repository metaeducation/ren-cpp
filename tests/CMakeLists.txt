# This is an input file for the CMake makefile generator

# See notes in root directory, where this is added via add_subdirectory



#
# These tests do not require an evaluator, and simply test Ren values
#

add_executable(literals-test literals-test.cpp)
target_link_libraries(literals-test RenCpp)
add_test(run-literals-test literals-test)

add_executable(cast-test cast-test.cpp)
target_link_libraries(cast-test RenCpp)
add_test(run-cast-test cast-test)

add_executable(block-test block-test.cpp)
target_link_libraries(block-test RenCpp)
add_test(run-block-test block-test)

add_executable(assign-test assign-test.cpp)
target_link_libraries(assign-test RenCpp)
add_test(run-assign-test assign-test)

add_executable(form-test form-test.cpp)
target_link_libraries(form-test RenCpp)
add_test(run-form-test form-test)

add_executable(iterator-test iterator-test.cpp)
target_link_libraries(iterator-test RenCpp)
add_test(run-iterator-test iterator-test)



#
# These tests should work with any evaluator, assuming there is one
#

if(NOT RUNTIME STREQUAL "none")

    add_executable(apply-test apply-test.cpp)
    target_link_libraries(apply-test RenCpp)
    add_test(run-apply-test apply-test)

    add_executable(context-test context-test.cpp)
    target_link_libraries(context-test RenCpp)
    add_test(run-context-test context-test)

    add_executable(function-test function-test.cpp)
    target_link_libraries(function-test RenCpp)
    add_test(run-function-test function-test)

endif()



#
# These tests can call methods on the runtime object that are specific to
# the evaluator in use.
#

if(RUNTIME STREQUAL "red")

    add_executable(red-test red-test.cpp)
    target_link_libraries(red-test RenCpp)
    add_test(run-red-test red-test)

elseif(RUNTIME STREQUAL "rebol")

    add_executable(rebol-test rebol-test.cpp)
    target_link_libraries(rebol-test RenCpp)
    add_test(run-rebol-test rebol-test)

endif()
