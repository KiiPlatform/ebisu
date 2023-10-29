cmake_minimum_required(VERSION 3.12)
PROJECT(khc C)
INCLUDE(${CMAKE_SOURCE_DIR}/../version/EbisuVersion.cmake)
set_git_version_and_generate_header()

SET(CMAKE_C_FLAGS "-Wall")

OPTION(KHC_TRACE "Trace KHC activity to stdout" ON)
IF(KHC_TRACE)
  ADD_DEFINITIONS(-DKHC_TRACE)
ENDIF()

SET(CMAKE_C_FLAGS_RELEASE "-O2")
SET(CMAKE_C_FLAGS_DEBUG "-g")

FILE(GLOB srcs src/*.c)
FILE(GLOB hdrs include/*.h)
ADD_LIBRARY(khc SHARED ${srcs})
TARGET_INCLUDE_DIRECTORIES(khc PUBLIC ./include)
INSTALL(FILES ${hdrs} DESTINATION include)
INSTALL(TARGETS khc DESTINATION lib)
