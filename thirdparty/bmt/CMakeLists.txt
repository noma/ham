# Copyright (c) 2017 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(libnoma_bmt LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# header only library 
add_library(noma_bmt INTERFACE)
# NOTE: we want to use '#include "noma/bmt/bmt.hpp"', not '#include "bmt.hpp"'
target_include_directories(noma_bmt INTERFACE include) 
target_compile_features(noma_bmt INTERFACE )

#set_target_properties(noma_bmt PROPERTIES
#    CXX_STANDARD 11
#    CXX_STANDARD_REQUIRED YES
#    CXX_EXTENSIONS NO
#)

add_subdirectory(src)
