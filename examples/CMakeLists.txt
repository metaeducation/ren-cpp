# This is an input file for the CMake makefile generator

# See notes in root directory, where this is added via add_subdirectory

if(NOT RUNTIME STREQUAL "none")

    add_executable(parse-1 parse-1.cpp)
    target_link_libraries(parse-1 RenCpp)

    add_executable(parse-2 parse-2.cpp)
    target_link_libraries(parse-2 RenCpp)

    add_executable(function-1 function-1.cpp)
    target_link_libraries(function-1 RenCpp)

endif()



#
# These demos are in subdirectories and have more complex dependencies 
# besides just baseline C++11.  They must be requested specifically, such 
# as passing in: -DGARDEN=yes
#


if(GARDEN)

    # Ren Garden is temporarily being built in the Rencpp project, but has
    # its own issue tracker and will be migrated to:
    #
    #     https://github.com/metaeducation/ren-garden
    #

    add_subdirectory(workbench)

endif()
