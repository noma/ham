# Copyright (c) 2018 Marcel Ehrhardt <marcel.ehrhardt@fu-berlin.de>
# Copyright (c) 2018-2019 Matthias Noack <ma.noack.pr@gmail.com>
#
# See accompanying file LICENSE and README for further information.

project(ham LANGUAGES CXX)
cmake_minimum_required(VERSION 3.2 FATAL_ERROR) # TODO verfify

# set output directory for executable, dynamic libs (.so), and static libs/archives (.a)
message(STATUS "Output directory: " ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# add cmake subdirector to CMake module path
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

# detect parent scope, i.e. this project is used via add_subdirectory()
set(HAM_BUILD_EXECUTABLES_DESCRIPTION "Build executables, if disabled, only the libraries will be built.")
get_directory_property(HAM_HAS_PARENT PARENT_DIRECTORY)
if (HAM_HAS_PARENT)
	# used as subdirectory, so export some variables, and disable executable builds by default
	message(STATUS "Parent directory detected: HAM_BUILD_EXECUTABLES_DESCRIPTION default is OFF")
	option(HAM_BUILD_EXECUTABLES ${HAM_BUILD_EXECUTABLES_DESCRIPTION} OFF)
else ()
	# standalong build, enable exectuable builds by default
	option(HAM_BUILD_EXECUTABLES ${HAM_BUILD_EXECUTABLES_DESCRIPTION} ON)
endif ()

# set default build type to "Release"
set(HAM_DEFAULT_BUILD_TYPE "Release")
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${HAM_DEFAULT_BUILD_TYPE}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${HAM_DEFAULT_BUILD_TYPE}" CACHE
			STRING "Choose the type of build." FORCE)
	# set the possible values of build type for CMake-GUI
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
			"Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif ()

# detect if NEC compiler is used (for VEO backend)
get_filename_component(HAM_CXX_COMPILER_EXE ${CMAKE_CXX_COMPILER} NAME)
message(STATUS "C++ compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ compiler exe: ${HAM_CXX_COMPILER_EXE}")
if (HAM_CXX_COMPILER_EXE STREQUAL "nc++")
	set(HAM_NEC_COMPILER_DETECTED ON)
        if (HAM_HAS_PARENT)
                set(HAM_NEC_COMPILER_DETECTED ON PARENT_SCOPE)
        endif ()

	message(STATUS "HAM_NEC_COMPILER_DETECTED: ${HAM_NEC_COMPILER_DETECTED}")
endif ()
# helper function for NEC Backend
include(CreateStaticVeorunBinary)

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

	set(MPI_FOUND ON)
	if (HAM_HAS_PARENT)
		set(MPI_FOUND ${MPI_FOUND} PARENT_SCOPE)
	endif ()
endif ()

# Intel SCIF (for Xeon Phi accelerators with KNC architecture)
find_file(SCIF_HEADER_FILE "scif.h")
if (SCIF_HEADER_FILE)
	set(SCIF_FOUND ON)
	if (HAM_HAS_PARENT)
		set(SCIF_FOUND ${SCIF_FOUND} PARENT_SCOPE)
	endif ()

	get_filename_component(SCIF_INCLUDE_DIR "${SCIF_HEADER_FILE}" DIRECTORY)
	message(STATUS "Found SCIF: ${SCIF_HEADER_FILE}")

	add_library(scif_library INTERFACE)
	target_include_directories(scif_library INTERFACE ${SCIF_INCLUDE_DIR})
else ()
	message(STATUS "Could NOT find SCIF (missing: scif.h)")
endif ()

# NEC VEO (NEC Vector Engine)
find_file(VEO_HEADER_FILE "ve_offload.h" "/opt/nec/ve/veos/include/")
if (VEO_HEADER_FILE)
	set(VEO_FOUND ON)
	if (HAM_HAS_PARENT)
		set(VEO_FOUND ${VEO_FOUND} PARENT_SCOPE)
	endif ()

	get_filename_component(VEO_INCLUDE_DIR "${VEO_HEADER_FILE}" DIRECTORY)
	message(STATUS "Found VEO: ${VEO_HEADER_FILE}")

	add_library(veo_library INTERFACE)
	target_include_directories(veo_library INTERFACE ${VEO_INCLUDE_DIR})
	target_link_libraries(veo_library INTERFACE "-L/opt/nec/ve/veos/lib64 -Wl,-rpath=/opt/nec/ve/veos/lib64 -lveo")

	add_library(veos_system_libraries INTERFACE)
	target_link_libraries(veos_system_libraries INTERFACE "-lpthread -lveio")

	set(MK_VEORUN_STATIC_LIBS "-lpthread -lveio")
	if (HAM_HAS_PARENT)
		set(MK_VEORUN_STATIC_LIBS ${MK_VEORUN_STATIC_LIBS} PARENT_SCOPE)
	endif ()

	# TODO: try again to figure out parent scope exports with less redundancy
	#  - passing PARENT_SCOPE always generates warning if there is none
	#  - using a variable containing PARENT_SCOPE is tough

	# some convenience variables
	SET(HAM_LIB_VEO_VE ham_offload_veo_ve)
	if (HAM_HAS_PARENT)
		SET(HAM_LIB_VEO_VE ${HAM_LIB_VEO_VE} PARENT_SCOPE)
	endif ()
	# https://stackoverflow.com/questions/5693405/specifying-link-flags-for-only-one-static-lib-while-linking-executable
	# hams static lib contains symbols needed for VEO calls
	SET(HAM_LIB_VEO_VE_CLI -Wl,--whole-archive ${HAM_LIB_VEO_VE} -Wl,--no-whole-archive)
	if (HAM_HAS_PARENT)
		SET(HAM_LIB_VEO_VE_CLI ${HAM_LIB_VEO_VE_CLI} PARENT_SCOPE)
	endif ()
	SET(HAM_LIB_VEDMA_VE ham_offload_vedma_ve)
	if (HAM_HAS_PARENT)
		SET(HAM_LIB_VEDMA_VE ${HAM_LIB_VEDMA_VE} PARENT_SCOPE)
	endif ()
	SET(HAM_LIB_VEDMA_VE_CLI -Wl,--whole-archive ${HAM_LIB_VEDMA_VE} -Wl,--no-whole-archive)
	if (HAM_HAS_PARENT)
		SET(HAM_LIB_VEDMA_VE_CLI ${HAM_LIB_VEDMA_VE_CLI} PARENT_SCOPE)
	endif ()
else ()
	message(STATUS "Could NOT find VEO (missing: ve_offload.h)")
endif ()

# globally tell the compiler to be strict with warnings
if (HAM_NEC_COMPILER_DETECTED)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
else ()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wextra")
endif ()

add_subdirectory(thirdparty/bmt ${CMAKE_CURRENT_BINARY_DIR}/build.noma_bmt)
add_subdirectory(thirdparty/misc ${CMAKE_CURRENT_BINARY_DIR}/build.noma_misc)
add_subdirectory(src)
