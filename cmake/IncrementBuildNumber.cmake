# Increment a persistent build number and generate a tiny header consumed by Main.cpp.
# Expected variables:
#   BUILD_NUMBER_FILE  path to build_number.txt
#   OUTPUT_HEADER      path to generated BuildInfo.h
#   PROJECT_VERSION    semantic base version (e.g. 1.0.0)

if(NOT DEFINED BUILD_NUMBER_FILE OR BUILD_NUMBER_FILE STREQUAL "")
    message(FATAL_ERROR "BUILD_NUMBER_FILE is required")
endif()

if(NOT DEFINED OUTPUT_HEADER OR OUTPUT_HEADER STREQUAL "")
    message(FATAL_ERROR "OUTPUT_HEADER is required")
endif()

if(NOT DEFINED PROJECT_VERSION OR PROJECT_VERSION STREQUAL "")
    set(PROJECT_VERSION "0.0.0")
endif()

if(EXISTS "${BUILD_NUMBER_FILE}")
    file(READ "${BUILD_NUMBER_FILE}" BUILD_NUMBER_RAW)
    string(STRIP "${BUILD_NUMBER_RAW}" BUILD_NUMBER_RAW)
else()
    set(BUILD_NUMBER_RAW "0")
endif()

if(NOT BUILD_NUMBER_RAW MATCHES "^[0-9]+$")
    set(BUILD_NUMBER_RAW "0")
endif()

math(EXPR BUILD_NUMBER "${BUILD_NUMBER_RAW} + 1")

get_filename_component(BUILD_NUMBER_DIR "${BUILD_NUMBER_FILE}" DIRECTORY)
if(BUILD_NUMBER_DIR)
    file(MAKE_DIRECTORY "${BUILD_NUMBER_DIR}")
endif()

get_filename_component(OUTPUT_DIR "${OUTPUT_HEADER}" DIRECTORY)
if(OUTPUT_DIR)
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")
endif()

file(WRITE "${BUILD_NUMBER_FILE}" "${BUILD_NUMBER}\n")

set(VERSION_WITH_BUILD "${PROJECT_VERSION}.${BUILD_NUMBER}")

set(HEADER_CONTENT "#pragma once\n\n#define ENCORE_BUILD_NUMBER ${BUILD_NUMBER}\n#define ENCORE_VERSION_WITH_BUILD \"${VERSION_WITH_BUILD}\"\n")
file(WRITE "${OUTPUT_HEADER}" "${HEADER_CONTENT}")

message(STATUS "Encore build number: ${BUILD_NUMBER} (version ${VERSION_WITH_BUILD})")
