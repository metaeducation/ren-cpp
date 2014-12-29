# This is an input file for the CMake makefile generator

# See notes in root directory, where this is added via add_subdirectory


#
# These tests do not require an evaluator, and simply test Ren values
#

add_executable(literals-test literals-test.cpp ${SRC_ALL})
target_link_libraries(literals-test ${LIBS_ALL})

add_executable(cast-test cast-test.cpp ${SRC_ALL})
target_link_libraries(cast-test ${LIBS_ALL})

add_executable(block-test block-test.cpp ${SRC_ALL})
target_link_libraries(block-test ${LIBS_ALL})

add_executable(assign-test assign-test.cpp ${SRC_ALL})
target_link_libraries(assign-test ${LIBS_ALL})

add_executable(form-test form-test.cpp ${SRC_ALL})
target_link_libraries(form-test ${LIBS_ALL})



#
# These tests should work with any evaluator, assuming there is one
#

if(NOT RUNTIME STREQUAL "none")

    add_executable(apply-test apply-test.cpp ${SRC_ALL})
    target_link_libraries(apply-test ${LIBS_ALL})

    add_executable(parse-test-1 parse-test-1.cpp ${SRC_ALL})
    target_link_libraries(parse-test-1 ${LIBS_ALL})

    add_executable(parse-test-2 parse-test-2.cpp ${SRC_ALL})
    target_link_libraries(parse-test-2 ${LIBS_ALL})

    add_executable(context-test context-test.cpp ${SRC_ALL})
    target_link_libraries(context-test ${LIBS_ALL})

    add_executable(extension-test-1 extension-test-1.cpp ${SRC_ALL})
    target_link_libraries(extension-test-1 ${LIBS_ALL})

    add_executable(extension-test-2 extension-test-2.cpp ${SRC_ALL})
    target_link_libraries(extension-test-2 ${LIBS_ALL})

endif()



#
# These tests can call methods on the runtime object that are specific to
# the evaluator in use.
#

if(RUNTIME STREQUAL "red")

    add_executable(red-test red-test.cpp ${SRC_ALL})
    target_link_libraries(red-test ${LIBS_ALL})

elseif(RUNTIME STREQUAL "rebol")

    add_executable(rebol-test rebol-test.cpp ${SRC_ALL})
    target_link_libraries(rebol-test ${LIBS_ALL})

endif()


#
# These demos are in subdirectories and have more complex dependencies 
# besides just baseline C++11.  They must be requested specifically, such 
# as passing in:
#


if(GARDEN)

    # Ren Garden is temporarily being built in the Rencpp project, but has
    # its own issue tracker and will be migrated to:
    #
    #     https://github.com/hostilefork/ren-garden
    #

    add_subdirectory(workbench)

endif()
