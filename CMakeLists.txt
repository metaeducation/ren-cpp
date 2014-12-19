# This is an input file for the CMake makefile generator

#
# REN (REadable Notation) C++ Binding for the Red and Rebol languages
#
# Copyright (C) 2014 HostileFork.com
#
# Licensed under the Boost License, Version 1.0 (the "License")
#
#       http://www.boost.org/LICENSE_1_0.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied.  See the License for the specific language governing
# permissions and limitations under the License.
#

#
# If you want to run cmake in an interactive mode that gives you prompts
# for build options, use "cmake -i" (or one of the graphical interfaces)
# Otherwise you will get the default build.  Here are some resources:
#
#       http://en.wikipedia.org/wiki/CMake
#       http://gearbox.sourceforge.net/gbx_doc_buildsys.html
#       http://www.ddj.com/cpp/184405251
#       http://www.elpauer.org/stuff/learning_cmake.pdf
#
# If you just want to get a debug build (e.g. running from a CMake
# project in QtCreator) then open the CMakeLists.txt file in the
# IDE, and when asked to provide command-line options provide either:
#
#       -DRUNTIME=red
#
# Or if you want the binding for Rebol support instead of Red, then use:
#
#       -DRUNTIME=rebol
#
# The default will look for a directory named red or rebol for the code to
# be bound to which is in the same directory that the rencpp main directory
# can be found..  Clone your repository for red from:
#
#     https://github.com/red/red
#
# ...or if using Rebol, https://github.com/rebol/rebol
#
# (Note: It's helpful if the project has the Rebol include directories so you
# can "jump to definition" and such on them.  In order for this to
# work, you must put a symbolic link to rebol in the root directory of
# the rencpp project.)
#
# If you aren't aware of CMake's build settings, if doing development work
# you will likely want the Debug build:
#
#       -DCMAKE_BUILD_TYPE=Debug
#
# Also, should you be messing with this makefile and need to print out a
# status message to check what's going on in the make process, then 
# use something like:
#
#       message(STATUS "Some string" ${SOME_VAR})
#

project(rencpp)

cmake_minimum_required(VERSION 2.8)



# Default to looking for the runtime installation up one level

if (RUNTIME AND NOT RUNTIME_PATH)
    set(RUNTIME_PATH "../${RUNTIME}")
endif()


#
# This is a C++11 project, but it should work on pre-2011 compilers
# If you are using gcc 4.6 or older, then use --std=c++0x instead of
# --std=c++11
#

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")


#
# Turn up the warnings very high.
# Currently not adding in -Wshadow, because I like naming constructor
# arguments the same thing as the variables they initialize.
#
# http://stackoverflow.com/a/9862800/211160
#
# Also, -pedantic and -Wold-style-cast are disabled in the Rebol build
#
# Also, CMake has no good way to split this line:
#
#    https://www.mail-archive.com/cmake@cmake.org/msg16160.html
#

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wsign-conversion -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Woverloaded-virtual -Wredundant-decls -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Werror -Wno-unused")


#
# Include files intended to be used by clients of the binding are under
# %include/rencpp/ so that includes are prefixed as:
#
#     #include "rencpp/feature.hpp"
#
# The implementation cpp files and *internal* headers are in src/
#

include_directories(. include src)


#
# We build all the sources in the %src/ directory regardless of which
# binding is being used.
#

aux_source_directory(src SRC_LIST)


#
# Include files aren't technically necessary for the make, but you have
# to point them out to show up in QtCreator/VisualStudio/Xcode in your
# project tree.
#
# "Globbing" is to gather up files using a pattern.  While convenient,
# note the caveat here about how globbing doesn't record dependencies
# so you have to process file additions and removals with a re-run
# of CMake manually:
#
# http://stackoverflow.com/questions/1167154/#comment10556582_5843681
#
# Better ideas welcome.
#

