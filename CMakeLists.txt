# This is an input file for the CMake makefile generator

#
# REN (REadable Notation) C++ Binding for the Red and Rebol languages
#
# Copyright (C) 2015 HostileFork.com
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
# Comments in this CMakeLists.txt were originally the entirety of the "build
# documentation".  However, look at doc/build.md for what is developing as the
# more user-friendly instructions.  These instructions will be pared down
# so as not to be redundant as that effort stabilizes.
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
# (Note: Due to a schedule pacing issue, RenCpp has been pre-released in a
# mode with Rebol functionality, while the Red function is unfinished. This
# will hopefully be remedied soon; but until then, these instructions will
# only work with -DRUNTIME=rebol)
#
# The default heuristic of this makefile is to look for a directory named red
# or rebol which is in the same directory that the rencpp main directory
# can be found.  If you wish to override this, specify:
#
#     -DRUNTIME_PATH=/wherever/you/put/runtime-source
#
# Clone your repository for red from:
#
#     https://github.com/red/red
#
# ...or if using Rebol, https://github.com/rebol/rebol
#
# (Note: If you are using Qt Creator, it can be helpful if the project indexes
# the Rebol include directories, so you can "jump to definition" on Rebol code.
# In order for that indexing to work, you must put a symbolic link to rebol in
# the root directory of the rencpp project.)
#

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
# If at all possible, please check changes against multiple compilers.  This
# CMakeLists.txt is supposed to be compiler independent, so specify
# alternative compilers e.g. with:
#
#       -DCMAKE_CXX_COMPILER=/usr/bin/clang
#

project(rencpp)

cmake_minimum_required(VERSION 2.8)


# Simplify appending additional flags to CMAKE_CXX_FLAGS.

macro(add_cxx_flags flags)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flags}")
endmacro()


# Allow removing of flags from CMAKE_CXX_FLAGS
#
# http://stackoverflow.com/a/18234926/211160

macro(remove_cxx_flag flag)
    string(REPLACE "${flag}" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endmacro()


# Helper for dumping lists, each item on a new line and not pushed together
# without spaces (which is the default behavior)

macro(dump_list message list)
    message(STATUS ${message})
    foreach(ITEM ${list})
        message(STATUS "  " ${ITEM})
    endforeach()
endmacro()


# Equivalent to list(REMOVE_ITEM list item) but checks to make sure the
# removal actually happened.  There must be an easier way to say this.

macro(ensure_remove_item list_name item)
    list(FIND "${list_name}" ${item} ITEM_INDEX)
    if(ITEM_INDEX EQUAL -1)
        message(FATAL_ERROR "Couldn't find item to remove " ${item})
    endif()
    list(REMOVE_AT "${list_name}" ${ITEM_INDEX})
    list(FIND "${list_name}" ${item} ITEM_INDEX)
    if(NOT ITEM_INDEX EQUAL -1)
        message(FATAL_ERROR "Removal failed of item " ${item})
    endif()
endmacro()


#
# Default to looking for the runtime installation up one level
#
# We use absolute paths here to find almost all files so that the directories
# which inherit this file's settings vs add_subdirectory will still work
#

if(RUNTIME AND NOT RUNTIME_PATH)
    set(RUNTIME_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../${RUNTIME})
endif()


#
# This is a C++11 project, but it should work on pre-2011 compilers
# If you are using gcc 4.6 or older, then use --std=c++0x instead of
# --std=c++11
#

add_cxx_flags("-std=c++11")


# Turn up the warnings very high.
#
#     http://stackoverflow.com/a/9862800/211160
#
# Currently not adding in -Wshadow, because @HostileFork likes naming
# constructor arguments the same thing as the variables they initialize.
#
# For compiler identification notes:
#
#     http://stackoverflow.com/a/10055571/211160

add_cxx_flags(-Wall)
add_cxx_flags(-Wsign-conversion)
add_cxx_flags(-Wextra)
add_cxx_flags(-Wcast-align)
add_cxx_flags(-Wcast-qual)
add_cxx_flags(-Wctor-dtor-privacy)
add_cxx_flags(-Wdisabled-optimization)
add_cxx_flags(-Wformat=2)
add_cxx_flags(-Winit-self)
add_cxx_flags(-Wlogical-op)
add_cxx_flags(-Wmissing-declarations)
add_cxx_flags(-Wmissing-include-dirs)
add_cxx_flags(-Wnoexcept)
add_cxx_flags(-Woverloaded-virtual)
add_cxx_flags(-Wredundant-decls)
add_cxx_flags(-Wsign-promo)
add_cxx_flags(-Wstrict-null-sentinel)
add_cxx_flags(-Wstrict-overflow=5)
add_cxx_flags(-Wswitch-default)
add_cxx_flags(-Wundef)
add_cxx_flags(-Werror)
add_cxx_flags(-Wno-unused)
add_cxx_flags(-pedantic)
add_cxx_flags(-Wold-style-cast)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # using regular Clang or AppleClang

  remove_cxx_flag(-Wlogical-op)
  remove_cxx_flag(-Wnoexcept)
  remove_cxx_flag(-Wstrict-null-sentinel)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  # using GCC
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  # using Intel C++
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  # using Visual Studio C++
endif()


# Include files intended to be used by clients of the binding are under
# %include/rencpp/ so that includes are prefixed as:
#
#     #include "rencpp/feature.hpp"
#
# The implementation cpp files and *internal* headers are in src/

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)


