# Copyright (c) 2018 Marcel Ehrhardt <marcel.ehrhardt@fu-berlin.de>
# Copyright (c) 2018 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

project(ham LANGUAGES CXX)
cmake_minimum_required(VERSION 3.2 FATAL_ERROR) # TODO verfify

# set output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
message(STATUS "CMAKE_BINARY_DIR: " ${CMAKE_BINARY_DIR})

### thirdparty dependencies

# Boost
find_package(Boost 1.40 COMPONENTS program_options system REQUIRED)
add_library(boost_library INTERFACE)
target_include_directories (boost_library INTERFACE ${Boost_INCLUDE_DIRS})
target_link_libraries (boost_library INTERFACE ${Boost_LIBRARIES})

# MPI
find_package(MPI) # not required
if (MPI_FOUND)
	add_library (mpi_library INTERFACE)
	target_include_directories (mpi_library INTERFACE ${MPI_CXX_INCLUDE_PATH})
	target_compile_options (mpi_library INTERFACE ${MPI_CXX_COMPILE_FLAGS})
	target_link_libraries (mpi_library INTERFACE ${MPI_CXX_LIBRARIES})
endif ()

# Intel SCIF (for Xeon Phi accelerators with KNC architecture)
find_file(SCIF_HEADER_FILE "scif.h")
if (SCIF_HEADER_FILE)
	set(SCIF_FOUND ON)
	get_filename_component(SCIF_INCLUDE_DIR "${SCIF_HEADER_FILE}" DIRECTORY)
	message(STATUS "Found SCIF: ${SCIF_HEADER_FILE}")

	add_library (scif_library INTERFACE)
	target_include_directories (scif_library INTERFACE ${SCIF_INCLUDE_DIR})
else ()
	message(STATUS "Could NOT find SCIF (missing: scif.h)")
endif ()

# tell the compiler to be strict
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DHAM_DEBUG_ON")

add_subdirectory(thirdparty/bmt ${CMAKE_CURRENT_BINARY_DIR}/build.noma_bmt)
add_subdirectory(src)
