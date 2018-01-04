cmake_minimum_required(VERSION 2.8)
project(catch_builder CXX)

include(ExternalProject)

find_package(Git REQUIRED)

ExternalProject_Add(catch
    PREFIX ${CMAKE_BINARY_DIR}/external/catch/upstream
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    TIMEOUT 10
    LOG_DOWNLOAD ON
    UPDATE_COMMAND ${GIT_EXECUTABLE} pull
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
)

# Expose required variable (CATCH_INCLUDE_DIR) to parent scope
ExternalProject_Get_Property(catch SOURCE_DIR)
set(CATCH_INCLUDE_DIR
    ${SOURCE_DIR}/single_include CACHE INTERNAL
    "Path to include folder for Catch"
)