# We build all the sources in the %src/ directory regardless of which
# binding is being used.

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)


# Include files aren't technically necessary for the make, but you have
# to point them out to show up in QtCreator/VisualStudio/Xcode in your
# project tree.
#
# "Globbing" is to gather up files using a pattern.  While convenient,
# note the caveat here about how globbing doesn't record dependencies
# so you have to process file additions and removals with a re-run
# of CMake manually:
#
#     http://stackoverflow.com/questions/1167154/#comment10556582_5843681
#
# Better ideas welcome.

file(
    GLOB INC_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/include/rencpp/*.h*
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h*
)



#
# Because the standard library doesn't have any Unicode support to speak of
# and several of the Qt Toolkit's types have interesting mappings to Ren
# otherwise, you can build against the Qt library in addition to building
# against the standard C++ library, for things like converting Ren values
# into QString vs std::string (or both, or neither).
#


if(GARDEN AND NOT (CLASSLIB_QT EQUAL 1))

    # Technically speaking Ren Garden *could* fall back on some helper stubs
    # if not built for Qt.  But we're experimenting here with making things
    # as integrative and "pretty" as possible...

    # https://github.com/hostilefork/rencpp/issues/23

    message(FATAL_ERROR "CLASSLIB_QT must be 1 if building Ren Garden")
endif()


if((NOT CLASSLIB_STD) OR (CLASSLIB_STD EQUAL 1))

    # Assume we want the standard classlib if none specified

    add_definitions(-DREN_CLASSLIB_STD=1)

elseif(CLASSLIB_STD EQUAL 0)

    add_definitions(-DREN_CLASSLIB_STD=0)

else()

    message(FATAL_ERROR "CLASSLIB_STD must be 0 or 1 if defined")

endif()



if((NOT CLASSLIB_QT) OR (CLASSLIB_QT EQUAL 0))

    # Assume we don't want the Qt classlib if none specified

    add_definitions(-DREN_CLASSLIB_QT=0)

elseif(CLASSLIB_QT EQUAL 1)

    # Find the QtCore library - we only need the core for types like QString
    # and QByteArray and such.
    #
    #     http://www.kdab.com/using-cmake-with-qt-5/

    find_package(Qt5Core REQUIRED)
    find_package(Qt5Gui REQUIRED) # for QImage
    add_definitions(-DREN_CLASSLIB_QT=1)
    include_directories(${Qt5Core_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS})
    add_cxx_flags(
        "${Qt5Core_EXECUTABLE_COMPILE_FLAGS}"
        "${Qt5Gui_EXECUTABLE_COMPILE_FLAGS}"
    )
    set(LIBS_ALL ${LIBS_ALL} Qt5::Core Qt5::Gui)

else()

    message(FATAL_ERROR "CLASSLIB_QT must be 0 or 1 if defined")

endif()



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

    aux_source_directory(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/red-binding
        SRC_BINDING_LIST
    )

    # The CMake variable for "RUNTIME" is distinct from the flag we use in
    # the headers, and controls more things (as you can see)

    add_definitions("-DREN_RUNTIME=REN_RUNTIME_RED")

    # Add any extra includes for the project browser

    file(GLOB INC_BINDING ${CMAKE_CURRENT_SOURCE_DIR}/src/red-binding/*.h*)
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

    add_cxx_flags(-I${RUNTIME_PATH}/../)

    # Include all the sources specific to the Rebol Binding

    aux_source_directory(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/rebol-binding
        SRC_BINDING_LIST
    )

    # Put the Rebol headers in so you can search them in the project

    file(GLOB INC_REBOL ${RUNTIME_PATH}/src/include/*.h)

    # Rebol is written in "old" ANSI C, so when we include its headers then it
    # can have some complaints if the warnings are set too high.  We tone the
    # strictness down a little so the headers will work.

    add_cxx_flags(-Wno-unknown-pragmas)
    remove_cxx_flag(-pedantic)
    remove_cxx_flag(-Wold-style-cast)
    remove_cxx_flag(-Wsign-conversion)
    remove_cxx_flag(-Wcast-align)
    remove_cxx_flag(-Wcast-qual)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # Rebol's use of setjmp and longjmp triggers several of these warnings,
        # which exist on GCC but not Clang

        add_cxx_flags(-Wno-clobbered)
    endif()

    # Add any extra includes for the project browser

    file(GLOB INC_BINDING ${CMAKE_CURRENT_SOURCE_DIR}/rebol-binding/*.h*)
    set(INC_LIST ${INC_LIST} ${INC_BINDING} ${INC_REBOL})

    # The CMake variable for "RUNTIME" is distinct from the flag we use in
    # the headers, and controls more things (as you can see)

    add_definitions("-DREN_RUNTIME=REN_RUNTIME_REBOL")

    # It's important to make sure you're set to the right platform, as it
    # affects endianness and other things.

    if(WIN32)

        add_definitions(-DTO_WIN32)
        add_definitions(-DOS_DEFS) # Kill warning about IS_ERROR redefinition.
        add_definitions(-DUNICODE)

    else()

        add_definitions(-DTO_LINUX)

    endif()

    # Try and get all the guts out of the Rebol Core executable to
    # include in the binding.  Take out the main and Host_Lib
    # instance so that the program you link with the binding is
    # the entry point.

    file(GLOB EXTRA_OBJS ${RUNTIME_PATH}/make/objs/*.o)
    ensure_remove_item("EXTRA_OBJS" ${RUNTIME_PATH}/make/objs/host-main.o)
    ensure_remove_item("EXTRA_OBJS" ${RUNTIME_PATH}/make/objs/host-stdio.o)


    # Among things that would be nice to omit, the great-wall-of-Rebol-lib
    # is one of them.  It's a fairly anemic interface; the binding
    # runs at a privileged level and wants to do anything a native
    # could have done.  Unfortunately, small links here and there keep
    # them dependent.
    #
    # ensure_remove_item("EXTRA_OBJS" ${RUNTIME_PATH}/make/objs/a-lib.o)
    # ensure_remove_item("EXTRA_OBJS" ${RUNTIME_PATH}/make/objs/a-lib2.o)

    # The binding could replace the entirety of the "host", implementing all
    # the network calls in "Rebolese" using its ioctl system.  That's work
    # one should only do selectively.  One thing you can't really punt on
    # if you're going to do an interesting demo is capturing the stdio, so
    # the hooks are reimplemented to speak to the C++ iostreams and we axe
    # the host kit one.

    ensure_remove_item("EXTRA_OBJS" ${RUNTIME_PATH}/make/objs/dev-stdio.o)

    # We cannot use the c-do.c file unmodified from github's rebol/rebol,
    # due to Halt_State being static.  Though that's a small change, more
    # radical changes can give us interesting hooks into the evaluator.
    # So it's worth it to pull that file out and customize it in Rencpp.
    # But we then have to put the customized version from the Rencpp
    # sources at the right position in the list so its globals get picked
    # up by the other Rebol object files
    #
    # Note that the license inherited by c-do.cpp is Apache, not Boost.

    ensure_remove_item("EXTRA_OBJS" ${RUNTIME_PATH}/make/objs/c-do.o)

    # There is a nasty issue in Bind_Block_Words, for which a pull request
    # that may (or may not) fix it has been sent:
    #
    #     https://github.com/rebol/rebol/pull/234
    #
    # The idea of that being "the fix" is tenuous, but it at least allows one
    # to COPY an object that has been used for binding...a critical feature
    # for RenCpp.  We use the forked c-frame here until such time as the
    # solution is integrated.

    ensure_remove_item("EXTRA_OBJS" ${RUNTIME_PATH}/make/objs/c-frame.o)

    # Rebol depends on dlopen and other calls in "libDl".  Cmake can
    # supposedly find that for you automatically with this variable.
    # http://en.wikipedia.org/wiki/Dynamic_loading

    set(LIBS_ALL ${LIBS_ALL} ${CMAKE_DL_LIBS})

    if(WIN32)

        # Rebol depends on Winsocks when built on Windows.

        set(LIBS_ALL ${LIBS_ALL} ws2_32)

    endif()

else()

    message(FATAL_ERROR "Invalid Runtime Specified (not none, red, rebol)")

endif()



#
# Though it's early yet to decide exactly how the project will be packaged,
# CMake's static libraries at least speed up the build for now.
#

add_library(
    RenCpp STATIC
    ${EXTRA_OBJS} ${SRC_LIST} ${SRC_BINDING_LIST} ${INC_LIST}
)

target_link_libraries(RenCpp ${LIBS_ALL} -lstdc++ -lm)


#
# examples/ has its own CMakeLists.txt, with settings pertinent to each
# case (e.g. OpenGL configuration, etc.), but inherits the settings
# from this file.
#
#     http://stackoverflow.com/q/7979963/211160
#

add_subdirectory(examples)



#
# CMake has a testing framework called CTest, which we don't really do much
# with:
#
#     http://www.vtk.org/Wiki/CMake/Testing_With_CTest
#
# It's easier just to run the "test" executable and see what it says.
#
# But for what it's worth, you can run `make test` from CMake and it will
# run the test executable for you, suppress the output, and hide it
# two levels deep in a directory.  If that's what you want.
#
# tests/ has a separate CMakeLists.txt, for structured unit tests. These tests
# use the Catch framework, which is held as a CMake-managed dependency in
# external/catch/.
#

enable_testing()

add_subdirectory(external/catch)

add_subdirectory(tests)
