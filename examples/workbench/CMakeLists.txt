#
# This file inherits the definitions for the Ren binding from root
#

#
# For the way to include Qt in CMake that is in fashion *this* week (on the
# site you visit to read Qt documentation for the next few days until they
# change that again too) see here:
#
#    http://doc.qt.io/qt-5/cmake-manual.html
# 

cmake_minimum_required(VERSION 2.8.11)


# Find includes in corresponding build directories

set(CMAKE_INCLUDE_CURRENT_DIR ON)


# Add includes for libraries such as optional in the "include" directory

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)


# Instruct CMake to run moc automatically when needed.

set(CMAKE_AUTOMOC ON)


# Ren Garden needs the Qt Widgets (tab controls, list controls) which will
# automatically bring in the GUI and Qt Core.

find_package(Qt5Widgets REQUIRED)


# Qt's network is only being used experimentally in Ren Garden.  Linkage to
# it was added as a way to try and think about bringing in module updates for
# Rebol code...but that was before Ren/C was created and brought in support
# for HTTPS from the Atronix and Saphirion builds.
#
# It can be removed if there is a compelling reason to do so, but it may be
# useful in implementing (for instance) an experimental alternative port
# system to replace the existing code.

find_package(Qt5Network REQUIRED)


# These rigid warnings from Ren/C++ are too strict for building Qt5 projects

try_remove_cxx_flag("-Wsign-conversion")
try_remove_cxx_flag("-Wctor-dtor-privacy")
try_remove_cxx_flag("-Wswitch-default")
try_remove_cxx_flag("-Wmissing-declarations")


# These workarounds were added as an experiment for getting Ren Garden to be
# able to have the EXE and DLLs packed together with MoleBox:
#
#    http://molebox.com/
#
# Among application virtualizers for Windows, it's relatively lightweight.
# And with the workarounds, it *seems* to be viable on WinXP through Win8.
# In any case, even if we don't use MoleBox specifically, the workarounds
# may be necessary in other virtual solutions.

if(GARDEN_BOXED)
   if(NOT WIN32)
       message(FATAL "Can only build Ren Garden for Boxing on Windows")
   endif()
   add_cxx_flags("-DREN_GARDEN_BOXED")
endif()



# Resource files need to be compiled, and then the compiled resource is set
# into the ${RESOURCE_ADDED} variable

qt5_add_resources(RESOURCE_ADDED workbench.qrc)


# CMake is able to automatically do configuration for the linker in order
# to get the right GUI type if you mention the platform (as of CMake 2.8.11,
# which is a dependency for this build file).
#
# However we can't put an if statement in the middle of `add_executable()`.
# So instead of putting `add_executable()` inside of a conditional we make
# a `GUI_TYPE` variable and include that.

if(WIN32)
    set(GUI_TYPE WIN32)
    message(STATUS "win32 guitype")
endif(WIN32)

if(APPLE)
    set(GUI_TYPE MACOSX_BUNDLE)
endif(APPLE)

if(WIN32)
   set(WINDOWS_RC "workbench.rc")
endif()


# Source files included in the executable

add_executable(
    workbench

    ${GUI_TYPE}

    # Resource in compiled form (and .RES if windows for icon)

    ${RESOURCE_ADDED}
    ${WINDOWS_RC}

    # Source files

    main.cpp
    mainwindow.cpp
    replpad.cpp
    fakestdio.cpp
    evaluator.cpp
    renconsole.cpp
    watchlist.cpp
    renshell.cpp
    renpackage.cpp
    valueexplorer.cpp

    # Include files - not technically needed for the build but good to have
    # a mention of because then the generated project file will show them in
    # the file browser

    mainwindow.h
    evaluator.h
    renconsole.h
    replpad.h
    fakestdio.h
    watchlist.h    
    renshell.h
    renpackage.h
    valueexplorer.h
)


# Linker settings

target_link_libraries(
    workbench 
    RenCpp
    Qt5::Widgets
    Qt5::Network
)

