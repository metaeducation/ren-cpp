# This is an input file for the CMake makefile generator

#
# REN (REadable Notation) C++ Binding for Rebol languages
#
# Copyright (C) 2015-2017 HostileFork.com and contributors
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

# NOTE: 9 times out of 10 these days, people working on this project want:
#
#    -DCMAKE_BUILD_TYPE=Debug -DCLASSLIB_QT=1 -DGARDEN=yes
#        -DCMAKE_VERBOSE_MAKEFILE=ON

# ...as the CMake build parameters.  So putting these up top.  Qt Creator
# has a bad habit of losing the cache of this, so we keep needing it.  :-/
#
# If the build stops due to a warning, please report that warning to the
# issue tracker.  However, as things have been set to be very sensitive and
# compilers can add new "extra" warnings on a whim, you may be able to
# get it to build by adding -DRIGOROUS=no to the list.
#

#
# NOTE: Comments in this CMakeLists.txt were originally the entirety of
# the "build documentation".  However, look at %doc/build.md for what is
# developing as the more user-friendly instructions.  These instructions
# will be pared down so as not to be redundant as that effort stabilizes.
#

#
# If you want to run cmake in an interactive mode that gives you prompts
# for build options, use `cmake -i` (or one of the graphical interfaces)
# Otherwise you will get the default build.  Here are some resources:
#
#     http://en.wikipedia.org/wiki/CMake
#     http://gearbox.sourceforge.net/gbx_doc_buildsys.html
#     http://www.ddj.com/cpp/184405251
#     http://www.elpauer.org/stuff/learning_cmake.pdf
#
# The default heuristic of this makefile is to look for a directory named
# `rebol` which is in the same directory that the rencpp main directory
# can be found.  If you wish to override this, specify:
#
#     -DRUNTIME_PATH=/wherever/you/put/runtime-source
#
# Clone your repository for rebol from:
#
#     https://github.com/metaeducation/ren-c
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
# %CMakeLists.txt is supposed to be compiler independent, so specify
# alternative compilers e.g. with:
#
#       -DCMAKE_CXX_COMPILER=/usr/bin/clang
#

project(rencpp)

cmake_minimum_required(VERSION 2.8)


# This is a C++11 project, but it should work on pre-2011 compilers
# If you are using gcc 4.6 or older, then use `--std=c++0x` instead of
# `--std=c++11`
#
# Note that this *should* work:
#
#     set(CMAKE_CXX_STANDARD 11)
#
# But for some reason it isn't working on the CMake with Travis CI.
#
# http://stackoverflow.com/questions/40877744/
#
set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")


# Unfortunately, the way this is written right now needs to explicitly call
# out the names of OBJ files, which vary between compilers.  To make this less
# long winded, make the variable OBJ a shortcut for the extension.
#
set(.OBJ ${CMAKE_C_OUTPUT_EXTENSION})


# Simplify appending flags to CMAKE_CXX_FLAGS

macro(add_cxx_flags flags)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flags}")
endmacro()


# Simplify appending flags to CMAKE_LINKER_FLAGS_DEBUG (for address sanitizer)
#
# https://stackoverflow.com/q/44320465/

macro(add_link_debug_flags flags)
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} ${flags}")
endmacro()



# Removing a flag string from CMAKE_CXX_FLAGS, fail silently if not present
#
# http://stackoverflow.com/a/18234926/211160

macro(try_remove_cxx_flag flag)
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


# Equivalent to `list(REMOVE_ITEM list item)` but checks to make sure the
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


# Default to looking for the runtime installation up one level
#
# We use absolute paths here to find almost all files so that the directories
# which inherit this file's settings vs `add_subdirectory()` will still work
#
# Also: A version of Ren which can be built without Red or Rebol and has some
# kind of formal parser that is C++ only is a concept for the future, but we
# can "fake it" for today by subsetting the Rebol code so that there is no
# evaluator offered...

if(NOT RUNTIME_PATH)
    set(RUNTIME_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../ren-c)
endif()


