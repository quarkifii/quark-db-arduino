# Copyright (c) 2024-2026 Quarkifi Technologies Pvt Ltd
# SPDX-License-Identifier: MIT
# See LICENSE at the repository root for the full license text.

cmake_minimum_required(VERSION 3.16)
project(QuarkDBTests CXX)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- GoogleTest -------------------------------------------------------------
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- ArduinoJson (header only) ---------------------------------------------
# Prefer the copy installed next to this library in the Arduino libraries
# folder so tests use the same version as the sketches; otherwise download.
set(ARDUINOJSON_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ArduinoJson/src"
    CACHE PATH "Path to ArduinoJson's src directory")
if(NOT EXISTS "${ARDUINOJSON_SRC_DIR}/ArduinoJson.h")
  FetchContent_Declare(arduinojson
    URL https://github.com/bblanchon/ArduinoJson/archive/refs/tags/v7.3.0.tar.gz)
  FetchContent_GetProperties(arduinojson)
  if(NOT arduinojson_POPULATED)
    FetchContent_Populate(arduinojson)
  endif()
  set(ARDUINOJSON_SRC_DIR "${arduinojson_SOURCE_DIR}/src" CACHE PATH "" FORCE)
endif()
message(STATUS "ArduinoJson: ${ARDUINOJSON_SRC_DIR}")

set(QUARKDB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")
file(GLOB_RECURSE QUARKDB_SOURCES CONFIGURE_DEPENDS "${QUARKDB_SRC_DIR}/*.cpp")
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp")

enable_testing()
include(GoogleTest)

# Build the library + tests once per target platform, since QuarkDB has
# ESP8266- and ESP32-specific code paths (e.g. list enumeration).
function(add_quarkdb_test_target platform)
  set(target quarkdb_tests_${platform})
  add_executable(${target}
    ${QUARKDB_SOURCES}
    ${TEST_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_globals.cpp)
  string(TOUPPER ${platform} PLATFORM_DEFINE)
  target_compile_definitions(${target} PRIVATE
    ${PLATFORM_DEFINE}
    QUARKDB_TEST_PLATFORM="${platform}"
    ARDUINOJSON_ENABLE_ARDUINO_STRING=1
    ARDUINOJSON_ENABLE_ARDUINO_PRINT=1
    ARDUINOJSON_ENABLE_ARDUINO_STREAM=1)
  # mocks/ comes first so <Arduino.h>, <FS.h>, <SPIFFS.h>, <SPI.h> resolve to the doubles.
  target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${QUARKDB_SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/unit)
  # Third-party headers: don't report their warnings as ours.
  target_include_directories(${target} SYSTEM PRIVATE ${ARDUINOJSON_SRC_DIR})
  # No --gc-sections here (unlike the Arduino toolchain), so a method that is
  # declared and referenced but never defined fails the test build.
  # Warnings are errors so the library and tests stay warning-free.
  target_compile_options(${target} PRIVATE -Wall -Werror)
  # Memory checking is mandatory: every build runs under ASan + UBSan, and
  # any UB report aborts the test run instead of just printing a warning.
  target_compile_options(${target} PRIVATE
    -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g)
  target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  target_link_libraries(${target} PRIVATE GTest::gtest_main)
  gtest_discover_tests(${target} TEST_PREFIX "${platform}.")
endfunction()

add_quarkdb_test_target(esp8266)
add_quarkdb_test_target(esp32)
