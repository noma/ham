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

# CLI11-1.6.2 (header only)
add_library(cli11_library INTERFACE)
target_include_directories (cli11_library INTERFACE "thirdparty/CLI11-1.6.2/include")

# Boost Preprocessor (header only)
add_library(boost_library INTERFACE)
target_include_directories (boost_library INTERFACE "thirdparty/boost-1.69.0/include")

# MPI
find_package(MPI) # not required
if (MPI_FOUND)
	add_library(mpi_library INTERFACE)
	target_include_directories(mpi_library INTERFACE ${MPI_CXX_INCLUDE_PATH})
	target_compile_options(mpi_library INTERFACE ${MPI_CXX_COMPILE_FLAGS})
	target_link_libraries(mpi_library INTERFACE ${MPI_CXX_LIBRARIES})
endif ()

# Intel SCIF (for Xeon Phi accelerators with KNC architecture)
find_file(SCIF_HEADER_FILE "scif.h")
if (SCIF_HEADER_FILE)
	set(SCIF_FOUND ON)
	get_filename_component(SCIF_INCLUDE_DIR "${SCIF_HEADER_FILE}" DIRECTORY)
	message(STATUS "Found SCIF: ${SCIF_HEADER_FILE}")

	add_library(scif_library INTERFACE)
	target_include_directories(scif_library INTERFACE ${SCIF_INCLUDE_DIR})
else ()
	message(STATUS "Could NOT find SCIF (missing: scif.h)")
endif ()

find_file(VEO_HEADER_FILE "ve_offload.h" "/opt/nec/ve/veos/include/")
if (VEO_HEADER_FILE)
	set(VEO_FOUND ON)
	get_filename_component(VEO_INCLUDE_DIR "${VEO_HEADER_FILE}" DIRECTORY)
	message(STATUS "Found VEO: ${VEO_HEADER_FILE}")

	add_library(veo_library INTERFACE)
	target_include_directories(veo_library INTERFACE ${VEO_INCLUDE_DIR})
	target_link_libraries(veo_library INTERFACE "-L/opt/nec/ve/veos/lib64 -Wl,-rpath=/opt/nec/ve/veos/lib64 -lveo")
else ()
	message(STATUS "Could NOT find VEO (missing: ve_offload.h)")
endif ()

# tell the compiler to be strict
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wextra")

add_subdirectory(thirdparty/bmt ${CMAKE_CURRENT_BINARY_DIR}/build.noma_bmt)
add_subdirectory(src)
