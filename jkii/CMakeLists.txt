cmake_minimum_required(VERSION 3.12)
PROJECT(jkii C)
INCLUDE(${CMAKE_SOURCE_DIR}/../version/EbisuVersion.cmake)
set_git_version_and_generate_header()

SET(CMAKE_C_FLAGS "-Wall")

SET(CMAKE_C_FLAGS_RELEASE "-O2")
SET(CMAKE_C_FLAGS_DEBUG "-g")

FILE(GLOB srcs src/*.c libs/jsmn/*.c)
FILE(GLOB hdrs include/*.h libs/jsmn/jsmn.h)
ADD_LIBRARY(jkii SHARED ${srcs})
TARGET_INCLUDE_DIRECTORIES(jkii PUBLIC ./include)
TARGET_INCLUDE_DIRECTORIES(jkii PRIVATE ./libs/jsmn)
INSTALL(FILES ${hdrs} DESTINATION include)
INSTALL(TARGETS jkii DESTINATION lib)