# If Ren-C was built with address sanitizer, it has to be linked with address
# sanitizer when executables are built with it.

if(SANITIZE MATCHES "yes")
    add_cxx_flags(-fno-omit-frame-pointer)
    add_cxx_flags(-fsanitize=address)

    add_link_debug_flags(-fno-omit-frame-pointer)
    add_link_debug_flags(-fsanitize=address)
endif()


# Turn up the warnings very high.
#
#     http://stackoverflow.com/a/9862800/211160
#
# Currently not adding in `-Wshadow`, because @HostileFork likes naming
# constructor arguments the same thing as the variables they initialize.
#
# For compiler identification notes:
#
#     http://stackoverflow.com/a/10055571/211160

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")

    # GCC and regular Clang or AppleClang share a lot of compatible switches

    add_cxx_flags(-Wall)
    add_cxx_flags(-Wsign-conversion)
    add_cxx_flags(-Wextra)
    add_cxx_flags(-Wcast-align)
    add_cxx_flags(-Wctor-dtor-privacy)
    add_cxx_flags(-Wdisabled-optimization)
    add_cxx_flags(-Wformat=2)
    add_cxx_flags(-Winit-self)
    add_cxx_flags(-Wmissing-declarations)
    add_cxx_flags(-Wmissing-include-dirs)
    add_cxx_flags(-Woverloaded-virtual)
    add_cxx_flags(-Wredundant-decls)
    add_cxx_flags(-Wsign-promo)
    add_cxx_flags(-Wstrict-overflow=5)
    add_cxx_flags(-Wswitch-default)
    add_cxx_flags(-Wundef)
    add_cxx_flags(-Wno-unused)
    add_cxx_flags(-pedantic)

    # Ren-C uses "Casts for the Masses" to get a literate, checkable cast for
    # both C and C++:
    #
    # http://blog.hostilefork.com/c-casts-for-the-masses/
    #
    # However, they're annoying to step through in debug code, and don't get
    # inlined in a debug build either...so they're slow too.  Hence the extra
    # "magic" is only in the *release* C++ builds, to catch mistakes which C
    # would not.  Hence they appear to break the rules in the debug build
    # when casting qualifiers are heeded, but are checked in release to be ok.
    #
    if(CMAKE_BUILD_TYPE MATCHES Debug)
        add_cxx_flags(-Wno-cast-qual)
    else()
        add_cxx_flags(-Wcast-qual)
    endif()

    # !!! These are new warnings being triggered by static_cast<bool> of an
    # Any_Value and it's not clear why they are happening.  Needs to be
    # tracked down and generate a MCVE.
    #
    add_cxx_flags(-Wno-address)
    add_cxx_flags(-Wno-nonnull-compare)

    # The C headers for Rebol use old-style C casts in some places, for
    # reasons of performance in the debug build (since there's a templated
    # function that doesn't get inlined)
    #
    #add_cxx_flags(-Wold-style-cast)

    # We encourage *everyone* who is building the project to use the `-Werror`
    # switch.  This converts messages that are considered "warnings" to the
    # status of being show-stopper errors.  Being rigorous about this helps
    # keep distracting warnings from accumulating in the build process over
    # time, drowning out important messages that should be heeded.
    #
    # However...this idealism isn't long-term compatible with telling the
    # compiler it can throw in as many errors as it can think of (via the
    # switches `-Wpedantic`, `-Wall`, and `-Wextra`).  Each time a new
    # compiler or new compiler version is used to build the project, it may
    # invent new and never before seen warnings that haven't been explicitly
    # disabled yet.
    #
    # Reports to help adjust the warnings to keep them at zero on relevant
    # platforms are preferred to turning off the switch.  But the switch is
    # available to use on the command line: `-DRIGOROUS=no`

    if(NOT DEFINED(RIGOROUS) OR RIGOROUS)
        add_cxx_flags(-Werror)
    endif()

    # Rebol is written in "old" ANSI C, so when we include its headers then it
    # can have some complaints if the warnings are set too high.  We tone the
    # strictness down a little so the headers will work.

    if(RUNTIME STREQUAL "rebol")
        add_cxx_flags(-Wno-unknown-pragmas)
        add_cxx_flags(-Wno-cast-align)
        try_remove_cxx_flag(-Wold-style-cast)
    endif()

endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")

    # using regular Clang or AppleClang

    # mismatched-tags is a "questionable" warning when specializations of
    # struct and class are mixed.  Such a mixture is legal C++ and header
    # files are not consistent in their usage:
    #
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=58025
    #
    # Seems to not be an issue in GCC for some reason but showing up in clang

    add_cxx_flags(-Wno-mismatched-tags)

elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")

    # using GCC

    add_cxx_flags(-Wlogical-op)
    add_cxx_flags(-Wnoexcept)
    add_cxx_flags(-Wstrict-null-sentinel)

elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

    # using Visual Studio C++ (note C++11 and beyond support on by default)

    # Historically, Rebol triggers its 64-bitness on either __LLP64__ or
    # __LP64__ being defined.  MSVC doesn't define this itself the way GCC
    # does, so the Rebol makefiles do it.  We're not using the Rebol makefile
    # here, hence we have to do it manually.
    
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-D__LLP64__)
    endif()

    # If you turn the warning level up, Qt sets off these warnings in MSVC
    # and there's not much you can do about it but disable them.
     
    if(CLASSLIB_QT EQUAL 1)
        add_cxx_flags(/wd4180)
    endif()

    add_cxx_flags(/wd4244)
    add_cxx_flags(/wd4267)

else()

    # !!! If you're not using GCC, Clang, or MSVC then you're pretty much on
    # your own.  Pull requests welcome, however.
    #
    # e.g. if(CMAKE_CXX_COMPILER_ID MATCHES "Intel")...

endif()

#
# !!! Rebol needs to know the endianness.  For now assume all Ren Garden
# builds are for little endian machines.
#
add_definitions(-DENDIAN_LITTLE)

# Include files intended to be used by clients of the binding are under
# %include/rencpp/ so that includes are prefixed as:
#
#     #include "rencpp/feature.hpp"
#
# The implementation cpp files and *internal* headers are in %src/

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)


# We build all the sources in the %src/ directory regardless of which
# binding is being used.

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)


# .h and .hpp "include" files aren't technically necessary to mention here
# for the build to succeed.  But you have to point them out to CMake if you
# want them indexed in your QtCreator/VisualStudio/Xcode project tree.
#
# "Globbing" is to gather up files using a pattern.  While convenient,
# note the caveat here about how globbing doesn't record dependencies.
# So you have to process file additions and removals with a re-run
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


# The C++ standard library doesn't have any Unicode support to speak of,
# but Qt does.  Also several of the Qt Toolkit's types have interesting
# mappings to Ren.  So you can build against the Qt library in addition to
# (or in lieu of) building against the standard C++ library.  Or don't use
# either if you want to do all your Unicode and I/O through Ren.
#
# e.g. this is for for things like converting Ren values into `QString` vs.
# `std::string` (or both, or neither).

if(GARDEN AND NOT (CLASSLIB_QT EQUAL 1))

    # Technically speaking Ren Garden *could* fall back on some helper stubs
    # if not built for Qt.  But we're experimenting here with making things
    # as integrative and "pretty" as possible...

    # https://github.com/hostilefork/rencpp/issues/23

    message(FATAL_ERROR "CLASSLIB_QT must be 1 if building Ren Garden")

endif()


if((NOT DEFINED CLASSLIB_QT) OR (CLASSLIB_QT EQUAL 0))

    # Assume we don't want the Qt classlib if none specified

    add_definitions(-DREN_CLASSLIB_QT=0)