file(GLOB INC_LIST "${CMAKE_CURRENT_SOURCE_DIR}/include/rencpp/*.h*"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h*")



if(NOT RUNTIME)

    # A version of Ren which can be built without Red or Rebol and has some
    # kind of formal parser that is C++ only is a concept for the future.
    # Another idea would be to build on a subset of Rebol or Red code and
    # just don't include anything related to the evalution engine, just
    # scanning of the types e.g. l-scan.c

    message(FATAL_ERROR "Please choose -DRUNTIME=red or -DRUNTIME=rebol")

elseif(RUNTIME STREQUAL "red")

    message(STATUS "Configuring Binding for Red")

    # Include all the sources specific to the Red Binding
    # (Exclude the Rebol ones for this build)

    aux_source_directory(src/red-binding SRC_BINDING_LIST)

    # The CMake variable for "RUNTIME" is distinct from the flag we use in
    # the headers, and controls more things (as you can see)

    add_definitions("-DREN_RUNTIME=REN_RUNTIME_RED")

    # Rebol is written in "old" ANSI C, so when we include its headers then it
    # can have some complaints if the warnings are set too high.  The Red
    # binding has these warnings turned on because there's no old C headers

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wold-style-cast")

    # Add any extra includes for the project browser

    file(GLOB INC_BINDING "${CMAKE_CURRENT_SOURCE_DIR}/src/red-binding/*.h*")
    set(INC_LIST ${INC_LIST} ${INC_BINDING})

elseif(RUNTIME STREQUAL "rebol")

    # We need to get at the C includes from Rebol to call its internal
    # functions to initialize the interpreter and implement the necessary
    # hooks by calls to those internal functions.  This does not use the
    # "RL_" API,

    message(STATUS "Runtime path is ${RUNTIME_PATH}")

    # Though an initiative for the Red project, this is the grand binding
    # attempt for C++.  And it should be able to recompile and work just as
    # effectively for Rebol, just substituting a different cell type into
    # the structure.  Gives a broader base for testing as well...

    message(STATUS "Configuring Binding for Rebol")

    # We want to be able to #include "rebol/include/whatever.h", but to get
    # that to work we have to add an include path which has rebol as a
    # subdirectory.  This might wreak havoc if your parent directory contains
    # other stuff, and you could wind up with something like this:
    #
    #     http://stackoverflow.com/questions/1621396/
    #
    # Apologies if that happens to you.  The best way to sandbox the whole
    # thing is to put your runtime path as a subdirectory of something, so
    # like putting rebol under a rebol directory and then having the outer
    # rebol in the include directory.  So worst case scenario, do that.
    # This should work for most cases.

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${RUNTIME_PATH}/../")

    # Include all the sources specific to the Rebol Binding
    # (Exclude the Red ones for this build)

    aux_source_directory(src/rebol-binding SRC_BINDING_LIST)

    # Put the Rebol headers in so you can search them in the project

    file(GLOB INC_REBOL "${RUNTIME_PATH}/src/include/*.h")

    # Add any extra includes for the project browser

    file(GLOB INC_BINDING "${CMAKE_CURRENT_SOURCE_DIR}/rebol-binding/*.h*")
    set(INC_LIST ${INC_LIST} ${INC_BINDING} ${INC_REBOL})

    # The CMake variable for "RUNTIME" is distinct from the flag we use in
    # the headers, and controls more things (as you can see)

    add_definitions("-DREN_RUNTIME=REN_RUNTIME_REBOL")

    # It's important to make sure you're set to the right platform, as it
    # affects endianness and other things.

    add_definitions(-DTO_LINUX)

    # Try and get all the guts out of the Rebol Core executable to
    # include in the binding.  Take out the main and Host_Lib
    # instance so that the program you link with the binding is
    # the entry point.

    file(
        GLOB EXTRA_OBJS
        RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        "${CMAKE_CURRENT_SOURCE_DIR}/${RUNTIME_PATH}/make/objs/*.o"
    )
    list(REMOVE_ITEM EXTRA_OBJS ${RUNTIME_PATH}/make/objs/host-main.o)

    message(STATUS "path" "${CMAKE_CURRENT_SOURCE_DIR}/${RUNTIME_PATH}")
    message(STATUS "extra_objs" ${EXTRA_OBJS})

    # Speaking of things to rip out, the great-wall-of-Rebol-lib
    # is one of them.  It's a fairly anemic interface; the binding
    # runs at a privileged level and wants to do anything a native
    # could have done.  Don't have to remove them...but they're
    # not useful when you have the full interpreter internals available

    list(REMOVE_ITEM EXTRA_OBJS
        "{$RUNTIME_PATH}/make/objs/a-lib2.o"
        "{$RUNTIME_PATH}/make/objs/a-lib.o"
    )

    # Rebol depends on dlopen and other calls in "libDl".  Cmake can
    # supposedly find that for you automatically with this variable.
    # http://en.wikipedia.org/wiki/Dynamic_loading

    set(LIBS_ALL ${CMAKE_DL_LIBS})

else()

    message(FATAL_ERROR "Invalid Runtime Specified (not none, red, rebol)")

endif()


set(SRC_ALL ${SRC_LIST} ${SRC_BINDING_LIST} ${INC_LIST} ${EXTRA_OBJS})



#
# These tests do not require an evaluator, and simply test Ren values
#

add_executable(literals-test examples/literals-test.cpp ${SRC_ALL})
target_link_libraries(literals-test ${LIBS_ALL})

add_executable(cast-test examples/cast-test.cpp ${SRC_ALL})
target_link_libraries(cast-test ${LIBS_ALL})

add_executable(block-test examples/block-test.cpp ${SRC_ALL})
target_link_libraries(block-test ${LIBS_ALL})

add_executable(assign-test examples/assign-test.cpp ${SRC_ALL})
target_link_libraries(assign-test ${LIBS_ALL})

add_executable(form-test examples/form-test.cpp ${SRC_ALL})
target_link_libraries(form-test ${LIBS_ALL})



#
# These tests should work with any evaluator, assuming there is one
#

if(NOT RUNTIME STREQUAL "none")

    add_executable(apply-test examples/apply-test.cpp ${SRC_ALL})
    target_link_libraries(apply-test ${LIBS_ALL})

    add_executable(parse-test-1 examples/parse-test-1.cpp ${SRC_ALL})
    target_link_libraries(parse-test-1 ${LIBS_ALL})

    add_executable(parse-test-2 examples/parse-test-2.cpp ${SRC_ALL})
    target_link_libraries(parse-test-2 ${LIBS_ALL})

    add_executable(extension-test examples/extension-test.cpp ${SRC_ALL})
    target_link_libraries(extension-test ${LIBS_ALL})

    add_executable(context-test examples/context-test.cpp ${SRC_ALL})
    target_link_libraries(context-test ${LIBS_ALL})

endif()



#
# These tests can call methods on the runtime object that are specific to
# the evaluator in use.
#

if(RUNTIME STREQUAL "red")

    add_executable(red-test examples/red-test.cpp ${SRC_ALL})
    target_link_libraries(red-test ${LIBS_ALL})

elseif(RUNTIME STREQUAL "rebol")

    add_executable(rebol-test examples/rebol-test.cpp ${SRC_ALL})
    target_link_libraries(rebol-test ${LIBS_ALL})

endif()
