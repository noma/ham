# Copyright (c) 2017 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(libnoma_misc LANGUAGES CXX)

# header only debug library
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# header only library 
add_library(noma_debug INTERFACE)
# NOTE: we want to use '#include "noma/debug/debug.hpp"', not '#include "debug.hpp"'
target_include_directories(noma_debug INTERFACE include) 
target_compile_features(noma_debug INTERFACE )

# static memory library
add_library(noma_memory STATIC src/noma/memory/memory.cpp)
# NOTE: we want to use '#include "noma/memory/memory.hpp"', not '#include "memory.hpp"'
target_include_directories(noma_memory PUBLIC include) 

set_target_properties(noma_memory PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