elseif(CLASSLIB_QT EQUAL 1)

    # Enable code conditional under `#if REN_CLASSLIB_QT == 1` in the source

    add_definitions(-DREN_CLASSLIB_QT=1)

    # According to this page, this is how to use find_package with Qt
    #
    # https://blog.kitware.com/cmake-finding-qt5-the-right-way/ 
    #
    # We use the Core library, which is needed for types like `QString` and
    # `QByteArray` and other basics:
    #
    #     http://www.kdab.com/using-cmake-with-qt-5/
    #
    # Also Gui, which has the basic windowing and graphics services but
    # not the widgets (those are in Qt5Widgets).  It is currently used for
    # the experiment of `QImage` conversions of Ren image values, which has
    # only very primitive support at time of writing.
    #
    find_package(Qt5 COMPONENTS Core Gui REQUIRED)

    # If the `find_package()` calls above succeed then CMake will not stop
    # running, but rather we get to this point with several Qt-related
    # variables defined.  We add some of those automagically defined things
    # to our include and library paths.

    include_directories(${Qt5Core_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS})

    add_cxx_flags(
        "${Qt5Core_EXECUTABLE_COMPILE_FLAGS}"
        "${Qt5Gui_EXECUTABLE_COMPILE_FLAGS}"
    )

    set(LIBS_ALL ${LIBS_ALL} Qt5::Core Qt5::Gui)

    # Behavior since CMake 2.8.11 is to automatically include Qt5::WinMain when
    # the `add_executable()` has been configured to a WIN32 graphical target:
    #
    #     https://cmake.org/cmake/help/git-master/policy/CMP0020.html
    #
    # We link console test executables only when Qt support is on, so automatic
    # linking to WinMain is not relevant for baseline Ren/C++ (yet).  Clients
    # like Ren Garden may be concerned (except it already requires 2.8.11), or
    # the tests may care if there are ever tests that build windowed apps
    # for some reason.
    #
    # In the meantime, we disable this warning so that Ren/C++ can still build
    # with CMake 2.8, but without issuing warnings if you use a later version.

    if(POLICY CMP0020)
        cmake_policy(SET CMP0020 NEW)
    endif()

else()

    message(FATAL_ERROR "CLASSLIB_QT must be 0 or 1 if defined")

endif()


# We need to get at the C includes from Rebol to call its internal
# functions to initialize the interpreter and implement the necessary
# hooks by calls to those internal functions.  This does not use the
# "RL_" API,

message(STATUS "Runtime path is ${RUNTIME_PATH}")


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


# Put the Rebol headers in so you can search them in the project

