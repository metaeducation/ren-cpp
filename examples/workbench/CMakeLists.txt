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


# Instruct CMake to run moc automatically when needed.

set(CMAKE_AUTOMOC ON)


# Find the QtWidgets library

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Network REQUIRED)


# These rigid warnings thrown in are too strict for building Qt5 projects

remove_cxx_flag("-Wsign-conversion")
remove_cxx_flag("-Wctor-dtor-privacy")
remove_cxx_flag("-Wswitch-default")
remove_cxx_flag("-Wmissing-declarations")


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


# GUI Type
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
    renconsole.cpp
    watchlist.cpp
    rensyntaxer.cpp
    renshell.cpp
    renpackage.cpp

    # Include files - not technically needed for the build but good to have
    # a mention of because then the generated project file will show them in
    # the file browser

    mainwindow.h
    renconsole.h
    replpad.h
    fakestdio.h
    watchlist.h    
    syntaxer.h
    rensyntaxer.h
    renshell.h
    renpackage.h
)


# Linker settings

target_link_libraries(
    workbench 
    RenCpp
    Qt5::Widgets
    Qt5::Network
)

