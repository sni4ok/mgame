cmake_minimum_required(VERSION 3.10)

SET(CMAKE_COLOR_MAKEFILE ON)
SET(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "" FORCE)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

project(mgame)

set(MAKE_PLAZA OFF CACHE BOOL "plaza")
set(MAKE_SQL OFF CACHE BOOL "sql")

IF(NOT IS_CLANG)
    set(IS_CLANG OFF CACHE BOOL "clang" FORCE)
ENDIF(NOT IS_CLANG)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -march=native -std=c++23")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

IF("${IS_CLANG}")

    set(CMAKE_C_COMPILER "/usr/bin/clang" CACHE STRING "clang compiler" FORCE)
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE STRING "clang compiler" FORCE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-braces")
    add_definitions(-DCLANG_COMPILER)
    message("clang mode")

ELSE("${IS_CLANG}")

    set(CMAKE_C_COMPILER "/usr/bin/gcc" CACHE STRING "gcc compiler" FORCE)
    set(CMAKE_CXX_COMPILER "/usr/bin/g++" CACHE STRING "gcc compiler" FORCE)
    message("gcc mode")

ENDIF("${IS_CLANG}")

SUBDIRS(evie makoa ying tyra viktor alco)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