file(GLOB INC_REBOL ${RUNTIME_PATH}/src/include/*.h)


# Add any extra includes for the project browser

file(GLOB INC_BINDING ${CMAKE_CURRENT_SOURCE_DIR}/rebol-binding/*.h*)
set(INC_LIST ${INC_LIST} ${INC_BINDING} ${INC_REBOL})


# It's important to make sure you're set to the right platform, as it
# affects endianness and other things.  These flags were largely moved
# in Ren/C to being auto-generated, hence they're not set in the
# %reb-config.h any longer.  Review the ramifications of this.

add_definitions(-DENDIAN_LITTLE)

if(WIN32)
    add_definitions(-DTO_WINDOWS)

    # Windows natively supports wide characters, and UNICODE signals that
    # to the Rebol build.

    add_definitions(-DUNICODE)

    # CMake is able to detect whether you are compiling for a 64-bit or
    # 32-bit platform using this test.

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-DTO_WINDOWS_X64)
    else()
        add_definitions(-DTO_WINDOWS_X86)
    endif()

    # Rebol defines a number of macros for testing types, like `IS_BLOCK()`
    # and `IS_SET_WORD()`.  One of those is `IS_ERROR()`, and it happens
    # that %Winerror.h defines that too:
    #
    #     BOOL IS_ERROR(HRESULT Status);
    #
    # Rebol worked around this basically by `#undef IS_ERROR`, and for
    # some reason signals the workaround with the define `OS_DEFS`.
    # Including this for now, but mentioning why it's here in case some
    # better solution has arisen and is looked into later.

    add_definitions(-DOS_DEFS)

    # Something about how CMake includes Qt seems to throw in a -fPIC
    # flag to the compiler options, which is meaningless on Windows and
    # MinGW complains...many have deemed it a "useless warning":
    #
    #     http://stackoverflow.com/questions/16708148/
    #
    # Because it's not this CMake file that's adding `-fPIC`, it's hard
    # to guarantee that this removal is going to be "timed" after the
    # addition, but it seems to work.  If it does not work, your only
    # option is probably to comment out the add_definitions(-Werror)
    # earlier in this file, as MinGW has no disable code for the warning.

    try_remove_cxx_flag(-fPIC)

elseif(APPLE)

    add_definitions(-DTO_OSX)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-DTO_OSX_X64)
    else()
        add_definitions(-DTO_OSX_X86)
    endif()

else()

    add_definitions(-DTO_LINUX)

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-DTO_LINUX_X64)
    else()
        add_definitions(-DTO_LINUX_X86)
    endif()

endif()


# Try and get all the guts out of the Rebol Core executable to include in the
# binding.  Take out the main() so that the program you link with the binding
# is the entry point.
#
# Note: there is a GLOB RELATIVE in CMake to do it relative to a path, but
# it doesn't work on Windows.

get_filename_component(EXTRA_OBJ_PATH ${RUNTIME_PATH}/make/objs/ REALPATH)

file(GLOB_RECURSE EXTRA_OBJS ${EXTRA_OBJ_PATH}/*${.OBJ})

ensure_remove_item("EXTRA_OBJS" ${EXTRA_OBJ_PATH}/host-main${.OBJ})
ensure_remove_item("EXTRA_OBJS" ${EXTRA_OBJ_PATH}/host-stdio${.OBJ})


# R3-Alpha had an an extension system called "host kit", which was a way
# of providing I/O or GUI services that Rebol would call out to.  This
# meant the core did not have things hardcoded into it like how to
# print to the screen or speak to the network.
#
# The system was a sort of grab-bag of "ioctl"-like functions with no
# particularly interesting design.  However, these services were
# implemented by REBOL Technologies and others for several platforms and
# have been used in deployment.  These provide services ranging from
# sockets and HTTPS to the windows clipboard and serial ports.
#
# Ren/C++ doesn't get involved in overriding these, but it has to get
# involved in hooking the stdio.  So we remove the OBJ containing Rebol's
# implementation of talking to STDIN/STDOUT so that it may be replaced
# with one that bridges to C++ <iostream> classes.

if(WIN32)
    ensure_remove_item("EXTRA_OBJS" ${EXTRA_OBJ_PATH}/windows/dev-stdio${.OBJ})
else()
    ensure_remove_item("EXTRA_OBJS" ${EXTRA_OBJ_PATH}/posix/dev-stdio${.OBJ})
endif()


# Rebol depends on dlopen and other calls in "libDl".  Cmake can
# supposedly find that for you automatically with this variable.
# http://en.wikipedia.org/wiki/Dynamic_loading

set(LIBS_ALL ${LIBS_ALL} ${CMAKE_DL_LIBS})


# Rebol depends on WinSock 2 sockets library when built on Windows.

if(WIN32)
    set(LIBS_ALL ${LIBS_ALL} ws2_32)
endif()


# Though it's early yet to decide exactly how the project will be packaged,
# CMake's static libraries at least speed up the build for now.

add_library(
    RenCpp STATIC
    ${EXTRA_OBJS} ${SRC_LIST} ${INC_LIST}
)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_link_libraries(RenCpp ${LIBS_ALL})
else()
    target_link_libraries(RenCpp ${LIBS_ALL} -lstdc++ -lm)
endif()


# %examples/ has its own CMakeLists.txt, with settings pertinent to each
# case (e.g. OpenGL configuration, etc.), but inherits the settings
# from this file.
#
#     http://stackoverflow.com/q/7979963/211160

add_subdirectory(examples)


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
# %tests/ has a separate CMakeLists.txt, for structured unit tests.  These
# use the Catch framework, which is held as a CMake-managed dependency in
# %external/catch/

enable_testing()

add_subdirectory(external/catch)

add_subdirectory(tests)

add_subdirectory(doxygen)
