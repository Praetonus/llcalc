# Copyright 2015 Benoît Vey

cmake_minimum_required(VERSION 2.8)

project(calc)

set(EXECUTABLE_OUTPUT_PATH bin/${CMAKE_BUILD_TYPE})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_LD_FLAGS} -lLLVM -lrt -ldl -lcurses -lpthread -lz -lm")

file(GLOB source_files *.cpp)

add_executable(calc ${source_files})
