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


# These rigid warnings thrown in are too strict for building Qt5 projects

remove_cxx_flag("-Wsign-conversion")
remove_cxx_flag("-Wctor-dtor-privacy")
remove_cxx_flag("-Wswitch-default")
remove_cxx_flag("-Wmissing-declarations")


# Resource files need to be compiled, and then the compiled resource is set
# into the ${RESOURCE_ADDED} variable

qt5_add_resources(RESOURCE_ADDED workbench.qrc)


# Source files included in the executable

add_executable(
    workbench

    # Resource in compiled form

    ${RESOURCE_ADDED}

    # Source files

    main.cpp
    mainwindow.cpp
    replpad.cpp
    renconsole.cpp
    watchlist.cpp
    rensyntaxer.cpp
    renshell.cpp

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
)


# Linker settings

target_link_libraries(
    workbench 
    RenCpp
    Qt5::Widgets 
)

